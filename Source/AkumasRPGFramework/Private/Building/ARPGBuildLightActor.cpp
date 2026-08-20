#include "Building/ARPGBuildLightActor.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleSystemComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"

AARPGBuildLightActor::AARPGBuildLightActor()
{
    PointLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("PointLight"));
    PointLight->SetupAttachment(RootComponent);
    PointLight->SetMobility(EComponentMobility::Movable);
    PointLight->SetVisibility(false, true);
    PointLight->SetIntensity(0.f);
    PointLight->SetCanEverAffectNavigation(false);

    SpotLight = CreateDefaultSubobject<USpotLightComponent>(TEXT("SpotLight"));
    SpotLight->SetupAttachment(RootComponent);
    SpotLight->SetMobility(EComponentMobility::Movable);
    SpotLight->SetVisibility(false, true);
    SpotLight->SetIntensity(0.f);
    SpotLight->SetCanEverAffectNavigation(false);

    NiagaraEffect = CreateDefaultSubobject<UNiagaraComponent>(TEXT("NiagaraEffect"));
    NiagaraEffect->SetupAttachment(RootComponent);
    NiagaraEffect->SetAutoActivate(false);
    NiagaraEffect->SetVisibility(false, true);

    CascadeEffect = CreateDefaultSubobject<UParticleSystemComponent>(TEXT("CascadeEffect"));
    CascadeEffect->SetupAttachment(RootComponent);
    CascadeEffect->bAutoActivate = false;
    CascadeEffect->SetVisibility(false, true);
}

void AARPGBuildLightActor::InitializeBuilding(UARPGBuildPieceDefinition* InDefinition, AActor* Builder)
{
    Super::InitializeBuilding(InDefinition, Builder);
    if (!HasAuthority() || !Definition || Definition->PieceKind != EARPGBuildPieceKind::Light) return;

    bLightOn = Definition->bLightStartsOn;
    ApplyStateImmediately();
    ForceNetUpdate();
}

void AARPGBuildLightActor::BeginPlay()
{
    Super::BeginPlay();
    RefreshLightConfiguration();
    ApplyStateImmediately();
}

void AARPGBuildLightActor::RefreshDefinitionPresentation()
{
    Super::RefreshDefinitionPresentation();

    // Buildable fixtures are intentionally non-blocking decorative occupants. Their imported mesh or
    // Physics Asset must never become a hidden structural blocker for Stairs, Walls, Floors or the
    // placement trace. Interaction uses the framework's view-corridor resolver instead.
    if (BuildMesh) BuildMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    if (BuildSkeletalMesh) BuildSkeletalMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    RefreshLightConfiguration();
    ApplyStateImmediately();
}

void AARPGBuildLightActor::RefreshConstructionPresentation(bool bForce)
{
    Super::RefreshConstructionPresentation(bForce);

    // Super may restore mesh collision after construction; fixtures must remain decorative/query-free.
    if (BuildMesh) BuildMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    if (BuildSkeletalMesh) BuildSkeletalMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    if (!IsConstructionComplete())
    {
        bLightFadeActive = false;
        ApplyEffects(false);
        ApplyLightAlpha(0.f);
    }
    else if (bForce)
    {
        ApplyStateImmediately();
    }
    UpdateTickOwnership();
}

bool AARPGBuildLightActor::SetLightOn(bool bOn, AActor* Requester)
{
    if (!HasAuthority() || !Definition || Definition->PieceKind != EARPGBuildPieceKind::Light ||
        !IsConstructionComplete() || (Requester && !CanActorUse(Requester)))
        return false;

    if (bLightOn == bOn)
        return true;

    bLightOn = bOn;
    BeginLightFade(true);
    OnLightStateChanged.Broadcast(bLightOn);
    ForceNetUpdate();
    return true;
}

bool AARPGBuildLightActor::ToggleLight(AActor* Requester)
{
    return SetLightOn(!bLightOn, Requester);
}

void AARPGBuildLightActor::RestoreLightState(bool bOn)
{
    if (!HasAuthority()) return;

    bLightOn = bOn;
    bLightFadeActive = false;
    ApplyStateImmediately();
    ForceNetUpdate();
}

void AARPGBuildLightActor::OnRep_LightOn()
{
    BeginLightFade(true);
    OnLightStateChanged.Broadcast(bLightOn);
}

