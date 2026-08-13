#include "Components/ARPGQuickAccessComponent.h"
#include "Components/ARPGInventoryComponent.h"
#include "Components/ARPGEquipmentComponent.h"
#include "Components/ARPGStatsComponent.h"
#include "Components/ARPGItemUseComponent.h"
#include "Items/ARPGItemUseBehavior.h"
#include "Data/ARPGItemDefinition.h"
#include "AbilitySystemInterface.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "GameFramework/Character.h"
#include "GameFramework/GameStateBase.h"
#include "Kismet/GameplayStatics.h"
#include "Animation/AnimMontage.h"
#include "Sound/SoundBase.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"

UARPGQuickAccessComponent::UARPGQuickAccessComponent()
{
    SetIsReplicatedByDefault(true);
    PrimaryComponentTick.bCanEverTick = false;
}

void UARPGQuickAccessComponent::BeginPlay()
{
    Super::BeginPlay();
    if (UARPGInventoryComponent* Inventory = GetInventory())
        Inventory->OnInventoryChanged.AddDynamic(this, &UARPGQuickAccessComponent::HandleInventoryChanged);

    if (GetOwner() && GetOwner()->HasAuthority())
    {
        EnsureSlotArrayAuthority();
        RebuildAssignmentRevisionCounterAuthority();
        RepairRuntimeBindingsAuthority();
    }
}

void UARPGQuickAccessComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (UARPGInventoryComponent* Inventory = GetInventory())
        Inventory->OnInventoryChanged.RemoveDynamic(this, &UARPGQuickAccessComponent::HandleInventoryChanged);
    Super::EndPlay(EndPlayReason);
}

bool UARPGQuickAccessComponent::IsValidSlotNumber(int32 SlotNumber) const
{
    return SlotNumber >= 1 && SlotNumber <= FMath::Max(1, MaxQuickAccessSlots);
}

UARPGInventoryComponent* UARPGQuickAccessComponent::GetInventory() const
{
    return GetOwner() ? GetOwner()->FindComponentByClass<UARPGInventoryComponent>() : nullptr;
}

UARPGEquipmentComponent* UARPGQuickAccessComponent::GetEquipment() const
{
    return GetOwner() ? GetOwner()->FindComponentByClass<UARPGEquipmentComponent>() : nullptr;
}

const FARPGInventoryEntry* UARPGQuickAccessComponent::FindOwnedEntryById(FName ItemId) const
{
    const TSet<FGuid> NoExcludedInstances;
    return FindOwnedEntryByIdExcluding(ItemId, NoExcludedInstances);
}

const FARPGInventoryEntry* UARPGQuickAccessComponent::FindOwnedEntryByIdExcluding(FName ItemId, const TSet<FGuid>& ExcludedInstanceIds) const
{
    const UARPGInventoryComponent* Inventory = GetInventory();
    if (!Inventory || ItemId.IsNone()) return nullptr;

    // Prefer the already-equipped runtime instance so save migration/rebinding does not silently point a hotbar slot
    // at a different copy while the character is visibly holding the original one. Exclusions are used by Quick Access
    // repair/fallback so one runtime inventory instance can never back more than one slot.
    const FARPGInventoryEntry* Fallback = nullptr;
    for (const FARPGInventoryEntry& Entry : Inventory->Items)
    {
        if (Entry.ItemId != ItemId || !Entry.InstanceId.IsValid() || Entry.Quantity <= 0 || ExcludedInstanceIds.Contains(Entry.InstanceId)) continue;
        if (Entry.bEquipped) return &Entry;
        if (!Fallback) Fallback = &Entry;
    }
    return Fallback;
}

const FARPGInventoryEntry* UARPGQuickAccessComponent::ResolveOwnedEntry(const FARPGQuickAccessSlot& Slot) const
{
    const UARPGInventoryComponent* Inventory = GetInventory();
    if (!Inventory) return nullptr;

    if (Slot.ItemInstanceId.IsValid())
    {
        const FARPGInventoryEntry* Exact = Inventory->Items.FindByPredicate([&](const FARPGInventoryEntry& Entry)
        {
            return Entry.InstanceId == Slot.ItemInstanceId && Entry.Quantity > 0 && (Slot.ItemId.IsNone() || Entry.ItemId == Slot.ItemId);
        });
        if (Exact) return Exact;
    }

    // ItemId is only a persistent hotbar bookmark. Fallback rebinding must still resolve to a real, positive-quantity
    // runtime entry, and it must not steal an instance already bound to another Quick Access slot.
    TSet<FGuid> ClaimedByOtherSlots;
    for (const FARPGQuickAccessSlot& OtherSlot : QuickAccessSlots)
    {
        if (&OtherSlot == &Slot || !OtherSlot.ItemInstanceId.IsValid()) continue;
        ClaimedByOtherSlots.Add(OtherSlot.ItemInstanceId);
    }
    return FindOwnedEntryByIdExcluding(Slot.ItemId, ClaimedByOtherSlots);
}

EARPGQuickAccessAction UARPGQuickAccessComponent::ResolveAction(const UARPGItemDefinition* Definition) const
{
    if (!Definition) return EARPGQuickAccessAction::SelectOnly;
    if (Definition->QuickAccessAction != EARPGQuickAccessAction::Auto) return Definition->QuickAccessAction;
    if (Definition->bEquippable) return EARPGQuickAccessAction::Equip;
    if (Definition->bUsable) return EARPGQuickAccessAction::Use;
    return EARPGQuickAccessAction::SelectOnly;
}

float UARPGQuickAccessComponent::GetServerTimeSeconds() const
{
    if (UWorld* World = GetWorld())
    {
        if (const AGameStateBase* GameState = World->GetGameState<AGameStateBase>()) return GameState->GetServerWorldTimeSeconds();
        return World->GetTimeSeconds();
    }
    return 0.f;
}

void UARPGQuickAccessComponent::EnsureSlotArrayAuthority()
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;
    const int32 Desired = FMath::Max(1, MaxQuickAccessSlots);
    if (QuickAccessSlots.Num() < Desired) QuickAccessSlots.SetNum(Desired);
    else if (QuickAccessSlots.Num() > Desired) QuickAccessSlots.SetNum(Desired);
    ActiveSlotNumber = IsValidSlotNumber(ActiveSlotNumber) ? ActiveSlotNumber : 0;
}

void UARPGQuickAccessComponent::RebuildAssignmentRevisionCounterAuthority()
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;
    int32 HighestRevision = 0;
    for (const FARPGQuickAccessSlot& Slot : QuickAccessSlots)
        HighestRevision = FMath::Max(HighestRevision, Slot.AssignmentRevision);
    NextAssignmentRevision = FMath::Max(1, HighestRevision + 1);
}

