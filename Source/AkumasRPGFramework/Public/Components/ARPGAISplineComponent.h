#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ARPGAISplineComponent.generated.h"

class AARPGAISplineRoute;
class AAIController;
class UNavigationQueryFilter;
class APawn;

UENUM(BlueprintType)
enum class EARPGAISplinePatrolMode : uint8
{
    Once UMETA(DisplayName="Once"),
    Loop UMETA(DisplayName="Loop"),
    PingPong UMETA(DisplayName="Ping Pong")
};

UENUM(BlueprintType)
enum class EARPGAISplineStartMode : uint8
{
    NearestLocation UMETA(DisplayName="Nearest Location On Route"),
    FirstPoint UMETA(DisplayName="First Route Point"),
    LastPoint UMETA(DisplayName="Last Route Point"),
    ExplicitPoint UMETA(DisplayName="Explicit Route Point"),
    RandomPoint UMETA(DisplayName="Random Route Point")
};

UENUM(BlueprintType)
enum class EARPGAISplineResumeMode : uint8
{
    NearestLocation UMETA(DisplayName="Rejoin At Nearest Route Location"),
    ContinuePreviousProgress UMETA(DisplayName="Return To Previous Route Progress")
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FARPGAISplineSimpleEvent);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FARPGAISplinePointReached, int32, PointIndex, FName, PointId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FARPGAISplineRouteChanged, AARPGAISplineRoute*, NewRoute);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FARPGAISplineMoveFailed, FVector, RequestedLocation);

/**
 * Server-authoritative NavMesh spline follower for ARPG AI.
 * The pawn is never attached to or transformed along the spline. The component samples a
 * look-ahead point from the route, projects that point to Navigation, and lets AAIController
 * path following move the pawn naturally through the world.
 */
