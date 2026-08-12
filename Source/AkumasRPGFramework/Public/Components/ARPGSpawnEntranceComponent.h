#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ARPGSpawnEntranceComponent.generated.h"

class ACharacter;
class USkeletalMeshComponent;

/** Compact replicated state for the visual ground-rise entrance. */
USTRUCT()
struct FARPGSpawnEntranceRepState
{
    GENERATED_BODY()

    UPROPERTY() bool bActive = false;
    UPROPERTY() uint16 Sequence = 0;
    UPROPERTY() float StartServerTime = 0.f;
    UPROPERTY() float StartDelay = 0.f;
    UPROPERTY() float Duration = 1.f;
    UPROPERTY() float Depth = 200.f;
    UPROPERTY() float EaseExponent = 2.f;
};

/**
 * Replicated, short-lived spawn presentation used by ARPG AI spawners.
 *
 * The real Character/capsule never leaves its collision-safe final spawn position. Only the
 * skeletal mesh receives a temporary relative Z offset, so the NPC appears to rise through the
 * ground without reintroducing underground capsules, floor penetration or NavMesh invalidation.
 * Authority movement/AI owners are paused for the entrance and restored afterward.
 */
UCLASS(ClassGroup=(ARPG), meta=(BlueprintSpawnableComponent))
class AKUMASRPGFRAMEWORK_API UARPGSpawnEntranceComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UARPGSpawnEntranceComponent();

    /** Native spawner entry point. Authority only. Returns false when the owner cannot use the character-mesh presentation. */
    bool StartGroundRise(float Depth, float Duration, float StartDelay, float EaseExponent, bool bSuspendAIBehaviour, bool bLockActorLocation);

    /** Immediately completes an active entrance. Authority only. */
    void FinishGroundRiseNow();

    bool IsGroundRiseActive() const { return RepState.bActive; }

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
    UPROPERTY(ReplicatedUsing=OnRep_RepState)
    FARPGSpawnEntranceRepState RepState;

    UFUNCTION()
    void OnRep_RepState();

private:
    TWeakObjectPtr<ACharacter> CachedCharacter;
    TWeakObjectPtr<USkeletalMeshComponent> CachedMesh;
    FVector BaseMeshRelativeLocation = FVector::ZeroVector;
    FVector LockedActorLocation = FVector::ZeroVector;
    uint16 LocalPresentationSequence = 0;
    uint8 SavedMovementMode = 0;
    uint8 SavedCustomMovementMode = 0;
    bool bHasVisualBaseline = false;
    bool bLocalPresentationActive = false;
    bool bAuthorityLockApplied = false;
    bool bSavedMovementMode = false;
    bool bSavedAICombatEnabled = false;
    bool bSavedSocialEnabled = false;
    bool bSplineWasActiveBeforeLock = false;
    bool bSuspendAIBehaviourForCurrentRise = true;
    bool bLockActorLocationForCurrentRise = true;

    float GetSynchronizedWorldTimeSeconds() const;
    bool ResolveCharacterAndMesh();
    bool IsOwnerAlive() const;
    void BeginLocalPresentation();
    void FinishLocalPresentation();
    void ApplyVisualAtCurrentTime();
    void ApplyAuthorityMovementLock();
    void ReleaseAuthorityMovementLock(bool bRestoreLocomotion);
    void FinishGroundRiseAuthority(bool bRestoreLocomotion);
};
