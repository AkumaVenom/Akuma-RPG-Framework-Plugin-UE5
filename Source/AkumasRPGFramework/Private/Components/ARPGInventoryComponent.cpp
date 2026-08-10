#include "Components/ARPGInventoryComponent.h"
#include "Components/ARPGEquipmentComponent.h"
#include "Components/ARPGQuickAccessComponent.h"
#include "Data/ARPGItemDefinition.h"
#include "Utilities/ARPGAssetLibrary.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "Engine/World.h"

UARPGInventoryComponent::UARPGInventoryComponent()
{
    SetIsReplicatedByDefault(true);
    PrimaryComponentTick.bCanEverTick = false;
}

UARPGItemDefinition* UARPGInventoryComponent::ResolveItemDefinition(const FARPGInventoryEntry& Entry) const
{
    if (!Entry.ItemDefinition.IsNull())
    {
        if (UARPGItemDefinition* Direct = Entry.ItemDefinition.LoadSynchronous())
            return Direct;
    }

    // Starting Items are also an authoritative designer reference source for migrating older ID-only saves.
    // This does NOT grant/equip the item; it only tells an already-existing runtime entry which asset it represents.
    if (!Entry.ItemId.IsNone())
    {
        for (const FARPGStartingInventoryItem& Starting : StartingItems)
        {
            if (!Starting.Item) continue;
            const FName StartingId = Starting.Item->DefinitionId.IsNone() ? Starting.Item->GetFName() : Starting.Item->DefinitionId;
            if (StartingId == Entry.ItemId) return Starting.Item;
        }
    }

    return Entry.ItemId.IsNone() ? nullptr
        : Cast<UARPGItemDefinition>(UARPGAssetLibrary::ResolveDefinitionById(UARPGItemDefinition::StaticClass(), Entry.ItemId));
}

UARPGItemDefinition* UARPGInventoryComponent::GetItemDefinitionForInstance(FGuid InstanceId) const
{
    if (!InstanceId.IsValid()) return nullptr;
    const FARPGInventoryEntry* Entry = Items.FindByPredicate([&](const FARPGInventoryEntry& Candidate)
    {
        return Candidate.InstanceId == InstanceId;
    });
    return Entry ? ResolveItemDefinition(*Entry) : nullptr;
}

bool UARPGInventoryComponent::IsItemInstanceEquipped(FGuid InstanceId) const
{
    if (!InstanceId.IsValid()) return false;
    const FARPGInventoryEntry* Entry = Items.FindByPredicate([&](const FARPGInventoryEntry& Candidate)
    {
        return Candidate.InstanceId == InstanceId;
    });
    if (!Entry || Entry->Quantity <= 0 || !Entry->bEquipped || !Entry->EquipmentSlot.IsValid()) return false;
    const UARPGItemDefinition* Definition = ResolveItemDefinition(*Entry);
    return Definition && Definition->bEquippable && Definition->EquipmentSlot.IsValid() && Entry->EquipmentSlot == Definition->EquipmentSlot;
}

void UARPGInventoryComponent::BackfillDefinitionReference(FARPGInventoryEntry& Entry)
{
    if (!Entry.ItemDefinition.IsNull() || Entry.ItemId.IsNone()) return;
    if (UARPGItemDefinition* Definition = ResolveItemDefinition(Entry)) Entry.ItemDefinition = Definition;
}

void UARPGInventoryComponent::BeginPlay()
{
    Super::BeginPlay();
    if (GetOwner() && GetOwner()->HasAuthority() && bGrantStartingItemsOnBeginPlay && GetWorld())
        GetWorld()->GetTimerManager().SetTimerForNextTick(this, &UARPGInventoryComponent::ApplyStartingItemsDeferred);
}

void UARPGInventoryComponent::ApplyStartingItemsDeferred()
{
    // Persistence also performs its automatic load on the next tick. Give that load one full
    // deferred pass before seeding designer-authored defaults so an existing save never
    // briefly spawns/equips starter gear before its saved inventory is restored.
    if (!bStartingItemsDelayPrimed)
    {
        bStartingItemsDelayPrimed = true;
        if (GetWorld())
        {
            GetWorld()->GetTimerManager().SetTimerForNextTick(this, &UARPGInventoryComponent::ApplyStartingItemsDeferred);
            return;
        }
    }
    ApplyStartingItems(false);
}

