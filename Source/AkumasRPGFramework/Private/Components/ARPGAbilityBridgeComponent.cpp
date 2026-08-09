#include "Components/ARPGAbilityBridgeComponent.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "GameplayEffect.h"

static UAbilitySystemComponent* ARPG_GetASC(AActor* Owner)
{
    if (IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(Owner)) return ASI->GetAbilitySystemComponent();
    return Owner ? Owner->FindComponentByClass<UAbilitySystemComponent>() : nullptr;
}

bool UARPGAbilityBridgeComponent::TryActivateAbilityByTag(FGameplayTag AbilityTag)
{
    UAbilitySystemComponent* ASC = ARPG_GetASC(GetOwner());
    if (!ASC || !AbilityTag.IsValid()) return false;
    FGameplayTagContainer Tags; Tags.AddTag(AbilityTag); return ASC->TryActivateAbilitiesByTag(Tags, true);
}

FActiveGameplayEffectHandle UARPGAbilityBridgeComponent::ApplyEffectToOwner(TSubclassOf<UGameplayEffect> EffectClass, float Level)
{
    UAbilitySystemComponent* ASC = ARPG_GetASC(GetOwner());
    if (!ASC || !EffectClass || !GetOwner() || !GetOwner()->HasAuthority()) return FActiveGameplayEffectHandle();
    FGameplayEffectContextHandle Context = ASC->MakeEffectContext(); Context.AddSourceObject(GetOwner());
    FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(EffectClass, Level, Context);
    return Spec.IsValid() ? ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get()) : FActiveGameplayEffectHandle();
}

int32 UARPGAbilityBridgeComponent::RemoveEffectsWithGrantedTags(FGameplayTagContainer Tags)
{
    UAbilitySystemComponent* ASC = ARPG_GetASC(GetOwner());
    if (!ASC || !GetOwner() || !GetOwner()->HasAuthority()) return 0;
    FGameplayEffectQuery Query = FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(Tags);
    return ASC->RemoveActiveEffects(Query);
}
