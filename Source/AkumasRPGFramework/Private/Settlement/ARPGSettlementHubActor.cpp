#include "Settlement/ARPGSettlementHubActor.h"

#include "AkumasRPGFramework.h"
#include "AI/NavigationSystemBase.h"
#include "Components/ARPGFactionOwnershipComponent.h"
#include "Components/ARPGInventoryComponent.h"
#include "Data/ARPGBuildPieceDefinition.h"
#include "Data/ARPGSettlementDefinition.h"
#include "EngineUtils.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "NavigationSystem.h"
#include "Net/UnrealNetwork.h"
#include "Settlement/ARPGBuildBedActor.h"
#include "Settlement/ARPGSettlementResidentComponent.h"
#include "Settlement/ARPGSettlementVillagerCharacter.h"
#include "TimerManager.h"

namespace
{
    static bool SettlementWallLike(EARPGBuildPieceKind Kind)
    {
        return Kind == EARPGBuildPieceKind::Wall || Kind == EARPGBuildPieceKind::WindowWall || Kind == EARPGBuildPieceKind::Doorway;
    }

    static bool SettlementCoverKind(EARPGBuildPieceKind Kind, bool bAllowRoof)
    {
        return Kind == EARPGBuildPieceKind::Floor || Kind == EARPGBuildPieceKind::Ceiling || (bAllowRoof && Kind == EARPGBuildPieceKind::Roof);
    }

    static bool YawAxisEquivalent(float A, float B, float Tolerance = 5.f)
    {
        const float D = FMath::Abs(FMath::FindDeltaAngleDegrees(A, B));
        return D <= Tolerance || FMath::Abs(D - 180.f) <= Tolerance;
    }


    static bool DoorOccupiesDoorway(const AARPGBuildPieceActor* Doorway, const AARPGBuildPieceActor* Door)
    {
        if (!Doorway || !Doorway->Definition || !Door || !Door->Definition) return false;
        if (Doorway->Definition->PieceKind != EARPGBuildPieceKind::Doorway || Door->Definition->PieceKind != EARPGBuildPieceKind::Door) return false;
        if (!Doorway->IsConstructionComplete() || !Door->IsConstructionComplete()) return false;
        const float Tol = FMath::Max(1.f, FMath::Max(Doorway->Definition->PlacementCollisionClearance, Door->Definition->PlacementCollisionClearance) + 0.75f);
        const float TolSq = FMath::Square(Tol);
        TArray<FTransform> Candidates;
        Doorway->GetSnapTransformsFor(Door->Definition, Candidates);
        for (const FTransform& Candidate : Candidates)
        {
            if (FVector::DistSquared(Candidate.GetLocation(), Door->GetActorLocation()) > TolSq) continue;
            const float YawDelta = FMath::Abs(FMath::FindDeltaAngleDegrees(Candidate.Rotator().Yaw, Door->GetActorRotation().Yaw));
            if (YawDelta <= 1.5f) return true;
        }
        return false;
    }

    struct FSettlementPieceSpatial
    {
        AARPGBuildPieceActor* Actor = nullptr;
        EARPGBuildPieceKind Kind = EARPGBuildPieceKind::Custom;
        FVector LocalMin = FVector::ZeroVector;
        FVector LocalMax = FVector::ZeroVector;
        FVector WorldCenter = FVector::ZeroVector;
        float WorldMinZ = 0.f;
        float WorldMaxZ = 0.f;
    };

    static void SettlementGetDefinitionLocalBounds(const UARPGBuildPieceDefinition* Piece, FVector& OutMin, FVector& OutMax)
    {
        if (!Piece)
        {
            OutMin = FVector::ZeroVector;
            OutMax = FVector::ZeroVector;
            return;
        }

        // Match the building system's pivot-aware bounds contract exactly. Home validation must reason
        // about the structural surfaces players actually snapped together, not arbitrary actor origins.
        if (USkeletalMesh* Mesh = Piece->BuildSkeletalMesh.LoadSynchronous())
        {
            const FBoxSphereBounds Bounds = Mesh->GetBounds();
            const FBox RawBox(Bounds.Origin - Bounds.BoxExtent, Bounds.Origin + Bounds.BoxExtent);
            const FBox ActorLocalBox = RawBox.TransformBy(Piece->MeshRelativeTransform);
            OutMin = ActorLocalBox.Min;
            OutMax = ActorLocalBox.Max;
            return;
        }
        if (UStaticMesh* Mesh = Piece->BuildMesh.LoadSynchronous())
        {
            const FBoxSphereBounds Bounds = Mesh->GetBounds();
            const FBox RawBox(Bounds.Origin - Bounds.BoxExtent, Bounds.Origin + Bounds.BoxExtent);
            const FBox ActorLocalBox = RawBox.TransformBy(Piece->MeshRelativeTransform);
            OutMin = ActorLocalBox.Min;
            OutMax = ActorLocalBox.Max;
            return;
        }

        OutMin = -Piece->PlacementBounds;
        OutMax = Piece->PlacementBounds;
    }

    static bool SettlementBuildSpatial(AARPGBuildPieceActor* Actor, FSettlementPieceSpatial& OutSpatial)
    {
        if (!Actor || !Actor->Definition) return false;

        OutSpatial = FSettlementPieceSpatial();
        OutSpatial.Actor = Actor;
        OutSpatial.Kind = Actor->Definition->PieceKind;
        SettlementGetDefinitionLocalBounds(Actor->Definition, OutSpatial.LocalMin, OutSpatial.LocalMax);

        const FTransform& Transform = Actor->GetActorTransform();
        const FVector LocalCenter = (OutSpatial.LocalMin + OutSpatial.LocalMax) * 0.5f;
        OutSpatial.WorldCenter = Transform.TransformPosition(LocalCenter);
        const FBox WorldBox = FBox(OutSpatial.LocalMin, OutSpatial.LocalMax).TransformBy(Transform);
        OutSpatial.WorldMinZ = WorldBox.Min.Z;
        OutSpatial.WorldMaxZ = WorldBox.Max.Z;
        return true;
    }

    static float SettlementProjectedDistanceSquaredToBounds(const FSettlementPieceSpatial& Spatial, const FVector& WorldPoint)
    {
        if (!Spatial.Actor) return TNumericLimits<float>::Max();
        const FVector Local = Spatial.Actor->GetActorTransform().InverseTransformPosition(
            FVector(WorldPoint.X, WorldPoint.Y, Spatial.WorldCenter.Z));
        const float DX = Local.X < Spatial.LocalMin.X ? Spatial.LocalMin.X - Local.X :
            (Local.X > Spatial.LocalMax.X ? Local.X - Spatial.LocalMax.X : 0.f);
        const float DY = Local.Y < Spatial.LocalMin.Y ? Spatial.LocalMin.Y - Local.Y :
            (Local.Y > Spatial.LocalMax.Y ? Local.Y - Spatial.LocalMax.Y : 0.f);
        return DX * DX + DY * DY;
    }

