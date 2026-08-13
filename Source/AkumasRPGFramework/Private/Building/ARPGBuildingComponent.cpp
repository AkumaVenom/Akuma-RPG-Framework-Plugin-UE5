#include "Building/ARPGBuildingComponent.h"
#include "Building/ARPGBuildDoorActor.h"
#include "Building/ARPGBuildPieceActor.h"
#include "Building/ARPGBuildPreviewActor.h"
#include "Building/ARPGFactionTerritoryVolume.h"
#include "Components/ARPGEventRouterComponent.h"
#include "Components/ARPGFactionComponent.h"
#include "Components/ARPGInventoryComponent.h"
#include "Crafting/ARPGCraftingStationActor.h"
#include "Crafting/ARPGStorageActor.h"
#include "Data/ARPGBuildPieceDefinition.h"
#include "Data/ARPGCraftingStationDefinition.h"
#include "Data/ARPGItemDefinition.h"
#include "Engine/OverlapResult.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInterface.h"

static FName ARPGResolveBuildCostId(const FARPGItemAmount& Amount)
{
    if (Amount.Item) return Amount.Item->DefinitionId.IsNone() ? Amount.Item->GetFName() : Amount.Item->DefinitionId;
    return Amount.ItemId;
}

static void ARPGAggregateBuildCosts(const UARPGBuildPieceDefinition* Piece, TMap<FName, int32>& OutCosts)
{
    OutCosts.Reset();
    if (!Piece) return;
    for (const FARPGItemAmount& Cost : Piece->BuildCost)
    {
        const FName Id = ARPGResolveBuildCostId(Cost);
        if (!Id.IsNone() && Cost.Quantity > 0) OutCosts.FindOrAdd(Id) += Cost.Quantity;
    }
}

static bool ARPGIsWallLikeKind(EARPGBuildPieceKind Kind)
{
    return Kind == EARPGBuildPieceKind::Wall || Kind == EARPGBuildPieceKind::WindowWall || Kind == EARPGBuildPieceKind::Doorway;
}

static bool ARPGIsHorizontalStructuralKind(EARPGBuildPieceKind Kind)
{
    return Kind == EARPGBuildPieceKind::Foundation || Kind == EARPGBuildPieceKind::Floor ||
           Kind == EARPGBuildPieceKind::Ceiling || Kind == EARPGBuildPieceKind::Roof;
}

/**
 * Multiple completed structures can advertise the same geometric snap slot. This is common at a
 * foundation corner after the first wall exists: both the horizontal support and that wall can
 * produce an incoming-wall transform at essentially the same location. The support edge owns the
 * unambiguous outward-facing orientation, while the wall corner deliberately exposes both turn
 * facings for wall-only construction. Prefer the support only when candidates are the same slot;
 * ordinary distance/yaw scoring remains unchanged everywhere else.
 */
static int32 ARPGGetSnapTargetSemanticPriority(const AARPGBuildPieceActor* Target, const UARPGBuildPieceDefinition* IncomingPiece)
{
    if (!Target || !Target->Definition || !IncomingPiece) return 0;
    if (!ARPGIsWallLikeKind(IncomingPiece->PieceKind)) return 0;

    const EARPGBuildPieceKind TargetKind = Target->Definition->PieceKind;
    if (ARPGIsHorizontalStructuralKind(TargetKind)) return 0;
    if (ARPGIsWallLikeKind(TargetKind)) return 1;
    return 2;
}

/**
 * PlacementBounds are validation half-extents, not a promise about where a mesh pivot lives.
 * Modular building meshes are commonly authored with center, bottom-center, or corner pivots.
 * Resolve the visible Build Mesh bounds so ground placement and validation share the real pivot.
 */
static bool ARPGGetBuildPieceLocalBounds(const UARPGBuildPieceDefinition* Piece, FVector& OutMin, FVector& OutMax)
{
    if (!Piece) return false;
    if (UStaticMesh* Mesh = Piece->BuildMesh.LoadSynchronous())
    {
        const FBoxSphereBounds Bounds = Mesh->GetBounds();
        const FBox RawBox(Bounds.Origin - Bounds.BoxExtent, Bounds.Origin + Bounds.BoxExtent);
        const FBox ActorLocalBox = RawBox.TransformBy(Piece->MeshRelativeTransform);
        OutMin = ActorLocalBox.Min;
        OutMax = ActorLocalBox.Max;
        return true;
    }

    // Safe fallback for definitions that intentionally use only a custom actor.
    OutMin = -Piece->PlacementBounds;
    OutMax = Piece->PlacementBounds;
    return false;
}

