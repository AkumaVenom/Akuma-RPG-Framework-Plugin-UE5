#include "Components/ARPGAISplineComponent.h"

#include "AIController.h"
#include "Actors/ARPGAISplineRoute.h"
#include "Components/ARPGAICombatComponent.h"
#include "Components/ARPGCombatComponent.h"
#include "Components/ARPGWandererComponent.h"
#include "Components/SplineComponent.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "NavigationSystem.h"
#include "Navigation/PathFollowingComponent.h"
#include "TimerManager.h"

UARPGAISplineComponent::UARPGAISplineComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UARPGAISplineComponent::BeginPlay()
{
    Super::BeginPlay();
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;

    CurrentDirectionSign = bStartMovingForward ? 1 : -1;
    RuntimeLateralOffset = LateralOffset;
    if (bRandomizeLateralOffset && RandomLateralOffsetRange > 0.f)
        RuntimeLateralOffset += FMath::FRandRange(-RandomLateralOffsetRange, RandomLateralOffsetRange);

    ResolveRouteIfNeeded();
    if (bAutoStart && Route) StartRoute();
}

void UARPGAISplineComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(UpdateTimer);
    Super::EndPlay(EndPlayReason);
}


void UARPGAISplineComponent::EnsureUpdateTimer()
{
    if (!GetWorld() || GetWorld()->GetTimerManager().IsTimerActive(UpdateTimer)) return;
    const float Interval = FMath::Max(0.05f, UpdateInterval);
    GetWorld()->GetTimerManager().SetTimer(UpdateTimer, this, &UARPGAISplineComponent::UpdateSplineMovement, Interval, true, FMath::FRandRange(0.02f, Interval));
}

AAIController* UARPGAISplineComponent::GetAIController() const
{
    const APawn* Pawn = Cast<APawn>(GetOwner());
    return Pawn ? Cast<AAIController>(Pawn->GetController()) : nullptr;
}

bool UARPGAISplineComponent::ResolveRouteIfNeeded()
{
    if (IsValid(Route)) return true;
    Route = nullptr;
    if (!bAutoFindRouteById || RouteId.IsNone() || !GetWorld()) return false;

    AARPGAISplineRoute* BestRoute = nullptr;
    float BestDistanceSq = TNumericLimits<float>::Max();
    for (TActorIterator<AARPGAISplineRoute> It(GetWorld()); It; ++It)
    {
        AARPGAISplineRoute* Candidate = *It;
        if (!IsValid(Candidate) || Candidate->RouteId != RouteId) continue;
        const FVector OwnerLocation = GetOwner() ? GetOwner()->GetActorLocation() : FVector::ZeroVector;
        const FVector CandidateAnchor = Candidate->GetLocationAtRouteDistance(Candidate->FindDistanceClosestToWorldLocation(OwnerLocation));
        const float DistanceSq = FVector::DistSquared(OwnerLocation, CandidateAnchor);
        if (DistanceSq < BestDistanceSq)
        {
            BestDistanceSq = DistanceSq;
            BestRoute = Candidate;
        }
    }
    if (BestRoute)
    {
        Route = BestRoute;
        OnRouteChanged.Broadcast(Route);
        return true;
    }
    return false;
}

void UARPGAISplineComponent::SetRoute(AARPGAISplineRoute* NewRoute, bool bStartImmediately)
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;
    if (Route == NewRoute && (!bStartImmediately || bRouteActive)) return;

    if (AAIController* AI = GetAIController()) AI->StopMovement();
    Route = NewRoute;
    bRouteActive = false;
    bRouteFinished = false;
    bWaitingAtPoint = false;
    bSuspendedForCombat = false;
    bManualPaused = false;
    bHasMoveGoal = false;
    LastReachedPointIndex = INDEX_NONE;
    bNeedsRouteRejoin = false;
    bOpenLoopReturnPending = false;
    bWaitingForGroupDirection = false;
    OnRouteChanged.Broadcast(Route);

    if (bStartImmediately && Route) StartRoute();
}

