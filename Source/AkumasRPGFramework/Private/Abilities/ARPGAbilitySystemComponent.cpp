#include "Abilities/ARPGAbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"
UARPGAbilitySystemComponent::UARPGAbilitySystemComponent(){ SetIsReplicatedByDefault(true); SetReplicationMode(EGameplayEffectReplicationMode::Mixed); }
void UARPGAbilitySystemComponent::GrantAbilities(const TArray<TSubclassOf<UGameplayAbility>>& Abilities,int32 AbilityLevel){ if(!IsOwnerActorAuthoritative()) return; for(const auto& C:Abilities){ if(C) FrameworkGrantedHandles.Add(GiveAbility(FGameplayAbilitySpec(C,FMath::Max(1,AbilityLevel)))); } }
void UARPGAbilitySystemComponent::RemoveAllGrantedAbilities(){ if(!IsOwnerActorAuthoritative()) return; for(const auto& H:FrameworkGrantedHandles) ClearAbility(H); FrameworkGrantedHandles.Reset(); }
