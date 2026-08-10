#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ARPGTypes.h"
#include "ARPGStatsComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FARPGOnHealthChanged, float, NewHealth, float, Delta);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FARPGOnDeath);

UCLASS(ClassGroup=(ARPG), BlueprintType, Blueprintable, meta=(BlueprintSpawnableComponent))
class AKUMASRPGFRAMEWORK_API UARPGStatsComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UARPGStatsComponent();

    UPROPERTY(EditAnywhere, BlueprintReadOnly, ReplicatedUsing=OnRep_Health, Category="Vitals") float Health = 100.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, Category="Vitals") float MaxHealth = 100.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, Category="Vitals") float Mana = 100.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, Category="Vitals") float MaxMana = 100.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, Category="Vitals") float Stamina = 100.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, Category="Vitals") float MaxStamina = 100.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, Category="Combat") float Armor = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, Category="Combat") float AttackPower = 10.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, Category="Combat") float SpellPower = 10.f;

    UPROPERTY(BlueprintAssignable, Category="Events") FARPGOnHealthChanged OnHealthChanged;
    UPROPERTY(BlueprintAssignable, Category="Events") FARPGOnDeath OnDeath;

    UFUNCTION(BlueprintCallable, Category="ARPG|Stats", meta=(BlueprintAuthorityOnly)) bool ApplyDamage(float Amount);
    UFUNCTION(BlueprintCallable, Category="ARPG|Stats", meta=(BlueprintAuthorityOnly)) bool Heal(float Amount);
    UFUNCTION(BlueprintCallable, Category="ARPG|Stats", meta=(BlueprintAuthorityOnly)) bool SpendMana(float Amount);
    UFUNCTION(BlueprintCallable, Category="ARPG|Stats", meta=(BlueprintAuthorityOnly)) bool SpendStamina(float Amount);
    UFUNCTION(BlueprintCallable, Category="ARPG|Stats", meta=(BlueprintAuthorityOnly)) void RestoreMana(float Amount);
    UFUNCTION(BlueprintCallable, Category="ARPG|Stats", meta=(BlueprintAuthorityOnly)) void RestoreStamina(float Amount);
    UFUNCTION(BlueprintCallable, Category="ARPG|Stats", meta=(BlueprintAuthorityOnly)) void RestoreAllVitals();
    UFUNCTION(BlueprintPure, Category="ARPG|Stats") float GetHealthPercent() const;

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
    UFUNCTION() void OnRep_Health(float OldHealth);
};
