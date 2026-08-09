#include "Components/ARPGBattlePetBattleComponent.h"
#include "Components/ARPGBattlePetComponent.h"
#include "Components/ARPGEventRouterComponent.h"
#include "Data/ARPGBattlePetDefinition.h"
#include "Utilities/ARPGAssetLibrary.h"
#include "Net/UnrealNetwork.h"

UARPGBattlePetBattleComponent::UARPGBattlePetBattleComponent() { SetIsReplicatedByDefault(true); }

FARPGPetBattleUnit UARPGBattlePetBattleComponent::MakeUnitFromCollectionPet(const FARPGPetInstance& Pet) const
{
    FARPGPetBattleUnit U; U.PetInstanceId = Pet.InstanceId; U.SpeciesId = Pet.SpeciesId; U.Level = Pet.Level; U.Quality = Pet.Quality; U.AbilityIds = Pet.EquippedAbilityIds;
    if (const UARPGBattlePetDefinition* Def = Cast<UARPGBattlePetDefinition>(UARPGAssetLibrary::ResolveDefinitionById(UARPGBattlePetDefinition::StaticClass(), Pet.SpeciesId)))
    { U.MaxHealth = Def->BaseHealth + (Pet.Level - 1) * 5.f; U.Power = Def->BasePower + (Pet.Level - 1) * 0.75f; U.Speed = Def->BaseSpeed + (Pet.Level - 1) * 0.35f; }
    U.CurrentHealth = FMath::Clamp(Pet.CurrentHealth, 1.f, U.MaxHealth);
    return U;
}

FARPGPetBattleUnit UARPGBattlePetBattleComponent::MakeWildUnit(UARPGBattlePetDefinition* Def, int32 Level, EARPGRarity Quality) const
{
    FARPGPetBattleUnit U; if (!Def) return U;
    U.PetInstanceId = FGuid::NewGuid(); U.SpeciesId = Def->DefinitionId; U.Level = FMath::Clamp(Level, 1, Def->MaxLevel); U.Quality = Quality;
    U.MaxHealth = Def->BaseHealth + (U.Level - 1) * 5.f; U.CurrentHealth = U.MaxHealth; U.Power = Def->BasePower + (U.Level - 1) * .75f; U.Speed = Def->BaseSpeed + (U.Level - 1) * .35f;
    for (const FARPGPetAbilityDefinition& A : Def->Abilities) if (A.UnlockLevel <= U.Level && U.AbilityIds.Num() < 3) U.AbilityIds.Add(A.AbilityId);
    return U;
}

bool UARPGBattlePetBattleComponent::StartWildBattle(UARPGBattlePetDefinition* WildDefinition, int32 WildLevel, EARPGRarity Quality)
{
    if (!GetOwner() || !WildDefinition) return false;
    if (!GetOwner()->HasAuthority()) { ServerStartWildBattle(WildDefinition, WildLevel, Quality); return true; }
    return StartWildBattleAuthority(WildDefinition, WildLevel, Quality);
}

bool UARPGBattlePetBattleComponent::StartWildBattleAuthority(UARPGBattlePetDefinition* WildDefinition, int32 WildLevel, EARPGRarity Quality)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || IsBattleActive() || !WildDefinition) return false;
    const UARPGBattlePetComponent* Collection = GetOwner()->FindComponentByClass<UARPGBattlePetComponent>(); if (!Collection || Collection->ActiveTeam.Num() == 0) return false;
    PlayerTeam.Reset(); EnemyTeam.Reset();
    for (const FGuid& Id : Collection->ActiveTeam)
        if (const FARPGPetInstance* P = Collection->Pets.FindByPredicate([&](const FARPGPetInstance& X){ return X.InstanceId == Id; })) PlayerTeam.Add(MakeUnitFromCollectionPet(*P));
    if (PlayerTeam.Num() == 0) return false;
    EnemyTeam.Add(MakeWildUnit(WildDefinition, WildLevel, Quality)); ActivePlayerIndex = 0; ActiveEnemyIndex = 0; TurnNumber = 1; bWildBattle = true;
    BattleState = EARPGPetBattleState::ChoosingAction; OnBattleStateChanged.Broadcast(BattleState); return true;
}

