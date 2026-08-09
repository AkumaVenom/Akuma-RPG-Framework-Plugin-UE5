#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ARPGBossComponent.generated.h"

class UARPGBossDefinition;
class UARPGStatsComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FARPGBossPhaseChanged, FName, OldPhase, FName, NewPhase);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FARPGBossEncounterEvent);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FARPGBossDefeated, AActor*, BossActor);

UCLASS(ClassGroup=(ARPG), meta=(BlueprintSpawnableComponent))
class AKUMASRPGFRAMEWORK_API UARPGBossComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UARPGBossComponent();

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss") TObjectPtr<UARPGBossDefinition> Definition;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category="Boss") bool bEncounterActive = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category="Boss") bool bEnraged = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category="Boss") bool bWaitingForWorldRespawn = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category="Boss") FName CurrentPhase = NAME_None;
    UPROPERTY(BlueprintAssignable) FARPGBossPhaseChanged OnPhaseChanged;
    UPROPERTY(BlueprintAssignable) FARPGBossEncounterEvent OnEncounterStarted;
    UPROPERTY(BlueprintAssignable) FARPGBossEncounterEvent OnEncounterReset;
    UPROPERTY(BlueprintAssignable) FARPGBossEncounterEvent OnEnraged;
    UPROPERTY(BlueprintAssignable) FARPGBossDefeated OnBossDefeated;

    UFUNCTION(BlueprintCallable, Category="ARPG|Boss", meta=(BlueprintAuthorityOnly)) void StartEncounter();
    UFUNCTION(BlueprintCallable, Category="ARPG|Boss", meta=(BlueprintAuthorityOnly)) void ResetEncounter();
    UFUNCTION(BlueprintCallable, Category="ARPG|Boss", meta=(BlueprintAuthorityOnly)) void EvaluatePhase();
    UFUNCTION(BlueprintCallable, Category="ARPG|Boss", meta=(BlueprintAuthorityOnly)) void SetEnraged(bool bNewEnraged);
    UFUNCTION(BlueprintCallable, Category="ARPG|Boss", meta=(BlueprintAuthorityOnly)) bool ApplyBossDamage(float Amount, AActor* Contributor);
    UFUNCTION(BlueprintCallable, Category="ARPG|Boss", meta=(BlueprintAuthorityOnly)) void RegisterContribution(AActor* Contributor, float Amount);
    UFUNCTION(BlueprintPure, Category="ARPG|Boss") TArray<AActor*> GetEligibleContributors() const;
    UFUNCTION(BlueprintPure, Category="ARPG|Boss") float GetContributionPercent(AActor* Contributor) const;
    UFUNCTION(BlueprintCallable, Category="ARPG|Boss", meta=(BlueprintAuthorityOnly)) void ApplyPlayerCountScaling(int32 PlayerCount, float HealthPerExtraPlayer=0.65f, float DamagePerExtraPlayer=0.15f);

    virtual void BeginPlay() override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
protected:
    FVector HomeLocation = FVector::ZeroVector;
    FRotator HomeRotation = FRotator::ZeroRotator;
    float BaseMaxHealth = 0.f;
    float BaseAttackPower = 0.f;
    TMap<TWeakObjectPtr<AActor>, float> Contributions;
    FTimerHandle EnrageTimer;
    FTimerHandle LeashTimer;
    FTimerHandle WorldRespawnTimer;
    UFUNCTION() void HandleHealthChanged(float NewHealth, float Delta);
    UFUNCTION() void HandleBossDeath();
    void CheckLeash();
    void RespawnWorldBoss();
};
