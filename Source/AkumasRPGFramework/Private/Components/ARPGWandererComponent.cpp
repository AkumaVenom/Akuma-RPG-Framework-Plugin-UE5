#include "Components/ARPGWandererComponent.h"

#include "AkumasRPGFramework.h"
#include "AIController.h"
#include "Components/ARPGAICombatComponent.h"
#include "Components/ARPGAISplineComponent.h"
#include "Components/ARPGCombatComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "NavigationSystem.h"
#include "Navigation/PathFollowingComponent.h"
#include "TimerManager.h"

namespace
{
    constexpr float ARPGWanderMovementProofDistance = 8.f;
    constexpr float ARPGWanderMovementProofInterval = 0.18f;
    constexpr int32 ARPGWanderMovementProofMaxChecks = 8;
}

UARPGWandererComponent::UARPGWandererComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UARPGWandererComponent::BeginPlay()
{
    Super::BeginPlay();
    HomeLocation = GetOwner() ? GetOwner()->GetActorLocation() : FVector::ZeroVector;
    if (GetOwner() && GetOwner()->HasAuthority() && bEnabled && !IsMovementPaused())
    {
        EnsureThinkTimer();
        // Never submit the very first MoveTo synchronously from component BeginPlay. Spawned pawns can
        // still be finishing possession/path-following initialization. Use a short readiness retry instead.
        ScheduleMovementRetry();
    }
}

void UARPGWandererComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(ThinkTimer);
        GetWorld()->GetTimerManager().ClearTimer(MovementRetryTimer);
        GetWorld()->GetTimerManager().ClearTimer(MovementProofTimer);
    }
    MovementPauseReasons.Reset();
    bHasEstablishedFreeRoam = false;
    bAwaitingMovementProof = false;
    MovementRetryCount = 0;
    MovementProofChecks = 0;
    Super::EndPlay(EndPlayReason);
}

void UARPGWandererComponent::EnsureThinkTimer()
{
    if (!GetWorld() || !bEnabled || IsMovementPaused() || GetWorld()->GetTimerManager().IsTimerActive(ThinkTimer)) return;
    const float Interval = FMath::Max(0.1f, ThinkInterval);
    GetWorld()->GetTimerManager().SetTimer(
        ThinkTimer,
        this,
        &UARPGWandererComponent::Think,
        Interval,
        true,
        FMath::FRandRange(0.05f, Interval));
}

void UARPGWandererComponent::SetWandererEnabled(bool bNewEnabled)
{
    const bool bWasEnabled = bEnabled;
    bEnabled = bNewEnabled;
    if (!GetWorld()) return;

    if (bEnabled)
    {
        // A temporary external pause owns movement without changing the designer/spawner enable state.
        if (!IsMovementPaused())
        {
            EnsureThinkTimer();
            if (!bWasEnabled || !bHasEstablishedFreeRoam)
                ScheduleMovementRetry();
        }
    }
    else
    {
        GetWorld()->GetTimerManager().ClearTimer(ThinkTimer);
        GetWorld()->GetTimerManager().ClearTimer(MovementRetryTimer);
        GetWorld()->GetTimerManager().ClearTimer(MovementProofTimer);
        bHasEstablishedFreeRoam = false;
        bAwaitingMovementProof = false;
        MovementRetryCount = 0;
        MovementProofChecks = 0;
        if (bStopMovementWhenDisabled)
        {
            if (APawn* Pawn = Cast<APawn>(GetOwner()))
                if (AAIController* AI = Cast<AAIController>(Pawn->GetController())) AI->StopMovement();
        }
    }
}

