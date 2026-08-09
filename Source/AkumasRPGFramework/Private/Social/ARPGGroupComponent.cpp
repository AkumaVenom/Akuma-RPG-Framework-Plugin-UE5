#include "Social/ARPGGroupComponent.h"
#include "Net/UnrealNetwork.h"

UARPGGroupComponent::UARPGGroupComponent() { SetIsReplicatedByDefault(true); }
void UARPGGroupComponent::SetMembership(const FARPGGroupMembership& NewMembership) { if (GetOwner() && GetOwner()->HasAuthority()) { Membership = NewMembership; OnGroupChanged.Broadcast(); } }
void UARPGGroupComponent::ClearGroup() { if (GetOwner() && GetOwner()->HasAuthority()) { Membership = FARPGGroupMembership(); OnGroupChanged.Broadcast(); } }
void UARPGGroupComponent::SetGuild(FName NewGuildId) { if (GetOwner() && GetOwner()->HasAuthority()) { GuildId = NewGuildId; OnGroupChanged.Broadcast(); } }
bool UARPGGroupComponent::IsInSameGroup(const UARPGGroupComponent* Other, bool bRequireRaidMatch) const
{
    if (!Other || !Membership.GroupId.IsValid() || Membership.GroupId != Other->Membership.GroupId) return false;
    return !bRequireRaidMatch || (Membership.GroupType == EARPGGroupType::Raid && Other->Membership.GroupType == EARPGGroupType::Raid);
}
void UARPGGroupComponent::OnRep_Group() { OnGroupChanged.Broadcast(); }
void UARPGGroupComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const { Super::GetLifetimeReplicatedProps(OutLifetimeProps); DOREPLIFETIME(UARPGGroupComponent, Membership); DOREPLIFETIME(UARPGGroupComponent, GuildId); }
