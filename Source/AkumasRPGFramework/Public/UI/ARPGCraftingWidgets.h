#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Data/ARPGRecipeDefinition.h"
#include "ARPGCraftingWidgets.generated.h"

class AARPGCharacter;
class UARPGCraftingComponent;
class UARPGInventoryUIComponent;
class UARPGItemDefinition;
class UARPGRecipeDefinition;
class UButton;
class UImage;
class UProgressBar;
class UScrollBox;
class UTextBlock;
class UVerticalBox;
class UWidgetSwitcher;

UENUM(BlueprintType)
enum class EARPGCraftingUIMode : uint8
{
    Craft,
    Repair
};

USTRUCT(BlueprintType)
struct AKUMASRPGFRAMEWORK_API FARPGCraftingRecipeView
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly, Category="ARPG|Crafting UI") TObjectPtr<UARPGRecipeDefinition> Recipe = nullptr;
    UPROPERTY(BlueprintReadOnly, Category="ARPG|Crafting UI") FText DisplayName;
    UPROPERTY(BlueprintReadOnly, Category="ARPG|Crafting UI") FText Description;
    UPROPERTY(BlueprintReadOnly, Category="ARPG|Crafting UI") FName Category = NAME_None;
    UPROPERTY(BlueprintReadOnly, Category="ARPG|Crafting UI") bool bCanCraft = false;
    UPROPERTY(BlueprintReadOnly, Category="ARPG|Crafting UI") int32 MaxCraftable = 0;
    UPROPERTY(BlueprintReadOnly, Category="ARPG|Crafting UI") FText InputSummary;
    UPROPERTY(BlueprintReadOnly, Category="ARPG|Crafting UI") FText OutputSummary;
    UPROPERTY(BlueprintReadOnly, Category="ARPG|Crafting UI") FText RequirementSummary;
    UPROPERTY(BlueprintReadOnly, Category="ARPG|Crafting UI") FText FailureReason;
    UPROPERTY(BlueprintReadOnly, Category="ARPG|Crafting UI") float CraftSeconds = 0.f;
};

USTRUCT(BlueprintType)
struct AKUMASRPGFRAMEWORK_API FARPGRepairItemView
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly, Category="ARPG|Crafting UI") FGuid ItemInstanceId;
    UPROPERTY(BlueprintReadOnly, Category="ARPG|Crafting UI") TObjectPtr<UARPGItemDefinition> ItemDefinition = nullptr;
    UPROPERTY(BlueprintReadOnly, Category="ARPG|Crafting UI") FText DisplayName;
    UPROPERTY(BlueprintReadOnly, Category="ARPG|Crafting UI") bool bEquipped = false;
    UPROPERTY(BlueprintReadOnly, Category="ARPG|Crafting UI") bool bBroken = false;
    UPROPERTY(BlueprintReadOnly, Category="ARPG|Crafting UI") bool bCanRepair = false;
    UPROPERTY(BlueprintReadOnly, Category="ARPG|Crafting UI") float CurrentDurability = 0.f;
    UPROPERTY(BlueprintReadOnly, Category="ARPG|Crafting UI") float MaxDurability = 0.f;
    UPROPERTY(BlueprintReadOnly, Category="ARPG|Crafting UI") float DurabilityPercent = 0.f;
    UPROPERTY(BlueprintReadOnly, Category="ARPG|Crafting UI") FText RepairCostSummary;
    UPROPERTY(BlueprintReadOnly, Category="ARPG|Crafting UI") FText FailureReason;
};

class UARPGCraftingPanelWidget;

UCLASS(BlueprintType, Blueprintable)
class AKUMASRPGFRAMEWORK_API UARPGCraftingRecipeRowWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="ARPG|Crafting UI") void InitializeRecipeRow(UARPGCraftingPanelWidget* InOwnerPanel, const FARPGCraftingRecipeView& InView);
    UFUNCTION(BlueprintCallable, Category="ARPG|Crafting UI") void SetRecipeView(const FARPGCraftingRecipeView& InView);
    UFUNCTION(BlueprintPure, Category="ARPG|Crafting UI") FARPGCraftingRecipeView GetRecipeView() const { return View; }
    UFUNCTION(BlueprintImplementableEvent, Category="ARPG|Crafting UI", meta=(DisplayName="On ARPG Crafting Recipe Row Updated")) void BP_OnRecipeRowUpdated(FARPGCraftingRecipeView RecipeView);

    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Crafting UI|Bindings") TObjectPtr<UButton> RecipeButton;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Crafting UI|Bindings") TObjectPtr<UImage> RecipeIcon;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Crafting UI|Bindings") TObjectPtr<UTextBlock> RecipeNameText;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Crafting UI|Bindings") TObjectPtr<UTextBlock> RecipeAvailabilityText;
