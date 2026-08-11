#include "Actors/ARPGAISpawner.h"

#include "AkumasRPGFramework.h"
#include "AIController.h"
#include "Actors/ARPGAISplineRoute.h"
#include "Components/ARPGAICombatComponent.h"
#include "Components/ARPGAISplineComponent.h"
#include "Components/ARPGCombatComponent.h"
#include "Components/ARPGWandererComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "NavigationSystem.h"
#include "World/ARPGDayNightCycle.h"
#include "TimerManager.h"

AARPGAISpawner::AARPGAISpawner()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = false; // Server-only manager; spawned pawns replicate normally.
}

void AARPGAISpawner::BeginPlay()
{
    Super::BeginPlay();
    if (!HasAuthority()) return;

    CurrentGroupSplineDirectionSign = ChooseGroupSplineDirectionSign();
    InitializeDayNightPopulation();

    if (bEnableDistanceBasedPopulation)
    {
        // Distance streaming supersedes unconditional BeginPlay population so far-away spawners start unloaded.
        bPopulationActive = false;
        if (GetWorld())
        {
            const float Rate = FMath::Max(0.25f, PopulationCheckInterval);
            const float FirstDelay = FMath::FRandRange(0.05f, Rate); // Stagger many spawners across frames.
            GetWorld()->GetTimerManager().SetTimer(
                PopulationRelevanceTimer,
                this,
                &AARPGAISpawner::CheckPopulationRelevance,
                Rate,
                true,
                FirstDelay);
        }
        CheckPopulationRelevance();
    }
    else
    {
        bPopulationActive = true;
        if (bSpawnOnBeginPlay) SpawnGroup();
        StartActiveRuntimeTimers();
    }
}

void AARPGAISpawner::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    ShutdownDayNightPopulation();
    Super::EndPlay(EndPlayReason);
}

void AARPGAISpawner::InitializeDayNightPopulation()
{
    bMidnightPopulationActive = false;
    if (!HasAuthority() || !bEnableMidnightPopulationSwap) return;

    ResolveAndBindDayNightCycle();
    if (ResolvedDayNightCycle.IsValid())
        bMidnightPopulationActive = IsWorldTimeInMidnightPopulationWindow();
}

void AARPGAISpawner::ShutdownDayNightPopulation()
{
    if (AARPGDayNightCycle* Cycle = ResolvedDayNightCycle.Get())
    {
        Cycle->OnHourChanged.RemoveDynamic(this, &AARPGAISpawner::HandleWorldHourChanged);
        Cycle->OnDayStarted.RemoveDynamic(this, &AARPGAISpawner::HandleDayStarted);
    }
    ResolvedDayNightCycle.Reset();
}

void AARPGAISpawner::ResolveAndBindDayNightCycle()
{
    if (!HasAuthority() || !bEnableMidnightPopulationSwap || !GetWorld()) return;

    AARPGDayNightCycle* Cycle = DayNightCycleOverride;
    if (!IsValid(Cycle))
    {
        for (TActorIterator<AARPGDayNightCycle> It(GetWorld()); It; ++It)
        {
            if (IsValid(*It))
            {
                Cycle = *It;
                break;
            }
        }
    }

    if (!IsValid(Cycle))
    {
        if (!bWarnedMissingDayNightCycle)
        {
            UE_LOG(LogARPG, Warning, TEXT("AI Spawner '%s' has Enable Midnight Population Swap enabled but no ARPGDayNightCycle was found. Using the normal Spawn Table until a cycle becomes available."), *GetName());
            bWarnedMissingDayNightCycle = true;
        }
        return;
    }

    if (ResolvedDayNightCycle.Get() == Cycle) return;
    ShutdownDayNightPopulation();

    ResolvedDayNightCycle = Cycle;
    bWarnedMissingDayNightCycle = false;
    Cycle->OnHourChanged.AddDynamic(this, &AARPGAISpawner::HandleWorldHourChanged);
    Cycle->OnDayStarted.AddDynamic(this, &AARPGAISpawner::HandleDayStarted);
}

bool AARPGAISpawner::IsWorldTimeInMidnightPopulationWindow() const
{
    const AARPGDayNightCycle* Cycle = ResolvedDayNightCycle.Get();
    if (!IsValid(Cycle)) return false;

    // Midnight population begins at 00:00 and remains until the configured semantic Day Start Hour.
    // This intentionally lets normal/daylight NPCs remain through the evening and swaps only at true midnight.
    const float MorningHour = FMath::Clamp(Cycle->DayStartHour, 0.f, 24.f);
    if (MorningHour <= KINDA_SMALL_NUMBER) return false;
    return Cycle->GetWorldHour() < MorningHour;
}

