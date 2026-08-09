#include "Actors/ARPGAISpawner.h"

#include "AIController.h"
#include "Actors/ARPGAISplineRoute.h"
#include "Components/ARPGAICombatComponent.h"
#include "Components/ARPGAISplineComponent.h"
#include "Components/ARPGCombatComponent.h"
#include "Components/ARPGWandererComponent.h"
#include "GameFramework/Pawn.h"
#include "NavigationSystem.h"
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
    if (bSpawnOnBeginPlay) SpawnGroup();

    if (GetWorld())
    {
        // This timer also handles corpse cleanup, so it remains active even when positional leash is disabled.
        GetWorld()->GetTimerManager().SetTimer(
            LeashTimer,
            this,
            &AARPGAISpawner::CheckLeashes,
            FMath::Max(0.1f, LeashCheckInterval),
            true);

        if (bStayTogether)
        {
            GetWorld()->GetTimerManager().SetTimer(
                GroupCohesionTimer,
                this,
                &AARPGAISpawner::CheckGroupCohesion,
                FMath::Max(0.1f, GroupCohesionCheckInterval),
                true);
        }
    }
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
    float Total = 0.f;
    for (const FARPGSpawnEntry& Entry : SpawnTable)
        if (Entry.PawnClass && Entry.Weight > 0.f) Total += Entry.Weight;

    if (Total <= 0.f) return nullptr;

    float Roll = FMath::FRandRange(0.f, Total);
    for (const FARPGSpawnEntry& Entry : SpawnTable)
    {
        if (!Entry.PawnClass || Entry.Weight <= 0.f) continue;
        Roll -= Entry.Weight;
        if (Roll <= 0.f) return Entry.PawnClass;
    }
    return SpawnTable.IsEmpty() ? nullptr : SpawnTable.Last().PawnClass;
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

    Pawn->OnDestroyed.AddDynamic(this, &AARPGAISpawner::HandlePawnDestroyed);
    SpawnedPawns.Add(Pawn);
    RefreshGroupLeader();
    ConfigureSpawnedPawn(Pawn);
    RefreshSplineGroupDirectionLeaders();
    OnSpawnedAI.Broadcast(Pawn);
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
        if (bStayTogether)
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

    if (GetAliveCount() == 0)
    {
        GroupLeader = nullptr;
        CohesionRecoveringPawns.Reset();
        CurrentGroupSplineDirectionSign = ChooseGroupSplineDirectionSign();
    }

    DesiredGroupSize = FMath::RandRange(
        FMath::Min(MinGroupSize, MaxGroupSize),
        FMath::Max(MinGroupSize, MaxGroupSize));

    for (int32 Index = GetAliveCount(); Index < DesiredGroupSize; ++Index)
        SpawnOne();

    RefreshSplineGroupDirectionLeaders();
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
    if (GetAliveCount() == 0) OnSpawnGroupDefeated.Broadcast();

    if (RespawnMode == EARPGRespawnMode::Individual)
    {
        GetWorld()->GetTimerManager().SetTimer(RespawnTimer, this, &AARPGAISpawner::ReplenishGroup, RespawnDelay, false);
    }
    else if (RespawnMode == EARPGRespawnMode::WholeGroup && GetAliveCount() == 0)
    {
        GetWorld()->GetTimerManager().SetTimer(RespawnTimer, this, &AARPGAISpawner::SpawnGroup, RespawnDelay, false);
    }
}

void AARPGAISpawner::ReplenishGroup()
{
    if (!HasAuthority()) return;
    while (GetAliveCount() < DesiredGroupSize)
        if (!SpawnOne()) break;
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
