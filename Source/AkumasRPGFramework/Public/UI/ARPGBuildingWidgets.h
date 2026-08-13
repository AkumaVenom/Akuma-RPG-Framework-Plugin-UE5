#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ARPGTypes.h"
#include "ARPGBuildingWidgets.generated.h"

class AARPGCharacter;
class AARPGStorageActor;
class AARPGCraftingStationActor;
class UARPGBuildPieceDefinition;
class UARPGItemDefinition;
class UARPGRecipeDefinition;
class UARPGBuildingUIComponent;
class UButton;
class UImage;
class UProgressBar;
class UTextBlock;
class UVerticalBox;
class UHorizontalBox;
class UScrollBox;

USTRUCT(BlueprintType)
struct AKUMASRPGFRAMEWORK_API FARPGBuildPieceView
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly, Category="ARPG|Building UI") TObjectPtr<UARPGBuildPieceDefinition> Piece = nullptr;
    UPROPERTY(BlueprintReadOnly, Category="ARPG|Building UI") FText DisplayName;
    UPROPERTY(BlueprintReadOnly, Category="ARPG|Building UI") FText Description;
    UPROPERTY(BlueprintReadOnly, Category="ARPG|Building UI") FName Category = NAME_None;
    UPROPERTY(BlueprintReadOnly, Category="ARPG|Building UI") FText CostSummary;
    UPROPERTY(BlueprintReadOnly, Category="ARPG|Building UI") int32 BuildableCount = 0;
    UPROPERTY(BlueprintReadOnly, Category="ARPG|Building UI") float ConstructionSeconds = 0.f;
    UPROPERTY(BlueprintReadOnly, Category="ARPG|Building UI") bool bCanAfford = false;
};

USTRUCT(BlueprintType)
struct AKUMASRPGFRAMEWORK_API FARPGStructureItemView
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly, Category="ARPG|Structure UI") FName ItemId = NAME_None;
    UPROPERTY(BlueprintReadOnly, Category="ARPG|Structure UI") FGuid InstanceId;
    UPROPERTY(BlueprintReadOnly, Category="ARPG|Structure UI") TObjectPtr<UARPGItemDefinition> ItemDefinition = nullptr;
    UPROPERTY(BlueprintReadOnly, Category="ARPG|Structure UI") FText DisplayName;
    UPROPERTY(BlueprintReadOnly, Category="ARPG|Structure UI") int32 Quantity = 0;
    UPROPERTY(BlueprintReadOnly, Category="ARPG|Structure UI") bool bFromPlayerInventory = false;
    UPROPERTY(BlueprintReadOnly, Category="ARPG|Structure UI") bool bStationOutput = false;
};

USTRUCT(BlueprintType)
struct AKUMASRPGFRAMEWORK_API FARPGStationRecipeView
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly, Category="ARPG|Structure UI") TObjectPtr<UARPGRecipeDefinition> Recipe = nullptr;
    UPROPERTY(BlueprintReadOnly, Category="ARPG|Structure UI") FText DisplayName;
    UPROPERTY(BlueprintReadOnly, Category="ARPG|Structure UI") FText InputSummary;
    UPROPERTY(BlueprintReadOnly, Category="ARPG|Structure UI") FText OutputSummary;
    UPROPERTY(BlueprintReadOnly, Category="ARPG|Structure UI") FText FuelSummary;
    UPROPERTY(BlueprintReadOnly, Category="ARPG|Structure UI") float CraftSeconds = 0.f;
    UPROPERTY(BlueprintReadOnly, Category="ARPG|Structure UI") bool bCanQueue = false;
};

class UARPGBuildMenuWidget;
class UARPGStoragePanelWidget;
class UARPGCraftingStationPanelWidget;

UCLASS(BlueprintType, Blueprintable)
class AKUMASRPGFRAMEWORK_API UARPGBuildPieceRowWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="ARPG|Building UI") void InitializeBuildPieceRow(UARPGBuildMenuWidget* InOwner, const FARPGBuildPieceView& InView);
    UFUNCTION(BlueprintCallable, Category="ARPG|Building UI") void SetBuildPieceView(const FARPGBuildPieceView& InView);
    UFUNCTION(BlueprintImplementableEvent, Category="ARPG|Building UI", meta=(DisplayName="On ARPG Build Piece Row Updated")) void BP_OnBuildPieceRowUpdated(FARPGBuildPieceView PieceView);
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Building UI|Bindings") TObjectPtr<UButton> BuildPieceButton;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Building UI|Bindings") TObjectPtr<UImage> BuildPieceIcon;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Building UI|Bindings") TObjectPtr<UTextBlock> BuildPieceNameText;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Building UI|Bindings") TObjectPtr<UTextBlock> BuildPieceCostText;
protected:
    virtual void NativeOnInitialized() override;