bool UARPGQuickAccessComponent::RepairRuntimeBindingsAuthority(int32 PreferredSlotNumber)
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return false;
    EnsureSlotArrayAuthority();
    const UARPGInventoryComponent* Inventory = GetInventory();
    if (!Inventory) return false;

    RebuildAssignmentRevisionCounterAuthority();
    bool bChanged = false;

    // Duplicate repair must keep the most recently assigned slot, not whichever array index happens to be visited first.
    // PreferredSlotNumber is used by an in-flight assignment so the drop target wins even when legacy duplicate data exists.
    TArray<int32> OrderedIndices;
    OrderedIndices.Reserve(QuickAccessSlots.Num());
    for (int32 Index = 0; Index < QuickAccessSlots.Num(); ++Index) OrderedIndices.Add(Index);
    OrderedIndices.Sort([&](int32 A, int32 B)
    {
        const int32 SlotA = A + 1;
        const int32 SlotB = B + 1;
        if (SlotA == PreferredSlotNumber && SlotB != PreferredSlotNumber) return true;
        if (SlotB == PreferredSlotNumber && SlotA != PreferredSlotNumber) return false;
        const int32 RevA = QuickAccessSlots[A].AssignmentRevision;
        const int32 RevB = QuickAccessSlots[B].AssignmentRevision;
        if (RevA != RevB) return RevA > RevB;
        return A < B;
    });

    TSet<FName> ClaimedItemIds;
    TSet<FGuid> ClaimedInstanceIds;

    for (int32 Index : OrderedIndices)
    {
        FARPGQuickAccessSlot& Slot = QuickAccessSlots[Index];

        // Recover ItemId from an exact owned instance when loading old/runtime-only state.
        if (Slot.ItemId.IsNone() && Slot.ItemInstanceId.IsValid())
        {
            const FARPGInventoryEntry* Existing = Inventory->Items.FindByPredicate([&](const FARPGInventoryEntry& Entry)
            {
                return Entry.InstanceId == Slot.ItemInstanceId && Entry.Quantity > 0;
            });
            if (Existing)
            {
                Slot.ItemId = Existing->ItemId;
                bChanged = true;
            }
        }

        if (Slot.ItemId.IsNone())
        {
            if (Slot.ItemInstanceId.IsValid() || Slot.CooldownEndServerTime != 0.f || Slot.AssignmentRevision != 0)
            {
                Slot = FARPGQuickAccessSlot();
                if (ActiveSlotNumber == Index + 1) ActiveSlotNumber = 0;
                bChanged = true;
            }
            continue;
        }

        // With duplicate item types disabled, only one bookmark for an ItemId may survive. The newest/preferred slot wins.
        if (!bAllowDuplicateItemAssignments && ClaimedItemIds.Contains(Slot.ItemId))
        {
            Slot = FARPGQuickAccessSlot();
            if (ActiveSlotNumber == Index + 1) ActiveSlotNumber = 0;
            bChanged = true;
            continue;
        }

        const FARPGInventoryEntry* Owned = nullptr;
        if (Slot.ItemInstanceId.IsValid() && !ClaimedInstanceIds.Contains(Slot.ItemInstanceId))
        {
            Owned = Inventory->Items.FindByPredicate([&](const FARPGInventoryEntry& Entry)
            {
                return Entry.InstanceId == Slot.ItemInstanceId && Entry.Quantity > 0 && Entry.ItemId == Slot.ItemId;
            });
        }

        // Rebind only to an unclaimed runtime instance. This prevents two slots from resolving to the same GUID even if
        // serialized/replicated legacy state contained duplicate ItemIds or stale instance bindings.
        if (!Owned) Owned = FindOwnedEntryByIdExcluding(Slot.ItemId, ClaimedInstanceIds);

        const FGuid DesiredInstance = Owned ? Owned->InstanceId : FGuid();
        if (DesiredInstance.IsValid() && ClaimedInstanceIds.Contains(DesiredInstance))
        {
            Slot = FARPGQuickAccessSlot();
            if (ActiveSlotNumber == Index + 1) ActiveSlotNumber = 0;
            bChanged = true;
            continue;
        }

        if (Slot.ItemInstanceId != DesiredInstance)
        {
            Slot.ItemInstanceId = DesiredInstance;
            bChanged = true;
        }

        ClaimedItemIds.Add(Slot.ItemId);
        if (DesiredInstance.IsValid()) ClaimedInstanceIds.Add(DesiredInstance);

        const float DesiredCooldown = CooldownEndByItemId.FindRef(Slot.ItemId);
        if (!FMath::IsNearlyEqual(Slot.CooldownEndServerTime, DesiredCooldown))
        {
            Slot.CooldownEndServerTime = DesiredCooldown;
            bChanged = true;
        }
    }

    if (ActiveSlotNumber != 0 && IsValidSlotNumber(ActiveSlotNumber))
    {
        const FARPGQuickAccessSlot& ActiveSlot = QuickAccessSlots[ToIndex(ActiveSlotNumber)];
        if (!ResolveOwnedEntry(ActiveSlot))
        {
            ActiveSlotNumber = 0;
            bChanged = true;
        }
    }
    return bChanged;
}

bool UARPGQuickAccessComponent::IsCanonicalSlotForView(int32 SlotNumber) const
{
    if (!IsValidSlotNumber(SlotNumber) || !QuickAccessSlots.IsValidIndex(ToIndex(SlotNumber))) return false;
    const int32 Index = ToIndex(SlotNumber);
    const FARPGQuickAccessSlot& Slot = QuickAccessSlots[Index];
    if (Slot.ItemId.IsNone() && !Slot.ItemInstanceId.IsValid()) return false;

    // Defensive owner-side projection: if a replicated/legacy array ever contains duplicates, only the newest revision is
    // exposed to UMG/gameplay queries. This prevents transient or corrupt duplicate state from appearing as two hotbar items.
    for (int32 OtherIndex = 0; OtherIndex < QuickAccessSlots.Num(); ++OtherIndex)
    {
        if (OtherIndex == Index) continue;
        const FARPGQuickAccessSlot& Other = QuickAccessSlots[OtherIndex];
        const bool bSameInstance = Slot.ItemInstanceId.IsValid() && Other.ItemInstanceId.IsValid() && Slot.ItemInstanceId == Other.ItemInstanceId;
        const bool bSameItemType = !bAllowDuplicateItemAssignments && !Slot.ItemId.IsNone() && Slot.ItemId == Other.ItemId;
        if (!bSameInstance && !bSameItemType) continue;

        if (Other.AssignmentRevision > Slot.AssignmentRevision) return false;
        if (Other.AssignmentRevision == Slot.AssignmentRevision && OtherIndex < Index) return false;
    }
    return true;
}

