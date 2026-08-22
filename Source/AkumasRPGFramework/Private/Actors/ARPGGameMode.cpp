#include "Actors/ARPGGameMode.h"

#include "ARPGDeveloperSettings.h"
#include "Actors/ARPGGameState.h"
#include "Actors/ARPGPlayerController.h"
#include "Subsystems/ARPGSaveSubsystem.h"
#include "Engine/GameInstance.h"
#include "GameFramework/GameSession.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"

AARPGGameMode::AARPGGameMode()
{
    GameStateClass = AARPGGameState::StaticClass();
    PlayerControllerClass = AARPGPlayerController::StaticClass();
    if (const UARPGDeveloperSettings* Settings = GetDefault<UARPGDeveloperSettings>())
        bRequireProfileIdentityBeforeSpawn = Settings->bRequireLocalProfileForGameplay;
}

void AARPGGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
    Super::InitGame(MapName, Options, ErrorMessage);
    if (GameSession)
        if (const UARPGDeveloperSettings* Settings = GetDefault<UARPGDeveloperSettings>())
            GameSession->MaxPlayers = FMath::Max(1, Settings->MaxPlayers);
}

FString AARPGGameMode::ResolveWorldId() const
{
    if (!PersistentWorldId.TrimStartAndEnd().IsEmpty()) return PersistentWorldId;
    return GetWorld() ? UGameplayStatics::GetCurrentLevelName(this, true) : TEXT("DefaultWorld");
}

void AARPGGameMode::BeginPlay()
{
    Super::BeginPlay();
    InitializeWorldPersistenceContext();
    if (bAutoLoadPersistentWorld && GetWorld()) GetWorld()->GetTimerManager().SetTimerForNextTick(this, &AARPGGameMode::HandleAutoLoadWorld);
    if (bAutoSavePersistentWorld && WorldAutoSaveIntervalSeconds > 0.f && GetWorld())
        GetWorld()->GetTimerManager().SetTimer(WorldAutoSaveTimer, this, &AARPGGameMode::HandleAutoSaveWorld, WorldAutoSaveIntervalSeconds, true);
}

void AARPGGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
    AARPGPlayerController* RPGPC = Cast<AARPGPlayerController>(NewPlayer);
    if (!bRequireProfileIdentityBeforeSpawn || !RPGPC)
    {
        Super::HandleStartingNewPlayer_Implementation(NewPlayer);
        return;
    }

    if (!RPGPC->IsProfileIdentityAccepted() && RPGPC->IsLocalController())
        RPGPC->InitializeAuthorityProfileIdentityFromLocalAccount();

    if (RPGPC->IsProfileIdentityAccepted())
    {
        if (!RPGPC->GetPawn()) Super::HandleStartingNewPlayer_Implementation(NewPlayer);
        return;
    }

    // Do not spawn an unbound pawn. Remote identity arrives through the owned PlayerController RPC; once
    // accepted NotifyProfileIdentityAccepted resumes the normal spawn path. This keeps character BeginPlay
    // and Persistence from ever observing the listen host's account as a fallback identity.
    const UARPGDeveloperSettings* Settings = GetDefault<UARPGDeveloperSettings>();
    RPGPC->BeginProfileIdentityHandshake(Settings ? Settings->ProfileHandshakeTimeoutSeconds : 15.f);
}

void AARPGGameMode::NotifyProfileIdentityAccepted(AARPGPlayerController* PlayerController)
{
    if (!HasAuthority() || !PlayerController || !PlayerController->IsProfileIdentityAccepted()) return;
    if (PlayerController->GetPawn()) return;
    Super::HandleStartingNewPlayer_Implementation(PlayerController);
}

void AARPGGameMode::InitializeWorldPersistenceContext()
{
    UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
    UARPGSaveSubsystem* Save = GI ? GI->GetSubsystem<UARPGSaveSubsystem>() : nullptr;
    if (!Save)
    {
        ActiveWorldSaveWorldId.Reset();
        ActiveWorldSaveAccountId.Invalidate();
        ActiveWorldSaveSlotName.Reset();
        return;
    }

    // Capture once. A listen host owns one authoritative shared world even while remote accounts join/leave,
    // and a later frontend/logout transition must never redirect this world's final EndPlay save into Guest.
    ActiveWorldSaveWorldId = ResolveWorldId();
    ActiveWorldSaveAccountId = Save->ResolveWorldSaveAccountId();
    ActiveWorldSaveSlotName = Save->MakeWorldSlotNameForAccount(ActiveWorldSaveAccountId, ActiveWorldSaveWorldId);
}

void AARPGGameMode::HandleAutoLoadWorld() { LoadPersistentWorld(); }
void AARPGGameMode::HandleAutoSaveWorld() { SavePersistentWorld(); }

bool AARPGGameMode::SavePersistentWorld()
{
    UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
    UARPGSaveSubsystem* S = GI ? GI->GetSubsystem<UARPGSaveSubsystem>() : nullptr;
    return S && S->SaveWorldForAccount(ActiveWorldSaveAccountId, ActiveWorldSaveWorldId.IsEmpty() ? ResolveWorldId() : ActiveWorldSaveWorldId, ActiveWorldSaveSlotName);
}

bool AARPGGameMode::LoadPersistentWorld()
{
    UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
    UARPGSaveSubsystem* S = GI ? GI->GetSubsystem<UARPGSaveSubsystem>() : nullptr;
    return S && S->DoesWorldSaveExistForAccount(ActiveWorldSaveAccountId, ActiveWorldSaveWorldId.IsEmpty() ? ResolveWorldId() : ActiveWorldSaveWorldId) && S->LoadWorldForAccount(ActiveWorldSaveAccountId, ActiveWorldSaveWorldId.IsEmpty() ? ResolveWorldId() : ActiveWorldSaveWorldId, ActiveWorldSaveSlotName);
}

void AARPGGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (bAutoSavePersistentWorld && EndPlayReason != EEndPlayReason::Destroyed) SavePersistentWorld();
    Super::EndPlay(EndPlayReason);
}
