#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "World/ARPGDayNightCycle.h"
#include "ARPGDynamicStreetLight.generated.h"

class USceneComponent;
class UPointLightComponent;
class UNiagaraComponent;
class UParticleSystemComponent;
class ULightComponent;

UENUM(BlueprintType)
enum class EARPGDynamicStreetLightFXMode : uint8
{
    None UMETA(DisplayName="None"),
    NiagaraPreferredWithCascadeFallback UMETA(DisplayName="Niagara Preferred / Cascade Fallback"),
    NiagaraOnly UMETA(DisplayName="Niagara Only"),
    CascadeOnly UMETA(DisplayName="Cascade Only"),
    NiagaraAndCascade UMETA(DisplayName="Niagara + Cascade")
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FARPGOnDynamicStreetLightStateChanged, bool, bIsOn);

/**
 * Blueprint-derivable, event-driven street lamp/world light actor.
 *
 * The actor consumes ARPGDayNightCycle as the single replicated clock source, so individual lamps do not
 * need their own per-frame Tick or replicated clock state. By default the lamp is active through Night and
 * Dawn and turns off when Day begins. Niagara is preferred when assigned, with an optional Cascade fallback.
 */
UCLASS(BlueprintType, Blueprintable)
class AKUMASRPGFRAMEWORK_API AARPGDynamicStreetLight : public AActor
{
    GENERATED_BODY()

public:
    AARPGDynamicStreetLight();

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void OnConstruction(const FTransform& Transform) override;

