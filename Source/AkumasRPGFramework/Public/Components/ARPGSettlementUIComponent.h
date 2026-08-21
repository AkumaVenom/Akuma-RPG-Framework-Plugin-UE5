#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ARPGTypes.h"
#include "ARPGSettlementUIComponent.generated.h"

class AARPGCharacter;
class AARPGSettlementHubActor;
class AARPGBuildBedActor;
class UARPGSettlementHUDWidget;
class UARPGSettlementPanelWidget;
class UARPGSettlementResidentRowWidget;
class UARPGBedPanelWidget;
class APlayerController;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FARPGNearbySettlementChanged, AARPGSettlementHubActor*, NewSettlement);

/** Local-only settlement HUD/panel owner. Native widgets work immediately and every surface is subclassable/reskinnable. */
UCLASS(ClassGroup=(ARPG), meta=(BlueprintSpawnableComponent))
class AKUMASRPGFRAMEWORK_API UARPGSettlementUIComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UARPGSettlementUIComponent();

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Settlement UI|Classes") TSubclassOf<UARPGSettlementHUDWidget> SettlementHUDWidgetClass;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Settlement UI|Classes") TSubclassOf<UARPGSettlementPanelWidget> SettlementPanelWidgetClass;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Settlement UI|Classes") TSubclassOf<UARPGSettlementResidentRowWidget> ResidentRowWidgetClass;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Settlement UI|Classes") TSubclassOf<UARPGBedPanelWidget> BedPanelWidgetClass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settlement UI|Behavior") bool bAutoShowNearbySettlementHUD = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settlement UI|Behavior", meta=(ClampMin="0.1", Units="s")) float ProximityPollInterval = 0.35f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settlement UI|Behavior") bool bManageInputMode = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settlement UI|Behavior") bool bShowMouseCursorWhilePanelOpen = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settlement UI|Layering") int32 SettlementHUDZOrder = 44;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settlement UI|Layering") int32 SettlementPanelZOrder = 70;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settlement UI|Layering") int32 BedPanelZOrder = 70;

    UPROPERTY(BlueprintAssignable, Category="Settlement UI|Events") FARPGNearbySettlementChanged OnNearbySettlementChanged;

    UFUNCTION(BlueprintCallable, Category="ARPG|Settlement UI") bool OpenSettlementPanel(AARPGSettlementHubActor* Hub);
    UFUNCTION(BlueprintCallable, Category="ARPG|Settlement UI") bool OpenBedPanel(AARPGBuildBedActor* Bed);
    UFUNCTION(BlueprintCallable, Category="ARPG|Settlement UI") bool CloseSettlementPanel();
    UFUNCTION(BlueprintCallable, Category="ARPG|Settlement UI") bool CloseBedPanel();
    UFUNCTION(BlueprintCallable, Category="ARPG|Settlement UI") bool CloseAllSettlementUI();
    UFUNCTION(BlueprintCallable, Category="ARPG|Settlement UI") bool SetBedRole(AARPGBuildBedActor* Bed, EARPGBedRole NewRole);
    UFUNCTION(BlueprintCallable, Category="ARPG|Settlement UI") bool RequestSettlementRefresh(AARPGSettlementHubActor* Hub);
    /** Opens the Hub's native storage panel so player-built settlement resources are directly usable. */
    UFUNCTION(BlueprintCallable, Category="ARPG|Settlement UI") bool OpenSettlementStockpile(AARPGSettlementHubActor* Hub);
    UFUNCTION(BlueprintCallable, Category="ARPG|Settlement UI") void RefreshOpenSettlementUI();
    UFUNCTION(BlueprintPure, Category="ARPG|Settlement UI") AARPGSettlementHubActor* GetNearbySettlementHub() const { return NearbySettlementHub.Get(); }
    UFUNCTION(BlueprintPure, Category="ARPG|Settlement UI") bool IsSettlementPanelOpen() const;
    UFUNCTION(BlueprintPure, Category="ARPG|Settlement UI") bool IsBedPanelOpen() const;
    UFUNCTION(BlueprintPure, Category="ARPG|Settlement UI|Classes") TSubclassOf<UARPGSettlementResidentRowWidget> GetResidentRowWidgetClass() const { return ResidentRowWidgetClass; }

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
private:
    UPROPERTY(Transient) TObjectPtr<UARPGSettlementHUDWidget> ActiveSettlementHUD = nullptr;
    UPROPERTY(Transient) TObjectPtr<UARPGSettlementPanelWidget> ActiveSettlementPanel = nullptr;
    UPROPERTY(Transient) TObjectPtr<UARPGBedPanelWidget> ActiveBedPanel = nullptr;
    TWeakObjectPtr<AARPGSettlementHubActor> NearbySettlementHub;
    TWeakObjectPtr<AARPGSettlementHubActor> PanelSettlementHub;
    TWeakObjectPtr<AARPGBuildBedActor> PanelBed;
    TWeakObjectPtr<APlayerController> CachedPlayerController;
    FTimerHandle ProximityTimer;
    bool bPreviousMouseCursor = false;
    bool bCursorStateCaptured = false;

    bool ResolveLocalPlayer(AARPGCharacter*& OutCharacter, APlayerController*& OutController) const;
    void PollNearbySettlement();
    AARPGSettlementHubActor* FindNearestUsableSettlement(AARPGCharacter* Character) const;
    void ShowSettlementHUD(AARPGSettlementHubActor* Hub, APlayerController* Controller);
    void HideSettlementHUD();
    void ApplyMenuInputMode(APlayerController* Controller);
    void RestoreGameInputMode(APlayerController* Controller);
};