    static bool SettlementProjectedContains(const FSettlementPieceSpatial& Spatial, const FVector& WorldPoint, float Tolerance)
    {
        if (!Spatial.Actor) return false;
        const FVector Local = Spatial.Actor->GetActorTransform().InverseTransformPosition(
            FVector(WorldPoint.X, WorldPoint.Y, Spatial.WorldCenter.Z));
        return Local.X >= Spatial.LocalMin.X - Tolerance && Local.X <= Spatial.LocalMax.X + Tolerance &&
               Local.Y >= Spatial.LocalMin.Y - Tolerance && Local.Y <= Spatial.LocalMax.Y + Tolerance;
    }

    struct FHomeCandidateProgress
    {
        FARPGSettlementHomeValidation Validation;
        int32 MissingScore = TNumericLimits<int32>::Max();
        int32 Area = TNumericLimits<int32>::Max();
    };
}

AARPGSettlementHubActor::AARPGSettlementHubActor()
{
    bReplicates = true;
    bPersistent = true;
}

void AARPGSettlementHubActor::BeginPlay()
{
    Super::BeginPlay();
    OnConstructionCompleted.AddUniqueDynamic(this, &AARPGSettlementHubActor::HandleConstructionCompleted);
    if (HasAuthority() && IsConstructionComplete() && Definition) StartSettlementRuntime();
}

void AARPGSettlementHubActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    StopSettlementRuntime();
    if (HasAuthority()) DismissResidentsAuthority();
    OnConstructionCompleted.RemoveDynamic(this, &AARPGSettlementHubActor::HandleConstructionCompleted);
    Super::EndPlay(EndPlayReason);
}

void AARPGSettlementHubActor::InitializeBuilding(UARPGBuildPieceDefinition* InDefinition, AActor* Builder)
{
    Super::InitializeBuilding(InDefinition, Builder);
    if (!HasAuthority() || !InDefinition) return;
    if (Inventory)
    {
        const UARPGSettlementDefinition* Def = GetSettlementDefinition();
        Inventory->MaxSlots = Def ? FMath::Max(1, Def->SettlementStockpileSlots) : 96;
        Inventory->bGrantStartingItemsOnBeginPlay = false;
    }
    if (IsConstructionComplete()) StartSettlementRuntime();
}


UARPGSettlementDefinition* AARPGSettlementHubActor::GetSettlementDefinition() const
{
    return Definition ? Definition->SettlementDefinition.Get() : nullptr;
}

float AARPGSettlementHubActor::GetSettlementRadius() const
{
    const UARPGSettlementDefinition* Def = GetSettlementDefinition();
    return Def ? FMath::Max(300.f, Def->SettlementRadius) : 5000.f;
}

float AARPGSettlementHubActor::GetSettlementHUDRadius() const
{
    const UARPGSettlementDefinition* Def = GetSettlementDefinition();
    return Def ? FMath::Max(100.f, Def->SettlementHUDRadius) : 1800.f;
}

bool AARPGSettlementHubActor::IsLocationInsideSettlement(const FVector& WorldLocation) const
{
    return FVector::DistSquared2D(WorldLocation, GetActorLocation()) <= FMath::Square(GetSettlementRadius());
}

bool AARPGSettlementHubActor::CanManageBuilding(const AARPGBuildPieceActor* Building) const
{
    if (!Building || !Building->Definition || !Building->IsConstructionComplete() || !IsLocationInsideSettlement(Building->GetActorLocation())) return false;
    if (Building == this) return true;
    if (!Ownership || !Building->Ownership) return false;

    if (Ownership->OwnerCharacterId.IsValid() && Building->Ownership->OwnerCharacterId == Ownership->OwnerCharacterId) return true;
    if (Ownership->OwnerAccountId.IsValid() && Building->Ownership->OwnerAccountId == Ownership->OwnerAccountId) return true;

    const UARPGSettlementDefinition* Def = GetSettlementDefinition();
    if (Def && Def->bAcceptSameFactionBuildings && !Ownership->OwnerFactionId.IsNone() &&
        Building->Ownership->OwnerFactionId == Ownership->OwnerFactionId)
        return true;
    return false;
}

void AARPGSettlementHubActor::GetManagedBeds(TArray<AARPGBuildBedActor*>& OutBeds) const
{
    OutBeds.Reset();
    if (!GetWorld()) return;
    for (TActorIterator<AARPGBuildBedActor> It(GetWorld()); It; ++It)
    {
        AARPGBuildBedActor* Bed = *It;
        if (Bed && CanManageBuilding(Bed) && Bed->FindManagingSettlementHub() == this) OutBeds.Add(Bed);
    }
}

void AARPGSettlementHubActor::GetSettlementResidents(TArray<AARPGSettlementVillagerCharacter*>& OutResidents) const
{
    OutResidents.Reset();
    TSet<const AARPGSettlementVillagerCharacter*> SeenActors;
    TSet<FGuid> SeenResidentIds;
    for (const TWeakObjectPtr<AARPGSettlementVillagerCharacter>& Weak : Residents)
    {
        AARPGSettlementVillagerCharacter* Resident = Weak.Get();
        if (!Resident || SeenActors.Contains(Resident)) continue;
        const FGuid Id = Resident->SettlementResident ? Resident->SettlementResident->ResidentId : FGuid();
        if (Id.IsValid() && SeenResidentIds.Contains(Id)) continue;
        SeenActors.Add(Resident);
        if (Id.IsValid()) SeenResidentIds.Add(Id);
        OutResidents.Add(Resident);
    }
}

AARPGBuildBedActor* AARPGSettlementHubActor::FindBedByBuildingId(FGuid BedBuildingId) const
{
    if (!BedBuildingId.IsValid() || !GetWorld()) return nullptr;
    for (TActorIterator<AARPGBuildBedActor> It(GetWorld()); It; ++It)
        if (It->BuildingId == BedBuildingId && CanManageBuilding(*It)) return *It;
    return nullptr;
}