UCLASS(ClassGroup=(ARPG), meta=(BlueprintSpawnableComponent))
class AKUMASRPGFRAMEWORK_API UARPGAISplineComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UARPGAISplineComponent();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI Spline|Core") bool bEnabled = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI Spline|Core") bool bAutoStart = true;
    /** Direct route reference for placed NPC instances. Spawners can assign this automatically at runtime. */
    UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="AI Spline|Core") TObjectPtr<AARPGAISplineRoute> Route;
    /** Optional route id used when a direct level reference is not convenient (for example Blueprint-spawned NPCs). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI Spline|Core") FName RouteId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI Spline|Core") bool bAutoFindRouteById = true;

    /** Use the route actor's Loop Route / Closed Loop / Reverse At Open Ends settings. Recommended and enabled by default. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI Spline|Patrol") bool bUseRouteTraversalSettings = true;
    /** Legacy/per-NPC override used only when Use Route Traversal Settings is disabled. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI Spline|Patrol", meta=(EditCondition="!bUseRouteTraversalSettings")) EARPGAISplinePatrolMode PatrolMode = EARPGAISplinePatrolMode::PingPong;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI Spline|Patrol") EARPGAISplineStartMode StartMode = EARPGAISplineStartMode::NearestLocation;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI Spline|Patrol", meta=(ClampMin="0")) int32 ExplicitStartPoint = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI Spline|Patrol") bool bStartMovingForward = true;
    /** How far along the spline each NavMesh movement request advances. Smaller values hug tight curves more closely. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI Spline|Patrol", meta=(ClampMin="50.0")) float FollowStepDistance = 300.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI Spline|Patrol", meta=(ClampMin="5.0")) float AcceptanceRadius = 80.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI Spline|Patrol", meta=(ClampMin="0.05")) float UpdateInterval = 0.20f;
    /** Optional lane offset from the spline centerline. Useful when several NPCs share a route. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI Spline|Patrol") float LateralOffset = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI Spline|Patrol") bool bRandomizeLateralOffset = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI Spline|Patrol", meta=(EditCondition="bRandomizeLateralOffset", ClampMin="0.0")) float RandomLateralOffsetRange = 90.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI Spline|Navigation") bool bProjectGoalsToNavMesh = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI Spline|Navigation") FVector NavProjectionExtent = FVector(250.f, 250.f, 500.f);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI Spline|Navigation") TSubclassOf<UNavigationQueryFilter> NavigationFilterClass;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI Spline|Navigation") bool bAllowPartialPaths = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI Spline|Navigation") bool bUsePathfinding = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI Spline|Navigation", meta=(ClampMin="0.05")) float FailedMoveRetryDelay = 0.75f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI Spline|Navigation") bool bRetryStalledMoves = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI Spline|Navigation", meta=(EditCondition="bRetryStalledMoves", ClampMin="0.5")) float StalledMoveRetrySeconds = 3.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI Spline|Navigation", meta=(EditCondition="bRetryStalledMoves", ClampMin="0.0")) float StalledProgressTolerance = 15.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI Spline|Combat") bool bPauseDuringCombat = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI Spline|Combat") bool bResumeAfterCombat = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI Spline|Combat") EARPGAISplineResumeMode CombatResumeMode = EARPGAISplineResumeMode::NearestLocation;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI Spline|Combat", meta=(ClampMin="0.0")) float CombatResumeDelay = 0.25f;
    /** Makes the AI combat leash use the point where this NPC left its route rather than its original spawn position. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI Spline|Combat") bool bUseRouteAsCombatLeashAnchor = true;

    /** Optional leader assigned by ARPG AI Spawner. Followers mirror its route direction so a spawned group never splits and runs both ways. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="AI Spline|Group") TObjectPtr<APawn> GroupDirectionLeader;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="AI Spline|Group") bool bSynchronizeDirectionWithGroupLeader = true;
    /** Zero disables physical route-progress cohesion while retaining optional direction synchronization. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="AI Spline|Group") float GroupMaxSeparation = 0.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="AI Spline|Group") bool bWaitingForGroupDirection = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="AI Spline|Runtime") bool bRouteActive = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="AI Spline|Runtime") bool bWaitingAtPoint = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="AI Spline|Runtime") bool bSuspendedForCombat = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="AI Spline|Runtime") bool bRouteFinished = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="AI Spline|Runtime") float CurrentDistanceAlongRoute = 0.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="AI Spline|Runtime") int32 CurrentDirectionSign = 1;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="AI Spline|Runtime") int32 LastReachedPointIndex = INDEX_NONE;

    UPROPERTY(BlueprintAssignable, Category="AI Spline|Events") FARPGAISplineRouteChanged OnRouteChanged;
    UPROPERTY(BlueprintAssignable, Category="AI Spline|Events") FARPGAISplineSimpleEvent OnRouteStarted;
    UPROPERTY(BlueprintAssignable, Category="AI Spline|Events") FARPGAISplineSimpleEvent OnRoutePausedForCombat;
    UPROPERTY(BlueprintAssignable, Category="AI Spline|Events") FARPGAISplineSimpleEvent OnRouteResumedAfterCombat;
    UPROPERTY(BlueprintAssignable, Category="AI Spline|Events") FARPGAISplineSimpleEvent OnRouteFinished;
    UPROPERTY(BlueprintAssignable, Category="AI Spline|Events") FARPGAISplinePointReached OnRoutePointReached;
    UPROPERTY(BlueprintAssignable, Category="AI Spline|Events") FARPGAISplineMoveFailed OnMoveFailed;

    UFUNCTION(BlueprintCallable, Category="ARPG|AI Spline", meta=(BlueprintAuthorityOnly)) void SetRoute(AARPGAISplineRoute* NewRoute, bool bStartImmediately = true);
    UFUNCTION(BlueprintCallable, Category="ARPG|AI Spline", meta=(BlueprintAuthorityOnly)) bool FindAndAssignRouteById(bool bStartImmediately = true);
    UFUNCTION(BlueprintCallable, Category="ARPG|AI Spline", meta=(BlueprintAuthorityOnly)) bool StartRoute();
    UFUNCTION(BlueprintCallable, Category="ARPG|AI Spline", meta=(BlueprintAuthorityOnly)) void StopRoute(bool bStopMovement = true);
    UFUNCTION(BlueprintCallable, Category="ARPG|AI Spline", meta=(BlueprintAuthorityOnly)) void PauseRoute(bool bStopMovement = true);
    UFUNCTION(BlueprintCallable, Category="ARPG|AI Spline", meta=(BlueprintAuthorityOnly)) bool ResumeRoute();
    UFUNCTION(BlueprintCallable, Category="ARPG|AI Spline", meta=(BlueprintAuthorityOnly)) void RejoinNearestRouteLocation();
    UFUNCTION(BlueprintCallable, Category="ARPG|AI Spline", meta=(BlueprintAuthorityOnly)) void NotifyCombatStarted();
    UFUNCTION(BlueprintCallable, Category="ARPG|AI Spline", meta=(BlueprintAuthorityOnly)) void NotifyCombatEnded();
    /** Synchronize this pawn's travel direction to another spawned pawn without attaching either pawn to the spline. */
    UFUNCTION(BlueprintCallable, Category="ARPG|AI Spline", meta=(BlueprintAuthorityOnly)) void SetGroupDirectionLeader(APawn* NewLeader, bool bSynchronizeDirection, float MaxSeparation);

    UFUNCTION(BlueprintPure, Category="ARPG|AI Spline") bool IsRouteActive() const { return bRouteActive && Route != nullptr; }
    UFUNCTION(BlueprintPure, Category="ARPG|AI Spline") FVector GetCurrentRouteAnchorLocation() const;
    UFUNCTION(BlueprintPure, Category="ARPG|AI Spline") FVector GetCurrentSplineDirection() const;

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

