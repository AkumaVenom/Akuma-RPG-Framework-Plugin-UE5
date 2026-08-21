#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "Engine/EngineTypes.h"
#include "Data/ARPGSkillDefinition.h"
#include "ARPGMiningComponent.generated.h"

class AARPGMineableRock;
class UARPGItemDefinition;
class UARPGSkillDefinition;
class UAnimMontage;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FARPGMiningRockEvent, AARPGMineableRock*, Rock);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FARPGMiningSwingEvent, AARPGMineableRock*, Rock, float, MiningPower, float, RemainingHealth);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FARPGMiningResultEvent, bool, bSuccess, FText, Message);

/**
 * Persistent levelled Mining profession for AARPGCharacter. Interact starts an automatic mining loop;
 * Basic Attack can contextually become one free Mining strike when a valid Pickaxe/resource is intended.
 */
UCLASS(ClassGroup=(ARPG), meta=(BlueprintSpawnableComponent))
class AKUMASRPGFRAMEWORK_API UARPGMiningComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UARPGMiningComponent();

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mining|Progression") FName SkillId = TEXT("Mining");
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mining|Progression") TObjectPtr<UARPGSkillDefinition> SkillDefinition = nullptr;
    /** Native Mining defaults to the familiar 1-99 RuneScape-style curve when no Skill Definition is assigned. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mining|Progression", meta=(DisplayName="Use RuneScape-Style XP Without Skill Definition")) bool bUseRuneScapeStyleXPWithoutSkillDefinition = true;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mining|Progression", meta=(ClampMin="1", EditCondition="bUseRuneScapeStyleXPWithoutSkillDefinition")) int32 NativeMiningMaxLevel = 99;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mining|Progression", meta=(ClampMin="0.0")) float SkillPowerPerLevel = 0.01f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mining|Striking", meta=(ClampMin="50.0", Units="cm")) float MaxMiningDistance = 450.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mining|Striking", meta=(ClampMin="0.01")) float BaseMiningPower = 20.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mining|Striking", meta=(ClampMin="0.1", Units="s")) float SwingIntervalSeconds = 1.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mining|Striking", meta=(ClampMin="0.0", Units="s")) float SwingImpactDelay = 0.25f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mining|Striking") bool bAutoRepeatMining = true;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mining|Striking") TSoftObjectPtr<UAnimMontage> DefaultMiningMontage;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mining|Targeting", meta=(ClampMin="50.0", Units="cm")) float AutoTargetTraceDistance = 600.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mining|Targeting", meta=(ClampMin="0.0", Units="cm")) float AutoTargetTraceRadius = 35.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mining|Targeting") TEnumAsByte<ECollisionChannel> AutoTargetTraceChannel = ECC_Visibility;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mining|Tools") FGameplayTag PreferredMiningToolTag;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mining|Tools") bool bUseBestEquippedTool = true;

    // Context-sensitive combat integration mirrors Woodcutting: the normal Basic Attack is free to use as
    // one Mining strike and never spends combat stamina/mana when a Mineable Rock + valid Pickaxe is intended.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mining|Combat Integration", meta=(DisplayName="Basic Attack Auto Mines Rocks")) bool bAutoMineRocksWithBasicAttack = true;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mining|Combat Integration", meta=(EditCondition="bAutoMineRocksWithBasicAttack", DisplayName="Basic Attack Requires Equipped Pickaxe")) bool bBasicAttackRequiresEquippedPickaxe = true;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Mining|Combat Integration", meta=(DisplayName="Use Combat Melee Montage As Mining Fallback")) bool bUseCombatMeleeMontageAsMiningFallback = true;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, ReplicatedUsing=OnRep_CurrentRock, Category="Mining|State") TObjectPtr<AARPGMineableRock> CurrentRock = nullptr;

    UPROPERTY(BlueprintAssignable, Category="Mining|Events") FARPGMiningRockEvent OnMiningStarted;
    UPROPERTY(BlueprintAssignable, Category="Mining|Events") FARPGMiningRockEvent OnMiningStopped;
    UPROPERTY(BlueprintAssignable, Category="Mining|Events") FARPGMiningSwingEvent OnMiningSwing;
    UPROPERTY(BlueprintAssignable, Category="Mining|Events") FARPGMiningResultEvent OnMiningResult;

    /** Interaction-style entry point: view trace + repeated Mining until depletion/cancel/range failure. */
    UFUNCTION(BlueprintCallable, Category="ARPG|Mining") bool StartMiningFromView();
    UFUNCTION(BlueprintCallable, Category="ARPG|Mining") AARPGMineableRock* FindMineableRockInView() const;
    UFUNCTION(BlueprintCallable, Category="ARPG|Mining") bool StartMining(AARPGMineableRock* Rock);
    UFUNCTION(BlueprintCallable, Category="ARPG|Mining") bool MineRockOnce(AARPGMineableRock* Rock);
    UFUNCTION(BlueprintCallable, Category="ARPG|Mining") void StopMining();

    UFUNCTION(BlueprintPure, Category="ARPG|Mining") bool IsMining() const { return CurrentRock != nullptr; }
    UFUNCTION(BlueprintPure, Category="ARPG|Mining") AARPGMineableRock* GetCurrentRock() const { return CurrentRock; }
    UFUNCTION(BlueprintPure, Category="ARPG|Mining|Progression") int32 GetMiningLevel() const;
    UFUNCTION(BlueprintPure, Category="ARPG|Mining|Progression") int64 GetMiningXP() const;
    UFUNCTION(BlueprintPure, Category="ARPG|Mining|Progression") int64 GetMiningXPForNextLevel() const;
    UFUNCTION(BlueprintPure, Category="ARPG|Mining|Progression") int64 GetMiningXPRemaining() const;
    UFUNCTION(BlueprintPure, Category="ARPG|Mining|Progression") float GetMiningLevelProgress() const;
    UFUNCTION(BlueprintPure, Category="ARPG|Mining|Progression") bool HasMiningUnlock(FGameplayTag UnlockTag) const;

    UFUNCTION(BlueprintPure, Category="ARPG|Mining|Tools") UARPGItemDefinition* GetBestEquippedMiningTool() const;
    UFUNCTION(BlueprintPure, Category="ARPG|Mining|Tools") FGuid GetBestEquippedMiningToolInstanceId() const;
    UFUNCTION(BlueprintPure, Category="ARPG|Mining|Tools") float GetBestEquippedToolPower() const;
    UFUNCTION(BlueprintPure, Category="ARPG|Mining|Tools") int32 GetBestEquippedToolTier() const;
    UFUNCTION(BlueprintPure, Category="ARPG|Mining|Tools") bool HasEquippedMiningTool() const;
    UFUNCTION(BlueprintPure, Category="ARPG|Mining|Tools") bool HasValidToolForRock(const AARPGMineableRock* Rock) const;

    UFUNCTION(BlueprintCallable, Category="ARPG|Mining|Combat Integration") bool TryMineRockWithBasicAttack(AActor* OptionalTarget = nullptr);
    bool TryHandleBasicAttackAsMining(AActor* OptionalTarget, bool& bOutHandled);
    UFUNCTION(BlueprintCallable, Category="ARPG|Mining") bool CanMineRock(const AARPGMineableRock* Rock, FText& OutFailureReason) const;
    UFUNCTION(BlueprintPure, Category="ARPG|Mining") float CalculateMiningPower(const AARPGMineableRock* Rock) const;
    UFUNCTION(BlueprintCallable, Category="ARPG|Mining|Progression", meta=(BlueprintAuthorityOnly)) void AwardMiningXP(int64 Amount);

    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
    UFUNCTION(Server, Reliable) void ServerStartMining(AARPGMineableRock* Rock, bool bSingleSwing);
    UFUNCTION(Server, Reliable) void ServerStopMining();
    UFUNCTION(Client, Reliable) void ClientMiningResult(bool bSuccess, const FText& Message);
    UFUNCTION(NetMulticast, Unreliable) void MulticastPlayMiningMontage(AARPGMineableRock* Rock, UARPGItemDefinition* EquippedTool);
    UFUNCTION() void OnRep_CurrentRock(AARPGMineableRock* OldRock);

    bool BeginMiningAuthority(AARPGMineableRock* Rock, bool bSingleSwing);
    void StopMiningAuthority(const FText& Message = FText::GetEmpty(), bool bReportResult = false);
    void StartSwingAuthority();
    void ResolveSwingImpactAuthority();
    UARPGItemDefinition* FindBestToolForRock(const AARPGMineableRock* Rock, FGuid* OutInstanceId = nullptr) const;
    AARPGMineableRock* ResolveBasicAttackRock(AActor* OptionalTarget) const;
    bool IsOwnerAlive() const;
    EARPGSkillXPModel GetNativeXPModel() const;

    FTimerHandle SwingTimer;
    FTimerHandle ImpactTimer;
    bool bSingleSwingActive = false;
    float LastBasicAttackMiningAt = -1000.f;
};