AARPGSettlementVillagerCharacter* AARPGSettlementHubActor::FindResidentById(FGuid ResidentId) const
{
    if (!ResidentId.IsValid()) return nullptr;
    for (const TWeakObjectPtr<AARPGSettlementVillagerCharacter>& Weak : Residents)
    {
        AARPGSettlementVillagerCharacter* Resident = Weak.Get();
        if (Resident && Resident->SettlementResident && Resident->SettlementResident->ResidentId == ResidentId) return Resident;
    }
    return nullptr;
}

bool AARPGSettlementHubActor::ValidateHomeForBed(AARPGBuildBedActor* Bed, FARPGSettlementHomeValidation& OutValidation) const
{
    OutValidation = FARPGSettlementHomeValidation();
    if (!Bed || !GetWorld() || !CanManageBuilding(Bed) || Bed->FindManagingSettlementHub() != this)
    {
        OutValidation.State = EARPGSettlementHomeState::NoSettlement;
        OutValidation.StatusText = FText::FromString(TEXT("Bed is outside this Settlement, is not owned by it, or is managed by a nearer Hub."));
        return false;
    }
    const UARPGSettlementDefinition* Def = GetSettlementDefinition();
    if (!Def)
    {
        OutValidation.State = EARPGSettlementHomeState::NoSettlement;
        OutValidation.StatusText = FText::FromString(TEXT("Settlement Hub has no Settlement Definition."));
        return false;
    }

    const float Grid = FMath::Max(50.f, Def->HomeGridSize);
    const float Tol = FMath::Max(1.f, Def->HomeGridTolerance);
    const float Story = FMath::Max(50.f, Def->HomeStoryHeight);
    const float PlaneTol = FMath::Max(12.f, Tol + 6.f);
    const int32 MinW = FMath::Max(2, Def->MinimumFoundationWidth);
    const int32 MinD = FMath::Max(2, Def->MinimumFoundationDepth);
    const int32 MaxDim = FMath::Max(FMath::Max(MinW, MinD), Def->MaximumHomeDimensionCells);
    OutValidation.RequiredFoundationCount = MinW * MinD;
    OutValidation.RequiredCoverCount = MinW * MinD;
    OutValidation.RequiredPerimeterSegmentCount = 2 * MinW + 2 * MinD;

    // Cache each managed piece's transformed visible/placement envelope once. The structural build
    // graph already supports center-, bottom- and corner-pivot art; settlement validation must use the
    // same transformed bounds instead of assuming actor origins are cell centers or story planes.
    TArray<FSettlementPieceSpatial> Managed;
    for (TActorIterator<AARPGBuildPieceActor> It(GetWorld()); It; ++It)
    {
        AARPGBuildPieceActor* B = *It;
        if (!B || !CanManageBuilding(B)) continue;
        FSettlementPieceSpatial Spatial;
        if (SettlementBuildSpatial(B, Spatial)) Managed.Add(MoveTemp(Spatial));
    }

    FSettlementPieceSpatial BedSpatial;
    if (!SettlementBuildSpatial(Bed, BedSpatial))
    {
        OutValidation.State = EARPGSettlementHomeState::Incomplete;
        OutValidation.StatusText = FText::FromString(TEXT("Bed has no valid settlement placement envelope."));
        return false;
    }

    // Resolve the actual Foundation underneath the Bed by projected visible bounds + finished top
    // surface. This mirrors furnishing placement and is stable even when the Foundation/Bed pivots are
    // not at their geometric centers.
    const FSettlementPieceSpatial* Anchor = nullptr;
    float BestAnchorScore = TNumericLimits<float>::Max();
    for (const FSettlementPieceSpatial& Spatial : Managed)
    {
        if (Spatial.Kind != EARPGBuildPieceKind::Foundation) continue;
        if (FMath::Abs(BedSpatial.WorldMinZ - Spatial.WorldMaxZ) > FMath::Max(PlaneTol, 60.f)) continue;
        const float D2 = SettlementProjectedDistanceSquaredToBounds(Spatial, BedSpatial.WorldCenter);
        if (D2 < BestAnchorScore && D2 <= FMath::Square(Grid * 0.35f))
        {
            Anchor = &Spatial;
            BestAnchorScore = D2;
        }
    }
    if (!Anchor)
    {
        OutValidation.State = EARPGSettlementHomeState::Incomplete;
        OutValidation.StatusText = FText::FromString(TEXT("Bed needs a completed Foundation-based home inside the Settlement."));
        return false;
    }

    OutValidation.AnchorFoundationId = Anchor->Actor->BuildingId;
    const FVector OriginCenter = Anchor->WorldCenter;
    const float FoundationTopZ = Anchor->WorldMaxZ;
    const float BasisYaw = Anchor->Actor->GetActorRotation().Yaw;
    const FVector XAxis = FRotator(0.f, BasisYaw, 0.f).RotateVector(FVector::XAxisVector);
    const FVector YAxis = FRotator(0.f, BasisYaw, 0.f).RotateVector(FVector::YAxisVector);

    // Map completed Foundations by their transformed visual centers on the shared finished top plane.
    // Actor origins are deliberately never used as semantic cell centers here.
    TMap<FIntPoint, const FSettlementPieceSpatial*> FoundationMap;
    for (const FSettlementPieceSpatial& Spatial : Managed)
    {
        if (Spatial.Kind != EARPGBuildPieceKind::Foundation) continue;
        if (FMath::Abs(Spatial.WorldMaxZ - FoundationTopZ) > PlaneTol) continue;
        const FVector Delta = Spatial.WorldCenter - OriginCenter;
        const float LX = FVector::DotProduct(Delta, XAxis) / Grid;
        const float LY = FVector::DotProduct(Delta, YAxis) / Grid;
        const int32 IX = FMath::RoundToInt(LX);
        const int32 IY = FMath::RoundToInt(LY);
        const FVector Expected = OriginCenter + XAxis * (IX * Grid) + YAxis * (IY * Grid);
        if (FVector::DistSquared2D(Expected, Spatial.WorldCenter) > FMath::Square(Tol)) continue;

        const FIntPoint Cell(IX, IY);
        if (const FSettlementPieceSpatial* const* Existing = FoundationMap.Find(Cell))
        {
            if (FVector::DistSquared2D(Expected, Spatial.WorldCenter) >= FVector::DistSquared2D(Expected, (*Existing)->WorldCenter)) continue;
        }
        FoundationMap.Add(Cell, &Spatial);
    }

    // Anchor is the Bed's actual support cell, so every searched rectangle must contain (0,0).
    // This avoids misclassifying long/rotated Bed actor pivots near a Foundation edge.
    constexpr int32 BedX = 0;
    constexpr int32 BedY = 0;

    TArray<const FSettlementPieceSpatial*> Covers;
    TArray<const FSettlementPieceSpatial*> Walls;
    TArray<AARPGBuildPieceActor*> Doors;
    for (const FSettlementPieceSpatial& Spatial : Managed)
    {
        if (SettlementCoverKind(Spatial.Kind, Def->bAllowRoofAsHomeCover)) Covers.Add(&Spatial);
        if (SettlementWallLike(Spatial.Kind)) Walls.Add(&Spatial);
        if (Spatial.Kind == EARPGBuildPieceKind::Door) Doors.Add(Spatial.Actor);
    }

    auto FindCover = [&](const FVector& Target, float ExpectedTopZ, const FSettlementPieceSpatial*& OutPiece) -> bool
    {
        OutPiece = nullptr;
        float Best = TNumericLimits<float>::Max();
        for (const FSettlementPieceSpatial* Spatial : Covers)
        {
            if (!Spatial || !Spatial->Actor) continue;
            // Floors/Ceilings/Roofs built from a Wall-family story are top-aligned to the canonical
            // story plane. Compare that finished visible plane, not the actor's arbitrary pivot Z.
            if (FMath::Abs(Spatial->WorldMaxZ - ExpectedTopZ) > PlaneTol) continue;
            if (!SettlementProjectedContains(*Spatial, Target, Tol)) continue;
            const float D2 = FVector::DistSquared2D(Spatial->WorldCenter, Target);
            if (D2 < Best) { OutPiece = Spatial; Best = D2; }
        }
        return OutPiece != nullptr;
    };

    auto FindWall = [&](const FVector& Target, float ExpectedBottomZ, float ExpectedYaw, const TSet<const AARPGBuildPieceActor*>& Used, const FSettlementPieceSpatial*& OutPiece) -> bool
    {
        OutPiece = nullptr;
        float Best = TNumericLimits<float>::Max();
        for (const FSettlementPieceSpatial* Spatial : Walls)
        {
            if (!Spatial || !Spatial->Actor || Used.Contains(Spatial->Actor)) continue;
            if (!YawAxisEquivalent(Spatial->Actor->GetActorRotation().Yaw, ExpectedYaw)) continue;
            // Wall-family pieces are bottom-aligned to the supporting horizontal finished surface.
            if (FMath::Abs(Spatial->WorldMinZ - ExpectedBottomZ) > PlaneTol) continue;
            if (!SettlementProjectedContains(*Spatial, Target, Tol)) continue;
            const float D2 = FVector::DistSquared2D(Spatial->WorldCenter, Target);
            if (D2 < Best) { OutPiece = Spatial; Best = D2; }
        }
        return OutPiece != nullptr;
    };

    FHomeCandidateProgress BestProgress;
    for (int32 W = MinW; W <= MaxDim; ++W)
    {
        for (int32 D = MinD; D <= MaxDim; ++D)
        {
            for (int32 MinX = BedX - W + 1; MinX <= BedX; ++MinX)
            {
                const int32 MaxX = MinX + W - 1;
                for (int32 MinY = BedY - D + 1; MinY <= BedY; ++MinY)
                {
                    const int32 MaxY = MinY + D - 1;
                    int32 FoundationCount = 0;
                    for (int32 X = MinX; X <= MaxX; ++X)
                        for (int32 Y = MinY; Y <= MaxY; ++Y)
                            if (FoundationMap.Contains(FIntPoint(X, Y))) ++FoundationCount;
                    const int32 RequiredFoundations = W * D;
                    if (FoundationCount != RequiredFoundations) continue;

                    FARPGSettlementHomeValidation Candidate;
                    Candidate.State = EARPGSettlementHomeState::Incomplete;
                    Candidate.AnchorFoundationId = Anchor->Actor->BuildingId;
                    Candidate.FoundationCount = RequiredFoundations;
                    Candidate.RequiredFoundationCount = RequiredFoundations;
                    Candidate.RequiredCoverCount = RequiredFoundations;
                    Candidate.RequiredPerimeterSegmentCount = 2 * W + 2 * D;
                    Candidate.HomeCenter = OriginCenter + XAxis * (((MinX + MaxX) * 0.5f) * Grid) +
                        YAxis * (((MinY + MaxY) * 0.5f) * Grid);
                    Candidate.HomeCenter.Z = FoundationTopZ + Story * 0.5f;
                    Candidate.HomeExtent = FVector(W * Grid * 0.5f, D * Grid * 0.5f, Story * 0.5f);

                    // A cover is credited when its transformed footprint covers the actual Foundation
                    // cell center and its finished top is exactly one canonical story above that
                    // Foundation's finished top. One large authored cover may legitimately cover more
                    // than one cell; the requirement is spatial coverage, not actor count.
                    for (int32 X = MinX; X <= MaxX; ++X)
                    {
                        for (int32 Y = MinY; Y <= MaxY; ++Y)
                        {
                            const FSettlementPieceSpatial* const* FoundationCellPtr = FoundationMap.Find(FIntPoint(X, Y));
                            if (!FoundationCellPtr || !*FoundationCellPtr) continue;
                            const FSettlementPieceSpatial* FoundationCell = *FoundationCellPtr;
                            FVector Target = FoundationCell->WorldCenter;
                            Target.Z = FoundationCell->WorldMaxZ + Story;
                            const FSettlementPieceSpatial* Cover = nullptr;
                            if (FindCover(Target, FoundationCell->WorldMaxZ + Story, Cover)) ++Candidate.CoverCount;
                        }
                    }

                    TSet<const AARPGBuildPieceActor*> UsedPerimeterPieces;
                    auto CheckWallSegment = [&](const FVector& Target, float ExpectedBottomZ, float ExpectedYaw)
                    {
                        const FSettlementPieceSpatial* Segment = nullptr;
                        if (!FindWall(Target, ExpectedBottomZ, ExpectedYaw, UsedPerimeterPieces, Segment) || !Segment || !Segment->Actor) return;
                        UsedPerimeterPieces.Add(Segment->Actor);
                        ++Candidate.PerimeterSegmentCount;
                        if (Segment->Kind == EARPGBuildPieceKind::Doorway)
                        {
                            Candidate.bHasDoorway = true;
                            for (AARPGBuildPieceActor* Door : Doors)
                                if (DoorOccupiesDoorway(Segment->Actor, Door)) { Candidate.bHasDoor = true; break; }
                        }
                    };

                    // Perimeter targets are derived from each real Foundation cell's transformed center
                    // and top plane. This is the same half-grid edge contract used by native structural
                    // snapping and therefore survives center/bottom/corner pivots and MeshRelativeTransform.
                    for (int32 X = MinX; X <= MaxX; ++X)
                    {
                        if (const FSettlementPieceSpatial* const* SouthPtr = FoundationMap.Find(FIntPoint(X, MinY)))
                        {
                            const FSettlementPieceSpatial* South = *SouthPtr;
                            CheckWallSegment(South->WorldCenter - YAxis * (Grid * 0.5f), South->WorldMaxZ, BasisYaw);
                        }
                        if (const FSettlementPieceSpatial* const* NorthPtr = FoundationMap.Find(FIntPoint(X, MaxY)))
                        {
                            const FSettlementPieceSpatial* North = *NorthPtr;
                            CheckWallSegment(North->WorldCenter + YAxis * (Grid * 0.5f), North->WorldMaxZ, BasisYaw);
                        }
                    }
                    for (int32 Y = MinY; Y <= MaxY; ++Y)
                    {
                        if (const FSettlementPieceSpatial* const* WestPtr = FoundationMap.Find(FIntPoint(MinX, Y)))
                        {
                            const FSettlementPieceSpatial* West = *WestPtr;
                            CheckWallSegment(West->WorldCenter - XAxis * (Grid * 0.5f), West->WorldMaxZ, BasisYaw + 90.f);
                        }
                        if (const FSettlementPieceSpatial* const* EastPtr = FoundationMap.Find(FIntPoint(MaxX, Y)))
                        {
                            const FSettlementPieceSpatial* East = *EastPtr;
                            CheckWallSegment(East->WorldCenter + XAxis * (Grid * 0.5f), East->WorldMaxZ, BasisYaw + 90.f);
                        }
                    }

                    Candidate.bValid = Candidate.CoverCount == Candidate.RequiredCoverCount &&
                        Candidate.PerimeterSegmentCount == Candidate.RequiredPerimeterSegmentCount &&
                        Candidate.bHasDoorway && Candidate.bHasDoor;
                    if (Candidate.bValid)
                    {
                        Candidate.State = EARPGSettlementHomeState::Valid;
                        Candidate.StatusText = FText::Format(FText::FromString(TEXT("Valid {0}x{1} settlement home.")), FText::AsNumber(W), FText::AsNumber(D));
                        OutValidation = Candidate;
                        return true;
                    }

                    // For an incomplete larger connected building, report the rectangle that is
                    // closest to becoming valid rather than blindly preferring the largest footprint.
                    // This keeps diagnostics actionable (for example 0/4 on a missing 2x2 ceiling)
                    // while still selecting a larger outer room when it is the genuinely complete shell.
                    const int32 MissingCover = Candidate.RequiredCoverCount - Candidate.CoverCount;
                    const int32 MissingPerimeter = Candidate.RequiredPerimeterSegmentCount - Candidate.PerimeterSegmentCount;
                    const int32 MissingScore = MissingCover * 6 + MissingPerimeter * 4 +
                        (Candidate.bHasDoorway ? 0 : 4) + (Candidate.bHasDoor ? 0 : 5);
                    const int32 Area = W * D;
                    if (MissingScore < BestProgress.MissingScore ||
                        (MissingScore == BestProgress.MissingScore && Area < BestProgress.Area))
                    {
                        BestProgress.MissingScore = MissingScore;
                        BestProgress.Area = Area;
                        BestProgress.Validation = Candidate;
                    }
                }
            }
        }
    }

    if (BestProgress.MissingScore != TNumericLimits<int32>::Max())
    {
        OutValidation = BestProgress.Validation;
        if (OutValidation.CoverCount < OutValidation.RequiredCoverCount)
            OutValidation.StatusText = FText::Format(FText::FromString(TEXT("Home needs complete overhead Floor/Ceiling cover ({0}/{1}).")), FText::AsNumber(OutValidation.CoverCount), FText::AsNumber(OutValidation.RequiredCoverCount));
        else if (OutValidation.PerimeterSegmentCount < OutValidation.RequiredPerimeterSegmentCount)
            OutValidation.StatusText = FText::Format(FText::FromString(TEXT("Home perimeter is incomplete ({0}/{1} wall segments).")), FText::AsNumber(OutValidation.PerimeterSegmentCount), FText::AsNumber(OutValidation.RequiredPerimeterSegmentCount));
        else if (!OutValidation.bHasDoorway)
            OutValidation.StatusText = FText::FromString(TEXT("Home requires a Doorway on its perimeter."));
        else if (!OutValidation.bHasDoor)
            OutValidation.StatusText = FText::FromString(TEXT("Home Doorway requires a completed Door."));
        else
            OutValidation.StatusText = FText::FromString(TEXT("Home is not complete."));
    }
    else
    {
        OutValidation.State = EARPGSettlementHomeState::Incomplete;
        OutValidation.FoundationCount = FoundationMap.Num();
        OutValidation.StatusText = FText::Format(FText::FromString(TEXT("Home needs at least a complete {0}x{1} Foundation footprint around this Bed.")), FText::AsNumber(MinW), FText::AsNumber(MinD));
    }
    return false;
}