void UARPGQuickAccessComponent::HandleInventoryChanged()
{
    if (GetOwner() && GetOwner()->HasAuthority())
    {
        const int32 OldActive = ActiveSlotNumber;
        const bool bBindingsChanged = RepairRuntimeBindingsAuthority();
        if (bBindingsChanged) OnQuickAccessChanged.Broadcast();
        if (OldActive != ActiveSlotNumber) BroadcastActiveSlotChangedLocal();
    }
    else
    {
        // Quantity/equipped state lives on Inventory. Tell owner-side UMG to refresh even when the slot array itself did not change.
        OnQuickAccessChanged.Broadcast();
    }
}

bool UARPGQuickAccessComponent::AssignItemToSlot(int32 SlotNumber, FGuid ItemInstanceId)
{
    if (!GetOwner() || !IsValidSlotNumber(SlotNumber) || !ItemInstanceId.IsValid()) return false;
    if (!GetOwner()->HasAuthority())
    {
        ServerAssignItemToSlot(SlotNumber, ItemInstanceId);
        return true;
    }
    return AssignItemToSlotAuthority(SlotNumber, ItemInstanceId);
}

bool UARPGQuickAccessComponent::AssignItemIdToSlot(int32 SlotNumber, FName ItemId)
{
    if (!GetOwner() || !IsValidSlotNumber(SlotNumber) || ItemId.IsNone()) return false;
    if (!GetOwner()->HasAuthority())
    {
        ServerAssignItemIdToSlot(SlotNumber, ItemId);
        return true;
    }
    return AssignItemIdToSlotAuthority(SlotNumber, ItemId);
}

bool UARPGQuickAccessComponent::AssignItemToSlotAuthority(int32 SlotNumber, FGuid ItemInstanceId)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || !IsValidSlotNumber(SlotNumber) || !ItemInstanceId.IsValid()) return false;
    EnsureSlotArrayAuthority();
    UARPGInventoryComponent* Inventory = GetInventory();
    if (!Inventory) return false;

    const FARPGInventoryEntry* Entry = Inventory->Items.FindByPredicate([&](const FARPGInventoryEntry& Candidate)
    {
        return Candidate.InstanceId == ItemInstanceId && Candidate.Quantity > 0;
    });
    if (!Entry) return false;
    UARPGItemDefinition* Definition = Inventory->ResolveItemDefinition(*Entry);
    if (!Definition || !Definition->bAllowQuickAccess) return false;

    const FName ItemId = Entry->ItemId;
    const int32 TargetIndex = ToIndex(SlotNumber);
    const int32 OldActiveSlotNumber = ActiveSlotNumber;

    // Preserve the currently held Quick Access item before replacing/moving slot state.
    CaptureTrackedQuickAccessEquipmentFromSlotAuthority(ActiveSlotNumber);

    RebuildAssignmentRevisionCounterAuthority();
    const int32 NewRevision = NextAssignmentRevision++;

    // Build the move atomically in the replicated array: clear every previous claim first, then write the target.
    // Exact runtime GUID duplicates are never legal. Same ItemId duplicates are also cleared unless explicitly enabled.
    for (int32 Index = 0; Index < QuickAccessSlots.Num(); ++Index)
    {
        if (Index == TargetIndex) continue;
        FARPGQuickAccessSlot& OtherSlot = QuickAccessSlots[Index];
        const bool bSameRuntimeInstance = OtherSlot.ItemInstanceId.IsValid() && OtherSlot.ItemInstanceId == Entry->InstanceId;
        const bool bSameItemTypeDisallowed = !bAllowDuplicateItemAssignments && !ItemId.IsNone() && OtherSlot.ItemId == ItemId;
        if (!bSameRuntimeInstance && !bSameItemTypeDisallowed) continue;

        if (ActiveSlotNumber == Index + 1) ActiveSlotNumber = SlotNumber;
        OtherSlot = FARPGQuickAccessSlot();
    }

    FARPGQuickAccessSlot& Target = QuickAccessSlots[TargetIndex];
    Target.ItemId = ItemId;
    Target.ItemInstanceId = Entry->InstanceId;
    Target.AssignmentRevision = NewRevision;
    Target.CooldownEndServerTime = CooldownEndByItemId.FindRef(ItemId);

    // Final canonicalization is target-preferred and revision-aware. This repairs old save data and guarantees the target
    // survives even if the incoming array was already corrupt before this assignment.
    RepairRuntimeBindingsAuthority(SlotNumber);

    // Blueprint-compatibility hotfix: keep the public/reflected component schema unchanged while ensuring that
    // replacing the contents of the currently active slot immediately refreshes held weapon/tool equipment.
    // Assignment itself must never auto-use consumables, so only Equip actions are activated here.
    const bool bReplacedCurrentlyActiveSlot = OldActiveSlotNumber == SlotNumber;
    if (bReplacedCurrentlyActiveSlot && ResolveAction(Definition) == EARPGQuickAccessAction::Equip)
    {
        FGuid ActivatedInstanceId;
        const EARPGQuickAccessResult ActivationResult = ActivateSlotAuthority(SlotNumber, ActivatedInstanceId);
        SendActionResult(ActivationResult, SlotNumber, ActivatedInstanceId);
    }

    OnQuickAccessChanged.Broadcast();
    if (OldActiveSlotNumber != ActiveSlotNumber || ActiveSlotNumber == SlotNumber) BroadcastActiveSlotChangedLocal();
    return true;
}

bool UARPGQuickAccessComponent::AssignItemIdToSlotAuthority(int32 SlotNumber, FName ItemId)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || !IsValidSlotNumber(SlotNumber) || ItemId.IsNone()) return false;
    const FARPGInventoryEntry* Entry = FindOwnedEntryById(ItemId);
    return Entry ? AssignItemToSlotAuthority(SlotNumber, Entry->InstanceId) : false;
}

bool UARPGQuickAccessComponent::ClearSlot(int32 SlotNumber)
{
    if (!GetOwner() || !IsValidSlotNumber(SlotNumber)) return false;
    if (!GetOwner()->HasAuthority())
    {
        ServerClearSlot(SlotNumber);
        return true;
    }
    return ClearSlotAuthority(SlotNumber);
}

