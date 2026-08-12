#include "Components/ARPGFootstepComponent.h"

#include "Actors/ARPGCharacter.h"
#include "Components/ARPGCombatComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundConcurrency.h"
#include "TimerManager.h"

UARPGFootstepComponent::UARPGFootstepComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(true);
}

void UARPGFootstepComponent::BeginPlay()
{
    Super::BeginPlay();

    CachedCharacter = Cast<ACharacter>(GetOwner());
    if (AARPGCharacter* ARPGCharacter = Cast<AARPGCharacter>(GetOwner()))
    {
        CachedCombat = ARPGCharacter->Combat;
    }

    if (ACharacter* Character = CachedCharacter.Get())
    {
        LastSampleLocation = Character->GetActorLocation();
        bHasSampleLocation = true;
    }

    UpdateSamplingTimer();
}

void UARPGFootstepComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(SamplingTimer);
    }
    Super::EndPlay(EndPlayReason);
}

void UARPGFootstepComponent::SetFootstepsEnabled(bool bEnabled)
{
    if (bEnableFootsteps == bEnabled) return;
    bEnableFootsteps = bEnabled;
    ResetStrideState(false);
    UpdateSamplingTimer();
}

void UARPGFootstepComponent::SetAutomaticFootstepsEnabled(bool bEnabled)
{
    if (bAutomaticFootsteps == bEnabled) return;
    bAutomaticFootsteps = bEnabled;
    ResetStrideState(false);
    UpdateSamplingTimer();
}

void UARPGFootstepComponent::RefreshFootstepRuntime()
{
    if (!CachedCharacter.IsValid())
    {
        CachedCharacter = Cast<ACharacter>(GetOwner());
    }
    if (!CachedCombat.IsValid())
    {
        if (AARPGCharacter* ARPGCharacter = Cast<AARPGCharacter>(GetOwner()))
        {
            CachedCombat = ARPGCharacter->Combat;
        }
    }
    ResetStrideState(false);
    UpdateSamplingTimer();
}

void UARPGFootstepComponent::UpdateSamplingTimer()
{
    UWorld* World = GetWorld();
    if (!World) return;

    FTimerManager& Timers = World->GetTimerManager();
    Timers.ClearTimer(SamplingTimer);

    if (!bEnableFootsteps || !bAutomaticFootsteps || !HasAnyConfiguredSound() || !ShouldSampleOnThisMachine()) return;

    const float Interval = FMath::Clamp(SampleInterval, 0.02f, 0.25f);
    const float InitialDelay = FMath::FRandRange(0.01f, Interval);
    Timers.SetTimer(SamplingTimer, this, &UARPGFootstepComponent::SampleMovement, Interval, true, InitialDelay);
}

bool UARPGFootstepComponent::ShouldSampleOnThisMachine() const
{
    const ACharacter* Character = CachedCharacter.Get();
    if (!Character) return false;

    if (Character->HasAuthority()) return true;

    return bPredictOwningPlayer
        && Character->GetNetMode() == NM_Client
        && Character->IsPlayerControlled()
        && Character->IsLocallyControlled();
}

bool UARPGFootstepComponent::HasAnyConfiguredSound() const
{
    for (const TObjectPtr<USoundBase>& Sound : DefaultSounds)
    {
        if (IsValid(Sound.Get())) return true;
    }
    for (const FARPGFootstepSurfaceAudio& Entry : SurfaceAudio)
    {
        for (const TObjectPtr<USoundBase>& Sound : Entry.Sounds)
        {
            if (IsValid(Sound.Get())) return true;
        }
    }
    return false;
}

bool UARPGFootstepComponent::CanGenerateFootstep() const
{
    const ACharacter* Character = CachedCharacter.Get();
    if (!bEnableFootsteps || !Character) return false;

    const UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
    if (!Movement || !Movement->IsMovingOnGround()) return false;

    if (const UARPGCombatComponent* Combat = CachedCombat.Get())
    {
        if (!Combat->IsAlive() || Combat->IsRagdollActive()) return false;
    }

    return true;
}

