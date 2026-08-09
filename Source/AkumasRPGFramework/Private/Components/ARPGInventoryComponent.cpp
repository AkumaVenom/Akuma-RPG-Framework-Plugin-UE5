#include "Components/ARPGInventoryComponent.h"
#include "Data/ARPGItemDefinition.h"
#include "Utilities/ARPGAssetLibrary.h"
#include "Net/UnrealNetwork.h"

UARPGInventoryComponent::UARPGInventoryComponent()
{
    SetIsReplicatedByDefault(true);
}

int32 UARPGInventoryComponent::ResolveMaxStack(FName ItemId) const
{
    if (const UARPGItemDefinition* Def = Cast<UARPGItemDefinition>(UARPGAssetLibrary::ResolveDefinitionById(UARPGItemDefinition::StaticClass(), ItemId)))
        return FMath::Max(1, Def->MaxStack);
    return FMath::Max(1, FallbackMaxStack);
}

bool UARPGInventoryComponent::CanAddItem(FName ItemId, int32 Quantity) const
{
    if (ItemId.IsNone() || Quantity <= 0) return false;
    int32 Remaining = Quantity;
    const int32 MaxStack = ResolveMaxStack(ItemId);
    for (const FARPGInventoryEntry& Entry : Items)
    {
        if (Entry.ItemId == ItemId && !Entry.bEquipped && Entry.Quantity < MaxStack)
            Remaining -= FMath::Min(Remaining, MaxStack - Entry.Quantity);
        if (Remaining <= 0) return true;
    }
    const int32 NeededSlots = FMath::CeilToInt(static_cast<float>(Remaining) / static_cast<float>(MaxStack));
    return Items.Num() + NeededSlots <= MaxSlots;
}

bool UARPGInventoryComponent::AddItem(FName ItemId, int32 Quantity)
{
    if (!GetOwner() || ItemId.IsNone() || Quantity <= 0) return false;
    if (!GetOwner()->HasAuthority()) return false;
    return AddItemAuthority(ItemId, Quantity);
}

bool UARPGInventoryComponent::AddItemDefinition(const UARPGItemDefinition* Item, int32 Quantity)
{
    if (!Item || Item->DefinitionId.IsNone() || Quantity <= 0) return false;
    if (!GetOwner() || !GetOwner()->HasAuthority()) return false;
    return AddItemAuthority(Item->DefinitionId, Quantity, Item->MaxStack);
}

bool UARPGInventoryComponent::AddItemAuthority(FName ItemId, int32 Quantity, int32 ExplicitMaxStack)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || ItemId.IsNone() || Quantity <= 0) return false;
    const int32 MaxStack = FMath::Max(1, ExplicitMaxStack > 0 ? ExplicitMaxStack : ResolveMaxStack(ItemId));
    int32 Remaining = Quantity;
    for (FARPGInventoryEntry& Entry : Items)
    {
        if (Entry.ItemId != ItemId || Entry.bEquipped || Entry.Quantity >= MaxStack) continue;
        const int32 Added = FMath::Min(Remaining, MaxStack - Entry.Quantity);
        Entry.Quantity += Added;
        Remaining -= Added;
        if (Remaining <= 0) { OnInventoryChanged.Broadcast(); return true; }
    }
    while (Remaining > 0 && Items.Num() < MaxSlots)
    {
        FARPGInventoryEntry Entry;
        Entry.InstanceId = FGuid::NewGuid();
        Entry.ItemId = ItemId;
        Entry.Quantity = FMath::Min(Remaining, MaxStack);
        Remaining -= Entry.Quantity;
        Items.Add(Entry);
    }
    OnInventoryChanged.Broadcast();
    return Remaining == 0;
}

bool UARPGInventoryComponent::RemoveItem(FName ItemId, int32 Quantity)
{
    if (!GetOwner() || ItemId.IsNone() || Quantity <= 0) return false;
    if (!GetOwner()->HasAuthority()) return false;
    return RemoveItemAuthority(ItemId, Quantity);
}