bool UARPGQuickAccessComponent::ClearSlotAuthority(int32 SlotNumber)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || !IsValidSlotNumber(SlotNumber)) return false;
    EnsureSlotArrayAuthority();
    if (RepairRuntimeBindingsAuthority()) OnQuickAccessChanged.Broadcast();
    if (!IsCanonicalSlotForView(SlotNumber)) return false;
    FARPGQuickAccessSlot& Slot = QuickAccessSlots[ToIndex(SlotNumber)];
    if (Slot.ItemId.IsNone() && !Slot.ItemInstanceId.IsValid()) return false;
    if (ActiveSlotNumber == SlotNumber) CaptureTrackedQuickAccessEquipmentFromSlotAuthority(SlotNumber);
    Slot = FARPGQuickAccessSlot();
    if (ActiveSlotNumber == SlotNumber)
    {
        ActiveSlotNumber = 0;
        BroadcastActiveSlotChangedLocal();
    }
    OnQuickAccessChanged.Broadcast();
    return true;
}

bool UARPGQuickAccessComponent::ClearSlotAndUnequipActive(int32 SlotNumber)
{
    if (!GetOwner() || !IsValidSlotNumber(SlotNumber)) return false;
    if (!GetOwner()->HasAuthority())
    {
        ServerClearSlotAndUnequipActive(SlotNumber);
        return true;
    }
    return ClearSlotAndUnequipActiveAuthority(SlotNumber);
}

bool UARPGQuickAccessComponent::ClearSlotAndUnequipActiveAuthority(int32 SlotNumber)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || !IsValidSlotNumber(SlotNumber)) return false;
    EnsureSlotArrayAuthority();
    if (RepairRuntimeBindingsAuthority()) OnQuickAccessChanged.Broadcast();
    if (!IsCanonicalSlotForView(SlotNumber)) return false;

    const FARPGQuickAccessSlot& Slot = QuickAccessSlots[ToIndex(SlotNumber)];
    if (Slot.ItemId.IsNone() && !Slot.ItemInstanceId.IsValid()) return false;

    if (ActiveSlotNumber == SlotNumber)
    {
        UARPGInventoryComponent* Inventory = GetInventory();
        UARPGEquipmentComponent* Equipment = GetEquipment();
        const FARPGInventoryEntry* Entry = ResolveOwnedEntry(Slot);
        if (Inventory && Entry && Inventory->IsItemInstanceEquipped(Entry->InstanceId))
        {
            const FGuid EquippedInstanceId = Entry->InstanceId;
            if (!Equipment || !Equipment->UnequipItem(EquippedInstanceId)) return false;
            if (LastQuickAccessEquippedInstanceId == EquippedInstanceId) LastQuickAccessEquippedInstanceId.Invalidate();
        }
    }

    return ClearSlotAuthority(SlotNumber);
}

bool UARPGQuickAccessComponent::SwapSlots(int32 FirstSlotNumber, int32 SecondSlotNumber)
{
    if (!GetOwner() || !IsValidSlotNumber(FirstSlotNumber) || !IsValidSlotNumber(SecondSlotNumber) || FirstSlotNumber == SecondSlotNumber) return false;
    if (!GetOwner()->HasAuthority())
    {
        ServerSwapSlots(FirstSlotNumber, SecondSlotNumber);
        return true;
    }
    return SwapSlotsAuthority(FirstSlotNumber, SecondSlotNumber);
}

bool UARPGQuickAccessComponent::SwapSlotsAuthority(int32 FirstSlotNumber, int32 SecondSlotNumber)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || !IsValidSlotNumber(FirstSlotNumber) || !IsValidSlotNumber(SecondSlotNumber) || FirstSlotNumber == SecondSlotNumber) return false;
    EnsureSlotArrayAuthority();
    QuickAccessSlots.Swap(ToIndex(FirstSlotNumber), ToIndex(SecondSlotNumber));
    if (ActiveSlotNumber == FirstSlotNumber) ActiveSlotNumber = SecondSlotNumber;
    else if (ActiveSlotNumber == SecondSlotNumber) ActiveSlotNumber = FirstSlotNumber;
    OnQuickAccessChanged.Broadcast();
    BroadcastActiveSlotChangedLocal();
    return true;
}

bool UARPGQuickAccessComponent::ClearAllSlots()
{
    if (!GetOwner()) return false;
    if (!GetOwner()->HasAuthority())
    {
        ServerClearAllSlots();
        return true;
    }
    return ClearAllSlotsAuthority();
}

bool UARPGQuickAccessComponent::ClearAllSlotsAuthority()
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return false;
    EnsureSlotArrayAuthority();
    bool bHadAnything = ActiveSlotNumber != 0;
    for (FARPGQuickAccessSlot& Slot : QuickAccessSlots)
    {
        bHadAnything |= !Slot.ItemId.IsNone() || Slot.ItemInstanceId.IsValid();
        Slot = FARPGQuickAccessSlot();
    }
    ActiveSlotNumber = 0;
    if (bHadAnything)
    {
        OnQuickAccessChanged.Broadcast();
        BroadcastActiveSlotChangedLocal();
    }
    return bHadAnything;
}


void UARPGQuickAccessComponent::CaptureTrackedQuickAccessEquipmentFromSlotAuthority(int32 SlotNumber)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || !bExclusiveActiveQuickAccessEquipment || !IsValidSlotNumber(SlotNumber)) return;
    EnsureSlotArrayAuthority();
    if (!QuickAccessSlots.IsValidIndex(ToIndex(SlotNumber))) return;

    UARPGInventoryComponent* Inventory = GetInventory();
    if (!Inventory) return;
    const FARPGInventoryEntry* Entry = ResolveOwnedEntry(QuickAccessSlots[ToIndex(SlotNumber)]);
    if (!Entry || !Inventory->IsItemInstanceEquipped(Entry->InstanceId)) return;

    UARPGItemDefinition* Definition = Inventory->ResolveItemDefinition(*Entry);
    if (Definition && Definition->bEquippable && ResolveAction(Definition) == EARPGQuickAccessAction::Equip)
        LastQuickAccessEquippedInstanceId = Entry->InstanceId;
}

bool UARPGQuickAccessComponent::UnequipPreviousQuickAccessEquipmentAuthority(FGuid NewItemInstanceId, FGuid PreviousActiveInstanceId)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || !bExclusiveActiveQuickAccessEquipment || !NewItemInstanceId.IsValid()) return true;

    UARPGInventoryComponent* Inventory = GetInventory();
    UARPGEquipmentComponent* Equipment = GetEquipment();
    if (!Inventory || !Equipment) return false;

    FGuid PreviousInstanceId = LastQuickAccessEquippedInstanceId;
    if ((!PreviousInstanceId.IsValid() || !Inventory->IsItemInstanceEquipped(PreviousInstanceId)) &&
        PreviousActiveInstanceId.IsValid() && Inventory->IsItemInstanceEquipped(PreviousActiveInstanceId))
    {
        PreviousInstanceId = PreviousActiveInstanceId;
    }

    if (!PreviousInstanceId.IsValid() || PreviousInstanceId == NewItemInstanceId)
        return true;

    if (Inventory->IsItemInstanceEquipped(PreviousInstanceId) && !Equipment->UnequipItem(PreviousInstanceId))
        return false;

    if (LastQuickAccessEquippedInstanceId == PreviousInstanceId)
        LastQuickAccessEquippedInstanceId.Invalidate();
    return true;
}