bool UARPGAISplineComponent::FindAndAssignRouteById(bool bStartImmediately)
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return false;
    Route = nullptr;
    const bool bFound = ResolveRouteIfNeeded();
    if (bFound && bStartImmediately) StartRoute();
    return bFound;
}

void UARPGAISplineComponent::InitializeStartProgress()
{
    if (!Route) return;
    const float Length = Route->GetRouteLength();
    const int32 PointCount = Route->GetRoutePointCount();
    CurrentDirectionSign = bStartMovingForward ? 1 : -1;

    switch (StartMode)
    {
        case EARPGAISplineStartMode::FirstPoint:
            CurrentDistanceAlongRoute = 0.f;
            CurrentDirectionSign = 1;
            break;
        case EARPGAISplineStartMode::LastPoint:
            CurrentDistanceAlongRoute = Length;
            CurrentDirectionSign = -1;
            break;
        case EARPGAISplineStartMode::ExplicitPoint:
        {
            const int32 Index = FMath::Clamp(ExplicitStartPoint, 0, FMath::Max(0, PointCount - 1));
            CurrentDistanceAlongRoute = Route->GetDistanceAtRoutePoint(Index);
            break;
        }
        case EARPGAISplineStartMode::RandomPoint:
        {
            if (PointCount > 0)
                CurrentDistanceAlongRoute = Route->GetDistanceAtRoutePoint(FMath::RandRange(0, PointCount - 1));
            else
                CurrentDistanceAlongRoute = FMath::FRandRange(0.f, Length);
            break;
        }
        case EARPGAISplineStartMode::NearestLocation:
        default:
            CurrentDistanceAlongRoute = GetOwner() ? Route->FindDistanceClosestToWorldLocation(GetOwner()->GetActorLocation()) : 0.f;
            break;
    }
    CurrentDistanceAlongRoute = FMath::Clamp(CurrentDistanceAlongRoute, 0.f, Length);
    CombatDepartureDistance = CurrentDistanceAlongRoute;
    bNeedsRouteRejoin = false;
    if (GetOwner() && Route)
    {
        const FVector Anchor = Route->GetLocationAtRouteDistance(CurrentDistanceAlongRoute);
        bNeedsRouteRejoin = FVector::DistSquared2D(GetOwner()->GetActorLocation(), Anchor) > FMath::Square(FMath::Max(10.f, AcceptanceRadius));
    }
}

bool UARPGAISplineComponent::StartRoute()
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || !bEnabled) return false;
    if (!ResolveRouteIfNeeded() || !Route || Route->GetRouteLength() <= KINDA_SMALL_NUMBER) return false;

    InitializeStartProgress();
    bRouteActive = true;
    bRouteFinished = false;
    bManualPaused = false;
    bWaitingAtPoint = false;
    bSuspendedForCombat = false;
    bHasMoveGoal = false;
    WaitUntilWorldTime = -1.f;
    RetryMoveAfterWorldTime = -1.f;
    ResumeAfterWorldTime = -1.f;
    LastReachedPointIndex = INDEX_NONE;
    bOpenLoopReturnPending = false;
    bFinishAfterWait = false;
    bWaitingForGroupDirection = false;
    SetWandererForSpline(true);
    EnsureUpdateTimer();
    OnRouteStarted.Broadcast();
    return true;
}

void UARPGAISplineComponent::StopRoute(bool bStopMovement)
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;
    bRouteActive = false;
    bRouteFinished = false;
    bManualPaused = false;
    bWaitingAtPoint = false;
    bSuspendedForCombat = false;
    bHasMoveGoal = false;
    bFinishAfterWait = false;
    bWaitingForGroupDirection = false;
    if (bStopMovement)
        if (AAIController* AI = GetAIController()) AI->StopMovement();
    SetWandererForSpline(false);
    if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(UpdateTimer);
}

void UARPGAISplineComponent::PauseRoute(bool bStopMovement)
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;
    bManualPaused = true;
    bHasMoveGoal = false;
    if (bStopMovement)
        if (AAIController* AI = GetAIController()) AI->StopMovement();
    if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(UpdateTimer);
}