void AARPGAISpawner::CheckDayNightPopulationPhase()
{
    if (!HasAuthority() || !bEnableMidnightPopulationSwap) return;

    if (!ResolvedDayNightCycle.IsValid()) ResolveAndBindDayNightCycle();
    if (!ResolvedDayNightCycle.IsValid()) return;

    ApplyDayNightPopulationPhase(IsWorldTimeInMidnightPopulationWindow());
}

void AARPGAISpawner::ApplyDayNightPopulationPhase(bool bUseMidnightPopulation, bool bForceRefresh)
{
    if (!HasAuthority() || !bEnableMidnightPopulationSwap) return;
    if (!bForceRefresh && bMidnightPopulationActive == bUseMidnightPopulation) return;

    bMidnightPopulationActive = bUseMidnightPopulation;

    if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(RespawnTimer);
    RespawnNotBeforeTime = 0.0;
    bWholeGroupRespawnPending = false;
    PreservedImmediatePopulationCount = 0;
    DesiredGroupSize = 0;

    // A phase transition is not a death/defeat. DespawnAll removes OnDestroyed bindings before Destroy(),
    // preventing phase cleanup from scheduling respawns or broadcasting group-defeated events.
    if (bPopulationActive)
    {
        DespawnAll(true);
        SpawnGroup();
    }

    UE_LOG(LogARPG, Verbose, TEXT("AI Spawner '%s' switched to %s population."),
        *GetName(), bMidnightPopulationActive ? TEXT("Midnight") : TEXT("Daylight"));
}

void AARPGAISpawner::HandleWorldHourChanged(int32 NewHour)
{
    // Re-evaluate on every authority hour change. This catches midnight and also remains correct when
    // simulated/fixed clocks jump across multiple hours in one update.
    (void)NewHour;
    CheckDayNightPopulationPhase();
}

void AARPGAISpawner::HandleDayStarted()
{
    CheckDayNightPopulationPhase();
}

void AARPGAISpawner::RefreshDayNightPopulationNow()
{
    if (!HasAuthority() || !bEnableMidnightPopulationSwap) return;

    ResolveAndBindDayNightCycle();
    if (!ResolvedDayNightCycle.IsValid()) return;
    ApplyDayNightPopulationPhase(IsWorldTimeInMidnightPopulationWindow(), true);
}

const TArray<FARPGSpawnEntry>& AARPGAISpawner::GetActiveSpawnTable() const
{
    if (bEnableMidnightPopulationSwap && bMidnightPopulationActive) return MidnightSpawnTable;
    return SpawnTable;
}

void AARPGAISpawner::GetActiveGroupSizeRange(int32& OutMinGroupSize, int32& OutMaxGroupSize) const
{
    if (bEnableMidnightPopulationSwap && bMidnightPopulationActive && bUseSeparateMidnightGroupSize)
    {
        OutMinGroupSize = MidnightMinGroupSize;
        OutMaxGroupSize = MidnightMaxGroupSize;
        return;
    }

    OutMinGroupSize = MinGroupSize;
    OutMaxGroupSize = MaxGroupSize;
}

void AARPGAISpawner::StartActiveRuntimeTimers()
{
    if (!HasAuthority() || !GetWorld()) return;

    FTimerManager& Timers = GetWorld()->GetTimerManager();
    Timers.ClearTimer(LeashTimer);
    Timers.ClearTimer(GroupCohesionTimer);

    // Also owns corpse cleanup, so keep this running for every loaded population.
    Timers.SetTimer(
        LeashTimer,
        this,
        &AARPGAISpawner::CheckLeashes,
        FMath::Max(0.1f, LeashCheckInterval),
        true);

    if (bStayTogether)
    {
        Timers.SetTimer(
            GroupCohesionTimer,
            this,
            &AARPGAISpawner::CheckGroupCohesion,
            FMath::Max(0.1f, GroupCohesionCheckInterval),
            true);
    }
}

void AARPGAISpawner::StopActiveRuntimeTimers()
{
    if (!GetWorld()) return;
    FTimerManager& Timers = GetWorld()->GetTimerManager();
    Timers.ClearTimer(LeashTimer);
    Timers.ClearTimer(GroupCohesionTimer);
}

float AARPGAISpawner::GetEffectiveDespawnRadius() const
{
    // Always keep a real hysteresis band even if a designer accidentally enters a smaller despawn radius.
    return FMath::Max(FMath::Max(100.f, DespawnRadius), FMath::Max(100.f, SpawnActivationRadius) + 100.f);
}