protected:
    FTimerHandle UpdateTimer;
    float CurrentGoalDistance = 0.f;
    FVector CurrentGoalLocation = FVector::ZeroVector;
    bool bHasMoveGoal = false;
    bool bManualPaused = false;
    float WaitUntilWorldTime = -1.f;
    float RetryMoveAfterWorldTime = -1.f;
    float CombatDepartureDistance = 0.f;
    float ResumeAfterWorldTime = -1.f;
    float RuntimeLateralOffset = 0.f;
    float LastGoalDistanceToPawn = TNumericLimits<float>::Max();
    float LastGoalProgressWorldTime = 0.f;
    bool bNeedsRouteRejoin = false;
    bool bOpenLoopReturnPending = false;
    bool bFinishAfterWait = false;
    bool bCapturedWandererState = false;
    bool bWandererWasEnabledBeforeSpline = false;

    void UpdateSplineMovement();
    void EnsureUpdateTimer();
    AAIController* GetAIController() const;
    bool ResolveRouteIfNeeded();
    void InitializeStartProgress();
    bool IssueNextMove();
    bool ProjectGoalToNavigation(const FVector& RawGoal, FVector& OutGoal) const;
    float ComputeNextGoalDistance() const;
    EARPGAISplinePatrolMode GetEffectivePatrolMode() const;
    bool SyncDirectionFromGroupLeader();
    bool IsDirectionBlockedAtCurrentBoundary() const;
    int32 FindPointAtDistance(float Distance, float Tolerance = 2.f) const;
    int32 FindNextPointBetween(float StartDistance, float EndDistance, int32 DirectionSign) const;
    void HandleReachedGoal();
    void HandleReachedRoutePoint(int32 PointIndex);
    void HandleRouteBoundary();
    void FinishRoute();
    void SuspendForCombatInternal();
    void ResumeAfterCombatInternal();
    bool IsCombatActive() const;
    void FaceSplineDirection() const;
    void SetWandererForSpline(bool bSplineOwnsMovement);
};
