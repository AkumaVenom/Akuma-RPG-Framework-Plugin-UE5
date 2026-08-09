#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ARPGTypes.h"
#include "ARPGCombatComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FARPGLifeStateChanged, EARPGLifeState, NewState);

UCLASS(ClassGroup=(ARPG), meta=(BlueprintSpawnableComponent))
class AKUMASRPGFRAMEWORK_API UARPGCombatComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UARPGCombatComponent();

    UPROPERTY(EditAnywhere, BlueprintReadOnly, ReplicatedUsing=OnRep_LifeState, Category="Combat") EARPGLifeState LifeState = EARPGLifeState::Alive;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Combat") float RespawnDelay = 5.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animation") FARPGCombatMontageSet Montages;
    UPROPERTY(BlueprintAssignable) FARPGLifeStateChanged OnLifeStateChanged;

    UFUNCTION(BlueprintCallable, Category="ARPG|Combat", meta=(BlueprintAuthorityOnly)) void Kill();
    UFUNCTION(BlueprintCallable, Category="ARPG|Combat", meta=(BlueprintAuthorityOnly)) void RespawnAtTransform(const FTransform& Transform);
    UFUNCTION(BlueprintPure, Category="ARPG|Combat") bool IsAlive() const { return LifeState == EARPGLifeState::Alive; }
    UFUNCTION(BlueprintCallable, Category="ARPG|Combat|Animation") UAnimMontage* PickRandomAttackMontage(bool bMagic, bool bRanged) const;

    virtual void BeginPlay() override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
protected:
    UFUNCTION() void OnRep_LifeState(EARPGLifeState OldState);
    UFUNCTION() void HandleStatsDeath();
};
