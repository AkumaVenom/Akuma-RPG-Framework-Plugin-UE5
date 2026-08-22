#include "Subsystems/ARPGNetworkSubsystem.h"

#include "ARPGDeveloperSettings.h"
#include "Actors/ARPGGameMode.h"
#include "AkumasRPGFramework.h"
#include "Engine/Engine.h"
#include "Engine/NetDriver.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

void UARPGNetworkSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    if (const UARPGDeveloperSettings* Settings = GetDefault<UARPGDeveloperSettings>())
    {
        MainMenuMap = Settings->DefaultMainMenuMap;
        GameplayMap = Settings->DefaultGameplayMap;
        GameplayGameMode = Settings->DefaultGameplayGameMode;
    }
    if (GEngine)
    {
        NetworkFailureHandle = GEngine->OnNetworkFailure().AddUObject(this, &UARPGNetworkSubsystem::HandleNetworkFailure);
        TravelFailureHandle = GEngine->OnTravelFailure().AddUObject(this, &UARPGNetworkSubsystem::HandleTravelFailure);
    }
}

void UARPGNetworkSubsystem::Deinitialize()
{
    if (GEngine)
    {
        if (NetworkFailureHandle.IsValid()) GEngine->OnNetworkFailure().Remove(NetworkFailureHandle);
        if (TravelFailureHandle.IsValid()) GEngine->OnTravelFailure().Remove(TravelFailureHandle);
    }
    Super::Deinitialize();
}

void UARPGNetworkSubsystem::SetConnectionState(EARPGNetworkConnectionState NewState, const FText& Message)
{
    ConnectionState = NewState;
    LastNetworkMessage = Message;
    OnNetworkStatus.Broadcast(Message);
    OnConnectionStateChanged.Broadcast(NewState, Message);
}

void UARPGNetworkSubsystem::ConfigureFrontendMaps(FName InMainMenuMap, FName InGameplayMap)
{
    if (!InMainMenuMap.IsNone()) MainMenuMap = InMainMenuMap;
    if (!InGameplayMap.IsNone()) GameplayMap = InGameplayMap;
}

void UARPGNetworkSubsystem::ConfigureGameplayGameMode(TSoftClassPtr<AARPGGameMode> InGameplayGameMode)
{
    if (!InGameplayGameMode.IsNull()) GameplayGameMode = InGameplayGameMode;
}

bool UARPGNetworkSubsystem::BuildGameplayTravelOptions(bool bListen, int32 Port, bool bLAN, FString& OutOptions, FString& OutError)
{
    OutOptions.Reset();
    OutError.Reset();

    if (GameplayGameMode.IsNull())
    {
        OutError = TEXT("Default Gameplay GameMode is not configured. Set Project Settings > Game > Akuma's RPG Framework > Frontend > Default Gameplay GameMode to your ARPGGameMode Blueprint.");
        return false;
    }

    UClass* GameplayClass = GameplayGameMode.LoadSynchronous();
    if (!GameplayClass || !GameplayClass->IsChildOf(AARPGGameMode::StaticClass()))
    {
        OutError = TEXT("Configured Default Gameplay GameMode could not be loaded or does not derive from ARPGGameMode.");
        return false;
    }

    // Unreal's URL game= option has higher precedence than project/map defaults. Force the known gameplay
    // class on every local frontend travel so PIE cannot retain or re-resolve ARPGFrontendGameMode.
    OutOptions = FString::Printf(TEXT("game=%s"), *GameplayClass->GetPathName());
    if (bListen)
    {
        const int32 SafePort = FMath::Clamp(Port, 1, 65535);
        OutOptions += FString::Printf(TEXT("?listen?Port=%d?ARPG_LAN=%d"), SafePort, bLAN ? 1 : 0);
    }
    return true;
}

bool UARPGNetworkSubsystem::StartSinglePlayer(FName MapName)
{
    UWorld* World = GetWorld();
    if (!World || MapName.IsNone()) return false;

    FString Options;
    FString Error;
    if (!BuildGameplayTravelOptions(false, 0, false, Options, Error))
    {
        SetConnectionState(EARPGNetworkConnectionState::Failed, FText::FromString(Error));
        return false;
    }

    GameplayMap = MapName;
    CurrentPlayMode = EARPGFrontendPlayMode::SinglePlayer;
    SetConnectionState(EARPGNetworkConnectionState::OpeningSinglePlayer,
        FText::FromString(FString::Printf(TEXT("Opening %s in single player"), *MapName.ToString())));
    UE_LOG(LogARPG, Log, TEXT("Frontend -> Single Player travel: Map='%s' GameMode='%s' Options='%s'"),
        *MapName.ToString(), *GameplayGameMode.ToSoftObjectPath().ToString(), *Options);
    UGameplayStatics::OpenLevel(World, MapName, true, Options);
    return true;
}

bool UARPGNetworkSubsystem::HostListenServer(FName MapName, int32 Port, bool bLAN)
{
    UWorld* World = GetWorld();
    if (!World || MapName.IsNone()) return false;
    const int32 SafePort = FMath::Clamp(Port, 1, 65535);

    FString Options;
    FString Error;
    if (!BuildGameplayTravelOptions(true, SafePort, bLAN, Options, Error))
    {
        SetConnectionState(EARPGNetworkConnectionState::Failed, FText::FromString(Error));
        return false;
    }

    GameplayMap = MapName;
    CurrentPlayMode = EARPGFrontendPlayMode::ListenHost;
    SetConnectionState(EARPGNetworkConnectionState::StartingListenHost,
        FText::FromString(FString::Printf(TEXT("Hosting %s on port %d"), *MapName.ToString(), SafePort)));
    UE_LOG(LogARPG, Log, TEXT("Frontend -> Listen Host travel: Map='%s' GameMode='%s' Port=%d LAN=%d Options='%s'"),
        *MapName.ToString(), *GameplayGameMode.ToSoftObjectPath().ToString(), SafePort, bLAN ? 1 : 0, *Options);
    UGameplayStatics::OpenLevel(World, MapName, true, Options);
    return true;
}

