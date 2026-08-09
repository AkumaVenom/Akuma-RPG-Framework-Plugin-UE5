#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"
#include "GameplayTagContainer.h"
#include "ARPGTargetingComponent.generated.h"

class APlayerController;
class UARPGCombatComponent;
class UARPGTargetMarkerWidget;
class UMaterialInterface;
class UTexture2D;
class UWidgetComponent;
class UGameplayAbility;
class USpringArmComponent;
class UCameraComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FARPGTargetingTargetChanged, AActor*, OldTarget, AActor*, NewTarget);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FARPGTargetingLockStateChanged, bool, bLockedOn);

/**
 * Player-facing TERA-style lock-on targeting.
 * ARPGCharacter owns one by default; normal Blueprint setup only needs ToggleLockOn bound to input.
 */
UCLASS(ClassGroup=(ARPG), meta=(BlueprintSpawnableComponent))
class AKUMASRPGFRAMEWORK_API UARPGTargetingComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UARPGTargetingComponent();

    // --- Acquisition / maintenance ---
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Targeting|General") bool bEnabled = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Targeting|General") bool bOnlyHostileTargets = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Targeting|General") bool bRequireLineOfSightOnAcquire = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Targeting|General") bool bRequireLineOfSightToMaintain = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Targeting|General") bool bAutoReacquireWhenTargetLost = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Targeting|General", meta=(ClampMin="100.0")) float MaxAcquireDistance = 3000.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Targeting|General", meta=(ClampMin="100.0")) float MaxMaintainDistance = 3800.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Targeting|General", meta=(ClampMin="1.0", ClampMax="180.0")) float AcquireHalfAngleDegrees = 72.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Targeting|General", meta=(ClampMin="0.0")) float LostLineOfSightGraceSeconds = 0.65f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Targeting|General") TEnumAsByte<ECollisionChannel> VisibilityTraceChannel = ECC_Visibility;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Targeting|General") FName TargetAimSocketName = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Targeting|Scoring", meta=(ClampMin="0.0")) float ScreenCenterWeight = 0.72f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Targeting|Scoring", meta=(ClampMin="0.0")) float DistanceWeight = 0.28f;

    // --- Target switching ---
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Targeting|Switching") bool bAllowTargetSwitching = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Targeting|Switching") bool bWrapTargetSwitching = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Targeting|Switching", meta=(ClampMin="100.0")) float MaxSwitchDistance = 3500.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Targeting|Switching", meta=(ClampMin="0.1")) float MinimumSwitchAngleDegrees = 2.f;

    // --- Facing / action integration ---
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Targeting|Facing") bool bAutoFaceOnBasicAttack = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Targeting|Facing") bool bAutoFaceOnAbilities = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Targeting|Facing") bool bFaceContinuouslyWhileLocked = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Targeting|Facing") bool bIgnorePitchWhenFacing = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Targeting|Facing") bool bSmoothFacing = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Targeting|Facing", meta=(ClampMin="0.1")) float RotationInterpSpeed = 16.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Targeting|Facing", meta=(ClampMin="0.0")) float ActionFacingDuration = 0.70f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Targeting|Facing") FGameplayTagContainer AbilityFacingIgnoredTags;

    // --- Z-target / camera lock ---
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Targeting|Camera Lock") bool bLockCameraToTarget = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Targeting|Camera Lock") bool bSmoothCameraLock = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Targeting|Camera Lock", meta=(ClampMin="0.1")) float CameraRotationInterpSpeed = 11.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Targeting|Camera Lock") bool bCameraTracksTargetPitch = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Targeting|Camera Lock", meta=(ClampMin="-89.0", ClampMax="89.0")) float CameraMinPitch = -60.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Targeting|Camera Lock", meta=(ClampMin="-89.0", ClampMax="89.0")) float CameraMaxPitch = 55.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Targeting|Camera Lock", meta=(ClampMin="-45.0", ClampMax="45.0")) float CameraPitchOffsetDegrees = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Targeting|Camera Lock") bool bOverrideMovementFacingWhileLocked = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Targeting|Camera Lock") bool bAutoConfigureCameraRigForLock = true;

    // --- Automatic target marker ---
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Targeting|Marker") bool bAutoCreateTargetMarker = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Targeting|Marker") TSubclassOf<UARPGTargetMarkerWidget> TargetMarkerWidgetClass;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Targeting|Marker") TSoftObjectPtr<UTexture2D> TargetMarkerTexture;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Targeting|Marker") TSoftObjectPtr<UMaterialInterface> TargetMarkerMaterial;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Targeting|Marker") FLinearColor TargetMarkerColor = FLinearColor(1.f, 0.12f, 0.04f, 1.f);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Targeting|Marker", meta=(ClampMin="8.0")) FVector2D TargetMarkerSize = FVector2D(58.f, 58.f);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Targeting|Marker") float TargetMarkerHeightOffset = 26.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Targeting|Marker") FName TargetMarkerSocketName = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Targeting|Marker") bool bAnimateMarkerOnAcquire = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Targeting|Marker", meta=(ClampMin="0.01")) float MarkerAcquireAnimationDuration = 0.18f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Targeting|Marker") bool bPulseMarkerWhileLocked = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Targeting|Marker", meta=(ClampMin="0.0")) float MarkerPulseSpeed = 2.4f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Targeting|Marker", meta=(ClampMin="0.0", ClampMax="0.5")) float MarkerPulseScale = 0.05f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Targeting|Marker", meta=(ClampMin="0.01")) float MarkerReleaseAnimationDuration = 0.14f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Targeting|Runtime") TObjectPtr<AActor> CurrentTarget;
    UPROPERTY(BlueprintAssignable, Category="Targeting|Events") FARPGTargetingTargetChanged OnTargetChanged;
    UPROPERTY(BlueprintAssignable, Category="Targeting|Events") FARPGTargetingLockStateChanged OnLockStateChanged;

    UFUNCTION(BlueprintCallable, Category="ARPG|Targeting|Input") bool ToggleLockOn();
    UFUNCTION(BlueprintCallable, Category="ARPG|Targeting|Input") bool LockOnBestTarget();
    UFUNCTION(BlueprintCallable, Category="ARPG|Targeting|Input") bool SwitchTargetLeft();
    UFUNCTION(BlueprintCallable, Category="ARPG|Targeting|Input") bool SwitchTargetRight();
    UFUNCTION(BlueprintCallable, Category="ARPG|Targeting|Input") void UnlockTarget();
    UFUNCTION(BlueprintCallable, Category="ARPG|Targeting") bool SetLockOnTarget(AActor* NewTarget);
    UFUNCTION(BlueprintPure, Category="ARPG|Targeting") AActor* GetCurrentTarget() const;
    UFUNCTION(BlueprintPure, Category="ARPG|Targeting") bool IsLockedOn() const { return IsValid(CurrentTarget); }
    UFUNCTION(BlueprintPure, Category="ARPG|Targeting") bool IsValidLockOnTarget(AActor* Candidate) const;
    UFUNCTION(BlueprintPure, Category="ARPG|Targeting") FVector GetTargetAimLocation(AActor* Target) const;
    UFUNCTION(BlueprintCallable, Category="ARPG|Targeting|Facing") void RequestAttackFacing();
    UFUNCTION(BlueprintCallable, Category="ARPG|Targeting|Facing") void RequestAbilityFacing();
    UFUNCTION(BlueprintCallable, Category="ARPG|Targeting|Facing") void RequestActionFacing(float Duration, bool bInstant = false);
    UFUNCTION(BlueprintCallable, Category="ARPG|Targeting|Facing") void FaceCurrentTargetNow(bool bInstant = true);

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
    UFUNCTION(Server, Reliable) void ServerSetLockOnTarget(AActor* NewTarget);
    UFUNCTION(Client, Reliable) void ClientConfirmLockOnTarget(AActor* ConfirmedTarget);
    UFUNCTION() void HandleCombatTargetChanged(AActor* NewCombatTarget);
    void HandleAbilityActivated(UGameplayAbility* Ability);

    APlayerController* GetOwningPlayerController() const;
    bool IsLocallyControlledPlayer() const;
    bool HasLineOfSightToTarget(AActor* Candidate) const;
    void GetViewData(FVector& OutLocation, FVector& OutForward, FVector& OutRight) const;
    TArray<AActor*> GatherCandidates(float Radius) const;
    AActor* FindBestTargetInternal() const;
    AActor* FindSwitchTargetInternal(bool bRight) const;
    float ScoreCandidate(AActor* Candidate, const FVector& ViewLocation, const FVector& ViewForward) const;
    void ApplyTargetLocal(AActor* NewTarget, bool bSyncCombatTarget);
    void ValidateCurrentTarget(float DeltaTime);
    void UpdateFacing(float DeltaTime);
    void UpdateCameraLock(float DeltaTime);
    void ApplyLockedMovementFacing(bool bLocked);
    void ApplyLockedCameraRig(bool bLocked);
    void SetLockedOnGameplayTag(bool bLocked) const;

    void CreateTargetMarker(AActor* Target);
    void DestroyTargetMarker(bool bPlayReleaseAnimation);
    void UpdateTargetMarkerLocation();

    UPROPERTY(Transient) TObjectPtr<UWidgetComponent> TargetMarkerComponent;
    UPROPERTY(Transient) TObjectPtr<UARPGTargetMarkerWidget> TargetMarkerWidget;
    float LineOfSightLostAt = -1.f;
    float ForceFacingUntil = -1.f;
    bool bApplyingCombatTargetChange = false;

    bool bMovementFacingCached = false;
    bool bCachedUseControllerRotationYaw = false;
    bool bCachedOrientRotationToMovement = false;
    bool bCachedUseControllerDesiredRotation = false;

    TWeakObjectPtr<USpringArmComponent> CachedSpringArm;
    TWeakObjectPtr<UCameraComponent> CachedCamera;
    bool bCameraRigLockCached = false;
    bool bCachedSpringArmUsePawnControlRotation = false;
    bool bCachedCameraUsePawnControlRotation = false;
};
