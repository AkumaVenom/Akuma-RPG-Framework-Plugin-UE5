#include "Building/ARPGBuildPreviewActor.h"
#include "Building/ARPGBuildPathActor.h"
#include "AkumasRPGFramework.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/MeshComponent.h"
#include "Components/SplineComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Data/ARPGBuildPieceDefinition.h"
#include "Materials/MaterialInterface.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"

/**
 * Skeletal placement ghosts need the SkeletalMesh material permutation. Static-only preview
 * materials otherwise render with Unreal's fallback/default surface even though SetMaterial() was
 * called correctly. UMaterialInterface::CheckMaterialUsage is intentionally used here because in
 * editor/PIE it can enable/recompile the missing usage permutation on the assigned master material;
 * packaged builds still get a deterministic compatibility check instead of silently assuming it.
 */
static void ARPGPrepareSkeletalPreviewMaterial(UMaterialInterface* Material)
{
    if (!Material) return;

    if (!Material->CheckMaterialUsage(MATUSAGE_SkeletalMesh))
    {
        UE_LOG(LogARPG, Warning, TEXT("Building preview material '%s' is not usable with Skeletal Meshes; the skeletal placement ghost may use Unreal's fallback material."), *GetNameSafe(Material));
        return;
    }

    // CheckMaterialUsage can request a shader recompile in editor/PIE. Ensure the permutation is ready
    // before the first skeletal ghost is rendered so it does not remain grey for the placement session.
    Material->EnsureIsComplete();
}

AARPGBuildPreviewActor::AARPGBuildPreviewActor()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = false;
    SetActorEnableCollision(false);
    PreviewRoot = CreateDefaultSubobject<USceneComponent>(TEXT("PreviewRoot"));
    RootComponent = PreviewRoot;
    PreviewMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PreviewMesh"));
    PreviewMesh->SetupAttachment(PreviewRoot);
    PreviewMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    PreviewMesh->SetGenerateOverlapEvents(false);
    PreviewMesh->SetCastShadow(false);
    PreviewMesh->bReceivesDecals = false;

    PreviewSkeletalMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("PreviewSkeletalMesh"));
    PreviewSkeletalMesh->SetupAttachment(PreviewRoot);
    PreviewSkeletalMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    PreviewSkeletalMesh->SetGenerateOverlapEvents(false);
    PreviewSkeletalMesh->SetCastShadow(false);
    PreviewSkeletalMesh->bReceivesDecals = false;
    PreviewSkeletalMesh->SetVisibility(false, true);
    PreviewSkeletalMesh->SetHiddenInGame(true, true);
    // Placement ghosts never animate; do not add per-frame skeletal component work to build mode.
    PreviewSkeletalMesh->SetComponentTickEnabled(false);

    PathPreviewSpline = CreateDefaultSubobject<USplineComponent>(TEXT("PathPreviewSpline"));
    PathPreviewSpline->SetupAttachment(PreviewRoot);
    PathPreviewSpline->SetMobility(EComponentMobility::Movable);
    PathPreviewSpline->SetClosedLoop(false);
    PathPreviewSpline->SetDrawDebug(false);
    PathPreviewSpline->SetCanEverAffectNavigation(false);
}

void AARPGBuildPreviewActor::ConfigurePreview(const UARPGBuildPieceDefinition* Piece, UMaterialInterface* ValidMaterial, UMaterialInterface* InvalidMaterial)
{
    ClearSettlementPathSegmentPreview();
    ValidPreviewMaterial = ValidMaterial;
    InvalidPreviewMaterial = InvalidMaterial;
    if (Piece)
    {
        // Explicit preview assets win first. This deliberately allows a skeletal final piece to use a
        // lighter Static Mesh placement proxy. If neither preview field is authored, preview falls back
        // to the same skeletal-first Build visual selection used by the final actor.
        USkeletalMesh* SkeletalMesh = Piece->PreviewSkeletalMesh.IsNull() ? nullptr : Piece->PreviewSkeletalMesh.LoadSynchronous();
        UStaticMesh* StaticMesh = nullptr;
        if (!SkeletalMesh && !Piece->PreviewMesh.IsNull())
            StaticMesh = Piece->PreviewMesh.LoadSynchronous();
        if (!SkeletalMesh && !StaticMesh)
        {
            SkeletalMesh = Piece->BuildSkeletalMesh.IsNull() ? nullptr : Piece->BuildSkeletalMesh.LoadSynchronous();
            if (!SkeletalMesh && !Piece->BuildMesh.IsNull()) StaticMesh = Piece->BuildMesh.LoadSynchronous();
        }

        const bool bUseSkeletalMesh = SkeletalMesh != nullptr;
        if (bUseSkeletalMesh)
        {
            ARPGPrepareSkeletalPreviewMaterial(ValidPreviewMaterial.Get());
            ARPGPrepareSkeletalPreviewMaterial(InvalidPreviewMaterial.Get());
        }

        if (PreviewSkeletalMesh)
        {
            PreviewSkeletalMesh->SetSkeletalMesh(SkeletalMesh, true);
            PreviewSkeletalMesh->SetRelativeTransform(Piece->MeshRelativeTransform);
            PreviewSkeletalMesh->SetVisibility(bUseSkeletalMesh, true);
            PreviewSkeletalMesh->SetHiddenInGame(!bUseSkeletalMesh, true);
        }
        if (PreviewMesh)
        {
            PreviewMesh->SetStaticMesh(bUseSkeletalMesh ? nullptr : StaticMesh);
            PreviewMesh->SetRelativeTransform(Piece->MeshRelativeTransform);
            PreviewMesh->SetVisibility(!bUseSkeletalMesh && StaticMesh != nullptr, true);
            PreviewMesh->SetHiddenInGame(bUseSkeletalMesh || StaticMesh == nullptr, true);
        }

        if (UMeshComponent* ActiveMesh = GetActivePreviewMeshComponent())
            ActiveMesh->SetScalarParameterValueOnMaterials(TEXT("PreviewOpacity"), 0.45f);
    }
    ApplyMaterialState();
}

