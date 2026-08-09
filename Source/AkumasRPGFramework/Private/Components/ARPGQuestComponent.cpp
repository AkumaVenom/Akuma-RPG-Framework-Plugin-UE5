#include "Components/ARPGQuestComponent.h"
#include "Data/ARPGQuestDefinition.h"
#include "Components/ARPGProgressionComponent.h"
#include "Components/ARPGInventoryComponent.h"
#include "Components/ARPGCurrencyComponent.h"
#include "Components/ARPGFactionComponent.h"
#include "Utilities/ARPGAssetLibrary.h"
#include "Net/UnrealNetwork.h"

UARPGQuestComponent::UARPGQuestComponent() { SetIsReplicatedByDefault(true); }

int32 UARPGQuestComponent::GetActiveQuestCount() const
{
    int32 Count = 0;
    for (const FARPGQuestRuntime& Q : Quests)
        if (Q.State == EARPGQuestState::Active || Q.State == EARPGQuestState::ObjectivesComplete) ++Count;
    return Count;
}

bool UARPGQuestComponent::CanAcceptQuest(const UARPGQuestDefinition* Quest) const
{
    if (!GetOwner() || !Quest || Quest->DefinitionId.IsNone() || GetActiveQuestCount() >= MaxActiveQuests) return false;
    if (const UARPGProgressionComponent* Progression = GetOwner()->FindComponentByClass<UARPGProgressionComponent>())
        if (Progression->Level < FMath::Max(1, Quest->RequiredLevel)) return false;
    for (const FName Prereq : Quest->PrerequisiteQuestIds)
        if (!IsQuestComplete(Prereq)) return false;
    for (const FARPGQuestRuntime& Existing : Quests)
    {
        if (Existing.QuestId != Quest->DefinitionId) continue;
        if (Existing.State != EARPGQuestState::Completed) return false;
        if (!Quest->bRepeatable) return false;
    }
    return true;
}

bool UARPGQuestComponent::AcceptQuest(const UARPGQuestDefinition* Quest)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || !CanAcceptQuest(Quest)) return false;
    FARPGQuestRuntime Runtime;
    Runtime.QuestId = Quest->DefinitionId;
    Runtime.State = Quest->Objectives.Num() == 0 ? EARPGQuestState::ObjectivesComplete : EARPGQuestState::Active;
    Runtime.AcceptedAtUtc = FDateTime::UtcNow();
    for (const FARPGQuestObjectiveDefinition& Obj : Quest->Objectives)
    {
        FARPGQuestObjectiveProgress P;
        P.ObjectiveId = Obj.ObjectiveId; P.Type = Obj.Type; P.TargetTag = Obj.TargetTag; P.TargetId = Obj.TargetId;
        P.Required = FMath::Max(1, Obj.RequiredCount);
        Runtime.Objectives.Add(P);
    }
    Quests.Add(Runtime);
    OnQuestChanged.Broadcast(Runtime.QuestId);
    if (Quest->bAutoComplete && Runtime.State == EARPGQuestState::ObjectivesComplete) CompleteQuest(Runtime.QuestId);
    return true;
}

void UARPGQuestComponent::ReevaluateQuest(FARPGQuestRuntime& Runtime)
{
    if (Runtime.State != EARPGQuestState::Active) return;
    bool bAll = Runtime.Objectives.Num() == 0;
    if (Runtime.Objectives.Num() > 0)
    {
        bAll = true;
        for (const FARPGQuestObjectiveProgress& P : Runtime.Objectives) { if (!P.bComplete) { bAll = false; break; } }
    }
    if (bAll) Runtime.State = EARPGQuestState::ObjectivesComplete;
}

int32 UARPGQuestComponent::ProgressByTag(EARPGQuestObjectiveType Type, FGameplayTag TargetTag, int32 Amount)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || Amount <= 0 || !TargetTag.IsValid()) return 0;
    int32 Changed = 0;
    TArray<FName> AutoComplete;
    for (FARPGQuestRuntime& Q : Quests)
    {
        if (Q.State != EARPGQuestState::Active) continue;
        bool bAny = false;
        for (FARPGQuestObjectiveProgress& P : Q.Objectives)
        {
            if (P.Type == Type && P.TargetTag.IsValid() && TargetTag.MatchesTag(P.TargetTag) && !P.bComplete)
            {
                P.Current = FMath::Min(P.Required, P.Current + Amount); P.bComplete = P.Current >= P.Required; ++Changed; bAny = true;
            }
        }
        if (bAny)
        {
            ReevaluateQuest(Q); OnQuestChanged.Broadcast(Q.QuestId);
            if (Q.State == EARPGQuestState::ObjectivesComplete)
                if (const UARPGQuestDefinition* Def = ResolveQuestDefinition(Q.QuestId); Def && Def->bAutoComplete) AutoComplete.Add(Q.QuestId);
        }
    }
    for (const FName Id : AutoComplete) CompleteQuest(Id);
    return Changed;
}

