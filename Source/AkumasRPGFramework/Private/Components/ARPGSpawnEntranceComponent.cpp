#include "Components/ARPGSpawnEntranceComponent.h"

#include "AIController.h"
#include "Components/ARPGAICombatComponent.h"
#include "Components/ARPGAISocialComponent.h"
#include "Components/ARPGAISplineComponent.h"
#include "Components/ARPGCombatComponent.h"
#include "Components/ARPGWandererComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"

namespace
{
    const FName ARPGSpawnEntranceWanderPauseReason(TEXT("SpawnerGroundRise"));
}

UARPGSpawnEntranceComponent::UARPGSpawnEntranceComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = false;
    SetIsReplicatedByDefault(true);
}

void UARPGSpawnEntranceComponent::BeginPlay()
{
    Super::BeginPlay();
    ResolveCharacterAndMesh();
    SetComponentTickEnabled(false);
}

void UARPGSpawnEntranceComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (GetOwner() && GetOwner()->HasAuthority() && bAuthorityLockApplied)
    {
        ReleaseAuthorityMovementLock(false);
    }
    FinishLocalPresentation();
    Super::EndPlay(EndPlayReason);
}

void UARPGSpawnEntranceComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(UARPGSpawnEntranceComponent, RepState);
}

float UARPGSpawnEntranceComponent::GetSynchronizedWorldTimeSeconds() const
{
    const UWorld* World = GetWorld();
    if (!World) return 0.f;
    if (const AGameStateBase* GameState = World->GetGameState())
    {
        return GameState->GetServerWorldTimeSeconds();
    }
    return World->GetTimeSeconds();
}

bool UARPGSpawnEntranceComponent::ResolveCharacterAndMesh()
{
    if (!CachedCharacter.IsValid())
    {
        CachedCharacter = Cast<ACharacter>(GetOwner());
    }

    ACharacter* Character = CachedCharacter.Get();
    if (!Character) return false;

    if (!CachedMesh.IsValid())
    {
        CachedMesh = Character->GetMesh();
    }
    return CachedMesh.IsValid();
}

bool UARPGSpawnEntranceComponent::IsOwnerAlive() const
{
    if (const AActor* Owner = GetOwner())
    {
        if (const UARPGCombatComponent* Combat = Owner->FindComponentByClass<UARPGCombatComponent>())
        {
            return Combat->IsAlive();
        }
    }
    return true;
}

bool UARPGSpawnEntranceComponent::StartGroundRise(
    float Depth,
    float Duration,
    float StartDelay,
    float EaseExponent,
    bool bSuspendAIBehaviour,
    bool bLockActorLocation)
{
    AActor* Owner = GetOwner();
    if (!Owner || !Owner->HasAuthority() || !ResolveCharacterAndMesh()) return false;

    if (RepState.bActive)
    {
        FinishGroundRiseAuthority(true);
    }

    const float SafeDepth = FMath::Max(1.f, Depth);
    const float SafeDuration = FMath::Max(0.05f, Duration);

    RepState.Sequence = static_cast<uint16>(RepState.Sequence + 1);
    if (RepState.Sequence == 0) RepState.Sequence = 1;
    RepState.bActive = true;
    RepState.StartServerTime = GetSynchronizedWorldTimeSeconds();
    RepState.StartDelay = FMath::Max(0.f, StartDelay);
    RepState.Duration = SafeDuration;
    RepState.Depth = SafeDepth;
    RepState.EaseExponent = FMath::Clamp(EaseExponent, 0.1f, 8.f);

    bSuspendAIBehaviourForCurrentRise = bSuspendAIBehaviour;
    bLockActorLocationForCurrentRise = bLockActorLocation;
    LockedActorLocation = Owner->GetActorLocation();

    ApplyAuthorityMovementLock();
    BeginLocalPresentation();
    ApplyVisualAtCurrentTime();
    SetComponentTickEnabled(true);
    Owner->ForceNetUpdate();
    return true;
}

void UARPGSpawnEntranceComponent::FinishGroundRiseNow()
{
    if (GetOwner() && GetOwner()->HasAuthority() && RepState.bActive)
    {
        FinishGroundRiseAuthority(IsOwnerAlive());
    }
}

void UARPGSpawnEntranceComponent::OnRep_RepState()
{
    if (RepState.bActive)
    {
        BeginLocalPresentation();
        ApplyVisualAtCurrentTime();
        SetComponentTickEnabled(true);
    }
    else
    {
        FinishLocalPresentation();
        SetComponentTickEnabled(false);
    }
}