float AARPGAISpawner::FindNearestRelevantPlayerDistance(bool bIncludeSpawnedPawnAnchors) const
{
    UWorld* World = GetWorld();
    if (!World) return -1.f;

    float BestDistanceSq = MAX_flt;
    bool bFoundPlayer = false;
    const FVector SpawnerLocation = GetActorLocation();

    for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
    {
        const APlayerController* PC = It->Get();
        if (!IsValid(PC)) continue;
        const APawn* PlayerPawn = PC->GetPawn();
        if (!IsValid(PlayerPawn)) continue;

        bFoundPlayer = true;
        const FVector PlayerLocation = PlayerPawn->GetActorLocation();
        const float SpawnerDistanceSq = bUse2DPlayerDistance
            ? FVector::DistSquared2D(PlayerLocation, SpawnerLocation)
            : FVector::DistSquared(PlayerLocation, SpawnerLocation);
        BestDistanceSq = FMath::Min(BestDistanceSq, SpawnerDistanceSq);

        if (!bIncludeSpawnedPawnAnchors) continue;
        for (const APawn* SpawnedPawn : SpawnedPawns)
        {
            if (!IsValid(SpawnedPawn)) continue;
            const float PawnDistanceSq = bUse2DPlayerDistance
                ? FVector::DistSquared2D(PlayerLocation, SpawnedPawn->GetActorLocation())
                : FVector::DistSquared(PlayerLocation, SpawnedPawn->GetActorLocation());
            BestDistanceSq = FMath::Min(BestDistanceSq, PawnDistanceSq);
        }
    }

    return bFoundPlayer ? FMath::Sqrt(BestDistanceSq) : -1.f;
}

bool AARPGAISpawner::IsPlayerInsideSpawnActivationRadius() const
{
    const float Distance = FindNearestRelevantPlayerDistance(false);
    return Distance >= 0.f && Distance <= FMath::Max(100.f, SpawnActivationRadius);
}

bool AARPGAISpawner::HasAnySpawnedPawnInCombat() const
{
    for (const APawn* Pawn : SpawnedPawns)
        if (IsPawnAlive(Pawn) && IsPawnInCombat(Pawn)) return true;
    return false;
}

void AARPGAISpawner::SpawnUntilAliveCount(int32 TargetAliveCount)
{
    const int32 SafeTarget = FMath::Max(0, TargetAliveCount);
    while (GetAliveCount() < SafeTarget)
        if (!SpawnOne()) break;
}

void AARPGAISpawner::ResumePopulationAfterDistanceLoad()
{
    if (!HasAuthority() || !GetWorld()) return;

    GroupLeader = nullptr;
    CohesionRecoveringPawns.Reset();
    CurrentGroupSplineDirectionSign = ChooseGroupSplineDirectionSign();

    if (DesiredGroupSize <= 0)
    {
        SpawnGroup();
        PreservedImmediatePopulationCount = GetAliveCount();
        return;
    }

    // Never-respawn groups must not be resurrected simply because world-distance streaming unloaded/reloaded them.
    if (RespawnMode == EARPGRespawnMode::Never)
    {
        SpawnUntilAliveCount(FMath::Clamp(PreservedImmediatePopulationCount, 0, DesiredGroupSize));
        RefreshSplineGroupDirectionLeaders();
        return;
    }

    const double Now = GetWorld()->GetTimeSeconds();
    const float RemainingRespawn = RespawnNotBeforeTime > Now
        ? static_cast<float>(RespawnNotBeforeTime - Now)
        : 0.f;

    if (RemainingRespawn > 0.f)
    {
        // Restore only members that were alive when the area unloaded; dead members keep their real respawn cooldown.
        SpawnUntilAliveCount(FMath::Clamp(PreservedImmediatePopulationCount, 0, DesiredGroupSize));
        if (bWholeGroupRespawnPending && GetAliveCount() == 0)
            GetWorld()->GetTimerManager().SetTimer(RespawnTimer, this, &AARPGAISpawner::SpawnGroup, RemainingRespawn, false);
        else
            GetWorld()->GetTimerManager().SetTimer(RespawnTimer, this, &AARPGAISpawner::ReplenishGroup, RemainingRespawn, false);
    }
    else if (bWholeGroupRespawnPending || bRerollGroupSizeOnDistanceReload)
    {
        SpawnGroup();
    }
    else
    {
        SpawnUntilAliveCount(DesiredGroupSize);
        RespawnNotBeforeTime = 0.0;
    }

    RefreshSplineGroupDirectionLeaders();
    PreservedImmediatePopulationCount = GetAliveCount();
}

