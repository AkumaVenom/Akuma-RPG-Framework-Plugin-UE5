#include "Building/ARPGFactionTerritoryVolume.h"
#include "Components/ARPGFactionComponent.h"

bool AARPGFactionTerritoryVolume::CanActorBuildHere(const AActor* Actor) const
{
    if (OwnerFactionId.IsNone()) return true;
    const UARPGFactionComponent* F = Actor ? Actor->FindComponentByClass<UARPGFactionComponent>() : nullptr;
    if (!F) return bAllowNeutral;
    if (F->GetPrimaryFactionId() == OwnerFactionId) return bAllowOwnerFaction;
    if (F->GetReputation(OwnerFactionId) < MinimumReputationToBuild) return false;
    const int32 Rel = F->GetBaseRelationshipToFactionId(OwnerFactionId);
    if (Rel > 0) return bAllowAllies;
    if (Rel < 0) return bAllowHostile;
    return bAllowNeutral;
}
