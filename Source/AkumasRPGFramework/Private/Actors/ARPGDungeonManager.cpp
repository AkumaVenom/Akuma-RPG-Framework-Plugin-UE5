#include "Actors/ARPGDungeonManager.h"
#include "Data/ARPGDungeonDefinition.h"
#include "Net/UnrealNetwork.h"

AARPGDungeonManager::AARPGDungeonManager()
{
    PrimaryActorTick.bCanEverTick = false; bReplicates = true; bAlwaysRelevant = true;
}

void AARPGDungeonManager::BeginPlay()
{
    Super::BeginPlay(); if (HasAuthority() && Encounters.Num() == 0) InitializeFromDefinition();
}

void AARPGDungeonManager::InitializeFromDefinition()
{
    if (!HasAuthority() || !Definition) return;
    Encounters.Reset();
    for (const FARPGDungeonEncounterDefinition& E : Definition->Encounters) { FARPGEncounterRuntime R; R.EncounterId = E.EncounterId; Encounters.Add(R); }
    bDungeonComplete = false; CurrentCheckpoint = 0;
}

bool AARPGDungeonManager::SetEncounterState(FName EncounterId, EARPGEncounterState NewState)
{
    if (!HasAuthority()) return false;
    for (FARPGEncounterRuntime& E : Encounters)
    {
        if (E.EncounterId == EncounterId)
        {
            E.State = NewState; OnEncounterStateChanged.Broadcast(EncounterId, NewState);
            if (!bDungeonComplete && AreRequiredEncountersComplete()) { bDungeonComplete = true; OnDungeonCompleted.Broadcast(); }
            return true;
        }
    }
    return false;
}

EARPGEncounterState AARPGDungeonManager::GetEncounterState(FName EncounterId) const
{
    for (const FARPGEncounterRuntime& E : Encounters) if (E.EncounterId == EncounterId) return E.State;
    return EARPGEncounterState::NotStarted;
}

void AARPGDungeonManager::RegisterWipe(FName EncounterId)
{
    if (!HasAuthority()) return;
    for (FARPGEncounterRuntime& E : Encounters) if (E.EncounterId == EncounterId) { ++E.WipeCount; E.State = EARPGEncounterState::Failed; OnEncounterStateChanged.Broadcast(EncounterId, E.State); break; }
    OnWipe.Broadcast();
}

void AARPGDungeonManager::UnlockCheckpoint(int32 CheckpointIndex)
{
    if (HasAuthority()) CurrentCheckpoint = FMath::Max(CurrentCheckpoint, CheckpointIndex);
}

bool AARPGDungeonManager::AreRequiredEncountersComplete() const
{
    if (!Definition) return false;
    for (const FARPGDungeonEncounterDefinition& Required : Definition->Encounters)
    {
        if (!Required.bRequiredForCompletion) continue;
        if (GetEncounterState(Required.EncounterId) != EARPGEncounterState::Completed) return false;
    }
    return true;
}

void AARPGDungeonManager::RestoreEncounterProgress(const TArray<FARPGEncounterRuntime>& NewProgress)
{
    if (!HasAuthority()) return; Encounters = NewProgress; bDungeonComplete = AreRequiredEncountersComplete();
}

void AARPGDungeonManager::OnRep_Encounters() { OnEncounterStateChanged.Broadcast(NAME_None, EARPGEncounterState::NotStarted); }
void AARPGDungeonManager::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps); DOREPLIFETIME(AARPGDungeonManager, Encounters); DOREPLIFETIME(AARPGDungeonManager, CurrentCheckpoint); DOREPLIFETIME(AARPGDungeonManager, bDungeonComplete);
}