void AARPGAISpawner::ActivateDistancePopulation()
{
    if (!HasAuthority() || bPopulationActive) return;

    bPopulationActive = true;
    OutOfRangeSinceTime = -1.0;
    StartActiveRuntimeTimers();
    ResumePopulationAfterDistanceLoad();
    OnPopulationActivated.Broadcast();
}

void AARPGAISpawner::DeactivateDistancePopulation()
{
    if (!HasAuthority() || !bPopulationActive) return;

    PreservedImmediatePopulationCount = GetAliveCount();
    OutOfRangeSinceTime = -1.0;
    StopActiveRuntimeTimers();
    if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(RespawnTimer);

    // Remove delegates before destroying actors so distance-unload is never mistaken for defeat/death.
    DespawnAll(true);
    bPopulationActive = false;
    OnPopulationDeactivated.Broadcast();
}

void AARPGAISpawner::CheckPopulationRelevance()
{
    if (!HasAuthority() || !bEnableDistanceBasedPopulation) return;
    CheckDayNightPopulationPhase();

    const bool bUsePawnAnchors = bPopulationActive && bKeepLoadedNearSpawnedPawns;
    NearestRelevantPlayerDistance = FindNearestRelevantPlayerDistance(bUsePawnAnchors);

    if (!bPopulationActive)
    {
        OutOfRangeSinceTime = -1.0;
        if (bAutoSpawnWhenPlayerIsNear && NearestRelevantPlayerDistance >= 0.f &&
            NearestRelevantPlayerDistance <= FMath::Max(100.f, SpawnActivationRadius))
        {
            ActivateDistancePopulation();
        }
        return;
    }

    if (bPreventDistanceDespawnWhileInCombat && HasAnySpawnedPawnInCombat())
    {
        OutOfRangeSinceTime = -1.0;
        return;
    }

    const bool bStillRelevant = NearestRelevantPlayerDistance >= 0.f &&
        NearestRelevantPlayerDistance <= GetEffectiveDespawnRadius();
    if (bStillRelevant)
    {
        OutOfRangeSinceTime = -1.0;
        return;
    }

    const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
    if (OutOfRangeSinceTime < 0.0)
    {
        OutOfRangeSinceTime = Now;
        return;
    }

    if (DistanceDespawnDelay <= 0.f || (Now - OutOfRangeSinceTime) >= DistanceDespawnDelay)
        DeactivateDistancePopulation();
}

void AARPGAISpawner::EvaluatePopulationRelevanceNow()
{
    CheckPopulationRelevance();
}

void AARPGAISpawner::SetPopulationActive(bool bNewActive)
{
    if (!HasAuthority()) return;
    if (bNewActive) ActivateDistancePopulation();
    else DeactivateDistancePopulation();
}

bool AARPGAISpawner::IsPawnAlive(const APawn* Pawn) const
{
    if (!IsValid(Pawn)) return false;
    if (const UARPGCombatComponent* Combat = Pawn->FindComponentByClass<UARPGCombatComponent>())
        return Combat->IsAlive();
    return true;
}

bool AARPGAISpawner::IsPawnInCombat(const APawn* Pawn) const
{
    if (!IsValid(Pawn)) return false;
    if (const UARPGAICombatComponent* AICombat = Pawn->FindComponentByClass<UARPGAICombatComponent>())
        return IsValid(AICombat->CurrentTarget);
    return false;
}

int32 AARPGAISpawner::GetAliveCount() const
{
    int32 Count = 0;
    for (APawn* Pawn : SpawnedPawns)
        if (IsPawnAlive(Pawn)) ++Count;
    return Count;
}

TSubclassOf<APawn> AARPGAISpawner::ChoosePawnClass() const
{
    const TArray<FARPGSpawnEntry>& ActiveSpawnTable = GetActiveSpawnTable();

    float Total = 0.f;
    for (const FARPGSpawnEntry& Entry : ActiveSpawnTable)
        if (Entry.PawnClass && Entry.Weight > 0.f) Total += Entry.Weight;

    if (Total <= 0.f) return nullptr;

    float Roll = FMath::FRandRange(0.f, Total);
    for (const FARPGSpawnEntry& Entry : ActiveSpawnTable)
    {
        if (!Entry.PawnClass || Entry.Weight <= 0.f) continue;
        Roll -= Entry.Weight;
        if (Roll <= 0.f) return Entry.PawnClass;
    }
    return ActiveSpawnTable.IsEmpty() ? nullptr : ActiveSpawnTable.Last().PawnClass;
}

