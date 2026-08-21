#include "Building/ARPGBuildPathActor.h"
#include "Components/SplineComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Data/ARPGBuildPieceDefinition.h"
#include "Engine/StaticMesh.h"
#include "Net/UnrealNetwork.h"

static ESplineMeshAxis::Type ARPGResolveSettlementPathForwardAxis(EARPGSettlementPathForwardAxis Axis)
{
    switch (Axis)
    {
    case EARPGSettlementPathForwardAxis::Y: return ESplineMeshAxis::Y;
    case EARPGSettlementPathForwardAxis::Z: return ESplineMeshAxis::Z;
    case EARPGSettlementPathForwardAxis::X:
    default: return ESplineMeshAxis::X;
    }
}

static FVector ARPGResolveStableSettlementPathEndpointTangent(
    const USplineComponent* Spline,
    int32 PointIndex,
    const FVector& DesiredTangentDirectionLocal)
{
    if (!Spline || DesiredTangentDirectionLocal.IsNearlyZero()) return FVector::ZeroVector;

    const int32 PointCount = Spline->GetNumberOfSplinePoints();
    if (PointCount < 2 || PointIndex < 0 || PointIndex >= PointCount) return FVector::ZeroVector;

    const bool bStartPoint = PointIndex == 0;
    const bool bEndPoint = PointIndex == PointCount - 1;
    if (!bStartPoint && !bEndPoint) return FVector::ZeroVector;

    const int32 NeighborIndex = bStartPoint ? 1 : PointCount - 2;
    const FVector Point = Spline->GetLocationAtSplinePoint(PointIndex, ESplineCoordinateSpace::Local);
    const FVector Neighbor = Spline->GetLocationAtSplinePoint(NeighborIndex, ESplineCoordinateSpace::Local);
    const FVector LocalTravel = bStartPoint ? (Neighbor - Point) : (Point - Neighbor);
    const float AdjacentSpan = LocalTravel.Size();
    if (AdjacentSpan <= KINDA_SMALL_NUMBER) return FVector::ZeroVector;

    const FVector TravelDirection = LocalTravel / AdjacentSpan;
    FVector DesiredDirection = DesiredTangentDirectionLocal.GetSafeNormal();

    // A tangent at a spline endpoint is applied to the adjacent sampled interval, not the entire
    // player-authored path segment. v2.16.11 used whole-segment magnitudes here; on a 125 cm terrain
    // interval a 600-1200 cm turn tangent can overshoot the Hermite curve and fold a wide road mesh
    // into spikes. Persist only direction semantics and derive a bounded magnitude from this local span.
    // Extremely sharp reversals cannot have a useful shared bisector without a real turn-radius solver,
    // so fall back to the segment's travel direction instead of permitting endpoint fold-back.
    constexpr float MinimumForwardDot = 0.15f;
    if (FVector::DotProduct(DesiredDirection, TravelDirection) < MinimumForwardDot)
        DesiredDirection = TravelDirection;

    return DesiredDirection * AdjacentSpan;
}

AARPGBuildPathActor::AARPGBuildPathActor()
{
    PathSpline = CreateDefaultSubobject<USplineComponent>(TEXT("PathSpline"));
    PathSpline->SetupAttachment(BuildRoot);
    PathSpline->SetMobility(EComponentMobility::Movable);
    PathSpline->SetClosedLoop(false);
    PathSpline->SetDrawDebug(false);
    PathSpline->SetCanEverAffectNavigation(false);
}

bool AARPGBuildPathActor::InitializePathSegment(UARPGBuildPieceDefinition* InDefinition, AActor* Builder, FVector StartWorld, FVector EndWorld)
{
    if (!HasAuthority() || !InDefinition || InDefinition->PieceKind != EARPGBuildPieceKind::SettlementPath) return false;
    if ((EndWorld - StartWorld).SizeSquared2D() <= KINDA_SMALL_NUMBER) return false;

    const FVector Direction = EndWorld - StartWorld;
    SetActorLocationAndRotation(StartWorld, FRotator(0.f, Direction.Rotation().Yaw, 0.f), false, nullptr, ETeleportType::TeleportPhysics);
    InitializeBuilding(InDefinition, Builder);

    PathStartLocal = GetActorTransform().InverseTransformPosition(StartWorld);
    PathEndLocal = GetActorTransform().InverseTransformPosition(EndWorld);
    PathStartTangentLocal = FVector::ZeroVector;
    PathEndTangentLocal = FVector::ZeroVector;
    RefreshPathPresentation();
    ForceNetUpdate();
    return true;
}