private:
    UPROPERTY(Transient) TObjectPtr<UARPGBuildMenuWidget> OwnerMenu = nullptr;
    UPROPERTY(Transient) FARPGBuildPieceView View;
    UFUNCTION() void HandleClicked();
    void EnsureNativeLayoutOrBindings();
    void ApplyView();
};

UCLASS(BlueprintType, Blueprintable)
class AKUMASRPGFRAMEWORK_API UARPGBuildMenuWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="ARPG|Building UI") void InitializeBuildMenu(AARPGCharacter* InCharacter, UARPGBuildingUIComponent* InUI);
    UFUNCTION(BlueprintCallable, Category="ARPG|Building UI") void RefreshBuildMenu();
    UFUNCTION(BlueprintCallable, Category="ARPG|Building UI") void ChooseBuildPiece(UARPGBuildPieceDefinition* Piece);
    UFUNCTION(BlueprintImplementableEvent, Category="ARPG|Building UI", meta=(DisplayName="On ARPG Build Menu Refreshed")) void BP_OnBuildMenuRefreshed();
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Building UI|Bindings") TObjectPtr<UVerticalBox> BuildPieceListBox;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Building UI|Bindings") TObjectPtr<UTextBlock> BuildMenuTitleText;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Building UI|Bindings") TObjectPtr<UTextBlock> BuildMenuHelpText;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Building UI|Bindings") TObjectPtr<UButton> CloseBuildMenuButton;
protected:
    virtual void NativeOnInitialized() override;
private:
    UPROPERTY(Transient) TObjectPtr<AARPGCharacter> Character = nullptr;
    UPROPERTY(Transient) TObjectPtr<UARPGBuildingUIComponent> BuildingUI = nullptr;
    UFUNCTION() void HandleClose();
    void EnsureNativeLayoutOrBindings();
};

/** Non-interactive in-world-placement HUD. */
UCLASS(BlueprintType, Blueprintable)
class AKUMASRPGFRAMEWORK_API UARPGBuildPlacementHUDWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="ARPG|Building UI") void InitializePlacementHUD(AARPGCharacter* InCharacter);
    UFUNCTION(BlueprintCallable, Category="ARPG|Building UI") void RefreshPlacementHUD();
    UFUNCTION(BlueprintImplementableEvent, Category="ARPG|Building UI", meta=(DisplayName="On ARPG Build Placement HUD Updated")) void BP_OnBuildPlacementHUDUpdated();
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Building UI|Bindings") TObjectPtr<UTextBlock> PlacementPieceNameText;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Building UI|Bindings") TObjectPtr<UTextBlock> PlacementCostText;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Building UI|Bindings") TObjectPtr<UTextBlock> PlacementStatusText;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Building UI|Bindings") TObjectPtr<UTextBlock> PlacementControlsText;
protected:
    virtual void NativeOnInitialized() override;
private:
    UPROPERTY(Transient) TObjectPtr<AARPGCharacter> Character = nullptr;
    void EnsureNativeLayoutOrBindings();
};

UCLASS(BlueprintType, Blueprintable)
class AKUMASRPGFRAMEWORK_API UARPGStructureItemRowWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="ARPG|Structure UI") void InitializeStructureItemRow(UUserWidget* InOwnerPanel, const FARPGStructureItemView& InView);
    UFUNCTION(BlueprintCallable, Category="ARPG|Structure UI") void SetStructureItemView(const FARPGStructureItemView& InView);
    UFUNCTION(BlueprintImplementableEvent, Category="ARPG|Structure UI", meta=(DisplayName="On ARPG Structure Item Row Updated")) void BP_OnStructureItemRowUpdated(FARPGStructureItemView ItemView);
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Structure UI|Bindings") TObjectPtr<UImage> ItemIcon;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Structure UI|Bindings") TObjectPtr<UTextBlock> ItemNameText;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Structure UI|Bindings") TObjectPtr<UTextBlock> ItemQuantityText;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Structure UI|Bindings") TObjectPtr<UButton> TransferOneButton;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Structure UI|Bindings") TObjectPtr<UButton> TransferAllButton;
protected:
    virtual void NativeOnInitialized() override;
private:
    UPROPERTY(Transient) TObjectPtr<UUserWidget> OwnerPanel = nullptr;
    UPROPERTY(Transient) FARPGStructureItemView View;
    UFUNCTION() void HandleTransferOne();
    UFUNCTION() void HandleTransferAll();
    void EnsureNativeLayoutOrBindings();
    void ApplyView();
};

