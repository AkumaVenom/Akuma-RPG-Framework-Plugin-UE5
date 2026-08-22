#pragma once

#include "CoreMinimal.h"
#include "Actors/ARPGPlayerController.h"
#include "Frontend/ARPGFrontendTypes.h"
#include "ARPGFrontendPlayerController.generated.h"

class UARPGLoginWidget;
class UARPGMainMenuWidget;
class UUserWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FARPGFrontendScreenChanged);

/** PlayerController used by the blank frontend map. No gameplay pawn is required. */
UCLASS(BlueprintType, Blueprintable)
class AKUMASRPGFRAMEWORK_API AARPGFrontendPlayerController : public AARPGPlayerController
{
    GENERATED_BODY()
public:
    AARPGFrontendPlayerController();

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="ARPG|Frontend|UI") TSubclassOf<UARPGLoginWidget> LoginWidgetClass;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="ARPG|Frontend|UI") TSubclassOf<UARPGMainMenuWidget> MainMenuWidgetClass;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ARPG|Frontend|Travel") FName MainMenuMap = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ARPG|Frontend|Travel") FName GameplayMap = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ARPG|Frontend|Network", meta=(ClampMin="1", ClampMax="65535")) int32 DefaultListenPort = 7777;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ARPG|Frontend|Network") bool bDefaultLAN = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ARPG|Frontend|Account") bool bRequireLogin = true;

    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Frontend") TObjectPtr<UUserWidget> ActiveFrontendWidget;
    UPROPERTY(BlueprintAssignable, Category="ARPG|Frontend") FARPGFrontendScreenChanged OnFrontendScreenChanged;

    virtual void BeginPlay() override;

    UFUNCTION(BlueprintCallable, Category="ARPG|Frontend") void ShowLoginScreen();
    UFUNCTION(BlueprintCallable, Category="ARPG|Frontend") void ShowMainMenuScreen();
    UFUNCTION(BlueprintCallable, Category="ARPG|Frontend") bool LoginLocalProfile(const FString& Username, const FString& Password, FText& OutMessage);
    UFUNCTION(BlueprintCallable, Category="ARPG|Frontend") bool CreateLocalProfileAndLogin(const FString& Username, const FString& Password, FText& OutMessage);
    UFUNCTION(BlueprintCallable, Category="ARPG|Frontend") void LogoutToLogin();
    UFUNCTION(BlueprintCallable, Category="ARPG|Frontend") bool StartSinglePlayer();
    UFUNCTION(BlueprintCallable, Category="ARPG|Frontend") bool HostAndPlay(int32 ListenPort, bool bLAN);
    UFUNCTION(BlueprintCallable, Category="ARPG|Frontend") bool JoinDirectIP(const FString& Address, int32 DefaultPort);
    UFUNCTION(BlueprintCallable, Category="ARPG|Frontend") void QuitGame();
    UFUNCTION(BlueprintPure, Category="ARPG|Frontend") FARPGFrontendSessionSettings GetFrontendSessionSettings() const;
    UFUNCTION(BlueprintPure, Category="ARPG|Frontend") FString GetLoggedInUsername() const;

private:
    void ApplyFrontendInputMode();
    void PrepareForGameplayTravel();
    void RemoveActiveFrontendWidget();
    void LoadFrontendDefaultsAndPreferences();
};
