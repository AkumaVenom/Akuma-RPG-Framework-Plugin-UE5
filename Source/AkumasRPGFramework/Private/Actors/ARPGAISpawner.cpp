#include "Actors/ARPGAISpawner.h"
#include "AIController.h"
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
    if (bSpawnOnBeginPlay) SpawnGroup();
    if (GetWorld()) GetWorld()->GetTimerManager().SetTimer(LeashTimer, this, &AARPGAISpawner::CheckLeashes, LeashCheckInterval, true);
}

int32 AARPGAISpawner::GetAliveCount() const
{
    int32 Count = 0; for (APawn* Pawn : SpawnedPawns) if (IsValid(Pawn)) ++Count; return Count;
}

TSubclassOf<APawn> AARPGAISpawner::ChoosePawnClass() const
{
    float Total = 0.f; for (const FARPGSpawnEntry& E : SpawnTable) if (E.PawnClass && E.Weight > 0.f) Total += E.Weight;
    if (Total <= 0.f) return nullptr;
    float Roll = FMath::FRandRange(0.f, Total);
    for (const FARPGSpawnEntry& E : SpawnTable) if (E.PawnClass && E.Weight > 0.f) { Roll -= E.Weight; if (Roll <= 0.f) return E.PawnClass; }
    return SpawnTable.Last().PawnClass;
}

FVector AARPGAISpawner::ChooseSpawnLocation() const
{
    const FVector Origin = GetActorLocation();
    if (SpawnShape == EARPGSpawnerShape::Point) return Origin;
    FVector Candidate = Origin;
    if (SpawnShape == EARPGSpawnerShape::Circle)
    {
        const FVector2D Offset = FMath::RandPointInCircle(SpawnRadius); Candidate += FVector(Offset.X, Offset.Y, 0.f);
    }
    else
    {
        Candidate += FVector(FMath::FRandRange(-BoxExtents.X, BoxExtents.X), FMath::FRandRange(-BoxExtents.Y, BoxExtents.Y), FMath::FRandRange(-BoxExtents.Z, BoxExtents.Z));
    }
    if (UNavigationSystemV1* Nav = GetWorld() ? FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()) : nullptr)
    {
        FNavLocation Projected; if (Nav->ProjectPointToNavigation(Candidate, Projected, FVector(300.f,300.f,1000.f))) return Projected.Location;
    }
    return Candidate;
}

APawn* AARPGAISpawner::SpawnOne()
{
    if (!HasAuthority() || !GetWorld()) return nullptr;
    TSubclassOf<APawn> Class = ChoosePawnClass(); if (!Class) return nullptr;
    FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
    APawn* Pawn = GetWorld()->SpawnActor<APawn>(Class, ChooseSpawnLocation(), GetActorRotation(), Params);
    if (Pawn)
    {
        Pawn->OnDestroyed.AddDynamic(this, &AARPGAISpawner::HandlePawnDestroyed); SpawnedPawns.Add(Pawn); OnSpawnedAI.Broadcast(Pawn);
    }
    return Pawn;
}

void AARPGAISpawner::SpawnGroup()
{
    if (!HasAuthority()) return;
    DesiredGroupSize = FMath::RandRange(FMath::Min(MinGroupSize, MaxGroupSize), FMath::Max(MinGroupSize, MaxGroupSize));
    for (int32 i = GetAliveCount(); i < DesiredGroupSize; ++i) SpawnOne();
}

void AARPGAISpawner::DespawnAll(bool bDestroyActors)
{
    if (!HasAuthority()) return;
    TArray<TObjectPtr<APawn>> Copy = SpawnedPawns; SpawnedPawns.Reset();
    if (bDestroyActors) for (APawn* Pawn : Copy) if (IsValid(Pawn)) { Pawn->OnDestroyed.RemoveDynamic(this, &AARPGAISpawner::HandlePawnDestroyed); Pawn->Destroy(); }
}

void AARPGAISpawner::HandlePawnDestroyed(AActor* DestroyedActor)
{
    SpawnedPawns.Remove(Cast<APawn>(DestroyedActor));
    if (!HasAuthority() || !GetWorld()) return;
    if (GetAliveCount() == 0) OnSpawnGroupDefeated.Broadcast();
    if (RespawnMode == EARPGRespawnMode::Individual)
        GetWorld()->GetTimerManager().SetTimer(RespawnTimer, this, &AARPGAISpawner::ReplenishGroup, RespawnDelay, false);
    else if (RespawnMode == EARPGRespawnMode::WholeGroup && GetAliveCount() == 0)
        GetWorld()->GetTimerManager().SetTimer(RespawnTimer, this, &AARPGAISpawner::SpawnGroup, RespawnDelay, false);
}

void AARPGAISpawner::ReplenishGroup()
{
    if (!HasAuthority()) return;
    while (GetAliveCount() < DesiredGroupSize) if (!SpawnOne()) break;
}

void AARPGAISpawner::CheckLeashes()
{
    if (!HasAuthority() || !bStayInRange) return;
    const FVector Home = GetActorLocation();
    for (APawn* Pawn : SpawnedPawns)
    {
        if (!IsValid(Pawn)) continue;
        const float Dist = FVector::Dist2D(Home, Pawn->GetActorLocation());
        if (Dist <= MaxLeashDistance) continue;
        if (bTeleportHomeIfOverDoubleRange && Dist > MaxLeashDistance * 2.f) Pawn->SetActorLocation(ChooseSpawnLocation(), false, nullptr, ETeleportType::TeleportPhysics);
        else if (AAIController* AI = Cast<AAIController>(Pawn->GetController())) { AI->StopMovement(); AI->MoveToLocation(Home, 100.f); }
    }
}
