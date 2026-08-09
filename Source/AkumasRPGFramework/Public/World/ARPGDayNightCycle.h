#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Misc/DateTime.h"
#include "ARPGDayNightCycle.generated.h"

class USceneComponent;
class UDirectionalLightComponent;
class USkyLightComponent;
class USkyAtmosphereComponent;
class UExponentialHeightFogComponent;
class ADirectionalLight;
class ASkyLight;
class ASkyAtmosphere;
class AExponentialHeightFog;

UENUM(BlueprintType)
enum class EARPGWorldTimeSource : uint8
{
    HostSystemClock UMETA(DisplayName="Host System Clock"),
    SimulatedClock UMETA(DisplayName="Simulated Clock"),
    FixedTime UMETA(DisplayName="Fixed Time")
};

UENUM(BlueprintType)
enum class EARPGDayNightPhase : uint8
{
    Dawn,
    Day,
    Dusk,
    Night
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FARPGOnDayNightPhaseChanged, EARPGDayNightPhase, NewPhase);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FARPGOnWorldHourChanged, int32, NewHour);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FARPGOnWorldTimeEvent);

/**
 * Drop-in, server-authoritative day/night cycle. Host System Clock is the default and mirrors the
 * authority machine's local wall clock (including its time zone/DST). Clients extrapolate between
 * replicated clock samples so the sun/camera-facing world never advances in visible network steps.
 */
UCLASS(BlueprintType, Blueprintable)
class AKUMASRPGFRAMEWORK_API AARPGDayNightCycle : public AActor
{
    GENERATED_BODY()

public:
    AARPGDayNightCycle();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // Built-in one-actor environment rig. Leave enabled for the easiest setup.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="ARPG|Day Night|Built-In Rig") TObjectPtr<USceneComponent> SceneRoot;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="ARPG|Day Night|Built-In Rig") TObjectPtr<UDirectionalLightComponent> SunLight;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="ARPG|Day Night|Built-In Rig") TObjectPtr<UDirectionalLightComponent> MoonLight;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="ARPG|Day Night|Built-In Rig") TObjectPtr<USkyLightComponent> SkyLight;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="ARPG|Day Night|Built-In Rig") TObjectPtr<USkyAtmosphereComponent> SkyAtmosphere;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="ARPG|Day Night|Built-In Rig") TObjectPtr<UExponentialHeightFogComponent> HeightFog;

