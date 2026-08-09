#include "Crafting/ARPGStorageActor.h"
#include "Components/ARPGInventoryComponent.h"
#include "Components/ARPGFactionOwnershipComponent.h"
#include "Net/UnrealNetwork.h"
AARPGStorageActor::AARPGStorageActor()
{
    Inventory=CreateDefaultSubobject<UARPGInventoryComponent>(TEXT("StorageInventory")); Inventory->MaxSlots=48;
}
void AARPGStorageActor::BeginPlay(){ Super::BeginPlay(); if(HasAuthority()) EnsureContainerId(); }
void AARPGStorageActor::EnsureContainerId(){ if(HasAuthority()&&!ContainerId.IsValid()) ContainerId=FGuid::NewGuid(); }
void AARPGStorageActor::InitializeStorageOwnership(AActor* OwnerActor){ if(HasAuthority()&&Ownership) Ownership->InitializeFromActor(OwnerActor,true); }
bool AARPGStorageActor::CanActorAccess(AActor* Actor) const { return !Ownership || Ownership->CanActorUse(Actor); }
void AARPGStorageActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const { Super::GetLifetimeReplicatedProps(OutLifetimeProps); DOREPLIFETIME(AARPGStorageActor,ContainerId); }
