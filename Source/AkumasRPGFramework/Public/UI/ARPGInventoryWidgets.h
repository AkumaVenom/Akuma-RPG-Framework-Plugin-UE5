#pragma once

#include "CoreMinimal.h"
#include "Blueprint/DragDropOperation.h"
#include "Blueprint/UserWidget.h"
#include "ARPGTypes.h"
#include "UI/ARPGCraftingWidgets.h"
#include "ARPGInventoryWidgets.generated.h"

class AARPGCharacter;
class UARPGInventoryUIComponent;
class UARPGItemDefinition;
class UBorder;
class UButton;
class UHorizontalBox;
class UImage;
class UProgressBar;
class UTextBlock;
class UUniformGridPanel;
class UWidgetSwitcher;
class USizeBox;

UENUM(BlueprintType)
enum class EARPGItemManagementTab : uint8
{
    Inventory,
    Crafting
};

UENUM(BlueprintType)
enum class EARPGInventoryUISlotSource : uint8
{
    Inventory,
    QuickAccess
};

/** Blueprint-friendly presentation view used by the ready Inventory and Quick Access widgets. */
USTRUCT(BlueprintType)
struct AKUMASRPGFRAMEWORK_API FARPGInventoryUISlotView
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="ARPG|Inventory UI") EARPGInventoryUISlotSource Source = EARPGInventoryUISlotSource::Inventory;
    /** Inventory display slot or Quick Access slot. Both are intentionally 1-based for easy Blueprint authoring. */
    UPROPERTY(BlueprintReadOnly, Category="ARPG|Inventory UI") int32 SlotNumber = 0;
    UPROPERTY(BlueprintReadOnly, Category="ARPG|Inventory UI") bool bOccupied = false;
    UPROPERTY(BlueprintReadOnly, Category="ARPG|Inventory UI") bool bOwned = false;
    UPROPERTY(BlueprintReadOnly, Category="ARPG|Inventory UI") bool bActive = false;
    UPROPERTY(BlueprintReadOnly, Category="ARPG|Inventory UI") bool bEquipped = false;
    UPROPERTY(BlueprintReadOnly, Category="ARPG|Inventory UI") bool bBound = false;
    UPROPERTY(BlueprintReadOnly, Category="ARPG|Inventory UI") FName ItemId = NAME_None;
    UPROPERTY(BlueprintReadOnly, Category="ARPG|Inventory UI") FGuid ItemInstanceId;
    UPROPERTY(BlueprintReadOnly, Category="ARPG|Inventory UI") int32 Quantity = 0;
    UPROPERTY(BlueprintReadOnly, Category="ARPG|Inventory UI") float Durability = 0.f;
    UPROPERTY(BlueprintReadOnly, Category="ARPG|Inventory UI") float MaxDurability = 0.f;
    UPROPERTY(BlueprintReadOnly, Category="ARPG|Inventory UI") float DurabilityPercent = 0.f;
    UPROPERTY(BlueprintReadOnly, Category="ARPG|Inventory UI") bool bUsesDurability = false;
    UPROPERTY(BlueprintReadOnly, Category="ARPG|Inventory UI") bool bBroken = false;
    UPROPERTY(BlueprintReadOnly, Category="ARPG|Inventory UI") FGameplayTag EquipmentSlot;
    UPROPERTY(BlueprintReadOnly, Category="ARPG|Inventory UI") TObjectPtr<UARPGItemDefinition> ItemDefinition = nullptr;
    UPROPERTY(BlueprintReadOnly, Category="ARPG|Inventory UI") FText DisplayName;
    UPROPERTY(BlueprintReadOnly, Category="ARPG|Inventory UI") FText Description;
    UPROPERTY(BlueprintReadOnly, Category="ARPG|Inventory UI") EARPGRarity Rarity = EARPGRarity::Common;
    UPROPERTY(BlueprintReadOnly, Category="ARPG|Inventory UI") EARPGQuickAccessAction ResolvedQuickAccessAction = EARPGQuickAccessAction::SelectOnly;
    UPROPERTY(BlueprintReadOnly, Category="ARPG|Inventory UI") float CooldownRemaining = 0.f;
    UPROPERTY(BlueprintReadOnly, Category="ARPG|Inventory UI") float CooldownPercent = 0.f;
};