    /** When false, the built-in rendering components are hidden and the optional external actors below are driven instead. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ARPG|Day Night|Lighting Rig", meta=(DisplayName="Use Built-In Lighting Rig")) bool bUseBuiltInLightingRig = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ARPG|Day Night|Lighting Rig", meta=(EditCondition="!bUseBuiltInLightingRig")) TObjectPtr<ADirectionalLight> ExternalSunLight;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ARPG|Day Night|Lighting Rig", meta=(EditCondition="!bUseBuiltInLightingRig")) TObjectPtr<ADirectionalLight> ExternalMoonLight;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ARPG|Day Night|Lighting Rig", meta=(EditCondition="!bUseBuiltInLightingRig")) TObjectPtr<ASkyLight> ExternalSkyLight;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ARPG|Day Night|Lighting Rig", meta=(EditCondition="!bUseBuiltInLightingRig")) TObjectPtr<ASkyAtmosphere> ExternalSkyAtmosphere;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ARPG|Day Night|Lighting Rig", meta=(EditCondition="!bUseBuiltInLightingRig")) TObjectPtr<AExponentialHeightFog> ExternalHeightFog;

    // Clock source. HostSystemClock is WoW-style real-world local time by default.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ARPG|Day Night|Clock") EARPGWorldTimeSource TimeSource = EARPGWorldTimeSource::HostSystemClock;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ARPG|Day Night|Clock", meta=(ClampMin="0.01", EditCondition="TimeSource==EARPGWorldTimeSource::SimulatedClock")) float SimulatedMinutesPerRealSecond = 1.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ARPG|Day Night|Clock", meta=(EditCondition="TimeSource==EARPGWorldTimeSource::SimulatedClock")) FDateTime SimulatedStartDateTime;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ARPG|Day Night|Clock", meta=(EditCondition="TimeSource==EARPGWorldTimeSource::FixedTime")) FDateTime FixedDateTime;
    /** Adds a designer/testing offset after the selected clock source. Zero by default. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ARPG|Day Night|Clock", meta=(ClampMin="-168.0", ClampMax="168.0")) float ClockOffsetHours = 0.f;
    /** Server clock snapshots are infrequent; clients extrapolate smoothly between them. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ARPG|Day Night|Networking", meta=(ClampMin="0.25", ClampMax="30.0")) float ClockReplicationInterval = 5.f;

    // Semantic day/night boundaries used by pure Blueprint checks.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ARPG|Day Night|Phases", meta=(ClampMin="0.0", ClampMax="24.0")) float DawnStartHour = 5.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ARPG|Day Night|Phases", meta=(ClampMin="0.0", ClampMax="24.0")) float DayStartHour = 6.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ARPG|Day Night|Phases", meta=(ClampMin="0.0", ClampMax="24.0")) float DuskStartHour = 17.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ARPG|Day Night|Phases", meta=(ClampMin="0.0", ClampMax="24.0")) float NightStartHour = 18.f;

    // Celestial motion.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ARPG|Day Night|Celestial", meta=(ClampMin="0.0", ClampMax="24.0")) float SunriseHour = 6.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ARPG|Day Night|Celestial", meta=(ClampMin="0.0", ClampMax="24.0")) float SunsetHour = 18.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ARPG|Day Night|Celestial") float SunYaw = -30.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ARPG|Day Night|Celestial") float SunPitchOffset = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ARPG|Day Night|Celestial") float MoonYawOffset = 180.f;

    // Lighting quality defaults. Values can be tuned per project without touching Blueprint graphs.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ARPG|Day Night|Sun", meta=(ClampMin="0.0")) float SunMaxIntensityLux = 100000.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ARPG|Day Night|Sun") FLinearColor SunNoonColor = FLinearColor(1.f, 0.97f, 0.90f);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ARPG|Day Night|Sun") FLinearColor SunHorizonColor = FLinearColor(1.f, 0.32f, 0.08f);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ARPG|Day Night|Moon", meta=(ClampMin="0.0")) float MoonMaxIntensityLux = 0.5f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ARPG|Day Night|Moon") FLinearColor MoonColor = FLinearColor(0.55f, 0.68f, 1.f);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ARPG|Day Night|Sky Light", meta=(ClampMin="0.0")) float SkyLightDayIntensity = 1.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ARPG|Day Night|Sky Light", meta=(ClampMin="0.0")) float SkyLightNightIntensity = 0.12f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ARPG|Day Night|Sky Light") bool bUseRealTimeSkyCapture = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ARPG|Day Night|Fog", meta=(ClampMin="0.0")) float FogDayDensity = 0.008f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ARPG|Day Night|Fog", meta=(ClampMin="0.0")) float FogNightDensity = 0.016f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ARPG|Day Night|Fog") FLinearColor FogDayColor = FLinearColor(0.55f, 0.68f, 0.78f);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ARPG|Day Night|Fog") FLinearColor FogNightColor = FLinearColor(0.035f, 0.055f, 0.10f);

    // Runtime events.
    UPROPERTY(BlueprintAssignable, Category="ARPG|Day Night|Events") FARPGOnDayNightPhaseChanged OnPhaseChanged;
    UPROPERTY(BlueprintAssignable, Category="ARPG|Day Night|Events") FARPGOnWorldHourChanged OnHourChanged;
    UPROPERTY(BlueprintAssignable, Category="ARPG|Day Night|Events") FARPGOnWorldTimeEvent OnDawnStarted;
    UPROPERTY(BlueprintAssignable, Category="ARPG|Day Night|Events") FARPGOnWorldTimeEvent OnDayStarted;
    UPROPERTY(BlueprintAssignable, Category="ARPG|Day Night|Events") FARPGOnWorldTimeEvent OnDuskStarted;
    UPROPERTY(BlueprintAssignable, Category="ARPG|Day Night|Events") FARPGOnWorldTimeEvent OnNightStarted;

    // Green Blueprint queries.
    UFUNCTION(BlueprintPure, Category="ARPG|Day Night") FDateTime GetWorldDateTime() const;
    UFUNCTION(BlueprintPure, Category="ARPG|Day Night") float GetWorldHour() const;
    UFUNCTION(BlueprintPure, Category="ARPG|Day Night") int32 GetWorldHourInteger() const;
    UFUNCTION(BlueprintPure, Category="ARPG|Day Night") bool IsDay() const;
    UFUNCTION(BlueprintPure, Category="ARPG|Day Night") bool IsNight() const;
    UFUNCTION(BlueprintPure, Category="ARPG|Day Night") EARPGDayNightPhase GetDayNightPhase() const;
    UFUNCTION(BlueprintPure, Category="ARPG|Day Night") float GetDaylightAmount() const;
    UFUNCTION(BlueprintPure, Category="ARPG|Day Night") float GetNightAmount() const { return 1.f - GetDaylightAmount(); }
    UFUNCTION(BlueprintPure, Category="ARPG|Day Night") FString GetWorldTimeString(bool bUse24HourClock = true, bool bIncludeSeconds = false) const;
    UFUNCTION(BlueprintPure, Category="ARPG|Day Night") FString GetWorldDateString() const;

    // Authority/test controls.
    UFUNCTION(BlueprintCallable, Category="ARPG|Day Night", meta=(BlueprintAuthorityOnly)) void UseHostSystemClock();
    UFUNCTION(BlueprintCallable, Category="ARPG|Day Night", meta=(BlueprintAuthorityOnly)) void SetFixedWorldTime(const FDateTime& NewDateTime);
    UFUNCTION(BlueprintCallable, Category="ARPG|Day Night", meta=(BlueprintAuthorityOnly)) void StartSimulatedClock(const FDateTime& StartDateTime, float MinutesPerRealSecond = 1.f);
    UFUNCTION(BlueprintCallable, Category="ARPG|Day Night", meta=(BlueprintAuthorityOnly)) void ForceClockSync();
    UFUNCTION(BlueprintCallable, Category="ARPG|Day Night") void RefreshLightingNow();

protected:
    UPROPERTY(ReplicatedUsing=OnRep_ReplicatedClock) FDateTime ReplicatedHostDateTime;
    UPROPERTY(ReplicatedUsing=OnRep_ReplicatedClock) float ReplicatedClockRate = 1.f;

    UFUNCTION() void OnRep_ReplicatedClock();

    FDateTime SimulatedCurrentDateTime;
    double ClientSyncRealTimeSeconds = 0.0;
    float ReplicationAccumulator = 0.f;
    EARPGDayNightPhase LastPhase = EARPGDayNightPhase::Night;
    int32 LastHour = INDEX_NONE;
    bool bHasInitializedPhase = false;

    FDateTime GetAuthorityDateTime() const;
    FDateTime GetVisualDateTime() const;
    float GetHourFromDateTime(const FDateTime& DateTime) const;
    EARPGDayNightPhase EvaluatePhase(float Hour) const;
    float EvaluateDaylight(float Hour) const;
    void UpdateAuthoritativeClock(float DeltaSeconds);
    void UpdateLightingAndEvents(bool bBroadcastEvents);
    void ApplyLighting(const FDateTime& DateTime);
    void ApplyRigVisibility();
    void BroadcastPhaseIfChanged(EARPGDayNightPhase NewPhase, int32 NewHour);

    UDirectionalLightComponent* ResolveSunComponent() const;
    UDirectionalLightComponent* ResolveMoonComponent() const;
    USkyLightComponent* ResolveSkyLightComponent() const;
    UExponentialHeightFogComponent* ResolveFogComponent() const;
};
