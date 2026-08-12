#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TimerManager.h"
#include "UI/ARPGStatsPanelWidget.h"
#include "ARPGStatsUIComponent.generated.h"

class AARPGCharacter;
class APlayerController;

/**
 * Local-only player stats panel owner. The component is inherited by every AARPGCharacter for easy
 * Blueprint authoring, but only a locally controlled player pawn may open it.
 */
UCLASS(ClassGroup=(ARPG), BlueprintType, Blueprintable, meta=(BlueprintSpawnableComponent))
class AKUMASRPGFRAMEWORK_API UARPGStatsUIComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UARPGStatsUIComponent();

    /** Ready-to-use native panel by default. Select a Blueprint subclass of ARPGStatsPanelWidget for custom art. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stats UI|Widget", meta=(DisplayName="Stats Widget Class"))
    TSubclassOf<UARPGStatsPanelWidget> StatsWidgetClass;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stats UI|Widget", meta=(ClampMin="0", ClampMax="10000"))
    int32 ZOrder = 80;

    /** Lightweight local refresh used only while the panel is actually open, so Mana/Stamina stay live without permanent Tick. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stats UI|Runtime", meta=(ClampMin="0.05", ClampMax="2.0", Units="s"))
    float RefreshInterval = 0.15f;

    /** Makes the supplied Close/+ buttons immediately mouse-interactable with no extra PlayerController wiring. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stats UI|Input")
    bool bManageInputMode = true;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stats UI|Input", meta=(EditCondition="bManageInputMode"))
    bool bShowMouseCursorWhileOpen = true;

    /** Restores Game Only input on close. Disable if a project has a larger menu/input-stack manager. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stats UI|Input", meta=(EditCondition="bManageInputMode"))
    bool bRestoreGameOnlyInputOnClose = true;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="Stats UI|Runtime")
    TObjectPtr<UARPGStatsPanelWidget> ActiveStatsWidget;

    UFUNCTION(BlueprintCallable, Category="ARPG|Stats UI") bool OpenStatsUI();
    UFUNCTION(BlueprintCallable, Category="ARPG|Stats UI") bool CloseStatsUI();
    UFUNCTION(BlueprintCallable, Category="ARPG|Stats UI") bool ToggleStatsUI();
    UFUNCTION(BlueprintPure, Category="ARPG|Stats UI") bool IsStatsUIOpen() const;
    UFUNCTION(BlueprintCallable, Category="ARPG|Stats UI") void RefreshStatsUI();
    UFUNCTION(BlueprintPure, Category="ARPG|Stats UI") FARPGStatsUISnapshot GetStatsUISnapshot() const;

    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    FTimerHandle RefreshTimerHandle;
    bool bPreviousMouseCursor = false;
    TWeakObjectPtr<APlayerController> CachedLocalPlayerController;

    bool ResolveLocalPlayer(AARPGCharacter*& OutCharacter, APlayerController*& OutPlayerController) const;
    void StartRefreshTimer();
    void StopRefreshTimer();
    void ApplyOpenInputMode(APlayerController* PlayerController);
    void RestoreClosedInputMode(APlayerController* PlayerController);
    bool CloseStatsUIInternal(bool bRestoreInputMode);
};
