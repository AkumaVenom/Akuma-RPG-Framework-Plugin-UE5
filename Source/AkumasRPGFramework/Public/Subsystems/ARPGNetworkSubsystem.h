#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Frontend/ARPGFrontendTypes.h"
#include "Engine/EngineBaseTypes.h"
#include "ARPGNetworkSubsystem.generated.h"

class UNetDriver;
class AARPGGameMode;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FARPGNetworkStatus, FText, Message);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FARPGNetworkStateChanged, EARPGNetworkConnectionState, State, FText, Message);

/** Direct-IP/standalone travel coordinator with frontend-safe failure reporting. */
UCLASS()
class AKUMASRPGFRAMEWORK_API UARPGNetworkSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    UPROPERTY(BlueprintAssignable, Category="ARPG|Network") FARPGNetworkStatus OnNetworkStatus;
    UPROPERTY(BlueprintAssignable, Category="ARPG|Network") FARPGNetworkStateChanged OnConnectionStateChanged;

    UPROPERTY(BlueprintReadOnly, Category="ARPG|Network") EARPGFrontendPlayMode CurrentPlayMode = EARPGFrontendPlayMode::None;
    UPROPERTY(BlueprintReadOnly, Category="ARPG|Network") EARPGNetworkConnectionState ConnectionState = EARPGNetworkConnectionState::Idle;
    UPROPERTY(BlueprintReadOnly, Category="ARPG|Network") FString LastResolvedAddress;
    UPROPERTY(BlueprintReadOnly, Category="ARPG|Network") FText LastNetworkMessage;
    UPROPERTY(BlueprintReadOnly, Category="ARPG|Network") FName MainMenuMap = NAME_None;
    UPROPERTY(BlueprintReadOnly, Category="ARPG|Network") FName GameplayMap = NAME_None;
    /** Explicit local gameplay GameMode forced into Single Player / Host travel URLs. */
    UPROPERTY(BlueprintReadOnly, Category="ARPG|Network") TSoftClassPtr<AARPGGameMode> GameplayGameMode;

    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    UFUNCTION(BlueprintCallable, Category="ARPG|Network") bool StartSinglePlayer(FName MapName);
    UFUNCTION(BlueprintCallable, Category="ARPG|Network") bool HostListenServer(FName MapName, int32 Port=7777, bool bLAN=true);
    UFUNCTION(BlueprintCallable, Category="ARPG|Network") bool JoinByIP(FString Address, int32 DefaultPort=7777);
    UFUNCTION(BlueprintCallable, Category="ARPG|Network") void DisconnectToMap(FName MapName);
    UFUNCTION(BlueprintCallable, Category="ARPG|Network") void ReturnToMainMenu();
    UFUNCTION(BlueprintCallable, Category="ARPG|Network") void ConfigureFrontendMaps(FName InMainMenuMap, FName InGameplayMap);
    UFUNCTION(BlueprintCallable, Category="ARPG|Network") void ConfigureGameplayGameMode(TSoftClassPtr<AARPGGameMode> InGameplayGameMode);
    UFUNCTION(BlueprintPure, Category="ARPG|Network") FString NormalizeAddress(FString Address, int32 DefaultPort=7777) const;

    /** Called by the gameplay profile handshake, making connection readiness observable to UI/Blueprint. */
    UFUNCTION(BlueprintCallable, Category="ARPG|Network") void NotifyProfileHandshakeStarted();
    UFUNCTION(BlueprintCallable, Category="ARPG|Network") void NotifyProfileHandshakeAccepted();
    UFUNCTION(BlueprintCallable, Category="ARPG|Network") void NotifyProfileHandshakeRejected(FText Reason);

private:
    FDelegateHandle NetworkFailureHandle;
    FDelegateHandle TravelFailureHandle;
    void SetConnectionState(EARPGNetworkConnectionState NewState, const FText& Message);
    void HandleNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString);
    void HandleTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& ErrorString);
    bool BuildGameplayTravelOptions(bool bListen, int32 Port, bool bLAN, FString& OutOptions, FString& OutError);
    void HandleFailureAndMaybeReturn(UWorld* World, const FString& ErrorString);
};
