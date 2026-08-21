#include "Components/ARPGInventoryComponent.h"
#include "Components/ARPGEquipmentComponent.h"
#include "Components/ARPGQuickAccessComponent.h"
#include "Components/ARPGPersistenceComponent.h"
#include "Data/ARPGItemDefinition.h"
#include "Utilities/ARPGAssetLibrary.h"
#include "Net/UnrealNetwork.h"
#include "UObject/SoftObjectPath.h"
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
    if (!GetOwner() || !GetOwner()->HasAuthority() || !bGrantStartingItemsOnBeginPlay) return;

    // Character persistence, when present and configured for automatic load, owns the decision of
    // whether this is an existing character or a brand-new one. Do not race it with an arbitrary
    // number of next-tick delays: a loaded save with an intentionally empty Inventory must stay
    // empty, and legacy Guest identity recovery can legitimately take more than one deferred pass.
    if (UARPGPersistenceComponent* Persistence = GetOwner()->FindComponentByClass<UARPGPersistenceComponent>())
    {
        if (Persistence->bAutoLoadOnBeginPlay)
        {
            if (Persistence->HasInitialAutoLoadResolved())
                ResolveStartingItemsAfterInitialPersistence(Persistence->DidInitialCharacterSaveExist());
            return;
        }
    }

    // NPCs/custom actors without account-character auto-load retain the normal designer-authored
    // starter-loadout behavior. One deferred frame keeps component initialization deterministic.
    if (GetWorld()) GetWorld()->GetTimerManager().SetTimerForNextTick(this, &UARPGInventoryComponent::ApplyStartingItemsDeferred);
}

void UARPGInventoryComponent::ApplyStartingItemsDeferred()
{
    ApplyStartingItems(false);
}

void UARPGInventoryComponent::ResolveStartingItemsAfterInitialPersistence(bool bExistingCharacterSave)
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;

    if (bExistingCharacterSave)
    {
        // Starting Items are creation defaults, not a refill policy. Mark them resolved even when
        // the loaded runtime Inventory has zero entries so a deliberately emptied saved character
        // never receives the default loadout again on BeginPlay. bForce remains an explicit escape
        // hatch for projects that intentionally want to grant the defaults later.
        bStartingItemsApplied = true;
        return;
    }

    if (bGrantStartingItemsOnBeginPlay) ApplyStartingItems(false);
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
        return Def->bUsesDurability ? 1 : FMath::Max(1, Def->MaxStack);
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
    const int32 MaxStack = Item->bUsesDurability ? 1 : FMath::Max(1, Item->MaxStack);
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
    const int32 MaxStack = ExplicitDefinition && ExplicitDefinition->bUsesDurability
        ? 1
        : FMath::Max(1, ExplicitMaxStack > 0 ? ExplicitMaxStack : ResolveMaxStack(ItemId));
    int32 Remaining = Quantity;
    for (FARPGInventoryEntry& Entry : Items)
    {
        if (Entry.ItemId != ItemId || Entry.bEquipped || Entry.Quantity >= MaxStack) continue;
        const int32 Added = FMath::Min(Remaining, MaxStack - Entry.Quantity);
        Entry.Quantity += Added;
        if (Entry.ItemDefinition.IsNull() && ExplicitDefinition)
        {
            Entry.ItemDefinition = TSoftObjectPtr<UARPGItemDefinition>(FSoftObjectPath(ExplicitDefinition));
        }
        Remaining -= Added;
        if (Remaining <= 0) { OnInventoryChanged.Broadcast(); return true; }
    }
    while (Remaining > 0 && Items.Num() < MaxSlots)
    {
        FARPGInventoryEntry Entry;
        Entry.InstanceId = FGuid::NewGuid();
        if (ExplicitDefinition)
        {
            Entry.ItemDefinition = TSoftObjectPtr<UARPGItemDefinition>(FSoftObjectPath(ExplicitDefinition));
        }
        Entry.ItemId = ItemId;
        Entry.Quantity = FMath::Min(Remaining, MaxStack);
        Entry.Durability = ExplicitDefinition && ExplicitDefinition->bUsesDurability
            ? FMath::Max(1.f, ExplicitDefinition->MaxDurability)
            : 100.f;
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

bool UARPGInventoryComponent::TransferItemInstanceTo(UARPGInventoryComponent* Destination, FGuid InstanceId, int32 Quantity)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || !Destination || Destination == this || !InstanceId.IsValid() || Quantity <= 0) return false;
    const int32 SourceIndex = Items.IndexOfByPredicate([&](const FARPGInventoryEntry& Entry){ return Entry.InstanceId == InstanceId; });
    if (!Items.IsValidIndex(SourceIndex)) return false;
    const FARPGInventoryEntry SourceEntry = Items[SourceIndex];
    if (SourceEntry.bEquipped || SourceEntry.Quantity < Quantity || SourceEntry.ItemId.IsNone()) return false;
    const UARPGItemDefinition* Definition = ResolveItemDefinition(SourceEntry);
    const bool bCanFit = Definition ? Destination->CanAddItemDefinition(Definition, Quantity) : Destination->CanAddItem(SourceEntry.ItemId, Quantity);
    if (!bCanFit) return false;

    const TArray<FARPGInventoryEntry> SourceBefore = Items;
    const TArray<FARPGInventoryEntry> DestinationBefore = Destination->Items;

    if (Definition && Definition->bUsesDurability)
    {
        TArray<FARPGInventoryEntry> MovedEntries;
        MovedEntries.Reserve(Quantity);
        for (int32 Unit = 0; Unit < Quantity; ++Unit)
        {
            FARPGInventoryEntry Moved = SourceEntry;
            Moved.Quantity = 1;
            Moved.bEquipped = false;
            Moved.EquipmentSlot = FGameplayTag();
            // Preserve identity only for a complete single-instance move; split legacy durable stacks into unique instances.
            if (SourceEntry.Quantity != 1 || Quantity != 1 || Unit > 0) Moved.InstanceId = FGuid::NewGuid();
            if (Destination->Items.ContainsByPredicate([&](const FARPGInventoryEntry& Existing){ return Existing.InstanceId == Moved.InstanceId; }))
                Moved.InstanceId = FGuid::NewGuid();
            MovedEntries.Add(Moved);
        }
        if (!RemoveItemInstanceAuthority(InstanceId, Quantity)) return false;
        Destination->Items.Append(MovedEntries);
        Destination->OnInventoryChanged.Broadcast();
        return true;
    }

    if (!RemoveItemInstanceAuthority(InstanceId, Quantity)) return false;
    const bool bAdded = Definition ? Destination->AddItemDefinition(Definition, Quantity) : Destination->AddItem(SourceEntry.ItemId, Quantity);
    if (!bAdded)
    {
        ReplaceInventory(SourceBefore);
        Destination->ReplaceInventory(DestinationBefore);
        return false;
    }
    return true;
}

