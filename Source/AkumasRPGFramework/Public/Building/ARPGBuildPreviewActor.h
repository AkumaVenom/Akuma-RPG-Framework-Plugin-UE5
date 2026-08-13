#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ARPGTypes.h"
#include "ARPGBuildPreviewActor.generated.h"

class USceneComponent;
class UStaticMeshComponent;
class UARPGBuildPieceDefinition;
class UMaterialInterface;

/** Local-only placement ghost. It never owns gameplay state and never replicates. */
UCLASS(NotBlueprintable, Transient)
class AKUMASRPGFRAMEWORK_API AARPGBuildPreviewActor : public AActor
{
    GENERATED_BODY()
public:
    AARPGBuildPreviewActor();
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Preview") TObjectPtr<USceneComponent> PreviewRoot;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Preview") TObjectPtr<UStaticMeshComponent> PreviewMesh;
    UFUNCTION() void ConfigurePreview(const UARPGBuildPieceDefinition* Piece, UMaterialInterface* ValidMaterial, UMaterialInterface* InvalidMaterial);
    UFUNCTION() void SetPlacementResult(EARPGPlacementResult Result);
private:
    UPROPERTY(Transient) TObjectPtr<UMaterialInterface> ValidPreviewMaterial = nullptr;
    UPROPERTY(Transient) TObjectPtr<UMaterialInterface> InvalidPreviewMaterial = nullptr;
    EARPGPlacementResult LastResult = EARPGPlacementResult::NoPiece;
    void ApplyMaterialState();
};
