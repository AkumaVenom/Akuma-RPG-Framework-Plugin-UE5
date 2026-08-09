#include "Components/ARPGSkillComponent.h"
#include "Data/ARPGSkillDefinition.h"
#include "Components/ARPGEventRouterComponent.h"
#include "Net/UnrealNetwork.h"

UARPGSkillComponent::UARPGSkillComponent() { SetIsReplicatedByDefault(true); }
int64 UARPGSkillComponent::GetXPForNextLevel(int32 InLevel) const
{
    const double L = static_cast<double>(FMath::Max(1, InLevel));
    return FMath::Max<int64>(1, FMath::RoundToInt64(50.0 * FMath::Pow(L, 1.65)));
}
void UARPGSkillComponent::AddSkillXP(FName SkillId, int64 Amount){ AddSkillXPInternal(SkillId,Amount,DefaultMaxLevel,nullptr); }
void UARPGSkillComponent::AddSkillXPFromDefinition(const UARPGSkillDefinition* Skill, int64 Amount)
{
    if(!Skill)return; AddSkillXPInternal(Skill->DefinitionId,Amount,FMath::Max(1,Skill->MaxLevel),Skill);
}
void UARPGSkillComponent::AddSkillXPInternal(FName SkillId,int64 Amount,int32 MaxLevel,const UARPGSkillDefinition* Definition)
{
    if(!GetOwner()||!GetOwner()->HasAuthority()||SkillId.IsNone()||Amount<=0)return;
    FARPGSkillState* State=Skills.FindByPredicate([&](const FARPGSkillState&S){return S.SkillId==SkillId;});
    if(!State){FARPGSkillState NewState;NewState.SkillId=SkillId;Skills.Add(NewState);State=&Skills.Last();}
    const int32 OldLevel=State->Level; State->XP+=Amount;
    while(State->Level<MaxLevel)
    {
        int64 Needed=GetXPForNextLevel(State->Level);
        if(Definition)
        {
            const FRichCurve* Curve=Definition->XPRequiredPerLevel.GetRichCurveConst();
            if(Curve && Curve->GetNumKeys()>0) Needed=FMath::Max<int64>(1,FMath::RoundToInt64(Curve->Eval(static_cast<float>(State->Level),static_cast<float>(Needed))));
        }
        if(State->XP<Needed)break; State->XP-=Needed; ++State->Level;
    }
    if(State->Level>=MaxLevel)State->XP=0;
    OnSkillChanged.Broadcast(SkillId,State->Level,State->XP);
    if(State->Level!=OldLevel) if(UARPGEventRouterComponent* R=GetOwner()->FindComponentByClass<UARPGEventRouterComponent>())R->ReportSkillLevel(SkillId,State->Level);
}
int32 UARPGSkillComponent::GetSkillLevel(FName SkillId)const{const auto*S=Skills.FindByPredicate([&](const FARPGSkillState&X){return X.SkillId==SkillId;});return S?S->Level:1;}
int64 UARPGSkillComponent::GetSkillXP(FName SkillId)const{const auto*S=Skills.FindByPredicate([&](const FARPGSkillState&X){return X.SkillId==SkillId;});return S?S->XP:0;}
void UARPGSkillComponent::ReplaceSkills(const TArray<FARPGSkillState>& NewSkills){if(!GetOwner()||!GetOwner()->HasAuthority())return;Skills=NewSkills;OnRep_Skills();}
void UARPGSkillComponent::OnRep_Skills(){OnSkillChanged.Broadcast(NAME_None,0,0);}
void UARPGSkillComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps)const{Super::GetLifetimeReplicatedProps(OutLifetimeProps);DOREPLIFETIME(UARPGSkillComponent,Skills);}