bool AARPGSettlementHubActor::ResolveResidentHomeAnchor(AARPGBuildBedActor* Bed, FVector& OutWorldLocation) const
{
    OutWorldLocation = FVector::ZeroVector;
    if (!Bed || !GetWorld()) return false;

    FARPGSettlementHomeValidation Validation;
    if (!ValidateHomeForBed(Bed, Validation) || !Validation.bValid) return false;

    const UARPGSettlementDefinition* Def = GetSettlementDefinition();
    if (!Def) return false;
    UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
    if (!Nav) return false;

    // HomeCenter is the middle of the validated structural story; subtracting the vertical extent
    // yields the finished Foundation/Floor plane that residents are actually supposed to walk on.
    // Never project from the Bed with a full-story Z extent: a NavMesh query spanning ~300 cm can
    // legitimately choose the roof/ceiling island above an otherwise valid room.
    const float WalkablePlaneZ = Validation.HomeCenter.Z - Validation.HomeExtent.Z;
    const float Grid = FMath::Max(50.f, Def->HomeGridSize);
    const float VerticalTolerance = FMath::Clamp(FMath::Max(24.f, Def->HomeGridTolerance * 2.f), 24.f, 80.f);
    const float QueryXY = FMath::Clamp(Grid * 0.30f, 60.f, 120.f);
    const float InteriorMargin = FMath::Clamp(Grid * 0.18f, 35.f, 90.f);

    float BasisYaw = Bed->GetActorRotation().Yaw;
    if (Validation.AnchorFoundationId.IsValid())
    {
        for (TActorIterator<AARPGBuildPieceActor> It(GetWorld()); It; ++It)
        {
            AARPGBuildPieceActor* Piece = *It;
            if (Piece && Piece->BuildingId == Validation.AnchorFoundationId)
            {
                BasisYaw = Piece->GetActorRotation().Yaw;
                break;
            }
        }
    }
    const FVector XAxis = FRotator(0.f, BasisYaw, 0.f).RotateVector(FVector::XAxisVector);
    const FVector YAxis = FRotator(0.f, BasisYaw, 0.f).RotateVector(FVector::YAxisVector);
    const float MaxLocalX = FMath::Max(10.f, Validation.HomeExtent.X - InteriorMargin);
    const float MaxLocalY = FMath::Max(10.f, Validation.HomeExtent.Y - InteriorMargin);
    const float OffsetX = FMath::Min(Grid * 0.28f, MaxLocalX * 0.55f);
    const float OffsetY = FMath::Min(Grid * 0.28f, MaxLocalY * 0.55f);

    FVector Center = Validation.HomeCenter;
    Center.Z = WalkablePlaneZ + 12.f;
    FVector BedCandidate = Bed->GetActorLocation();
    BedCandidate.Z = Center.Z;

    TArray<FVector, TInlineAllocator<10>> Candidates;
    Candidates.Add(Center);
    Candidates.Add(FMath::Lerp(Center, BedCandidate, 0.5f));
    Candidates.Add(Center + XAxis * OffsetX);
    Candidates.Add(Center - XAxis * OffsetX);
    Candidates.Add(Center + YAxis * OffsetY);
    Candidates.Add(Center - YAxis * OffsetY);
    Candidates.Add(Center + XAxis * OffsetX + YAxis * OffsetY);
    Candidates.Add(Center + XAxis * OffsetX - YAxis * OffsetY);
    Candidates.Add(Center - XAxis * OffsetX + YAxis * OffsetY);
    Candidates.Add(Center - XAxis * OffsetX - YAxis * OffsetY);

    for (const FVector& Candidate : Candidates)
    {
        FNavLocation Projected;
        if (!Nav->ProjectPointToNavigation(Candidate, Projected, FVector(QueryXY, QueryXY, VerticalTolerance))) continue;
        if (FMath::Abs(Projected.Location.Z - WalkablePlaneZ) > VerticalTolerance) continue;

        // Projection may slide laterally around furniture, but it must remain inside the validated
        // room footprint. This rejects exterior/roof fallbacks when the interior has no usable NavMesh.
        const FVector Delta = Projected.Location - Validation.HomeCenter;
        const float LocalX = FVector::DotProduct(Delta, XAxis);
        const float LocalY = FVector::DotProduct(Delta, YAxis);
        if (FMath::Abs(LocalX) > MaxLocalX || FMath::Abs(LocalY) > MaxLocalY) continue;

        OutWorldLocation = Projected.Location;
        return true;
    }

    return false;
}