bool UARPGAISplineComponent::ResumeRoute()
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || !bEnabled || !Route) return false;
    bManualPaused = false;
    bRouteActive = true;
    bRouteFinished = false;
    bHasMoveGoal = false;
    SetWandererForSpline(true);
    EnsureUpdateTimer();
    return true;
}

void UARPGAISplineComponent::RejoinNearestRouteLocation()
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || !Route) return;
    CurrentDistanceAlongRoute = Route->FindDistanceClosestToWorldLocation(GetOwner()->GetActorLocation());
    CurrentDistanceAlongRoute = FMath::Clamp(CurrentDistanceAlongRoute, 0.f, Route->GetRouteLength());
    bNeedsRouteRejoin = true;
    bHasMoveGoal = false;
    bWaitingAtPoint = false;
    WaitUntilWorldTime = -1.f;
}

bool UARPGAISplineComponent::IsCombatActive() const
{
    if (!GetOwner()) return false;
    if (const UARPGAICombatComponent* AICombat = GetOwner()->FindComponentByClass<UARPGAICombatComponent>())
        return IsValid(AICombat->CurrentTarget);
    return false;
}

void UARPGAISplineComponent::NotifyCombatStarted()
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || !bPauseDuringCombat || !bRouteActive) return;
    SuspendForCombatInternal();
}

void UARPGAISplineComponent::NotifyCombatEnded()
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || !bSuspendedForCombat) return;
    if (!bResumeAfterCombat)
    {
        bSuspendedForCombat = false;
        return;
    }
    ResumeAfterWorldTime = GetWorld() ? GetWorld()->GetTimeSeconds() + FMath::Max(0.f, CombatResumeDelay) : 0.f;
}

void UARPGAISplineComponent::SetGroupDirectionLeader(APawn* NewLeader, bool bSynchronizeDirection, float MaxSeparation)
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;
    APawn* OwnerPawn = Cast<APawn>(GetOwner());
    GroupDirectionLeader = (IsValid(NewLeader) && NewLeader != OwnerPawn) ? NewLeader : nullptr;
    bSynchronizeDirectionWithGroupLeader = bSynchronizeDirection;
    GroupMaxSeparation = GroupDirectionLeader ? FMath::Max(0.f, MaxSeparation) : 0.f;
    bWaitingForGroupDirection = false;
    if (SyncDirectionFromGroupLeader())
    {
        bHasMoveGoal = false;
        if (AAIController* AI = GetAIController()) AI->StopMovement();
    }
}

void UARPGAISplineComponent::SuspendForCombatInternal()
{
    if (bSuspendedForCombat) return;
    CombatDepartureDistance = CurrentDistanceAlongRoute;
    bSuspendedForCombat = true;
    bHasMoveGoal = false;
    bWaitingAtPoint = false;
    WaitUntilWorldTime = -1.f;
    ResumeAfterWorldTime = -1.f;
    OnRoutePausedForCombat.Broadcast();
}

void UARPGAISplineComponent::ResumeAfterCombatInternal()
{
    if (!bSuspendedForCombat || !Route) return;
    bSuspendedForCombat = false;
    ResumeAfterWorldTime = -1.f;
    if (CombatResumeMode == EARPGAISplineResumeMode::NearestLocation)
        RejoinNearestRouteLocation();
    else
        CurrentDistanceAlongRoute = FMath::Clamp(CombatDepartureDistance, 0.f, Route->GetRouteLength());
    bHasMoveGoal = false;
    OnRouteResumedAfterCombat.Broadcast();
}

bool UARPGAISplineComponent::ProjectGoalToNavigation(const FVector& RawGoal, FVector& OutGoal) const
{
    OutGoal = RawGoal;
    if (!bProjectGoalsToNavMesh) return true;
    UNavigationSystemV1* Nav = GetWorld() ? FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()) : nullptr;
    if (!Nav) return false;
    FNavLocation Projected;
    if (!Nav->ProjectPointToNavigation(RawGoal, Projected, NavProjectionExtent)) return false;
    OutGoal = Projected.Location;
    return true;
}

