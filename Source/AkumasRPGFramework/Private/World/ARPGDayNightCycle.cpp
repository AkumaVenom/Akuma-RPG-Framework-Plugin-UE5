#include "World/ARPGDayNightCycle.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/SkyLight.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

AARPGDayNightCycle::AARPGDayNightCycle()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;
    PrimaryActorTick.bTickEvenWhenPaused = true;
    PrimaryActorTick.TickInterval = 0.1f;

    bReplicates = true;
    bAlwaysRelevant = true;
    SetNetUpdateFrequency(2.f);

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(SceneRoot);

    SunLight = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("SunLight"));
    SunLight->SetupAttachment(SceneRoot);
    SunLight->SetMobility(EComponentMobility::Movable);
    SunLight->SetAtmosphereSunLight(true);
    SunLight->SetAtmosphereSunLightIndex(0);

    MoonLight = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("MoonLight"));
    MoonLight->SetupAttachment(SceneRoot);
    MoonLight->SetMobility(EComponentMobility::Movable);
    MoonLight->SetAtmosphereSunLight(true);
    MoonLight->SetAtmosphereSunLightIndex(1);

    SkyLight = CreateDefaultSubobject<USkyLightComponent>(TEXT("SkyLight"));
    SkyLight->SetupAttachment(SceneRoot);
    SkyLight->SetMobility(EComponentMobility::Movable);
    SkyLight->SetRealTimeCapture(true);

    SkyAtmosphere = CreateDefaultSubobject<USkyAtmosphereComponent>(TEXT("SkyAtmosphere"));
    SkyAtmosphere->SetupAttachment(SceneRoot);

    HeightFog = CreateDefaultSubobject<UExponentialHeightFogComponent>(TEXT("HeightFog"));
    HeightFog->SetupAttachment(SceneRoot);
    HeightFog->bEnableVolumetricFog = false;

    SimulatedStartDateTime = FDateTime(2026, 1, 1, 12, 0, 0);
    FixedDateTime = FDateTime(2026, 1, 1, 12, 0, 0);
    SimulatedCurrentDateTime = SimulatedStartDateTime;
}

void AARPGDayNightCycle::BeginPlay()
{
    Super::BeginPlay();

    ApplyRigVisibility();
    if (HasAuthority())
    {
        SimulatedCurrentDateTime = SimulatedStartDateTime;
        ForceClockSync();
    }
    else
    {
        ClientSyncRealTimeSeconds = GetWorld() ? GetWorld()->GetRealTimeSeconds() : 0.0;
    }

    UpdateLightingAndEvents(false);
}

void AARPGDayNightCycle::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    ApplyRigVisibility();

    if (!GetWorld() || !GetWorld()->IsGameWorld())
    {
        FDateTime PreviewTime = FDateTime::Now();
        if (TimeSource == EARPGWorldTimeSource::FixedTime) PreviewTime = FixedDateTime;
        else if (TimeSource == EARPGWorldTimeSource::SimulatedClock) PreviewTime = SimulatedStartDateTime;
        ApplyLighting(PreviewTime + FTimespan::FromHours(ClockOffsetHours));
    }
}

void AARPGDayNightCycle::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (HasAuthority())
    {
        UpdateAuthoritativeClock(DeltaSeconds);
    }

    UpdateLightingAndEvents(true);
}

void AARPGDayNightCycle::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AARPGDayNightCycle, ReplicatedHostDateTime);
    DOREPLIFETIME(AARPGDayNightCycle, ReplicatedClockRate);
}

void AARPGDayNightCycle::OnRep_ReplicatedClock()
{
    ClientSyncRealTimeSeconds = GetWorld() ? GetWorld()->GetRealTimeSeconds() : 0.0;
    UpdateLightingAndEvents(true);
}

FDateTime AARPGDayNightCycle::GetAuthorityDateTime() const
{
    FDateTime Base;
    switch (TimeSource)
    {
        case EARPGWorldTimeSource::FixedTime:
            Base = FixedDateTime;
            break;
        case EARPGWorldTimeSource::SimulatedClock:
            Base = SimulatedCurrentDateTime;
            break;
        case EARPGWorldTimeSource::HostSystemClock:
        default:
            Base = FDateTime::Now();
            break;
    }
    return Base + FTimespan::FromHours(ClockOffsetHours);
}

