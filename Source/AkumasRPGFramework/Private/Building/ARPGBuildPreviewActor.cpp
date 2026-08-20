#include "Building/ARPGBuildPreviewActor.h"
#include "AkumasRPGFramework.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/MeshComponent.h"
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
}

void AARPGBuildPreviewActor::ConfigurePreview(const UARPGBuildPieceDefinition* Piece, UMaterialInterface* ValidMaterial, UMaterialInterface* InvalidMaterial)
{
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