void AARPGBuildPathActor::RestorePathGeometry(FVector SavedStartLocal, FVector SavedEndLocal, FVector SavedStartTangentLocal, FVector SavedEndTangentLocal)
{
    if (!HasAuthority()) return;
    PathStartLocal = SavedStartLocal;
    PathEndLocal = SavedEndLocal;
    // v9 files written by v2.16.11 may contain whole-segment tangent magnitudes. Preserve their
    // directions but normalize them on restore so the next save is self-healed under v2.16.12.
    PathStartTangentLocal = SavedStartTangentLocal.IsNearlyZero() ? FVector::ZeroVector : SavedStartTangentLocal.GetSafeNormal();
    PathEndTangentLocal = SavedEndTangentLocal.IsNearlyZero() ? FVector::ZeroVector : SavedEndTangentLocal.GetSafeNormal();
    RefreshPathPresentation();
    ForceNetUpdate();
}

void AARPGBuildPathActor::SetPathEndpointTangentsWorld(FVector StartTangentWorld, FVector EndTangentWorld)
{
    if (!HasAuthority()) return;
    // Store direction only. RebuildSplineFromGeometry derives a stable magnitude from the adjacent
    // terrain-sampled span, which also automatically sanitizes oversized tangent vectors from v9 saves
    // created by v2.16.11 without changing the save schema.
    PathStartTangentLocal = StartTangentWorld.IsNearlyZero()
        ? FVector::ZeroVector
        : GetActorTransform().InverseTransformVectorNoScale(StartTangentWorld).GetSafeNormal();
    PathEndTangentLocal = EndTangentWorld.IsNearlyZero()
        ? FVector::ZeroVector
        : GetActorTransform().InverseTransformVectorNoScale(EndTangentWorld).GetSafeNormal();
    RefreshPathPresentation();
    ForceNetUpdate();
}

FVector AARPGBuildPathActor::GetPathStartWorld() const
{
    return GetActorTransform().TransformPosition(PathStartLocal);
}

FVector AARPGBuildPathActor::GetPathEndWorld() const
{
    return GetActorTransform().TransformPosition(PathEndLocal);
}

float AARPGBuildPathActor::GetPathSegmentLength() const
{
    return FVector::Distance(GetPathStartWorld(), GetPathEndWorld());
}

void AARPGBuildPathActor::DestroyGeneratedSplineMeshes()
{
    for (USplineMeshComponent* Component : PathMeshComponents)
        if (IsValid(Component)) Component->DestroyComponent();
    PathMeshComponents.Reset();
}

