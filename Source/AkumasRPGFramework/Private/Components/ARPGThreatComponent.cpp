#include "Components/ARPGThreatComponent.h"

UARPGThreatComponent::UARPGThreatComponent() { PrimaryComponentTick.bCanEverTick = false; }

float UARPGThreatComponent::GetThreat(AActor* Actor) const
{
    for (const FARPGThreatEntry& E : ThreatTable) if (E.Actor == Actor) return E.Threat;
    return 0.f;
}

AActor* UARPGThreatComponent::GetHighestThreatActor() const
{
    AActor* Best = nullptr; float BestThreat = -1.f;
    for (const FARPGThreatEntry& E : ThreatTable)
        if (IsValid(E.Actor) && E.Threat > BestThreat) { BestThreat = E.Threat; Best = E.Actor; }
    return Best;
}

void UARPGThreatComponent::NotifyIfHighestChanged()
{
    AActor* Current = GetHighestThreatActor();
    if (LastHighest.Get() != Current) { LastHighest = Current; OnHighestThreatTargetChanged.Broadcast(Current); }
}

void UARPGThreatComponent::AddThreat(AActor* Actor, float Amount)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || !IsValid(Actor) || Amount == 0.f) return;
    for (FARPGThreatEntry& E : ThreatTable) if (E.Actor == Actor) { E.Threat = FMath::Max(0.f, E.Threat + Amount); NotifyIfHighestChanged(); return; }
    FARPGThreatEntry E; E.Actor = Actor; E.Threat = FMath::Max(0.f, Amount); ThreatTable.Add(E); NotifyIfHighestChanged();
}
void UARPGThreatComponent::SetThreat(AActor* Actor, float Amount)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || !IsValid(Actor)) return;
    for (FARPGThreatEntry& E : ThreatTable) if (E.Actor == Actor) { E.Threat = FMath::Max(0.f, Amount); NotifyIfHighestChanged(); return; }
    FARPGThreatEntry E; E.Actor = Actor; E.Threat = FMath::Max(0.f, Amount); ThreatTable.Add(E); NotifyIfHighestChanged();
}
void UARPGThreatComponent::Taunt(AActor* Actor, float BonusThreat)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || !IsValid(Actor)) return;
    float Highest = 0.f; for (const FARPGThreatEntry& E : ThreatTable) Highest = FMath::Max(Highest, E.Threat);
    SetThreat(Actor, Highest + FMath::Max(0.01f, BonusThreat));
}
void UARPGThreatComponent::RemoveActor(AActor* Actor) { if (GetOwner() && GetOwner()->HasAuthority()) { ThreatTable.RemoveAll([Actor](const FARPGThreatEntry& E){ return E.Actor == Actor || !IsValid(E.Actor); }); NotifyIfHighestChanged(); } }
void UARPGThreatComponent::ClearThreat() { if (GetOwner() && GetOwner()->HasAuthority()) { ThreatTable.Reset(); NotifyIfHighestChanged(); } }