void UARPGQuickAccessComponent::SetActiveSlotAuthority(int32 SlotNumber)
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;
    if (ActiveSlotNumber == SlotNumber) return;
    ActiveSlotNumber = SlotNumber;
    BroadcastActiveSlotChangedLocal();
}

EARPGQuickAccessResult UARPGQuickAccessComponent::SelectSlotAuthority(int32 SlotNumber, FGuid& OutInstanceId)
{
    OutInstanceId.Invalidate();
    if (!GetOwner() || !GetOwner()->HasAuthority() || !IsValidSlotNumber(SlotNumber)) return EARPGQuickAccessResult::InvalidSlot;
    EnsureSlotArrayAuthority();
    if (RepairRuntimeBindingsAuthority()) OnQuickAccessChanged.Broadcast();
    if (!IsCanonicalSlotForView(SlotNumber)) return EARPGQuickAccessResult::EmptySlot;
    const FARPGQuickAccessSlot& Slot = QuickAccessSlots[ToIndex(SlotNumber)];
    if (Slot.ItemId.IsNone() && !Slot.ItemInstanceId.IsValid()) return EARPGQuickAccessResult::EmptySlot;
    const FARPGInventoryEntry* Entry = ResolveOwnedEntry(Slot);
    if (!Entry) return EARPGQuickAccessResult::ItemUnavailable;
    UARPGItemDefinition* Definition = GetInventory()->ResolveItemDefinition(*Entry);
    if (!Definition || !Definition->bAllowQuickAccess) return EARPGQuickAccessResult::ItemNotAllowed;
    OutInstanceId = Entry->InstanceId;
    SetActiveSlotAuthority(SlotNumber);
    return EARPGQuickAccessResult::Success;
}

EARPGQuickAccessResult UARPGQuickAccessComponent::UseSlotAuthority(int32 SlotNumber, FGuid& OutInstanceId)
{
    OutInstanceId.Invalidate();
    if (!GetOwner() || !GetOwner()->HasAuthority() || !IsValidSlotNumber(SlotNumber)) return EARPGQuickAccessResult::InvalidSlot;
    EnsureSlotArrayAuthority();
    FARPGQuickAccessSlot& Slot = QuickAccessSlots[ToIndex(SlotNumber)];
    if (Slot.ItemId.IsNone() && !Slot.ItemInstanceId.IsValid()) return EARPGQuickAccessResult::EmptySlot;

    UARPGInventoryComponent* Inventory = GetInventory();
    const FARPGInventoryEntry* Entry = ResolveOwnedEntry(Slot);
    if (!Inventory || !Entry) return EARPGQuickAccessResult::ItemUnavailable;
    UARPGItemDefinition* Definition = Inventory->ResolveItemDefinition(*Entry);
    if (!Definition || !Definition->bAllowQuickAccess) return EARPGQuickAccessResult::ItemNotAllowed;
    if (!Definition->bUsable) return EARPGQuickAccessResult::ItemNotUsable;

    OutInstanceId = Entry->InstanceId;
    UARPGItemUseComponent* ItemUse = GetOwner()->FindComponentByClass<UARPGItemUseComponent>();
    if (!ItemUse) return EARPGQuickAccessResult::UseFailed;

    FText FailureReason;
    const EARPGItemUseResult UseResult = ItemUse->UseItemAuthority(OutInstanceId, EARPGItemUseSource::QuickAccess, SlotNumber, FailureReason);
    EARPGQuickAccessResult QuickResult = EARPGQuickAccessResult::UseFailed;
    switch (UseResult)
    {
        case EARPGItemUseResult::Success: QuickResult = EARPGQuickAccessResult::Success; break;
        case EARPGItemUseResult::ItemUnavailable:
        case EARPGItemUseResult::InvalidItem: QuickResult = EARPGQuickAccessResult::ItemUnavailable; break;
        case EARPGItemUseResult::ItemNotUsable: QuickResult = EARPGQuickAccessResult::ItemNotUsable; break;
        case EARPGItemUseResult::OnCooldown: QuickResult = EARPGQuickAccessResult::OnCooldown; break;
        case EARPGItemUseResult::InsufficientQuantity: QuickResult = EARPGQuickAccessResult::InsufficientQuantity; break;
        case EARPGItemUseResult::NoUsefulEffect: QuickResult = EARPGQuickAccessResult::NoUsefulEffect; break;
        case EARPGItemUseResult::CustomUseRejected:
        case EARPGItemUseResult::UseFailed:
        default: QuickResult = EARPGQuickAccessResult::UseFailed; break;
    }

    // Preserve the longstanding Quick Access Blueprint presentation event without replaying sound/montage;
    // the central ItemUse component already multicasts those exactly once.
    if (QuickResult == EARPGQuickAccessResult::Success) MulticastPlayItemUsePresentation(SlotNumber, OutInstanceId, Definition);
    return QuickResult;
}