static FVector ARPGGetBuildPieceBoundsCenterLocal(const UARPGBuildPieceDefinition* Piece)
{
    FVector Min, Max;
    ARPGGetBuildPieceLocalBounds(Piece, Min, Max);
    return (Min + Max) * 0.5f;
}

static FVector ARPGGetBuildPieceBottomAnchorLocal(const UARPGBuildPieceDefinition* Piece)
{
    FVector Min, Max;
    ARPGGetBuildPieceLocalBounds(Piece, Min, Max);
    const FVector Center = (Min + Max) * 0.5f;
    return FVector(Center.X, Center.Y, Min.Z);
}

/**
 * A snapped modular seam may intentionally overlap a few centimetres of neighbouring art
 * (for example a 302 cm wall on a 300 cm structural grid, or two wall posts meeting at an L corner).
 * Collision validation must not globally ignore build pieces, because that would permit arbitrary
 * clipping. Instead, an overlapping completed build piece is tolerated only when it advertises the
 * incoming final transform as one of its native/custom snap transforms.
 */
static bool ARPGIsValidSnappedBuildNeighbor(AARPGBuildPieceActor* Neighbor, const UARPGBuildPieceDefinition* IncomingPiece, const FTransform& IncomingFinal)
{
    if (!Neighbor || !IncomingPiece || !Neighbor->IsConstructionComplete()) return false;

    TArray<FTransform> NeighborCandidates;
    Neighbor->GetSnapTransformsFor(IncomingPiece, NeighborCandidates);
    if (NeighborCandidates.Num() == 0) return false;

    // Candidate transforms come from the same authoritative snap graph, so only a small tolerance
    // is needed for transform composition/floating-point drift. PlacementCollisionClearance remains
    // the exposed per-piece seam/validation tuning value and therefore also informs this tolerance.
    const float PositionTolerance = FMath::Max(1.f, IncomingPiece->PlacementCollisionClearance + 0.5f);
    const float PositionToleranceSq = FMath::Square(PositionTolerance);
    constexpr float RotationToleranceDegrees = 1.f;

    for (const FTransform& Candidate : NeighborCandidates)
    {
        if (FVector::DistSquared(Candidate.GetLocation(), IncomingFinal.GetLocation()) > PositionToleranceSq) continue;
        const float RotationDeltaDegrees = FMath::RadiansToDegrees(Candidate.GetRotation().AngularDistance(IncomingFinal.GetRotation()));
        if (RotationDeltaDegrees <= RotationToleranceDegrees) return true;
    }
    return false;
}

UARPGBuildingComponent::UARPGBuildingComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = false;
    SetIsReplicatedByDefault(true);
}

void UARPGBuildingComponent::BeginPlay()
{
    Super::BeginPlay();
    if (bAutoSelectFirstCatalogPiece && BuildCatalog.Num() > 0 && BuildCatalog[0]) SelectedBuildPiece = BuildCatalog[0];
}

void UARPGBuildingComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    DestroyPreviewActor();
    Super::EndPlay(EndPlayReason);
}

bool UARPGBuildingComponent::IsLocalBuildController() const
{
    if (const APawn* Pawn = Cast<APawn>(GetOwner())) return Pawn->IsLocallyControlled();
    return GetOwner() && GetOwner()->HasAuthority();
}

bool UARPGBuildingComponent::BeginBuildMode(UARPGBuildPieceDefinition* Piece)
{
    if (!IsLocalBuildController()) return false;
    if (Piece) SelectedBuildPiece = Piece;
    if (!SelectedBuildPiece && BuildCatalog.Num() > 0) SelectedBuildPiece = BuildCatalog[0];
    if (!SelectedBuildPiece) return false;
    bBuildModeActive = true;
    PreviewYawOffset = 0.f;
    CurrentPreviewResult = EARPGPlacementResult::NoPiece;
    EnsurePreviewActor();
    SetComponentTickEnabled(true);
    UpdatePlacementPreview();
    OnBuildModeChanged.Broadcast(true, SelectedBuildPiece);
    return true;
}

