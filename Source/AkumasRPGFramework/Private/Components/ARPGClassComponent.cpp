#include "Components/ARPGClassComponent.h"
#include "Data/ARPGClassDefinition.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Abilities/GameplayAbility.h"
#include "Net/UnrealNetwork.h"

UARPGClassComponent::UARPGClassComponent() { SetIsReplicatedByDefault(true); }

bool UARPGClassComponent::ApplyClassDefinition(UARPGClassDefinition* NewClass, bool bGrantAbilities)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || !NewClass) return false;
    ClassDefinition = NewClass;
    if (bGrantAbilities)
    {
        if (IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(GetOwner()))
        {
            if (UAbilitySystemComponent* ASC = ASI->GetAbilitySystemComponent())
            {
                for (TSubclassOf<UGameplayAbility> AbilityClass : NewClass->GrantedAbilities)
                {
                    if (AbilityClass) ASC->GiveAbility(FGameplayAbilitySpec(AbilityClass, 1, INDEX_NONE, GetOwner()));
                }
            }
        }
    }
    OnClassChanged.Broadcast(NewClass->DefinitionId);
    return true;
}

FName UARPGClassComponent::GetClassId() const { return ClassDefinition ? ClassDefinition->DefinitionId : NAME_None; }
void UARPGClassComponent::OnRep_ClassDefinition() { OnClassChanged.Broadcast(GetClassId()); }
void UARPGClassComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps); DOREPLIFETIME(UARPGClassComponent, ClassDefinition);
}