void UARPGFootstepComponent::SampleMovement()
{
    ACharacter* Character = CachedCharacter.Get();
    if (!Character || !ShouldSampleOnThisMachine())
    {
        UpdateSamplingTimer();
        return;
    }

    const FVector CurrentLocation = Character->GetActorLocation();
    if (!bHasSampleLocation)
    {
        LastSampleLocation = CurrentLocation;
        bHasSampleLocation = true;
        return;
    }

    const FVector Delta = CurrentLocation - LastSampleLocation;
    LastSampleLocation = CurrentLocation;

    const float Distance2D = FVector(Delta.X, Delta.Y, 0.f).Size();
    if (Distance2D > FMath::Max(50.f, TeleportResetDistance))
    {
        AccumulatedStrideDistance = 0.f;
        return;
    }

    const UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
    const float Speed2D = Movement ? Movement->Velocity.Size2D() : 0.f;
    if (!CanGenerateFootstep() || Speed2D < FMath::Max(0.f, MinimumGroundSpeed))
    {
        AccumulatedStrideDistance = 0.f;
        return;
    }

    AccumulatedStrideDistance += Distance2D;
    const float RequiredDistance = ResolveCurrentStepDistance(Speed2D);
    if (AccumulatedStrideDistance < RequiredDistance) return;

    // Preserve small overshoot so cadence remains stable at different sampling rates, but never burst multiple steps in one sample.
    AccumulatedStrideDistance = FMath::Fmod(AccumulatedStrideDistance, FMath::Max(RequiredDistance, 1.f));

    const bool bThisFootLeft = bNextFootLeft;
    bNextFootLeft = !bNextFootLeft;

    if (Character->HasAuthority())
    {
        const bool bOwningClientPredicts = bPredictOwningPlayer && Character->IsPlayerControlled();
        TriggerAuthority(bThisFootLeft, bOwningClientPredicts, false);
    }
    else
    {
        TriggerPredictedLocal(bThisFootLeft);
    }
}

float UARPGFootstepComponent::ResolveCurrentStepDistance(float Speed2D) const
{
    const float MinSpeed = FMath::Max(0.f, MinimumGroundSpeed);
    const float MaxSpeed = FMath::Max(MinSpeed + 1.f, RunReferenceSpeed);
    const float Alpha = FMath::Clamp((Speed2D - MinSpeed) / (MaxSpeed - MinSpeed), 0.f, 1.f);
    return FMath::Max(20.f, FMath::Lerp(WalkStepDistance, RunStepDistance, Alpha));
}

bool UARPGFootstepComponent::BuildCue(bool bLeftFoot, float Speed2D, FResolvedCue& OutCue)
{
    UWorld* World = GetWorld();
    ACharacter* Character = CachedCharacter.Get();
    if (!World || !Character || !CanGenerateFootstep()) return false;

    const FVector FootLocation = ResolveFootLocation(bLeftFoot);
    const FVector TraceStart = FootLocation + FVector::UpVector * FMath::Max(0.f, TraceUpDistance);
    const FVector TraceEnd = FootLocation - FVector::UpVector * FMath::Max(1.f, TraceDownDistance);

    FCollisionQueryParams Params(SCENE_QUERY_STAT(ARPGFootstepGroundTrace), bTraceComplex, Character);
    Params.bReturnPhysicalMaterial = true;

    FHitResult Hit;
    if (!World->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, TraceChannel, Params) || !Hit.bBlockingHit)
    {
        return false;
    }

    EPhysicalSurface SurfaceType = SurfaceType_Default;
    if (bUsePhysicalSurfaceAudio)
    {
        SurfaceType = UPhysicalMaterial::DetermineSurfaceType(Hit.PhysMaterial.Get());
    }

    const FARPGFootstepSurfaceAudio* SurfaceEntry = bUsePhysicalSurfaceAudio ? FindSurfaceAudio(SurfaceType) : nullptr;
    const TArray<TObjectPtr<USoundBase>>* Pool = SurfaceEntry ? &SurfaceEntry->Sounds : &DefaultSounds;
    const int32 PoolKey = SurfaceEntry ? static_cast<int32>(SurfaceType) : -1;

    USoundBase* ChosenSound = PickSoundAvoidingImmediateRepeat(*Pool, PoolKey);
    if (!ChosenSound && SurfaceEntry)
    {
        ChosenSound = PickSoundAvoidingImmediateRepeat(DefaultSounds, -1);
        SurfaceEntry = nullptr;
    }
    if (!ChosenSound) return false;

    const float MinSpeed = FMath::Max(0.f, MinimumGroundSpeed);
    const float MaxSpeed = FMath::Max(MinSpeed + 1.f, RunReferenceSpeed);
    const float SpeedAlpha = FMath::Clamp((Speed2D - MinSpeed) / (MaxSpeed - MinSpeed), 0.f, 1.f);
    const float SpeedVolume = FMath::Lerp(MinimumSpeedVolume, MaximumSpeedVolume, SpeedAlpha);
    const float SurfaceVolume = SurfaceEntry ? SurfaceEntry->VolumeMultiplier : 1.f;

    const float PitchA = SurfaceEntry ? SurfaceEntry->MinPitch : DefaultMinPitch;
    const float PitchB = SurfaceEntry ? SurfaceEntry->MaxPitch : DefaultMaxPitch;
    const float PitchMin = FMath::Max(0.01f, FMath::Min(PitchA, PitchB));
    const float PitchMax = FMath::Max(PitchMin, FMath::Max(PitchA, PitchB));

    OutCue.Sound = ChosenSound;
    OutCue.Location = Hit.ImpactPoint + Hit.ImpactNormal * FMath::Max(0.f, SoundLocationNormalOffset);
    OutCue.Volume = FMath::Max(0.f, MasterVolume) * FMath::Max(0.f, SpeedVolume) * FMath::Max(0.f, SurfaceVolume);
    OutCue.Pitch = FMath::FRandRange(PitchMin, PitchMax);
    OutCue.SurfaceType = SurfaceType;
    return true;
}

