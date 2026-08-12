#pragma once

#include "CoreMinimal.h"
#include "Components/WidgetComponent.h"
#include "UI/ARPGCharacterInfoWidget.h"
#include "TimerManager.h"
#include "ARPGCharacterInfoComponent.generated.h"

class AARPGCharacter;
class APawn;
class ULocalPlayer;

/**
 * Local presentation component for automatic overhead ARPG character information.
 *
 * The component exists on every AARPGCharacter so a project can select it directly in a Character
 * Blueprint and assign a Widget Class. Visibility is evaluated independently on each local client;
 * no proximity state or UI data is replicated because the underlying Character name/stats/progression
 * are already replicated by their owning gameplay systems.
 */
UCLASS(ClassGroup=(ARPG), BlueprintType, Blueprintable, meta=(BlueprintSpawnableComponent))
class AKUMASRPGFRAMEWORK_API UARPGCharacterInfoComponent : public UWidgetComponent
{
    GENERATED_BODY()

public:
    UARPGCharacterInfoComponent();

    /** Master switch. Disable this on an archetype that should never display an overhead info popup. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ARPG|NPC Info|Setup", meta=(DisplayName="Enable Character Info Popup"))
    bool bEnableInfoPopup = true;

    /** AI-controlled ARPG Characters are shown by default. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ARPG|NPC Info|Setup", meta=(DisplayName="Show On AI Characters"))
    bool bShowOnAICharacters = true;

    /** Optional player nameplates. Disabled by default so this feature behaves as an NPC popup out of the box. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ARPG|NPC Info|Setup", meta=(DisplayName="Show On Player Controlled Characters"))
    bool bShowOnPlayerControlledCharacters = false;

    /** Never render the local player's own overhead popup even when player nameplates are enabled. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ARPG|NPC Info|Setup", meta=(DisplayName="Hide Local Player Self"))
    bool bHideLocalPlayerSelf = true;

    /** Hide the popup when the character reaches zero health. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ARPG|NPC Info|Visibility", meta=(DisplayName="Hide When Dead"))
    bool bHideWhenDead = true;

    /** Keep a newly spawned NPC's popup hidden until the replicated ground-rise entrance has finished. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ARPG|NPC Info|Visibility", meta=(DisplayName="Hide During Ground Rise Entrance"))
    bool bHideDuringSpawnEntrance = true;

    /** Distance at which a currently hidden popup becomes visible. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ARPG|NPC Info|Visibility", meta=(DisplayName="Show Distance (cm)", ClampMin="0.0", UIMin="0.0"))
    float ShowDistance = 1100.f;

    /** Larger hide distance provides hysteresis so the popup does not flicker at the range boundary. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ARPG|NPC Info|Visibility", meta=(DisplayName="Hide Distance (cm)", ClampMin="0.0", UIMin="0.0"))
    float HideDistance = 1350.f;

    /** Local proximity sampling rate. No permanent Tick is used. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ARPG|NPC Info|Performance", meta=(DisplayName="Proximity Check Interval", ClampMin="0.05", UIMin="0.05", Units="s"))
    float ProximityCheckInterval = 0.20f;

    /** Optional obstruction rule. Off by default to avoid extra line-of-sight work on large NPC populations. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ARPG|NPC Info|Visibility", meta=(DisplayName="Require Player Line Of Sight"))
    bool bRequireLineOfSight = false;

    /** Derive popup Z from the owning Character capsule, keeping different-sized NPCs correctly aligned. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ARPG|NPC Info|Placement", meta=(DisplayName="Auto Height From Capsule"))
    bool bAutoHeightFromCapsule = true;

    /** Additional height above the top of the scaled capsule when automatic height is enabled. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ARPG|NPC Info|Placement", meta=(EditCondition="bAutoHeightFromCapsule", DisplayName="Height Above Capsule (cm)", ClampMin="-500.0", ClampMax="1000.0"))
    float HeightAboveCapsule = 35.f;

    /** Relative Z used when automatic capsule height is disabled. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ARPG|NPC Info|Placement", meta=(EditCondition="!bAutoHeightFromCapsule", DisplayName="Manual Relative Height (cm)", ClampMin="-500.0", ClampMax="2000.0"))
    float ManualRelativeHeight = 140.f;

    /** Retire the runtime widget while initially hidden and recreate it on first in-range presentation.
     *  The selected Widget Class itself is preserved so UWidgetComponent screen-space registration remains reliable. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ARPG|NPC Info|Performance", meta=(DisplayName="Lazy Create Widget"))
    bool bLazyCreateWidget = true;

    /** Optionally release an already-created widget after it has remained far/hidden for a while. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ARPG|NPC Info|Performance", meta=(DisplayName="Release Widget When Far"))
    bool bReleaseWidgetWhenFar = true;

    /** Delay before a hidden widget instance is released; prevents churn near the distance boundary. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ARPG|NPC Info|Performance", meta=(EditCondition="bReleaseWidgetWhenFar", DisplayName="Far Widget Release Delay", ClampMin="0.0", UIMin="0.0", Units="s"))
    float FarWidgetReleaseDelay = 5.f;

    /** Optional automatic field mapping for any ordinary UserWidget assigned to Widget Class. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ARPG|NPC Info|Widget Binding", meta=(DisplayName="Name Text Widget"))
    FName NameTextWidgetName = TEXT("CharacterNameText");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ARPG|NPC Info|Widget Binding", meta=(DisplayName="Level Text Widget"))
    FName LevelTextWidgetName = TEXT("LevelText");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ARPG|NPC Info|Widget Binding", meta=(DisplayName="Health Bar Widget"))
    FName HealthBarWidgetName = TEXT("HealthBar");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ARPG|NPC Info|Widget Binding", meta=(DisplayName="Health Text Widget"))
    FName HealthTextWidgetName = TEXT("HealthText");

    /** Force an immediate local visibility/data refresh. Safe to call from Blueprint after changing popup settings. */
    UFUNCTION(BlueprintCallable, Category="ARPG|NPC Info")
    void RefreshCharacterInfoNow();

    /** Current replicated character presentation data used by the popup. */
    UFUNCTION(BlueprintPure, Category="ARPG|NPC Info")
    FARPGCharacterInfoSnapshot GetCharacterInfoSnapshot() const;

    UFUNCTION(BlueprintPure, Category="ARPG|NPC Info")
    bool IsCharacterInfoVisible() const { return bPopupVisible; }

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    UPROPERTY(Transient) TSubclassOf<UUserWidget> CachedConfiguredWidgetClass;
    FTimerHandle ProximityTimer;
    bool bPopupVisible = false;
    float HiddenSinceTime = -1.f;

    bool FindNearestLocalViewer(ULocalPlayer*& OutLocalPlayer, APawn*& OutPawn, float& OutDistanceSquared) const;
    bool ShouldShowForViewer(ULocalPlayer* LocalPlayer, APawn* ViewerPawn, float DistanceSquared) const;
    void ApplyAutomaticHeight();
    void EnsureWidgetCreated(ULocalPlayer* LocalPlayer);
    void ReleaseWidgetInstance();
    void ApplySnapshotToWidget(const FARPGCharacterInfoSnapshot& Snapshot);
    void SetPopupVisibleLocal(bool bShouldBeVisible, ULocalPlayer* LocalPlayer);

    UFUNCTION() void HandleHealthChanged(float NewHealth, float Delta);
    UFUNCTION() void HandleLevelChanged(int32 OldLevel, int32 NewLevel);
};