int32 UARPGAISplineComponent::FindPointAtDistance(float Distance, float Tolerance) const
{
    if (!Route) return INDEX_NONE;
    const int32 Count = Route->GetRoutePointCount();
    for (int32 Index = 0; Index < Count; ++Index)
        if (FMath::Abs(Route->GetDistanceAtRoutePoint(Index) - Distance) <= Tolerance) return Index;
    return INDEX_NONE;
}

int32 UARPGAISplineComponent::FindNextPointBetween(float StartDistance, float EndDistance, int32 DirectionSign) const
{
    if (!Route) return INDEX_NONE;
    const int32 Count = Route->GetRoutePointCount();
    int32 BestIndex = INDEX_NONE;
    float BestDistance = DirectionSign > 0 ? TNumericLimits<float>::Max() : -TNumericLimits<float>::Max();

    for (int32 Index = 0; Index < Count; ++Index)
    {
        const float PointDistance = Route->GetDistanceAtRoutePoint(Index);
        if (DirectionSign > 0)
        {
            if (PointDistance > StartDistance + 1.f && PointDistance <= EndDistance + 1.f && PointDistance < BestDistance)
            {
                BestDistance = PointDistance;
                BestIndex = Index;
            }
        }
        else
        {
            if (PointDistance < StartDistance - 1.f && PointDistance >= EndDistance - 1.f && PointDistance > BestDistance)
            {
                BestDistance = PointDistance;
                BestIndex = Index;
            }
        }
    }
    return BestIndex;
}

EARPGAISplinePatrolMode UARPGAISplineComponent::GetEffectivePatrolMode() const
{
    if (!bUseRouteTraversalSettings || !Route) return PatrolMode;
    if (!Route->bLoopRoute) return EARPGAISplinePatrolMode::Once;
    if (Route->bClosedLoop) return EARPGAISplinePatrolMode::Loop;
    return Route->bReverseAtOpenEnds ? EARPGAISplinePatrolMode::PingPong : EARPGAISplinePatrolMode::Loop;
}

bool UARPGAISplineComponent::SyncDirectionFromGroupLeader()
{
    APawn* OwnerPawn = Cast<APawn>(GetOwner());
    if (!bSynchronizeDirectionWithGroupLeader) return false;
    if (!IsValid(GroupDirectionLeader) || GroupDirectionLeader == OwnerPawn)
    {
        GroupDirectionLeader = nullptr;
        return false;
    }

    const UARPGAISplineComponent* LeaderSpline = GroupDirectionLeader->FindComponentByClass<UARPGAISplineComponent>();
    if (!LeaderSpline || !LeaderSpline->IsRouteActive() || LeaderSpline->Route != Route) return false;

    const int32 DesiredSign = LeaderSpline->CurrentDirectionSign >= 0 ? 1 : -1;
    if (DesiredSign == CurrentDirectionSign) return false;
    CurrentDirectionSign = DesiredSign;
    return true;
}

bool UARPGAISplineComponent::IsDirectionBlockedAtCurrentBoundary() const
{
    if (!Route) return false;
    const float Length = Route->GetRouteLength();
    const bool bAtEnd = CurrentDistanceAlongRoute >= Length - 1.f;
    const bool bAtStart = CurrentDistanceAlongRoute <= 1.f;
    return (bAtEnd && CurrentDirectionSign > 0) || (bAtStart && CurrentDirectionSign < 0);
}

float UARPGAISplineComponent::ComputeNextGoalDistance() const
{
    if (!Route) return CurrentDistanceAlongRoute;

    const float Length = Route->GetRouteLength();
    const float Step = FMath::Max(50.f, FollowStepDistance);
    float Desired = FMath::Clamp(CurrentDistanceAlongRoute + Step * static_cast<float>(CurrentDirectionSign), 0.f, Length);

    const int32 PointIndex = FindNextPointBetween(CurrentDistanceAlongRoute, Desired, CurrentDirectionSign);
    if (PointIndex != INDEX_NONE)
        Desired = Route->GetDistanceAtRoutePoint(PointIndex);
    return Desired;
}