bool UARPGInventoryComponent::ApplyStartingItems(bool bForce)
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return false;
    if (!bForce && bStartingItemsApplied) return false;
    if (!bForce && bOnlyGrantStartingItemsWhenEmpty && Items.Num() > 0)
    {
        bStartingItemsApplied = true;
        return false;
    }

    bool bAddedAny = false;
    TArray<const UARPGItemDefinition*> AutoEquipDefinitions;
    TArray<TPair<const UARPGItemDefinition*, int32>> QuickAccessAssignments;
    int32 PreferredActiveQuickAccessSlot = 0;
    for (const FARPGStartingInventoryItem& Starting : StartingItems)
    {
        if (!Starting.Item || Starting.Quantity <= 0) continue;
        if (!CanAddItemDefinition(Starting.Item, Starting.Quantity)) continue;
        if (AddItemDefinition(Starting.Item, Starting.Quantity))
        {
            bAddedAny = true;
            if (Starting.bEquipOnSpawn && Starting.Item->bEquippable) AutoEquipDefinitions.Add(Starting.Item);
            if (Starting.QuickAccessSlot > 0)
            {
                QuickAccessAssignments.Emplace(Starting.Item, Starting.QuickAccessSlot);
                if (PreferredActiveQuickAccessSlot == 0 && Starting.bEquipOnSpawn) PreferredActiveQuickAccessSlot = Starting.QuickAccessSlot;
            }
        }
    }
    bStartingItemsApplied = true;

    if (AutoEquipDefinitions.Num() > 0)
    {
        if (UARPGEquipmentComponent* Equipment = GetOwner()->FindComponentByClass<UARPGEquipmentComponent>())
        {
            for (const UARPGItemDefinition* Definition : AutoEquipDefinitions)
            {
                const FARPGInventoryEntry* Entry = Items.FindByPredicate([&](const FARPGInventoryEntry& Candidate)
                {
                    const FName DefinitionId = Definition->DefinitionId.IsNone() ? Definition->GetFName() : Definition->DefinitionId;
                    return !Candidate.bEquipped && Candidate.ItemId == DefinitionId;
                });
                if (Entry) Equipment->EquipItem(Entry->InstanceId);
            }
        }
    }

    if (QuickAccessAssignments.Num() > 0)
    {
        if (UARPGQuickAccessComponent* QuickAccess = GetOwner()->FindComponentByClass<UARPGQuickAccessComponent>())
        {
            for (const TPair<const UARPGItemDefinition*, int32>& Assignment : QuickAccessAssignments)
            {
                if (!Assignment.Key || Assignment.Value <= 0) continue;
                const FName StableId = Assignment.Key->DefinitionId.IsNone() ? Assignment.Key->GetFName() : Assignment.Key->DefinitionId;
                QuickAccess->AssignItemIdToSlot(Assignment.Value, StableId);
            }
            if (PreferredActiveQuickAccessSlot > 0) QuickAccess->SelectSlot(PreferredActiveQuickAccessSlot);
        }
    }
    return bAddedAny;
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