bool UARPGInventoryComponent::TransferItemTo(UARPGInventoryComponent* Destination, FName ItemId, int32 Quantity)
{
    if (!GetOwner() || !Destination || Destination == this || ItemId.IsNone() || Quantity <= 0) return false;
    if (!GetOwner()->HasAuthority()) return false;

    const FARPGInventoryEntry* SourceEntry = Items.FindByPredicate([&](const FARPGInventoryEntry& Candidate)
    {
        return Candidate.ItemId == ItemId && Candidate.Quantity > 0;
    });
    const UARPGItemDefinition* Definition = SourceEntry ? ResolveItemDefinition(*SourceEntry) : nullptr;

    // Durable equipment is runtime-instance state. Recreating it through AddItemAuthority would reset
    // durability to MaxDurability, so storage/container transfer moves exact entry state instead.
    // Equipped durable items are deliberately not transfer-consumed; normal Equipment unequip must happen first.
    if (Definition && Definition->bUsesDurability)
    {
        if (GetUnequippedItemCount(ItemId) < Quantity || !Destination->CanAddItemDefinition(Definition, Quantity)) return false;

        int32 Remaining = Quantity;
        TArray<FARPGInventoryEntry> MovedEntries;
        struct FSourceTake { FGuid InstanceId; int32 Quantity = 0; };
        TArray<FSourceTake> Takes;

        for (const FARPGInventoryEntry& Entry : Items)
        {
            if (Remaining <= 0) break;
            if (Entry.ItemId != ItemId || Entry.bEquipped || Entry.Quantity <= 0) continue;
            const int32 Take = FMath::Min(Remaining, Entry.Quantity);
            Takes.Add({Entry.InstanceId, Take});
            for (int32 Unit = 0; Unit < Take; ++Unit)
            {
                FARPGInventoryEntry Moved = Entry;
                Moved.Quantity = 1;
                Moved.bEquipped = false;
                Moved.EquipmentSlot = FGameplayTag();
                // Preserve identity only when the complete one-unit runtime entry moves. Legacy stacked
                // durable entries are split into unique runtime instances during transfer.
                if (Entry.Quantity != 1 || Take != 1) Moved.InstanceId = FGuid::NewGuid();
                if (Destination->Items.ContainsByPredicate([&](const FARPGInventoryEntry& Existing){ return Existing.InstanceId == Moved.InstanceId; }))
                    Moved.InstanceId = FGuid::NewGuid();
                MovedEntries.Add(Moved);
            }
            Remaining -= Take;
        }
        if (Remaining > 0 || MovedEntries.Num() != Quantity) return false;

        for (const FSourceTake& Take : Takes)
        {
            const int32 Index = Items.IndexOfByPredicate([&](const FARPGInventoryEntry& Entry){ return Entry.InstanceId == Take.InstanceId; });
            if (!Items.IsValidIndex(Index) || Items[Index].bEquipped || Items[Index].Quantity < Take.Quantity) return false;
            Items[Index].Quantity -= Take.Quantity;
            if (Items[Index].Quantity <= 0) Items.RemoveAt(Index);
        }
        Destination->Items.Append(MovedEntries);
        OnInventoryChanged.Broadcast();
        Destination->OnInventoryChanged.Broadcast();
        return true;
    }

    if (!HasItem(ItemId, Quantity) || !Destination->CanAddItem(ItemId, Quantity)) return false;
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

int32 UARPGInventoryComponent::GetUnequippedItemCount(FName ItemId) const
{
    int32 Count = 0;
    for (const FARPGInventoryEntry& Entry : Items)
        if (Entry.ItemId == ItemId && !Entry.bEquipped) Count += FMath::Max(0, Entry.Quantity);
    return Count;
}

bool UARPGInventoryComponent::HasUnequippedItem(FName ItemId, int32 Quantity) const
{
    return Quantity <= 0 || GetUnequippedItemCount(ItemId) >= Quantity;
}

bool UARPGInventoryComponent::RemoveUnequippedItem(FName ItemId, int32 Quantity)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || ItemId.IsNone() || Quantity <= 0) return false;
    if (!HasUnequippedItem(ItemId, Quantity)) return false;
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
    if (Remaining == 0) OnInventoryChanged.Broadcast();
    return Remaining == 0;
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
            if (Definition->bUsesDurability && Entry.Durability <= KINDA_SMALL_NUMBER) return false;
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