bool AARPGSettlementHubActor::CanResidentStartWoodcutting(const UARPGSettlementResidentComponent* Resident) const
{
    const UARPGSettlementDefinition* Def = GetSettlementDefinition();
    if (!Def || !Def->bEnableVillagerWoodcutting || !Resident) return false;
    const int32 MaxWorkers = Def->MaximumConcurrentWoodcutters;
    if (MaxWorkers <= 0) return true;
    int32 Active = 0;
    TArray<AARPGSettlementVillagerCharacter*> UniqueResidents;
    GetSettlementResidents(UniqueResidents);
    for (const AARPGSettlementVillagerCharacter* V : UniqueResidents)
        if (V && V->SettlementResident && V->SettlementResident->IsWorking()) ++Active;
    return Active < MaxWorkers || Resident->IsWorking();
}

void AARPGSettlementHubActor::HandleConstructionCompleted()
{
    if (HasAuthority()) StartSettlementRuntime();
}

void AARPGSettlementHubActor::StartSettlementRuntime()
{
    if (!HasAuthority() || !GetWorld() || !IsConstructionComplete() || !GetSettlementDefinition()) return;
    StopSettlementRuntime();
    const UARPGSettlementDefinition* Def = GetSettlementDefinition();
    RecruitmentUnlockServerTime = GetWorld()->GetTimeSeconds() + FMath::Max(0.f, Def->InitialRecruitmentDelay);
    NextRecruitmentServerTime = RecruitmentUnlockServerTime;
    const float Interval = FMath::Max(0.5f, Def->SettlementRefreshInterval);
    GetWorld()->GetTimerManager().SetTimer(SettlementRefreshTimer, this, &AARPGSettlementHubActor::RefreshSettlementAuthority, Interval, true, 0.25f);
    RefreshSettlementAuthority();
}

