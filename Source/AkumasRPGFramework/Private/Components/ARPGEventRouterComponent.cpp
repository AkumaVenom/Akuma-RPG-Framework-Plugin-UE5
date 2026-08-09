#include "Components/ARPGEventRouterComponent.h"
#include "Components/ARPGQuestComponent.h"
#include "Components/ARPGSlayerComponent.h"
#include "Components/ARPGProgressionComponent.h"
#include "Actors/ARPGGameState.h"

void UARPGEventRouterComponent::ReportKill(FName CreatureId, FGameplayTag SlayerCategory, int64 CharacterXP)
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;
    if (UARPGQuestComponent* Q = GetOwner()->FindComponentByClass<UARPGQuestComponent>())
    {
        Q->ProgressById(EARPGQuestObjectiveType::Kill, CreatureId, 1);
        if (SlayerCategory.IsValid()) Q->ProgressByTag(EARPGQuestObjectiveType::Kill, SlayerCategory, 1);
    }
    if (UARPGSlayerComponent* S = GetOwner()->FindComponentByClass<UARPGSlayerComponent>()) S->RegisterKill(SlayerCategory);
    if (CharacterXP > 0) if (UARPGProgressionComponent* P = GetOwner()->FindComponentByClass<UARPGProgressionComponent>()) P->AddXP(CharacterXP);
}
void UARPGEventRouterComponent::ReportItemLooted(FName Id,int32 N){ if(GetOwner()&&GetOwner()->HasAuthority()) if(auto*Q=GetOwner()->FindComponentByClass<UARPGQuestComponent>()) Q->ProgressById(EARPGQuestObjectiveType::Collect,Id,N); }
void UARPGEventRouterComponent::ReportPetCaptured(FName Id){ if(GetOwner()&&GetOwner()->HasAuthority()) if(auto*Q=GetOwner()->FindComponentByClass<UARPGQuestComponent>()) Q->ProgressById(EARPGQuestObjectiveType::CapturePet,Id,1); }
void UARPGEventRouterComponent::ReportPetBattleWon(FName Id){ if(GetOwner()&&GetOwner()->HasAuthority()) if(auto*Q=GetOwner()->FindComponentByClass<UARPGQuestComponent>()) { if(!Id.IsNone()) Q->ProgressById(EARPGQuestObjectiveType::WinPetBattle,Id,1); } }
void UARPGEventRouterComponent::ReportDungeonCompleted(FName Id){ if(GetOwner()&&GetOwner()->HasAuthority()) if(auto*Q=GetOwner()->FindComponentByClass<UARPGQuestComponent>()) Q->ProgressById(EARPGQuestObjectiveType::Dungeon,Id,1); }
void UARPGEventRouterComponent::ReportRaidBossDefeated(FName Id){ if(GetOwner()&&GetOwner()->HasAuthority()) if(auto*Q=GetOwner()->FindComponentByClass<UARPGQuestComponent>()) Q->ProgressById(EARPGQuestObjectiveType::RaidBoss,Id,1); }
void UARPGEventRouterComponent::ReportCrafted(FName Id,int32 N){ if(GetOwner()&&GetOwner()->HasAuthority()) if(auto*Q=GetOwner()->FindComponentByClass<UARPGQuestComponent>()) Q->ProgressById(EARPGQuestObjectiveType::Craft,Id,N); }
void UARPGEventRouterComponent::ReportBuilt(FName Id,int32 N){ if(GetOwner()&&GetOwner()->HasAuthority()) if(auto*Q=GetOwner()->FindComponentByClass<UARPGQuestComponent>()) Q->ProgressById(EARPGQuestObjectiveType::Build,Id,N); }
void UARPGEventRouterComponent::ReportSkillLevel(FName Id,int32 Level){ if(GetOwner()&&GetOwner()->HasAuthority()) if(auto*Q=GetOwner()->FindComponentByClass<UARPGQuestComponent>()) { const int32 Current=FMath::Max(0,Level); Q->SetProgressByIdAtLeast(EARPGQuestObjectiveType::Skill,Id,Current); } }
void UARPGEventRouterComponent::ReportMountUnlocked(FName Id){ if(GetOwner()&&GetOwner()->HasAuthority()) if(auto*Q=GetOwner()->FindComponentByClass<UARPGQuestComponent>()) Q->ProgressById(EARPGQuestObjectiveType::Mount,Id,1); }
void UARPGEventRouterComponent::ReportReputationChanged(FName Id,int32 NewValue){ if(GetOwner()&&GetOwner()->HasAuthority() && NewValue>0) if(auto*Q=GetOwner()->FindComponentByClass<UARPGQuestComponent>()) Q->SetProgressByIdAtLeast(EARPGQuestObjectiveType::Reputation,Id,NewValue); }
void UARPGEventRouterComponent::SendEventLogMessage(FText Message, EARPGChatChannel Channel){ if(!GetOwner()||!GetOwner()->HasAuthority()||!GetWorld())return; if(auto*GS=GetWorld()->GetGameState<AARPGGameState>()) GS->SendSystemMessage(Message,Channel); }