void AARPGBuildPathActor::RebuildSplineFromGeometry()
{
    DestroyGeneratedSplineMeshes();
    if (!PathSpline) return;
    PathSpline->ClearSplinePoints(false);

    if (!Definition || Definition->PieceKind != EARPGBuildPieceKind::SettlementPath) return;
    UStaticMesh* SegmentMesh = Definition->BuildMesh.LoadSynchronous();
    if (!SegmentMesh) return;

    const FVector StartWorld = GetPathStartWorld();
    const FVector EndWorld = GetPathEndWorld();
    const float HorizontalLength = FVector::Dist2D(StartWorld, EndWorld);
    if (HorizontalLength <= KINDA_SMALL_NUMBER) return;

    const float Spacing = FMath::Max(25.f, Definition->SettlementPathTerrainSampleSpacing);
    const int32 IntervalCount = FMath::Clamp(FMath::CeilToInt(HorizontalLength / Spacing), 1, 32);
    const float TraceUp = FMath::Max(25.f, Definition->SettlementPathTerrainTraceHeight);
    const float TraceDown = FMath::Max(25.f, Definition->SettlementPathTerrainTraceDepth);
    const float GroundOffset = FMath::Max(0.f, Definition->SettlementPathGroundOffset);

    FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(ARPGSettlementPathTerrain), false, this);
    for (int32 Index = 0; Index <= IntervalCount; ++Index)
    {
        const float Alpha = static_cast<float>(Index) / static_cast<float>(IntervalCount);
        FVector SampleWorld = FMath::Lerp(StartWorld, EndWorld, Alpha);

        // Preserve the authority-confirmed endpoints exactly. Intermediate points are cosmetic terrain
        // samples, so the generated road bends over small Landscape undulations without expanding save
        // or replication payloads with redundant derived geometry.
        if (Index > 0 && Index < IntervalCount && GetWorld())
        {
            FHitResult Hit;
            const FVector TraceStart = SampleWorld + FVector::UpVector * TraceUp;
            const FVector TraceEnd = SampleWorld - FVector::UpVector * TraceDown;
            FCollisionQueryParams SampleParams = TraceParams;
            constexpr int32 MaxPathPierceCount = 32;
            for (int32 Attempt = 0; Attempt < MaxPathPierceCount; ++Attempt)
            {
                if (!GetWorld()->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_Visibility, SampleParams)) break;
                if (AARPGBuildPathActor* ExistingPath = Cast<AARPGBuildPathActor>(Hit.GetActor()))
                {
                    SampleParams.AddIgnoredActor(ExistingPath);
                    continue;
                }
                SampleWorld = Hit.ImpactPoint + FVector::UpVector * GroundOffset;
                break;
            }
        }

        const FVector LocalPoint = GetActorTransform().InverseTransformPosition(SampleWorld);
        PathSpline->AddSplinePoint(LocalPoint, ESplineCoordinateSpace::Local, false);
        PathSpline->SetSplinePointType(Index, ESplinePointType::CurveClamped, false);
    }
    PathSpline->UpdateSpline();
    const int32 LastPointIndex = PathSpline->GetNumberOfSplinePoints() - 1;
    if (!PathStartTangentLocal.IsNearlyZero())
    {
        const FVector SafeStartTangent = ARPGResolveStableSettlementPathEndpointTangent(PathSpline, 0, PathStartTangentLocal);
        if (!SafeStartTangent.IsNearlyZero())
            PathSpline->SetTangentAtSplinePoint(0, SafeStartTangent, ESplineCoordinateSpace::Local, false);
    }
    if (LastPointIndex > 0 && !PathEndTangentLocal.IsNearlyZero())
    {
        const FVector SafeEndTangent = ARPGResolveStableSettlementPathEndpointTangent(PathSpline, LastPointIndex, PathEndTangentLocal);
        if (!SafeEndTangent.IsNearlyZero())
            PathSpline->SetTangentAtSplinePoint(LastPointIndex, SafeEndTangent, ESplineCoordinateSpace::Local, false);
    }
    PathSpline->UpdateSpline();

    const ESplineMeshAxis::Type ForwardAxis = ARPGResolveSettlementPathForwardAxis(Definition->SettlementPathForwardAxis);
    const FVector2D CrossSectionScale(
        FMath::Max(0.01f, Definition->SettlementPathMeshScale.X),
        FMath::Max(0.01f, Definition->SettlementPathMeshScale.Y));
    const float TangentScale = FMath::Max(0.05f, Definition->SettlementPathTangentScale);

    for (int32 Index = 0; Index < PathSpline->GetNumberOfSplinePoints() - 1; ++Index)
    {
        USplineMeshComponent* MeshComponent = NewObject<USplineMeshComponent>(this);
        if (!MeshComponent) continue;
        MeshComponent->SetupAttachment(PathSpline);
        MeshComponent->SetMobility(EComponentMobility::Movable);
        MeshComponent->SetStaticMesh(SegmentMesh);
        MeshComponent->SetForwardAxis(ForwardAxis, false);
        MeshComponent->SetStartScale(CrossSectionScale, false);
        MeshComponent->SetEndScale(CrossSectionScale, false);
        MeshComponent->SetCastShadow(Definition->bSettlementPathCastShadow);
        MeshComponent->SetGenerateOverlapEvents(false);
        MeshComponent->SetCanEverAffectNavigation(false);
        MeshComponent->bReceivesDecals = true;
        MeshComponent->SetCollisionEnabled(Definition->bSettlementPathCollisionEnabled && IsConstructionComplete()
            ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);

        const FVector Start = PathSpline->GetLocationAtSplinePoint(Index, ESplineCoordinateSpace::Local);
        const FVector End = PathSpline->GetLocationAtSplinePoint(Index + 1, ESplineCoordinateSpace::Local);
        const FVector StartTangent = PathSpline->GetTangentAtSplinePoint(Index, ESplineCoordinateSpace::Local) * TangentScale;
        const FVector EndTangent = PathSpline->GetTangentAtSplinePoint(Index + 1, ESplineCoordinateSpace::Local) * TangentScale;
        MeshComponent->SetStartAndEnd(Start, StartTangent, End, EndTangent, false);
        MeshComponent->RegisterComponent();
        MeshComponent->UpdateMesh();
        PathMeshComponents.Add(MeshComponent);
    }
}