void AARPGSettlementHubActor::StopSettlementRuntime()
{
    if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(SettlementRefreshTimer);
}

void AARPGSettlementHubActor::DismissResidentsAuthority()
{
    if (!HasAuthority()) return;
    TArray<AARPGSettlementVillagerCharacter*> UniqueResidents;
    GetSettlementResidents(UniqueResidents);
    for (AARPGSettlementVillagerCharacter* Resident : UniqueResidents)
    {
        if (!Resident) continue;
        if (Resident->SettlementResident && Resident->SettlementResident->AssignedBed)
            Resident->SettlementResident->AssignedBed->ClearResidentAssignment(Resident->SettlementResident->ResidentId);
        OnSettlementResidentChanged.Broadcast(Resident, false);
        Resident->Destroy();
    }
    Residents.Reset();
}

void AARPGSettlementHubActor::RefreshSettlementNow()
{
    if (HasAuthority()) RefreshSettlementAuthority();
}

void AARPGSettlementHubActor::CleanupResidentRegistry()
{
    // Runtime callbacks can occur while a freshly spawned resident is still initializing. Keep this
    // registry idempotent by both actor identity and persistent ResidentId so a single pawn can never
    // inflate capacity/worker counts or produce duplicate UI rows.
    TSet<AARPGSettlementVillagerCharacter*> SeenActors;
    TSet<FGuid> SeenResidentIds;
    for (int32 I = Residents.Num() - 1; I >= 0; --I)
    {
        AARPGSettlementVillagerCharacter* Resident = Residents[I].Get();
        if (!Resident)
        {
            Residents.RemoveAtSwap(I);
            continue;
        }
        const FGuid Id = Resident->SettlementResident ? Resident->SettlementResident->ResidentId : FGuid();
        const bool bDuplicateActor = SeenActors.Contains(Resident);
        const bool bDuplicateId = Id.IsValid() && SeenResidentIds.Contains(Id);
        if (bDuplicateActor || bDuplicateId)
        {
            Residents.RemoveAtSwap(I);
            continue;
        }
        SeenActors.Add(Resident);
        if (Id.IsValid()) SeenResidentIds.Add(Id);
    }
}

