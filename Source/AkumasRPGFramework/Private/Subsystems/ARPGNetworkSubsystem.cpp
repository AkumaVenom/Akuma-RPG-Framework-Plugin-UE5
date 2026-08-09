#include "Subsystems/ARPGNetworkSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"

bool UARPGNetworkSubsystem::HostListenServer(FName MapName, int32 Port, bool bLAN)
{
    if (!GetWorld() || MapName.IsNone()) return false;
    const int32 SafePort = FMath::Clamp(Port, 1, 65535);
    const FString Options = FString::Printf(TEXT("listen?Port=%d?ARPG_LAN=%d"), SafePort, bLAN ? 1 : 0);
    OnNetworkStatus.Broadcast(FText::FromString(FString::Printf(TEXT("Hosting %s on port %d"), *MapName.ToString(), SafePort)));
    UGameplayStatics::OpenLevel(GetWorld(), MapName, true, Options); return true;
}

FString UARPGNetworkSubsystem::NormalizeAddress(FString Address, int32 DefaultPort) const
{
    Address = Address.TrimStartAndEnd();
    if (Address.IsEmpty()) return Address;
    if (!Address.Contains(TEXT(":"))) Address += FString::Printf(TEXT(":%d"), FMath::Clamp(DefaultPort, 1, 65535));
    return Address;
}

bool UARPGNetworkSubsystem::JoinByIP(FString Address, int32 DefaultPort)
{
    if (!GetWorld()) return false;
    APlayerController* PC = GetWorld()->GetFirstPlayerController(); if (!PC) return false;
    const FString Final = NormalizeAddress(Address, DefaultPort); if (Final.IsEmpty()) return false;
    OnNetworkStatus.Broadcast(FText::FromString(FString::Printf(TEXT("Connecting to %s"), *Final)));
    PC->ClientTravel(Final, TRAVEL_Absolute); return true;
}

void UARPGNetworkSubsystem::DisconnectToMap(FName MapName)
{
    if (GetWorld() && !MapName.IsNone()) UGameplayStatics::OpenLevel(GetWorld(), MapName, true);
}