protected:
    virtual void NativeOnInitialized() override;
private:
    UPROPERTY(Transient) TObjectPtr<UARPGCraftingPanelWidget> OwnerPanel = nullptr;
    /** Reflected so the recipe UObject inside the snapshot stays GC-referenced while the row is alive. */
    UPROPERTY(Transient) FARPGCraftingRecipeView View;
    UFUNCTION() void HandleClicked();
    void EnsureNativeLayoutOrBindings();
    void ApplyView();
};

UCLASS(BlueprintType, Blueprintable)
class AKUMASRPGFRAMEWORK_API UARPGRepairItemRowWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="ARPG|Crafting UI") void InitializeRepairRow(UARPGCraftingPanelWidget* InOwnerPanel, const FARPGRepairItemView& InView);
    UFUNCTION(BlueprintCallable, Category="ARPG|Crafting UI") void SetRepairView(const FARPGRepairItemView& InView);
    UFUNCTION(BlueprintPure, Category="ARPG|Crafting UI") FARPGRepairItemView GetRepairView() const { return View; }
    UFUNCTION(BlueprintImplementableEvent, Category="ARPG|Crafting UI", meta=(DisplayName="On ARPG Repair Item Row Updated")) void BP_OnRepairRowUpdated(FARPGRepairItemView RepairView);

    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Crafting UI|Bindings") TObjectPtr<UButton> RepairItemButton;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Crafting UI|Bindings") TObjectPtr<UImage> RepairItemIcon;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Crafting UI|Bindings") TObjectPtr<UTextBlock> RepairItemNameText;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Crafting UI|Bindings") TObjectPtr<UProgressBar> RepairDurabilityBar;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Crafting UI|Bindings") TObjectPtr<UTextBlock> RepairDurabilityText;
protected:
    virtual void NativeOnInitialized() override;
private:
    UPROPERTY(Transient) TObjectPtr<UARPGCraftingPanelWidget> OwnerPanel = nullptr;
    /** Reflected so the item-definition UObject inside the snapshot stays GC-referenced while the row is alive. */
    UPROPERTY(Transient) FARPGRepairItemView View;
    UFUNCTION() void HandleClicked();
    void EnsureNativeLayoutOrBindings();
    void ApplyView();
};

