#include "Building/ARPGBuildPreviewActor.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Data/ARPGBuildPieceDefinition.h"
#include "Materials/MaterialInterface.h"
#include "Engine/StaticMesh.h"

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
}

void AARPGBuildPreviewActor::ConfigurePreview(const UARPGBuildPieceDefinition* Piece, UMaterialInterface* ValidMaterial, UMaterialInterface* InvalidMaterial)
{
    ValidPreviewMaterial = ValidMaterial;
    InvalidPreviewMaterial = InvalidMaterial;
    if (PreviewMesh && Piece)
    {
        UStaticMesh* Mesh = Piece->PreviewMesh.LoadSynchronous();
        if (!Mesh) Mesh = Piece->BuildMesh.LoadSynchronous();
        PreviewMesh->SetStaticMesh(Mesh);
        PreviewMesh->SetRelativeTransform(Piece->MeshRelativeTransform);
        PreviewMesh->SetScalarParameterValueOnMaterials(TEXT("PreviewOpacity"), 0.45f);
    }
    ApplyMaterialState();
}

void AARPGBuildPreviewActor::SetPlacementResult(EARPGPlacementResult Result)
{
    if (LastResult == Result) return;
    LastResult = Result;
    ApplyMaterialState();
}

void AARPGBuildPreviewActor::ApplyMaterialState()
{
    if (!PreviewMesh) return;
    UMaterialInterface* Override = LastResult == EARPGPlacementResult::Valid ? ValidPreviewMaterial.Get() : InvalidPreviewMaterial.Get();
    if (Override)
    {
        const int32 Num = FMath::Max(1, PreviewMesh->GetNumMaterials());
        for (int32 Index = 0; Index < Num; ++Index) PreviewMesh->SetMaterial(Index, Override);
    }
    const FLinearColor Tint = LastResult == EARPGPlacementResult::Valid ? FLinearColor(0.15f, 1.f, 0.25f, 1.f) : FLinearColor(1.f, 0.12f, 0.08f, 1.f);
    PreviewMesh->SetVectorParameterValueOnMaterials(TEXT("PreviewTint"), FVector(Tint.R, Tint.G, Tint.B));
    PreviewMesh->SetScalarParameterValueOnMaterials(TEXT("PlacementValid"), LastResult == EARPGPlacementResult::Valid ? 1.f : 0.f);
}
