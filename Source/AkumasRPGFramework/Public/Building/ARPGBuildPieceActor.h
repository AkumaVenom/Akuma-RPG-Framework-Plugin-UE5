#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Data/ARPGBuildPieceDefinition.h"
#include "ARPGBuildPieceActor.generated.h"

class UARPGFactionOwnershipComponent;
class USceneComponent;
class UStaticMeshComponent;
class USkeletalMeshComponent;
class UMeshComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FARPGBuildingHealthChanged, float, NewHealth, float, Delta);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FARPGBuildingDestroyed);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FARPGConstructionProgressChanged, float, Progress01);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FARPGConstructionCompleted);

UCLASS(BlueprintType, Blueprintable)
class AKUMASRPGFRAMEWORK_API AARPGBuildPieceActor : public AActor
{
    GENERATED_BODY()
public:
    AARPGBuildPieceActor();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Building|Components") TObjectPtr<USceneComponent> BuildRoot;
    /** Existing Static Mesh visual component. Kept intact for Blueprint/backward compatibility. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Building|Components") TObjectPtr<UStaticMeshComponent> BuildMesh;
    /** Optional Skeletal Mesh visual component selected automatically when Definition->BuildSkeletalMesh is assigned. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Building|Components") TObjectPtr<USkeletalMeshComponent> BuildSkeletalMesh;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, SaveGame, Category="Building") FGuid BuildingId;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, ReplicatedUsing=OnRep_Definition, Category="Building") TObjectPtr<UARPGBuildPieceDefinition> Definition;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Building") TObjectPtr<UARPGFactionOwnershipComponent> Ownership;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, ReplicatedUsing=OnRep_Health, SaveGame, Category="Building") float Health = 100.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, SaveGame, Category="Building") int32 UpgradeLevel = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, SaveGame, Category="Building") bool bRuntimePlaced = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Building") bool bAllowDemolish = true;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, ReplicatedUsing=OnRep_ConstructionState, SaveGame, Category="Building|Construction") bool bConstructionComplete = true;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, ReplicatedUsing=OnRep_ConstructionState, SaveGame, Category="Building|Construction") float ConstructionStartServerTime = 0.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, ReplicatedUsing=OnRep_ConstructionState, SaveGame, Category="Building|Construction") float ConstructionDuration = 0.f;

    UPROPERTY(BlueprintAssignable) FARPGBuildingHealthChanged OnBuildingHealthChanged;
    UPROPERTY(BlueprintAssignable) FARPGBuildingDestroyed OnBuildingDestroyed;
    UPROPERTY(BlueprintAssignable, Category="Building|Construction") FARPGConstructionProgressChanged OnConstructionProgressChanged;
    UPROPERTY(BlueprintAssignable, Category="Building|Construction") FARPGConstructionCompleted OnConstructionCompleted;

    UFUNCTION(BlueprintCallable, Category="ARPG|Building", meta=(BlueprintAuthorityOnly)) virtual void InitializeBuilding(UARPGBuildPieceDefinition* InDefinition, AActor* Builder);
    UFUNCTION(BlueprintCallable, Category="ARPG|Building", meta=(BlueprintAuthorityOnly)) bool ApplyBuildingDamage(float Amount, AActor* DamageCauser=nullptr);
    UFUNCTION(BlueprintCallable, Category="ARPG|Building", meta=(BlueprintAuthorityOnly)) bool RepairBuilding(float Amount, AActor* Repairer=nullptr);
    UFUNCTION(BlueprintCallable, Category="ARPG|Building", meta=(BlueprintAuthorityOnly)) bool Demolish(AActor* Requester=nullptr);
    UFUNCTION(BlueprintPure, Category="ARPG|Building") float GetMaxBuildingHealth() const;
    UFUNCTION(BlueprintPure, Category="ARPG|Building") bool CanActorUse(AActor* Actor) const;
    UFUNCTION(BlueprintPure, Category="ARPG|Building") bool CanActorModify(AActor* Actor) const;

    UFUNCTION(BlueprintPure, Category="ARPG|Building|Construction") bool IsConstructionComplete() const { return bConstructionComplete; }
    UFUNCTION(BlueprintPure, Category="ARPG|Building|Construction") float GetConstructionProgress01() const;
    UFUNCTION(BlueprintPure, Category="ARPG|Building|Construction") float GetConstructionRemainingSeconds() const;
    UFUNCTION(BlueprintCallable, Category="ARPG|Building|Construction", meta=(BlueprintAuthorityOnly)) void RestoreConstructionState(bool bWasComplete, float RemainingSeconds);

    /** Produces ready standard snap transforms plus any custom snap transforms authored on the definition. */
    UFUNCTION(BlueprintCallable, Category="ARPG|Building|Snapping") void GetSnapTransformsFor(const UARPGBuildPieceDefinition* IncomingPiece, TArray<FTransform>& OutWorldTransforms) const;