void UARPGSpawnEntranceComponent::BeginLocalPresentation()
{
    if (!ResolveCharacterAndMesh()) return;

    if (bLocalPresentationActive && LocalPresentationSequence == RepState.Sequence)
    {
        return;
    }

    if (bLocalPresentationActive)
    {
        FinishLocalPresentation();
    }

    if (USkeletalMeshComponent* Mesh = CachedMesh.Get())
    {
        BaseMeshRelativeLocation = Mesh->GetRelativeLocation();
        bHasVisualBaseline = true;
        LocalPresentationSequence = RepState.Sequence;
        bLocalPresentationActive = true;
    }
}

void UARPGSpawnEntranceComponent::FinishLocalPresentation()
{
    if (!bLocalPresentationActive) return;

    if (bHasVisualBaseline)
    {
        if (USkeletalMeshComponent* Mesh = CachedMesh.Get())
        {
            // Do not fight a death ragdoll's world-space physics state. Non-simulating meshes always return exactly to authored placement.
            if (!Mesh->IsSimulatingPhysics())
            {
                Mesh->SetRelativeLocation(BaseMeshRelativeLocation, false, nullptr, ETeleportType::TeleportPhysics);
            }
        }
    }

    bLocalPresentationActive = false;
    bHasVisualBaseline = false;
}

void UARPGSpawnEntranceComponent::ApplyVisualAtCurrentTime()
{
    if (!RepState.bActive || !bLocalPresentationActive || !bHasVisualBaseline) return;
    USkeletalMeshComponent* Mesh = CachedMesh.Get();
    if (!Mesh || Mesh->IsSimulatingPhysics()) return;

    const float Elapsed = GetSynchronizedWorldTimeSeconds() - RepState.StartServerTime - RepState.StartDelay;
    const float LinearAlpha = FMath::Clamp(Elapsed / FMath::Max(0.05f, RepState.Duration), 0.f, 1.f);
    const float EasedAlpha = FMath::InterpEaseOut(0.f, 1.f, LinearAlpha, FMath::Max(0.1f, RepState.EaseExponent));
    const float ZOffset = -FMath::Max(1.f, RepState.Depth) * (1.f - EasedAlpha);

    Mesh->SetRelativeLocation(BaseMeshRelativeLocation + FVector(0.f, 0.f, ZOffset), false, nullptr, ETeleportType::TeleportPhysics);
}

void UARPGSpawnEntranceComponent::ApplyAuthorityMovementLock()
{
    ACharacter* Character = CachedCharacter.Get();
    if (!Character || !Character->HasAuthority() || bAuthorityLockApplied) return;

    bAuthorityLockApplied = true;

    // Suspend higher-level behaviour first because clearing combat/social ownership can itself restore
    // Wanderer/Spline movement. The hard locomotion locks below are therefore always applied last.
    if (bSuspendAIBehaviourForCurrentRise)
    {
        if (UARPGAICombatComponent* AICombat = Character->FindComponentByClass<UARPGAICombatComponent>())
        {
            bSavedAICombatEnabled = AICombat->bEnabled;
            if (bSavedAICombatEnabled) AICombat->SetAICombatEnabled(false);
        }

        if (UARPGAISocialComponent* Social = Character->FindComponentByClass<UARPGAISocialComponent>())
        {
            bSavedSocialEnabled = Social->bEnableSocialInteractions;
            if (bSavedSocialEnabled) Social->SetSocialInteractionsEnabled(false);
        }
    }

    if (AAIController* AI = Cast<AAIController>(Character->GetController()))
    {
        AI->StopMovement();
    }

    if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
    {
        SavedMovementMode = static_cast<uint8>(Movement->MovementMode);
        SavedCustomMovementMode = Movement->CustomMovementMode;
        bSavedMovementMode = true;
        Movement->StopMovementImmediately();
        Movement->DisableMovement();
    }

    if (UARPGWandererComponent* Wanderer = Character->FindComponentByClass<UARPGWandererComponent>())
    {
        Wanderer->AcquireMovementPause(ARPGSpawnEntranceWanderPauseReason, true);
    }

    if (UARPGAISplineComponent* Spline = Character->FindComponentByClass<UARPGAISplineComponent>())
    {
        bSplineWasActiveBeforeLock = Spline->IsRouteActive();
        if (bSplineWasActiveBeforeLock)
        {
            Spline->PauseRoute(true);
        }
    }
}

