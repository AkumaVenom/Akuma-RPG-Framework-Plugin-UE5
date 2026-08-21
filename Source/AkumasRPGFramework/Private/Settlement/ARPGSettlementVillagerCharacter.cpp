#include "Settlement/ARPGSettlementVillagerCharacter.h"

#include "Components/ARPGFactionComponent.h"
#include "Components/ARPGFactionOwnershipComponent.h"
#include "Settlement/ARPGBuildBedActor.h"
#include "Settlement/ARPGSettlementHubActor.h"
#include "Settlement/ARPGSettlementResidentComponent.h"

AARPGSettlementVillagerCharacter::AARPGSettlementVillagerCharacter()
{
    SettlementResident = CreateDefaultSubobject<UARPGSettlementResidentComponent>(TEXT("SettlementResident"));
    SettlementOwnership = CreateDefaultSubobject<UARPGFactionOwnershipComponent>(TEXT("SettlementOwnership"));
}

bool AARPGSettlementVillagerCharacter::InitializeAsSettlementVillager(AARPGSettlementHubActor* Hub, AARPGBuildBedActor* Bed, FGuid ResidentId)
{
    if (!HasAuthority() || !Hub || !SettlementResident) return false;
    if (SettlementOwnership && Hub->Ownership)
        SettlementOwnership->SetOwnership(Hub->Ownership->OwnerAccountId, Hub->Ownership->OwnerCharacterId, Hub->Ownership->OwnerFactionId);
    if (Faction && Hub->Ownership && !Hub->Ownership->OwnerFactionId.IsNone())
        Faction->SetPrimaryFactionId(Hub->Ownership->OwnerFactionId);
    return SettlementResident->InitializeSettlementResident(Hub, Bed, ResidentId);
}
