#include "Components/ARPGAbilityBridgeComponent.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "GameplayEffect.h"
#include "Components/ARPGCombatComponent.h"
#include "Components/ARPGTargetingComponent.h"

static UAbilitySystemComponent* ARPG_GetASC(AActor* Owner)
{
    if (IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(Owner)) return ASI->GetAbilitySystemComponent();
    return Owner ? Owner->FindComponentByClass<UAbilitySystemComponent>() : nullptr;
}

bool UARPGAbilityBridgeComponent::TryActivateAbilityByTag(FGameplayTag AbilityTag)
{
    UAbilitySystemComponent* ASC = ARPG_GetASC(GetOwner());
    if (!ASC || !AbilityTag.IsValid()) return false;
    if (UARPGTargetingComponent* Targeting = GetOwner() ? GetOwner()->FindComponentByClass<UARPGTargetingComponent>() : nullptr)
        Targeting->RequestAbilityFacing();
    FGameplayTagContainer Tags;
    Tags.AddTag(AbilityTag);
    return ASC->TryActivateAbilitiesByTag(Tags, true);
}

bool UARPGAbilityBridgeComponent::TryActivateAbilityByTagWithTarget(FGameplayTag AbilityTag, AActor* ExplicitTarget)
{
    if (ExplicitTarget && GetOwner())
    {
        if (UARPGTargetingComponent* Targeting = GetOwner()->FindComponentByClass<UARPGTargetingComponent>())
        {
            if (Targeting->IsValidLockOnTarget(ExplicitTarget)) Targeting->SetLockOnTarget(ExplicitTarget);
        }
        else if (UARPGCombatComponent* Combat = GetOwner()->FindComponentByClass<UARPGCombatComponent>())
        {
            Combat->SetCombatTarget(ExplicitTarget);
        }
    }
    return TryActivateAbilityByTag(AbilityTag);
}

AActor* UARPGAbilityBridgeComponent::GetPreferredAbilityTarget() const
{
    if (!GetOwner()) return nullptr;
    if (const UARPGTargetingComponent* Targeting = GetOwner()->FindComponentByClass<UARPGTargetingComponent>())
        if (AActor* Target = Targeting->GetCurrentTarget()) return Target;
    if (const UARPGCombatComponent* Combat = GetOwner()->FindComponentByClass<UARPGCombatComponent>())
        return IsValid(Combat->CombatTarget) ? Combat->CombatTarget.Get() : nullptr;
    return nullptr;
}

FGameplayAbilityTargetDataHandle UARPGAbilityBridgeComponent::MakePreferredAbilityTargetData() const
{
    FGameplayAbilityTargetDataHandle Handle;
    AActor* Target = GetPreferredAbilityTarget();
    if (!Target) return Handle;
    FGameplayAbilityTargetData_ActorArray* Data = new FGameplayAbilityTargetData_ActorArray();
    Data->TargetActorArray.Add(Target);
    Handle.Add(Data);
    return Handle;
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
