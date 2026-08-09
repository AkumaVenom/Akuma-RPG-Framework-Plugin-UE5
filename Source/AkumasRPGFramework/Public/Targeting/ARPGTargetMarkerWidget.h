#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ARPGTargetMarkerWidget.generated.h"

class UImage;
class UMaterialInterface;
class UTextBlock;
class UTexture2D;

/**
 * Native lock-on marker used by ARPGTargetingComponent.
 * It works without a Blueprint widget asset, but can also be subclassed for a project's own art.
 */
UCLASS(BlueprintType, Blueprintable)
class AKUMASRPGFRAMEWORK_API UARPGTargetMarkerWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="ARPG|Targeting|Marker")
    void ConfigureMarker(UTexture2D* Texture, UMaterialInterface* Material, FLinearColor InColor,
                         bool bInAnimateAcquire, float InAcquireDuration,
                         bool bInPulseWhileLocked, float InPulseSpeed, float InPulseScale,
                         float InReleaseDuration);

    UFUNCTION(BlueprintCallable, Category="ARPG|Targeting|Marker") void PlayAcquireAnimation();
    UFUNCTION(BlueprintCallable, Category="ARPG|Targeting|Marker") void PlayReleaseAnimation();
    UFUNCTION(BlueprintPure, Category="ARPG|Targeting|Marker") bool IsReleaseAnimationFinished() const { return bReleaseFinished; }

    UFUNCTION(BlueprintImplementableEvent, Category="ARPG|Targeting|Marker", meta=(DisplayName="On ARPG Marker Configured"))
    void BP_OnMarkerConfigured(FLinearColor InColor);
    UFUNCTION(BlueprintImplementableEvent, Category="ARPG|Targeting|Marker", meta=(DisplayName="On ARPG Target Acquired"))
    void BP_OnTargetAcquired();
    UFUNCTION(BlueprintImplementableEvent, Category="ARPG|Targeting|Marker", meta=(DisplayName="On ARPG Target Released"))
    void BP_OnTargetReleased();

    UPROPERTY(BlueprintReadOnly, Category="ARPG|Targeting|Marker") TObjectPtr<UImage> MarkerImage;
    UPROPERTY(BlueprintReadOnly, Category="ARPG|Targeting|Marker") TObjectPtr<UTextBlock> FallbackReticle;

protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

    void EnsureNativeLayout();
    void ApplyMarkerArt();

    UPROPERTY(Transient) TObjectPtr<UTexture2D> ActiveTexture;
    UPROPERTY(Transient) TObjectPtr<UMaterialInterface> ActiveMaterial;
    FLinearColor MarkerColor = FLinearColor::White;
    bool bAnimateAcquire = true;
    bool bPulseWhileLocked = true;
    bool bReleasing = false;
    bool bReleaseFinished = false;
    float AcquireDuration = 0.18f;
    float PulseSpeed = 2.4f;
    float PulseScale = 0.05f;
    float ReleaseDuration = 0.14f;
    float AnimationElapsed = 0.f;
};