FDateTime AARPGDayNightCycle::GetVisualDateTime() const
{
    if (HasAuthority()) return GetAuthorityDateTime();

    if (ReplicatedHostDateTime.GetTicks() > 0)
    {
        const double NowReal = GetWorld() ? GetWorld()->GetRealTimeSeconds() : ClientSyncRealTimeSeconds;
        const double Elapsed = FMath::Max(0.0, NowReal - ClientSyncRealTimeSeconds);
        return ReplicatedHostDateTime + FTimespan::FromSeconds(Elapsed * static_cast<double>(ReplicatedClockRate));
    }

    // Only used for the short window before the first authoritative replication arrives.
    return FDateTime::Now() + FTimespan::FromHours(ClockOffsetHours);
}

FDateTime AARPGDayNightCycle::GetWorldDateTime() const
{
    return GetVisualDateTime();
}

float AARPGDayNightCycle::GetHourFromDateTime(const FDateTime& DateTime) const
{
    return static_cast<float>(DateTime.GetTimeOfDay().GetTotalHours());
}

float AARPGDayNightCycle::GetWorldHour() const
{
    return GetHourFromDateTime(GetVisualDateTime());
}

int32 AARPGDayNightCycle::GetWorldHourInteger() const
{
    return GetVisualDateTime().GetHour();
}

bool AARPGDayNightCycle::IsDay() const
{
    const float Hour = GetWorldHour();
    if (DayStartHour <= NightStartHour) return Hour >= DayStartHour && Hour < NightStartHour;
    return Hour >= DayStartHour || Hour < NightStartHour;
}

bool AARPGDayNightCycle::IsNight() const
{
    return !IsDay();
}

EARPGDayNightPhase AARPGDayNightCycle::EvaluatePhase(float Hour) const
{
    if (Hour >= DawnStartHour && Hour < DayStartHour) return EARPGDayNightPhase::Dawn;
    if (Hour >= DayStartHour && Hour < DuskStartHour) return EARPGDayNightPhase::Day;
    if (Hour >= DuskStartHour && Hour < NightStartHour) return EARPGDayNightPhase::Dusk;
    return EARPGDayNightPhase::Night;
}

EARPGDayNightPhase AARPGDayNightCycle::GetDayNightPhase() const
{
    return EvaluatePhase(GetWorldHour());
}

float AARPGDayNightCycle::EvaluateDaylight(float Hour) const
{
    const float Rise = FMath::Clamp(SunriseHour, 0.f, 24.f);
    const float Set = FMath::Clamp(SunsetHour, 0.f, 24.f);
    if (Set <= Rise + KINDA_SMALL_NUMBER) return IsDay() ? 1.f : 0.f;
    if (Hour <= Rise || Hour >= Set) return 0.f;

    const float Alpha = FMath::Clamp((Hour - Rise) / (Set - Rise), 0.f, 1.f);
    const float Solar = FMath::Max(0.f, FMath::Sin(Alpha * PI));
    return FMath::SmoothStep(0.f, 0.20f, Solar);
}

float AARPGDayNightCycle::GetDaylightAmount() const
{
    return EvaluateDaylight(GetWorldHour());
}

FString AARPGDayNightCycle::GetWorldTimeString(bool bUse24HourClock, bool bIncludeSeconds) const
{
    const FDateTime DT = GetVisualDateTime();
    if (bUse24HourClock)
    {
        return bIncludeSeconds ? DT.ToString(TEXT("%H:%M:%S")) : DT.ToString(TEXT("%H:%M"));
    }

    int32 Hour = DT.GetHour();
    const bool bPM = Hour >= 12;
    Hour %= 12;
    if (Hour == 0) Hour = 12;
    return bIncludeSeconds
        ? FString::Printf(TEXT("%d:%02d:%02d %s"), Hour, DT.GetMinute(), DT.GetSecond(), bPM ? TEXT("PM") : TEXT("AM"))
        : FString::Printf(TEXT("%d:%02d %s"), Hour, DT.GetMinute(), bPM ? TEXT("PM") : TEXT("AM"));
}

FString AARPGDayNightCycle::GetWorldDateString() const
{
    return GetVisualDateTime().ToString(TEXT("%Y-%m-%d"));
}