void AARPGBuildLightActor::RefreshLightConfiguration()
{
    if (!Definition || Definition->PieceKind != EARPGBuildPieceKind::Light) return;

    const bool bUsePoint = Definition->BuildLightType == EARPGBuildLightType::Point;
    const bool bUseSpot = Definition->BuildLightType == EARPGBuildLightType::Spot;

    if (PointLight)
    {
        PointLight->SetRelativeTransform(Definition->LightComponentRelativeTransform);
        PointLight->SetVisibility(bUsePoint, true);
        PointLight->SetAttenuationRadius(FMath::Max(1.f, Definition->LightAttenuationRadius));
        PointLight->SetLightColor(Definition->LightColor, true);
        PointLight->SetUseTemperature(Definition->bLightUseTemperature);
        PointLight->SetTemperature(FMath::Clamp(Definition->LightTemperature, 1000.f, 15000.f));
        PointLight->SetCastShadows(Definition->bLightCastShadows);
        PointLight->SetSourceRadius(FMath::Max(0.f, Definition->LightSourceRadius));
        PointLight->SetSoftSourceRadius(FMath::Max(0.f, Definition->LightSoftSourceRadius));
    }

    if (SpotLight)
    {
        SpotLight->SetRelativeTransform(Definition->LightComponentRelativeTransform);
        SpotLight->SetVisibility(bUseSpot, true);
        SpotLight->SetAttenuationRadius(FMath::Max(1.f, Definition->LightAttenuationRadius));
        SpotLight->SetLightColor(Definition->LightColor, true);
        SpotLight->SetUseTemperature(Definition->bLightUseTemperature);
        SpotLight->SetTemperature(FMath::Clamp(Definition->LightTemperature, 1000.f, 15000.f));
        SpotLight->SetCastShadows(Definition->bLightCastShadows);
        SpotLight->SetSourceRadius(FMath::Max(0.f, Definition->LightSourceRadius));
        SpotLight->SetSoftSourceRadius(FMath::Max(0.f, Definition->LightSoftSourceRadius));
        SpotLight->SetInnerConeAngle(FMath::Clamp(Definition->LightSpotInnerConeAngle, 0.f, 89.f));
        SpotLight->SetOuterConeAngle(FMath::Clamp(Definition->LightSpotOuterConeAngle,
            FMath::Max(1.f, Definition->LightSpotInnerConeAngle), 89.f));
    }

    if (NiagaraEffect)
    {
        NiagaraEffect->SetRelativeTransform(Definition->LightEffectRelativeTransform);
        NiagaraEffect->SetAsset(Definition->LightNiagaraSystem.LoadSynchronous());
    }

    if (CascadeEffect)
    {
        CascadeEffect->SetRelativeTransform(Definition->LightEffectRelativeTransform);
        CascadeEffect->SetTemplate(Definition->LightCascadeSystem.LoadSynchronous());
    }
}

void AARPGBuildLightActor::BeginLightFade(bool bPlaySound)
{
    if (!Definition || Definition->PieceKind != EARPGBuildPieceKind::Light || !IsConstructionComplete())
    {
        bLightFadeActive = false;
        ApplyEffects(false);
        ApplyLightAlpha(0.f);
        UpdateTickOwnership();
        return;
    }

    RefreshLightConfiguration();

    const float Target = bLightOn ? 1.f : 0.f;
    const float Duration = FMath::Max(0.f, Definition->LightFadeSeconds);
    if (bPlaySound)
    {
        USoundBase* Sound = bLightOn ? Definition->LightOnSound.LoadSynchronous() : Definition->LightOffSound.LoadSynchronous();
        if (Sound) UGameplayStatics::PlaySoundAtLocation(this, Sound, GetActorLocation());
    }

    if (bLightOn)
        ApplyEffects(true);

    if (Duration <= KINDA_SMALL_NUMBER)
    {
        bLightFadeActive = false;
        ApplyLightAlpha(Target);
        if (!bLightOn) ApplyEffects(false);
        UpdateTickOwnership();
        return;
    }

    LightFadeElapsed = 0.f;
    LightFadeStartAlpha = CurrentLightAlpha;
    LightFadeTargetAlpha = Target;
    bLightFadeActive = !FMath::IsNearlyEqual(LightFadeStartAlpha, LightFadeTargetAlpha, 0.001f);

    if (!bLightFadeActive)
    {
        ApplyLightAlpha(Target);
        if (!bLightOn) ApplyEffects(false);
    }
    UpdateTickOwnership();
}

void AARPGBuildLightActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!bLightFadeActive || !Definition || !IsConstructionComplete())
    {
        UpdateTickOwnership();
        return;
    }

    const float Duration = FMath::Max(0.001f, Definition->LightFadeSeconds);
    LightFadeElapsed += FMath::Max(0.f, DeltaSeconds);
    const float LinearAlpha = FMath::Clamp(LightFadeElapsed / Duration, 0.f, 1.f);
    const float SmoothAlpha = FMath::SmoothStep(0.f, 1.f, LinearAlpha);
    ApplyLightAlpha(FMath::Lerp(LightFadeStartAlpha, LightFadeTargetAlpha, SmoothAlpha));

    if (LinearAlpha >= 1.f - KINDA_SMALL_NUMBER)
        FinishLightFade();
}