void UARPGWandererComponent::AcquireMovementPause(FName PauseReason, bool bStopCurrentMovement)
{
    if (PauseReason.IsNone()) return;
    const bool bAlreadyPausedForReason = MovementPauseReasons.Contains(PauseReason);
    MovementPauseReasons.Add(PauseReason);

    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(ThinkTimer);
        GetWorld()->GetTimerManager().ClearTimer(MovementRetryTimer);
        GetWorld()->GetTimerManager().ClearTimer(MovementProofTimer);
    }
    bAwaitingMovementProof = false;
    MovementProofChecks = 0;

    if (bStopCurrentMovement && (!bAlreadyPausedForReason || bEnabled))
    {
        if (APawn* Pawn = Cast<APawn>(GetOwner()))
            if (AAIController* AI = Cast<AAIController>(Pawn->GetController())) AI->StopMovement();
    }
}

void UARPGWandererComponent::ReleaseMovementPause(FName PauseReason, bool bChooseNewDestinationImmediately)
{
    if (PauseReason.IsNone()) return;
    MovementPauseReasons.Remove(PauseReason);

    if (!bEnabled || IsMovementPaused() || !GetWorld()) return;

    // Always restore the recurring timer. Combat/spline ownership is checked inside Think(), so
    // releasing one temporary owner cannot strand the Wanderer after that higher-priority state ends.
    EnsureThinkTimer();
    if (bChooseNewDestinationImmediately)
        ScheduleMovementRetry(0.05f, 0.18f);
}

bool UARPGWandererComponent::HasMovementPauseOtherThan(FName AllowedReason) const
{
    for (const FName& Reason : MovementPauseReasons)
    {
        if (Reason != AllowedReason)
            return true;
    }
    return false;
}

void UARPGWandererComponent::ScheduleMovementRetry(float MinDelay, float MaxDelay)
{
    if (!GetWorld() || !bEnabled || IsMovementPaused() || !GetOwner() || !GetOwner()->HasAuthority()) return;
    FTimerManager& Timers = GetWorld()->GetTimerManager();
    if (Timers.IsTimerActive(MovementRetryTimer)) return;

    float SafeMin = FMath::Max(0.02f, FMath::Min(MinDelay, MaxDelay));
    float SafeMax = FMath::Max(SafeMin, FMath::Max(MinDelay, MaxDelay));

    // A broken/missing NavMesh should not turn hundreds of NPCs into permanent high-frequency retry loops.
    // Keep the first readiness attempts fast, then back off while the normal Think timer continues as a fallback.
    if (!bHasEstablishedFreeRoam)
    {
        const float BackoffScale = MovementRetryCount >= 12 ? 4.f : (MovementRetryCount >= 6 ? 2.f : 1.f);
        SafeMin = FMath::Min(1.0f, SafeMin * BackoffScale);
        SafeMax = FMath::Min(1.5f, SafeMax * BackoffScale);
        ++MovementRetryCount;
    }
    else
    {
        MovementRetryCount = 0;
    }

    Timers.SetTimer(MovementRetryTimer, this, &UARPGWandererComponent::Think, FMath::FRandRange(SafeMin, SafeMax), false);
}

void UARPGWandererComponent::MarkNavigationRequestAccepted()
{
    if (!GetWorld() || !GetOwner()) return;

    // RequestSuccessful only means path-following accepted the request. It does NOT prove that the
    // character capsule can actually translate (for example, a newly spawned pawn can still be
    // encroaching another blocking body). Do not expose Free Roam as healthy until real displacement
    // is observed. This also keeps Social AI from reserving a rotate-only/stuck pawn.
    bHasEstablishedFreeRoam = false;
    bAwaitingMovementProof = true;
    MovementRetryCount = 0;
    MovementProofChecks = 0;
    MovementProofStartLocation = GetOwner()->GetActorLocation();
    GetWorld()->GetTimerManager().ClearTimer(MovementRetryTimer);
    GetWorld()->GetTimerManager().ClearTimer(MovementProofTimer);
    GetWorld()->GetTimerManager().SetTimer(
        MovementProofTimer, this, &UARPGWandererComponent::VerifyNavigationMovement,
        ARPGWanderMovementProofInterval, false);
}

