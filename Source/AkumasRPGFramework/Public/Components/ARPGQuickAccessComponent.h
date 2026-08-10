#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ARPGTypes.h"
#include "ARPGQuickAccessComponent.generated.h"

class UARPGInventoryComponent;
class UARPGEquipmentComponent;
class UARPGItemDefinition;
struct FARPGInventoryEntry;

USTRUCT(BlueprintType)
struct AKUMASRPGFRAMEWORK_API FARPGQuickAccessSlotView
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="Quick Access") int32 SlotNumber = 0;
    UPROPERTY(BlueprintReadOnly, Category="Quick Access") bool bAssigned = false;
    UPROPERTY(BlueprintReadOnly, Category="Quick Access") bool bOwned = false;
    UPROPERTY(BlueprintReadOnly, Category="Quick Access") bool bActive = false;
    UPROPERTY(BlueprintReadOnly, Category="Quick Access") FName ItemId = NAME_None;
    UPROPERTY(BlueprintReadOnly, Category="Quick Access") FGuid ItemInstanceId;
    UPROPERTY(BlueprintReadOnly, Category="Quick Access") int32 AssignmentRevision = 0;
    UPROPERTY(BlueprintReadOnly, Category="Quick Access") int32 Quantity = 0;
    UPROPERTY(BlueprintReadOnly, Category="Quick Access") TObjectPtr<UARPGItemDefinition> ItemDefinition = nullptr;
    UPROPERTY(BlueprintReadOnly, Category="Quick Access") EARPGQuickAccessAction ResolvedAction = EARPGQuickAccessAction::SelectOnly;
    UPROPERTY(BlueprintReadOnly, Category="Quick Access") float CooldownRemaining = 0.f;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FARPGQuickAccessChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FARPGActiveQuickAccessSlotChanged, int32, SlotNumber, FName, ItemId, FGuid, ItemInstanceId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FARPGQuickAccessActionResult, EARPGQuickAccessResult, Result, int32, SlotNumber, FGuid, ItemInstanceId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FARPGQuickAccessItemUsed, int32, SlotNumber, FGuid, ItemInstanceId, UARPGItemDefinition*, ItemDefinition);

