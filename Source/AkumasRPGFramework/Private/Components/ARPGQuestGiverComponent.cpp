#include "Components/ARPGQuestGiverComponent.h"
#include "Components/ARPGQuestComponent.h"
#include "Data/ARPGQuestDefinition.h"

UARPGQuestGiverComponent::UARPGQuestGiverComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

bool UARPGQuestGiverComponent::OffersQuest(const UARPGQuestDefinition* Quest) const
{
    return Quest && Quests.Contains(Quest);
}

EARPGQuestGiverStatus UARPGQuestGiverComponent::GetQuestStatus(AActor* Character, const UARPGQuestDefinition* Quest) const
{
    if (!Character || !OffersQuest(Quest)) return EARPGQuestGiverStatus::Unavailable;
    const UARPGQuestComponent* QuestLog = Character->FindComponentByClass<UARPGQuestComponent>();
    if (!QuestLog) return EARPGQuestGiverStatus::Unavailable;

    for (const FARPGQuestRuntime& Runtime : QuestLog->Quests)
    {
        if (Runtime.QuestId != Quest->DefinitionId) continue;
        if (Runtime.State == EARPGQuestState::ObjectivesComplete) return bAllowTurnIn ? EARPGQuestGiverStatus::ReadyToTurnIn : EARPGQuestGiverStatus::InProgress;
        if (Runtime.State == EARPGQuestState::Active) return EARPGQuestGiverStatus::InProgress;
        if (Runtime.State == EARPGQuestState::Completed && !Quest->bRepeatable) return EARPGQuestGiverStatus::Completed;
    }
    return QuestLog->CanAcceptQuest(Quest) ? EARPGQuestGiverStatus::Available : EARPGQuestGiverStatus::Unavailable;
}

TArray<UARPGQuestDefinition*> UARPGQuestGiverComponent::GetAvailableQuests(AActor* Character) const
{
    TArray<UARPGQuestDefinition*> Result;
    for (UARPGQuestDefinition* Quest : Quests)
        if (Quest && GetQuestStatus(Character, Quest) == EARPGQuestGiverStatus::Available) Result.Add(Quest);
    return Result;
}

bool UARPGQuestGiverComponent::AcceptQuestFor(AActor* Character, UARPGQuestDefinition* Quest)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || !Character || !OffersQuest(Quest)) return false;
    UARPGQuestComponent* QuestLog = Character->FindComponentByClass<UARPGQuestComponent>();
    return QuestLog && QuestLog->AcceptQuest(Quest);
}

bool UARPGQuestGiverComponent::TurnInQuestFor(AActor* Character, UARPGQuestDefinition* Quest)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || !bAllowTurnIn || !Character || !OffersQuest(Quest)) return false;
    UARPGQuestComponent* QuestLog = Character->FindComponentByClass<UARPGQuestComponent>();
    return QuestLog && QuestLog->CompleteQuest(Quest->DefinitionId);
}
