#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "ARPGTree.generated.h"

class AARPGTree;
class USceneComponent;
class UStaticMeshComponent;
class UStaticMesh;
class UARPGItemDefinition;
class UAnimMontage;
class UNiagaraSystem;
class UParticleSystem;
class USoundBase;
class AARPGBuildPieceActor;

UENUM(BlueprintType)
enum class EARPGTreeState : uint8
{
    Standing,
    Falling,
    Stump
};

USTRUCT(BlueprintType)
struct AKUMASRPGFRAMEWORK_API FARPGTreeBonusDrop
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drop") TObjectPtr<UARPGItemDefinition> Item = nullptr;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drop", meta=(ClampMin="0")) int32 MinQuantity = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drop", meta=(ClampMin="0")) int32 MaxQuantity = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drop", meta=(ClampMin="0.0", ClampMax="1.0")) float Chance = 1.f;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FARPGTreeChoppedEvent, AARPGTree*, Tree, AActor*, Harvester, float, DamageApplied, float, RemainingHealth);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FARPGTreeFelledEvent, AARPGTree*, Tree, AActor*, Harvester);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FARPGTreeRespawnedEvent, AARPGTree*, Tree);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FARPGTreeStateChangedEvent, EARPGTreeState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FARPGTreeBuildingSuppressionChangedEvent, bool, bSuppressed);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FARPGTreeRewardEvent, AActor*, Harvester, UARPGItemDefinition*, Item, int32, Quantity, bool, bAddedToInventory);