void UARPGSpawnEntranceComponent::ReleaseAuthorityMovementLock(bool bRestoreLocomotion)
{
    ACharacter* Character = CachedCharacter.Get();
    if (!Character || !bAuthorityLockApplied) return;

    if (AAIController* AI = Cast<AAIController>(Character->GetController()))
    {
        // Cancel any MoveTo that another short-interval system may have queued while movement was disabled.
        AI->StopMovement();
    }

    const bool bAlive = IsOwnerAlive();
    const bool bCanRestore = bRestoreLocomotion && bAlive;

    if (bCanRestore && bSavedMovementMode)
    {
        if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
        {
            // Only undo our own MOVE_None. If another system deliberately changed movement mode, leave it alone.
            if (Movement->MovementMode == MOVE_None)
            {
                Movement->SetMovementMode(static_cast<EMovementMode>(SavedMovementMode), SavedCustomMovementMode);
            }
        }
    }

    if (UARPGWandererComponent* Wanderer = Character->FindComponentByClass<UARPGWandererComponent>())
    {
        Wanderer->ReleaseMovementPause(ARPGSpawnEntranceWanderPauseReason, bCanRestore);
    }

    if (bCanRestore && bSplineWasActiveBeforeLock)
    {
        if (UARPGAISplineComponent* Spline = Character->FindComponentByClass<UARPGAISplineComponent>())
        {
            // PauseRoute keeps the route active. StopRoute/mode changes make IsRouteActive false, preventing stale route resurrection.
            if (Spline->IsRouteActive()) Spline->ResumeRoute();
        }
    }

    if (bSuspendAIBehaviourForCurrentRise && bCanRestore)
    {
        if (bSavedAICombatEnabled)
        {
            if (UARPGAICombatComponent* AICombat = Character->FindComponentByClass<UARPGAICombatComponent>())
                AICombat->SetAICombatEnabled(true);
        }

        if (bSavedSocialEnabled)
        {
            if (UARPGAISocialComponent* Social = Character->FindComponentByClass<UARPGAISocialComponent>())
                Social->SetSocialInteractionsEnabled(true);
        }
    }

    bAuthorityLockApplied = false;
    bSavedMovementMode = false;
    bSavedAICombatEnabled = false;
    bSavedSocialEnabled = false;
    bSplineWasActiveBeforeLock = false;
}

void UARPGSpawnEntranceComponent::FinishGroundRiseAuthority(bool bRestoreLocomotion)
{
    AActor* Owner = GetOwner();
    if (!Owner || !Owner->HasAuthority()) return;

    RepState.bActive = false;
    FinishLocalPresentation();
    ReleaseAuthorityMovementLock(bRestoreLocomotion);
    SetComponentTickEnabled(false);
    Owner->ForceNetUpdate();
}

void UARPGSpawnEntranceComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    (void)DeltaTime;

    if (!RepState.bActive)
    {
        SetComponentTickEnabled(false);
        return;
    }

    ApplyVisualAtCurrentTime();

    AActor* Owner = GetOwner();
    if (Owner && Owner->HasAuthority())
    {
        if (bLockActorLocationForCurrentRise && FVector::DistSquared(Owner->GetActorLocation(), LockedActorLocation) > 1.f)
        {
            // The mesh animation never moves the actor. This only rejects external/custom movement attempts while the entrance owns locomotion.
            Owner->SetActorLocation(LockedActorLocation, false, nullptr, ETeleportType::TeleportPhysics);
        }

        if (!IsOwnerAlive())
        {
            FinishGroundRiseAuthority(false);
            return;
        }

        const float EndTime = RepState.StartServerTime + RepState.StartDelay + RepState.Duration;
        if (GetSynchronizedWorldTimeSeconds() >= EndTime)
        {
            FinishGroundRiseAuthority(true);
        }
    }
    else
    {
        const float EndTime = RepState.StartServerTime + RepState.StartDelay + RepState.Duration;
        if (GetSynchronizedWorldTimeSeconds() >= EndTime)
        {
            // Clients can stop their short-lived visual Tick immediately; replicated bActive=false will follow from authority.
            FinishLocalPresentation();
            SetComponentTickEnabled(false);
        }
    }
}
