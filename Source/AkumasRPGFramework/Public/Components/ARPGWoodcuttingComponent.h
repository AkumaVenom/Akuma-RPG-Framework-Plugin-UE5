#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "Engine/EngineTypes.h"
#include "ARPGWoodcuttingComponent.generated.h"

class AARPGTree;
class UARPGItemDefinition;
class UARPGSkillDefinition;
class UAnimMontage;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FARPGWoodcuttingTreeEvent, AARPGTree*, Tree);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FARPGWoodcuttingSwingEvent, AARPGTree*, Tree, float, ChopPower, float, RemainingHealth);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FARPGWoodcuttingResultEvent, bool, bSuccess, FText, Message);

UCLASS(ClassGroup=(ARPG), meta=(BlueprintSpawnableComponent))
class AKUMASRPGFRAMEWORK_API UARPGWoodcuttingComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UARPGWoodcuttingComponent();

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Woodcutting|Progression") FName SkillId = TEXT("Woodcutting");
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Woodcutting|Progression") TObjectPtr<UARPGSkillDefinition> SkillDefinition = nullptr;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Woodcutting|Progression", meta=(ClampMin="0.0")) float SkillPowerPerLevel = 0.01f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Woodcutting|Chopping", meta=(ClampMin="50.0")) float MaxChopDistance = 450.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Woodcutting|Chopping", meta=(ClampMin="0.01")) float BaseChopPower = 20.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Woodcutting|Chopping", meta=(ClampMin="0.1")) float SwingIntervalSeconds = 1.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Woodcutting|Chopping", meta=(ClampMin="0.0")) float SwingImpactDelay = 0.25f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Woodcutting|Chopping") bool bAutoRepeatChops = true;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Woodcutting|Chopping") TSoftObjectPtr<UAnimMontage> DefaultChopMontage;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Woodcutting|Chopping", meta=(ClampMin="50.0")) float AutoTargetTraceDistance = 600.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Woodcutting|Chopping", meta=(ClampMin="0.0")) float AutoTargetTraceRadius = 30.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Woodcutting|Chopping") TEnumAsByte<ECollisionChannel> AutoTargetTraceChannel = ECC_Visibility;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Woodcutting|Tools") FGameplayTag PreferredWoodcuttingToolTag;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Woodcutting|Tools") bool bUseBestEquippedTool = true;

    // Quality-of-life combat integration: pressing the normal Basic Attack input with an equipped axe
    // automatically becomes one authoritative Woodcutting swing when a harvestable tree is the intended target.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Woodcutting|Combat Integration", meta=(DisplayName="Basic Attack Auto Chops Trees")) bool bAutoChopTreesWithBasicAttack = true;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Woodcutting|Combat Integration", meta=(EditCondition="bAutoChopTreesWithBasicAttack", DisplayName="Basic Attack Requires Equipped Axe")) bool bBasicAttackRequiresEquippedAxe = true;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Woodcutting|Combat Integration", meta=(DisplayName="Use Combat Melee Montage As Chop Fallback")) bool bUseCombatMeleeMontageAsChopFallback = true;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, ReplicatedUsing=OnRep_CurrentTree, Category="Woodcutting|State") TObjectPtr<AARPGTree> CurrentTree = nullptr;

    UPROPERTY(BlueprintAssignable, Category="Woodcutting|Events") FARPGWoodcuttingTreeEvent OnWoodcuttingStarted;
    UPROPERTY(BlueprintAssignable, Category="Woodcutting|Events") FARPGWoodcuttingTreeEvent OnWoodcuttingStopped;
    UPROPERTY(BlueprintAssignable, Category="Woodcutting|Events") FARPGWoodcuttingSwingEvent OnWoodcuttingSwing;
    UPROPERTY(BlueprintAssignable, Category="Woodcutting|Events") FARPGWoodcuttingResultEvent OnWoodcuttingResult;

    UFUNCTION(BlueprintCallable, Category="ARPG|Woodcutting") bool StartWoodcuttingFromView();
    UFUNCTION(BlueprintCallable, Category="ARPG|Woodcutting") AARPGTree* FindWoodcuttingTreeInView() const;
    UFUNCTION(BlueprintCallable, Category="ARPG|Woodcutting") bool StartWoodcutting(AARPGTree* Tree);
    UFUNCTION(BlueprintCallable, Category="ARPG|Woodcutting") bool ChopTreeOnce(AARPGTree* Tree);
    UFUNCTION(BlueprintCallable, Category="ARPG|Woodcutting") void StopWoodcutting();

    UFUNCTION(BlueprintPure, Category="ARPG|Woodcutting") bool IsWoodcutting() const { return CurrentTree != nullptr; }
    UFUNCTION(BlueprintPure, Category="ARPG|Woodcutting") AARPGTree* GetCurrentTree() const { return CurrentTree; }
    UFUNCTION(BlueprintPure, Category="ARPG|Woodcutting|Progression") int32 GetWoodcuttingLevel() const;
    UFUNCTION(BlueprintPure, Category="ARPG|Woodcutting|Progression") int64 GetWoodcuttingXP() const;
    UFUNCTION(BlueprintPure, Category="ARPG|Woodcutting|Progression") int64 GetWoodcuttingXPForNextLevel() const;
    UFUNCTION(BlueprintPure, Category="ARPG|Woodcutting|Progression") int64 GetWoodcuttingXPRemaining() const;
    UFUNCTION(BlueprintPure, Category="ARPG|Woodcutting|Progression") float GetWoodcuttingLevelProgress() const;
    UFUNCTION(BlueprintPure, Category="ARPG|Woodcutting|Progression") bool HasWoodcuttingUnlock(FGameplayTag UnlockTag) const;

    UFUNCTION(BlueprintPure, Category="ARPG|Woodcutting|Tools") UARPGItemDefinition* GetBestEquippedWoodcuttingTool() const;
    UFUNCTION(BlueprintPure, Category="ARPG|Woodcutting|Tools") FGuid GetBestEquippedWoodcuttingToolInstanceId() const;
    UFUNCTION(BlueprintPure, Category="ARPG|Woodcutting|Tools") float GetBestEquippedToolPower() const;
    UFUNCTION(BlueprintPure, Category="ARPG|Woodcutting|Tools") int32 GetBestEquippedToolTier() const;
    UFUNCTION(BlueprintPure, Category="ARPG|Woodcutting|Tools") bool HasEquippedWoodcuttingTool() const;
    UFUNCTION(BlueprintPure, Category="ARPG|Woodcutting|Tools") bool HasValidToolForTree(const AARPGTree* Tree) const;
    UFUNCTION(BlueprintCallable, Category="ARPG|Woodcutting|Combat Integration") bool TryChopTreeWithBasicAttack(AActor* OptionalTarget = nullptr);
    bool TryHandleBasicAttackAsWoodcutting(AActor* OptionalTarget, bool& bOutHandled);
    UFUNCTION(BlueprintCallable, Category="ARPG|Woodcutting") bool CanChopTree(const AARPGTree* Tree, FText& OutFailureReason) const;
    UFUNCTION(BlueprintPure, Category="ARPG|Woodcutting") float CalculateChopPower(const AARPGTree* Tree) const;

    UFUNCTION(BlueprintCallable, Category="ARPG|Woodcutting|Progression", meta=(BlueprintAuthorityOnly)) void AwardWoodcuttingXP(int64 Amount);

    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
    UFUNCTION(Server, Reliable) void ServerStartWoodcutting(AARPGTree* Tree, bool bSingleSwing);
    UFUNCTION(Server, Reliable) void ServerStopWoodcutting();
    UFUNCTION(Client, Reliable) void ClientWoodcuttingResult(bool bSuccess, const FText& Message);
    UFUNCTION(NetMulticast, Unreliable) void MulticastPlayChopMontage(AARPGTree* Tree, UARPGItemDefinition* EquippedTool);
    UFUNCTION() void OnRep_CurrentTree(AARPGTree* OldTree);

    bool BeginWoodcuttingAuthority(AARPGTree* Tree, bool bSingleSwing);
    void StopWoodcuttingAuthority(const FText& Message = FText::GetEmpty(), bool bReportResult = false);
    void StartSwingAuthority();
    void ResolveSwingImpactAuthority();
    UARPGItemDefinition* FindBestToolForTree(const AARPGTree* Tree, FGuid* OutInstanceId=nullptr) const;
    AARPGTree* ResolveBasicAttackTree(AActor* OptionalTarget) const;
    bool IsOwnerAlive() const;

    FTimerHandle SwingTimer;
    FTimerHandle ImpactTimer;
    bool bSingleSwingActive = false;
    float LastBasicAttackChopAt = -1000.f;
};
