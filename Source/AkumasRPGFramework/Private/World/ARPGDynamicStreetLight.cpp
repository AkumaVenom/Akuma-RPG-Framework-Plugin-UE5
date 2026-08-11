#include "World/ARPGDynamicStreetLight.h"

#include "AkumasRPGFramework.h"
#include "Components/LightComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "NiagaraComponent.h"
#include "Particles/ParticleSystemComponent.h"
#include "TimerManager.h"

AARPGDynamicStreetLight::AARPGDynamicStreetLight()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = false; // Cosmetic state derives from the already-replicated ARPGDayNightCycle.

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(SceneRoot);

    LampLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("LampLight"));
    LampLight->SetupAttachment(SceneRoot);
    LampLight->SetMobility(EComponentMobility::Movable);
    LampLight->SetIntensity(2500.f);
    LampLight->SetAttenuationRadius(850.f);
    LampLight->SetLightColor(FLinearColor(1.f, 0.55f, 0.22f), false);

    NiagaraEffect = CreateDefaultSubobject<UNiagaraComponent>(TEXT("NiagaraEffect"));
    NiagaraEffect->SetupAttachment(SceneRoot);
    NiagaraEffect->SetAutoActivate(false);

    CascadeEffect = CreateDefaultSubobject<UParticleSystemComponent>(TEXT("CascadeEffect"));
    CascadeEffect->SetupAttachment(SceneRoot);
    CascadeEffect->SetAutoActivate(false);
}

void AARPGDynamicStreetLight::BeginPlay()
{
    Super::BeginPlay();

    if (bEnableAutomaticDayNightControl)
    {
        ResolveAndBindDayNightCycle();
        RefreshFromDayNightCycleNow();
    }
    else
    {
        ApplyStreetLightState(bManualLightOn, true);
    }
}

void AARPGDynamicStreetLight::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    ClearCycleResolveRetry();
    UnbindFromDayNightCycle();
    Super::EndPlay(EndPlayReason);
}

void AARPGDynamicStreetLight::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);

    // Construction scripts are not a safe place to activate FX simulations. Preview visibility only;
    // BeginPlay owns the actual runtime activation/deactivation path.
    const bool bPreviewOn = (!GetWorld() || !GetWorld()->IsGameWorld())
        ? bEditorPreviewOn
        : (bEnableAutomaticDayNightControl ? bStreetLightOn : bManualLightOn);

    bStreetLightOn = bPreviewOn;
    ApplyOwnedLights(bPreviewOn);
    if (NiagaraEffect) NiagaraEffect->SetVisibility(bPreviewOn, true);
    if (CascadeEffect) CascadeEffect->SetVisibility(bPreviewOn, true);
}

bool AARPGDynamicStreetLight::ShouldBeOnForPhase(EARPGDayNightPhase Phase) const
{
    switch (Phase)
    {
        case EARPGDayNightPhase::Dawn: return bActiveDuringDawn;
        case EARPGDayNightPhase::Day: return bActiveDuringDay;
        case EARPGDayNightPhase::Dusk: return bActiveDuringDusk;
        case EARPGDayNightPhase::Night: return bActiveDuringNight;
        default: return false;
    }
}

void AARPGDynamicStreetLight::RefreshFromDayNightCycleNow()
{
    if (!bEnableAutomaticDayNightControl)
    {
        ApplyStreetLightState(bManualLightOn, true);
        return;
    }

    if (!IsValid(ResolvedDayNightCycle) || (DayNightCycleOverride && ResolvedDayNightCycle != DayNightCycleOverride))
    {
        ResolveAndBindDayNightCycle();
    }

    if (IsValid(ResolvedDayNightCycle))
    {
        bWarnedMissingCycle = false;
        ApplyStreetLightState(ShouldBeOnForPhase(ResolvedDayNightCycle->GetDayNightPhase()), true);
        return;
    }

    if (bTurnOffWhenCycleUnavailable)
    {
        ApplyStreetLightState(false, true);
    }

    if (!bWarnedMissingCycle)
    {
        bWarnedMissingCycle = true;
        UE_LOG(LogARPG, Warning, TEXT("Dynamic Street Light '%s' has automatic Day/Night control enabled but no ARPGDayNightCycle is available yet. The lamp will retry discovery."), *GetName());
    }

    ScheduleCycleResolveRetry();
}