static ESplineMeshAxis::Type ARPGResolvePreviewPathForwardAxis(EARPGSettlementPathForwardAxis Axis)
{
    switch (Axis)
    {
    case EARPGSettlementPathForwardAxis::Y: return ESplineMeshAxis::Y;
    case EARPGSettlementPathForwardAxis::Z: return ESplineMeshAxis::Z;
    case EARPGSettlementPathForwardAxis::X:
    default: return ESplineMeshAxis::X;
    }
}

void AARPGBuildPreviewActor::DestroySettlementPathPreviewMeshes()
{
    for (USplineMeshComponent* Component : PathPreviewMeshComponents)
        if (IsValid(Component)) Component->DestroyComponent();
    PathPreviewMeshComponents.Reset();
    if (PathPreviewSpline) PathPreviewSpline->ClearSplinePoints(false);
}

void AARPGBuildPreviewActor::SetNormalPreviewVisibility(bool bVisible)
{
    if (PreviewMesh && PreviewMesh->GetStaticMesh())
    {
        PreviewMesh->SetVisibility(bVisible, true);
        PreviewMesh->SetHiddenInGame(!bVisible, true);
    }
    if (PreviewSkeletalMesh && PreviewSkeletalMesh->GetSkeletalMeshAsset())
    {
        PreviewSkeletalMesh->SetVisibility(bVisible, true);
        PreviewSkeletalMesh->SetHiddenInGame(!bVisible, true);
    }
}

void AARPGBuildPreviewActor::ClearSettlementPathSegmentPreview()
{
    DestroySettlementPathPreviewMeshes();
    bSettlementPathSegmentPreviewActive = false;
    SetNormalPreviewVisibility(true);
    ApplyMaterialState();
}