const FARPGPetAbilityDefinition* UARPGBattlePetBattleComponent::ResolveAbility(const FARPGPetBattleUnit& Unit, FName AbilityId) const
{
    const UARPGBattlePetDefinition* Def = Cast<UARPGBattlePetDefinition>(UARPGAssetLibrary::ResolveDefinitionById(UARPGBattlePetDefinition::StaticClass(), Unit.SpeciesId));
    return Def ? Def->Abilities.FindByPredicate([&](const FARPGPetAbilityDefinition& A){ return A.AbilityId == AbilityId; }) : nullptr;
}

bool UARPGBattlePetBattleComponent::IsAbilityReady(const FARPGPetBattleUnit& Unit, FName AbilityId) const
{
    if (!Unit.AbilityIds.Contains(AbilityId)) return false;
    const FARPGPetAbilityCooldown* CD = Unit.Cooldowns.FindByPredicate([&](const FARPGPetAbilityCooldown& C){ return C.AbilityId == AbilityId; });
    return !CD || CD->RemainingTurns <= 0;
}

void UARPGBattlePetBattleComponent::SetCooldown(FARPGPetBattleUnit& Unit, FName AbilityId, int32 Turns)
{
    if (Turns <= 0) return;
    if (FARPGPetAbilityCooldown* ExistingCooldown = Unit.Cooldowns.FindByPredicate([&](const FARPGPetAbilityCooldown& C){ return C.AbilityId == AbilityId; }))
    {
        ExistingCooldown->RemainingTurns = Turns;
    }
    else
    {
        FARPGPetAbilityCooldown NewCooldown;
        NewCooldown.AbilityId = AbilityId;
        NewCooldown.RemainingTurns = Turns;
        Unit.Cooldowns.Add(NewCooldown);
    }
}

void UARPGBattlePetBattleComponent::TickCooldowns(FARPGPetBattleUnit& Unit) { for (FARPGPetAbilityCooldown& C : Unit.Cooldowns) C.RemainingTurns = FMath::Max(0, C.RemainingTurns - 1); }

FName UARPGBattlePetBattleComponent::ChooseEnemyAbility() const
{
    if (!EnemyTeam.IsValidIndex(ActiveEnemyIndex)) return NAME_None;
    const FARPGPetBattleUnit& U = EnemyTeam[ActiveEnemyIndex];
    TArray<FName> Ready; for (FName Id : U.AbilityIds) if (IsAbilityReady(U, Id)) Ready.Add(Id);
    return Ready.Num() > 0 ? Ready[FMath::RandRange(0, Ready.Num()-1)] : NAME_None;
}

float UARPGBattlePetBattleComponent::ApplyAbility(FARPGPetBattleUnit& Source, FARPGPetBattleUnit& Target, const FARPGPetAbilityDefinition& Ability, bool bPlayerAction)
{
    if (FMath::FRand() > FMath::Clamp(Ability.Accuracy, 0.f, 1.f)) { OnActionResolved.Broadcast(bPlayerAction, Ability.AbilityId, 0.f); return 0.f; }
    const float Amount = FMath::Max(1.f, Ability.Power + Source.Power * .35f + Source.Level * .5f);
    if (Ability.bHealing || Ability.Target == EARPGPetAbilityTarget::Self)
        Source.CurrentHealth = FMath::Min(Source.MaxHealth, Source.CurrentHealth + Amount);
    else
        Target.CurrentHealth = FMath::Max(0.f, Target.CurrentHealth - Amount);
    if (Ability.AppliedStatusTag.IsValid()) (Ability.Target == EARPGPetAbilityTarget::Self ? Source : Target).StatusTags.AddTag(Ability.AppliedStatusTag);
    SetCooldown(Source, Ability.AbilityId, Ability.CooldownTurns);
    OnActionResolved.Broadcast(bPlayerAction, Ability.AbilityId, Amount); return Amount;
}

bool UARPGBattlePetBattleComponent::AdvanceIfKnockedOut(TArray<FARPGPetBattleUnit>& Team, int32& ActiveIndex)
{
    if (Team.IsValidIndex(ActiveIndex) && Team[ActiveIndex].CurrentHealth > 0.f) return true;
    for (int32 i=0;i<Team.Num();++i) if (Team[i].CurrentHealth > 0.f) { ActiveIndex=i; return true; }
    return false;
}

