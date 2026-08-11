#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "ARPGWandererComponent.generated.h"

UCLASS(ClassGroup=(ARPG), meta=(BlueprintSpawnableComponent))
class AKUMASRPGFRAMEWORK_API UARPGWandererComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UARPGWandererComponent();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wanderer") bool bEnabled = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wanderer") bool bStayNearHome = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wanderer", meta=(ClampMin="100")) float WanderRadius = 5000.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wanderer", meta=(ClampMin="0.1")) float ThinkInterval = 5.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wanderer", meta=(ClampMin="5.0")) float AcceptanceRadius = 75.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wanderer") bool bPauseDuringCombat = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wanderer") bool bStopMovementWhenDisabled = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wanderer") FGameplayTagContainer ActivityTags;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Wanderer") FVector HomeLocation = FVector::ZeroVector;

    UFUNCTION(BlueprintCallable, Category="ARPG|Wanderer") void SetWandererEnabled(bool bNewEnabled);
    UFUNCTION(BlueprintCallable, Category="ARPG|Wanderer") void SetHomeLocation(const FVector& NewHomeLocation);
    UFUNCTION(BlueprintCallable, Category="ARPG|Wanderer") void ForceChooseNewDestination();
    UFUNCTION(BlueprintCallable, Category="ARPG|Wanderer") void ForceReturnHome();

    /** Native coordination hook for temporary AI owners (social encounters, group recovery, etc.).
     *  This never changes the authored bEnabled flag, so temporary pauses cannot permanently disable Free Roam. */
    void AcquireMovementPause(FName PauseReason, bool bStopCurrentMovement = true);
    /** Releases only the caller's pause. If this was the last pause, the normal Wanderer timer is restored. */
    void ReleaseMovementPause(FName PauseReason, bool bChooseNewDestinationImmediately = true);
    bool IsMovementPaused() const { return MovementPauseReasons.Num() > 0; }
    bool HasMovementPauseOtherThan(FName AllowedReason) const;
    /** True only after Free Roam has produced real actor translation from an accepted navigation request. Native-only locomotion health hook. */
    bool HasEstablishedFreeRoam() const { return bHasEstablishedFreeRoam; }

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
protected:
    FTimerHandle ThinkTimer;
    FTimerHandle MovementRetryTimer;
    FTimerHandle MovementProofTimer;
    TSet<FName> MovementPauseReasons;
    bool bHasEstablishedFreeRoam = false;
    bool bAwaitingMovementProof = false;
    bool bWarnedLocomotionStall = false;
    int32 MovementRetryCount = 0;
    int32 MovementProofChecks = 0;
    FVector MovementProofStartLocation = FVector::ZeroVector;
    void Think();
    void EnsureThinkTimer();
    void ScheduleMovementRetry(float MinDelay = 0.10f, float MaxDelay = 0.30f);
    void MarkNavigationRequestAccepted();
    void VerifyNavigationMovement();
    void CancelMovementProof();
};