void UARPGBuildingComponent::EndBuildMode()
{
    if (!bBuildModeActive && !ActivePreviewActor) return;
    bBuildModeActive = false;
    CurrentPreviewResult = EARPGPlacementResult::NoPiece;
    CurrentSnapTarget.Reset();
    SetComponentTickEnabled(false);
    DestroyPreviewActor();
    OnBuildModeChanged.Broadcast(false, SelectedBuildPiece);
}

bool UARPGBuildingComponent::ToggleBuildMode(UARPGBuildPieceDefinition* OptionalPiece)
{
    if (bBuildModeActive) { EndBuildMode(); return false; }
    return BeginBuildMode(OptionalPiece);
}

bool UARPGBuildingComponent::SelectBuildPiece(UARPGBuildPieceDefinition* Piece)
{
    if (!Piece) return false;
    SelectedBuildPiece = Piece;
    PreviewYawOffset = 0.f;
    if (bBuildModeActive)
    {
        DestroyPreviewActor();
        EnsurePreviewActor();
        UpdatePlacementPreview();
        OnBuildModeChanged.Broadcast(true, SelectedBuildPiece);
    }
    return true;
}

bool UARPGBuildingComponent::SelectNextBuildPiece()
{
    if (BuildCatalog.Num() == 0) return false;
    int32 Index = BuildCatalog.IndexOfByKey(SelectedBuildPiece);
    Index = (Index == INDEX_NONE) ? 0 : (Index + 1) % BuildCatalog.Num();
    return SelectBuildPiece(BuildCatalog[Index]);
}

bool UARPGBuildingComponent::SelectPreviousBuildPiece()
{
    if (BuildCatalog.Num() == 0) return false;
    int32 Index = BuildCatalog.IndexOfByKey(SelectedBuildPiece);
    Index = (Index == INDEX_NONE) ? 0 : (Index - 1 + BuildCatalog.Num()) % BuildCatalog.Num();
    return SelectBuildPiece(BuildCatalog[Index]);
}

void UARPGBuildingComponent::RotatePreview(float Direction)
{
    if (!SelectedBuildPiece || !bBuildModeActive) return;
    PreviewYawOffset += FMath::Sign(Direction) * FMath::Max(1.f, SelectedBuildPiece->RotationStepDegrees);
    PreviewYawOffset = FMath::Fmod(PreviewYawOffset, 360.f);
    UpdatePlacementPreview();
}

void UARPGBuildingComponent::EnsurePreviewActor()
{
    if (ActivePreviewActor || !GetWorld() || !SelectedBuildPiece) return;
    FActorSpawnParameters Params;
    Params.Owner = GetOwner();
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    ActivePreviewActor = GetWorld()->SpawnActor<AARPGBuildPreviewActor>(AARPGBuildPreviewActor::StaticClass(), FTransform::Identity, Params);
    if (ActivePreviewActor)
        ActivePreviewActor->ConfigurePreview(SelectedBuildPiece, ValidPreviewMaterial.LoadSynchronous(), InvalidPreviewMaterial.LoadSynchronous());
}

void UARPGBuildingComponent::DestroyPreviewActor()
{
    if (ActivePreviewActor) ActivePreviewActor->Destroy();
    ActivePreviewActor = nullptr;
}

void UARPGBuildingComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    if (bBuildModeActive && IsLocalBuildController()) UpdatePlacementPreview();
}