/** Ready Crafting + Repair page designed to live inside the shared Inventory shell. */
UCLASS(BlueprintType, Blueprintable)
class AKUMASRPGFRAMEWORK_API UARPGCraftingPanelWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="ARPG|Crafting UI") void InitializeCraftingUI(AARPGCharacter* InCharacter, UARPGInventoryUIComponent* InInventoryUI);
    UFUNCTION(BlueprintCallable, Category="ARPG|Crafting UI") void RefreshCraftingUI();
    /** Lightweight timer refresh: updates only active craft progress/button state, never rebuilds lists. */
    UFUNCTION(BlueprintCallable, Category="ARPG|Crafting UI") void RefreshCraftingProgress();
    UFUNCTION(BlueprintCallable, Category="ARPG|Crafting UI") void SetMode(EARPGCraftingUIMode NewMode);
    UFUNCTION(BlueprintPure, Category="ARPG|Crafting UI") EARPGCraftingUIMode GetMode() const { return Mode; }
    UFUNCTION(BlueprintCallable, Category="ARPG|Crafting UI") void SelectRecipe(UARPGRecipeDefinition* Recipe);
    UFUNCTION(BlueprintCallable, Category="ARPG|Crafting UI") void SelectRepairItem(FGuid ItemInstanceId);
    UFUNCTION(BlueprintPure, Category="ARPG|Crafting UI") int32 GetCraftQuantity() const { return CraftQuantity; }

    UFUNCTION(BlueprintImplementableEvent, Category="ARPG|Crafting UI", meta=(DisplayName="On ARPG Crafting UI Refreshed")) void BP_OnCraftingUIRefreshed();
    UFUNCTION(BlueprintImplementableEvent, Category="ARPG|Crafting UI", meta=(DisplayName="On ARPG Crafting Recipe Selected")) void BP_OnCraftingRecipeSelected(FARPGCraftingRecipeView RecipeView);
    UFUNCTION(BlueprintImplementableEvent, Category="ARPG|Crafting UI", meta=(DisplayName="On ARPG Repair Item Selected")) void BP_OnRepairItemSelected(FARPGRepairItemView RepairView);

    // Standard child names for zero-graph reskinning.
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Crafting UI|Bindings") TObjectPtr<UButton> CraftModeButton;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Crafting UI|Bindings") TObjectPtr<UButton> RepairModeButton;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Crafting UI|Bindings") TObjectPtr<UWidgetSwitcher> CraftingModeSwitcher;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Crafting UI|Bindings") TObjectPtr<UVerticalBox> RecipeListBox;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Crafting UI|Bindings") TObjectPtr<UVerticalBox> RepairListBox;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Crafting UI|Bindings") TObjectPtr<UTextBlock> CraftingTitleText;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Crafting UI|Bindings") TObjectPtr<UTextBlock> SelectedRecipeNameText;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Crafting UI|Bindings") TObjectPtr<UTextBlock> SelectedRecipeDetailsText;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Crafting UI|Bindings") TObjectPtr<UTextBlock> CraftQuantityText;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Crafting UI|Bindings") TObjectPtr<UButton> CraftMinusButton;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Crafting UI|Bindings") TObjectPtr<UButton> CraftPlusButton;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Crafting UI|Bindings") TObjectPtr<UButton> CraftMaxButton;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Crafting UI|Bindings") TObjectPtr<UButton> CraftButton;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Crafting UI|Bindings") TObjectPtr<UButton> CancelCraftButton;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Crafting UI|Bindings") TObjectPtr<UTextBlock> CraftButtonText;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Crafting UI|Bindings") TObjectPtr<UProgressBar> CraftProgressBar;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Crafting UI|Bindings") TObjectPtr<UTextBlock> CraftProgressText;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Crafting UI|Bindings") TObjectPtr<UTextBlock> SelectedRepairNameText;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Crafting UI|Bindings") TObjectPtr<UTextBlock> SelectedRepairDetailsText;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Crafting UI|Bindings") TObjectPtr<UProgressBar> SelectedRepairDurabilityBar;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Crafting UI|Bindings") TObjectPtr<UButton> RepairButton;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Crafting UI|Bindings") TObjectPtr<UTextBlock> RepairButtonText;

protected:
    virtual void NativeOnInitialized() override;

private:
    UPROPERTY(Transient) TObjectPtr<AARPGCharacter> ObservedCharacter = nullptr;
    UPROPERTY(Transient) TObjectPtr<UARPGInventoryUIComponent> InventoryUI = nullptr;
    UPROPERTY(Transient) TObjectPtr<UARPGRecipeDefinition> SelectedRecipe = nullptr;
    FGuid SelectedRepairItemId;
    /** These snapshots carry UObject references, so keep them reflected for GC safety in custom/reskinned widgets. */
    UPROPERTY(Transient) FARPGCraftingRecipeView SelectedRecipeView;
    UPROPERTY(Transient) FARPGRepairItemView SelectedRepairView;
    EARPGCraftingUIMode Mode = EARPGCraftingUIMode::Craft;
    int32 CraftQuantity = 1;
    UPROPERTY(Transient) TArray<TObjectPtr<UARPGCraftingRecipeRowWidget>> RuntimeRecipeRows;
    UPROPERTY(Transient) TArray<TObjectPtr<UARPGRepairItemRowWidget>> RuntimeRepairRows;

    UFUNCTION() void HandleCraftModeClicked();
    UFUNCTION() void HandleRepairModeClicked();
    UFUNCTION() void HandleCraftMinusClicked();
    UFUNCTION() void HandleCraftPlusClicked();
    UFUNCTION() void HandleCraftMaxClicked();
    UFUNCTION() void HandleCraftClicked();
    UFUNCTION() void HandleCancelCraftClicked();
    UFUNCTION() void HandleRepairClicked();

    void EnsureNativeLayoutOrBindings();
    void BindButtons();
    void RebuildRecipeList();
    void RebuildRepairList();
    bool BuildRecipeView(UARPGRecipeDefinition* Recipe, FARPGCraftingRecipeView& OutView) const;
    bool BuildRepairView(FGuid ItemInstanceId, FARPGRepairItemView& OutView) const;
    void RefreshSelectedRecipe();
    void RefreshSelectedRepair();
    void ApplyModeVisibility();
};