void AARPGDayNightCycle::UseHostSystemClock()
{
    if (!HasAuthority()) return;
    TimeSource = EARPGWorldTimeSource::HostSystemClock;
    ForceClockSync();
}

void AARPGDayNightCycle::SetFixedWorldTime(const FDateTime& NewDateTime)
{
    if (!HasAuthority()) return;
    TimeSource = EARPGWorldTimeSource::FixedTime;
    FixedDateTime = NewDateTime;
    ForceClockSync();
}

void AARPGDayNightCycle::StartSimulatedClock(const FDateTime& StartDateTime, float MinutesPerRealSecond)
{
    if (!HasAuthority()) return;
    TimeSource = EARPGWorldTimeSource::SimulatedClock;
    SimulatedStartDateTime = StartDateTime;
    SimulatedCurrentDateTime = StartDateTime;
    SimulatedMinutesPerRealSecond = FMath::Max(0.01f, MinutesPerRealSecond);
    ForceClockSync();
}

void AARPGDayNightCycle::ForceClockSync()
{
    if (!HasAuthority()) return;

    ReplicatedHostDateTime = GetAuthorityDateTime();
    switch (TimeSource)
    {
        case EARPGWorldTimeSource::FixedTime:
            ReplicatedClockRate = 0.f;
            break;
        case EARPGWorldTimeSource::SimulatedClock:
            ReplicatedClockRate = FMath::Max(0.01f, SimulatedMinutesPerRealSecond) * 60.f;
            break;
        case EARPGWorldTimeSource::HostSystemClock:
        default:
            ReplicatedClockRate = 1.f;
            break;
    }
    ReplicationAccumulator = 0.f;
    ForceNetUpdate();
}

void AARPGDayNightCycle::RefreshLightingNow()
{
    ApplyRigVisibility();
    ApplyLighting(GetVisualDateTime());
}

void AARPGDayNightCycle::UpdateAuthoritativeClock(float DeltaSeconds)
{
    if (TimeSource == EARPGWorldTimeSource::SimulatedClock)
    {
        SimulatedCurrentDateTime += FTimespan::FromMinutes(FMath::Max(0.01f, SimulatedMinutesPerRealSecond) * DeltaSeconds);
    }

    ReplicationAccumulator += DeltaSeconds;
    if (ReplicationAccumulator >= FMath::Max(0.25f, ClockReplicationInterval))
    {
        ForceClockSync();
    }
}

void AARPGDayNightCycle::UpdateLightingAndEvents(bool bBroadcastEvents)
{
    const FDateTime DateTime = GetVisualDateTime();
    ApplyLighting(DateTime);

    const float Hour = GetHourFromDateTime(DateTime);
    const EARPGDayNightPhase Phase = EvaluatePhase(Hour);
    if (!bBroadcastEvents)
    {
        LastPhase = Phase;
        LastHour = DateTime.GetHour();
        bHasInitializedPhase = true;
        return;
    }

    BroadcastPhaseIfChanged(Phase, DateTime.GetHour());
}

void AARPGDayNightCycle::BroadcastPhaseIfChanged(EARPGDayNightPhase NewPhase, int32 NewHour)
{
    if (!bHasInitializedPhase)
    {
        LastPhase = NewPhase;
        LastHour = NewHour;
        bHasInitializedPhase = true;
        return;
    }

    if (NewHour != LastHour)
    {
        LastHour = NewHour;
        OnHourChanged.Broadcast(NewHour);
    }

    if (NewPhase == LastPhase) return;
    LastPhase = NewPhase;
    OnPhaseChanged.Broadcast(NewPhase);

    switch (NewPhase)
    {
        case EARPGDayNightPhase::Dawn: OnDawnStarted.Broadcast(); break;
        case EARPGDayNightPhase::Day: OnDayStarted.Broadcast(); break;
        case EARPGDayNightPhase::Dusk: OnDuskStarted.Broadcast(); break;
        case EARPGDayNightPhase::Night: OnNightStarted.Broadcast(); break;
        default: break;
    }
}