void AARPGBuildPathActor::RefreshPathPresentation()
{
    RebuildSplineFromGeometry();
    RefreshConstructionPresentation(true);
    OnPathGeometryChanged.Broadcast();
}

void AARPGBuildPathActor::RefreshDefinitionPresentation()
{
    Super::RefreshDefinitionPresentation();

    // BuildMesh is the deformation source, not a second rigid world mesh. Keep the inherited visual
    // components dormant while preserving all base building state/ownership/health behavior.
    if (BuildMesh)
    {
        BuildMesh->SetVisibility(false, true);
        BuildMesh->SetHiddenInGame(true, true);
        BuildMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }
    if (BuildSkeletalMesh)
    {
        BuildSkeletalMesh->SetVisibility(false, true);
        BuildSkeletalMesh->SetHiddenInGame(true, true);
        BuildSkeletalMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }
    RebuildSplineFromGeometry();
}

void AARPGBuildPathActor::RefreshConstructionPresentation(bool bForce)
{
    // Do not use the base rigid-mesh Z-scale reveal on a deformed spline. Drive the same material
    // progress contract across every generated section and gate optional path collision until complete.
    const float Progress = GetConstructionProgress01();
    for (USplineMeshComponent* Component : PathMeshComponents)
    {
        if (!IsValid(Component)) continue;
        Component->SetScalarParameterValueOnMaterials(TEXT("ConstructionProgress"), Progress);
        Component->SetScalarParameterValueOnMaterials(TEXT("BuildProgress"), Progress);
        if (Definition)
            Component->SetScalarParameterValueOnMaterials(Definition->ConstructionProgressMaterialParameter, Progress);
        Component->SetCollisionEnabled(Definition && Definition->bSettlementPathCollisionEnabled && IsConstructionComplete()
            ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
    }

    // The inherited mesh is intentionally hidden, but calling Super keeps construction completion,
    // Tick enablement and Blueprint progress events aligned with every other build piece.
    Super::RefreshConstructionPresentation(bForce);
    if (BuildMesh)
    {
        BuildMesh->SetVisibility(false, true);
        BuildMesh->SetHiddenInGame(true, true);
        BuildMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }
    if (BuildSkeletalMesh)
    {
        BuildSkeletalMesh->SetVisibility(false, true);
        BuildSkeletalMesh->SetHiddenInGame(true, true);
        BuildSkeletalMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }
}

void AARPGBuildPathActor::OnRep_PathGeometry()
{
    RefreshPathPresentation();
}

void AARPGBuildPathActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AARPGBuildPathActor, PathStartLocal);
    DOREPLIFETIME(AARPGBuildPathActor, PathEndLocal);
    DOREPLIFETIME(AARPGBuildPathActor, PathStartTangentLocal);
    DOREPLIFETIME(AARPGBuildPathActor, PathEndTangentLocal);
}
