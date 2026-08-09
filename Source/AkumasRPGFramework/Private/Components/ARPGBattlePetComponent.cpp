#include "Components/ARPGBattlePetComponent.h"
#include "Data/ARPGBattlePetDefinition.h"
#include "Net/UnrealNetwork.h"

UARPGBattlePetComponent::UARPGBattlePetComponent() { SetIsReplicatedByDefault(true); }

bool UARPGBattlePetComponent::CapturePet(const UARPGBattlePetDefinition* Definition, int32 Level, EARPGRarity Quality)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || !Definition || Definition->DefinitionId.IsNone()) return false;
    FARPGPetInstance Pet; Pet.InstanceId = FGuid::NewGuid(); Pet.SpeciesId = Definition->DefinitionId; Pet.Level = FMath::Clamp(Level, 1, Definition->MaxLevel);
    Pet.Quality = Quality; Pet.CurrentHealth = Definition->BaseHealth + (Pet.Level - 1) * 5.f;
    for (const FARPGPetAbilityDefinition& Ability : Definition->Abilities) if (Ability.UnlockLevel <= Pet.Level && Pet.EquippedAbilityIds.Num() < 3) Pet.EquippedAbilityIds.Add(Ability.AbilityId);
    Pets.Add(Pet); OnPetCaptured.Broadcast(Pet.SpeciesId, Pet.InstanceId); OnCollectionChanged.Broadcast(); return true;
}

bool UARPGBattlePetComponent::SetActiveTeam(const TArray<FGuid>& PetIds)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || PetIds.Num() > MaxTeamSize) return false;
    TSet<FGuid> Unique;
    for (const FGuid& Id : PetIds)
    {
        if (Unique.Contains(Id) || !Pets.ContainsByPredicate([&](const FARPGPetInstance& P){ return P.InstanceId == Id; })) return false;
        Unique.Add(Id);
    }
    ActiveTeam = PetIds; OnCollectionChanged.Broadcast(); return true;
}

bool UARPGBattlePetComponent::OwnsSpecies(FName SpeciesId) const
{
    return Pets.ContainsByPredicate([&](const FARPGPetInstance& P){ return P.SpeciesId == SpeciesId; });
}

void UARPGBattlePetComponent::AddPetXP(FGuid PetId, int64 XPAmount)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || XPAmount <= 0) return;
    for (FARPGPetInstance& Pet : Pets)
    {
        if (Pet.InstanceId != PetId) continue;
        Pet.XP += XPAmount;
        while (Pet.Level < 25)
        {
            const int64 Needed = 50LL * Pet.Level * Pet.Level;
            if (Pet.XP < Needed) break;
            Pet.XP -= Needed; ++Pet.Level;
        }
        OnCollectionChanged.Broadcast(); return;
    }
}

void UARPGBattlePetComponent::ReplacePetState(const TArray<FARPGPetInstance>& NewPets, const TArray<FGuid>& NewTeam)
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return; Pets = NewPets; ActiveTeam = NewTeam; OnCollectionChanged.Broadcast();
}

void UARPGBattlePetComponent::OnRep_Pets() { OnCollectionChanged.Broadcast(); }
void UARPGBattlePetComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps); DOREPLIFETIME(UARPGBattlePetComponent, Pets); DOREPLIFETIME(UARPGBattlePetComponent, ActiveTeam);
}