UCLASS(ClassGroup=(ARPG), BlueprintType, Blueprintable, meta=(BlueprintSpawnableComponent))
class AKUMASRPGFRAMEWORK_API UARPGQuickAccessComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UARPGQuickAccessComponent();

    // Slots are intentionally numbered 1..MaxQuickAccessSlots so Blueprint input wiring matches keyboard/gamepad labels.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quick Access", meta=(ClampMin="1", ClampMax="20")) int32 MaxQuickAccessSlots = 8;
    // When enabled, different owned runtime instances of the same ItemId may occupy separate slots.
    // The exact same runtime instance is always unique and is moved rather than duplicated.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quick Access", meta=(DisplayName="Allow Same Item Type In Multiple Slots", ToolTip="Allows different owned runtime instances/stacks of the same ItemId in multiple Quick Access slots. The exact same runtime inventory instance is always unique and moves to the newly assigned slot.")) bool bAllowDuplicateItemAssignments = false;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quick Access|Cycling") bool bCycleSkipsUnavailableSlots = true;
    // Active Quick Access equipment is a single held-item channel by default. Switching to another equippable
    // Quick Access item explicitly unequips the previously activated Quick Access equipment even when the
    // two Item Definitions use different EquipmentSlot tags (for example Tool vs Weapon) but share a hand.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quick Access|Equipment", meta=(DisplayName="Exclusive Active Quick Access Equipment", ToolTip="When enabled, activating a new equippable Quick Access item unequips the previously activated Quick Access equipment first. Armor and unrelated equipment are unaffected.")) bool bExclusiveActiveQuickAccessEquipment = true;

    // Runtime state is owner-only replicated: remote players need equipped visuals, not another player's private hotbar layout.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, ReplicatedUsing=OnRep_QuickAccessSlots, SaveGame, Category="Quick Access|Runtime") TArray<FARPGQuickAccessSlot> QuickAccessSlots;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, ReplicatedUsing=OnRep_ActiveSlotNumber, SaveGame, Category="Quick Access|Runtime") int32 ActiveSlotNumber = 0;

    UPROPERTY(BlueprintAssignable, Category="Quick Access|Events") FARPGQuickAccessChanged OnQuickAccessChanged;
    UPROPERTY(BlueprintAssignable, Category="Quick Access|Events") FARPGActiveQuickAccessSlotChanged OnActiveQuickAccessSlotChanged;
    UPROPERTY(BlueprintAssignable, Category="Quick Access|Events") FARPGQuickAccessActionResult OnQuickAccessActionResult;
    UPROPERTY(BlueprintAssignable, Category="Quick Access|Events") FARPGQuickAccessItemUsed OnQuickAccessItemUsed;

    // Assignment always validates a real owned runtime inventory entry. A Data Asset existing in Content is never enough.
    UFUNCTION(BlueprintCallable, Category="ARPG|Quick Access") bool AssignItemToSlot(int32 SlotNumber, FGuid ItemInstanceId);
    UFUNCTION(BlueprintCallable, Category="ARPG|Quick Access") bool AssignItemIdToSlot(int32 SlotNumber, FName ItemId);
    UFUNCTION(BlueprintCallable, Category="ARPG|Quick Access") bool ClearSlot(int32 SlotNumber);
    UFUNCTION(BlueprintCallable, Category="ARPG|Quick Access") bool SwapSlots(int32 FirstSlotNumber, int32 SecondSlotNumber);
    UFUNCTION(BlueprintCallable, Category="ARPG|Quick Access") bool ClearAllSlots();

    // Select only changes the active slot. Activate is the one-button gameplay path: weapon/tool -> equip, usable -> use.
    UFUNCTION(BlueprintCallable, Category="ARPG|Quick Access|Input") bool SelectSlot(int32 SlotNumber);
    UFUNCTION(BlueprintCallable, Category="ARPG|Quick Access|Input") bool ActivateSlot(int32 SlotNumber);
    UFUNCTION(BlueprintCallable, Category="ARPG|Quick Access|Input") bool UseActiveSlot();
    UFUNCTION(BlueprintCallable, Category="ARPG|Quick Access|Input") bool ActivateNextSlot();
    UFUNCTION(BlueprintCallable, Category="ARPG|Quick Access|Input") bool ActivatePreviousSlot();

    UFUNCTION(BlueprintPure, Category="ARPG|Quick Access") bool GetSlot(int32 SlotNumber, FARPGQuickAccessSlot& OutSlot) const;
    UFUNCTION(BlueprintPure, Category="ARPG|Quick Access") bool GetSlotView(int32 SlotNumber, FARPGQuickAccessSlotView& OutView) const;
    UFUNCTION(BlueprintPure, Category="ARPG|Quick Access") UARPGItemDefinition* GetItemDefinitionInSlot(int32 SlotNumber) const;
    UFUNCTION(BlueprintPure, Category="ARPG|Quick Access") int32 GetItemQuantityInSlot(int32 SlotNumber) const;
    UFUNCTION(BlueprintPure, Category="ARPG|Quick Access") float GetSlotCooldownRemaining(int32 SlotNumber) const;
    UFUNCTION(BlueprintPure, Category="ARPG|Quick Access") EARPGQuickAccessAction GetResolvedActionForSlot(int32 SlotNumber) const;
    UFUNCTION(BlueprintPure, Category="ARPG|Quick Access") bool IsSlotOwnedAndAvailable(int32 SlotNumber) const;
    UFUNCTION(BlueprintPure, Category="ARPG|Quick Access") int32 FindSlotForItemId(FName ItemId) const;
    UFUNCTION(BlueprintPure, Category="ARPG|Quick Access") int32 FindSlotForItemInstance(FGuid ItemInstanceId) const;

    // Persistence hook. Load Inventory first, then replace quick-access state so bindings repair against real runtime entries.
    UFUNCTION(BlueprintCallable, Category="ARPG|Quick Access|Persistence", meta=(BlueprintAuthorityOnly)) void ReplaceQuickAccessState(const TArray<FARPGQuickAccessSlot>& NewSlots, int32 NewActiveSlotNumber);
    UFUNCTION(BlueprintCallable, Category="ARPG|Quick Access", meta=(BlueprintAuthorityOnly)) void RefreshRuntimeBindings();

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
    UFUNCTION() void OnRep_QuickAccessSlots();
    UFUNCTION() void OnRep_ActiveSlotNumber();
    UFUNCTION() void HandleInventoryChanged();

    UFUNCTION(Server, Reliable) void ServerAssignItemToSlot(int32 SlotNumber, FGuid ItemInstanceId);
    UFUNCTION(Server, Reliable) void ServerAssignItemIdToSlot(int32 SlotNumber, FName ItemId);
    UFUNCTION(Server, Reliable) void ServerClearSlot(int32 SlotNumber);
    UFUNCTION(Server, Reliable) void ServerSwapSlots(int32 FirstSlotNumber, int32 SecondSlotNumber);
    UFUNCTION(Server, Reliable) void ServerClearAllSlots();
    UFUNCTION(Server, Reliable) void ServerSelectSlot(int32 SlotNumber);
    UFUNCTION(Server, Reliable) void ServerActivateSlot(int32 SlotNumber);
    UFUNCTION(Server, Reliable) void ServerUseActiveSlot();
    UFUNCTION(Server, Reliable) void ServerCycleSlot(int32 Direction);

    UFUNCTION(Client, Reliable) void ClientReceiveActionResult(EARPGQuickAccessResult Result, int32 SlotNumber, FGuid ItemInstanceId);
    UFUNCTION(NetMulticast, Unreliable) void MulticastPlayItemUsePresentation(int32 SlotNumber, FGuid ItemInstanceId, UARPGItemDefinition* Definition);

    bool IsValidSlotNumber(int32 SlotNumber) const;
    int32 ToIndex(int32 SlotNumber) const { return SlotNumber - 1; }
    UARPGInventoryComponent* GetInventory() const;
    UARPGEquipmentComponent* GetEquipment() const;
    const FARPGInventoryEntry* ResolveOwnedEntry(const FARPGQuickAccessSlot& Slot) const;
    const FARPGInventoryEntry* FindOwnedEntryById(FName ItemId) const;
    const FARPGInventoryEntry* FindOwnedEntryByIdExcluding(FName ItemId, const TSet<FGuid>& ExcludedInstanceIds) const;
    EARPGQuickAccessAction ResolveAction(const UARPGItemDefinition* Definition) const;
    float GetServerTimeSeconds() const;

    void EnsureSlotArrayAuthority();
    void RebuildAssignmentRevisionCounterAuthority();
    bool RepairRuntimeBindingsAuthority(int32 PreferredSlotNumber = 0);
    bool IsCanonicalSlotForView(int32 SlotNumber) const;
    bool AssignItemToSlotAuthority(int32 SlotNumber, FGuid ItemInstanceId);
    bool AssignItemIdToSlotAuthority(int32 SlotNumber, FName ItemId);
    bool ClearSlotAuthority(int32 SlotNumber);
    bool SwapSlotsAuthority(int32 FirstSlotNumber, int32 SecondSlotNumber);
    bool ClearAllSlotsAuthority();
    EARPGQuickAccessResult SelectSlotAuthority(int32 SlotNumber, FGuid& OutInstanceId);
    EARPGQuickAccessResult ActivateSlotAuthority(int32 SlotNumber, FGuid& OutInstanceId);
    EARPGQuickAccessResult UseSlotAuthority(int32 SlotNumber, FGuid& OutInstanceId);
    EARPGQuickAccessResult CycleSlotAuthority(int32 Direction, int32& OutSlotNumber, FGuid& OutInstanceId);
    void CaptureTrackedQuickAccessEquipmentFromSlotAuthority(int32 SlotNumber);
    bool UnequipPreviousQuickAccessEquipmentAuthority(FGuid NewItemInstanceId, FGuid PreviousActiveInstanceId);
    void SetActiveSlotAuthority(int32 SlotNumber);
    void SendActionResult(EARPGQuickAccessResult Result, int32 SlotNumber, FGuid ItemInstanceId);
    void BroadcastActiveSlotChangedLocal();
    void PlayItemUsePresentationLocal(int32 SlotNumber, FGuid ItemInstanceId, const UARPGItemDefinition* Definition);

    // Authoritative anti-spam state. The replicated slot end-times are the UI projection of this map.
    TMap<FName, float> CooldownEndByItemId;

    // Server-only monotonic counter used to make duplicate repair deterministic. The per-slot revision is replicated/saved.
    int32 NextAssignmentRevision = 1;

    // Server-only equipment handoff state. It intentionally survives hotbar reassignment/clearing so replacing the
    // currently active slot cannot orphan the old held weapon/tool before the new slot is activated.
    FGuid LastQuickAccessEquippedInstanceId;
};