FVector AARPGAISpawner::ChooseSpawnLocation() const
{
    const FVector Origin = GetActorLocation();
    if (SpawnShape == EARPGSpawnerShape::Point) return Origin;

    FVector Candidate = Origin;
    if (SpawnShape == EARPGSpawnerShape::Circle)
    {
        const FVector2D Offset = FMath::RandPointInCircle(SpawnRadius);
        Candidate += FVector(Offset.X, Offset.Y, 0.f);
    }
    else
    {
        Candidate += FVector(
            FMath::FRandRange(-BoxExtents.X, BoxExtents.X),
            FMath::FRandRange(-BoxExtents.Y, BoxExtents.Y),
            FMath::FRandRange(-BoxExtents.Z, BoxExtents.Z));
    }

    if (UNavigationSystemV1* Nav = GetWorld() ? FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()) : nullptr)
    {
        FNavLocation Projected;
        if (Nav->ProjectPointToNavigation(Candidate, Projected, FVector(300.f, 300.f, 1000.f)))
            return Projected.Location;
    }
    return Candidate;
}

EARPGSpawnerMovementMode AARPGAISpawner::GetEffectiveMovementMode() const
{
    if (MovementMode != EARPGSpawnerMovementMode::Automatic) return MovementMode;
    return AssignedSplineRoute ? EARPGSpawnerMovementMode::SplineRoute : EARPGSpawnerMovementMode::HoldPosition;
}

int32 AARPGAISpawner::ChooseGroupSplineDirectionSign() const
{
    switch (GroupSplineDirection)
    {
        case EARPGGroupSplineDirection::Reverse: return -1;
        case EARPGGroupSplineDirection::RandomPerGroup: return FMath::RandRange(0, 1) == 0 ? 1 : -1;
        case EARPGGroupSplineDirection::Forward:
        default: return 1;
    }
}

UARPGWandererComponent* AARPGAISpawner::GetOrCreateWanderer(APawn* Pawn) const
{
    if (!IsValid(Pawn)) return nullptr;
    if (UARPGWandererComponent* Existing = Pawn->FindComponentByClass<UARPGWandererComponent>())
        return Existing;

    // Custom APawn classes can still use spawner free-roam without manually adding the component.
    UARPGWandererComponent* Wanderer = NewObject<UARPGWandererComponent>(Pawn, FName(TEXT("ARPGSpawnerFreeRoam")));
    if (!Wanderer) return nullptr;
    Wanderer->bEnabled = false;
    Pawn->AddInstanceComponent(Wanderer);
    Wanderer->RegisterComponent();
    return Wanderer;
}

void AARPGAISpawner::ConfigureFreeRoamPawn(APawn* Pawn, bool bIsLeader)
{
    if (!IsValid(Pawn)) return;
    UARPGWandererComponent* Wanderer = GetOrCreateWanderer(Pawn);
    if (!Wanderer) return;

    Wanderer->ThinkInterval = FMath::Max(0.25f, FreeRoamThinkInterval);
    Wanderer->bPauseDuringCombat = true;
    Wanderer->bStayNearHome = bFreeRoamLeashedToSpawner || (bStayTogether && !bIsLeader);

    if (bStayTogether && !bIsLeader && IsValid(GroupLeader))
    {
        Wanderer->SetHomeLocation(GroupLeader->GetActorLocation());
        Wanderer->WanderRadius = FMath::Max(100.f, FMath::Min(FreeRoamRadius, GroupCohesionRadius * 0.65f));
    }
    else
    {
        Wanderer->SetHomeLocation(GetActorLocation());
        Wanderer->WanderRadius = FMath::Max(100.f, FreeRoamRadius);
    }

    Wanderer->SetWandererEnabled(true);
}