void UARPGBuildingComponent::UpdatePlacementPreview()
{
    if (!bBuildModeActive || !SelectedBuildPiece || !GetOwner() || !GetWorld()) return;
    EnsurePreviewActor();

    FVector ViewLocation;
    FRotator ViewRotation;
    if (const APawn* Pawn = Cast<APawn>(GetOwner()))
    {
        if (APlayerController* PC = Cast<APlayerController>(Pawn->GetController())) PC->GetPlayerViewPoint(ViewLocation, ViewRotation);
        else { ViewLocation = GetOwner()->GetActorLocation(); ViewRotation = GetOwner()->GetActorRotation(); }
    }
    else { ViewLocation = GetOwner()->GetActorLocation(); ViewRotation = GetOwner()->GetActorRotation(); }

    const FVector End = ViewLocation + ViewRotation.Vector() * MaxPlacementDistance;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(ARPGBuildPreviewTrace), false, GetOwner());
    if (ActivePreviewActor) Params.AddIgnoredActor(ActivePreviewActor);
    FHitResult Hit;
    const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, ViewLocation, End, PlacementTraceChannel, Params);

    const FRotator DesiredRotation(0.f, ViewRotation.Yaw + PreviewYawOffset, 0.f);
    FVector Location = bHit ? Hit.ImpactPoint : End;
    if (bHit && SelectedBuildPiece->bAllowGroundPlacement)
    {
        // Put the visible mesh's real bottom-center on the traced surface. This is pivot-aware:
        // bottom-pivot meshes need no artificial lift, while center-pivot meshes are lifted only by
        // their actual local bottom offset. Do not move along ImpactNormal: that causes sideways drift
        // on slopes and was the source of visibly floating bottom-pivot foundations.
        const FVector BottomAnchorLocal = ARPGGetBuildPieceBottomAnchorLocal(SelectedBuildPiece);
        Location = Hit.ImpactPoint - DesiredRotation.RotateVector(BottomAnchorLocal);
    }
    Location += DesiredRotation.RotateVector(SelectedBuildPiece->PlacementOffset);

    FTransform Desired(DesiredRotation, Location);
    AARPGBuildPieceActor* SnapTarget = nullptr;
    CurrentPreviewTransform = ResolvePlacementTransform(SelectedBuildPiece, Desired, SnapTarget);
    CurrentSnapTarget = SnapTarget;
    CurrentPreviewResult = bHit || SnapTarget ? EvaluatePlacementInternal(SelectedBuildPiece, CurrentPreviewTransform, SnapTarget) : EARPGPlacementResult::InvalidSurface;

    if (ActivePreviewActor)
    {
        ActivePreviewActor->SetActorTransform(CurrentPreviewTransform, false, nullptr, ETeleportType::TeleportPhysics);
        ActivePreviewActor->SetPlacementResult(CurrentPreviewResult);
    }
    OnBuildPreviewUpdated.Broadcast(CurrentPreviewResult, CurrentPreviewTransform);
}

FTransform UARPGBuildingComponent::ResolvePlacementTransform(const UARPGBuildPieceDefinition* Piece, const FTransform& DesiredTransform, AARPGBuildPieceActor*& OutSnapTarget) const
{
    OutSnapTarget = nullptr;
    if (!Piece) return DesiredTransform;
    FTransform Snapped;
    if (Piece->bSnapPlacement && FindBestSnapTransform(Piece, DesiredTransform, Snapped, OutSnapTarget)) return Snapped;

    FTransform Result = DesiredTransform;

    // Rotate around the piece's visible ground anchor instead of blindly around the actor pivot.
    // This keeps corner/bottom authored pivots from walking sideways when yaw is quantized.
    const FVector AnchorLocal = Piece->bAllowGroundPlacement ? ARPGGetBuildPieceBottomAnchorLocal(Piece) : FVector::ZeroVector;
    const FVector AnchorBeforeRotation = Result.TransformPosition(AnchorLocal);
    FRotator R = Result.Rotator();
    R.Pitch = 0.f; R.Roll = 0.f;
    R.Yaw = FMath::GridSnap(R.Yaw, FMath::Max(1.f, Piece->RotationStepDegrees));
    Result.SetRotation(R.Quaternion());
    Result.AddToTranslation(AnchorBeforeRotation - Result.TransformPosition(AnchorLocal));

    if (Piece->bSnapPlacement && Piece->SnapSize > KINDA_SMALL_NUMBER)
    {
        // Grid-snap the visible footprint anchor, not the actor pivot. This remains correct for
        // center, bottom-center and corner pivots. Z is intentionally left on the support surface.
        const FVector AnchorWorld = Result.TransformPosition(AnchorLocal);
        FVector SnappedAnchor = AnchorWorld;
        SnappedAnchor.X = FMath::GridSnap(SnappedAnchor.X, Piece->SnapSize);
        SnappedAnchor.Y = FMath::GridSnap(SnappedAnchor.Y, Piece->SnapSize);
        Result.AddToTranslation(SnappedAnchor - AnchorWorld);
    }
    return Result;
}