bool UARPGAISplineComponent::IssueNextMove()
{
    if (!Route || !GetOwner()) return false;
    AAIController* AI = GetAIController();
    if (!AI) return false;

    if (bNeedsRouteRejoin)
    {
        CurrentGoalDistance = CurrentDistanceAlongRoute;
    }
    else if (bOpenLoopReturnPending)
    {
        const float Length = Route->GetRouteLength();
        CurrentGoalDistance = CurrentDistanceAlongRoute >= Length - 1.f ? 0.f : Length;
    }
    else
    {
        CurrentGoalDistance = ComputeNextGoalDistance();
    }

    FVector RawGoal = Route->GetLocationAtRouteDistance(CurrentGoalDistance);
    if (!FMath::IsNearlyZero(RuntimeLateralOffset))
    {
        FVector Tangent = Route->GetDirectionAtRouteDistance(CurrentGoalDistance);
        Tangent.Z = 0.f;
        Tangent.Normalize();
        const FVector Right = FVector::CrossProduct(FVector::UpVector, Tangent).GetSafeNormal();
        RawGoal += Right * RuntimeLateralOffset;
    }

    if (!ProjectGoalToNavigation(RawGoal, CurrentGoalLocation))
    {
        RetryMoveAfterWorldTime = GetWorld() ? GetWorld()->GetTimeSeconds() + FMath::Max(0.05f, FailedMoveRetryDelay) : 0.f;
        OnMoveFailed.Broadcast(RawGoal);
        return false;
    }

    const EPathFollowingRequestResult::Type Result = AI->MoveToLocation(
        CurrentGoalLocation,
        FMath::Max(5.f, AcceptanceRadius),
        false,
        bUsePathfinding,
        true,
        false,
        NavigationFilterClass,
        bAllowPartialPaths);

    if (Result == EPathFollowingRequestResult::Failed)
    {
        RetryMoveAfterWorldTime = GetWorld() ? GetWorld()->GetTimeSeconds() + FMath::Max(0.05f, FailedMoveRetryDelay) : 0.f;
        OnMoveFailed.Broadcast(CurrentGoalLocation);
        return false;
    }

    bHasMoveGoal = true;
    LastGoalDistanceToPawn = FVector::Dist2D(GetOwner()->GetActorLocation(), CurrentGoalLocation);
    LastGoalProgressWorldTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
    return true;
}

void UARPGAISplineComponent::HandleReachedRoutePoint(int32 PointIndex)
{
    if (!Route || PointIndex == INDEX_NONE) return;
    LastReachedPointIndex = PointIndex;
    const FARPGAISplinePointSettings Settings = Route->GetRoutePointSettings(PointIndex);
    OnRoutePointReached.Broadcast(PointIndex, Settings.PointId);

    const float WaitSeconds = FMath::Max(0.f, Settings.WaitTime) + (Settings.RandomWaitTime > 0.f ? FMath::FRandRange(0.f, Settings.RandomWaitTime) : 0.f);
    if (WaitSeconds > 0.f && GetWorld())
    {
        bWaitingAtPoint = true;
        WaitUntilWorldTime = GetWorld()->GetTimeSeconds() + WaitSeconds;
        if (AAIController* AI = GetAIController()) AI->StopMovement();
        if (Settings.bFaceSplineDirectionWhileWaiting) FaceSplineDirection();
    }
}