    /** Re-registers this runtime build piece with NavigationSystem and dirties its local Recast tiles. */
    UFUNCTION(BlueprintCallable, Category="ARPG|Building|Navigation", meta=(BlueprintAuthorityOnly)) void RefreshRuntimeNavigation();
    /** Re-evaluates nearby ARPG Tree resource respawn occupancy. Called automatically on placement/load/removal; exposed for custom runtime build actors. */
    UFUNCTION(BlueprintCallable, Category="ARPG|Building|Environment", meta=(BlueprintAuthorityOnly)) void RefreshNearbyTreeRespawnSuppression();
    /** Logical PlacementBounds overlap test used by environment/resource suppression. Rotation and actor scale are respected without depending on decorative mesh collision. */
    UFUNCTION(BlueprintPure, Category="ARPG|Building|Environment") bool DoesLogicalPlacementOverlapWorldCylinder(FVector WorldCenter, float HorizontalRadius, float WorldMinZ, float WorldMaxZ) const;
    /** Deprecated compatibility node. Automatic Stair NavLinks were removed in v2.16.8; refreshes real Dynamic Recast instead. */
    UFUNCTION(BlueprintCallable, Category="ARPG|Building|Navigation", meta=(BlueprintAuthorityOnly, DeprecatedFunction, DeprecationMessage="Automatic Stair NavLinks were removed in v2.16.8. Use Refresh Runtime Navigation.")) void RefreshStairNavigationBridge();
    /** Deprecated compatibility query. Automatic Stair NavLinks were removed in v2.16.8 and this always returns false. */
    UFUNCTION(BlueprintPure, Category="ARPG|Building|Navigation", meta=(DeprecatedFunction, DeprecationMessage="Automatic Stair NavLinks were removed in v2.16.8.")) bool HasActiveStairNavigationBridge() const;

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
protected:
    UFUNCTION() void OnRep_Definition();
    UFUNCTION() void OnRep_Health(float OldHealth);
    UFUNCTION() void OnRep_ConstructionState();
    virtual void RefreshDefinitionPresentation();
    virtual void RefreshConstructionPresentation(bool bForce = false);
    /** Returns the currently selected Static/Skeletal visual component, or null when this definition is custom-actor-only. */
    UMeshComponent* GetActiveBuildMeshComponent() const;
    /** Returns transformed actor-local visible bounds for the currently selected Static/Skeletal build visual. */
    bool GetActiveBuildVisualLocalBounds(FVector& OutMin, FVector& OutMax) const;
    /** Returns raw asset-local bounds before MeshRelativeTransform; used by the existing construction reveal. */
    bool GetActiveBuildVisualRawBounds(FVector& OutMin, FVector& OutMax) const;
    void CompleteConstructionAuthority();
    float GetAuthoritativeServerTime() const;
private:
    void NotifyNearbyTreesOfOccupancy(bool bPresent);
    FVector BaseMeshRelativeLocation = FVector::ZeroVector;
    FVector BaseMeshRelativeScale = FVector::OneVector;
    float LastConstructionVisualProgress = -1.f;
};
