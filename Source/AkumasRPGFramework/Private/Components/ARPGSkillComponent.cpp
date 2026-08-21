#include "Components/ARPGSkillComponent.h"
#include "Components/ARPGEventRouterComponent.h"
#include "Net/UnrealNetwork.h"

UARPGSkillComponent::UARPGSkillComponent()
{
    SetIsReplicatedByDefault(true);
}

int64 UARPGSkillComponent::GetXPForNextLevel(int32 InLevel) const
{
    const double L = static_cast<double>(FMath::Max(1, InLevel));
    return FMath::Max<int64>(1, FMath::RoundToInt64(50.0 * FMath::Pow(L, 1.65)));
}

int64 UARPGSkillComponent::GetRuneScapeStyleTotalXPForLevel(int32 Level) const
{
    // RuneScape's familiar 1-99 progression is defined as a cumulative threshold. FARPGSkillState stores
    // XP inside the current level, so GetXPForNextLevelForModel converts consecutive cumulative thresholds
    // into the exact per-level delta while preserving the framework's existing compact save representation.
    const int32 TargetLevel = FMath::Clamp(Level, 1, 126);
    int64 Points = 0;
    for (int32 L = 1; L < TargetLevel; ++L)
    {
        const double Term = static_cast<double>(L) + 300.0 * FMath::Pow(2.0, static_cast<double>(L) / 7.0);
        Points += static_cast<int64>(Term); // Term is positive; truncation is the required floor operation.
    }
    return FMath::Max<int64>(0, Points / 4);
}

int64 UARPGSkillComponent::GetXPForNextLevelForModel(int32 Level, EARPGSkillXPModel XPModel) const
{
    const int32 CurrentLevel = FMath::Max(1, Level);
    if (XPModel == EARPGSkillXPModel::RuneScapeStyle99)
    {
        const int64 CurrentTotal = GetRuneScapeStyleTotalXPForLevel(CurrentLevel);
        const int64 NextTotal = GetRuneScapeStyleTotalXPForLevel(CurrentLevel + 1);
        return FMath::Max<int64>(1, NextTotal - CurrentTotal);
    }
    return GetXPForNextLevel(CurrentLevel);
}

int64 UARPGSkillComponent::GetXPForNextLevelFromDefinition(const UARPGSkillDefinition* Skill, int32 Level) const
{
    const EARPGSkillXPModel Model = Skill ? Skill->XPModel : EARPGSkillXPModel::FrameworkPower;
    int64 Needed = GetXPForNextLevelForModel(Level, Model);
    if (Skill)
    {
        const FRichCurve* Curve = Skill->XPRequiredPerLevel.GetRichCurveConst();
        // Existing authored curves remain authoritative regardless of the newly-added model selector.
        if (Curve && Curve->GetNumKeys() > 0)
            Needed = FMath::Max<int64>(1, FMath::RoundToInt64(Curve->Eval(static_cast<float>(Level), static_cast<float>(Needed))));
    }
    return FMath::Max<int64>(1, Needed);
}

void UARPGSkillComponent::AddSkillXP(FName SkillId, int64 Amount)
{
    AddSkillXPInternal(SkillId, Amount, DefaultMaxLevel, nullptr, EARPGSkillXPModel::FrameworkPower);
}

void UARPGSkillComponent::AddSkillXPFromDefinition(const UARPGSkillDefinition* Skill, int64 Amount)
{
    if (!Skill) return;
    AddSkillXPInternal(Skill->DefinitionId, Amount, FMath::Max(1, Skill->MaxLevel), Skill, Skill->XPModel);
}

void UARPGSkillComponent::AddSkillXPWithModel(FName SkillId, int64 Amount, int32 MaxLevel, EARPGSkillXPModel XPModel)
{
    AddSkillXPInternal(SkillId, Amount, FMath::Max(1, MaxLevel), nullptr, XPModel);
}

void UARPGSkillComponent::AddSkillXPInternal(FName SkillId, int64 Amount, int32 MaxLevel, const UARPGSkillDefinition* Definition, EARPGSkillXPModel FallbackModel)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || SkillId.IsNone() || Amount <= 0) return;
    FARPGSkillState* State = Skills.FindByPredicate([&](const FARPGSkillState& S){ return S.SkillId == SkillId; });
    if (!State)
    {
        FARPGSkillState NewState;
        NewState.SkillId = SkillId;
        Skills.Add(NewState);
        State = &Skills.Last();
    }

    const int32 OldLevel = State->Level;
    State->XP += Amount;
    while (State->Level < MaxLevel)
    {
        const int64 Needed = Definition
            ? GetXPForNextLevelFromDefinition(Definition, State->Level)
            : GetXPForNextLevelForModel(State->Level, FallbackModel);
        if (State->XP < Needed) break;
        State->XP -= Needed;
        ++State->Level;
    }
    if (State->Level >= MaxLevel) State->XP = 0;

    OnSkillChanged.Broadcast(SkillId, State->Level, State->XP);
    if (State->Level != OldLevel)
        if (UARPGEventRouterComponent* Router = GetOwner()->FindComponentByClass<UARPGEventRouterComponent>())
            Router->ReportSkillLevel(SkillId, State->Level);
}

int32 UARPGSkillComponent::GetSkillLevel(FName SkillId) const
{
    const auto* State = Skills.FindByPredicate([&](const FARPGSkillState& X){ return X.SkillId == SkillId; });
    return State ? State->Level : 1;
}

int64 UARPGSkillComponent::GetSkillXP(FName SkillId) const
{
    const auto* State = Skills.FindByPredicate([&](const FARPGSkillState& X){ return X.SkillId == SkillId; });
    return State ? State->XP : 0;
}

void UARPGSkillComponent::ReplaceSkills(const TArray<FARPGSkillState>& NewSkills)
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;
    Skills = NewSkills;
    OnRep_Skills();
}

void UARPGSkillComponent::OnRep_Skills()
{
    OnSkillChanged.Broadcast(NAME_None, 0, 0);
}

void UARPGSkillComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(UARPGSkillComponent, Skills);
}