void UARPGBattlePetBattleComponent::ResolveTurn(FName PlayerAbilityId)
{
    if (!PlayerTeam.IsValidIndex(ActivePlayerIndex) || !EnemyTeam.IsValidIndex(ActiveEnemyIndex)) return;
    BattleState = EARPGPetBattleState::ResolvingTurn; OnBattleStateChanged.Broadcast(BattleState);
    FARPGPetBattleUnit& P = PlayerTeam[ActivePlayerIndex]; FARPGPetBattleUnit& E = EnemyTeam[ActiveEnemyIndex];
    const FARPGPetAbilityDefinition* PA = ResolveAbility(P, PlayerAbilityId); const FName EnemyId = ChooseEnemyAbility(); const FARPGPetAbilityDefinition* EA = ResolveAbility(E, EnemyId);
    const bool PlayerFirst = !EA || (PA && (PA->Priority > EA->Priority || (PA->Priority == EA->Priority && P.Speed >= E.Speed)));
    if (PlayerFirst) { if (PA && P.CurrentHealth > 0) ApplyAbility(P,E,*PA,true); if (E.CurrentHealth > 0 && EA) ApplyAbility(E,P,*EA,false); }
    else { if (EA && E.CurrentHealth > 0) ApplyAbility(E,P,*EA,false); if (P.CurrentHealth > 0 && PA) ApplyAbility(P,E,*PA,true); }
    TickCooldowns(P); TickCooldowns(E);
    const bool PlayerAlive = AdvanceIfKnockedOut(PlayerTeam, ActivePlayerIndex); const bool EnemyAlive = AdvanceIfKnockedOut(EnemyTeam, ActiveEnemyIndex);
    if (!EnemyAlive) { EndBattle(EARPGPetBattleState::Victory); return; }
    if (!PlayerAlive) { EndBattle(EARPGPetBattleState::Defeat); return; }
    ++TurnNumber; BattleState = EARPGPetBattleState::ChoosingAction; OnBattleStateChanged.Broadcast(BattleState);
}

bool UARPGBattlePetBattleComponent::ChooseAbility(FName AbilityId)
{
    if (!GetOwner()) return false; if (!GetOwner()->HasAuthority()) { ServerChooseAbility(AbilityId); return true; } return ChooseAbilityAuthority(AbilityId);
}
bool UARPGBattlePetBattleComponent::ChooseAbilityAuthority(FName AbilityId)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || BattleState != EARPGPetBattleState::ChoosingAction || !PlayerTeam.IsValidIndex(ActivePlayerIndex) || !IsAbilityReady(PlayerTeam[ActivePlayerIndex], AbilityId)) return false;
    ResolveTurn(AbilityId); return true;
}

bool UARPGBattlePetBattleComponent::SwapActivePet(int32 TeamIndex)
{
    if (!GetOwner()) return false; if (!GetOwner()->HasAuthority()) { ServerSwapActivePet(TeamIndex); return true; } return SwapActivePetAuthority(TeamIndex);
}
bool UARPGBattlePetBattleComponent::SwapActivePetAuthority(int32 TeamIndex)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || BattleState != EARPGPetBattleState::ChoosingAction || !PlayerTeam.IsValidIndex(TeamIndex) || TeamIndex == ActivePlayerIndex || PlayerTeam[TeamIndex].CurrentHealth <= 0.f) return false;
    ActivePlayerIndex = TeamIndex; ++TurnNumber; return true;
}