UCLASS(BlueprintType, Blueprintable)
class AKUMASRPGFRAMEWORK_API UARPGStoragePanelWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="ARPG|Structure UI") void InitializeStorageUI(AARPGCharacter* InCharacter, AARPGStorageActor* InStorage, UARPGBuildingUIComponent* InUI);
    UFUNCTION(BlueprintCallable, Category="ARPG|Structure UI") void RefreshStorageUI();
    void HandleItemTransfer(const FARPGStructureItemView& View, int32 Quantity);
    UFUNCTION(BlueprintImplementableEvent, Category="ARPG|Structure UI", meta=(DisplayName="On ARPG Storage UI Refreshed")) void BP_OnStorageUIRefreshed();
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Structure UI|Bindings") TObjectPtr<UTextBlock> StorageTitleText;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Structure UI|Bindings") TObjectPtr<UVerticalBox> PlayerItemListBox;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Structure UI|Bindings") TObjectPtr<UVerticalBox> StorageItemListBox;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Structure UI|Bindings") TObjectPtr<UButton> CloseStorageButton;
protected:
    virtual void NativeOnInitialized() override;
private:
    UPROPERTY(Transient) TObjectPtr<AARPGCharacter> Character = nullptr;
    UPROPERTY(Transient) TObjectPtr<AARPGStorageActor> Storage = nullptr;
    UPROPERTY(Transient) TObjectPtr<UARPGBuildingUIComponent> BuildingUI = nullptr;
    UFUNCTION() void HandleClose();
    void EnsureNativeLayoutOrBindings();
};

UCLASS(BlueprintType, Blueprintable)
class AKUMASRPGFRAMEWORK_API UARPGStationRecipeRowWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="ARPG|Structure UI") void InitializeStationRecipeRow(UARPGCraftingStationPanelWidget* InOwner, const FARPGStationRecipeView& InView);
    UFUNCTION(BlueprintCallable, Category="ARPG|Structure UI") void SetStationRecipeView(const FARPGStationRecipeView& InView);
    UFUNCTION(BlueprintImplementableEvent, Category="ARPG|Structure UI", meta=(DisplayName="On ARPG Station Recipe Row Updated")) void BP_OnStationRecipeRowUpdated(FARPGStationRecipeView RecipeView);
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Structure UI|Bindings") TObjectPtr<UButton> QueueRecipeButton;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Structure UI|Bindings") TObjectPtr<UTextBlock> StationRecipeNameText;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Structure UI|Bindings") TObjectPtr<UTextBlock> StationRecipeDetailsText;
protected:
    virtual void NativeOnInitialized() override;
private:
    UPROPERTY(Transient) TObjectPtr<UARPGCraftingStationPanelWidget> OwnerPanel = nullptr;
    UPROPERTY(Transient) FARPGStationRecipeView View;
    UFUNCTION() void HandleQueue();
    void EnsureNativeLayoutOrBindings();
    void ApplyView();
};

UCLASS(BlueprintType, Blueprintable)
class AKUMASRPGFRAMEWORK_API UARPGCraftingStationPanelWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="ARPG|Structure UI") void InitializeStationUI(AARPGCharacter* InCharacter, AARPGCraftingStationActor* InStation, UARPGBuildingUIComponent* InUI);
    UFUNCTION(BlueprintCallable, Category="ARPG|Structure UI") void RefreshStationUI();
    UFUNCTION(BlueprintCallable, Category="ARPG|Structure UI") void RefreshStationProgress();
    void HandleItemTransfer(const FARPGStructureItemView& View, int32 Quantity);
    void QueueRecipe(UARPGRecipeDefinition* Recipe);
    UFUNCTION(BlueprintImplementableEvent, Category="ARPG|Structure UI", meta=(DisplayName="On ARPG Station UI Refreshed")) void BP_OnStationUIRefreshed();
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Structure UI|Bindings") TObjectPtr<UTextBlock> StationTitleText;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Structure UI|Bindings") TObjectPtr<UVerticalBox> PlayerItemListBox;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Structure UI|Bindings") TObjectPtr<UVerticalBox> StationInputListBox;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Structure UI|Bindings") TObjectPtr<UVerticalBox> StationOutputListBox;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Structure UI|Bindings") TObjectPtr<UVerticalBox> StationRecipeListBox;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Structure UI|Bindings") TObjectPtr<UProgressBar> StationProgressBar;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Structure UI|Bindings") TObjectPtr<UTextBlock> StationStateText;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Structure UI|Bindings") TObjectPtr<UButton> CloseStationButton;
protected:
    virtual void NativeOnInitialized() override;
private:
    UPROPERTY(Transient) TObjectPtr<AARPGCharacter> Character = nullptr;
    UPROPERTY(Transient) TObjectPtr<AARPGCraftingStationActor> Station = nullptr;
    UPROPERTY(Transient) TObjectPtr<UARPGBuildingUIComponent> BuildingUI = nullptr;
    UFUNCTION() void HandleClose();
    void EnsureNativeLayoutOrBindings();
};