bool UARPGInventoryComponent::GetItemDurability(FGuid InstanceId, float& OutCurrentDurability, float& OutMaxDurability, float& OutDurabilityPercent, bool& bOutBroken) const
{
    OutCurrentDurability = 0.f;
    OutMaxDurability = 0.f;
    OutDurabilityPercent = 0.f;
    bOutBroken = false;
    if (!InstanceId.IsValid()) return false;
    const FARPGInventoryEntry* Entry = Items.FindByPredicate([&](const FARPGInventoryEntry& Candidate){ return Candidate.InstanceId == InstanceId; });
    if (!Entry) return false;
    const UARPGItemDefinition* Definition = ResolveItemDefinition(*Entry);
    if (!Definition || !Definition->bUsesDurability) return false;
    OutMaxDurability = FMath::Max(1.f, Definition->MaxDurability);
    OutCurrentDurability = FMath::Clamp(Entry->Durability, 0.f, OutMaxDurability);
    OutDurabilityPercent = FMath::Clamp(OutCurrentDurability / OutMaxDurability, 0.f, 1.f);
    bOutBroken = OutCurrentDurability <= KINDA_SMALL_NUMBER;
    return true;
}

bool UARPGInventoryComponent::IsItemBroken(FGuid InstanceId) const
{
    float Current = 0.f, Max = 0.f, Percent = 0.f;
    bool bBroken = false;
    return GetItemDurability(InstanceId, Current, Max, Percent, bBroken) && bBroken;
}

