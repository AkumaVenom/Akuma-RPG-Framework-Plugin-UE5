#include "Components/ARPGSlayerMasterComponent.h"
#include "Components/ARPGProgressionComponent.h"
#include "Components/ARPGSkillComponent.h"
#include "Components/ARPGSlayerComponent.h"
#include "Data/ARPGSlayerDefinition.h"

UARPGSlayerMasterComponent::UARPGSlayerMasterComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

bool UARPGSlayerMasterComponent::CanAssignTaskTo(AActor* Character) const
{
    if (!Character || !MasterDefinition) return false;
    const UARPGSlayerComponent* Slayer = Character->FindComponentByClass<UARPGSlayerComponent>();
    const UARPGSkillComponent* Skills = Character->FindComponentByClass<UARPGSkillComponent>();
    const UARPGProgressionComponent* Progression = Character->FindComponentByClass<UARPGProgressionComponent>();
    if (!Slayer || !Skills || Slayer->HasActiveTask()) return false;
    if (Progression && Progression->Level < FMath::Max(1, MasterDefinition->MinimumCombatLevel)) return false;

    const int32 SlayerLevel = Skills->GetSkillLevel(TEXT("Slayer"));
    for (const FARPGSlayerTaskOption& Option : MasterDefinition->Tasks)
        if (Option.Weight > 0.f && SlayerLevel >= Option.MinimumSlayerLevel) return true;
    return false;
}

bool UARPGSlayerMasterComponent::RequestTaskFor(AActor* Character)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || !CanAssignTaskTo(Character)) return false;
    UARPGSlayerComponent* Slayer = Character->FindComponentByClass<UARPGSlayerComponent>();
    UARPGSkillComponent* Skills = Character->FindComponentByClass<UARPGSkillComponent>();
    return Slayer && Skills && Slayer->RequestTask(MasterDefinition, Skills->GetSkillLevel(TEXT("Slayer")));
}

bool UARPGSlayerMasterComponent::CancelTaskFor(AActor* Character)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || !Character || !MasterDefinition) return false;
    UARPGSlayerComponent* Slayer = Character->FindComponentByClass<UARPGSlayerComponent>();
    return Slayer && Slayer->CancelTask(FMath::Max(0, MasterDefinition->CancelPointCost));
}
