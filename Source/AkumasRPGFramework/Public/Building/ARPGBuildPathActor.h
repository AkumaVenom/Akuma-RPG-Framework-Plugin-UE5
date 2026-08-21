#pragma once
#include "CoreMinimal.h"
#include "Building/ARPGBuildPieceActor.h"
#include "ARPGBuildPathActor.generated.h"

class USplineComponent;
class USplineMeshComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FARPGSettlementPathGeometryChanged);

/**
 * One authoritative, independently persistent segment of a player-built Settlement Path.
 * The segment internally samples terrain and may create several Spline Mesh components, but its
 * gameplay identity remains one normal build piece so ownership, demolition, refunds and saves use
 * the framework's existing building contracts.
 */
UCLASS(BlueprintType, Blueprintable)
class AKUMASRPGFRAMEWORK_API AARPGBuildPathActor : public AARPGBuildPieceActor
{
    GENERATED_BODY()
public:
    AARPGBuildPathActor();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Building|Settlement|Path|Components")
    TObjectPtr<USplineComponent> PathSpline;

    /** Actor-local confirmed start/end. Replicated instead of generated mesh components. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, ReplicatedUsing=OnRep_PathGeometry, SaveGame, Category="Building|Settlement|Path")
    FVector PathStartLocal = FVector::ZeroVector;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, ReplicatedUsing=OnRep_PathGeometry, SaveGame, Category="Building|Settlement|Path")
    FVector PathEndLocal = FVector::ZeroVector;
    /** Optional local endpoint tangent-direction overrides. Zero keeps the spline's clamped auto tangent; runtime magnitude is bounded to the adjacent sampled span. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, ReplicatedUsing=OnRep_PathGeometry, SaveGame, Category="Building|Settlement|Path")
    FVector PathStartTangentLocal = FVector::ZeroVector;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, ReplicatedUsing=OnRep_PathGeometry, SaveGame, Category="Building|Settlement|Path")
    FVector PathEndTangentLocal = FVector::ZeroVector;

    UPROPERTY(BlueprintAssignable, Category="Building|Settlement|Path")
    FARPGSettlementPathGeometryChanged OnPathGeometryChanged;

    /** Initializes a newly built segment from two already authority-projected world points. */
    UFUNCTION(BlueprintCallable, Category="ARPG|Building|Settlement Path", meta=(BlueprintAuthorityOnly))
    bool InitializePathSegment(UARPGBuildPieceDefinition* InDefinition, AActor* Builder, FVector StartWorld, FVector EndWorld);

    /** Restores v9 saved local endpoints after the actor/build definition has been recreated. */
    UFUNCTION(BlueprintCallable, Category="ARPG|Building|Settlement Path", meta=(BlueprintAuthorityOnly))
    void RestorePathGeometry(FVector SavedStartLocal, FVector SavedEndLocal, FVector SavedStartTangentLocal, FVector SavedEndTangentLocal);

    /** Authority-only tangent-direction correction used by the active build session to smooth a newly created turn without unbounded Hermite overshoot. */
    UFUNCTION(BlueprintCallable, Category="ARPG|Building|Settlement Path", meta=(BlueprintAuthorityOnly))
    void SetPathEndpointTangentsWorld(FVector StartTangentWorld, FVector EndTangentWorld);

    UFUNCTION(BlueprintPure, Category="ARPG|Building|Settlement Path") FVector GetPathStartWorld() const;
    UFUNCTION(BlueprintPure, Category="ARPG|Building|Settlement Path") FVector GetPathEndWorld() const;
    UFUNCTION(BlueprintPure, Category="ARPG|Building|Settlement Path") float GetPathSegmentLength() const;
    UFUNCTION(BlueprintPure, Category="ARPG|Building|Settlement Path") int32 GetGeneratedSplineMeshCount() const { return PathMeshComponents.Num(); }
    UFUNCTION(BlueprintCallable, Category="ARPG|Building|Settlement Path") void RefreshPathPresentation();

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
protected:
    UFUNCTION() void OnRep_PathGeometry();
    virtual void RefreshDefinitionPresentation() override;
    virtual void RefreshConstructionPresentation(bool bForce = false) override;
private:
    UPROPERTY(Transient) TArray<TObjectPtr<USplineMeshComponent>> PathMeshComponents;
    void RebuildSplineFromGeometry();
    void DestroyGeneratedSplineMeshes();
};
