#include "Components/ARPGSlayerComponent.h"
#include "Components/ARPGSkillComponent.h"
#include "Data/ARPGSlayerDefinition.h"
#include "Net/UnrealNetwork.h"

UARPGSlayerComponent::UARPGSlayerComponent() { SetIsReplicatedByDefault(true); }

bool UARPGSlayerComponent::RequestTask(const UARPGSlayerMasterDefinition* Master, int32 SlayerLevel)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || !Master || HasActiveTask()) return false;
    TArray<const FARPGSlayerTaskOption*> Eligible;
    float TotalWeight = 0.f;
    for (const FARPGSlayerTaskOption& Option : Master->Tasks)
    {
        if (SlayerLevel >= Option.MinimumSlayerLevel && Option.Weight > 0.f) { Eligible.Add(&Option); TotalWeight += Option.Weight; }
    }
    if (Eligible.Num() == 0 || TotalWeight <= 0.f) return false;
    float Roll = FMath::FRandRange(0.f, TotalWeight);
    const FARPGSlayerTaskOption* Chosen = Eligible.Last();
    for (const FARPGSlayerTaskOption* Option : Eligible) { Roll -= Option->Weight; if (Roll <= 0.f) { Chosen = Option; break; } }
    ActiveTask = FARPGSlayerTask();
    ActiveTask.TaskInstanceId = FGuid::NewGuid(); ActiveTask.MasterId = Master->DefinitionId; ActiveTask.TaskId = Chosen->TaskId; ActiveTask.TargetCategory = Chosen->TargetCategory;
    ActiveTask.RequiredKills = FMath::RandRange(FMath::Min(Chosen->MinKills, Chosen->MaxKills), FMath::Max(Chosen->MinKills, Chosen->MaxKills));
    ActiveTask.SlayerXPPerKill = Chosen->XPPerKill; ActiveTask.CompletionPoints = Chosen->CompletionPoints;
    OnSlayerTaskChanged.Broadcast(ActiveTask); return true;
}

bool UARPGSlayerComponent::RegisterKill(FGameplayTag SlayerCategory)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || !HasActiveTask() || !SlayerCategory.IsValid() || !SlayerCategory.MatchesTag(ActiveTask.TargetCategory)) return false;
    ++ActiveTask.CurrentKills;
    if (UARPGSkillComponent* Skills = GetOwner()->FindComponentByClass<UARPGSkillComponent>()) Skills->AddSkillXP(TEXT("Slayer"), ActiveTask.SlayerXPPerKill);
    if (ActiveTask.CurrentKills >= ActiveTask.RequiredKills)
    {
        ActiveTask.CurrentKills = ActiveTask.RequiredKills; ActiveTask.bComplete = true; SlayerPoints += ActiveTask.CompletionPoints; ++TaskStreak;
    }
    OnSlayerTaskChanged.Broadcast(ActiveTask); return true;
}

bool UARPGSlayerComponent::CancelTask(int32 PointCost)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || ActiveTask.TaskId.IsNone() || PointCost < 0 || SlayerPoints < PointCost) return false;
    SlayerPoints -= PointCost; TaskStreak = 0; ActiveTask = FARPGSlayerTask(); OnSlayerTaskChanged.Broadcast(ActiveTask); return true;
}

void UARPGSlayerComponent::RestoreSlayerState(const FARPGSlayerTask& Task, int32 Points, int32 Streak)
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return; ActiveTask = Task; SlayerPoints = FMath::Max(0, Points); TaskStreak = FMath::Max(0, Streak);
}

void UARPGSlayerComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps); DOREPLIFETIME(UARPGSlayerComponent, ActiveTask); DOREPLIFETIME(UARPGSlayerComponent, SlayerPoints); DOREPLIFETIME(UARPGSlayerComponent, TaskStreak);
}
