#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "ARPGMineableRock.generated.h"

class AARPGMineableRock;
class AARPGBuildPieceActor;
class USceneComponent;
class UStaticMeshComponent;
class UStaticMesh;
class UARPGItemDefinition;
class UAnimMontage;
class UNiagaraSystem;
class UParticleSystem;
class USoundBase;

UENUM(BlueprintType)
enum class EARPGMineableRockState : uint8
{
    Available,
    Depleted
};

UENUM(BlueprintType)
enum class EARPGMiningBonusDropTrigger : uint8
{
    SuccessfulStrike,
    Depletion,
    Both
};

/** One normal resource roll. Use Strike Drops for Palworld-style repeated yield and Depletion Drops for the final payload. */
USTRUCT(BlueprintType)
struct AKUMASRPGFRAMEWORK_API FARPGMiningDrop
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drop") TObjectPtr<UARPGItemDefinition> Item = nullptr;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drop", meta=(ClampMin="0")) int32 MinQuantity = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drop", meta=(ClampMin="0")) int32 MaxQuantity = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drop", meta=(ClampMin="0.0", ClampMax="1.0")) float Chance = 1.f;
};

/** Rare/bonus reward roll with optional Mining-level/tool-tier gates and level-scaled discovery chance. */
USTRUCT(BlueprintType)
struct AKUMASRPGFRAMEWORK_API FARPGMiningBonusDrop
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bonus Drop") TObjectPtr<UARPGItemDefinition> Item = nullptr;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bonus Drop", meta=(ClampMin="0")) int32 MinQuantity = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bonus Drop", meta=(ClampMin="0")) int32 MaxQuantity = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bonus Drop", meta=(ClampMin="0.0", ClampMax="1.0")) float BaseChance = 0.05f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bonus Drop") EARPGMiningBonusDropTrigger Trigger = EARPGMiningBonusDropTrigger::Depletion;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bonus Drop", meta=(ClampMin="1")) int32 RequiredMiningLevel = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bonus Drop", meta=(ClampMin="0")) int32 MinimumToolTier = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bonus Drop", meta=(DisplayName="Scale Chance With Mining Level")) bool bScaleChanceWithMiningLevel = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bonus Drop", meta=(DisplayName="Scale Chance With Tool Tier")) bool bScaleChanceWithToolTier = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FARPGMineableRockStruckEvent, AARPGMineableRock*, Rock, AActor*, Harvester, float, DamageApplied, float, RemainingHealth);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FARPGMineableRockDepletedEvent, AARPGMineableRock*, Rock, AActor*, Harvester);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FARPGMineableRockRespawnedEvent, AARPGMineableRock*, Rock);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FARPGMineableRockStateChangedEvent, EARPGMineableRockState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FARPGMineableRockBuildingSuppressionChangedEvent, bool, bSuppressed);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_SixParams(FARPGMineableRockRewardEvent, AActor*, Harvester, UARPGItemDefinition*, Item, int32, Quantity, bool, bAddedToInventory, bool, bBonusDrop, EARPGMiningBonusDropTrigger, RewardMoment);

/**
 * Blueprintable replicated mining resource actor. Intended for hand placement or Unreal Actor Foliage.
 * The authority selects visual variation/scale/yaw, owns health/rewards/respawn, and validates Mining level/tool gates.
 */