void AARPGBuildLightActor::FinishLightFade()
{
    bLightFadeActive = false;
    ApplyLightAlpha(bLightOn ? 1.f : 0.f);
    if (!bLightOn) ApplyEffects(false);
    UpdateTickOwnership();
}

void AARPGBuildLightActor::ApplyLightAlpha(float Alpha)
{
    CurrentLightAlpha = FMath::Clamp(Alpha, 0.f, 1.f);
    const float Intensity = Definition ? FMath::Max(0.f, Definition->LightIntensity) * CurrentLightAlpha : 0.f;

    const bool bRuntimeLightVisible = Definition && CurrentLightAlpha > KINDA_SMALL_NUMBER;
    if (PointLight)
    {
        PointLight->SetIntensity(Intensity);
        PointLight->SetVisibility(bRuntimeLightVisible && Definition->BuildLightType == EARPGBuildLightType::Point, true);
    }
    if (SpotLight)
    {
        SpotLight->SetIntensity(Intensity);
        SpotLight->SetVisibility(bRuntimeLightVisible && Definition->BuildLightType == EARPGBuildLightType::Spot, true);
    }

    if (Definition && !Definition->LightEmissiveMaterialParameter.IsNone())
    {
        const float Emissive = FMath::Lerp(
            Definition->LightEmissiveOffValue,
            Definition->LightEmissiveOnValue,
            CurrentLightAlpha);
        if (BuildMesh) BuildMesh->SetScalarParameterValueOnMaterials(Definition->LightEmissiveMaterialParameter, Emissive);
        if (BuildSkeletalMesh) BuildSkeletalMesh->SetScalarParameterValueOnMaterials(Definition->LightEmissiveMaterialParameter, Emissive);
    }
}

void AARPGBuildLightActor::ApplyEffects(bool bActive)
{
    bool bUseNiagara = false;
    bool bUseCascade = false;

    if (bActive && Definition)
    {
        const bool bHasNiagara = NiagaraEffect && NiagaraEffect->GetFXSystemAsset() != nullptr;
        const bool bHasCascade = CascadeEffect && CascadeEffect->GetFXSystemAsset() != nullptr;

        switch (Definition->LightFXMode)
        {
            case EARPGBuildLightFXMode::NiagaraPreferredWithCascadeFallback:
                bUseNiagara = bHasNiagara;
                bUseCascade = !bHasNiagara && bHasCascade;
                break;
            case EARPGBuildLightFXMode::NiagaraOnly:
                bUseNiagara = bHasNiagara;
                break;
            case EARPGBuildLightFXMode::CascadeOnly:
                bUseCascade = bHasCascade;
                break;
            case EARPGBuildLightFXMode::NiagaraAndCascade:
                bUseNiagara = bHasNiagara;
                bUseCascade = bHasCascade;
                break;
            case EARPGBuildLightFXMode::None:
            default:
                break;
        }
    }

    if (NiagaraEffect)
    {
        NiagaraEffect->SetVisibility(bUseNiagara, true);
        if (bUseNiagara)
            NiagaraEffect->Activate(Definition ? Definition->bResetLightEffectsOnActivation : true);
        else
            NiagaraEffect->DeactivateImmediate();
    }

    if (CascadeEffect)
    {
        CascadeEffect->SetVisibility(bUseCascade, true);
        if (bUseCascade)
            CascadeEffect->Activate(Definition ? Definition->bResetLightEffectsOnActivation : true);
        else
            CascadeEffect->DeactivateImmediate();
    }
}

void AARPGBuildLightActor::ApplyStateImmediately()
{
    bLightFadeActive = false;
    LightFadeElapsed = 0.f;

    const bool bShouldBeOn = Definition && Definition->PieceKind == EARPGBuildPieceKind::Light &&
        IsConstructionComplete() && bLightOn;

    RefreshLightConfiguration();
    ApplyEffects(bShouldBeOn);
    ApplyLightAlpha(bShouldBeOn ? 1.f : 0.f);
    UpdateTickOwnership();
}

void AARPGBuildLightActor::UpdateTickOwnership()
{
    // Parent construction uses the same actor Tick while incomplete. Once construction is complete,
    // the light owns Tick only for the short fade window.
    if (!IsConstructionComplete())
        SetActorTickEnabled(true);
    else
        SetActorTickEnabled(bLightFadeActive);
}

void AARPGBuildLightActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AARPGBuildLightActor, bLightOn);
}
