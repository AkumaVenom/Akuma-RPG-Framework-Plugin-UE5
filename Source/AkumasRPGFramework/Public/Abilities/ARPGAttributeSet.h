#pragma once
#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "ARPGAttributeSet.generated.h"
#define ARPG_ATTRIBUTE_ACCESSORS(ClassName,PropertyName) GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName,PropertyName) GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)
UCLASS(BlueprintType) class AKUMASRPGFRAMEWORK_API UARPGAttributeSet:public UAttributeSet { GENERATED_BODY() public: UARPGAttributeSet();
#define ATTR(Name,Cat) UPROPERTY(BlueprintReadOnly,ReplicatedUsing=OnRep_##Name,Category=Cat) FGameplayAttributeData Name; ARPG_ATTRIBUTE_ACCESSORS(UARPGAttributeSet,Name)
ATTR(Health,"Vitals") ATTR(MaxHealth,"Vitals") ATTR(Mana,"Vitals") ATTR(MaxMana,"Vitals") ATTR(Stamina,"Vitals") ATTR(MaxStamina,"Vitals") ATTR(Strength,"Primary") ATTR(Agility,"Primary") ATTR(Intellect,"Primary") ATTR(Armor,"Combat") ATTR(AttackPower,"Combat") ATTR(SpellPower,"Combat")
#undef ATTR
virtual void PreAttributeChange(const FGameplayAttribute& Attribute,float& NewValue) override; virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override; virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
protected:
#define REP(Name) UFUNCTION() void OnRep_##Name(const FGameplayAttributeData& OldValue);
REP(Health) REP(MaxHealth) REP(Mana) REP(MaxMana) REP(Stamina) REP(MaxStamina) REP(Strength) REP(Agility) REP(Intellect) REP(Armor) REP(AttackPower) REP(SpellPower)
#undef REP
};