UCLASS(BlueprintType, Blueprintable)
class AKUMASRPGFRAMEWORK_API AARPGTree : public AActor
{
    GENERATED_BODY()
    friend class AARPGBuildPieceActor;
public:
    AARPGTree();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="ARPG|Tree") TObjectPtr<USceneComponent> Root;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="ARPG|Tree") TObjectPtr<USceneComponent> FallPivot;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="ARPG|Tree") TObjectPtr<UStaticMeshComponent> TreeMesh;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="ARPG|Tree") TObjectPtr<UStaticMeshComponent> StumpMesh;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tree|Visual", meta=(DisplayName="Tree Mesh Variations")) TArray<TObjectPtr<UStaticMesh>> TreeMeshes;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tree|Visual") TObjectPtr<UStaticMesh> StumpMeshAsset = nullptr;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tree|Visual") bool bRandomizeTreeMesh = true;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tree|Visual") bool bRerollTreeMeshOnRespawn = true;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, ReplicatedUsing=OnRep_SelectedTreeMeshIndex, Category="Tree|Visual") int32 SelectedTreeMeshIndex = INDEX_NONE;

    // Uniform per-instance visual size variation. The authority selects one scalar and replicates it so
    // every client sees the same size while preserving any scale authored on the native mesh components.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tree|Visual|Scale", meta=(DisplayName="Randomize Tree Mesh Scale")) bool bRandomizeTreeMeshScale = true;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tree|Visual|Scale", meta=(ClampMin="0.05", DisplayName="Minimum Mesh Scale")) float MinimumMeshScale = 0.90f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tree|Visual|Scale", meta=(ClampMin="0.05", DisplayName="Maximum Mesh Scale")) float MaximumMeshScale = 1.10f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tree|Visual|Scale", meta=(DisplayName="Reroll Mesh Scale On Respawn")) bool bRerollTreeMeshScaleOnRespawn = true;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, ReplicatedUsing=OnRep_SelectedTreeMeshScale, Category="Tree|Visual|Scale") float SelectedTreeMeshScale = 1.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tree|Woodcutting", meta=(ClampMin="1.0")) float MaxChopHealth = 100.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category="Tree|Woodcutting") float CurrentChopHealth = 100.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tree|Woodcutting", meta=(ClampMin="1")) int32 RequiredWoodcuttingLevel = 1;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tree|Woodcutting", meta=(ClampMin="0.05")) float ChopResistance = 1.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tree|Woodcutting") bool bRequireWoodcuttingTool = false;
    /** Allows residents owned by a Settlement Hub to use the Hub's configured native worker rules. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tree|Woodcutting") bool bAllowSettlementVillagerHarvest = true;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tree|Woodcutting", meta=(EditCondition="bRequireWoodcuttingTool")) FGameplayTag RequiredToolTag;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tree|Woodcutting", meta=(EditCondition="bRequireWoodcuttingTool", ClampMin="0")) int32 MinimumToolTier = 0;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tree|Woodcutting", meta=(ClampMin="0")) int64 XPPerSuccessfulChop = 5;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tree|Woodcutting", meta=(ClampMin="0")) int64 XPOnFell = 25;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tree|Woodcutting", meta=(ClampMin="0.0")) float ChopImpactHeight = 100.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tree|Woodcutting") TSoftObjectPtr<UAnimMontage> ChopMontageOverride;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tree|Drops", meta=(DisplayName="Wood Item")) TObjectPtr<UARPGItemDefinition> WoodItem = nullptr;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tree|Drops", meta=(ClampMin="0")) int32 MinWoodQuantity = 2;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tree|Drops", meta=(ClampMin="0")) int32 MaxWoodQuantity = 4;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tree|Drops") TArray<FARPGTreeBonusDrop> BonusDrops;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tree|Drops") bool bGrantRewardsOnFell = true;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tree|Fall") bool bFallAwayFromHarvester = true;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tree|Fall", meta=(ClampMin="0.05")) float FallDuration = 1.4f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tree|Fall", meta=(ClampMin="1.0", ClampMax="90.0")) float FallAngleDegrees = 88.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tree|Fall", meta=(ClampMin="0.0")) float FallDirectionRandomDegrees = 12.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tree|Fall", meta=(ClampMin="0.0")) float FallenTreeVisibleSeconds = 3.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tree|Respawn") bool bRespawn = true;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tree|Respawn", meta=(EditCondition="bRespawn", ClampMin="0.0")) float RespawnSeconds = 180.f;

    /** Player-built structures may replace this resource location. While a build piece occupies the tree's respawn volume the tree remains fully hidden/non-colliding and cannot regenerate. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tree|Respawn|Building Suppression", meta=(DisplayName="Suppress Respawn While Built Over")) bool bSuppressRespawnWhileBuiltOver = true;
    /** Horizontal radius around the tree trunk/root used for build-occupancy suppression. This deliberately ignores the canopy so nearby houses do not suppress unrelated trees. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tree|Respawn|Building Suppression", meta=(EditCondition="bSuppressRespawnWhileBuiltOver", ClampMin="0.0", Units="cm")) float BuildingRespawnBlockRadius = 85.f;
    /** How often a building-suppressed tree self-heals its blocker set. Only suppressed trees run this timer; normal standing trees have no permanent polling cost. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tree|Respawn|Building Suppression", meta=(EditCondition="bSuppressRespawnWhileBuiltOver", ClampMin="0.1", Units="s")) float BuildingRespawnRecheckSeconds = 1.f;
    /** Derived authoritative state. True means at least one ARPG Build Piece currently occupies the tree's regeneration volume. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, ReplicatedUsing=OnRep_BuildingRespawnSuppressed, Category="Tree|Respawn|Building Suppression") bool bBuildingRespawnSuppressed = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tree|Feedback") TObjectPtr<UNiagaraSystem> ChopNiagara = nullptr;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tree|Feedback") TObjectPtr<UParticleSystem> ChopCascadeFallback = nullptr;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tree|Feedback") TObjectPtr<UNiagaraSystem> FellNiagara = nullptr;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tree|Feedback") TObjectPtr<UParticleSystem> FellCascadeFallback = nullptr;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tree|Feedback") TObjectPtr<USoundBase> ChopSound = nullptr;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tree|Feedback") TObjectPtr<USoundBase> FellSound = nullptr;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tree|Feedback", meta=(ClampMin="0.0")) float FeedbackScale = 1.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, ReplicatedUsing=OnRep_TreeState, Category="Tree|State") EARPGTreeState TreeState = EARPGTreeState::Standing;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category="Tree|State") FVector FallDirection = FVector::ForwardVector;

    UPROPERTY(BlueprintAssignable, Category="Tree|Events") FARPGTreeChoppedEvent OnTreeChopped;
    UPROPERTY(BlueprintAssignable, Category="Tree|Events") FARPGTreeFelledEvent OnTreeFelled;
    UPROPERTY(BlueprintAssignable, Category="Tree|Events") FARPGTreeRespawnedEvent OnTreeRespawned;
    UPROPERTY(BlueprintAssignable, Category="Tree|Events") FARPGTreeStateChangedEvent OnTreeStateChanged;
    UPROPERTY(BlueprintAssignable, Category="Tree|Events") FARPGTreeBuildingSuppressionChangedEvent OnTreeBuildingSuppressionChanged;
    UPROPERTY(BlueprintAssignable, Category="Tree|Events") FARPGTreeRewardEvent OnTreeRewardGranted;

    UFUNCTION(BlueprintPure, Category="ARPG|Woodcutting|Tree") bool IsStanding() const { return TreeState == EARPGTreeState::Standing; }
    UFUNCTION(BlueprintPure, Category="ARPG|Woodcutting|Tree") bool IsFelled() const { return TreeState != EARPGTreeState::Standing; }
    UFUNCTION(BlueprintPure, Category="ARPG|Woodcutting|Tree") float GetChopHealthPercent() const;
    UFUNCTION(BlueprintPure, Category="ARPG|Woodcutting|Tree") UStaticMesh* GetSelectedTreeMesh() const;
    UFUNCTION(BlueprintPure, Category="ARPG|Woodcutting|Tree") float GetSelectedTreeMeshScale() const { return SelectedTreeMeshScale; }
    UFUNCTION(BlueprintPure, Category="ARPG|Woodcutting|Tree") FVector GetChopImpactLocation(AActor* Harvester = nullptr) const;
    UFUNCTION(BlueprintPure, Category="ARPG|Woodcutting|Tree|Building Suppression") bool IsRespawnSuppressedByBuilding() const { return bBuildingRespawnSuppressed; }
    /** Rebuilds the authoritative blocker set from current ARPG Build Pieces. Useful for custom runtime world changes; normal framework construction calls this automatically. */
    UFUNCTION(BlueprintCallable, Category="ARPG|Woodcutting|Tree|Building Suppression", meta=(BlueprintAuthorityOnly)) bool RefreshBuildingRespawnSuppression();
    /** Tests whether one build piece occupies this tree's trunk-root respawn volume using the build piece's logical Placement Bounds. */
    UFUNCTION(BlueprintPure, Category="ARPG|Woodcutting|Tree|Building Suppression") bool IsRespawnBlockedByBuildPiece(const AARPGBuildPieceActor* Building) const;
    UFUNCTION(BlueprintCallable, Category="ARPG|Woodcutting|Tree") bool CanBeChoppedBy(AActor* Harvester, FText& OutFailureReason) const;

    UFUNCTION(BlueprintCallable, Category="ARPG|Woodcutting|Tree", meta=(BlueprintAuthorityOnly)) bool ApplyChop(AActor* Harvester, float ChopPower);
    UFUNCTION(BlueprintCallable, Category="ARPG|Woodcutting|Tree", meta=(BlueprintAuthorityOnly)) bool FellTree(AActor* Harvester);
    UFUNCTION(BlueprintCallable, Category="ARPG|Woodcutting|Tree", meta=(BlueprintAuthorityOnly)) void ForceRespawn();
    UFUNCTION(BlueprintCallable, Category="ARPG|Woodcutting|Tree", meta=(BlueprintAuthorityOnly)) void SelectRandomTreeMesh();
    UFUNCTION(BlueprintCallable, Category="ARPG|Woodcutting|Tree", meta=(BlueprintAuthorityOnly)) bool SetTreeMeshIndex(int32 NewIndex);
    UFUNCTION(BlueprintCallable, Category="ARPG|Woodcutting|Tree", meta=(BlueprintAuthorityOnly)) void SelectRandomTreeMeshScale();
    UFUNCTION(BlueprintCallable, Category="ARPG|Woodcutting|Tree", meta=(BlueprintAuthorityOnly)) bool SetTreeMeshScale(float NewScale);

    virtual void BeginPlay() override;
    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
    UFUNCTION() void OnRep_TreeState(EARPGTreeState OldState);
    UFUNCTION() void OnRep_BuildingRespawnSuppressed();
    UFUNCTION() void OnRep_SelectedTreeMeshIndex();
    UFUNCTION() void OnRep_SelectedTreeMeshScale();
    UFUNCTION(NetMulticast, Reliable) void MulticastBeginTreeFall(FVector_NetQuantizeNormal InFallDirection);
    UFUNCTION(NetMulticast, Unreliable) void MulticastPlayChopFeedback(AActor* Harvester, FVector_NetQuantize ImpactLocation, UARPGItemDefinition* EquippedTool);
    UFUNCTION(NetMulticast, Reliable) void MulticastPlayFellFeedback(FVector_NetQuantize Location);

    void ApplySelectedTreeMesh();
    void ApplySelectedTreeMeshScale();
    void CacheBaseVisualScales();
    void GetSanitizedMeshScaleRange(float& OutMinScale, float& OutMaxScale) const;
    void ApplyTreeStateVisuals(bool bLateJoinOrImmediate = false);
    void StartFallVisualLocal(const FVector& InFallDirection);
    void FinishFallVisualLocal();
    void EnterStumpAuthority();
    void TryRespawnAuthority();
    void CompleteRespawnAuthority();
    void UpdateBuildingSuppressionStateAuthority();
    void CacheStandingRespawnBounds();
    void ScheduleSuppressionRecheck();
    void RecheckBuildingRespawnSuppressionAuthority();
    void NotifyBuildPieceOccupancyChanged(AARPGBuildPieceActor* Building, bool bPresent);
    void GrantRewards(AActor* Harvester);
    void AwardWoodcuttingXP(AActor* Harvester, int64 Amount) const;
    void PlayFeedbackLocal(bool bFell, const FVector& Location) const;

    FTimerHandle StumpTimer;
    FTimerHandle RespawnTimer;
    FTimerHandle BuildingSuppressionRecheckTimer;
    TSet<TWeakObjectPtr<AARPGBuildPieceActor>> BuildingRespawnBlockers;
    double RespawnEligibleServerTime = 0.0;
    bool bForcedRespawnPending = false;
    FBox CachedStandingRespawnBounds = FBox(EForceInit::ForceInit);
    float LocalFallElapsed = 0.f;
    FVector LocalFallDirection = FVector::ForwardVector;
    bool bLocalFallActive = false;
    bool bBaseVisualScalesCached = false;
    FVector BaseFallPivotScale = FVector::OneVector;
    FVector BaseStumpMeshScale = FVector::OneVector;
};