void UARPGAISplineComponent::HandleRouteBoundary()
{
    if (!Route) return;
    const float Length = Route->GetRouteLength();
    const bool bAtEnd = CurrentDistanceAlongRoute >= Length - 1.f;
    const bool bAtStart = CurrentDistanceAlongRoute <= 1.f;
    if (!bAtEnd && !bAtStart) return;

    const EARPGAISplinePatrolMode EffectiveMode = GetEffectivePatrolMode();

    // Ping-pong is the only mode where a member can independently reverse and split a group.
    // Followers therefore wait for the group's direction leader to reverse first. Loop/Once
    // keep their normal boundary behavior because no opposite-direction split can occur.
    if (EffectiveMode == EARPGAISplinePatrolMode::PingPong && bSynchronizeDirectionWithGroupLeader && IsValid(GroupDirectionLeader))
    {
        SyncDirectionFromGroupLeader();
        if (IsDirectionBlockedAtCurrentBoundary())
        {
            bWaitingForGroupDirection = true;
            if (AAIController* AI = GetAIController()) AI->StopMovement();
            return;
        }
        bWaitingForGroupDirection = false;
        return;
    }

    switch (EffectiveMode)
    {
        case EARPGAISplinePatrolMode::Once:
            if (bWaitingAtPoint) bFinishAfterWait = true;
            else FinishRoute();
            break;
        case EARPGAISplinePatrolMode::PingPong:
            CurrentDirectionSign *= -1;
            break;
        case EARPGAISplinePatrolMode::Loop:
        default:
            if (Route->Spline && Route->Spline->IsClosedLoop())
            {
                CurrentDistanceAlongRoute = bAtEnd ? 0.f : Length;
            }
            else
            {
                // Open-loop mode returns through Navigation instead of attaching/teleporting the pawn.
                bOpenLoopReturnPending = true;
            }
            break;
    }
}

void UARPGAISplineComponent::FinishRoute()
{
    if (bRouteFinished) return;
    bRouteFinished = true;
    bRouteActive = false;
    bHasMoveGoal = false;
    if (AAIController* AI = GetAIController()) AI->StopMovement();
    SetWandererForSpline(false);
    if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(UpdateTimer);
    OnRouteFinished.Broadcast();
}

void UARPGAISplineComponent::HandleReachedGoal()
{
    const bool bWasRejoining = bNeedsRouteRejoin;
    const bool bWasOpenLoopReturn = bOpenLoopReturnPending;
    CurrentDistanceAlongRoute = CurrentGoalDistance;
    bHasMoveGoal = false;
    bNeedsRouteRejoin = false;
    bOpenLoopReturnPending = false;
    const int32 PointIndex = FindPointAtDistance(CurrentDistanceAlongRoute);
    if (PointIndex != INDEX_NONE && PointIndex != LastReachedPointIndex)
        HandleReachedRoutePoint(PointIndex);
    if (!bWasRejoining && !bWasOpenLoopReturn)
        HandleRouteBoundary();
}

void UARPGAISplineComponent::FaceSplineDirection() const
{
    if (!GetOwner() || !Route) return;
    FVector Direction = Route->GetDirectionAtRouteDistance(CurrentDistanceAlongRoute) * static_cast<float>(CurrentDirectionSign);
    Direction.Z = 0.f;
    if (!Direction.IsNearlyZero()) GetOwner()->SetActorRotation(Direction.Rotation());
}

void UARPGAISplineComponent::SetWandererForSpline(bool bSplineOwnsMovement)
{
    if (!GetOwner()) return;
    if (UARPGWandererComponent* Wanderer = GetOwner()->FindComponentByClass<UARPGWandererComponent>())
    {
        if (bSplineOwnsMovement)
        {
            if (!bCapturedWandererState)
            {
                bWandererWasEnabledBeforeSpline = Wanderer->bEnabled;
                bCapturedWandererState = true;
            }
            Wanderer->SetWandererEnabled(false);
        }
        else if (bCapturedWandererState)
        {
            const UARPGAICombatComponent* AICombat = GetOwner()->FindComponentByClass<UARPGAICombatComponent>();
            if (!AICombat || !IsValid(AICombat->CurrentTarget))
                Wanderer->SetWandererEnabled(bWandererWasEnabledBeforeSpline);
            bCapturedWandererState = false;
        }
    }
}

FVector UARPGAISplineComponent::GetCurrentRouteAnchorLocation() const
{
    return Route ? Route->GetLocationAtRouteDistance(CurrentDistanceAlongRoute) : (GetOwner() ? GetOwner()->GetActorLocation() : FVector::ZeroVector);
}

