#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ARPGTypes.h"
#include "ARPGBuildingUIComponent.generated.h"

class AARPGCharacter;
class AARPGStorageActor;
class AARPGCraftingStationActor;
class UARPGBuildPieceDefinition;
class UARPGBuildMenuWidget;
class UARPGBuildPieceRowWidget;
class UARPGBuildPlacementHUDWidget;
class UARPGStoragePanelWidget;
class UARPGCraftingStationPanelWidget;
class UARPGStructureItemRowWidget;
class UARPGStationRecipeRowWidget;
class UARPGInventoryComponent;
class UARPGBuildingComponent;
class APlayerController;

/** Local-only ready UI controller for build catalogue/placement HUD and built storage/production structures. */
UCLASS(ClassGroup=(ARPG), meta=(BlueprintSpawnableComponent))
class AKUMASRPGFRAMEWORK_API UARPGBuildingUIComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UARPGBuildingUIComponent();

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Building UI|Classes") TSubclassOf<UARPGBuildMenuWidget> BuildMenuWidgetClass;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Building UI|Classes") TSubclassOf<UARPGBuildPieceRowWidget> BuildPieceRowWidgetClass;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Building UI|Classes") TSubclassOf<UARPGBuildPlacementHUDWidget> PlacementHUDWidgetClass;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Building UI|Classes") TSubclassOf<UARPGStoragePanelWidget> StorageWidgetClass;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Building UI|Classes") TSubclassOf<UARPGCraftingStationPanelWidget> CraftingStationWidgetClass;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Building UI|Classes") TSubclassOf<UARPGStructureItemRowWidget> StructureItemRowWidgetClass;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Building UI|Classes") TSubclassOf<UARPGStationRecipeRowWidget> StationRecipeRowWidgetClass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Building UI|Behavior") bool bCloseBuildMenuWhenPlacementStarts = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Building UI|Behavior") bool bManageInputMode = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Building UI|Behavior") bool bShowMouseCursorWhileMenusOpen = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Building UI|Behavior") bool bRestoreGameOnlyInputOnClose = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Building UI|Behavior", meta=(ClampMin="100.0")) float StructureInteractionDistance = 650.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Building UI|Behavior") TEnumAsByte<ECollisionChannel> StructureInteractionTraceChannel = ECC_Visibility;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Building UI|Layering") int32 BuildMenuZOrder = 60;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Building UI|Layering") int32 PlacementHUDZOrder = 55;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Building UI|Layering") int32 StructureUIZOrder = 65;

    UFUNCTION(BlueprintCallable, Category="ARPG|Building UI") bool OpenBuildMenu();
    UFUNCTION(BlueprintCallable, Category="ARPG|Building UI") bool CloseBuildMenu();
    UFUNCTION(BlueprintCallable, Category="ARPG|Building UI") bool ToggleBuildMenu();
    UFUNCTION(BlueprintPure, Category="ARPG|Building UI") bool IsBuildMenuOpen() const;
    UFUNCTION(BlueprintCallable, Category="ARPG|Building UI") bool BeginPlacementFromMenu(UARPGBuildPieceDefinition* Piece);
    UFUNCTION(BlueprintCallable, Category="ARPG|Building UI") bool OpenStorageUI(AARPGStorageActor* Storage);
    UFUNCTION(BlueprintCallable, Category="ARPG|Building UI") bool OpenCraftingStationUI(AARPGCraftingStationActor* Station);
    UFUNCTION(BlueprintCallable, Category="ARPG|Building UI") bool CloseStructureUI();
    /** View trace: doors toggle immediately; storage and production stations open their ready interfaces. */
    UFUNCTION(BlueprintCallable, Category="ARPG|Building UI|Input") bool InteractWithBuiltStructureFromView();
    /** View trace and request authority demolition/refund through the player-owned Interaction component. */
    UFUNCTION(BlueprintCallable, Category="ARPG|Building UI|Input") bool DemolishBuiltStructureFromView();

    UFUNCTION(BlueprintPure, Category="ARPG|Building UI|Classes") TSubclassOf<UARPGBuildPieceRowWidget> GetBuildPieceRowWidgetClass() const { return BuildPieceRowWidgetClass; }
    UFUNCTION(BlueprintPure, Category="ARPG|Building UI|Classes") TSubclassOf<UARPGStructureItemRowWidget> GetStructureItemRowWidgetClass() const { return StructureItemRowWidgetClass; }
    UFUNCTION(BlueprintPure, Category="ARPG|Building UI|Classes") TSubclassOf<UARPGStationRecipeRowWidget> GetStationRecipeRowWidgetClass() const { return StationRecipeRowWidgetClass; }

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
protected:
    UFUNCTION() void HandleBuildModeChanged(bool bActive, UARPGBuildPieceDefinition* Piece);
    UFUNCTION() void HandleBuildPreviewUpdated(EARPGPlacementResult Result, FTransform PreviewTransform);
    UFUNCTION() void HandlePlayerInventoryChanged();
    UFUNCTION() void HandleStructureInventoryChanged();
    UFUNCTION() void HandleStationQueueChanged();
private:
    UPROPERTY(Transient) TObjectPtr<UARPGBuildMenuWidget> ActiveBuildMenu = nullptr;
    UPROPERTY(Transient) TObjectPtr<UARPGBuildPlacementHUDWidget> ActivePlacementHUD = nullptr;
    UPROPERTY(Transient) TObjectPtr<UARPGStoragePanelWidget> ActiveStorageWidget = nullptr;
    UPROPERTY(Transient) TObjectPtr<UARPGCraftingStationPanelWidget> ActiveStationWidget = nullptr;
    UPROPERTY(Transient) TObjectPtr<AARPGStorageActor> ActiveStorage = nullptr;
    UPROPERTY(Transient) TObjectPtr<AARPGCraftingStationActor> ActiveStation = nullptr;
    TWeakObjectPtr<APlayerController> CachedPlayerController;
    FTimerHandle StationProgressTimer;
    bool bRuntimeEventsBound = false;
    bool bCursorStateCaptured = false;
    bool bPreviousMouseCursor = false;

    bool ResolveLocalPlayer(AARPGCharacter*& OutCharacter, APlayerController*& OutController) const;
    UARPGBuildingComponent* GetBuilding() const;
    UARPGInventoryComponent* GetPlayerInventory() const;
    void BindBuildEvents();
    void UnbindBuildEvents();
    void BindStructureEvents();
    void UnbindStructureEvents();
    void EnsurePlacementHUD();
    void RemovePlacementHUD();
    void ApplyMenuInputMode(APlayerController* Controller);
    void RestoreGameInputMode(APlayerController* Controller);
    void StartStationProgressTimer();
    void StopStationProgressTimer();
    void RefreshOpenStructureUI();
    void RefreshStationProgress();
};