void AARPGAISpawner::ConfigureSpawnedPawn(APawn* Pawn)
{
    if (!IsValid(Pawn)) return;

    if (UARPGAICombatComponent* AICombat = Pawn->FindComponentByClass<UARPGAICombatComponent>())
    {
        AICombat->SetSpawnGroupOwner(this);
        AICombat->HomeLocation = GetActorLocation();
        AICombat->bUseHomeLeash = bStayInRange;
        AICombat->MaxChaseDistanceFromHome = MaxLeashDistance;
    }

    UARPGAISplineComponent* SplineMovement = Pawn->FindComponentByClass<UARPGAISplineComponent>();
    UARPGWandererComponent* Wanderer = Pawn->FindComponentByClass<UARPGWandererComponent>();

    switch (GetEffectiveMovementMode())
    {
        case EARPGSpawnerMovementMode::SplineRoute:
        {
            if (Wanderer) Wanderer->SetWandererEnabled(false);
            if (SplineMovement && AssignedSplineRoute)
            {
                SplineMovement->bStartMovingForward = CurrentGroupSplineDirectionSign > 0;
                SplineMovement->SetRoute(AssignedSplineRoute, bAutoStartAssignedSplineRoute);
            }
            break;
        }
        case EARPGSpawnerMovementMode::FreeRoam:
        {
            if (SplineMovement) SplineMovement->StopRoute(true);
            ConfigureFreeRoamPawn(Pawn, Pawn == GroupLeader);
            break;
        }
        case EARPGSpawnerMovementMode::HoldPosition:
        case EARPGSpawnerMovementMode::Automatic:
        default:
        {
            if (SplineMovement) SplineMovement->StopRoute(true);
            if (Wanderer) Wanderer->SetWandererEnabled(false);
            break;
        }
    }
}

void AARPGAISpawner::ConfigureAllSpawnedPawns()
{
    RefreshGroupLeader();
    for (APawn* Pawn : SpawnedPawns)
        if (IsPawnAlive(Pawn)) ConfigureSpawnedPawn(Pawn);
    RefreshSplineGroupDirectionLeaders();
}

void AARPGAISpawner::RefreshGroupLeader()
{
    if (IsPawnAlive(GroupLeader)) return;
    GroupLeader = nullptr;
    for (APawn* Pawn : SpawnedPawns)
    {
        if (IsPawnAlive(Pawn))
        {
            GroupLeader = Pawn;
            break;
        }
    }
}

void AARPGAISpawner::RefreshSplineGroupDirectionLeaders()
{
    RefreshGroupLeader();
    if (GetEffectiveMovementMode() != EARPGSpawnerMovementMode::SplineRoute) return;

    for (APawn* Pawn : SpawnedPawns)
    {
        if (!IsPawnAlive(Pawn)) continue;
        if (UARPGAISplineComponent* SplineMovement = Pawn->FindComponentByClass<UARPGAISplineComponent>())
        {
            const bool bNeedsLeader = Pawn != GroupLeader && (bSynchronizeSplineGroupDirection || bStayTogether);
            APawn* LeaderToUse = bNeedsLeader ? GroupLeader.Get() : nullptr;
            SplineMovement->SetGroupDirectionLeader(
                LeaderToUse,
                bSynchronizeSplineGroupDirection,
                bStayTogether ? FMath::Max(100.f, GroupCohesionRadius) : 0.f);
        }
    }
}

APawn* AARPGAISpawner::SpawnOne()
{
    if (!HasAuthority() || !GetWorld()) return nullptr;
    TSubclassOf<APawn> Class = ChoosePawnClass();
    if (!Class) return nullptr;

    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
    APawn* Pawn = GetWorld()->SpawnActor<APawn>(Class, ChooseSpawnLocation(), GetActorRotation(), Params);
    if (!Pawn) return nullptr;

    const bool bWasInactive = !bPopulationActive;
    if (bWasInactive)
    {
        bPopulationActive = true;
        OutOfRangeSinceTime = -1.0;
        StartActiveRuntimeTimers();
    }

    Pawn->OnDestroyed.AddDynamic(this, &AARPGAISpawner::HandlePawnDestroyed);
    SpawnedPawns.Add(Pawn);
    RefreshGroupLeader();
    ConfigureSpawnedPawn(Pawn);
    RefreshSplineGroupDirectionLeaders();
    OnSpawnedAI.Broadcast(Pawn);
    if (bWasInactive) OnPopulationActivated.Broadcast();
    return Pawn;
}

void AARPGAISpawner::SetAssignedSplineRoute(AARPGAISplineRoute* NewRoute, bool bApplyToExisting)
{
    if (!HasAuthority()) return;
    AssignedSplineRoute = NewRoute;
    if (bApplyToExisting) ConfigureAllSpawnedPawns();
}

void AARPGAISpawner::SetStayTogether(bool bNewStayTogether)
{
    if (!HasAuthority()) return;
    bStayTogether = bNewStayTogether;
    CohesionRecoveringPawns.Reset();

    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(GroupCohesionTimer);
        if (bStayTogether && (!bEnableDistanceBasedPopulation || bPopulationActive))
        {
            GetWorld()->GetTimerManager().SetTimer(
                GroupCohesionTimer,
                this,
                &AARPGAISpawner::CheckGroupCohesion,
                FMath::Max(0.1f, GroupCohesionCheckInterval),
                true);
        }
    }

    ConfigureAllSpawnedPawns();
}