void AARPGBuildPreviewActor::RebuildSettlementPathPreview(const UARPGBuildPieceDefinition* Piece, FVector StartWorld, FVector EndWorld, FVector StartTangentDirectionWorld)
{
    if (!Piece || !PathPreviewSpline || Piece->PieceKind != EARPGBuildPieceKind::SettlementPath) return;
    PathPreviewSpline->ClearSplinePoints(false);
    UStaticMesh* SegmentMesh = Piece->BuildMesh.LoadSynchronous();
    if (!SegmentMesh) return;

    const float HorizontalLength = FVector::Dist2D(StartWorld, EndWorld);
    if (HorizontalLength <= KINDA_SMALL_NUMBER) return;

    // Preview actor stays at identity while the points are supplied in world-space coordinates. This
    // avoids accumulated transforms as the cursor moves and lets the spline reuse the final actor's
    // terrain sampling contract without spawning gameplay actors.
    SetActorTransform(FTransform::Identity, false, nullptr, ETeleportType::TeleportPhysics);
    const float Spacing = FMath::Max(25.f, Piece->SettlementPathTerrainSampleSpacing);
    const int32 IntervalCount = FMath::Clamp(FMath::CeilToInt(HorizontalLength / Spacing), 1, 32);
    const float TraceUp = FMath::Max(25.f, Piece->SettlementPathTerrainTraceHeight);
    const float TraceDown = FMath::Max(25.f, Piece->SettlementPathTerrainTraceDepth);
    const float GroundOffset = FMath::Max(0.f, Piece->SettlementPathGroundOffset);
    FCollisionQueryParams Params(SCENE_QUERY_STAT(ARPGSettlementPathPreviewTerrain), false, this);

    for (int32 Index = 0; Index <= IntervalCount; ++Index)
    {
        const float Alpha = static_cast<float>(Index) / static_cast<float>(IntervalCount);
        FVector Sample = FMath::Lerp(StartWorld, EndWorld, Alpha);
        if (Index > 0 && Index < IntervalCount && GetWorld())
        {
            FHitResult Hit;
            const FVector TraceStart = Sample + FVector::UpVector * TraceUp;
            const FVector TraceEnd = Sample - FVector::UpVector * TraceDown;
            FCollisionQueryParams SampleParams = Params;
            constexpr int32 MaxPathPierceCount = 32;
            for (int32 Attempt = 0; Attempt < MaxPathPierceCount; ++Attempt)
            {
                if (!GetWorld()->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_Visibility, SampleParams)) break;
                if (AARPGBuildPathActor* ExistingPath = Cast<AARPGBuildPathActor>(Hit.GetActor()))
                {
                    SampleParams.AddIgnoredActor(ExistingPath);
                    continue;
                }
                Sample = Hit.ImpactPoint + FVector::UpVector * GroundOffset;
                break;
            }
        }
        PathPreviewSpline->AddSplinePoint(Sample, ESplineCoordinateSpace::Local, false);
        PathPreviewSpline->SetSplinePointType(Index, ESplinePointType::CurveClamped, false);
    }
    PathPreviewSpline->UpdateSpline();

    // Mirror the final actor's bounded turn rule in the live preview. Tangent direction comes from
    // the previously confirmed segment; magnitude is derived only from this preview's first sampled
    // interval so a long pending segment cannot produce a fold or spike before confirmation.
    if (!StartTangentDirectionWorld.IsNearlyZero() && PathPreviewSpline->GetNumberOfSplinePoints() > 1)
    {
        const FVector Start = PathPreviewSpline->GetLocationAtSplinePoint(0, ESplineCoordinateSpace::Local);
        const FVector Next = PathPreviewSpline->GetLocationAtSplinePoint(1, ESplineCoordinateSpace::Local);
        const FVector LocalTravel = Next - Start;
        const float AdjacentSpan = LocalTravel.Size();
        if (AdjacentSpan > KINDA_SMALL_NUMBER)
        {
            const FVector TravelDirection = LocalTravel / AdjacentSpan;
            FVector DesiredDirection = StartTangentDirectionWorld.GetSafeNormal();
            constexpr float MinimumForwardDot = 0.15f;
            if (FVector::DotProduct(DesiredDirection, TravelDirection) < MinimumForwardDot)
                DesiredDirection = TravelDirection;
            PathPreviewSpline->SetTangentAtSplinePoint(0, DesiredDirection * AdjacentSpan, ESplineCoordinateSpace::Local, false);
            PathPreviewSpline->UpdateSpline();
        }
    }

    const ESplineMeshAxis::Type ForwardAxis = ARPGResolvePreviewPathForwardAxis(Piece->SettlementPathForwardAxis);
    const FVector2D CrossSectionScale(FMath::Max(0.01f, Piece->SettlementPathMeshScale.X), FMath::Max(0.01f, Piece->SettlementPathMeshScale.Y));
    const float TangentScale = FMath::Max(0.05f, Piece->SettlementPathTangentScale);
    UMaterialInterface* Override = LastResult == EARPGPlacementResult::Valid ? ValidPreviewMaterial.Get() : InvalidPreviewMaterial.Get();
    const FLinearColor Tint = LastResult == EARPGPlacementResult::Valid ? FLinearColor(0.15f, 1.f, 0.25f, 1.f) : FLinearColor(1.f, 0.12f, 0.08f, 1.f);

    const int32 NeededMeshCount = FMath::Max(0, PathPreviewSpline->GetNumberOfSplinePoints() - 1);
    while (PathPreviewMeshComponents.Num() < NeededMeshCount)
    {
        USplineMeshComponent* NewComponent = NewObject<USplineMeshComponent>(this);
        if (!NewComponent) break;
        NewComponent->SetupAttachment(PathPreviewSpline);
        NewComponent->SetMobility(EComponentMobility::Movable);
        NewComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        NewComponent->SetGenerateOverlapEvents(false);
        NewComponent->SetCastShadow(false);
        NewComponent->SetCanEverAffectNavigation(false);
        NewComponent->bReceivesDecals = false;
        NewComponent->RegisterComponent();
        PathPreviewMeshComponents.Add(NewComponent);
    }
    for (int32 Index = 0; Index < PathPreviewMeshComponents.Num(); ++Index)
        if (USplineMeshComponent* Component = PathPreviewMeshComponents[Index]) Component->SetVisibility(Index < NeededMeshCount, true);

    for (int32 Index = 0; Index < NeededMeshCount && Index < PathPreviewMeshComponents.Num(); ++Index)
    {
        USplineMeshComponent* Component = PathPreviewMeshComponents[Index];
        if (!Component) continue;
        Component->SetStaticMesh(SegmentMesh);
        Component->SetForwardAxis(ForwardAxis, false);
        Component->SetStartScale(CrossSectionScale, false);
        Component->SetEndScale(CrossSectionScale, false);
        const FVector Start = PathPreviewSpline->GetLocationAtSplinePoint(Index, ESplineCoordinateSpace::Local);
        const FVector End = PathPreviewSpline->GetLocationAtSplinePoint(Index + 1, ESplineCoordinateSpace::Local);
        const FVector StartTangent = PathPreviewSpline->GetTangentAtSplinePoint(Index, ESplineCoordinateSpace::Local) * TangentScale;
        const FVector EndTangent = PathPreviewSpline->GetTangentAtSplinePoint(Index + 1, ESplineCoordinateSpace::Local) * TangentScale;
        Component->SetStartAndEnd(Start, StartTangent, End, EndTangent, false);
        if (Override)
        {
            const int32 NumMaterials = FMath::Max(1, Component->GetNumMaterials());
            for (int32 MaterialIndex = 0; MaterialIndex < NumMaterials; ++MaterialIndex) Component->SetMaterial(MaterialIndex, Override);
        }
        Component->SetVectorParameterValueOnMaterials(TEXT("PreviewTint"), FVector(Tint.R, Tint.G, Tint.B));
        Component->SetScalarParameterValueOnMaterials(TEXT("PlacementValid"), LastResult == EARPGPlacementResult::Valid ? 1.f : 0.f);
        Component->SetScalarParameterValueOnMaterials(TEXT("PreviewOpacity"), 0.45f);
        Component->UpdateMesh();
    }
}