EARPGQuickAccessResult UARPGQuickAccessComponent::ActivateSlotAuthority(int32 SlotNumber, FGuid& OutInstanceId)
{
    OutInstanceId.Invalidate();

    FGuid PreviousActiveInstanceId;
    if (GetOwner() && GetOwner()->HasAuthority() && IsValidSlotNumber(ActiveSlotNumber))
    {
        EnsureSlotArrayAuthority();
        if (QuickAccessSlots.IsValidIndex(ToIndex(ActiveSlotNumber)))
        {
            if (const FARPGInventoryEntry* PreviousEntry = ResolveOwnedEntry(QuickAccessSlots[ToIndex(ActiveSlotNumber)]))
                PreviousActiveInstanceId = PreviousEntry->InstanceId;
        }
        CaptureTrackedQuickAccessEquipmentFromSlotAuthority(ActiveSlotNumber);
    }

    EARPGQuickAccessResult SelectResult = SelectSlotAuthority(SlotNumber, OutInstanceId);
    if (SelectResult != EARPGQuickAccessResult::Success) return SelectResult;

    UARPGInventoryComponent* Inventory = GetInventory();
    const FARPGInventoryEntry* Entry = Inventory ? Inventory->Items.FindByPredicate([&](const FARPGInventoryEntry& Candidate)
    {
        return Candidate.InstanceId == OutInstanceId && Candidate.Quantity > 0;
    }) : nullptr;
    if (!Inventory || !Entry) return EARPGQuickAccessResult::ItemUnavailable;
    UARPGItemDefinition* Definition = Inventory->ResolveItemDefinition(*Entry);
    if (!Definition) return EARPGQuickAccessResult::ItemUnavailable;

    switch (ResolveAction(Definition))
    {
        case EARPGQuickAccessAction::Equip:
        {
            if (!Definition->bEquippable || !Definition->EquipmentSlot.IsValid()) return EARPGQuickAccessResult::EquipFailed;
            UARPGEquipmentComponent* Equipment = GetEquipment();
            if (!Equipment) return EARPGQuickAccessResult::EquipFailed;

            if (!UnequipPreviousQuickAccessEquipmentAuthority(OutInstanceId, PreviousActiveInstanceId))
                return EARPGQuickAccessResult::EquipFailed;

            if (Inventory->IsItemInstanceEquipped(OutInstanceId))
            {
                LastQuickAccessEquippedInstanceId = OutInstanceId;
                return EARPGQuickAccessResult::Success;
            }

            const bool bEquipped = Equipment->EquipItem(OutInstanceId);
            if (bEquipped) LastQuickAccessEquippedInstanceId = OutInstanceId;
            return bEquipped ? EARPGQuickAccessResult::Success : EARPGQuickAccessResult::EquipFailed;
        }
        case EARPGQuickAccessAction::Use:
            // Consumables do not force the current held weapon/tool out of the hand.
            return UseSlotAuthority(SlotNumber, OutInstanceId);
        case EARPGQuickAccessAction::SelectOnly:
        case EARPGQuickAccessAction::Auto:
        default:
            return EARPGQuickAccessResult::Success;
    }
}

EARPGQuickAccessResult UARPGQuickAccessComponent::CycleSlotAuthority(int32 Direction, int32& OutSlotNumber, FGuid& OutInstanceId)
{
    OutSlotNumber = 0;
    OutInstanceId.Invalidate();
    if (!GetOwner() || !GetOwner()->HasAuthority() || Direction == 0) return EARPGQuickAccessResult::InvalidSlot;
    EnsureSlotArrayAuthority();
    const int32 Count = QuickAccessSlots.Num();
    if (Count <= 0) return EARPGQuickAccessResult::EmptySlot;

    int32 StartIndex = ActiveSlotNumber > 0 ? ToIndex(ActiveSlotNumber) : (Direction > 0 ? -1 : 0);
    for (int32 Step = 1; Step <= Count; ++Step)
    {
        int32 Index = (StartIndex + (Direction > 0 ? Step : -Step)) % Count;
        if (Index < 0) Index += Count;
        const FARPGQuickAccessSlot& Candidate = QuickAccessSlots[Index];
        if (Candidate.ItemId.IsNone() && !Candidate.ItemInstanceId.IsValid()) continue;
        if (bCycleSkipsUnavailableSlots && !ResolveOwnedEntry(Candidate)) continue;
        OutSlotNumber = Index + 1;
        return ActivateSlotAuthority(OutSlotNumber, OutInstanceId);
    }
    return EARPGQuickAccessResult::EmptySlot;
}

bool UARPGQuickAccessComponent::SelectSlot(int32 SlotNumber)
{
    if (!GetOwner() || !IsValidSlotNumber(SlotNumber)) return false;
    if (!GetOwner()->HasAuthority())
    {
        ServerSelectSlot(SlotNumber);
        return true;
    }
    FGuid ItemInstanceId;
    const EARPGQuickAccessResult Result = SelectSlotAuthority(SlotNumber, ItemInstanceId);
    SendActionResult(Result, SlotNumber, ItemInstanceId);
    return Result == EARPGQuickAccessResult::Success;
}

bool UARPGQuickAccessComponent::ActivateSlot(int32 SlotNumber)
{
    if (!GetOwner() || !IsValidSlotNumber(SlotNumber)) return false;
    if (!GetOwner()->HasAuthority())
    {
        if (QuickAccessSlots.IsValidIndex(ToIndex(SlotNumber)) && IsCanonicalSlotForView(SlotNumber))
        {
            const FARPGQuickAccessSlot& Slot = QuickAccessSlots[ToIndex(SlotNumber)];
            if (const FARPGInventoryEntry* Entry = ResolveOwnedEntry(Slot))
            {
                if (UARPGItemDefinition* Definition = GetInventory() ? GetInventory()->ResolveItemDefinition(*Entry) : nullptr)
                {
                    if (ResolveAction(Definition) == EARPGQuickAccessAction::Use)
                    {
                        if (UARPGItemUseComponent* ItemUse = GetOwner()->FindComponentByClass<UARPGItemUseComponent>())
                            if (!ItemUse->CanUseItemNow(Entry->InstanceId)) return false;
                    }
                }
            }
        }
        ServerActivateSlot(SlotNumber);
        return true;
    }
    FGuid ItemInstanceId;
    const EARPGQuickAccessResult Result = ActivateSlotAuthority(SlotNumber, ItemInstanceId);
    SendActionResult(Result, SlotNumber, ItemInstanceId);
    return Result == EARPGQuickAccessResult::Success;
}

bool UARPGQuickAccessComponent::UseActiveSlot()
{
    if (!GetOwner()) return false;
    if (!GetOwner()->HasAuthority())
    {
        if (!IsValidSlotNumber(ActiveSlotNumber) || !QuickAccessSlots.IsValidIndex(ToIndex(ActiveSlotNumber))) return false;
        const FARPGQuickAccessSlot& Slot = QuickAccessSlots[ToIndex(ActiveSlotNumber)];
        const FARPGInventoryEntry* Entry = ResolveOwnedEntry(Slot);
        UARPGItemUseComponent* ItemUse = GetOwner()->FindComponentByClass<UARPGItemUseComponent>();
        if (!Entry || (ItemUse && !ItemUse->CanUseItemNow(Entry->InstanceId))) return false;
        ServerUseActiveSlot();
        return true;
    }
    FGuid ItemInstanceId;
    const EARPGQuickAccessResult Result = ActiveSlotNumber > 0 ? UseSlotAuthority(ActiveSlotNumber, ItemInstanceId) : EARPGQuickAccessResult::InvalidSlot;
    SendActionResult(Result, ActiveSlotNumber, ItemInstanceId);
    return Result == EARPGQuickAccessResult::Success;
}

bool UARPGQuickAccessComponent::ActivateNextSlot()
{
    if (!GetOwner()) return false;
    if (!GetOwner()->HasAuthority())
    {
        ServerCycleSlot(1);
        return true;
    }
    int32 SlotNumber = 0; FGuid ItemInstanceId;
    const EARPGQuickAccessResult Result = CycleSlotAuthority(1, SlotNumber, ItemInstanceId);
    SendActionResult(Result, SlotNumber, ItemInstanceId);
    return Result == EARPGQuickAccessResult::Success;
}