void AARPGAISpawner::SetMovementMode(EARPGSpawnerMovementMode NewMode, bool bApplyToExisting)
{
    if (!HasAuthority()) return;
    MovementMode = NewMode;
    CohesionRecoveringPawns.Reset();
    if (bApplyToExisting) ConfigureAllSpawnedPawns();
}

void AARPGAISpawner::SpawnGroup()
{
    if (!HasAuthority()) return;

    const bool bWasInactive = !bPopulationActive;
    if (bWasInactive)
    {
        bPopulationActive = true;
        OutOfRangeSinceTime = -1.0;
        StartActiveRuntimeTimers();
    }

    RespawnNotBeforeTime = 0.0;
    bWholeGroupRespawnPending = false;

    if (GetAliveCount() == 0)
    {
        GroupLeader = nullptr;
        CohesionRecoveringPawns.Reset();
        CurrentGroupSplineDirectionSign = ChooseGroupSplineDirectionSign();
    }

    int32 ActiveMinGroupSize = MinGroupSize;
    int32 ActiveMaxGroupSize = MaxGroupSize;
    GetActiveGroupSizeRange(ActiveMinGroupSize, ActiveMaxGroupSize);
    DesiredGroupSize = FMath::RandRange(
        FMath::Min(ActiveMinGroupSize, ActiveMaxGroupSize),
        FMath::Max(ActiveMinGroupSize, ActiveMaxGroupSize));

    for (int32 Index = GetAliveCount(); Index < DesiredGroupSize; ++Index)
        SpawnOne();

    RefreshSplineGroupDirectionLeaders();
    PreservedImmediatePopulationCount = GetAliveCount();
    if (bWasInactive) OnPopulationActivated.Broadcast();
}

void AARPGAISpawner::DespawnAll(bool bDestroyActors)
{
    if (!HasAuthority()) return;

    TArray<TObjectPtr<APawn>> Copy = SpawnedPawns;
    SpawnedPawns.Reset();
    GroupLeader = nullptr;
    CohesionRecoveringPawns.Reset();

    if (bDestroyActors)
    {
        for (APawn* Pawn : Copy)
        {
            if (!IsValid(Pawn)) continue;
            Pawn->OnDestroyed.RemoveDynamic(this, &AARPGAISpawner::HandlePawnDestroyed);
            Pawn->Destroy();
        }
    }
}

void AARPGAISpawner::HandlePawnDestroyed(AActor* DestroyedActor)
{
    APawn* DestroyedPawn = Cast<APawn>(DestroyedActor);
    SpawnedPawns.Remove(DestroyedPawn);
    CohesionRecoveringPawns.Remove(TWeakObjectPtr<APawn>(DestroyedPawn));
    if (GroupLeader == DestroyedPawn) GroupLeader = nullptr;
    RefreshGroupLeader();
    RefreshSplineGroupDirectionLeaders();

    if (!HasAuthority() || !GetWorld()) return;
    if (bEnableDistanceBasedPopulation && !bPopulationActive) return;

    if (GetAliveCount() == 0) OnSpawnGroupDefeated.Broadcast();

    const float SafeRespawnDelay = FMath::Max(0.f, RespawnDelay);
    if (RespawnMode == EARPGRespawnMode::Individual)
    {
        bWholeGroupRespawnPending = false;
        RespawnNotBeforeTime = GetWorld()->GetTimeSeconds() + SafeRespawnDelay;
        GetWorld()->GetTimerManager().SetTimer(RespawnTimer, this, &AARPGAISpawner::ReplenishGroup, SafeRespawnDelay, false);
    }
    else if (RespawnMode == EARPGRespawnMode::WholeGroup && GetAliveCount() == 0)
    {
        bWholeGroupRespawnPending = true;
        RespawnNotBeforeTime = GetWorld()->GetTimeSeconds() + SafeRespawnDelay;
        GetWorld()->GetTimerManager().SetTimer(RespawnTimer, this, &AARPGAISpawner::SpawnGroup, SafeRespawnDelay, false);
    }
}

void AARPGAISpawner::ReplenishGroup()
{
    if (!HasAuthority()) return;
    if (bEnableDistanceBasedPopulation && !bPopulationActive) return;

    SpawnUntilAliveCount(DesiredGroupSize);
    RespawnNotBeforeTime = 0.0;
    bWholeGroupRespawnPending = false;
    PreservedImmediatePopulationCount = GetAliveCount();
    RefreshSplineGroupDirectionLeaders();
}