void AARPGDayNightCycle::ApplyLighting(const FDateTime& DateTime)
{
    const float Hour = GetHourFromDateTime(DateTime);
    const float Daylight = EvaluateDaylight(Hour);
    const float Night = 1.f - Daylight;

    // Pitch convention: 06:00 = horizon, 12:00 = overhead/downward light, 18:00 = opposite horizon.
    const float SunPitch = 90.f - Hour * 15.f + SunPitchOffset;
    const FRotator SunRotation(SunPitch, SunYaw, 0.f);
    const FRotator MoonRotation(SunPitch + 180.f, SunYaw + MoonYawOffset, 0.f);

    UDirectionalLightComponent* Sun = ResolveSunComponent();
    UDirectionalLightComponent* Moon = ResolveMoonComponent();
    USkyLightComponent* Sky = ResolveSkyLightComponent();
    UExponentialHeightFogComponent* Fog = ResolveFogComponent();

    if (Sun)
    {
        Sun->SetWorldRotation(SunRotation);
        Sun->SetIntensity(SunMaxIntensityLux * Daylight);
        const float HorizonBlend = FMath::Clamp(Daylight * 1.35f, 0.f, 1.f);
        Sun->SetLightColor(FLinearColor::LerpUsingHSV(SunHorizonColor, SunNoonColor, HorizonBlend), false);
        if (!Sun->IsUsedAsAtmosphereSunLight()) Sun->SetAtmosphereSunLight(true);
        if (Sun->GetAtmosphereSunLightIndex() != 0) Sun->SetAtmosphereSunLightIndex(0);
    }

    if (Moon)
    {
        Moon->SetWorldRotation(MoonRotation);
        const float MoonVisibility = FMath::SmoothStep(0.20f, 0.85f, Night);
        Moon->SetIntensity(MoonMaxIntensityLux * MoonVisibility);
        Moon->SetLightColor(MoonColor, false);
        if (!Moon->IsUsedAsAtmosphereSunLight()) Moon->SetAtmosphereSunLight(true);
        if (Moon->GetAtmosphereSunLightIndex() != 1) Moon->SetAtmosphereSunLightIndex(1);
    }

    if (Sky)
    {
        Sky->SetIntensity(FMath::Lerp(SkyLightNightIntensity, SkyLightDayIntensity, Daylight));
        if (Sky->bRealTimeCapture != bUseRealTimeSkyCapture) Sky->SetRealTimeCapture(bUseRealTimeSkyCapture);
    }

    if (Fog)
    {
        Fog->SetFogDensity(FMath::Lerp(FogNightDensity, FogDayDensity, Daylight));
        Fog->SetFogInscatteringColor(FLinearColor::LerpUsingHSV(FogNightColor, FogDayColor, Daylight));
    }
}

void AARPGDayNightCycle::ApplyRigVisibility()
{
    if (SunLight) SunLight->SetVisibility(bUseBuiltInLightingRig, true);
    if (MoonLight) MoonLight->SetVisibility(bUseBuiltInLightingRig, true);
    if (SkyLight) SkyLight->SetVisibility(bUseBuiltInLightingRig, true);
    if (SkyAtmosphere) SkyAtmosphere->SetVisibility(bUseBuiltInLightingRig, true);
    if (HeightFog) HeightFog->SetVisibility(bUseBuiltInLightingRig, true);
}

UDirectionalLightComponent* AARPGDayNightCycle::ResolveSunComponent() const
{
    // ADirectionalLight::GetComponent() is not available in non-editor/package targets in UE 5.8.
    // Resolve the runtime component through AActor instead so external lighting works in Editor and packaged builds.
    return bUseBuiltInLightingRig ? SunLight.Get() : (ExternalSunLight ? ExternalSunLight->FindComponentByClass<UDirectionalLightComponent>() : nullptr);
}

UDirectionalLightComponent* AARPGDayNightCycle::ResolveMoonComponent() const
{
    return bUseBuiltInLightingRig ? MoonLight.Get() : (ExternalMoonLight ? ExternalMoonLight->FindComponentByClass<UDirectionalLightComponent>() : nullptr);
}

USkyLightComponent* AARPGDayNightCycle::ResolveSkyLightComponent() const
{
    return bUseBuiltInLightingRig ? SkyLight.Get() : (ExternalSkyLight ? ExternalSkyLight->GetLightComponent() : nullptr);
}

UExponentialHeightFogComponent* AARPGDayNightCycle::ResolveFogComponent() const
{
    return bUseBuiltInLightingRig ? HeightFog.Get() : (ExternalHeightFog ? ExternalHeightFog->GetComponent() : nullptr);
}