void AARPGBuildPreviewActor::SetSettlementPathSegmentPreview(const UARPGBuildPieceDefinition* Piece, FVector StartWorld, FVector EndWorld, EARPGPlacementResult Result, FVector StartTangentDirectionWorld)
{
    LastResult = Result;
    bSettlementPathSegmentPreviewActive = true;
    SetNormalPreviewVisibility(false);
    RebuildSettlementPathPreview(Piece, StartWorld, EndWorld, StartTangentDirectionWorld);
}

void AARPGBuildPreviewActor::SetPlacementResult(EARPGPlacementResult Result)
{
    if (LastResult == Result) return;
    LastResult = Result;
    ApplyMaterialState();
}

UMeshComponent* AARPGBuildPreviewActor::GetActivePreviewMeshComponent() const
{
    if (PreviewSkeletalMesh && PreviewSkeletalMesh->GetSkeletalMeshAsset()) return PreviewSkeletalMesh;
    if (PreviewMesh && PreviewMesh->GetStaticMesh()) return PreviewMesh;
    return nullptr;
}

void AARPGBuildPreviewActor::ApplyMaterialState()
{
    if (bSettlementPathSegmentPreviewActive) return;
    UMeshComponent* ActiveMesh = GetActivePreviewMeshComponent();
    if (!ActiveMesh) return;
    UMaterialInterface* Override = LastResult == EARPGPlacementResult::Valid ? ValidPreviewMaterial.Get() : InvalidPreviewMaterial.Get();
    if (Override)
    {
        const int32 Num = FMath::Max(1, ActiveMesh->GetNumMaterials());
        for (int32 Index = 0; Index < Num; ++Index) ActiveMesh->SetMaterial(Index, Override);
        // Especially important after a first-use material permutation compile in PIE. SetMaterial()
        // normally dirties render state itself; this explicit refresh keeps the skeletal path
        // deterministic across editor/runtime component registration timing.
        ActiveMesh->MarkRenderStateDirty();
    }
    const FLinearColor Tint = LastResult == EARPGPlacementResult::Valid ? FLinearColor(0.15f, 1.f, 0.25f, 1.f) : FLinearColor(1.f, 0.12f, 0.08f, 1.f);
    ActiveMesh->SetVectorParameterValueOnMaterials(TEXT("PreviewTint"), FVector(Tint.R, Tint.G, Tint.B));
    ActiveMesh->SetScalarParameterValueOnMaterials(TEXT("PlacementValid"), LastResult == EARPGPlacementResult::Valid ? 1.f : 0.f);
}