FVector UARPGAISplineComponent::GetCurrentSplineDirection() const
{
    return Route ? Route->GetDirectionAtRouteDistance(CurrentDistanceAlongRoute) * static_cast<float>(CurrentDirectionSign) : FVector::ForwardVector;
}

void UARPGAISplineComponent::UpdateSplineMovement()
{
    if (!bEnabled || !bRouteActive || bRouteFinished || bManualPaused || !GetOwner() || !GetOwner()->HasAuthority()) return;
    if (!ResolveRouteIfNeeded() || !Route) return;

    if (const UARPGCombatComponent* Combat = GetOwner()->FindComponentByClass<UARPGCombatComponent>())
        if (!Combat->IsAlive()) return;

    const bool bInCombat = bPauseDuringCombat && IsCombatActive();
    if (bInCombat)
    {
        SuspendForCombatInternal();
        return;
    }

    if (bSuspendedForCombat)
    {
        if (!bResumeAfterCombat) return;
        const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
        if (ResumeAfterWorldTime < 0.f) ResumeAfterWorldTime = Now + FMath::Max(0.f, CombatResumeDelay);
        if (Now < ResumeAfterWorldTime) return;
        ResumeAfterCombatInternal();
    }

    if (IsValid(GroupDirectionLeader))
    {
        const bool bDirectionChanged = SyncDirectionFromGroupLeader();
        if (bDirectionChanged)
        {
            bHasMoveGoal = false;
            if (AAIController* AI = GetAIController()) AI->StopMovement();
        }

        if (bWaitingForGroupDirection)
        {
            if (IsDirectionBlockedAtCurrentBoundary()) return;
            bWaitingForGroupDirection = false;
            bHasMoveGoal = false;
        }

        if (GroupMaxSeparation > 0.f && !bNeedsRouteRejoin)
        {
            const UARPGAISplineComponent* LeaderSpline = GroupDirectionLeader->FindComponentByClass<UARPGAISplineComponent>();
            if (LeaderSpline && LeaderSpline->Route == Route && LeaderSpline->IsRouteActive())
            {
                const float Separation = FVector::Dist2D(GetOwner()->GetActorLocation(), GroupDirectionLeader->GetActorLocation());
                if (Separation > GroupMaxSeparation)
                {
                    CurrentDistanceAlongRoute = FMath::Clamp(LeaderSpline->CurrentDistanceAlongRoute, 0.f, Route->GetRouteLength());
                    bNeedsRouteRejoin = true;
                    bHasMoveGoal = false;
                    if (AAIController* AI = GetAIController()) AI->StopMovement();
                }
            }
        }
    }

    const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
    if (bWaitingAtPoint)
    {
        if (Now < WaitUntilWorldTime) return;
        bWaitingAtPoint = false;
        WaitUntilWorldTime = -1.f;
        if (bFinishAfterWait)
        {
            bFinishAfterWait = false;
            FinishRoute();
            return;
        }
    }
    if (Now < RetryMoveAfterWorldTime) return;

    if (bHasMoveGoal)
    {
        const float DistanceToGoal = FVector::Dist2D(GetOwner()->GetActorLocation(), CurrentGoalLocation);
        if (DistanceToGoal <= FMath::Max(10.f, AcceptanceRadius))
        {
            HandleReachedGoal();
        }
        else
        {
            if (DistanceToGoal + FMath::Max(0.f, StalledProgressTolerance) < LastGoalDistanceToPawn)
            {
                LastGoalDistanceToPawn = DistanceToGoal;
                LastGoalProgressWorldTime = Now;
            }
            else if (bRetryStalledMoves && Now - LastGoalProgressWorldTime >= FMath::Max(0.5f, StalledMoveRetrySeconds))
            {
                bHasMoveGoal = false;
                RetryMoveAfterWorldTime = Now + FMath::Max(0.05f, FailedMoveRetryDelay);
                if (AAIController* AI = GetAIController()) AI->StopMovement();
                OnMoveFailed.Broadcast(CurrentGoalLocation);
            }
            return;
        }
    }

    if (bRouteActive && !bRouteFinished && !bWaitingAtPoint)
        IssueNextMove();
}