int32 UARPGQuestComponent::ProgressById(EARPGQuestObjectiveType Type, FName TargetId, int32 Amount)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || TargetId.IsNone() || Amount <= 0) return 0;
    int32 Changed = 0;
    TArray<FName> AutoComplete;
    for (FARPGQuestRuntime& Q : Quests)
    {
        if (Q.State != EARPGQuestState::Active) continue;
        bool bAny = false;
        for (FARPGQuestObjectiveProgress& P : Q.Objectives)
        {
            if (P.Type == Type && P.TargetId == TargetId && !P.bComplete)
            {
                P.Current = FMath::Min(P.Required, P.Current + Amount); P.bComplete = P.Current >= P.Required; ++Changed; bAny = true;
            }
        }
        if (bAny)
        {
            ReevaluateQuest(Q); OnQuestChanged.Broadcast(Q.QuestId);
            if (Q.State == EARPGQuestState::ObjectivesComplete)
                if (const UARPGQuestDefinition* Def = ResolveQuestDefinition(Q.QuestId); Def && Def->bAutoComplete) AutoComplete.Add(Q.QuestId);
        }
    }
    for (const FName Id : AutoComplete) CompleteQuest(Id);
    return Changed;
}

int32 UARPGQuestComponent::SetProgressByIdAtLeast(EARPGQuestObjectiveType Type, FName TargetId, int32 AbsoluteValue)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || TargetId.IsNone() || AbsoluteValue < 0) return 0;
    int32 Changed = 0; TArray<FName> AutoComplete;
    for (FARPGQuestRuntime& Q : Quests)
    {
        if (Q.State != EARPGQuestState::Active) continue; bool bAny=false;
        for (FARPGQuestObjectiveProgress& P : Q.Objectives)
        {
            if(P.Type==Type && P.TargetId==TargetId && !P.bComplete && AbsoluteValue>P.Current)
            { P.Current=FMath::Min(P.Required,AbsoluteValue);P.bComplete=P.Current>=P.Required;++Changed;bAny=true; }
        }
        if(bAny){ReevaluateQuest(Q);OnQuestChanged.Broadcast(Q.QuestId);if(Q.State==EARPGQuestState::ObjectivesComplete)if(const UARPGQuestDefinition*Def=ResolveQuestDefinition(Q.QuestId);Def&&Def->bAutoComplete)AutoComplete.Add(Q.QuestId);}
    }
    for(const FName Id:AutoComplete)CompleteQuest(Id);return Changed;
}

const UARPGQuestDefinition* UARPGQuestComponent::ResolveQuestDefinition(FName QuestId) const
{
    return Cast<UARPGQuestDefinition>(UARPGAssetLibrary::ResolveDefinitionById(UARPGQuestDefinition::StaticClass(), QuestId));
}

bool UARPGQuestComponent::GrantRewards(const UARPGQuestDefinition* Quest)
{
    if (!Quest || !GetOwner() || !GetOwner()->HasAuthority()) return false;
    if (Quest->Rewards.CharacterXP > 0)
        if (UARPGProgressionComponent* P = GetOwner()->FindComponentByClass<UARPGProgressionComponent>()) P->AddXP(Quest->Rewards.CharacterXP);
    if (Quest->Rewards.CurrencyAmount != 0 && !Quest->Rewards.CurrencyId.IsNone())
        if (UARPGCurrencyComponent* C = GetOwner()->FindComponentByClass<UARPGCurrencyComponent>()) C->AddCurrency(Quest->Rewards.CurrencyId, Quest->Rewards.CurrencyAmount);
    if (UARPGInventoryComponent* I = GetOwner()->FindComponentByClass<UARPGInventoryComponent>())
        for (const TPair<FName,int32>& It : Quest->Rewards.ItemRewards) if (It.Value > 0) I->AddItem(It.Key, It.Value);
    if (UARPGFactionComponent* F = GetOwner()->FindComponentByClass<UARPGFactionComponent>())
        for (const TPair<FName,int32>& Rep : Quest->Rewards.ReputationRewards) if (Rep.Value != 0) F->AddReputation(Rep.Key, Rep.Value);
    return true;
}

bool UARPGQuestComponent::CompleteQuest(FName QuestId)
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return false;
    for (FARPGQuestRuntime& Q : Quests)
    {
        if (Q.QuestId == QuestId && Q.State == EARPGQuestState::ObjectivesComplete)
        {
            const UARPGQuestDefinition* Quest = ResolveQuestDefinition(QuestId);
            if (Quest) GrantRewards(Quest);
            Q.State = EARPGQuestState::Completed; ++Q.CompletionCount; Q.CompletedAtUtc = FDateTime::UtcNow();
            OnQuestChanged.Broadcast(QuestId); return true;
        }
    }
    return false;
}

bool UARPGQuestComponent::IsQuestComplete(FName QuestId) const
{
    for (const FARPGQuestRuntime& Q : Quests) if (Q.QuestId == QuestId && Q.State == EARPGQuestState::Completed) return true;
    return false;
}

bool UARPGQuestComponent::HasActiveQuest(FName QuestId) const
{
    for (const FARPGQuestRuntime& Q : Quests) if (Q.QuestId == QuestId && (Q.State == EARPGQuestState::Active || Q.State == EARPGQuestState::ObjectivesComplete)) return true;
    return false;
}

void UARPGQuestComponent::ReplaceQuests(const TArray<FARPGQuestRuntime>& NewQuests)
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;
    Quests = NewQuests; OnRep_Quests();
}

void UARPGQuestComponent::OnRep_Quests() { OnQuestChanged.Broadcast(NAME_None); }
void UARPGQuestComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps); DOREPLIFETIME(UARPGQuestComponent, Quests);
}
