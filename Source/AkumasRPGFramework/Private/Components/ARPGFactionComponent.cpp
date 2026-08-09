#include "Components/ARPGFactionComponent.h"
#include "Data/ARPGFactionDefinition.h"
#include "Components/ARPGEventRouterComponent.h"
#include "Net/UnrealNetwork.h"

UARPGFactionComponent::UARPGFactionComponent() { SetIsReplicatedByDefault(true); }

FName UARPGFactionComponent::GetPrimaryFactionId() const
{
    return PrimaryFaction ? PrimaryFaction->DefinitionId : ExplicitFactionId;
}

void UARPGFactionComponent::SetPrimaryFaction(UARPGFactionDefinition* NewFaction)
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;
    PrimaryFaction = NewFaction;
    if (NewFaction) ExplicitFactionId = NewFaction->DefinitionId;
    OnPrimaryFactionChanged.Broadcast(GetPrimaryFactionId());
}

void UARPGFactionComponent::SetPrimaryFactionId(FName NewFactionId)
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;
    ExplicitFactionId = NewFactionId;
    if (PrimaryFaction && PrimaryFaction->DefinitionId != NewFactionId) PrimaryFaction = nullptr;
    OnPrimaryFactionChanged.Broadcast(GetPrimaryFactionId());
}

int32 UARPGFactionComponent::GetReputation(FName FactionId) const
{
    for (const FARPGFactionStanding& Standing : Reputation) if (Standing.FactionId == FactionId) return Standing.Reputation;
    return 0;
}

void UARPGFactionComponent::AddReputation(FName FactionId, int32 Delta)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || FactionId.IsNone() || Delta == 0) return;
    SetReputation(FactionId, GetReputation(FactionId) + Delta);
}

void UARPGFactionComponent::SetReputation(FName FactionId, int32 Value)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || FactionId.IsNone()) return;
    for (FARPGFactionStanding& Standing : Reputation)
    {
        if (Standing.FactionId == FactionId)
        {
            Standing.Reputation = Value; OnReputationChanged.Broadcast(FactionId, Value);
            if (UARPGEventRouterComponent* Router = GetOwner()->FindComponentByClass<UARPGEventRouterComponent>()) Router->ReportReputationChanged(FactionId, Value);
            return;
        }
    }
    FARPGFactionStanding NewStanding; NewStanding.FactionId = FactionId; NewStanding.Reputation = Value; Reputation.Add(NewStanding); OnReputationChanged.Broadcast(FactionId, Value);
    if (UARPGEventRouterComponent* Router = GetOwner()->FindComponentByClass<UARPGEventRouterComponent>()) Router->ReportReputationChanged(FactionId, Value);
}

EARPGFactionDisposition UARPGFactionComponent::GetDisposition(const UARPGFactionDefinition* Faction) const
{
    if (!Faction) return EARPGFactionDisposition::Neutral;
    const int32 Value = GetReputation(Faction->DefinitionId);
    if (Value <= Faction->HatedThreshold) return EARPGFactionDisposition::Hated;
    if (Value <= Faction->HostileThreshold) return EARPGFactionDisposition::Hostile;
    if (Value <= Faction->UnfriendlyThreshold) return EARPGFactionDisposition::Unfriendly;
    if (Value >= Faction->ExaltedThreshold) return EARPGFactionDisposition::Exalted;
    if (Value >= Faction->ReveredThreshold) return EARPGFactionDisposition::Revered;
    if (Value >= Faction->HonoredThreshold) return EARPGFactionDisposition::Honored;
    if (Value >= Faction->FriendlyThreshold) return EARPGFactionDisposition::Friendly;
    return EARPGFactionDisposition::Neutral;
}

int32 UARPGFactionComponent::GetBaseRelationshipToFactionId(FName OtherFactionId) const
{
    const FName Mine = GetPrimaryFactionId();
    if (Mine.IsNone() || OtherFactionId.IsNone()) return 0;
    if (Mine == OtherFactionId) return 100;
    if (PrimaryFaction)
        for (const FARPGFactionRelationship& Rel : PrimaryFaction->Relationships)
            if (Rel.OtherFactionId == OtherFactionId) return Rel.BaseRelationship;
    return 0;
}

int32 UARPGFactionComponent::GetBaseRelationshipTo(const UARPGFactionComponent* Other) const
{
    if (!Other) return 0;
    const FName OtherId = Other->GetPrimaryFactionId();
    const int32 Direct = GetBaseRelationshipToFactionId(OtherId);
    if (Direct != 0 || GetPrimaryFactionId() == OtherId) return Direct;
    // Fallback to the other faction's authored relationship when this side uses only an explicit ID.
    const int32 Reverse = Other->GetBaseRelationshipToFactionId(GetPrimaryFactionId());
    return Reverse;
}

bool UARPGFactionComponent::IsHostileTo(const UARPGFactionComponent* Other) const { return GetBaseRelationshipTo(Other) < 0; }
bool UARPGFactionComponent::IsFriendlyTo(const UARPGFactionComponent* Other) const { return GetBaseRelationshipTo(Other) > 0; }

void UARPGFactionComponent::ReplaceReputation(const TArray<FARPGFactionStanding>& NewReputation)
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;
    Reputation = NewReputation;
}

void UARPGFactionComponent::OnRep_PrimaryFaction() { OnPrimaryFactionChanged.Broadcast(GetPrimaryFactionId()); }

void UARPGFactionComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(UARPGFactionComponent, PrimaryFaction);
    DOREPLIFETIME(UARPGFactionComponent, ExplicitFactionId);
    DOREPLIFETIME(UARPGFactionComponent, SecondaryFactions);
    DOREPLIFETIME(UARPGFactionComponent, Reputation);
}