FString UARPGNetworkSubsystem::NormalizeAddress(FString Address, int32 DefaultPort) const
{
    Address = Address.TrimStartAndEnd();
    if (Address.IsEmpty() || Address.Len() > 128) return FString();
    if (Address.Contains(TEXT("?")) || Address.Contains(TEXT("/")) || Address.Contains(TEXT("\\")) || Address.Contains(TEXT("#")) || Address.Contains(TEXT(" ")))
        return FString();

    const int32 SafePort = FMath::Clamp(DefaultPort, 1, 65535);
    if (Address.StartsWith(TEXT("[")))
    {
        const int32 Close = Address.Find(TEXT("]"));
        if (Close == INDEX_NONE) return FString();
        if (Close == Address.Len() - 1) Address += FString::Printf(TEXT(":%d"), SafePort);
        return Address;
    }

    int32 ColonCount = 0;
    for (const TCHAR C : Address) if (C == TEXT(':')) ++ColonCount;
    if (ColonCount > 1)
        return FString::Printf(TEXT("[%s]:%d"), *Address, SafePort); // raw IPv6 literal

    if (ColonCount == 1)
    {
        FString HostPart, PortPart;
        if (!Address.Split(TEXT(":"), &HostPart, &PortPart, ESearchCase::CaseSensitive, ESearchDir::FromEnd) || HostPart.IsEmpty() || PortPart.IsEmpty()) return FString();
        int32 ParsedPort = 0;
        if (!LexTryParseString(ParsedPort, *PortPart) || ParsedPort < 1 || ParsedPort > 65535) return FString();
        return Address;
    }

    return Address + FString::Printf(TEXT(":%d"), SafePort);
}

bool UARPGNetworkSubsystem::JoinByIP(FString Address, int32 DefaultPort)
{
    UWorld* World = GetWorld();
    if (!World) return false;
    APlayerController* PC = World->GetFirstPlayerController();
    if (!PC) return false;

    const FString Final = NormalizeAddress(Address, DefaultPort);
    if (Final.IsEmpty())
    {
        SetConnectionState(EARPGNetworkConnectionState::Failed, FText::FromString(TEXT("Enter a valid IP address or hostname.")));
        return false;
    }

    LastResolvedAddress = Final;
    CurrentPlayMode = EARPGFrontendPlayMode::DirectIPClient;
    SetConnectionState(EARPGNetworkConnectionState::Connecting, FText::FromString(FString::Printf(TEXT("Connecting to %s"), *Final)));
    PC->ClientTravel(Final, TRAVEL_Absolute);
    return true;
}

void UARPGNetworkSubsystem::DisconnectToMap(FName MapName)
{
    UWorld* World = GetWorld();
    if (!World || MapName.IsNone()) return;
    CurrentPlayMode = EARPGFrontendPlayMode::None;

    // Preserve a concrete rejection/network failure while travelling back to the frontend so the
    // newly-created Main Menu widget can display the reason instead of replacing it with a generic
    // "Returning to main menu" message. Ordinary voluntary disconnects still publish the transition.
    if (ConnectionState != EARPGNetworkConnectionState::Failed)
        SetConnectionState(EARPGNetworkConnectionState::ReturningToMenu, FText::FromString(TEXT("Returning to main menu")));

    UGameplayStatics::OpenLevel(World, MapName, true);
}

void UARPGNetworkSubsystem::ReturnToMainMenu()
{
    if (!MainMenuMap.IsNone()) DisconnectToMap(MainMenuMap);
}

void UARPGNetworkSubsystem::NotifyProfileHandshakeStarted()
{
    SetConnectionState(EARPGNetworkConnectionState::AwaitingProfileHandshake, FText::FromString(TEXT("Synchronizing player profile with host")));
}

void UARPGNetworkSubsystem::NotifyProfileHandshakeAccepted()
{
    SetConnectionState(EARPGNetworkConnectionState::Connected, FText::FromString(TEXT("Profile synchronized. Connected.")));
}

void UARPGNetworkSubsystem::NotifyProfileHandshakeRejected(FText Reason)
{
    SetConnectionState(EARPGNetworkConnectionState::Failed, Reason);
}

void UARPGNetworkSubsystem::HandleNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString)
{
    (void)NetDriver;
    (void)FailureType;
    HandleFailureAndMaybeReturn(World, ErrorString.IsEmpty() ? TEXT("Network connection failed.") : ErrorString);
}

void UARPGNetworkSubsystem::HandleTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& ErrorString)
{
    (void)FailureType;
    HandleFailureAndMaybeReturn(World, ErrorString.IsEmpty() ? TEXT("Travel failed.") : ErrorString);
}

void UARPGNetworkSubsystem::HandleFailureAndMaybeReturn(UWorld* World, const FString& ErrorString)
{
    CurrentPlayMode = EARPGFrontendPlayMode::None;
    SetConnectionState(EARPGNetworkConnectionState::Failed, FText::FromString(ErrorString.Left(256)));
    const UARPGDeveloperSettings* Settings = GetDefault<UARPGDeveloperSettings>();
    if (Settings && Settings->bReturnToMainMenuOnNetworkFailure && World && !MainMenuMap.IsNone())
    {
        UGameplayStatics::OpenLevel(World, MainMenuMap, true);
    }
}