bool UARPGInventoryComponent::CanAddItemDefinition(const UARPGItemDefinition* Item, int32 Quantity) const
{
    if (!Item || Quantity <= 0) return false;
    const FName StableId = Item->DefinitionId.IsNone() ? Item->GetFName() : Item->DefinitionId;
    if (StableId.IsNone()) return false;
    int32 Remaining = Quantity;
    const int32 MaxStack = FMath::Max(1, Item->MaxStack);
    for (const FARPGInventoryEntry& Entry : Items)
    {
        if (Entry.ItemId == StableId && !Entry.bEquipped && Entry.Quantity < MaxStack)
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
    const UARPGItemDefinition* Definition = Cast<UARPGItemDefinition>(UARPGAssetLibrary::ResolveDefinitionById(UARPGItemDefinition::StaticClass(), ItemId));
    return AddItemAuthority(ItemId, Quantity, 0, Definition);
}

bool UARPGInventoryComponent::AddItemDefinition(const UARPGItemDefinition* Item, int32 Quantity)
{
    if (!Item || Quantity <= 0) return false;
    if (!GetOwner() || !GetOwner()->HasAuthority()) return false;
    const FName StableId = Item->DefinitionId.IsNone() ? Item->GetFName() : Item->DefinitionId;
    if (StableId.IsNone()) return false;
    return AddItemAuthority(StableId, Quantity, Item->MaxStack, Item);
}

bool UARPGInventoryComponent::AddItemAuthority(FName ItemId, int32 Quantity, int32 ExplicitMaxStack, const UARPGItemDefinition* ExplicitDefinition)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || ItemId.IsNone() || Quantity <= 0) return false;
    const int32 MaxStack = FMath::Max(1, ExplicitMaxStack > 0 ? ExplicitMaxStack : ResolveMaxStack(ItemId));
    int32 Remaining = Quantity;
    for (FARPGInventoryEntry& Entry : Items)
    {
        if (Entry.ItemId != ItemId || Entry.bEquipped || Entry.Quantity >= MaxStack) continue;
        const int32 Added = FMath::Min(Remaining, MaxStack - Entry.Quantity);
        Entry.Quantity += Added;
        if (Entry.ItemDefinition.IsNull() && ExplicitDefinition) Entry.ItemDefinition = ExplicitDefinition;
        Remaining -= Added;
        if (Remaining <= 0) { OnInventoryChanged.Broadcast(); return true; }
    }
    while (Remaining > 0 && Items.Num() < MaxSlots)
    {
        FARPGInventoryEntry Entry;
        Entry.InstanceId = FGuid::NewGuid();
        Entry.ItemDefinition = ExplicitDefinition;
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
    const FARPGInventoryEntry* SourceEntry = Items.FindByPredicate([&](const FARPGInventoryEntry& Candidate)
    {
        return Candidate.ItemId == ItemId && Candidate.Quantity > 0;
    });
    const UARPGItemDefinition* Definition = SourceEntry ? ResolveItemDefinition(*SourceEntry) : nullptr;
    if (!RemoveItemAuthority(ItemId, Quantity)) return false;
    if (!Destination->AddItemAuthority(ItemId, Quantity, Definition ? Definition->MaxStack : 0, Definition))
    {
        AddItemAuthority(ItemId, Quantity, Definition ? Definition->MaxStack : 0, Definition);
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
        if (Entry.Quantity <= 0) return false;

        if (bEquipped)
        {
            UARPGItemDefinition* Definition = ResolveItemDefinition(Entry);
            if (!Definition || !Definition->bEquippable || !Definition->EquipmentSlot.IsValid()) return false;
            if (!Slot.IsValid() || Slot != Definition->EquipmentSlot) return false;
            BackfillDefinitionReference(Entry);

            for (FARPGInventoryEntry& Other : Items)
            {
                if (Other.InstanceId != InstanceId && Other.bEquipped && Other.EquipmentSlot == Slot)
                {
                    Other.bEquipped = false;
                    Other.EquipmentSlot = FGameplayTag();
                }
            }
        }

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
    TSet<FGameplayTag> OccupiedEquipmentSlots;
    for (FARPGInventoryEntry& Entry : Items)
    {
        if (!Entry.InstanceId.IsValid()) Entry.InstanceId = FGuid::NewGuid();
        Entry.Quantity = FMath::Max(0, Entry.Quantity);
        BackfillDefinitionReference(Entry);

        if (Entry.Quantity <= 0)
        {
            Entry.bEquipped = false;
            Entry.EquipmentSlot = FGameplayTag();
            continue;
        }

        if (Entry.bEquipped)
        {
            UARPGItemDefinition* Definition = ResolveItemDefinition(Entry);
            if (!Definition || !Definition->bEquippable || !Definition->EquipmentSlot.IsValid() || OccupiedEquipmentSlots.Contains(Definition->EquipmentSlot))
            {
                Entry.bEquipped = false;
                Entry.EquipmentSlot = FGameplayTag();
            }
            else
            {
                // Migrate stale/missing saved slot metadata to the slot authored by the exact item definition.
                Entry.EquipmentSlot = Definition->EquipmentSlot;
                OccupiedEquipmentSlots.Add(Entry.EquipmentSlot);
            }
        }
        else
        {
            Entry.EquipmentSlot = FGameplayTag();
        }
    }
    Items.RemoveAll([](const FARPGInventoryEntry& Entry){ return Entry.Quantity <= 0 || Entry.ItemId.IsNone(); });
    OnInventoryChanged.Broadcast();
}

void UARPGInventoryComponent::OnRep_Items() { OnInventoryChanged.Broadcast(); }

void UARPGInventoryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(UARPGInventoryComponent, Items);
}
