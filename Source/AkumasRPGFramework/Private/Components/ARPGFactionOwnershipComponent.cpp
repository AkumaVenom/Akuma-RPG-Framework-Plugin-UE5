#include "Components/ARPGFactionOwnershipComponent.h"
#include "Components/ARPGFactionComponent.h"
#include "Actors/ARPGCharacter.h"
#include "Actors/ARPGPlayerController.h"
#include "Subsystems/ARPGAccountSubsystem.h"
#include "GameFramework/Pawn.h"
#include "Engine/GameInstance.h"
#include "Net/UnrealNetwork.h"

UARPGFactionOwnershipComponent::UARPGFactionOwnershipComponent() { SetIsReplicatedByDefault(true); }

FGuid UARPGFactionOwnershipComponent::ResolveAccountId(const AActor* Actor) const
{
    if (!Actor) return FGuid();
    const APawn* Pawn = Cast<const APawn>(Actor);
    const AARPGPlayerController* PC = Pawn ? Cast<const AARPGPlayerController>(Pawn->GetController()) : Cast<const AARPGPlayerController>(Actor);
    if (PC && PC->AccountId.IsValid()) return PC->AccountId;
    // Local-account fallback is only valid for the locally controlled player. Never apply a listen host's
    // local account identity to an unauthenticated remote pawn.
    if ((!Pawn || Pawn->IsLocallyControlled()) && GetWorld() && GetWorld()->GetGameInstance())
        if (const UARPGAccountSubsystem* Accounts = GetWorld()->GetGameInstance()->GetSubsystem<UARPGAccountSubsystem>()) return Accounts->CurrentAccountId;
    return FGuid();
}

FGuid UARPGFactionOwnershipComponent::ResolveCharacterId(const AActor* Actor) const
{
    const AARPGCharacter* Character = Cast<AARPGCharacter>(Actor);
    return Character ? Character->CharacterId : FGuid();
}

FName UARPGFactionOwnershipComponent::ResolveFactionId(const AActor* Actor) const
{
    const UARPGFactionComponent* Faction = Actor ? Actor->FindComponentByClass<UARPGFactionComponent>() : nullptr;
    return Faction ? Faction->GetPrimaryFactionId() : NAME_None;
}

int32 UARPGFactionOwnershipComponent::ResolveRelationship(const AActor* Actor) const
{
    const UARPGFactionComponent* Faction = Actor ? Actor->FindComponentByClass<UARPGFactionComponent>() : nullptr;
    if (!Faction || OwnerFactionId.IsNone()) return 0;
    return Faction->GetBaseRelationshipToFactionId(OwnerFactionId);
}

void UARPGFactionOwnershipComponent::InitializeFromActor(AActor* OwnerActor, bool bInheritFaction)
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;
    SetOwnership(ResolveAccountId(OwnerActor), ResolveCharacterId(OwnerActor), bInheritFaction ? ResolveFactionId(OwnerActor) : NAME_None);
}

void UARPGFactionOwnershipComponent::SetOwnership(FGuid AccountId, FGuid CharacterId, FName FactionId)
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;
    OwnerAccountId = AccountId;
    OwnerCharacterId = CharacterId;
    OwnerFactionId = FactionId;
    OnOwnershipChanged.Broadcast();
}

bool UARPGFactionOwnershipComponent::IsPersonalOwner(const AActor* Actor) const
{
    const FGuid Account = ResolveAccountId(Actor);
    if (OwnerAccountId.IsValid() && Account.IsValid()) return OwnerAccountId == Account;
    const FGuid Character = ResolveCharacterId(Actor);
    return OwnerCharacterId.IsValid() && Character.IsValid() && OwnerCharacterId == Character;
}

bool UARPGFactionOwnershipComponent::CanActorUse(const AActor* Actor) const
{
    if (IsPersonalOwner(Actor)) return true;
    const FName FactionId = ResolveFactionId(Actor);
    if (!OwnerFactionId.IsNone() && FactionId == OwnerFactionId) return bSameFactionCanUse;
    const int32 Rel = ResolveRelationship(Actor);
    if (Rel > 0) return bAlliesCanUse;
    if (Rel < 0) return bHostilesCanUse;
    return bNeutralCanUse;
}

bool UARPGFactionOwnershipComponent::CanActorModify(const AActor* Actor) const
{
    if (IsPersonalOwner(Actor)) return true;
    return bFactionMembersCanModify && !OwnerFactionId.IsNone() && ResolveFactionId(Actor) == OwnerFactionId;
}

bool UARPGFactionOwnershipComponent::CanActorDamage(const AActor* Actor) const
{
    if (!Actor) return false;
    if (IsPersonalOwner(Actor)) return bFriendlyFireCanDamage;
    const FName FactionId = ResolveFactionId(Actor);
    if (!OwnerFactionId.IsNone() && FactionId == OwnerFactionId) return bFriendlyFireCanDamage;
    const int32 Rel = ResolveRelationship(Actor);
    if (Rel < 0) return bHostilesCanDamage;
    return bFriendlyFireCanDamage;
}

void UARPGFactionOwnershipComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(UARPGFactionOwnershipComponent, OwnerAccountId);
    DOREPLIFETIME(UARPGFactionOwnershipComponent, OwnerCharacterId);
    DOREPLIFETIME(UARPGFactionOwnershipComponent, OwnerFactionId);
}
