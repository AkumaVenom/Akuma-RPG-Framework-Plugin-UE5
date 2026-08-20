#pragma once
#include "CoreMinimal.h"
#include "Building/ARPGBuildPieceActor.h"
#include "ARPGBuildLightActor.generated.h"

class UPointLightComponent;
class USpotLightComponent;
class UNiagaraComponent;
class UParticleSystemComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FARPGBuildLightStateChanged, bool, bIsOn);

/**
 * Native replicated buildable light fixture.
 *
 * The actor deliberately keeps imported build-mesh collision disabled: buildable lamps/torches are
 * decorative surface occupants and must never regress the framework's structural Wall/Stair/Floor
 * coexistence rules. Interaction is resolved semantically from the player's view, while the server
 * still validates range, faction access and replicated state.
 */
UCLASS(BlueprintType, Blueprintable)
class AKUMASRPGFRAMEWORK_API AARPGBuildLightActor : public AARPGBuildPieceActor
{
    GENERATED_BODY()
public:
    AARPGBuildLightActor();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Build Light|Components")
    TObjectPtr<UPointLightComponent> PointLight;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Build Light|Components")
    TObjectPtr<USpotLightComponent> SpotLight;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Build Light|Components")
    TObjectPtr<UNiagaraComponent> NiagaraEffect;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Build Light|Components")
    TObjectPtr<UParticleSystemComponent> CascadeEffect;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, ReplicatedUsing=OnRep_LightOn, SaveGame, Category="Build Light")
    bool bLightOn = false;

    UPROPERTY(BlueprintAssignable, Category="Build Light")
    FARPGBuildLightStateChanged OnLightStateChanged;

    UFUNCTION(BlueprintCallable, Category="ARPG|Building|Light", meta=(BlueprintAuthorityOnly))
    bool SetLightOn(bool bOn, AActor* Requester = nullptr);

    UFUNCTION(BlueprintCallable, Category="ARPG|Building|Light", meta=(BlueprintAuthorityOnly))
    bool ToggleLight(AActor* Requester = nullptr);

    UFUNCTION(BlueprintCallable, Category="ARPG|Building|Light", meta=(BlueprintAuthorityOnly))
    void RestoreLightState(bool bOn);

    UFUNCTION(BlueprintPure, Category="ARPG|Building|Light")
    bool IsLightOn() const { return bLightOn; }

    UFUNCTION(BlueprintPure, Category="ARPG|Building|Light")
    bool IsLightFading() const { return bLightFadeActive; }

    virtual void InitializeBuilding(UARPGBuildPieceDefinition* InDefinition, AActor* Builder) override;
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
    UFUNCTION()
    void OnRep_LightOn();

    virtual void RefreshDefinitionPresentation() override;
    virtual void RefreshConstructionPresentation(bool bForce = false) override;

private:
    bool bLightFadeActive = false;
    float LightFadeElapsed = 0.f;
    float LightFadeStartAlpha = 0.f;
    float LightFadeTargetAlpha = 0.f;
    float CurrentLightAlpha = 0.f;

    void RefreshLightConfiguration();
    void BeginLightFade(bool bPlaySound);
    void ApplyLightAlpha(float Alpha);
    void ApplyEffects(bool bActive);
    void FinishLightFade();
    void ApplyStateImmediately();
    void UpdateTickOwnership();
};