bool UARPGBuildingComponent::FindBestSnapTransform(const UARPGBuildPieceDefinition* Piece, const FTransform& DesiredTransform, FTransform& OutTransform, AARPGBuildPieceActor*& OutSnapTarget) const
{
    OutSnapTarget = nullptr;
    UWorld* World = GetWorld();
    if (!Piece || !World) return false;

    TArray<FOverlapResult> Overlaps;
    FCollisionObjectQueryParams ObjectParams;
    ObjectParams.AddObjectTypesToQuery(ECC_WorldStatic);
    ObjectParams.AddObjectTypesToQuery(ECC_WorldDynamic);
    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ARPGBuildSnapSearch), false, GetOwner());
    World->OverlapMultiByObjectType(Overlaps, DesiredTransform.GetLocation(), FQuat::Identity, ObjectParams,
        FCollisionShape::MakeSphere(FMath::Max(1.f, Piece->SnapSearchRadius)), QueryParams);

    float BestScore = TNumericLimits<float>::Max();
    int32 BestSemanticPriority = TNumericLimits<int32>::Max();
    const float CaptureSq = FMath::Square(FMath::Max(1.f, Piece->SnapCaptureDistance));
    // Only use semantic priority as a tie-breaker for transforms that represent the same physical
    // socket. This avoids changing normal snap selection while preventing an overlapping wall-corner
    // candidate from overriding the foundation/floor edge that defines wall exterior orientation.
    const float SameSlotTolerance = FMath::Max(1.f, Piece->PlacementCollisionClearance + 0.5f);
    const float SameSlotToleranceSq = FMath::Square(SameSlotTolerance);
    TSet<AARPGBuildPieceActor*> Seen;
    for (const FOverlapResult& Overlap : Overlaps)
    {
        AARPGBuildPieceActor* Target = Cast<AARPGBuildPieceActor>(Overlap.GetActor());
        if (!Target || Seen.Contains(Target) || !Target->IsConstructionComplete()) continue;
        Seen.Add(Target);
        const int32 SemanticPriority = ARPGGetSnapTargetSemanticPriority(Target, Piece);
        TArray<FTransform> Candidates;
        Target->GetSnapTransformsFor(Piece, Candidates);
        for (const FTransform& Candidate : Candidates)
        {
            const float DistSq = FVector::DistSquared(Candidate.GetLocation(), DesiredTransform.GetLocation());
            if (DistSq > CaptureSq) continue;
            const float YawDelta = FMath::Abs(FMath::FindDeltaAngleDegrees(Candidate.Rotator().Yaw, DesiredTransform.Rotator().Yaw));
            const float Score = DistSq + FMath::Square(YawDelta * 1.5f);

            const bool bSamePhysicalSlot = OutSnapTarget &&
                FVector::DistSquared(Candidate.GetLocation(), OutTransform.GetLocation()) <= SameSlotToleranceSq;
            const bool bBetterSemanticOwner = bSamePhysicalSlot && SemanticPriority < BestSemanticPriority;
            const bool bSameSemanticOwner = !bSamePhysicalSlot || SemanticPriority == BestSemanticPriority;
            if (bBetterSemanticOwner || (bSameSemanticOwner && Score < BestScore))
            {
                BestScore = Score;
                BestSemanticPriority = SemanticPriority;
                OutTransform = Candidate;
                OutSnapTarget = Target;
            }
        }
    }
    return OutSnapTarget != nullptr;
}

FTransform UARPGBuildingComponent::SnapTransform(const UARPGBuildPieceDefinition* Piece, const FTransform& DesiredTransform) const
{
    AARPGBuildPieceActor* Target = nullptr;
    return ResolvePlacementTransform(Piece, DesiredTransform, Target);
}

