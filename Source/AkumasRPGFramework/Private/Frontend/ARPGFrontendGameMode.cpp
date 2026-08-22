#include "Frontend/ARPGFrontendGameMode.h"

#include "AkumasRPGFramework.h"
#include "Frontend/ARPGFrontendPlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "Kismet/GameplayStatics.h"

AARPGFrontendGameMode::AARPGFrontendGameMode()
{
    PlayerControllerClass = AARPGFrontendPlayerController::StaticClass();
    DefaultPawnClass = nullptr;
    bStartPlayersAsSpectators = true;
}

void AARPGFrontendGameMode::BeginPlay()
{
    Super::BeginPlay();
    RecoverDestinationAuthoredGameModeIfNeeded();
}

void AARPGFrontendGameMode::RecoverDestinationAuthoredGameModeIfNeeded()
{
    UWorld* World = GetWorld();
    AWorldSettings* WorldSettings = World ? World->GetWorldSettings() : nullptr;
    UClass* AuthoredGameModeClass = WorldSettings ? WorldSettings->DefaultGameMode.Get() : nullptr;

    // A genuine frontend map either authors this class (or a Blueprint child) or leaves no map override.
    // The recovery below is only for the invalid state observed in UE5.8 PIE where a frontend GameMode
    // survives OpenLevel into a destination map whose own World Settings explicitly author gameplay.
    if (!World || !AuthoredGameModeClass ||
        AuthoredGameModeClass->IsChildOf(AARPGFrontendGameMode::StaticClass()))
    {
        return;
    }

    const FString RecoveryMarker =
        UGameplayStatics::ParseOption(OptionsString, TEXT("ARPG_GameModeRecovery"));

    if (!RecoveryMarker.IsEmpty())
    {
        UE_LOG(LogARPG, Error,
            TEXT("Frontend GameMode recovery already ran, but '%s' is still active on map '%s'. "
                 "Authored destination GameMode is '%s'. Check for a project/PIE URL '?game=' override."),
            *GetClass()->GetPathName(),
            *UGameplayStatics::GetCurrentLevelName(this, true),
            *AuthoredGameModeClass->GetPathName());
        return;
    }

    const FString CurrentMap = UGameplayStatics::GetCurrentLevelName(this, true);
    if (CurrentMap.IsEmpty())
    {
        return;
    }

    FString RecoveryOptions = FString::Printf(
        TEXT("game=%s?ARPG_GameModeRecovery=1"),
        *AuthoredGameModeClass->GetPathName());

    // Preserve only framework-owned listen-host transport options. Do not carry arbitrary URL state
    // (especially an old game= override) into the corrective travel.
    if (OptionsString.Contains(TEXT("listen"), ESearchCase::IgnoreCase))
    {
        RecoveryOptions += TEXT("?listen");
    }
    const FString PortOption = UGameplayStatics::ParseOption(OptionsString, TEXT("Port"));
    if (!PortOption.IsEmpty())
    {
        RecoveryOptions += FString::Printf(TEXT("?Port=%s"), *PortOption);
    }
    const FString LANOption = UGameplayStatics::ParseOption(OptionsString, TEXT("ARPG_LAN"));
    if (!LANOption.IsEmpty())
    {
        RecoveryOptions += FString::Printf(TEXT("?ARPG_LAN=%s"), *LANOption);
    }

    UE_LOG(LogARPG, Warning,
        TEXT("Frontend GameMode '%s' was instantiated on destination map '%s' even though its World Settings "
             "author '%s'. Reopening the destination once with the authored GameMode explicitly forced."),
        *GetClass()->GetPathName(),
        *CurrentMap,
        *AuthoredGameModeClass->GetPathName());

    UGameplayStatics::OpenLevel(this, FName(*CurrentMap), true, RecoveryOptions);
}
