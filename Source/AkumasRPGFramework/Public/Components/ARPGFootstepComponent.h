#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "ARPGFootstepComponent.generated.h"

class ACharacter;
class UARPGCombatComponent;
class USoundAttenuation;
class USoundBase;
class USoundConcurrency;

/** Per-physical-surface footstep presentation. Add one entry for each project Surface Type you want to sound unique. */
USTRUCT(BlueprintType)
struct AKUMASRPGFRAMEWORK_API FARPGFootstepSurfaceAudio
{
    GENERATED_BODY()

    /** Project Settings -> Physics -> Physical Surface type resolved from the ground hit. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Footsteps") TEnumAsByte<EPhysicalSurface> SurfaceType = SurfaceType_Default;

    /** Randomized pool for this surface. Immediate repeats are avoided when two or more valid sounds are assigned. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Footsteps") TArray<TObjectPtr<USoundBase>> Sounds;

    /** Extra loudness multiplier applied after the component's speed-based volume. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Footsteps", meta=(ClampMin="0.0")) float VolumeMultiplier = 1.f;

    /** Random pitch range for this surface. Values are normalized at runtime if authored in reverse order. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Footsteps", meta=(ClampMin="0.01")) float MinPitch = 0.96f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Footsteps", meta=(ClampMin="0.01")) float MaxPitch = 1.04f;
};

/**
 * Automatic, multiplayer-ready character footsteps.
 *
 * Server movement drives the authoritative replicated cue for all players/NPCs. The owning client can also
 * predict its own automatic footsteps locally so input/network latency is not audible to the local player.
 * Ground traces resolve UE Physical Surface types and select from exposed sound pools automatically.
 */
UCLASS(ClassGroup=(ARPG), meta=(BlueprintSpawnableComponent))
class AKUMASRPGFRAMEWORK_API UARPGFootstepComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UARPGFootstepComponent();

    /** Master switch. Enabled by default on ARPG characters; no sound is produced until audio assets are assigned. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Footsteps|Automatic") bool bEnableFootsteps = true;

    /** Generate footsteps from real horizontal travel distance. No animation notifies are required. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Footsteps|Automatic") bool bAutomaticFootsteps = true;

    /** Locally predict the owning player's automatic sound, while the server multicasts the same step to everyone else. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Footsteps|Networking") bool bPredictOwningPlayer = true;

    /** Server/owner sampling cadence. Traces only occur when a step is actually due. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Footsteps|Automatic", meta=(ClampMin="0.02", ClampMax="0.25", Units="s")) float SampleInterval = 0.05f;

    /** Horizontal speed below this is treated as stopped and resets partial stride progress. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Footsteps|Automatic", meta=(ClampMin="0.0", Units="cm/s")) float MinimumGroundSpeed = 70.f;

    /** Distance between audible foot contacts near Minimum Ground Speed. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Footsteps|Automatic", meta=(ClampMin="20.0", Units="cm")) float WalkStepDistance = 145.f;

    /** Distance between audible foot contacts at/above Run Reference Speed. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Footsteps|Automatic", meta=(ClampMin="20.0", Units="cm")) float RunStepDistance = 185.f;

    /** Speed used to blend from Walk Step Distance to Run Step Distance and from minimum to maximum speed volume. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Footsteps|Automatic", meta=(ClampMin="1.0", Units="cm/s")) float RunReferenceSpeed = 600.f;

    /** Movement jumps larger than this between samples are treated as teleports/network corrections, never as strides. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Footsteps|Automatic", meta=(ClampMin="50.0", Units="cm")) float TeleportResetDistance = 260.f;

    /** Socket/bone names used for accurate contact location when present on the character mesh. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Footsteps|Feet") FName LeftFootSocket = TEXT("foot_l");
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Footsteps|Feet") FName RightFootSocket = TEXT("foot_r");

    /** Lateral fallback offset used when the configured foot socket/bone is unavailable. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Footsteps|Feet", meta=(ClampMin="0.0", Units="cm")) float FallbackFootSeparation = 12.f;

    /** Ground trace distance above the resolved foot position. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Footsteps|Ground Trace", meta=(ClampMin="0.0", Units="cm")) float TraceUpDistance = 24.f;

    /** Ground trace distance below the resolved foot position. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Footsteps|Ground Trace", meta=(ClampMin="1.0", Units="cm")) float TraceDownDistance = 65.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Footsteps|Ground Trace") TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Footsteps|Ground Trace", AdvancedDisplay) bool bTraceComplex = false;

    /** Small normal offset keeps spatial audio just above the contacted floor. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Footsteps|Ground Trace", meta=(ClampMin="0.0", Units="cm")) float SoundLocationNormalOffset = 2.f;

    /** Used when no matching Surface Audio entry exists, the hit has no Physical Material, or surface audio is disabled. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Footsteps|Audio") TArray<TObjectPtr<USoundBase>> DefaultSounds;

    /** Surface-specific audio pools resolved from the Physical Material hit by the foot trace. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Footsteps|Audio") TArray<FARPGFootstepSurfaceAudio> SurfaceAudio;

    /** Set false to ignore Physical Surface types and always use Default Sounds. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Footsteps|Audio") bool bUsePhysicalSurfaceAudio = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Footsteps|Audio", meta=(ClampMin="0.0")) float MasterVolume = 1.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Footsteps|Audio", meta=(ClampMin="0.0")) float MinimumSpeedVolume = 0.72f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Footsteps|Audio", meta=(ClampMin="0.0")) float MaximumSpeedVolume = 1.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Footsteps|Audio", meta=(ClampMin="0.01")) float DefaultMinPitch = 0.96f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Footsteps|Audio", meta=(ClampMin="0.01")) float DefaultMaxPitch = 1.04f;

    /** Optional project attenuation asset. Strongly recommended for multiplayer/NPC spatial audio. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Footsteps|Audio") TObjectPtr<USoundAttenuation> AttenuationSettings;

    /** Optional concurrency asset for limiting dense crowds and repeated footsteps. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Footsteps|Audio") TObjectPtr<USoundConcurrency> ConcurrencySettings;

    /** Enables/disables the component at runtime and updates its timer ownership safely. */
    UFUNCTION(BlueprintCallable, Category="ARPG|Footsteps") void SetFootstepsEnabled(bool bEnabled);