bool UARPGBuildingComponent::HasBuildResources(const UARPGBuildPieceDefinition* Piece) const
{
    if (!bConsumeResources) return true;
    const UARPGInventoryComponent* Inventory = GetOwner() ? GetOwner()->FindComponentByClass<UARPGInventoryComponent>() : nullptr;
    if (!Inventory || !Piece) return false;
    TMap<FName, int32> Costs;
    ARPGAggregateBuildCosts(Piece, Costs);
    for (const TPair<FName, int32>& Cost : Costs) if (!Inventory->HasUnequippedItem(Cost.Key, Cost.Value)) return false;
    return true;
}

int32 UARPGBuildingComponent::GetBuildableCount(const UARPGBuildPieceDefinition* Piece) const
{
    if (!Piece) return 0;
    if (!bConsumeResources) return 999;
    const UARPGInventoryComponent* Inventory = GetOwner() ? GetOwner()->FindComponentByClass<UARPGInventoryComponent>() : nullptr;
    if (!Inventory) return 0;
    TMap<FName, int32> Costs;
    ARPGAggregateBuildCosts(Piece, Costs);
    if (Costs.Num() == 0) return 999;
    int32 Count = MAX_int32;
    for (const TPair<FName, int32>& Cost : Costs)
        Count = FMath::Min(Count, Inventory->GetUnequippedItemCount(Cost.Key) / FMath::Max(1, Cost.Value));
    return FMath::Max(0, Count == MAX_int32 ? 0 : Count);
}

bool UARPGBuildingComponent::ConsumeBuildResources(const UARPGBuildPieceDefinition* Piece)
{
    if (!bConsumeResources) return true;
    UARPGInventoryComponent* Inventory = GetOwner() ? GetOwner()->FindComponentByClass<UARPGInventoryComponent>() : nullptr;
    if (!Inventory || !Piece || !HasBuildResources(Piece)) return false;
    TMap<FName, int32> Costs;
    ARPGAggregateBuildCosts(Piece, Costs);
    TArray<TPair<FName, int32>> Removed;
    for (const TPair<FName, int32>& Cost : Costs)
    {
        if (!Inventory->RemoveUnequippedItem(Cost.Key, Cost.Value))
        {
            for (const TPair<FName, int32>& Rollback : Removed) Inventory->AddItem(Rollback.Key, Rollback.Value);
            return false;
        }
        Removed.Add(Cost);
    }
    return true;
}

void UARPGBuildingComponent::RefundBuildResources(const UARPGBuildPieceDefinition* Piece)
{
    if (!bConsumeResources || !Piece) return;
    UARPGInventoryComponent* Inventory = GetOwner() ? GetOwner()->FindComponentByClass<UARPGInventoryComponent>() : nullptr;
    if (!Inventory) return;
    for (const FARPGItemAmount& Cost : Piece->BuildCost)
    {
        if (Cost.Quantity <= 0) continue;
        if (Cost.Item) Inventory->AddItemDefinition(Cost.Item, Cost.Quantity);
        else if (!ARPGResolveBuildCostId(Cost).IsNone()) Inventory->AddItem(ARPGResolveBuildCostId(Cost), Cost.Quantity);
    }
}