void AARPGSettlementHubActor::RefreshSettlementAuthority()
{
    if (!HasAuthority() || !IsConstructionComplete() || !GetSettlementDefinition()) return;
    CleanupResidentRegistry();
    TArray<AARPGBuildBedActor*> Beds;
    GetManagedBeds(Beds);
    ReconcileBedsAndResidents(Beds);
    UpdateReplicatedSummary(Beds);
}

void AARPGSettlementHubActor::ReconcileBedsAndResidents(const TArray<AARPGBuildBedActor*>& ManagedBeds)
{
    const UARPGSettlementDefinition* Def = GetSettlementDefinition();
    if (!Def) return;

    TSet<FGuid> LiveResidentIds;
    for (const TWeakObjectPtr<AARPGSettlementVillagerCharacter>& Weak : Residents)
    {
        AARPGSettlementVillagerCharacter* Resident = Weak.Get();
        if (!Resident || !Resident->SettlementResident) continue;
        LiveResidentIds.Add(Resident->SettlementResident->ResidentId);
        AARPGBuildBedActor* Bed = Resident->SettlementResident->AssignedBed;
        FARPGSettlementHomeValidation Validation;
        if (!Bed || Bed->BedRole != EARPGBedRole::Villager || !ValidateHomeForBed(Bed, Validation)) Resident->SettlementResident->ClearBed();
    }

    for (AARPGBuildBedActor* Bed : ManagedBeds)
    {
        if (!Bed || Bed->BedRole != EARPGBedRole::Villager) continue;
        if (Bed->AssignedResidentId.IsValid() && !LiveResidentIds.Contains(Bed->AssignedResidentId)) Bed->ClearResidentAssignment();
    }

    // Re-home existing homeless residents before recruiting anyone new.
    for (const TWeakObjectPtr<AARPGSettlementVillagerCharacter>& Weak : Residents)
    {
        AARPGSettlementVillagerCharacter* Resident = Weak.Get();
        if (!Resident || !Resident->SettlementResident || Resident->SettlementResident->AssignedBed) continue;
        for (AARPGBuildBedActor* Bed : ManagedBeds)
        {
            if (!Bed || Bed->BedRole != EARPGBedRole::Villager || Bed->AssignedResidentId.IsValid()) continue;
            FARPGSettlementHomeValidation Validation;
            if (ValidateHomeForBed(Bed, Validation) && Resident->SettlementResident->AssignBed(Bed)) break;
        }
    }

    if ((int32)Residents.Num() >= FMath::Max(1, Def->MaximumVillagers) || !GetWorld()) return;
    const float Now = GetWorld()->GetTimeSeconds();
    if (Now < RecruitmentUnlockServerTime || Now < NextRecruitmentServerTime) return;

    for (AARPGBuildBedActor* Bed : ManagedBeds)
    {
        if (!Bed || Bed->BedRole != EARPGBedRole::Villager || Bed->AssignedResidentId.IsValid()) continue;
        FARPGSettlementHomeValidation Validation;
        if (!ValidateHomeForBed(Bed, Validation)) continue;
        if (RecruitResidentForBed(Bed))
        {
            NextRecruitmentServerTime = Now + FMath::Max(0.1f, Def->RecruitmentInterval);
            break; // polished arrival cadence: at most one new resident per recruitment interval
        }
    }
}

bool AARPGSettlementHubActor::RecruitResidentForBed(AARPGBuildBedActor* Bed)
{
    if (!HasAuthority() || !GetWorld() || !Bed || !GetSettlementDefinition()) return false;
    FARPGSettlementHomeValidation Validation;
    if (!ValidateHomeForBed(Bed, Validation)) return false;
    UARPGSettlementDefinition* Def = GetSettlementDefinition();

    UClass* ResidentClass = Def->VillagerClass.LoadSynchronous();
    if (!ResidentClass || !ResidentClass->IsChildOf(AARPGSettlementVillagerCharacter::StaticClass())) ResidentClass = AARPGSettlementVillagerCharacter::StaticClass();

    FVector HomeAnchor;
    if (!ResolveResidentHomeAnchor(Bed, HomeAnchor))
    {
        UE_LOG(LogARPG, Warning, TEXT("Settlement Hub %s cannot recruit for Bed %s: the valid home has no NavMesh-backed interior point on its walkable story. The roof/ceiling will never be used as a fallback spawn surface."),
            *GetName(), *GetNameSafe(Bed));
        return false;
    }

    // Never use AlwaysSpawn for residents. In an enclosed house that policy can resolve an encroaching
    // capsule by pushing it vertically through the ceiling and onto the roof. Try several same-story
    // navigable points and reject any collision adjustment that escapes the interior floor plane.
    FActorSpawnParameters Params;
    Params.Owner = GetOwner();
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding;

    const float Grid = FMath::Max(50.f, Def->HomeGridSize);
    const float VerticalTolerance = FMath::Clamp(FMath::Max(24.f, Def->HomeGridTolerance * 2.f), 24.f, 80.f);
    // Character actor origins/capsule centres can be collision-adjusted roughly one capsule half-height
    // above the NavMesh floor. Permit that normal correction while remaining far below a full 300 cm story.
    const float SpawnStoryTolerance = FMath::Clamp(FMath::Max(VerticalTolerance, Grid * 0.40f), 90.f, 140.f);
    const float RetryRadius = FMath::Clamp(Grid * 0.38f, 80.f, 150.f);
    UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());

    AARPGSettlementVillagerCharacter* Resident = nullptr;
    constexpr int32 MaxResidentSpawnAttempts = 10;
    for (int32 Attempt = 0; Attempt < MaxResidentSpawnAttempts && !Resident; ++Attempt)
    {
        FVector Candidate = HomeAnchor;
        if (Attempt > 0 && Nav)
        {
            FNavLocation Reachable;
            if (!Nav->GetRandomReachablePointInRadius(HomeAnchor, RetryRadius, Reachable)) continue;
            if (FMath::Abs(Reachable.Location.Z - HomeAnchor.Z) > VerticalTolerance) continue;
            Candidate = Reachable.Location;
        }

        AARPGSettlementVillagerCharacter* Spawned = GetWorld()->SpawnActor<AARPGSettlementVillagerCharacter>(ResidentClass, Candidate, Bed->GetActorRotation(), Params);
        if (!Spawned) continue;

        // AdjustIfPossible is allowed to make a small collision-safe correction, but never a story jump.
        // If UE moves the capsule onto the roof, destroy it immediately and keep looking on the floor.
        if (FMath::Abs(Spawned->GetActorLocation().Z - HomeAnchor.Z) > SpawnStoryTolerance ||
            FVector::DistSquared2D(Spawned->GetActorLocation(), Candidate) > FMath::Square(RetryRadius + 40.f))
        {
            Spawned->Destroy();
            continue;
        }
        Resident = Spawned;
    }

    if (!Resident)
    {
        UE_LOG(LogARPG, Warning, TEXT("Settlement Hub %s could not find a collision-safe same-story spawn point inside the valid home for Bed %s after %d attempts. Recruitment is deferred instead of placing a villager on the roof."),
            *GetName(), *GetNameSafe(Bed), MaxResidentSpawnAttempts);
        return false;
    }

    const FGuid ResidentId = FGuid::NewGuid();
    CleanupResidentRegistry();
    Resident->RPGCharacterName = MakeResidentName(Residents.Num() + 1);

    // Register the pawn before initialization. InitializeSettlementResident changes state, and that
    // state notification intentionally routes back through RegisterLoadedResident. Pre-registering
    // makes that callback idempotent instead of appending the same pawn a second time.
    Residents.Add(Resident);
    if (!Resident->InitializeAsSettlementVillager(this, Bed, ResidentId))
    {
        Residents.RemoveAll([Resident](const TWeakObjectPtr<AARPGSettlementVillagerCharacter>& Weak)
        {
            return Weak.Get() == Resident;
        });
        Resident->Destroy();
        return false;
    }
    CleanupResidentRegistry();
    OnSettlementResidentChanged.Broadcast(Resident, true);
    return true;
}

