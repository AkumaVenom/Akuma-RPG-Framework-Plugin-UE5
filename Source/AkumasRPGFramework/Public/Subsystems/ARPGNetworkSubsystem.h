#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ARPGNetworkSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FARPGNetworkStatus, FText, Message);

UCLASS()
class AKUMASRPGFRAMEWORK_API UARPGNetworkSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    UPROPERTY(BlueprintAssignable) FARPGNetworkStatus OnNetworkStatus;

    UFUNCTION(BlueprintCallable, Category="ARPG|Network") bool HostListenServer(FName MapName, int32 Port=7777, bool bLAN=true);
    UFUNCTION(BlueprintCallable, Category="ARPG|Network") bool JoinByIP(FString Address, int32 DefaultPort=7777);
    UFUNCTION(BlueprintCallable, Category="ARPG|Network") void DisconnectToMap(FName MapName);
    UFUNCTION(BlueprintPure, Category="ARPG|Network") FString NormalizeAddress(FString Address, int32 DefaultPort=7777) const;
};