EARPGPlacementResult UARPGBuildingComponent::EvaluatePlacementInternal(const UARPGBuildPieceDefinition* Piece, const FTransform& Final, const AARPGBuildPieceActor* SnapTarget) const
{
    if (!Piece) return EARPGPlacementResult::NoPiece;
    AActor* Owner = GetOwner();
    UWorld* World = GetWorld();
    if (!Owner || !World) return EARPGPlacementResult::Restricted;
    if (!bAllowUnlistedBuildRequests && !BuildCatalog.ContainsByPredicate([&](const TObjectPtr<UARPGBuildPieceDefinition>& CatalogPiece){ return CatalogPiece.Get() == Piece; })) return EARPGPlacementResult::Restricted;
    if (SnapTarget && bRequireSnapTargetModificationAccess && !SnapTarget->CanActorModify(Owner)) return EARPGPlacementResult::Restricted;
    if (FVector::DistSquared(Owner->GetActorLocation(), Final.GetLocation()) > FMath::Square(MaxPlacementDistance)) return EARPGPlacementResult::TooFar;
    if (!HasBuildResources(Piece)) return EARPGPlacementResult::MissingResources;
    if (Piece->bRequiresSnapTarget && !SnapTarget) return EARPGPlacementResult::Unsupported;

    const UARPGFactionComponent* Faction = Owner->FindComponentByClass<UARPGFactionComponent>();
    if (!Piece->RequiredBuilderFactionId.IsNone())
    {
        if (!Faction || Faction->GetPrimaryFactionId() != Piece->RequiredBuilderFactionId || Faction->GetReputation(Piece->RequiredBuilderFactionId) < Piece->MinimumBuilderReputation)
            return EARPGPlacementResult::Restricted;
    }
    for (TActorIterator<AARPGFactionTerritoryVolume> It(World); It; ++It)
    {
        AARPGFactionTerritoryVolume* Territory = *It;
        if (Territory && Territory->GetComponentsBoundingBox(true).IsInside(Final.GetLocation()) && !Territory->CanActorBuildHere(Owner)) return EARPGPlacementResult::Restricted;
    }

    FCollisionQueryParams Params(SCENE_QUERY_STAT(ARPGBuildPlacement), false, Owner);
    if (SnapTarget) Params.AddIgnoredActor(SnapTarget);
    const FVector Clearance(FMath::Max(0.f, Piece->PlacementCollisionClearance));
    const FVector Extents = (Piece->PlacementBounds - Clearance).ComponentMax(FVector(1.f));
    const FVector PlacementBoundsCenter = Final.TransformPosition(ARPGGetBuildPieceBoundsCenterLocal(Piece));
    TArray<FOverlapResult> Overlaps;
    if (World->OverlapMultiByChannel(Overlaps, PlacementBoundsCenter, Final.GetRotation(), PlacementCollisionChannel, FCollisionShape::MakeBox(Extents), Params))
    {
        for (const FOverlapResult& Overlap : Overlaps)
        {
            AActor* Other = Overlap.GetActor();
            if (!Other || Other == Owner || Other == SnapTarget) continue;

            // Valid modular seams/corners can have deliberate art overlap. Only tolerate another
            // completed build actor when this exact final transform is one of that actor's declared
            // snap neighbours. Arbitrary build-piece clipping and all non-building blockers remain blocked.
            if (AARPGBuildPieceActor* BuildNeighbor = Cast<AARPGBuildPieceActor>(Other))
            {
                if (ARPGIsValidSnappedBuildNeighbor(BuildNeighbor, Piece, Final))
                {
                    // Treat an accepted seam neighbour like an additional snap relationship for access
                    // control, so a player cannot exploit corner tolerance to intersect protected builds.
                    if (bRequireSnapTargetModificationAccess && !BuildNeighbor->CanActorModify(Owner))
                        return EARPGPlacementResult::Restricted;
                    continue;
                }
            }

            return EARPGPlacementResult::Blocked;
        }
    }

    if (Piece->bRequiresSupport && !SnapTarget)
    {
        if (!Piece->bAllowGroundPlacement) return EARPGPlacementResult::Unsupported;
        FHitResult Hit;
        const FVector BottomAnchor = Final.TransformPosition(ARPGGetBuildPieceBottomAnchorLocal(Piece));
        const float ProbeLift = FMath::Max(4.f, Piece->PlacementCollisionClearance + 2.f);
        const FVector Start = BottomAnchor + FVector::UpVector * ProbeLift;
        const FVector End = BottomAnchor - FVector::UpVector * FMath::Max(1.f, Piece->SupportTraceDepth);
        if (!World->LineTraceSingleByChannel(Hit, Start, End, PlacementCollisionChannel, Params)) return EARPGPlacementResult::Unsupported;
        if (Piece->bRequireMostlyFlatGround)
        {
            const float Slope = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(Hit.ImpactNormal.Z, -1.f, 1.f)));
            if (Slope > Piece->MaxGroundSlopeDegrees) return EARPGPlacementResult::InvalidSurface;
        }
    }
    return EARPGPlacementResult::Valid;
}