    /** Switches automatic distance-driven footsteps at runtime. Manual Trigger Footstep remains available when false. */
    UFUNCTION(BlueprintCallable, Category="ARPG|Footsteps") void SetAutomaticFootstepsEnabled(bool bEnabled);

    /** Re-evaluates authority/local-prediction sampling after possession or runtime configuration changes. */
    UFUNCTION(BlueprintCallable, Category="ARPG|Footsteps") void RefreshFootstepRuntime();

    /** Optional animation/manual path. Client calls are locally predicted and validated/replayed by the server. */
    UFUNCTION(BlueprintCallable, Category="ARPG|Footsteps") bool TriggerFootstep(bool bLeftFoot);

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

protected:
    UFUNCTION(Server, Unreliable) void ServerTriggerFootstep(bool bLeftFoot, bool bClientPredicted);
    UFUNCTION(NetMulticast, Unreliable) void MulticastPlayFootstep(USoundBase* Sound, FVector Location, float Volume, float Pitch, bool bSkipOwningClient);

private:
    struct FResolvedCue
    {
        USoundBase* Sound = nullptr;
        FVector Location = FVector::ZeroVector;
        float Volume = 1.f;
        float Pitch = 1.f;
        EPhysicalSurface SurfaceType = SurfaceType_Default;
    };

    TWeakObjectPtr<ACharacter> CachedCharacter;
    TWeakObjectPtr<UARPGCombatComponent> CachedCombat;
    FTimerHandle SamplingTimer;
    FVector LastSampleLocation = FVector::ZeroVector;
    float AccumulatedStrideDistance = 0.f;
    float LastManualAuthorityTime = -1000.f;
    bool bNextFootLeft = true;
    bool bHasSampleLocation = false;
    TMap<int32, int32> LastSoundIndexByPool;

    void UpdateSamplingTimer();
    bool ShouldSampleOnThisMachine() const;
    bool CanGenerateFootstep() const;
    bool HasAnyConfiguredSound() const;
    void SampleMovement();
    float ResolveCurrentStepDistance(float Speed2D) const;
    bool BuildCue(bool bLeftFoot, float Speed2D, FResolvedCue& OutCue);
    FVector ResolveFootLocation(bool bLeftFoot) const;
    const FARPGFootstepSurfaceAudio* FindSurfaceAudio(EPhysicalSurface SurfaceType) const;
    USoundBase* PickSoundAvoidingImmediateRepeat(const TArray<TObjectPtr<USoundBase>>& Sounds, int32 PoolKey);
    void PlayCueLocal(const FResolvedCue& Cue) const;
    bool TriggerAuthority(bool bLeftFoot, bool bSkipOwningClient, bool bEnforceManualRateLimit);
    bool TriggerPredictedLocal(bool bLeftFoot);
    void ResetStrideState(bool bKeepLocation);
};
