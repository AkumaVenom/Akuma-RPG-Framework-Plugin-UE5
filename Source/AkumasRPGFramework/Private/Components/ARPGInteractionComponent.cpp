#include "Components/ARPGInteractionComponent.h"
#include "Components/ARPGVendorComponent.h"
#include "Components/ARPGInventoryComponent.h"
#include "Crafting/ARPGStorageActor.h"
#include "Building/ARPGBuildDoorActor.h"
#include "Building/ARPGBuildPieceActor.h"
#include "Crafting/ARPGCraftingStationActor.h"
#include "Data/ARPGRecipeDefinition.h"
#include "Data/ARPGQuestDefinition.h"
#include "Components/ARPGQuestGiverComponent.h"
#include "Components/ARPGSlayerMasterComponent.h"

UARPGInteractionComponent::UARPGInteractionComponent() { SetIsReplicatedByDefault(true); }

bool UARPGInteractionComponent::IsActorInRange(const AActor* Target) const
{
    return GetOwner() && Target && FVector::DistSquared(GetOwner()->GetActorLocation(), Target->GetActorLocation()) <= FMath::Square(MaxInteractionDistance);
}

void UARPGInteractionComponent::BuyFromVendor(UARPGVendorComponent* Vendor, FName ItemId, int32 Quantity)
{
    if (!GetOwner() || !Vendor) return;
    if (GetOwner()->HasAuthority()) ServerBuyFromVendor_Implementation(Vendor, ItemId, Quantity); else ServerBuyFromVendor(Vendor, ItemId, Quantity);
}

void UARPGInteractionComponent::SellToVendor(UARPGVendorComponent* Vendor, FName ItemId, int32 Quantity)
{
    if (!GetOwner() || !Vendor) return;
    if (GetOwner()->HasAuthority()) ServerSellToVendor_Implementation(Vendor, ItemId, Quantity); else ServerSellToVendor(Vendor, ItemId, Quantity);
}

void UARPGInteractionComponent::BuybackFromVendor(UARPGVendorComponent* Vendor, FGuid BuybackId)
{
    if (!GetOwner() || !Vendor || !BuybackId.IsValid()) return;
    if (GetOwner()->HasAuthority()) ServerBuybackFromVendor_Implementation(Vendor, BuybackId); else ServerBuybackFromVendor(Vendor, BuybackId);
}

void UARPGInteractionComponent::DepositToStorage(AARPGStorageActor* Storage, FName ItemId, int32 Quantity)
{
    if (!GetOwner() || !Storage) return;
    if (GetOwner()->HasAuthority()) ServerDepositToStorage_Implementation(Storage, ItemId, Quantity); else ServerDepositToStorage(Storage, ItemId, Quantity);
}

void UARPGInteractionComponent::WithdrawFromStorage(AARPGStorageActor* Storage, FName ItemId, int32 Quantity)
{
    if (!GetOwner() || !Storage) return;
    if (GetOwner()->HasAuthority()) ServerWithdrawFromStorage_Implementation(Storage, ItemId, Quantity); else ServerWithdrawFromStorage(Storage, ItemId, Quantity);
}

void UARPGInteractionComponent::WithdrawStationOutput(AARPGCraftingStationActor* Station, FName ItemId, int32 Quantity)
{
    if (!GetOwner() || !Station) return;
    if (GetOwner()->HasAuthority()) ServerWithdrawStationOutput_Implementation(Station, ItemId, Quantity); else ServerWithdrawStationOutput(Station, ItemId, Quantity);
}

void UARPGInteractionComponent::DepositToStorageInstance(AARPGStorageActor* Storage, FGuid InstanceId, int32 Quantity)
{
    if (!GetOwner() || !Storage || !InstanceId.IsValid()) return;
    if (GetOwner()->HasAuthority()) ServerDepositToStorageInstance_Implementation(Storage, InstanceId, Quantity); else ServerDepositToStorageInstance(Storage, InstanceId, Quantity);
}