/** Typed drag payload shared by Inventory and Quick Access slots. */
UCLASS(BlueprintType)
class AKUMASRPGFRAMEWORK_API UARPGInventoryDragDropOperation : public UDragDropOperation
{
    GENERATED_BODY()
public:
    UPROPERTY(BlueprintReadOnly, Category="ARPG|Inventory UI|Drag Drop") EARPGInventoryUISlotSource Source = EARPGInventoryUISlotSource::Inventory;
    UPROPERTY(BlueprintReadOnly, Category="ARPG|Inventory UI|Drag Drop") int32 SourceSlotNumber = 0;
    UPROPERTY(BlueprintReadOnly, Category="ARPG|Inventory UI|Drag Drop") FGuid ItemInstanceId;
    UPROPERTY(BlueprintReadOnly, Category="ARPG|Inventory UI|Drag Drop") FName ItemId = NAME_None;
    UPROPERTY(BlueprintReadOnly, Category="ARPG|Inventory UI|Drag Drop") FARPGInventoryUISlotView SourceView;
    UPROPERTY(Transient) TObjectPtr<UARPGInventoryUIComponent> InventoryUI = nullptr;
    UPROPERTY(Transient) bool bDropHandled = false;
};

/**
 * Ready item/quick-slot visual with native drag/drop behavior.
 * Blueprint subclasses can replace the visuals while keeping the same framework interaction logic.
 */
UCLASS(BlueprintType, Blueprintable)
class AKUMASRPGFRAMEWORK_API UARPGInventoryItemSlotWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="ARPG|Inventory UI")
    void InitializeInventorySlot(UARPGInventoryUIComponent* InInventoryUI, EARPGInventoryUISlotSource InSource, int32 InSlotNumber);

    UFUNCTION(BlueprintCallable, Category="ARPG|Inventory UI") void SetSlotView(const FARPGInventoryUISlotView& InView);
    UFUNCTION(BlueprintPure, Category="ARPG|Inventory UI") FARPGInventoryUISlotView GetSlotView() const { return SlotView; }

    UFUNCTION(BlueprintImplementableEvent, Category="ARPG|Inventory UI", meta=(DisplayName="On ARPG Inventory Slot Updated"))
    void BP_OnInventorySlotUpdated(FARPGInventoryUISlotView View);

    // Standard zero-graph child bindings for custom Widget Blueprint subclasses.
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Inventory UI|Bindings") TObjectPtr<UBorder> SlotBorder;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Inventory UI|Bindings") TObjectPtr<UImage> ItemIcon;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Inventory UI|Bindings") TObjectPtr<UTextBlock> QuantityText;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Inventory UI|Bindings") TObjectPtr<UTextBlock> SlotNumberText;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Inventory UI|Bindings") TObjectPtr<UTextBlock> ItemNameText;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Inventory UI|Bindings") TObjectPtr<UTextBlock> EquippedText;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Inventory UI|Bindings") TObjectPtr<UProgressBar> CooldownBar;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Inventory UI|Bindings") TObjectPtr<UProgressBar> DurabilityBar;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Inventory UI|Bindings") TObjectPtr<UTextBlock> BrokenText;

protected:
    virtual void NativeOnInitialized() override;
    // Inventory slots live inside a ScrollBox. Preview/tunneling input guarantees the slot
    // can arm selection/drag before the scrolling container sees the pointer press.
    virtual FReply NativeOnPreviewMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual void NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation) override;
    virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
    virtual void NativeOnDragCancelled(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;

private:
    UPROPERTY(Transient) TObjectPtr<UARPGInventoryUIComponent> InventoryUI = nullptr;
    FARPGInventoryUISlotView SlotView;
    bool bDragVisualOnly = false;
    bool bPointerPressed = false;

    void EnsureNativeLayoutOrBindings();
    void ApplyViewToStandardFields();
    void InitializeAsDragVisual(const FARPGInventoryUISlotView& InView);
    FLinearColor ResolveRarityColor() const;
};