bool AARPGSettlementHubActor::RegisterLoadedResident(AARPGSettlementVillagerCharacter* Resident)
{
    if (!HasAuthority() || !Resident || !Resident->SettlementResident || Resident->SettlementResident->SettlementHub != this) return false;
    CleanupResidentRegistry();

    const FGuid IncomingId = Resident->SettlementResident->ResidentId;
    for (const TWeakObjectPtr<AARPGSettlementVillagerCharacter>& Existing : Residents)
    {
        AARPGSettlementVillagerCharacter* ExistingResident = Existing.Get();
        if (!ExistingResident) continue;
        if (ExistingResident == Resident) return true;
        if (IncomingId.IsValid() && ExistingResident->SettlementResident && ExistingResident->SettlementResident->ResidentId == IncomingId)
        {
            // Persistent identity is authoritative. Never register two live actors for one resident save id.
            return false;
        }
    }

    Residents.Add(Resident);
    CleanupResidentRegistry();
    OnSettlementResidentChanged.Broadcast(Resident, true);
    RefreshSettlementAuthority();
    return true;
}

void AARPGSettlementHubActor::NotifyResidentStateChanged(AARPGSettlementVillagerCharacter* Resident)
{
    if (!HasAuthority()) return;
    if (Resident) RegisterLoadedResident(Resident);
    TArray<AARPGBuildBedActor*> Beds;
    GetManagedBeds(Beds);
    UpdateReplicatedSummary(Beds);
}

void AARPGSettlementHubActor::UpdateReplicatedSummary(const TArray<AARPGBuildBedActor*>& ManagedBeds)
{
    const UARPGSettlementDefinition* Def = GetSettlementDefinition();
    FARPGSettlementSummary NewSummary;
    NewSummary.SettlementId = BuildingId;
    NewSummary.SettlementName = Def && !Def->DisplayName.IsEmpty() ? Def->DisplayName : FText::FromString(TEXT("Settlement"));
    TArray<AARPGSettlementVillagerCharacter*> UniqueResidents;
    GetSettlementResidents(UniqueResidents);
    NewSummary.ResidentCount = UniqueResidents.Num();
    NewSummary.bSettlementOperational = IsConstructionComplete() && Def != nullptr;

    TSet<FIntVector> ValidHomeCells;
    int32 ValidVillagerBeds = 0;
    const float HomeKeyGrid = Def ? FMath::Max(50.f, Def->HomeGridSize) : 300.f;
    for (AARPGBuildBedActor* Bed : ManagedBeds)
    {
        if (!Bed || Bed->BedRole != EARPGBedRole::Villager) continue;
        ++NewSummary.VillagerBedCount;
        FARPGSettlementHomeValidation Validation;
        if (ValidateHomeForBed(Bed, Validation) && Validation.bValid)
        {
            ++ValidVillagerBeds;
            ValidHomeCells.Add(FIntVector(
                FMath::RoundToInt(Validation.HomeCenter.X / HomeKeyGrid),
                FMath::RoundToInt(Validation.HomeCenter.Y / HomeKeyGrid),
                FMath::RoundToInt(Validation.HomeCenter.Z / FMath::Max(50.f, Def ? Def->HomeStoryHeight : 300.f))));
        }
    }
    NewSummary.ValidHomeCount = ValidHomeCells.Num();
    NewSummary.ResidentCapacity = Def ? FMath::Min(FMath::Max(1, Def->MaximumVillagers), ValidVillagerBeds) : ValidVillagerBeds;
    for (const AARPGSettlementVillagerCharacter* Resident : UniqueResidents)
        if (Resident && Resident->SettlementResident && Resident->SettlementResident->IsWorking()) ++NewSummary.WoodcuttersActive;

    SettlementSummary = NewSummary;
    OnSettlementSummaryChanged.Broadcast(SettlementSummary);
    ForceNetUpdate();
}

FString AARPGSettlementHubActor::MakeResidentName(int32 Ordinal) const
{
    const UARPGSettlementDefinition* Def = GetSettlementDefinition();
    if (Def && Def->VillagerNamePool.Num() > 0)
        return Def->VillagerNamePool[(FMath::Max(1, Ordinal) - 1) % Def->VillagerNamePool.Num()];
    return FString::Printf(TEXT("Villager %d"), FMath::Max(1, Ordinal));
}

void AARPGSettlementHubActor::OnRep_SettlementSummary()
{
    OnSettlementSummaryChanged.Broadcast(SettlementSummary);
}

void AARPGSettlementHubActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AARPGSettlementHubActor, SettlementSummary);
}