void AARPGDynamicStreetLight::SetAutomaticDayNightControlEnabled(bool bEnabled)
{
    if (bEnableAutomaticDayNightControl == bEnabled)
    {
        if (bEnabled) RefreshFromDayNightCycleNow();
        else ApplyStreetLightState(bManualLightOn, true);
        return;
    }

    bEnableAutomaticDayNightControl = bEnabled;
    if (bEnabled)
    {
        ResolveAndBindDayNightCycle();
        RefreshFromDayNightCycleNow();
    }
    else
    {
        ClearCycleResolveRetry();
        UnbindFromDayNightCycle();
        ApplyStreetLightState(bManualLightOn, true);
    }
}

void AARPGDynamicStreetLight::SetManualLightState(bool bNewOn)
{
    bManualLightOn = bNewOn;
    bEnableAutomaticDayNightControl = false;
    ClearCycleResolveRetry();
    UnbindFromDayNightCycle();
    ApplyStreetLightState(bNewOn, true);
}

void AARPGDynamicStreetLight::ForceStreetLightState(bool bNewOn)
{
    ApplyStreetLightState(bNewOn, true);
}

void AARPGDynamicStreetLight::ResolveAndBindDayNightCycle()
{
    if (!bEnableAutomaticDayNightControl)
    {
        ClearCycleResolveRetry();
        UnbindFromDayNightCycle();
        return;
    }

    AARPGDayNightCycle* DesiredCycle = nullptr;
    if (IsValid(DayNightCycleOverride))
    {
        DesiredCycle = DayNightCycleOverride;
    }
    else if (bAutoDiscoverDayNightCycle && GetWorld())
    {
        for (TActorIterator<AARPGDayNightCycle> It(GetWorld()); It; ++It)
        {
            DesiredCycle = *It;
            break;
        }
    }

    if (DesiredCycle == ResolvedDayNightCycle && IsValid(ResolvedDayNightCycle))
    {
        ClearCycleResolveRetry();
        return;
    }

    UnbindFromDayNightCycle();
    if (IsValid(DesiredCycle))
    {
        BindToDayNightCycle(DesiredCycle);
        bWarnedMissingCycle = false;
        ClearCycleResolveRetry();
    }
    else
    {
        ScheduleCycleResolveRetry();
    }
}

void AARPGDynamicStreetLight::BindToDayNightCycle(AARPGDayNightCycle* NewCycle)
{
    if (!IsValid(NewCycle)) return;

    ResolvedDayNightCycle = NewCycle;
    ResolvedDayNightCycle->OnPhaseChanged.AddUniqueDynamic(this, &AARPGDynamicStreetLight::HandleDayNightPhaseChanged);
    // Hour changes are a cheap safety refresh for manual/simulated clock jumps and designer-edited phase boundaries.
    ResolvedDayNightCycle->OnHourChanged.AddUniqueDynamic(this, &AARPGDynamicStreetLight::HandleDayNightHourChanged);
    ResolvedDayNightCycle->OnDestroyed.AddUniqueDynamic(this, &AARPGDynamicStreetLight::HandleDayNightCycleDestroyed);
}

void AARPGDynamicStreetLight::UnbindFromDayNightCycle()
{
    if (IsValid(ResolvedDayNightCycle))
    {
        ResolvedDayNightCycle->OnPhaseChanged.RemoveDynamic(this, &AARPGDynamicStreetLight::HandleDayNightPhaseChanged);
        ResolvedDayNightCycle->OnHourChanged.RemoveDynamic(this, &AARPGDynamicStreetLight::HandleDayNightHourChanged);
        ResolvedDayNightCycle->OnDestroyed.RemoveDynamic(this, &AARPGDynamicStreetLight::HandleDayNightCycleDestroyed);
    }
    ResolvedDayNightCycle = nullptr;
}

void AARPGDynamicStreetLight::ScheduleCycleResolveRetry()
{
    if (!bEnableAutomaticDayNightControl || !GetWorld()) return;
    if (GetWorldTimerManager().IsTimerActive(CycleResolveRetryTimer)) return;

    GetWorldTimerManager().SetTimer(
        CycleResolveRetryTimer,
        this,
        &AARPGDynamicStreetLight::RefreshFromDayNightCycleNow,
        FMath::Max(0.25f, CycleResolveRetrySeconds),
        true);
}

void AARPGDynamicStreetLight::ClearCycleResolveRetry()
{
    if (GetWorld()) GetWorldTimerManager().ClearTimer(CycleResolveRetryTimer);
}