void UARPGWandererComponent::CancelMovementProof()
{
    if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(MovementProofTimer);
    bAwaitingMovementProof = false;
    MovementProofChecks = 0;
}

void UARPGWandererComponent::VerifyNavigationMovement()
{
    if (!GetWorld() || !GetOwner() || !GetOwner()->HasAuthority() || !bEnabled || IsMovementPaused())
    {
        CancelMovementProof();
        return;
    }

    APawn* Pawn = Cast<APawn>(GetOwner());
    AAIController* AI = Pawn ? Cast<AAIController>(Pawn->GetController()) : nullptr;
    if (!Pawn || !AI)
    {
        CancelMovementProof();
        bHasEstablishedFreeRoam = false;
        ScheduleMovementRetry();
        return;
    }

    if (const UARPGCombatComponent* Combat = GetOwner()->FindComponentByClass<UARPGCombatComponent>())
        if (!Combat->IsAlive()) { CancelMovementProof(); return; }

    if (bPauseDuringCombat)
    {
        if (const UARPGAICombatComponent* AICombat = GetOwner()->FindComponentByClass<UARPGAICombatComponent>())
            if (IsValid(AICombat->CurrentTarget)) { CancelMovementProof(); return; }
    }

    if (const UARPGAISplineComponent* Spline = GetOwner()->FindComponentByClass<UARPGAISplineComponent>())
        if (Spline->IsRouteActive()) { CancelMovementProof(); return; }

    const float Travelled2D = FVector::Dist2D(Pawn->GetActorLocation(), MovementProofStartLocation);
    if (Travelled2D >= ARPGWanderMovementProofDistance)
    {
        bHasEstablishedFreeRoam = true;
        bAwaitingMovementProof = false;
        bWarnedLocomotionStall = false;
        MovementRetryCount = 0;
        MovementProofChecks = 0;
        GetWorld()->GetTimerManager().ClearTimer(MovementProofTimer);
        return;
    }

    ++MovementProofChecks;
    const EPathFollowingStatus::Type MoveStatus = AI->GetMoveStatus();
    if (MoveStatus == EPathFollowingStatus::Moving && MovementProofChecks < ARPGWanderMovementProofMaxChecks)
    {
        GetWorld()->GetTimerManager().SetTimer(
            MovementProofTimer, this, &UARPGWandererComponent::VerifyNavigationMovement,
            ARPGWanderMovementProofInterval, false);
        return;
    }

    // The request was accepted but no meaningful translation followed. Abort the stale request and
    // pick a fresh reachable point. This catches capsule encroachment, path-following aborts and
    // startup requests that look valid at submission time but never generate locomotion.
    AI->StopMovement();
    bHasEstablishedFreeRoam = false;
    bAwaitingMovementProof = false;
    MovementProofChecks = 0;
    if (!bWarnedLocomotionStall)
    {
        bWarnedLocomotionStall = true;
        int32 MovementModeValue = INDEX_NONE;
        float MaxWalkSpeedValue = -1.f;
        if (const ACharacter* Character = Cast<ACharacter>(Pawn))
        {
            if (const UCharacterMovementComponent* Move = Character->GetCharacterMovement())
            {
                MovementModeValue = static_cast<int32>(Move->MovementMode);
                MaxWalkSpeedValue = Move->MaxWalkSpeed;
            }
        }
        UE_LOG(LogARPG, Warning, TEXT("Free Roam locomotion stall recovered for %s: MoveTo was accepted but no translation followed (PathStatus=%d MovementMode=%d MaxWalkSpeed=%.1f). Retrying a fresh destination."),
            *GetNameSafe(GetOwner()), static_cast<int32>(MoveStatus), MovementModeValue, MaxWalkSpeedValue);
    }
    ScheduleMovementRetry(0.08f, 0.22f);
}