void UARPGInteractionComponent::WithdrawFromStorageInstance(AARPGStorageActor* Storage, FGuid InstanceId, int32 Quantity)
{
    if (!GetOwner() || !Storage || !InstanceId.IsValid()) return;
    if (GetOwner()->HasAuthority()) ServerWithdrawFromStorageInstance_Implementation(Storage, InstanceId, Quantity); else ServerWithdrawFromStorageInstance(Storage, InstanceId, Quantity);
}

void UARPGInteractionComponent::WithdrawStationOutputInstance(AARPGCraftingStationActor* Station, FGuid InstanceId, int32 Quantity)
{
    if (!GetOwner() || !Station || !InstanceId.IsValid()) return;
    if (GetOwner()->HasAuthority()) ServerWithdrawStationOutputInstance_Implementation(Station, InstanceId, Quantity); else ServerWithdrawStationOutputInstance(Station, InstanceId, Quantity);
}

void UARPGInteractionComponent::ToggleBuiltDoor(AARPGBuildDoorActor* Door)
{
    if (!GetOwner() || !Door) return;
    if (GetOwner()->HasAuthority()) ServerToggleBuiltDoor_Implementation(Door); else ServerToggleBuiltDoor(Door);
}

void UARPGInteractionComponent::DemolishBuilding(AARPGBuildPieceActor* Building)
{
    if (!GetOwner() || !Building) return;
    if (GetOwner()->HasAuthority()) ServerDemolishBuilding_Implementation(Building); else ServerDemolishBuilding(Building);
}

void UARPGInteractionComponent::QueueCraft(AARPGCraftingStationActor* Station, UARPGRecipeDefinition* Recipe, int32 Count)
{
    if (!GetOwner() || !Station || !Recipe) return;
    if (GetOwner()->HasAuthority()) ServerQueueCraft_Implementation(Station, Recipe, Count); else ServerQueueCraft(Station, Recipe, Count);
}


void UARPGInteractionComponent::AcceptQuest(UARPGQuestGiverComponent* Giver, UARPGQuestDefinition* Quest)
{
    if (!GetOwner() || !Giver || !Quest) return;
    if (GetOwner()->HasAuthority()) ServerAcceptQuest_Implementation(Giver, Quest); else ServerAcceptQuest(Giver, Quest);
}

void UARPGInteractionComponent::TurnInQuest(UARPGQuestGiverComponent* Giver, UARPGQuestDefinition* Quest)
{
    if (!GetOwner() || !Giver || !Quest) return;
    if (GetOwner()->HasAuthority()) ServerTurnInQuest_Implementation(Giver, Quest); else ServerTurnInQuest(Giver, Quest);
}

void UARPGInteractionComponent::RequestSlayerTask(UARPGSlayerMasterComponent* Master)
{
    if (!GetOwner() || !Master) return;
    if (GetOwner()->HasAuthority()) ServerRequestSlayerTask_Implementation(Master); else ServerRequestSlayerTask(Master);
}

void UARPGInteractionComponent::CancelSlayerTask(UARPGSlayerMasterComponent* Master)
{
    if (!GetOwner() || !Master) return;
    if (GetOwner()->HasAuthority()) ServerCancelSlayerTask_Implementation(Master); else ServerCancelSlayerTask(Master);
}

void UARPGInteractionComponent::ServerBuyFromVendor_Implementation(UARPGVendorComponent* Vendor, FName ItemId, int32 Quantity)
{
    const bool bSuccess = Vendor && IsActorInRange(Vendor->GetOwner()) && Vendor->Purchase(GetOwner(), ItemId, Quantity);
    ClientInteractionResult(bSuccess, FText::FromString(bSuccess ? TEXT("Purchase complete.") : TEXT("Purchase failed.")));
}

void UARPGInteractionComponent::ServerSellToVendor_Implementation(UARPGVendorComponent* Vendor, FName ItemId, int32 Quantity)
{
    const bool bSuccess = Vendor && IsActorInRange(Vendor->GetOwner()) && Vendor->SellToVendor(GetOwner(), ItemId, Quantity);
    ClientInteractionResult(bSuccess, FText::FromString(bSuccess ? TEXT("Sale complete.") : TEXT("Sale failed.")));
}