bool UARPGQuickAccessComponent::ActivatePreviousSlot()
{
    if (!GetOwner()) return false;
    if (!GetOwner()->HasAuthority())
    {
        ServerCycleSlot(-1);
        return true;
    }
    int32 SlotNumber = 0; FGuid ItemInstanceId;
    const EARPGQuickAccessResult Result = CycleSlotAuthority(-1, SlotNumber, ItemInstanceId);
    SendActionResult(Result, SlotNumber, ItemInstanceId);
    return Result == EARPGQuickAccessResult::Success;
}

bool UARPGQuickAccessComponent::GetSlot(int32 SlotNumber, FARPGQuickAccessSlot& OutSlot) const
{
    OutSlot = FARPGQuickAccessSlot();
    if (!IsValidSlotNumber(SlotNumber) || !QuickAccessSlots.IsValidIndex(ToIndex(SlotNumber)) || !IsCanonicalSlotForView(SlotNumber)) return false;
    OutSlot = QuickAccessSlots[ToIndex(SlotNumber)];
    return true;
}

bool UARPGQuickAccessComponent::GetSlotView(int32 SlotNumber, FARPGQuickAccessSlotView& OutView) const
{
    OutView = FARPGQuickAccessSlotView();
    OutView.SlotNumber = SlotNumber;
    if (!IsValidSlotNumber(SlotNumber) || !QuickAccessSlots.IsValidIndex(ToIndex(SlotNumber))) return false;

    const FARPGQuickAccessSlot& Slot = QuickAccessSlots[ToIndex(SlotNumber)];
    if (!IsCanonicalSlotForView(SlotNumber))
    {
        OutView.bActive = false;
        return true;
    }

    OutView.bAssigned = !Slot.ItemId.IsNone() || Slot.ItemInstanceId.IsValid();
    OutView.bActive = ActiveSlotNumber == SlotNumber;
    OutView.ItemId = Slot.ItemId;
    OutView.AssignmentRevision = Slot.AssignmentRevision;
    OutView.CooldownRemaining = GetSlotCooldownRemaining(SlotNumber);

    const UARPGInventoryComponent* Inventory = GetInventory();
    const FARPGInventoryEntry* Entry = ResolveOwnedEntry(Slot);
    if (Inventory && Entry)
    {
        OutView.bOwned = true;
        OutView.ItemInstanceId = Entry->InstanceId;
        OutView.ItemId = Entry->ItemId;
        OutView.Quantity = Inventory->GetItemCount(Entry->ItemId);
        OutView.ItemDefinition = Inventory->ResolveItemDefinition(*Entry);
        OutView.ResolvedAction = ResolveAction(OutView.ItemDefinition.Get());
    }
    return true;
}

UARPGItemDefinition* UARPGQuickAccessComponent::GetItemDefinitionInSlot(int32 SlotNumber) const
{
    if (!IsValidSlotNumber(SlotNumber) || !QuickAccessSlots.IsValidIndex(ToIndex(SlotNumber)) || !IsCanonicalSlotForView(SlotNumber)) return nullptr;
    const UARPGInventoryComponent* Inventory = GetInventory();
    const FARPGInventoryEntry* Entry = ResolveOwnedEntry(QuickAccessSlots[ToIndex(SlotNumber)]);
    return Inventory && Entry ? Inventory->ResolveItemDefinition(*Entry) : nullptr;
}

int32 UARPGQuickAccessComponent::GetItemQuantityInSlot(int32 SlotNumber) const
{
    if (!IsValidSlotNumber(SlotNumber) || !QuickAccessSlots.IsValidIndex(ToIndex(SlotNumber)) || !IsCanonicalSlotForView(SlotNumber)) return 0;
    const UARPGInventoryComponent* Inventory = GetInventory();
    const FARPGQuickAccessSlot& Slot = QuickAccessSlots[ToIndex(SlotNumber)];
    return Inventory && !Slot.ItemId.IsNone() ? Inventory->GetItemCount(Slot.ItemId) : 0;
}

float UARPGQuickAccessComponent::GetSlotCooldownRemaining(int32 SlotNumber) const
{
    if (!IsValidSlotNumber(SlotNumber) || !QuickAccessSlots.IsValidIndex(ToIndex(SlotNumber))) return 0.f;
    return FMath::Max(0.f, QuickAccessSlots[ToIndex(SlotNumber)].CooldownEndServerTime - GetServerTimeSeconds());
}

EARPGQuickAccessAction UARPGQuickAccessComponent::GetResolvedActionForSlot(int32 SlotNumber) const
{
    return ResolveAction(GetItemDefinitionInSlot(SlotNumber));
}

bool UARPGQuickAccessComponent::IsSlotOwnedAndAvailable(int32 SlotNumber) const
{
    return IsValidSlotNumber(SlotNumber) && QuickAccessSlots.IsValidIndex(ToIndex(SlotNumber)) && IsCanonicalSlotForView(SlotNumber) && ResolveOwnedEntry(QuickAccessSlots[ToIndex(SlotNumber)]) != nullptr;
}

int32 UARPGQuickAccessComponent::FindSlotForItemId(FName ItemId) const
{
    if (ItemId.IsNone()) return 0;
    int32 BestSlot = 0;
    int32 BestRevision = -1;
    for (int32 Index = 0; Index < QuickAccessSlots.Num(); ++Index)
    {
        const FARPGQuickAccessSlot& Slot = QuickAccessSlots[Index];
        if (Slot.ItemId != ItemId || !IsCanonicalSlotForView(Index + 1)) continue;
        if (Slot.AssignmentRevision > BestRevision)
        {
            BestRevision = Slot.AssignmentRevision;
            BestSlot = Index + 1;
        }
    }
    return BestSlot;
}

int32 UARPGQuickAccessComponent::FindSlotForItemInstance(FGuid ItemInstanceId) const
{
    if (!ItemInstanceId.IsValid()) return 0;
    int32 BestSlot = 0;
    int32 BestRevision = -1;
    for (int32 Index = 0; Index < QuickAccessSlots.Num(); ++Index)
    {
        const FARPGQuickAccessSlot& Slot = QuickAccessSlots[Index];
        if (Slot.ItemInstanceId != ItemInstanceId || !IsCanonicalSlotForView(Index + 1)) continue;
        if (Slot.AssignmentRevision > BestRevision)
        {
            BestRevision = Slot.AssignmentRevision;
            BestSlot = Index + 1;
        }
    }
    return BestSlot;
}