void AARPGAISpawner::CheckGroupCohesion()
{
    if (!HasAuthority() || !bStayTogether) return;
    RefreshGroupLeader();
    if (!IsPawnAlive(GroupLeader)) return;

    const EARPGSpawnerMovementMode EffectiveMode = GetEffectiveMovementMode();
    if (EffectiveMode == EARPGSpawnerMovementMode::SplineRoute)
    {
        // Spline followers stay independent NavMesh pawns, but mirror one leader's direction.
        // This prevents a group from splitting at route endpoints without issuing competing MoveTo requests.
        RefreshSplineGroupDirectionLeaders();
        return;
    }

    const float CohesionRadius = FMath::Max(100.f, GroupCohesionRadius);
    const float RecoveryRadius = CohesionRadius * FMath::Clamp(GroupRecoveryFraction, 0.1f, 0.95f);
    const FVector LeaderLocation = GroupLeader->GetActorLocation();

    for (APawn* Pawn : SpawnedPawns)
    {
        if (!IsPawnAlive(Pawn) || Pawn == GroupLeader || IsPawnInCombat(Pawn)) continue;

        UARPGWandererComponent* Wanderer = nullptr;
        if (EffectiveMode == EARPGSpawnerMovementMode::FreeRoam)
        {
            Wanderer = GetOrCreateWanderer(Pawn);
            if (Wanderer)
            {
                Wanderer->bStayNearHome = true;
                Wanderer->SetHomeLocation(LeaderLocation);
                Wanderer->WanderRadius = FMath::Max(100.f, FMath::Min(FreeRoamRadius, CohesionRadius * 0.65f));
            }
        }

        const float Distance = FVector::Dist2D(Pawn->GetActorLocation(), LeaderLocation);
        const TWeakObjectPtr<APawn> PawnKey(Pawn);
        const bool bRecovering = CohesionRecoveringPawns.Contains(PawnKey);

        if (Distance > CohesionRadius)
        {
            if (Wanderer && Wanderer->bEnabled) Wanderer->SetWandererEnabled(false);
            if (AAIController* AI = Cast<AAIController>(Pawn->GetController()))
                AI->MoveToLocation(LeaderLocation, RecoveryRadius, true, true, true, false, nullptr, true);
            CohesionRecoveringPawns.Add(PawnKey);
        }
        else if (bRecovering && Distance <= RecoveryRadius)
        {
            CohesionRecoveringPawns.Remove(PawnKey);
            if (Wanderer)
            {
                Wanderer->SetWandererEnabled(true);
                Wanderer->ForceChooseNewDestination();
            }
        }
    }
}

void AARPGAISpawner::CheckLeashes()
{
    if (!HasAuthority()) return;
    CheckDayNightPopulationPhase();

    // Direction synchronization must survive leader death even when physical Stay Together is disabled.
    if (bSynchronizeSplineGroupDirection && GetEffectiveMovementMode() == EARPGSpawnerMovementMode::SplineRoute)
        RefreshSplineGroupDirectionLeaders();

    const FVector Home = GetActorLocation();

    for (APawn* Pawn : SpawnedPawns)
    {
        if (!IsValid(Pawn)) continue;

        if (UARPGCombatComponent* Combat = Pawn->FindComponentByClass<UARPGCombatComponent>())
        {
            if (!Combat->IsAlive())
            {
                if (Pawn->GetLifeSpan() <= 0.f)
                    Pawn->SetLifeSpan(FMath::Max(0.01f, CorpseDespawnDelay));
                continue;
            }
        }

        if (!bStayInRange) continue;

        if (bRouteOverridesSpawnerLeash)
        {
            if (const UARPGAISplineComponent* SplineMovement = Pawn->FindComponentByClass<UARPGAISplineComponent>())
                if (SplineMovement->IsRouteActive()) continue;
        }

        const float Distance = FVector::Dist2D(Home, Pawn->GetActorLocation());
        if (Distance <= MaxLeashDistance) continue;

        if (bTeleportHomeIfOverDoubleRange && Distance > MaxLeashDistance * 2.f)
        {
            Pawn->SetActorLocation(ChooseSpawnLocation(), false, nullptr, ETeleportType::TeleportPhysics);
            if (GetEffectiveMovementMode() == EARPGSpawnerMovementMode::FreeRoam)
                ConfigureFreeRoamPawn(Pawn, Pawn == GroupLeader);
        }
        else if (!IsPawnInCombat(Pawn))
        {
            if (AAIController* AI = Cast<AAIController>(Pawn->GetController()))
            {
                AI->StopMovement();
                AI->MoveToLocation(Home, 100.f);
            }
        }
    }
}