FVector UARPGFootstepComponent::ResolveFootLocation(bool bLeftFoot) const
{
    const ACharacter* Character = CachedCharacter.Get();
    if (!Character) return FVector::ZeroVector;

    // Dedicated servers commonly avoid full skeletal pose evaluation. Capsule fallback keeps authority traces reliable and cheap there.
    if (Character->GetNetMode() != NM_DedicatedServer)
    {
        if (const USkeletalMeshComponent* Mesh = Character->GetMesh())
        {
            const FName SocketName = bLeftFoot ? LeftFootSocket : RightFootSocket;
            if (!SocketName.IsNone() && (Mesh->DoesSocketExist(SocketName) || Mesh->GetBoneIndex(SocketName) != INDEX_NONE))
            {
                return Mesh->GetSocketLocation(SocketName);
            }
        }
    }

    float CapsuleHalfHeight = 88.f;
    if (const UCapsuleComponent* Capsule = Character->GetCapsuleComponent())
    {
        CapsuleHalfHeight = Capsule->GetScaledCapsuleHalfHeight();
    }

    const float Side = bLeftFoot ? -1.f : 1.f;
    return Character->GetActorLocation()
        - FVector::UpVector * CapsuleHalfHeight
        + Character->GetActorRightVector() * (FMath::Max(0.f, FallbackFootSeparation) * Side);
}

const FARPGFootstepSurfaceAudio* UARPGFootstepComponent::FindSurfaceAudio(EPhysicalSurface SurfaceType) const
{
    for (const FARPGFootstepSurfaceAudio& Entry : SurfaceAudio)
    {
        if (Entry.SurfaceType == SurfaceType && !Entry.Sounds.IsEmpty())
        {
            return &Entry;
        }
    }
    return nullptr;
}

USoundBase* UARPGFootstepComponent::PickSoundAvoidingImmediateRepeat(const TArray<TObjectPtr<USoundBase>>& Sounds, int32 PoolKey)
{
    TArray<int32, TInlineAllocator<8>> ValidIndices;
    for (int32 Index = 0; Index < Sounds.Num(); ++Index)
    {
        if (IsValid(Sounds[Index].Get())) ValidIndices.Add(Index);
    }
    if (ValidIndices.IsEmpty()) return nullptr;

    int32 ChosenValidSlot = FMath::RandRange(0, ValidIndices.Num() - 1);
    const int32* PreviousIndex = LastSoundIndexByPool.Find(PoolKey);
    if (ValidIndices.Num() > 1 && PreviousIndex && ValidIndices[ChosenValidSlot] == *PreviousIndex)
    {
        ChosenValidSlot = (ChosenValidSlot + FMath::RandRange(1, ValidIndices.Num() - 1)) % ValidIndices.Num();
    }

    const int32 ChosenIndex = ValidIndices[ChosenValidSlot];
    LastSoundIndexByPool.Add(PoolKey, ChosenIndex);
    return Sounds[ChosenIndex].Get();
}