void AARPGDynamicStreetLight::ApplyStreetLightState(bool bNewOn, bool bBroadcastChange)
{
    const bool bNeedsRuntimeNotification = !bHasAppliedRuntimeState || bStreetLightOn != bNewOn;
    bStreetLightOn = bNewOn;

    ApplyOwnedLights(bNewOn);
    ApplyEffects(bNewOn);

    if (bBroadcastChange && GetWorld() && GetWorld()->IsGameWorld())
    {
        bHasAppliedRuntimeState = true;
        if (bNeedsRuntimeNotification)
        {
            OnStreetLightStateChanged.Broadcast(bNewOn);
            ReceiveStreetLightStateChanged(bNewOn);
        }
    }
}

void AARPGDynamicStreetLight::ApplyOwnedLights(bool bNewOn)
{
    if (!bControlAllOwnedLightComponents)
    {
        if (LampLight) LampLight->SetVisibility(bNewOn, true);
        return;
    }

    TInlineComponentArray<ULightComponent*> LightComponents;
    GetComponents(LightComponents);
    for (ULightComponent* LightComponent : LightComponents)
    {
        if (IsValid(LightComponent)) LightComponent->SetVisibility(bNewOn, true);
    }
}

void AARPGDynamicStreetLight::ApplyEffects(bool bNewOn)
{
    bool bUseNiagara = false;
    bool bUseCascade = false;

    if (bNewOn)
    {
        const bool bHasNiagara = NiagaraEffect && NiagaraEffect->GetFXSystemAsset() != nullptr;
        const bool bHasCascade = CascadeEffect && CascadeEffect->GetFXSystemAsset() != nullptr;

        switch (FXMode)
        {
            case EARPGDynamicStreetLightFXMode::NiagaraPreferredWithCascadeFallback:
                bUseNiagara = bHasNiagara;
                bUseCascade = !bHasNiagara && bHasCascade;
                break;
            case EARPGDynamicStreetLightFXMode::NiagaraOnly:
                bUseNiagara = bHasNiagara;
                break;
            case EARPGDynamicStreetLightFXMode::CascadeOnly:
                bUseCascade = bHasCascade;
                break;
            case EARPGDynamicStreetLightFXMode::NiagaraAndCascade:
                bUseNiagara = bHasNiagara;
                bUseCascade = bHasCascade;
                break;
            case EARPGDynamicStreetLightFXMode::None:
            default:
                break;
        }
    }

    SetNiagaraActive(bUseNiagara);
    SetCascadeActive(bUseCascade);
}

void AARPGDynamicStreetLight::SetNiagaraActive(bool bNewActive)
{
    if (!NiagaraEffect) return;

    NiagaraEffect->SetVisibility(bNewActive, true);
    if (bNewActive)
    {
        NiagaraEffect->Activate(bResetEffectsOnActivation);
    }
    else
    {
        NiagaraEffect->DeactivateImmediate();
    }
}

void AARPGDynamicStreetLight::SetCascadeActive(bool bNewActive)
{
    if (!CascadeEffect) return;

    CascadeEffect->SetVisibility(bNewActive, true);
    if (bNewActive)
    {
        CascadeEffect->Activate(bResetEffectsOnActivation);
    }
    else
    {
        CascadeEffect->DeactivateImmediate();
    }
}

void AARPGDynamicStreetLight::HandleDayNightPhaseChanged(EARPGDayNightPhase NewPhase)
{
    if (!bEnableAutomaticDayNightControl) return;
    ApplyStreetLightState(ShouldBeOnForPhase(NewPhase), true);
}

void AARPGDynamicStreetLight::HandleDayNightHourChanged(int32 NewHour)
{
    (void)NewHour;
    if (!bEnableAutomaticDayNightControl || !IsValid(ResolvedDayNightCycle)) return;
    ApplyStreetLightState(ShouldBeOnForPhase(ResolvedDayNightCycle->GetDayNightPhase()), true);
}

void AARPGDynamicStreetLight::HandleDayNightCycleDestroyed(AActor* DestroyedActor)
{
    if (DestroyedActor != ResolvedDayNightCycle) return;

    ResolvedDayNightCycle = nullptr;
    if (bTurnOffWhenCycleUnavailable) ApplyStreetLightState(false, true);
    ScheduleCycleResolveRetry();
}