void UARPGWandererComponent::SetHomeLocation(const FVector& NewHomeLocation)
{
    HomeLocation = NewHomeLocation;
}

void UARPGWandererComponent::ForceChooseNewDestination()
{
    Think();
}

void UARPGWandererComponent::ForceReturnHome()
{
    if (!bEnabled || IsMovementPaused() || !GetOwner() || !GetOwner()->HasAuthority()) return;
    APawn* Pawn = Cast<APawn>(GetOwner());
    if (AAIController* AI = Pawn ? Cast<AAIController>(Pawn->GetController()) : nullptr)
        AI->MoveToLocation(HomeLocation, FMath::Max(5.f, AcceptanceRadius), true, true, true, false, nullptr, true);
}

void UARPGWandererComponent::Think()
{
    if (!bEnabled || IsMovementPaused() || !GetOwner() || !GetOwner()->HasAuthority()) return;

    // A one-shot retry may be invoking this function right now. Clear its handle before attempting
    // navigation so a failed attempt is always able to schedule the next short retry deterministically.
    if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(MovementRetryTimer);

    if (const UARPGCombatComponent* Combat = GetOwner()->FindComponentByClass<UARPGCombatComponent>())
        if (!Combat->IsAlive()) return;

    if (bPauseDuringCombat)
    {
        if (const UARPGAICombatComponent* AICombat = GetOwner()->FindComponentByClass<UARPGAICombatComponent>())
            if (IsValid(AICombat->CurrentTarget)) return;
    }

    // A route component is a higher-priority ambient movement owner. This guard prevents a
    // restored Wanderer timer from ever fighting an active spline route.
    if (const UARPGAISplineComponent* Spline = GetOwner()->FindComponentByClass<UARPGAISplineComponent>())
        if (Spline->IsRouteActive()) return;

    APawn* Pawn = Cast<APawn>(GetOwner());
    AAIController* AI = Pawn ? Cast<AAIController>(Pawn->GetController()) : nullptr;
    UNavigationSystemV1* Nav = GetWorld() ? FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()) : nullptr;

    // SpawnActor/BeginPlay/AutoPossessAI ordering can temporarily leave a perfectly valid pawn without
    // a ready AIController or navigation service. A failed startup attempt must be retried quickly rather
    // than waiting for the normal ambient ThinkInterval or becoming socially reserved while motionless.
    if (!Pawn || !AI || !Nav)
    {
        if (!bHasEstablishedFreeRoam) ScheduleMovementRetry();
        return;
    }

    const float Radius = FMath::Max(100.f, WanderRadius);
    EPathFollowingRequestResult::Type MoveResult = EPathFollowingRequestResult::Failed;

    if (bStayNearHome && FVector::Dist2D(Pawn->GetActorLocation(), HomeLocation) > Radius)
    {
        MoveResult = AI->MoveToLocation(HomeLocation, FMath::Max(5.f, AcceptanceRadius), true, true, true, false, nullptr, true);
    }
    else
    {
        const FVector Origin = bStayNearHome ? HomeLocation : Pawn->GetActorLocation();
        FNavLocation Destination;
        if (!Nav->GetRandomReachablePointInRadius(Origin, Radius, Destination))
        {
            if (!bHasEstablishedFreeRoam) ScheduleMovementRetry();
            return;
        }

        MoveResult = AI->MoveToLocation(
            Destination.Location,
            FMath::Max(5.f, AcceptanceRadius),
            true,
            true,
            true,
            false,
            nullptr,
            true);
    }

    if (MoveResult == EPathFollowingRequestResult::RequestSuccessful)
    {
        MarkNavigationRequestAccepted();
        return;
    }

    // Failed means possession/path following/NavData was not ready or the request was invalid. AlreadyAtGoal
    // also does not prove visible Free Roam has begun, so pick another point quickly during startup.
    if (!bHasEstablishedFreeRoam)
        ScheduleMovementRetry();
}

