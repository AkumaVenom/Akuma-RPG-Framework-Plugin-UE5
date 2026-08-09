#include "Abilities/ARPGAttributeSet.h"
#include "GameplayEffectExtension.h"
#include "Net/UnrealNetwork.h"

UARPGAttributeSet::UARPGAttributeSet()
{
    InitMaxHealth(100.f);
    InitHealth(100.f);
    InitMaxMana(100.f);
    InitMana(100.f);
    InitMaxStamina(100.f);
    InitStamina(100.f);
    InitStrength(10.f);
    InitAgility(10.f);
    InitIntellect(10.f);
    InitArmor(0.f);
    InitAttackPower(10.f);
    InitSpellPower(10.f);
}

void UARPGAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
    Super::PreAttributeChange(Attribute, NewValue);

    if (Attribute == GetMaxHealthAttribute() || Attribute == GetMaxManaAttribute() || Attribute == GetMaxStaminaAttribute())
    {
        NewValue = FMath::Max(1.f, NewValue);
    }
    else if (Attribute == GetHealthAttribute())
    {
        NewValue = FMath::Clamp(NewValue, 0.f, GetMaxHealth());
    }
    else if (Attribute == GetManaAttribute())
    {
        NewValue = FMath::Clamp(NewValue, 0.f, GetMaxMana());
    }
    else if (Attribute == GetStaminaAttribute())
    {
        NewValue = FMath::Clamp(NewValue, 0.f, GetMaxStamina());
    }
}

void UARPGAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
    Super::PostGameplayEffectExecute(Data);

    if (Data.EvaluatedData.Attribute == GetHealthAttribute())
    {
        SetHealth(FMath::Clamp(GetHealth(), 0.f, GetMaxHealth()));
    }
    else if (Data.EvaluatedData.Attribute == GetManaAttribute())
    {
        SetMana(FMath::Clamp(GetMana(), 0.f, GetMaxMana()));
    }
    else if (Data.EvaluatedData.Attribute == GetStaminaAttribute())
    {
        SetStamina(FMath::Clamp(GetStamina(), 0.f, GetMaxStamina()));
    }
}

void UARPGAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME_CONDITION_NOTIFY(UARPGAttributeSet, Health, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UARPGAttributeSet, MaxHealth, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UARPGAttributeSet, Mana, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UARPGAttributeSet, MaxMana, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UARPGAttributeSet, Stamina, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UARPGAttributeSet, MaxStamina, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UARPGAttributeSet, Strength, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UARPGAttributeSet, Agility, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UARPGAttributeSet, Intellect, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UARPGAttributeSet, Armor, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UARPGAttributeSet, AttackPower, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UARPGAttributeSet, SpellPower, COND_None, REPNOTIFY_Always);
}

#define ARPG_REP_NOTIFY(AttributeName) \
    void UARPGAttributeSet::OnRep_##AttributeName(const FGameplayAttributeData& OldValue) \
    { \
        GAMEPLAYATTRIBUTE_REPNOTIFY(UARPGAttributeSet, AttributeName, OldValue); \
    }

ARPG_REP_NOTIFY(Health)
ARPG_REP_NOTIFY(MaxHealth)
ARPG_REP_NOTIFY(Mana)
ARPG_REP_NOTIFY(MaxMana)
ARPG_REP_NOTIFY(Stamina)
ARPG_REP_NOTIFY(MaxStamina)
ARPG_REP_NOTIFY(Strength)
ARPG_REP_NOTIFY(Agility)
ARPG_REP_NOTIFY(Intellect)
ARPG_REP_NOTIFY(Armor)
ARPG_REP_NOTIFY(AttackPower)
ARPG_REP_NOTIFY(SpellPower)

#undef ARPG_REP_NOTIFY