bool UARPGInventoryComponent::RemoveItemAuthority(FName ItemId, int32 Quantity)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || GetItemCount(ItemId) < Quantity) return false;
    int32 Remaining = Quantity;
    for (int32 Index = Items.Num() - 1; Index >= 0 && Remaining > 0; --Index)
    {
        FARPGInventoryEntry& Entry = Items[Index];
        if (Entry.ItemId != ItemId || Entry.bEquipped) continue;
        const int32 Take = FMath::Min(Remaining, Entry.Quantity);
        Entry.Quantity -= Take;
        Remaining -= Take;
        if (Entry.Quantity <= 0) Items.RemoveAt(Index);
    }
    if (Remaining > 0)
    {
        // Fallback to equipped stacks only if necessary.
        for (int32 Index = Items.Num() - 1; Index >= 0 && Remaining > 0; --Index)
        {
            FARPGInventoryEntry& Entry = Items[Index];
            if (Entry.ItemId != ItemId) continue;
            const int32 Take = FMath::Min(Remaining, Entry.Quantity);
            Entry.Quantity -= Take;
            Remaining -= Take;
            if (Entry.Quantity <= 0) Items.RemoveAt(Index);
        }
    }
    OnInventoryChanged.Broadcast();
    return Remaining == 0;
}

bool UARPGInventoryComponent::RemoveItemInstance(FGuid InstanceId, int32 Quantity)
{
    if (!GetOwner() || !InstanceId.IsValid() || Quantity <= 0) return false;
    if (!GetOwner()->HasAuthority()) return false;
    return RemoveItemInstanceAuthority(InstanceId, Quantity);
}

bool UARPGInventoryComponent::RemoveItemInstanceAuthority(FGuid InstanceId, int32 Quantity)
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return false;
    for (int32 Index = 0; Index < Items.Num(); ++Index)
    {
        FARPGInventoryEntry& Entry = Items[Index];
        if (Entry.InstanceId != InstanceId) continue;
        if (Entry.Quantity > Quantity) Entry.Quantity -= Quantity;
        else Items.RemoveAt(Index);
        OnInventoryChanged.Broadcast();
        return true;
    }
    return false;
}

bool UARPGInventoryComponent::TransferItemTo(UARPGInventoryComponent* Destination, FName ItemId, int32 Quantity)
{
    if (!GetOwner() || !Destination || Destination == this || Quantity <= 0) return false;
    if (!GetOwner()->HasAuthority()) return false;
    if (!HasItem(ItemId, Quantity) || !Destination->CanAddItem(ItemId, Quantity)) return false;
    if (!RemoveItemAuthority(ItemId, Quantity)) return false;
    if (!Destination->AddItemAuthority(ItemId, Quantity))
    {
        AddItemAuthority(ItemId, Quantity);
        return false;
    }
    return true;
}

int32 UARPGInventoryComponent::GetItemCount(FName ItemId) const
{
    int32 Count = 0;
    for (const FARPGInventoryEntry& Entry : Items) if (Entry.ItemId == ItemId) Count += Entry.Quantity;
    return Count;
}

bool UARPGInventoryComponent::HasItem(FName ItemId, int32 Quantity) const
{
    return Quantity <= 0 || GetItemCount(ItemId) >= Quantity;
}

bool UARPGInventoryComponent::SetEquipped(const FGuid& InstanceId, bool bEquipped, FGameplayTag Slot)
{
    if (!GetOwner() || !InstanceId.IsValid()) return false;
    if (!GetOwner()->HasAuthority()) return false;
    for (FARPGInventoryEntry& Entry : Items)
    {
        if (Entry.InstanceId != InstanceId) continue;
        if (bEquipped && Slot.IsValid())
            for (FARPGInventoryEntry& Other : Items) if (Other.InstanceId != InstanceId && Other.bEquipped && Other.EquipmentSlot == Slot) { Other.bEquipped = false; Other.EquipmentSlot = FGameplayTag(); }
        Entry.bEquipped = bEquipped;
        Entry.EquipmentSlot = bEquipped ? Slot : FGameplayTag();
        OnInventoryChanged.Broadcast();
        return true;
    }
    return false;
}

void UARPGInventoryComponent::ReplaceInventory(const TArray<FARPGInventoryEntry>& NewItems)
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;
    Items = NewItems;
    OnInventoryChanged.Broadcast();
}

void UARPGInventoryComponent::OnRep_Items() { OnInventoryChanged.Broadcast(); }

void UARPGInventoryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(UARPGInventoryComponent, Items);
}