EARPGPlacementResult UARPGBuildingComponent::EvaluatePlacement(const UARPGBuildPieceDefinition* Piece, const FTransform& DesiredTransform) const
{
    AARPGBuildPieceActor* SnapTarget = nullptr;
    const FTransform Final = ResolvePlacementTransform(Piece, DesiredTransform, SnapTarget);
    return EvaluatePlacementInternal(Piece, Final, SnapTarget);
}

bool UARPGBuildingComponent::ConfirmPreviewPlacement()
{
    if (!bBuildModeActive || !SelectedBuildPiece || CurrentPreviewResult != EARPGPlacementResult::Valid) return false;
    const bool bRequested = RequestPlacePiece(SelectedBuildPiece, CurrentPreviewTransform);
    if (bRequested && !bKeepBuildModeAfterPlacement) EndBuildMode();
    return bRequested;
}

bool UARPGBuildingComponent::RequestPlacePiece(UARPGBuildPieceDefinition* Piece, const FTransform& DesiredTransform)
{
    if (!GetOwner() || !Piece) return false;
    if (!GetOwner()->HasAuthority())
    {
        ServerPlacePiece(Piece, DesiredTransform);
        return true;
    }
    return PlacePieceAuthority(Piece, DesiredTransform);
}

void UARPGBuildingComponent::ServerPlacePiece_Implementation(UARPGBuildPieceDefinition* Piece, FTransform DesiredTransform)
{
    PlacePieceAuthority(Piece, DesiredTransform);
}

UClass* UARPGBuildingComponent::ResolveNativeBuildActorClass(const UARPGBuildPieceDefinition* Piece) const
{
    if (!Piece) return nullptr;
    if (UClass* CustomClass = Piece->ActorClass.LoadSynchronous())
        if (CustomClass->IsChildOf(AARPGBuildPieceActor::StaticClass())) return CustomClass;
    switch (Piece->PieceKind)
    {
        case EARPGBuildPieceKind::Door: return AARPGBuildDoorActor::StaticClass();
        case EARPGBuildPieceKind::Storage: return AARPGStorageActor::StaticClass();
        case EARPGBuildPieceKind::Production: return AARPGCraftingStationActor::StaticClass();
        default: return AARPGBuildPieceActor::StaticClass();
    }
}

bool UARPGBuildingComponent::PlacePieceAuthority(UARPGBuildPieceDefinition* Piece, const FTransform& DesiredTransform)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || !Piece || !GetWorld()) return false;
    AARPGBuildPieceActor* SnapTarget = nullptr;
    const FTransform Final = ResolvePlacementTransform(Piece, DesiredTransform, SnapTarget);
    const EARPGPlacementResult Check = EvaluatePlacementInternal(Piece, Final, SnapTarget);
    if (Check != EARPGPlacementResult::Valid)
    {
        OnPlacementResult.Broadcast(Check, nullptr);
        return false;
    }

    UClass* LoadedClass = ResolveNativeBuildActorClass(Piece);
    if (!LoadedClass)
    {
        OnPlacementResult.Broadcast(EARPGPlacementResult::NoPiece, nullptr);
        return false;
    }
    if (!ConsumeBuildResources(Piece))
    {
        OnPlacementResult.Broadcast(EARPGPlacementResult::MissingResources, nullptr);
        return false;
    }

    FActorSpawnParameters Params;
    Params.Owner = GetOwner();
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AARPGBuildPieceActor* Spawned = GetWorld()->SpawnActor<AARPGBuildPieceActor>(LoadedClass, Final, Params);
    if (!Spawned)
    {
        RefundBuildResources(Piece);
        return false;
    }

    if (AARPGStorageActor* Storage = Cast<AARPGStorageActor>(Spawned))
        if (Storage->Inventory) Storage->Inventory->MaxSlots = FMath::Max(1, Piece->StorageSlots);
    if (AARPGCraftingStationActor* Station = Cast<AARPGCraftingStationActor>(Spawned))
        Station->ApplyStationDefinition(Piece->StationDefinition);

    Spawned->InitializeBuilding(Piece, GetOwner());
    if (UARPGEventRouterComponent* Router = GetOwner()->FindComponentByClass<UARPGEventRouterComponent>()) Router->ReportBuilt(Piece->DefinitionId, 1);
    OnPlacementResult.Broadcast(EARPGPlacementResult::Valid, Spawned);
    return true;
}