void UARPGInteractionComponent::ServerBuybackFromVendor_Implementation(UARPGVendorComponent* Vendor, FGuid BuybackId)
{
    const bool bSuccess = Vendor && IsActorInRange(Vendor->GetOwner()) && Vendor->BuybackFromVendor(GetOwner(), BuybackId);
    ClientInteractionResult(bSuccess, FText::FromString(bSuccess ? TEXT("Buyback complete.") : TEXT("Buyback failed.")));
}

void UARPGInteractionComponent::ServerDepositToStorage_Implementation(AARPGStorageActor* Storage, FName ItemId, int32 Quantity)
{
    UARPGInventoryComponent* PlayerInventory = GetOwner() ? GetOwner()->FindComponentByClass<UARPGInventoryComponent>() : nullptr;
    const bool bSuccess = Storage && IsActorInRange(Storage) && Storage->CanActorAccess(GetOwner()) && PlayerInventory && Storage->Inventory && PlayerInventory->TransferItemTo(Storage->Inventory, ItemId, Quantity);
    ClientInteractionResult(bSuccess, FText::FromString(bSuccess ? TEXT("Item stored.") : TEXT("Could not store item.")));
}

void UARPGInteractionComponent::ServerWithdrawFromStorage_Implementation(AARPGStorageActor* Storage, FName ItemId, int32 Quantity)
{
    UARPGInventoryComponent* PlayerInventory = GetOwner() ? GetOwner()->FindComponentByClass<UARPGInventoryComponent>() : nullptr;
    const bool bSuccess = Storage && IsActorInRange(Storage) && Storage->CanActorAccess(GetOwner()) && PlayerInventory && Storage->Inventory && Storage->Inventory->TransferItemTo(PlayerInventory, ItemId, Quantity);
    ClientInteractionResult(bSuccess, FText::FromString(bSuccess ? TEXT("Item withdrawn.") : TEXT("Could not withdraw item.")));
}

void UARPGInteractionComponent::ServerWithdrawStationOutput_Implementation(AARPGCraftingStationActor* Station, FName ItemId, int32 Quantity)
{
    UARPGInventoryComponent* PlayerInventory = GetOwner() ? GetOwner()->FindComponentByClass<UARPGInventoryComponent>() : nullptr;
    const bool bSuccess = Station && IsActorInRange(Station) && Station->CanActorAccess(GetOwner()) && PlayerInventory && Station->OutputInventory && Station->OutputInventory->TransferItemTo(PlayerInventory, ItemId, Quantity);
    ClientInteractionResult(bSuccess, FText::FromString(bSuccess ? TEXT("Output collected.") : TEXT("Could not collect output.")));
}

void UARPGInteractionComponent::ServerDepositToStorageInstance_Implementation(AARPGStorageActor* Storage, FGuid InstanceId, int32 Quantity)
{
    UARPGInventoryComponent* PlayerInventory = GetOwner() ? GetOwner()->FindComponentByClass<UARPGInventoryComponent>() : nullptr;
    const bool bSuccess = Storage && IsActorInRange(Storage) && Storage->CanActorAccess(GetOwner()) && PlayerInventory && Storage->Inventory && PlayerInventory->TransferItemInstanceTo(Storage->Inventory, InstanceId, Quantity);
    ClientInteractionResult(bSuccess, FText::FromString(bSuccess ? TEXT("Item stored.") : TEXT("Could not store item.")));
}

void UARPGInteractionComponent::ServerWithdrawFromStorageInstance_Implementation(AARPGStorageActor* Storage, FGuid InstanceId, int32 Quantity)
{
    UARPGInventoryComponent* PlayerInventory = GetOwner() ? GetOwner()->FindComponentByClass<UARPGInventoryComponent>() : nullptr;
    const bool bSuccess = Storage && IsActorInRange(Storage) && Storage->CanActorAccess(GetOwner()) && PlayerInventory && Storage->Inventory && Storage->Inventory->TransferItemInstanceTo(PlayerInventory, InstanceId, Quantity);
    ClientInteractionResult(bSuccess, FText::FromString(bSuccess ? TEXT("Item withdrawn.") : TEXT("Could not withdraw item.")));
}