void UARPGFootstepComponent::PlayCueLocal(const FResolvedCue& Cue) const
{
    const UWorld* World = GetWorld();
    if (!Cue.Sound || !World || World->GetNetMode() == NM_DedicatedServer) return;

    UGameplayStatics::PlaySoundAtLocation(
        this,
        Cue.Sound,
        Cue.Location,
        FRotator::ZeroRotator,
        Cue.Volume,
        Cue.Pitch,
        0.f,
        AttenuationSettings,
        ConcurrencySettings,
        GetOwner(),
        nullptr);
}

bool UARPGFootstepComponent::TriggerAuthority(bool bLeftFoot, bool bSkipOwningClient, bool bEnforceManualRateLimit)
{
    ACharacter* Character = CachedCharacter.Get();
    UWorld* World = GetWorld();
    if (!Character || !World || !Character->HasAuthority() || !CanGenerateFootstep()) return false;

    if (bEnforceManualRateLimit)
    {
        const float Now = World->GetTimeSeconds();
        const float MinimumInterval = FMath::Max(0.05f, SampleInterval * 0.5f);
        if (Now - LastManualAuthorityTime < MinimumInterval) return false;
        LastManualAuthorityTime = Now;
    }

    const UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
    const float Speed2D = Movement ? Movement->Velocity.Size2D() : MinimumGroundSpeed;

    FResolvedCue Cue;
    if (!BuildCue(bLeftFoot, FMath::Max(Speed2D, MinimumGroundSpeed), Cue)) return false;

    MulticastPlayFootstep(Cue.Sound, Cue.Location, Cue.Volume, Cue.Pitch, bSkipOwningClient);
    return true;
}

bool UARPGFootstepComponent::TriggerPredictedLocal(bool bLeftFoot)
{
    ACharacter* Character = CachedCharacter.Get();
    if (!Character || Character->HasAuthority() || !Character->IsLocallyControlled() || !CanGenerateFootstep()) return false;

    const UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
    const float Speed2D = Movement ? Movement->Velocity.Size2D() : MinimumGroundSpeed;

    FResolvedCue Cue;
    if (!BuildCue(bLeftFoot, FMath::Max(Speed2D, MinimumGroundSpeed), Cue)) return false;
    PlayCueLocal(Cue);
    return true;
}

bool UARPGFootstepComponent::TriggerFootstep(bool bLeftFoot)
{
    ACharacter* Character = CachedCharacter.Get();
    if (!bEnableFootsteps || !Character) return false;

    if (Character->HasAuthority())
    {
        return TriggerAuthority(bLeftFoot, false, true);
    }

    if (!Character->IsLocallyControlled()) return false;

    const bool bPredicted = bPredictOwningPlayer && TriggerPredictedLocal(bLeftFoot);
    ServerTriggerFootstep(bLeftFoot, bPredicted);
    return bPredicted;
}

void UARPGFootstepComponent::ServerTriggerFootstep_Implementation(bool bLeftFoot, bool bClientPredicted)
{
    TriggerAuthority(bLeftFoot, bClientPredicted, true);
}

void UARPGFootstepComponent::MulticastPlayFootstep_Implementation(USoundBase* Sound, FVector Location, float Volume, float Pitch, bool bSkipOwningClient)
{
    ACharacter* Character = CachedCharacter.Get();
    if (!Character) Character = Cast<ACharacter>(GetOwner());

    if (bSkipOwningClient
        && Character
        && Character->GetNetMode() == NM_Client
        && Character->IsPlayerControlled()
        && Character->IsLocallyControlled())
    {
        return;
    }

    const UWorld* World = GetWorld();
    if (!Sound || !World || World->GetNetMode() == NM_DedicatedServer) return;

    UGameplayStatics::PlaySoundAtLocation(
        this,
        Sound,
        Location,
        FRotator::ZeroRotator,
        FMath::Max(0.f, Volume),
        FMath::Max(0.01f, Pitch),
        0.f,
        AttenuationSettings,
        ConcurrencySettings,
        GetOwner(),
        nullptr);
}

void UARPGFootstepComponent::ResetStrideState(bool bKeepLocation)
{
    AccumulatedStrideDistance = 0.f;
    bNextFootLeft = true;

    if (!bKeepLocation)
    {
        if (const ACharacter* Character = CachedCharacter.Get())
        {
            LastSampleLocation = Character->GetActorLocation();
            bHasSampleLocation = true;
        }
        else
        {
            bHasSampleLocation = false;
        }
    }
}