/** Ready-to-use full Inventory panel with capacity, grid, selected-item detail and Close button. */
UCLASS(BlueprintType, Blueprintable)
class AKUMASRPGFRAMEWORK_API UARPGInventoryPanelWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="ARPG|Inventory UI")
    void InitializeInventoryUI(AARPGCharacter* InCharacter, UARPGInventoryUIComponent* InInventoryUIComponent);
    UFUNCTION(BlueprintCallable, Category="ARPG|Inventory UI") void RefreshInventoryUI();
    UFUNCTION(BlueprintCallable, Category="ARPG|Inventory UI") void RequestCloseInventoryUI();
    UFUNCTION(BlueprintCallable, Category="ARPG|Inventory UI") void SetSelectedSlotView(const FARPGInventoryUISlotView& InView);
    UFUNCTION(BlueprintCallable, Category="ARPG|Inventory UI") void SetActiveTab(EARPGItemManagementTab NewTab);
    UFUNCTION(BlueprintPure, Category="ARPG|Inventory UI") EARPGItemManagementTab GetActiveTab() const { return ActiveTab; }
    UFUNCTION(BlueprintCallable, Category="ARPG|Inventory UI") void RefreshCraftingUI();
    UFUNCTION(BlueprintPure, Category="ARPG|Inventory UI") FARPGInventoryUISlotView GetSelectedSlotView() const { return SelectedSlotView; }

    UFUNCTION(BlueprintImplementableEvent, Category="ARPG|Inventory UI", meta=(DisplayName="On ARPG Inventory UI Refreshed"))
    void BP_OnInventoryUIRefreshed();
    UFUNCTION(BlueprintImplementableEvent, Category="ARPG|Inventory UI", meta=(DisplayName="On ARPG Inventory Selection Changed"))
    void BP_OnInventorySelectionChanged(FARPGInventoryUISlotView SelectedView);
    UFUNCTION(BlueprintImplementableEvent, Category="ARPG|Inventory UI", meta=(DisplayName="On ARPG Item Management Tab Changed"))
    void BP_OnItemManagementTabChanged(EARPGItemManagementTab NewTab);

    // Standard bindings for custom Inventory Widget Blueprints.
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Inventory UI|Bindings") TObjectPtr<UUniformGridPanel> InventoryGrid;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Inventory UI|Bindings") TObjectPtr<UTextBlock> CapacityText;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Inventory UI|Bindings") TObjectPtr<UTextBlock> SelectedItemNameText;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Inventory UI|Bindings") TObjectPtr<UTextBlock> SelectedItemDetailsText;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Inventory UI|Bindings") TObjectPtr<UButton> CloseButton;
    /** Context-sensitive ready action: Equip/Unequip for equipment, Use for usable items. */
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Inventory UI|Bindings") TObjectPtr<UButton> PrimaryActionButton;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Inventory UI|Bindings") TObjectPtr<UTextBlock> PrimaryActionText;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Inventory UI|Bindings") TObjectPtr<UButton> InventoryTabButton;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Inventory UI|Bindings") TObjectPtr<UButton> CraftingTabButton;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Inventory UI|Bindings") TObjectPtr<UWidgetSwitcher> MainTabSwitcher;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Inventory UI|Bindings") TObjectPtr<USizeBox> CraftingPageHost;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Inventory UI|Bindings") TObjectPtr<UARPGCraftingPanelWidget> CraftingPanel;

protected:
    virtual void NativeOnInitialized() override;
    virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;

private:
    UPROPERTY(Transient) TObjectPtr<AARPGCharacter> ObservedCharacter = nullptr;
    UPROPERTY(Transient) TObjectPtr<UARPGInventoryUIComponent> InventoryUI = nullptr;
    FARPGInventoryUISlotView SelectedSlotView;
    TArray<TObjectPtr<UARPGInventoryItemSlotWidget>> RuntimeSlots;
    EARPGItemManagementTab ActiveTab = EARPGItemManagementTab::Inventory;

    UFUNCTION() void HandleCloseClicked();
    UFUNCTION() void HandleInventoryTabClicked();
    UFUNCTION() void HandleCraftingTabClicked();
    UFUNCTION() void HandlePrimaryActionClicked();
    void EnsureNativeLayoutOrBindings();
    void RebuildInventoryGrid();
    void UpdateSelectionText();
    void EnsureCraftingPanel();
    void ApplyActiveTab();
};

/** Always-available local Quick Access HUD. Drag inventory items here, rearrange slots, or drag a slot away to clear it. */
UCLASS(BlueprintType, Blueprintable)
class AKUMASRPGFRAMEWORK_API UARPGQuickAccessBarWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="ARPG|Inventory UI")
    void InitializeQuickAccessUI(AARPGCharacter* InCharacter, UARPGInventoryUIComponent* InInventoryUIComponent);
    UFUNCTION(BlueprintCallable, Category="ARPG|Inventory UI") void RefreshQuickAccessUI();

    UFUNCTION(BlueprintImplementableEvent, Category="ARPG|Inventory UI", meta=(DisplayName="On ARPG Quick Access UI Refreshed"))
    void BP_OnQuickAccessUIRefreshed();

    /** Standard child binding for custom hotbar Widget Blueprints. */
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Inventory UI|Bindings") TObjectPtr<UHorizontalBox> QuickAccessBox;

protected:
    virtual void NativeOnInitialized() override;

private:
    UPROPERTY(Transient) TObjectPtr<AARPGCharacter> ObservedCharacter = nullptr;
    UPROPERTY(Transient) TObjectPtr<UARPGInventoryUIComponent> InventoryUI = nullptr;
    TArray<TObjectPtr<UARPGInventoryItemSlotWidget>> RuntimeSlots;

    void EnsureNativeLayoutOrBindings();
    void RebuildQuickAccessSlots();
};