    /** Attachment root for meshes/components added by a derived Blueprint. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="ARPG|Dynamic Street Light|Components")
    TObjectPtr<USceneComponent> SceneRoot;

    /** Ready-to-use movable lamp light. Derived Blueprints may reposition/tune it or add Spot/Rect lights. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="ARPG|Dynamic Street Light|Components")
    TObjectPtr<UPointLightComponent> LampLight;

    /** Optional Niagara flame/glow/sparks component. Auto activation is disabled and managed by this actor. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="ARPG|Dynamic Street Light|Components")
    TObjectPtr<UNiagaraComponent> NiagaraEffect;

    /** Optional legacy Cascade particle component used independently or as a Niagara fallback. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="ARPG|Dynamic Street Light|Components")
    TObjectPtr<UParticleSystemComponent> CascadeEffect;

    /** Master switch. When enabled, the lamp follows the resolved ARPGDayNightCycle phases. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ARPG|Dynamic Street Light|Day Night", meta=(DisplayName="Enable Automatic Day/Night Control"))
    bool bEnableAutomaticDayNightControl = true;

    /** Optional explicit cycle. Leave unset to auto-discover the first ARPGDayNightCycle in the world. */
    UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="ARPG|Dynamic Street Light|Day Night", meta=(EditCondition="bEnableAutomaticDayNightControl"))
    TObjectPtr<AARPGDayNightCycle> DayNightCycleOverride;

    /** Automatically discover an ARPGDayNightCycle when no override is assigned. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ARPG|Dynamic Street Light|Day Night", meta=(EditCondition="bEnableAutomaticDayNightControl"))
    bool bAutoDiscoverDayNightCycle = true;

    /** Retry period used only while automatic control is enabled and no valid cycle is currently available. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ARPG|Dynamic Street Light|Day Night", meta=(ClampMin="0.25", ClampMax="30.0", EditCondition="bEnableAutomaticDayNightControl"))
    float CycleResolveRetrySeconds = 2.f;

    /** Safe behavior while a cycle is missing. When true, the lamp goes dark instead of remaining in a stale state. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ARPG|Dynamic Street Light|Day Night", meta=(EditCondition="bEnableAutomaticDayNightControl"))
    bool bTurnOffWhenCycleUnavailable = true;

    /** Default schedule mirrors ARPGDayNightCycle::IsNight(): Night + Dawn on, Day + Dusk off. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ARPG|Dynamic Street Light|Active Phases", meta=(EditCondition="bEnableAutomaticDayNightControl"))
    bool bActiveDuringDawn = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ARPG|Dynamic Street Light|Active Phases", meta=(EditCondition="bEnableAutomaticDayNightControl"))
    bool bActiveDuringDay = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ARPG|Dynamic Street Light|Active Phases", meta=(EditCondition="bEnableAutomaticDayNightControl"))
    bool bActiveDuringDusk = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ARPG|Dynamic Street Light|Active Phases", meta=(EditCondition="bEnableAutomaticDayNightControl"))
    bool bActiveDuringNight = true;

    /** Used when automatic day/night control is disabled. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ARPG|Dynamic Street Light|Manual", meta=(EditCondition="!bEnableAutomaticDayNightControl"))
    bool bManualLightOn = false;

    /** Keeps the inherited lamp components visible while authoring a Blueprint/level actor. Runtime still follows the actual schedule. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ARPG|Dynamic Street Light|Editor Preview")
    bool bEditorPreviewOn = true;

    /** When enabled, every LightComponent owned by this actor (including Blueprint-added Spot/Rect lights) follows the lamp state. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ARPG|Dynamic Street Light|Presentation")
    bool bControlAllOwnedLightComponents = true;

    /** Particle selection policy. Niagara-preferred mode activates Cascade only when no Niagara system is assigned. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ARPG|Dynamic Street Light|Presentation")
    EARPGDynamicStreetLightFXMode FXMode = EARPGDynamicStreetLightFXMode::NiagaraPreferredWithCascadeFallback;

    /** Reset particle simulation when the lamp transitions from off to on. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ARPG|Dynamic Street Light|Presentation")
    bool bResetEffectsOnActivation = true;

    /** Runtime state-change event for emissive materials, audio, extra effects or UI/debug hooks. */
    UPROPERTY(BlueprintAssignable, Category="ARPG|Dynamic Street Light|Events")
    FARPGOnDynamicStreetLightStateChanged OnStreetLightStateChanged;

    /** Re-resolve/rebind the cycle (if necessary) and immediately apply the current world-time state. */
    UFUNCTION(BlueprintCallable, Category="ARPG|Dynamic Street Light")
    void RefreshFromDayNightCycleNow();

    /** Enable/disable automatic cycle following. Disabling immediately applies Manual Light On. */
    UFUNCTION(BlueprintCallable, Category="ARPG|Dynamic Street Light")
    void SetAutomaticDayNightControlEnabled(bool bEnabled);

    /** Switch to manual mode and immediately apply the requested lamp state. */
    UFUNCTION(BlueprintCallable, Category="ARPG|Dynamic Street Light")
    void SetManualLightState(bool bNewOn);

    /** Immediate presentation override. Automatic control remains enabled and can change the state on the next phase refresh. */
    UFUNCTION(BlueprintCallable, Category="ARPG|Dynamic Street Light")
    void ForceStreetLightState(bool bNewOn);

    UFUNCTION(BlueprintPure, Category="ARPG|Dynamic Street Light")
    bool IsStreetLightOn() const { return bStreetLightOn; }

    UFUNCTION(BlueprintPure, Category="ARPG|Dynamic Street Light")
    AARPGDayNightCycle* GetResolvedDayNightCycle() const { return ResolvedDayNightCycle; }

    UFUNCTION(BlueprintPure, Category="ARPG|Dynamic Street Light")
    bool ShouldBeOnForPhase(EARPGDayNightPhase Phase) const;

    /** Blueprint extension point for emissive-material swaps, audio cues, shutters, animations, etc. */
    UFUNCTION(BlueprintImplementableEvent, Category="ARPG|Dynamic Street Light|Events", meta=(DisplayName="Receive Street Light State Changed"))
    void ReceiveStreetLightStateChanged(bool bIsOn);

private:
    UPROPERTY(Transient)
    TObjectPtr<AARPGDayNightCycle> ResolvedDayNightCycle;

    UPROPERTY(Transient)
    bool bStreetLightOn = false;

    bool bHasAppliedRuntimeState = false;
    FTimerHandle CycleResolveRetryTimer;
    bool bWarnedMissingCycle = false;

    void ResolveAndBindDayNightCycle();
    void BindToDayNightCycle(AARPGDayNightCycle* NewCycle);
    void UnbindFromDayNightCycle();
    void ScheduleCycleResolveRetry();
    void ClearCycleResolveRetry();
    void ApplyStreetLightState(bool bNewOn, bool bBroadcastChange);
    void ApplyOwnedLights(bool bNewOn);
    void ApplyEffects(bool bNewOn);
    void SetNiagaraActive(bool bNewActive);
    void SetCascadeActive(bool bNewActive);

    UFUNCTION()
    void HandleDayNightPhaseChanged(EARPGDayNightPhase NewPhase);

    UFUNCTION()
    void HandleDayNightHourChanged(int32 NewHour);

    UFUNCTION()
    void HandleDayNightCycleDestroyed(AActor* DestroyedActor);
};