void UARPGQuickAccessComponent::NotifyItemUsedAuthority(FName ItemId, float CooldownEndServerTime)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || ItemId.IsNone()) return;
    CooldownEndByItemId.Add(ItemId, CooldownEndServerTime);
    for (FARPGQuickAccessSlot& Slot : QuickAccessSlots)
    {
        if (Slot.ItemId == ItemId) Slot.CooldownEndServerTime = CooldownEndServerTime;
    }
    OnQuickAccessChanged.Broadcast();
}

void UARPGQuickAccessComponent::ReplaceQuickAccessState(const TArray<FARPGQuickAccessSlot>& NewSlots, int32 NewActiveSlotNumber)
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;
    QuickAccessSlots = NewSlots;
    EnsureSlotArrayAuthority();
    for (FARPGQuickAccessSlot& Slot : QuickAccessSlots) Slot.CooldownEndServerTime = 0.f;
    CooldownEndByItemId.Reset();
    ActiveSlotNumber = IsValidSlotNumber(NewActiveSlotNumber) ? NewActiveSlotNumber : 0;
    RebuildAssignmentRevisionCounterAuthority();
    RepairRuntimeBindingsAuthority();
    OnQuickAccessChanged.Broadcast();
    BroadcastActiveSlotChangedLocal();
}

void UARPGQuickAccessComponent::RefreshRuntimeBindings()
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;
    const int32 OldActive = ActiveSlotNumber;
    if (RepairRuntimeBindingsAuthority()) OnQuickAccessChanged.Broadcast();
    if (OldActive != ActiveSlotNumber) BroadcastActiveSlotChangedLocal();
}

void UARPGQuickAccessComponent::OnRep_QuickAccessSlots()
{
    OnQuickAccessChanged.Broadcast();
}

void UARPGQuickAccessComponent::OnRep_ActiveSlotNumber()
{
    BroadcastActiveSlotChangedLocal();
}

void UARPGQuickAccessComponent::BroadcastActiveSlotChangedLocal()
{
    FName ItemId = NAME_None;
    FGuid InstanceId;
    if (ActiveSlotNumber > 0 && QuickAccessSlots.IsValidIndex(ToIndex(ActiveSlotNumber)))
    {
        const FARPGQuickAccessSlot& Slot = QuickAccessSlots[ToIndex(ActiveSlotNumber)];
        ItemId = Slot.ItemId;
        if (const FARPGInventoryEntry* Entry = ResolveOwnedEntry(Slot)) InstanceId = Entry->InstanceId;
    }
    OnActiveQuickAccessSlotChanged.Broadcast(ActiveSlotNumber, ItemId, InstanceId);
}

void UARPGQuickAccessComponent::SendActionResult(EARPGQuickAccessResult Result, int32 SlotNumber, FGuid ItemInstanceId)
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;
    ClientReceiveActionResult(Result, SlotNumber, ItemInstanceId);
}

void UARPGQuickAccessComponent::ClientReceiveActionResult_Implementation(EARPGQuickAccessResult Result, int32 SlotNumber, FGuid ItemInstanceId)
{
    OnQuickAccessActionResult.Broadcast(Result, SlotNumber, ItemInstanceId);
}

void UARPGQuickAccessComponent::PlayItemUsePresentationLocal(int32 SlotNumber, FGuid ItemInstanceId, const UARPGItemDefinition* Definition)
{
    if (!Definition || !GetOwner() || GetOwner()->GetNetMode() == NM_DedicatedServer) return;
    // Audio/montage/custom presentation is centralized in ItemUse. Keep this legacy event alive for
    // existing Quick Access Blueprint listeners without causing duplicated presentation.
    OnQuickAccessItemUsed.Broadcast(SlotNumber, ItemInstanceId, const_cast<UARPGItemDefinition*>(Definition));
}

void UARPGQuickAccessComponent::MulticastPlayItemUsePresentation_Implementation(int32 SlotNumber, FGuid ItemInstanceId, UARPGItemDefinition* Definition)
{
    PlayItemUsePresentationLocal(SlotNumber, ItemInstanceId, Definition);
}

void UARPGQuickAccessComponent::ServerAssignItemToSlot_Implementation(int32 SlotNumber, FGuid ItemInstanceId) { AssignItemToSlotAuthority(SlotNumber, ItemInstanceId); }
void UARPGQuickAccessComponent::ServerAssignItemIdToSlot_Implementation(int32 SlotNumber, FName ItemId) { AssignItemIdToSlotAuthority(SlotNumber, ItemId); }
void UARPGQuickAccessComponent::ServerClearSlot_Implementation(int32 SlotNumber) { ClearSlotAuthority(SlotNumber); }
void UARPGQuickAccessComponent::ServerClearSlotAndUnequipActive_Implementation(int32 SlotNumber) { ClearSlotAndUnequipActiveAuthority(SlotNumber); }
void UARPGQuickAccessComponent::ServerSwapSlots_Implementation(int32 FirstSlotNumber, int32 SecondSlotNumber) { SwapSlotsAuthority(FirstSlotNumber, SecondSlotNumber); }
void UARPGQuickAccessComponent::ServerClearAllSlots_Implementation() { ClearAllSlotsAuthority(); }

void UARPGQuickAccessComponent::ServerSelectSlot_Implementation(int32 SlotNumber)
{
    FGuid ItemInstanceId;
    SendActionResult(SelectSlotAuthority(SlotNumber, ItemInstanceId), SlotNumber, ItemInstanceId);
}

void UARPGQuickAccessComponent::ServerActivateSlot_Implementation(int32 SlotNumber)
{
    FGuid ItemInstanceId;
    SendActionResult(ActivateSlotAuthority(SlotNumber, ItemInstanceId), SlotNumber, ItemInstanceId);
}

void UARPGQuickAccessComponent::ServerUseActiveSlot_Implementation()
{
    FGuid ItemInstanceId;
    const EARPGQuickAccessResult Result = ActiveSlotNumber > 0 ? UseSlotAuthority(ActiveSlotNumber, ItemInstanceId) : EARPGQuickAccessResult::InvalidSlot;
    SendActionResult(Result, ActiveSlotNumber, ItemInstanceId);
}

void UARPGQuickAccessComponent::ServerCycleSlot_Implementation(int32 Direction)
{
    int32 SlotNumber = 0; FGuid ItemInstanceId;
    SendActionResult(CycleSlotAuthority(Direction, SlotNumber, ItemInstanceId), SlotNumber, ItemInstanceId);
}

void UARPGQuickAccessComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME_CONDITION(UARPGQuickAccessComponent, QuickAccessSlots, COND_OwnerOnly);
    DOREPLIFETIME_CONDITION(UARPGQuickAccessComponent, ActiveSlotNumber, COND_OwnerOnly);
}