bool UARPGBattlePetBattleComponent::AttemptCapture(float CaptureBonus)
{
    if (!GetOwner()) return false; if (!GetOwner()->HasAuthority()) { ServerAttemptCapture(CaptureBonus); return true; } return AttemptCaptureAuthority(CaptureBonus);
}
bool UARPGBattlePetBattleComponent::AttemptCaptureAuthority(float CaptureBonus)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || !bWildBattle || BattleState != EARPGPetBattleState::ChoosingAction || !EnemyTeam.IsValidIndex(ActiveEnemyIndex)) return false;
    const FARPGPetBattleUnit& Wild = EnemyTeam[ActiveEnemyIndex];
    const float Missing = 1.f - FMath::Clamp(Wild.CurrentHealth / FMath::Max(1.f, Wild.MaxHealth), 0.f, 1.f);
    const float RarityPenalty = static_cast<float>(static_cast<uint8>(Wild.Quality)) * .05f;
    const float Chance = FMath::Clamp(.15f + Missing * .7f + CaptureBonus - RarityPenalty, .02f, .95f);
    if (FMath::FRand() <= Chance)
    {
        UARPGBattlePetDefinition* Def = Cast<UARPGBattlePetDefinition>(UARPGAssetLibrary::ResolveDefinitionById(UARPGBattlePetDefinition::StaticClass(), Wild.SpeciesId));
        if (UARPGBattlePetComponent* Collection = GetOwner()->FindComponentByClass<UARPGBattlePetComponent>())
            if (Def && Collection->CapturePet(Def, Wild.Level, Wild.Quality))
            {
                if (UARPGEventRouterComponent* Events = GetOwner()->FindComponentByClass<UARPGEventRouterComponent>()) Events->ReportPetCaptured(Wild.SpeciesId);
                EndBattle(EARPGPetBattleState::Captured); return true;
            }
    }
    const FName EnemyAbility = ChooseEnemyAbility();
    if (EnemyTeam.IsValidIndex(ActiveEnemyIndex) && PlayerTeam.IsValidIndex(ActivePlayerIndex))
        if (const FARPGPetAbilityDefinition* EA = ResolveAbility(EnemyTeam[ActiveEnemyIndex], EnemyAbility)) ApplyAbility(EnemyTeam[ActiveEnemyIndex], PlayerTeam[ActivePlayerIndex], *EA, false);
    if (!AdvanceIfKnockedOut(PlayerTeam, ActivePlayerIndex)) { EndBattle(EARPGPetBattleState::Defeat); return false; }
    ++TurnNumber; return false;
}

void UARPGBattlePetBattleComponent::EndBattle(EARPGPetBattleState Result)
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;
    if (Result == EARPGPetBattleState::Victory)
    {
        if (UARPGBattlePetComponent* Collection = GetOwner()->FindComponentByClass<UARPGBattlePetComponent>())
            if (PlayerTeam.IsValidIndex(ActivePlayerIndex)) Collection->AddPetXP(PlayerTeam[ActivePlayerIndex].PetInstanceId, 25LL * FMath::Max(1, EnemyTeam.IsValidIndex(ActiveEnemyIndex) ? EnemyTeam[ActiveEnemyIndex].Level : 1));
    }
    BattleState = Result; OnBattleStateChanged.Broadcast(BattleState);
}

void UARPGBattlePetBattleComponent::ServerStartWildBattle_Implementation(UARPGBattlePetDefinition* D, int32 L, EARPGRarity Q){ StartWildBattleAuthority(D,L,Q); }
void UARPGBattlePetBattleComponent::ServerChooseAbility_Implementation(FName A){ ChooseAbilityAuthority(A); }
void UARPGBattlePetBattleComponent::ServerSwapActivePet_Implementation(int32 I){ SwapActivePetAuthority(I); }
void UARPGBattlePetBattleComponent::ServerAttemptCapture_Implementation(float B){ AttemptCaptureAuthority(B); }
void UARPGBattlePetBattleComponent::OnRep_BattleState(EARPGPetBattleState OldState){ OnBattleStateChanged.Broadcast(BattleState); }
void UARPGBattlePetBattleComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(UARPGBattlePetBattleComponent, BattleState); DOREPLIFETIME(UARPGBattlePetBattleComponent, PlayerTeam); DOREPLIFETIME(UARPGBattlePetBattleComponent, EnemyTeam);
    DOREPLIFETIME(UARPGBattlePetBattleComponent, ActivePlayerIndex); DOREPLIFETIME(UARPGBattlePetBattleComponent, ActiveEnemyIndex); DOREPLIFETIME(UARPGBattlePetBattleComponent, TurnNumber); DOREPLIFETIME(UARPGBattlePetBattleComponent, bWildBattle);
}
