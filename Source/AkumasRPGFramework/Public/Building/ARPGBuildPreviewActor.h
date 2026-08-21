#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ARPGTypes.h"
#include "ARPGBuildPreviewActor.generated.h"

class USceneComponent;
class UStaticMeshComponent;
class USkeletalMeshComponent;
class UMeshComponent;
class USplineComponent;
class USplineMeshComponent;
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
    /** Existing Static Mesh ghost component. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Preview") TObjectPtr<UStaticMeshComponent> PreviewMesh;
    /** Skeletal ghost component used when a skeletal preview/build asset is selected. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Preview") TObjectPtr<USkeletalMeshComponent> PreviewSkeletalMesh;
    /** Local-only terrain-conforming Settlement Path preview spline. Dormant for every other piece kind. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Preview|Settlement Path") TObjectPtr<USplineComponent> PathPreviewSpline;
    UFUNCTION() void ConfigurePreview(const UARPGBuildPieceDefinition* Piece, UMaterialInterface* ValidMaterial, UMaterialInterface* InvalidMaterial);
    UFUNCTION() void SetPlacementResult(EARPGPlacementResult Result);
    /** Switches the ghost from its point marker into a live connecting Settlement Path spline. */
    UFUNCTION() void SetSettlementPathSegmentPreview(const UARPGBuildPieceDefinition* Piece, FVector StartWorld, FVector EndWorld, EARPGPlacementResult Result, FVector StartTangentDirectionWorld);
    /** Returns the ghost to the normal point/mesh presentation used before the first path anchor is confirmed. */
    UFUNCTION() void ClearSettlementPathSegmentPreview();
private:
    UPROPERTY(Transient) TObjectPtr<UMaterialInterface> ValidPreviewMaterial = nullptr;
    UPROPERTY(Transient) TObjectPtr<UMaterialInterface> InvalidPreviewMaterial = nullptr;
    EARPGPlacementResult LastResult = EARPGPlacementResult::NoPiece;
    bool bSettlementPathSegmentPreviewActive = false;
    UPROPERTY(Transient) TArray<TObjectPtr<USplineMeshComponent>> PathPreviewMeshComponents;
    UMeshComponent* GetActivePreviewMeshComponent() const;
    void ApplyMaterialState();
    void RebuildSettlementPathPreview(const UARPGBuildPieceDefinition* Piece, FVector StartWorld, FVector EndWorld, FVector StartTangentDirectionWorld);
    void DestroySettlementPathPreviewMeshes();
    void SetNormalPreviewVisibility(bool bVisible);
};
