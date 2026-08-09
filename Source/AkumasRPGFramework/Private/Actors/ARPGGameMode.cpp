#include "Actors/ARPGGameMode.h"
#include "Actors/ARPGGameState.h"
#include "Actors/ARPGPlayerController.h"
#include "Subsystems/ARPGSaveSubsystem.h"
#include "Engine/GameInstance.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"

AARPGGameMode::AARPGGameMode()
{
    GameStateClass = AARPGGameState::StaticClass();
    PlayerControllerClass = AARPGPlayerController::StaticClass();
}
FString AARPGGameMode::ResolveWorldId() const
{
    if(!PersistentWorldId.TrimStartAndEnd().IsEmpty())return PersistentWorldId;
    return GetWorld()?UGameplayStatics::GetCurrentLevelName(this,true):TEXT("DefaultWorld");
}
void AARPGGameMode::BeginPlay()
{
    Super::BeginPlay();
    if(bAutoLoadPersistentWorld && GetWorld())GetWorld()->GetTimerManager().SetTimerForNextTick(this,&AARPGGameMode::HandleAutoLoadWorld);
    if(bAutoSavePersistentWorld && WorldAutoSaveIntervalSeconds>0.f && GetWorld())GetWorld()->GetTimerManager().SetTimer(WorldAutoSaveTimer,this,&AARPGGameMode::HandleAutoSaveWorld,WorldAutoSaveIntervalSeconds,true);
}
void AARPGGameMode::HandleAutoLoadWorld(){ LoadPersistentWorld(); }
void AARPGGameMode::HandleAutoSaveWorld(){ SavePersistentWorld(); }

bool AARPGGameMode::SavePersistentWorld()
{
    UGameInstance* GI=GetWorld()?GetWorld()->GetGameInstance():nullptr; UARPGSaveSubsystem* S=GI?GI->GetSubsystem<UARPGSaveSubsystem>():nullptr; return S&&S->SaveWorld(ResolveWorldId());
}
bool AARPGGameMode::LoadPersistentWorld()
{
    UGameInstance* GI=GetWorld()?GetWorld()->GetGameInstance():nullptr; UARPGSaveSubsystem* S=GI?GI->GetSubsystem<UARPGSaveSubsystem>():nullptr; return S&&S->DoesWorldSaveExist(ResolveWorldId())&&S->LoadWorld(ResolveWorldId());
}
void AARPGGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if(bAutoSavePersistentWorld && EndPlayReason!=EEndPlayReason::Destroyed)SavePersistentWorld();
    Super::EndPlay(EndPlayReason);
}