void UARPGInteractionComponent::ServerWithdrawStationOutputInstance_Implementation(AARPGCraftingStationActor* Station, FGuid InstanceId, int32 Quantity)
{
    UARPGInventoryComponent* PlayerInventory = GetOwner() ? GetOwner()->FindComponentByClass<UARPGInventoryComponent>() : nullptr;
    const bool bSuccess = Station && IsActorInRange(Station) && Station->CanActorAccess(GetOwner()) && PlayerInventory && Station->OutputInventory && Station->OutputInventory->TransferItemInstanceTo(PlayerInventory, InstanceId, Quantity);
    ClientInteractionResult(bSuccess, FText::FromString(bSuccess ? TEXT("Output collected.") : TEXT("Could not collect output.")));
}

void UARPGInteractionComponent::ServerToggleBuiltDoor_Implementation(AARPGBuildDoorActor* Door)
{
    const bool bSuccess = Door && IsActorInRange(Door) && Door->ToggleDoor(GetOwner());
    ClientInteractionResult(bSuccess, FText::FromString(bSuccess ? TEXT("Door toggled.") : TEXT("Door cannot be used.")));
}

void UARPGInteractionComponent::ServerDemolishBuilding_Implementation(AARPGBuildPieceActor* Building)
{
    const bool bSuccess = Building && IsActorInRange(Building) && Building->Demolish(GetOwner());
    ClientInteractionResult(bSuccess, FText::FromString(bSuccess ? TEXT("Building demolished.") : TEXT("Building cannot be demolished.")));
}

void UARPGInteractionComponent::ServerQueueCraft_Implementation(AARPGCraftingStationActor* Station, UARPGRecipeDefinition* Recipe, int32 Count)
{
    const bool bSuccess = Station && IsActorInRange(Station) && Station->CanActorAccess(GetOwner()) && Station->QueueRecipe(GetOwner(), Recipe, Count);
    ClientInteractionResult(bSuccess, FText::FromString(bSuccess ? TEXT("Recipe queued.") : TEXT("Could not queue recipe.")));
}


void UARPGInteractionComponent::ServerAcceptQuest_Implementation(UARPGQuestGiverComponent* Giver, UARPGQuestDefinition* Quest)
{
    const bool bSuccess = Giver && IsActorInRange(Giver->GetOwner()) && Giver->AcceptQuestFor(GetOwner(), Quest);
    ClientInteractionResult(bSuccess, FText::FromString(bSuccess ? TEXT("Quest accepted.") : TEXT("Quest is not available.")));
}

void UARPGInteractionComponent::ServerTurnInQuest_Implementation(UARPGQuestGiverComponent* Giver, UARPGQuestDefinition* Quest)
{
    const bool bSuccess = Giver && IsActorInRange(Giver->GetOwner()) && Giver->TurnInQuestFor(GetOwner(), Quest);
    ClientInteractionResult(bSuccess, FText::FromString(bSuccess ? TEXT("Quest completed.") : TEXT("Quest is not ready to turn in.")));
}

void UARPGInteractionComponent::ServerRequestSlayerTask_Implementation(UARPGSlayerMasterComponent* Master)
{
    const bool bSuccess = Master && IsActorInRange(Master->GetOwner()) && Master->RequestTaskFor(GetOwner());
    ClientInteractionResult(bSuccess, FText::FromString(bSuccess ? TEXT("Slayer assignment received.") : TEXT("No Slayer assignment is available.")));
}

void UARPGInteractionComponent::ServerCancelSlayerTask_Implementation(UARPGSlayerMasterComponent* Master)
{
    const bool bSuccess = Master && IsActorInRange(Master->GetOwner()) && Master->CancelTaskFor(GetOwner());
    ClientInteractionResult(bSuccess, FText::FromString(bSuccess ? TEXT("Slayer assignment cancelled.") : TEXT("Could not cancel Slayer assignment.")));
}

void UARPGInteractionComponent::ClientInteractionResult_Implementation(bool bSuccess, const FText& Message)
{
    OnInteractionResult.Broadcast(bSuccess, Message);
}