bool UARPGInventoryComponent::SetItemDurability(FGuid InstanceId, float NewDurability)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || !InstanceId.IsValid()) return false;
    FARPGInventoryEntry* Entry = Items.FindByPredicate([&](const FARPGInventoryEntry& Candidate){ return Candidate.InstanceId == InstanceId; });
    if (!Entry) return false;
    UARPGItemDefinition* Definition = ResolveItemDefinition(*Entry);
    if (!Definition || !Definition->bUsesDurability) return false;

    const float MaxDurability = FMath::Max(1.f, Definition->MaxDurability);
    const float OldDurability = FMath::Clamp(Entry->Durability, 0.f, MaxDurability);
    const float Clamped = FMath::Clamp(NewDurability, 0.f, MaxDurability);
    if (FMath::IsNearlyEqual(OldDurability, Clamped, KINDA_SMALL_NUMBER)) return false;

    const bool bWasEquipped = Entry->bEquipped;
    Entry->Durability = Clamped;
    const bool bBroken = Clamped <= KINDA_SMALL_NUMBER;
    OnItemDurabilityChanged.Broadcast(InstanceId, Clamped, MaxDurability, bBroken);
    OnInventoryChanged.Broadcast();

    // Do not dereference the runtime entry after delegates broadcast: project listeners are allowed to
    // mutate Inventory and could invalidate its array address. The pre-broadcast equipped snapshot is enough.
    if (bBroken && bWasEquipped && Definition->bUnequipWhenBroken)
        if (UARPGEquipmentComponent* Equipment = GetOwner()->FindComponentByClass<UARPGEquipmentComponent>()) Equipment->UnequipItem(InstanceId);
    return true;
}

bool UARPGInventoryComponent::DamageItemDurability(FGuid InstanceId, float Amount)
{
    if (Amount <= 0.f || !GetOwner() || !GetOwner()->HasAuthority()) return false;
    FARPGInventoryEntry* Entry = Items.FindByPredicate([&](const FARPGInventoryEntry& Candidate){ return Candidate.InstanceId == InstanceId; });
    if (!Entry) return false;
    const UARPGItemDefinition* Definition = ResolveItemDefinition(*Entry);
    if (!Definition || !Definition->bUsesDurability || Entry->Durability <= KINDA_SMALL_NUMBER) return false;
    return SetItemDurability(InstanceId, Entry->Durability - Amount);
}

bool UARPGInventoryComponent::RepairItemDurability(FGuid InstanceId, float Amount)
{
    if (Amount <= 0.f || !GetOwner() || !GetOwner()->HasAuthority()) return false;
    FARPGInventoryEntry* Entry = Items.FindByPredicate([&](const FARPGInventoryEntry& Candidate){ return Candidate.InstanceId == InstanceId; });
    if (!Entry) return false;
    const UARPGItemDefinition* Definition = ResolveItemDefinition(*Entry);
    if (!Definition || !Definition->bUsesDurability) return false;
    const float MaxDurability = FMath::Max(1.f, Definition->MaxDurability);
    if (Entry->Durability >= MaxDurability - KINDA_SMALL_NUMBER) return false;
    return SetItemDurability(InstanceId, Entry->Durability + Amount);
}

bool UARPGInventoryComponent::RepairItemToFull(FGuid InstanceId)
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return false;
    FARPGInventoryEntry* Entry = Items.FindByPredicate([&](const FARPGInventoryEntry& Candidate){ return Candidate.InstanceId == InstanceId; });
    if (!Entry) return false;
    const UARPGItemDefinition* Definition = ResolveItemDefinition(*Entry);
    if (!Definition || !Definition->bUsesDurability) return false;
    return SetItemDurability(InstanceId, FMath::Max(1.f, Definition->MaxDurability));
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
        if (const UARPGItemDefinition* Definition = ResolveItemDefinition(Entry))
        {
            if (Definition->bUsesDurability)
                Entry.Durability = FMath::Clamp(Entry.Durability, 0.f, FMath::Max(1.f, Definition->MaxDurability));
            else
                Entry.Durability = FMath::Clamp(Entry.Durability, 0.f, 100.f);
        }

        if (Entry.Quantity <= 0)
        {
            Entry.bEquipped = false;
            Entry.EquipmentSlot = FGameplayTag();
            continue;
        }

        if (Entry.bEquipped)
        {
            UARPGItemDefinition* Definition = ResolveItemDefinition(Entry);
            if (!Definition || !Definition->bEquippable || !Definition->EquipmentSlot.IsValid() || (Definition->bUsesDurability && Entry.Durability <= KINDA_SMALL_NUMBER) || OccupiedEquipmentSlots.Contains(Definition->EquipmentSlot))
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