UCLASS(BlueprintType, Blueprintable)
class AKUMASRPGFRAMEWORK_API AARPGMineableRock : public AActor
{
    GENERATED_BODY()
    friend class AARPGBuildPieceActor;
public:
    AARPGMineableRock();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="ARPG|Mining Rock") TObjectPtr<USceneComponent> Root;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="ARPG|Mining Rock") TObjectPtr<USceneComponent> VisualRoot;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="ARPG|Mining Rock") TObjectPtr<UStaticMeshComponent> RockMesh;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="ARPG|Mining Rock") TObjectPtr<UStaticMeshComponent> DepletedMesh;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rock|Visual", meta=(DisplayName="Rock Mesh Variations")) TArray<TObjectPtr<UStaticMesh>> RockMeshes;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rock|Visual", meta=(DisplayName="Depleted / Rubble Mesh")) TObjectPtr<UStaticMesh> DepletedMeshAsset = nullptr;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rock|Visual") bool bRandomizeRockMesh = true;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rock|Visual") bool bRerollRockMeshOnRespawn = true;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, ReplicatedUsing=OnRep_SelectedRockMeshIndex, Category="Rock|Visual") int32 SelectedRockMeshIndex = INDEX_NONE;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rock|Visual|Scale", meta=(DisplayName="Randomize Rock Mesh Scale")) bool bRandomizeRockMeshScale = true;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rock|Visual|Scale", meta=(ClampMin="0.05", DisplayName="Minimum Mesh Scale")) float MinimumMeshScale = 0.90f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rock|Visual|Scale", meta=(ClampMin="0.05", DisplayName="Maximum Mesh Scale")) float MaximumMeshScale = 1.10f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rock|Visual|Scale", meta=(DisplayName="Reroll Mesh Scale On Respawn")) bool bRerollRockMeshScaleOnRespawn = true;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, ReplicatedUsing=OnRep_SelectedRockMeshScale, Category="Rock|Visual|Scale") float SelectedRockMeshScale = 1.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rock|Visual|Rotation", meta=(DisplayName="Randomize Rock Yaw")) bool bRandomizeRockYaw = true;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rock|Visual|Rotation", meta=(EditCondition="bRandomizeRockYaw")) float MinimumRandomYawDegrees = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rock|Visual|Rotation", meta=(EditCondition="bRandomizeRockYaw")) float MaximumRandomYawDegrees = 360.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rock|Visual|Rotation", meta=(DisplayName="Reroll Rock Yaw On Respawn")) bool bRerollRockYawOnRespawn = true;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, ReplicatedUsing=OnRep_SelectedRockYaw, Category="Rock|Visual|Rotation") float SelectedRockYawDegrees = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rock|Mining", meta=(ClampMin="1.0")) float MaxMiningHealth = 100.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category="Rock|Mining") float CurrentMiningHealth = 100.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rock|Mining", meta=(ClampMin="1")) int32 RequiredMiningLevel = 1;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rock|Mining", meta=(ClampMin="0.05")) float MiningResistance = 1.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rock|Mining") bool bRequireMiningTool = true;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rock|Mining", meta=(EditCondition="bRequireMiningTool")) FGameplayTag RequiredToolTag;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rock|Mining", meta=(EditCondition="bRequireMiningTool", ClampMin="0")) int32 MinimumToolTier = 1;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rock|Mining", meta=(ClampMin="0")) int64 XPPerSuccessfulStrike = 5;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rock|Mining", meta=(ClampMin="0")) int64 XPOnDepletion = 25;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rock|Mining", meta=(ClampMin="0.0", DisplayName="Impact Height Fallback")) float MiningImpactHeight = 60.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rock|Mining") TSoftObjectPtr<UAnimMontage> MiningMontageOverride;

    /** Normal rolls evaluated after every successful mining strike. Great for Stone/ore chips. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rock|Drops", meta=(DisplayName="Successful Strike Drops")) TArray<FARPGMiningDrop> StrikeDrops;
    /** Normal rolls evaluated once when health reaches zero. Great for the node's main final payload. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rock|Drops", meta=(DisplayName="Depletion Drops")) TArray<FARPGMiningDrop> DepletionDrops;
    /** Rare gems, geodes, bonus ore, fossils, etc. Trigger/level/tool requirements are authored per entry. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rock|Drops", meta=(DisplayName="Bonus Chance Drops")) TArray<FARPGMiningBonusDrop> BonusDrops;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rock|Drops|Bonus Scaling", meta=(ClampMin="0.0", ClampMax="1.0", DisplayName="Bonus Chance Per Mining Level Above Requirement")) float BonusChancePerMiningLevel = 0.001f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rock|Drops|Bonus Scaling", meta=(ClampMin="0.0", ClampMax="1.0", DisplayName="Bonus Chance Per Tool Tier Above Minimum")) float BonusChancePerToolTier = 0.01f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rock|Drops|Bonus Scaling", meta=(ClampMin="0.0", ClampMax="1.0", DisplayName="Maximum Effective Bonus Chance")) float MaximumEffectiveBonusChance = 1.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rock|Respawn") bool bRespawn = true;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rock|Respawn", meta=(EditCondition="bRespawn", ClampMin="0.0", Units="s")) float RespawnSeconds = 120.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rock|Respawn|Building Suppression", meta=(DisplayName="Suppress Respawn While Built Over")) bool bSuppressRespawnWhileBuiltOver = true;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rock|Respawn|Building Suppression", meta=(EditCondition="bSuppressRespawnWhileBuiltOver", ClampMin="0.0", Units="cm")) float BuildingRespawnBlockRadius = 100.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rock|Respawn|Building Suppression", meta=(EditCondition="bSuppressRespawnWhileBuiltOver", ClampMin="0.1", Units="s")) float BuildingRespawnRecheckSeconds = 1.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, ReplicatedUsing=OnRep_BuildingRespawnSuppressed, Category="Rock|Respawn|Building Suppression") bool bBuildingRespawnSuppressed = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rock|Feedback") TObjectPtr<UNiagaraSystem> StrikeNiagara = nullptr;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rock|Feedback") TObjectPtr<UParticleSystem> StrikeCascadeFallback = nullptr;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rock|Feedback") TObjectPtr<UNiagaraSystem> DepletedNiagara = nullptr;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rock|Feedback") TObjectPtr<UParticleSystem> DepletedCascadeFallback = nullptr;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rock|Feedback") TObjectPtr<USoundBase> StrikeSound = nullptr;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rock|Feedback") TObjectPtr<USoundBase> DepletedSound = nullptr;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rock|Feedback", meta=(ClampMin="0.0")) float FeedbackScale = 1.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, ReplicatedUsing=OnRep_RockState, Category="Rock|State") EARPGMineableRockState RockState = EARPGMineableRockState::Available;

    UPROPERTY(BlueprintAssignable, Category="Rock|Events") FARPGMineableRockStruckEvent OnRockStruck;
    UPROPERTY(BlueprintAssignable, Category="Rock|Events") FARPGMineableRockDepletedEvent OnRockDepleted;
    UPROPERTY(BlueprintAssignable, Category="Rock|Events") FARPGMineableRockRespawnedEvent OnRockRespawned;
    UPROPERTY(BlueprintAssignable, Category="Rock|Events") FARPGMineableRockStateChangedEvent OnRockStateChanged;
    UPROPERTY(BlueprintAssignable, Category="Rock|Events") FARPGMineableRockBuildingSuppressionChangedEvent OnRockBuildingSuppressionChanged;
    UPROPERTY(BlueprintAssignable, Category="Rock|Events") FARPGMineableRockRewardEvent OnRockRewardGranted;

    UFUNCTION(BlueprintPure, Category="ARPG|Mining|Rock") bool IsAvailable() const { return RockState == EARPGMineableRockState::Available; }
    UFUNCTION(BlueprintPure, Category="ARPG|Mining|Rock") bool IsDepleted() const { return RockState == EARPGMineableRockState::Depleted; }
    UFUNCTION(BlueprintPure, Category="ARPG|Mining|Rock") float GetMiningHealthPercent() const;
    UFUNCTION(BlueprintPure, Category="ARPG|Mining|Rock") UStaticMesh* GetSelectedRockMesh() const;
    UFUNCTION(BlueprintPure, Category="ARPG|Mining|Rock") float GetSelectedRockMeshScale() const { return SelectedRockMeshScale; }
    UFUNCTION(BlueprintPure, Category="ARPG|Mining|Rock") float GetSelectedRockYaw() const { return SelectedRockYawDegrees; }
    UFUNCTION(BlueprintPure, Category="ARPG|Mining|Rock") FVector GetMiningImpactLocation(AActor* Harvester = nullptr) const;
    UFUNCTION(BlueprintCallable, Category="ARPG|Mining|Rock") bool CanBeMinedBy(AActor* Harvester, FText& OutFailureReason) const;
    UFUNCTION(BlueprintPure, Category="ARPG|Mining|Rock|Rewards") float GetEffectiveBonusDropChance(const FARPGMiningBonusDrop& Drop, AActor* Harvester) const;

    UFUNCTION(BlueprintPure, Category="ARPG|Mining|Rock|Building Suppression") bool IsRespawnSuppressedByBuilding() const { return bBuildingRespawnSuppressed; }
    UFUNCTION(BlueprintCallable, Category="ARPG|Mining|Rock|Building Suppression", meta=(BlueprintAuthorityOnly)) bool RefreshBuildingRespawnSuppression();
    UFUNCTION(BlueprintPure, Category="ARPG|Mining|Rock|Building Suppression") bool IsRespawnBlockedByBuildPiece(const AARPGBuildPieceActor* Building) const;

    UFUNCTION(BlueprintCallable, Category="ARPG|Mining|Rock", meta=(BlueprintAuthorityOnly)) bool ApplyMiningStrike(AActor* Harvester, float MiningPower);
    UFUNCTION(BlueprintCallable, Category="ARPG|Mining|Rock", meta=(BlueprintAuthorityOnly)) bool DepleteRock(AActor* Harvester);
    UFUNCTION(BlueprintCallable, Category="ARPG|Mining|Rock", meta=(BlueprintAuthorityOnly)) void ForceRespawn();
    UFUNCTION(BlueprintCallable, Category="ARPG|Mining|Rock", meta=(BlueprintAuthorityOnly)) void SelectRandomRockMesh();
    UFUNCTION(BlueprintCallable, Category="ARPG|Mining|Rock", meta=(BlueprintAuthorityOnly)) bool SetRockMeshIndex(int32 NewIndex);
    UFUNCTION(BlueprintCallable, Category="ARPG|Mining|Rock", meta=(BlueprintAuthorityOnly)) void SelectRandomRockMeshScale();
    UFUNCTION(BlueprintCallable, Category="ARPG|Mining|Rock", meta=(BlueprintAuthorityOnly)) bool SetRockMeshScale(float NewScale);
    UFUNCTION(BlueprintCallable, Category="ARPG|Mining|Rock", meta=(BlueprintAuthorityOnly)) void SelectRandomRockYaw();
    UFUNCTION(BlueprintCallable, Category="ARPG|Mining|Rock", meta=(BlueprintAuthorityOnly)) bool SetRockYaw(float NewYawDegrees);

    virtual void BeginPlay() override;
    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
    UFUNCTION() void OnRep_RockState(EARPGMineableRockState OldState);
    UFUNCTION() void OnRep_BuildingRespawnSuppressed();
    UFUNCTION() void OnRep_SelectedRockMeshIndex();
    UFUNCTION() void OnRep_SelectedRockMeshScale();
    UFUNCTION() void OnRep_SelectedRockYaw();
    UFUNCTION(NetMulticast, Unreliable) void MulticastPlayStrikeFeedback(AActor* Harvester, FVector_NetQuantize ImpactLocation, UARPGItemDefinition* EquippedTool);
    UFUNCTION(NetMulticast, Reliable) void MulticastPlayDepletedFeedback(FVector_NetQuantize Location);

    void ApplySelectedRockMesh();
    void ApplySelectedRockVisualTransform();
    void ApplyRockStateVisuals();
    void CacheBaseVisualTransform();
    void GetSanitizedMeshScaleRange(float& OutMinScale, float& OutMaxScale) const;
    void GetSanitizedYawRange(float& OutMinYaw, float& OutMaxYaw) const;
    void TryRespawnAuthority();
    void CompleteRespawnAuthority();
    void UpdateBuildingSuppressionStateAuthority();
    void CacheAvailableRespawnBounds();
    void ScheduleSuppressionRecheck();
    void RecheckBuildingRespawnSuppressionAuthority();
    void NotifyBuildPieceOccupancyChanged(AARPGBuildPieceActor* Building, bool bPresent);
    void GrantNormalDropArray(AActor* Harvester, const TArray<FARPGMiningDrop>& Drops, EARPGMiningBonusDropTrigger Moment);
    void GrantBonusDrops(AActor* Harvester, EARPGMiningBonusDropTrigger Moment);
    bool GrantOneReward(AActor* Harvester, UARPGItemDefinition* Item, int32 MinQuantity, int32 MaxQuantity, bool bBonus, EARPGMiningBonusDropTrigger Moment);
    void AwardMiningXP(AActor* Harvester, int64 Amount) const;
    void PlayFeedbackLocal(bool bDepleted, const FVector& Location) const;

    FTimerHandle RespawnTimer;
    FTimerHandle BuildingSuppressionRecheckTimer;
    TSet<TWeakObjectPtr<AARPGBuildPieceActor>> BuildingRespawnBlockers;
    double RespawnEligibleServerTime = 0.0;
    bool bForcedRespawnPending = false;
    FBox CachedAvailableRespawnBounds = FBox(EForceInit::ForceInit);
    bool bBaseVisualTransformCached = false;
    FVector BaseVisualRootScale = FVector::OneVector;
    FRotator BaseVisualRootRotation = FRotator::ZeroRotator;
};
