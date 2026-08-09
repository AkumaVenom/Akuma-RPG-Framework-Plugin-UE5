#include "Mounts/ARPGMountComponent.h"
#include "Mounts/ARPGMountCharacter.h"
#include "Data/ARPGMountDefinition.h"
#include "Components/ARPGEventRouterComponent.h"
#include "GameFramework/Character.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

UARPGMountComponent::UARPGMountComponent() { SetIsReplicatedByDefault(true); }

bool UARPGMountComponent::UnlockMount(UARPGMountDefinition* Definition)
{
    if (!GetOwner() || !Definition || Definition->DefinitionId.IsNone()) return false;
    if (!GetOwner()->HasAuthority()) { ServerUnlockMount(Definition); return true; }
    return UnlockMountAuthority(Definition);
}

bool UARPGMountComponent::UnlockMountAuthority(UARPGMountDefinition* Definition)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || !Definition || Definition->DefinitionId.IsNone()) return false;
    const int32 Before = UnlockedMountIds.Num();
    UnlockedMountIds.AddUnique(Definition->DefinitionId);
    if (UnlockedMountIds.Num() != Before)
    {
        OnMountCollectionChanged.Broadcast(Definition->DefinitionId);
        if (UARPGEventRouterComponent* Router = GetOwner()->FindComponentByClass<UARPGEventRouterComponent>()) Router->ReportMountUnlocked(Definition->DefinitionId);
    }
    return true;
}

bool UARPGMountComponent::SummonAndRide(UARPGMountDefinition* Definition)
{
    if (!GetOwner() || !Definition) return false;
    if (!GetOwner()->HasAuthority()) { ServerSummonAndRide(Definition); return true; }
    return SummonAndRideAuthority(Definition);
}

bool UARPGMountComponent::SummonAndRideAuthority(UARPGMountDefinition* Definition)
{
    ACharacter* Rider = Cast<ACharacter>(GetOwner());
    if (!Rider || !Rider->HasAuthority() || !Definition || !IsMountUnlocked(Definition->DefinitionId) || !GetWorld())
    {
        OnMountSummonResult.Broadcast(false, nullptr);
        return false;
    }
    UClass* Loaded = Definition->MountPawnClass.LoadSynchronous();
    if (!Loaded || !Loaded->IsChildOf(AARPGMountCharacter::StaticClass()))
    {
        OnMountSummonResult.Broadcast(false, nullptr);
        return false;
    }
    const FVector Location = Rider->GetActorLocation() + Rider->GetActorForwardVector() * SummonForwardOffset;
    FActorSpawnParameters Params;
    Params.Owner = Rider->GetController();
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
    AARPGMountCharacter* Mount = GetWorld()->SpawnActor<AARPGMountCharacter>(Loaded, Location, Rider->GetActorRotation(), Params);
    if (!Mount || !Mount->MountRider(Rider, Definition))
    {
        if (Mount) Mount->Destroy();
        OnMountSummonResult.Broadcast(false, nullptr);
        return false;
    }
    ActiveMountId = Definition->DefinitionId;
    OnMountSummonResult.Broadcast(true, Mount);
    return true;
}

FARPGMountSaveState UARPGMountComponent::MakeMountSaveState() const
{
    FARPGMountSaveState State;
    State.UnlockedMountIds = UnlockedMountIds;
    State.ActiveMountId = ActiveMountId;
    return State;
}

void UARPGMountComponent::ReplaceMountState(const FARPGMountSaveState& State)
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;
    UnlockedMountIds = State.UnlockedMountIds;
    // Never auto-spawn a ridden mount while restoring; this is the last selected mount.
    ActiveMountId = State.ActiveMountId;
    OnRep_MountState();
}

void UARPGMountComponent::ServerUnlockMount_Implementation(UARPGMountDefinition* Definition) { UnlockMountAuthority(Definition); }
void UARPGMountComponent::ServerSummonAndRide_Implementation(UARPGMountDefinition* Definition) { SummonAndRideAuthority(Definition); }
void UARPGMountComponent::OnRep_MountState() { OnMountCollectionChanged.Broadcast(ActiveMountId); }

void UARPGMountComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(UARPGMountComponent, UnlockedMountIds);
    DOREPLIFETIME(UARPGMountComponent, ActiveMountId);
}
