#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TimerManager.h"
#include "UI/ARPGInventoryWidgets.h"
#include "ARPGInventoryUIComponent.generated.h"

class AARPGCharacter;
class APlayerController;
class UARPGInventoryComponent;
class UARPGQuickAccessComponent;
class UARPGItemUseComponent;

/**
 * Local player Inventory + Quick Access UI owner.
 * Gameplay ownership/mutation stays in the existing replicated Inventory, Equipment and Quick Access components.
 */
UCLASS(ClassGroup=(ARPG), BlueprintType, Blueprintable, meta=(BlueprintSpawnableComponent))
class AKUMASRPGFRAMEWORK_API UARPGInventoryUIComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UARPGInventoryUIComponent();

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Inventory UI|Widgets", meta=(DisplayName="Inventory Widget Class"))
    TSubclassOf<UARPGInventoryPanelWidget> InventoryWidgetClass;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Inventory UI|Widgets", meta=(DisplayName="Quick Access Widget Class"))
    TSubclassOf<UARPGQuickAccessBarWidget> QuickAccessWidgetClass;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Inventory UI|Widgets", meta=(DisplayName="Inventory Slot Widget Class"))
    TSubclassOf<UARPGInventoryItemSlotWidget> InventorySlotWidgetClass;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Inventory UI|Widgets", meta=(DisplayName="Quick Access Slot Widget Class"))
    TSubclassOf<UARPGInventoryItemSlotWidget> QuickAccessSlotWidgetClass;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Inventory UI|Inventory", meta=(ClampMin="1", ClampMax="12")) int32 InventoryColumns = 8;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Inventory UI|Inventory", meta=(ClampMin="48.0", ClampMax="160.0")) float InventorySlotSize = 86.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Inventory UI|Quick Access", meta=(DisplayName="Automatically Create Quick Access HUD")) bool bAutoCreateQuickAccessHUD = true;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Inventory UI|Quick Access") bool bShowQuickAccessHUD = true;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Inventory UI|Quick Access", meta=(ClampMin="48.0", ClampMax="160.0")) float QuickAccessSlotSize = 82.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Inventory UI|Quick Access", meta=(DisplayName="Unequip Active Item When Dragged Off Quick Access")) bool bUnequipActiveItemWhenDraggedOff = true;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Inventory UI|Layering", meta=(ClampMin="0", ClampMax="10000")) int32 InventoryZOrder = 80;
    /** Defaults above Inventory so inventory-to-hotbar drag/drop remains interactable while the panel is open. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Inventory UI|Layering", meta=(ClampMin="0", ClampMax="10000")) int32 QuickAccessZOrder = 100;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Inventory UI|Input") bool bManageInputMode = true;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Inventory UI|Input", meta=(EditCondition="bManageInputMode")) bool bShowMouseCursorWhileOpen = true;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Inventory UI|Input", meta=(EditCondition="bManageInputMode")) bool bRestoreGameOnlyInputOnClose = true;

    /** Cooldown visuals are event-driven except while at least one visible Quick Access slot is actually cooling down. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Inventory UI|Runtime", meta=(ClampMin="0.05", ClampMax="1.0", Units="s")) float CooldownRefreshInterval = 0.10f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="Inventory UI|Runtime") TObjectPtr<UARPGInventoryPanelWidget> ActiveInventoryWidget;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="Inventory UI|Runtime") TObjectPtr<UARPGQuickAccessBarWidget> ActiveQuickAccessWidget;

    UFUNCTION(BlueprintCallable, Category="ARPG|Inventory UI") bool OpenInventoryUI();
    UFUNCTION(BlueprintCallable, Category="ARPG|Inventory UI") bool CloseInventoryUI();
    UFUNCTION(BlueprintCallable, Category="ARPG|Inventory UI") bool ToggleInventoryUI();
    UFUNCTION(BlueprintPure, Category="ARPG|Inventory UI") bool IsInventoryUIOpen() const;
    UFUNCTION(BlueprintCallable, Category="ARPG|Inventory UI") bool EnsureQuickAccessUI();
    UFUNCTION(BlueprintCallable, Category="ARPG|Inventory UI") void SetQuickAccessHUDVisible(bool bShouldShow);
    UFUNCTION(BlueprintCallable, Category="ARPG|Inventory UI") void RefreshInventoryUI();
    UFUNCTION(BlueprintCallable, Category="ARPG|Inventory UI") void RefreshQuickAccessUI();

    UFUNCTION(BlueprintPure, Category="ARPG|Inventory UI") int32 GetInventoryDisplaySlotCount() const;
    UFUNCTION(BlueprintPure, Category="ARPG|Inventory UI") int32 GetQuickAccessDisplaySlotCount() const;
    UFUNCTION(BlueprintPure, Category="ARPG|Inventory UI") bool GetInventorySlotView(int32 SlotNumber, FARPGInventoryUISlotView& OutView) const;
    UFUNCTION(BlueprintPure, Category="ARPG|Inventory UI") bool GetQuickAccessSlotView(int32 SlotNumber, FARPGInventoryUISlotView& OutView) const;

    UFUNCTION(BlueprintCallable, Category="ARPG|Inventory UI|Actions") bool AssignInventoryItemToQuickAccess(FGuid ItemInstanceId, int32 TargetQuickAccessSlot);
    UFUNCTION(BlueprintCallable, Category="ARPG|Inventory UI|Actions") bool SwapQuickAccessSlots(int32 FirstSlotNumber, int32 SecondSlotNumber);
    UFUNCTION(BlueprintCallable, Category="ARPG|Inventory UI|Actions") bool ClearQuickAccessSlot(int32 SlotNumber, bool bUnequipIfActive=true);
    UFUNCTION(BlueprintCallable, Category="ARPG|Inventory UI|Actions") bool ActivateQuickAccessSlot(int32 SlotNumber);
    /** Context-sensitive primary action: equippable -> equip/unequip, usable -> use directly from Inventory. */
    UFUNCTION(BlueprintCallable, Category="ARPG|Inventory UI|Actions") bool ActivateInventoryItem(FGuid ItemInstanceId);
    /** Directly uses a usable item without requiring a Quick Access assignment. */
    UFUNCTION(BlueprintCallable, Category="ARPG|Inventory UI|Actions") bool UseInventoryItem(FGuid ItemInstanceId);
    /** Local UI preflight used to disable/reject unusable full-vital consumables before an RPC is sent. */
    UFUNCTION(BlueprintPure, Category="ARPG|Inventory UI|Actions") bool CanUseInventoryItemNow(FGuid ItemInstanceId) const;
    UFUNCTION(BlueprintCallable, Category="ARPG|Inventory UI|Actions") bool ToggleEquipInventoryItem(FGuid ItemInstanceId);
    UFUNCTION(BlueprintCallable, Category="ARPG|Inventory UI|Actions") void SelectInventorySlot(int32 SlotNumber);

    /** Called by AARPGCharacter possession/client-restart hooks so the hotbar appears reliably after local possession. */
    void HandleOwnerControlChanged();

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    FTimerHandle CooldownRefreshTimer;
    bool bPreviousMouseCursor = false;
    bool bEventsBound = false;
    TWeakObjectPtr<APlayerController> CachedLocalPlayerController;

    UFUNCTION() void HandleInventoryChanged();
    UFUNCTION() void HandleQuickAccessChanged();
    UFUNCTION() void HandleActiveQuickAccessSlotChanged(int32 SlotNumber, FName ItemId, FGuid ItemInstanceId);
    UFUNCTION() void HandleItemUseCooldownsChanged();

    bool ResolveLocalPlayer(AARPGCharacter*& OutCharacter, APlayerController*& OutPlayerController) const;
    UARPGInventoryComponent* GetInventory() const;
    UARPGQuickAccessComponent* GetQuickAccess() const;
    UARPGItemUseComponent* GetItemUse() const;
    void BindRuntimeEvents();
    void UnbindRuntimeEvents();
    void UpdateCooldownRefreshTimer();
    void HandleCooldownRefreshTick();
    void StopCooldownRefreshTimer();
    void ApplyOpenInputMode(APlayerController* PlayerController);
    void RestoreClosedInputMode(APlayerController* PlayerController);
    bool CloseInventoryUIInternal(bool bRestoreInputMode);
};
