#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Frontend/ARPGFrontendTypes.h"
#include "ARPGFrontendWidgets.generated.h"

class AARPGFrontendPlayerController;
class UButton;
class UCheckBox;
class UEditableTextBox;
class UTextBlock;

UCLASS(BlueprintType, Blueprintable)
class AKUMASRPGFRAMEWORK_API UARPGLoginWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="ARPG|Frontend") void InitializeLoginWidget(AARPGFrontendPlayerController* InController);
    UFUNCTION(BlueprintCallable, Category="ARPG|Frontend") void RefreshLoginWidget();
    UFUNCTION(BlueprintCallable, Category="ARPG|Frontend") void SetStatusMessage(FText Message, bool bIsError=false);
    UFUNCTION(BlueprintImplementableEvent, Category="ARPG|Frontend", meta=(DisplayName="On ARPG Login Screen Refreshed")) void BP_OnLoginScreenRefreshed();
    UFUNCTION(BlueprintImplementableEvent, Category="ARPG|Frontend", meta=(DisplayName="On ARPG Login Status Changed")) void BP_OnLoginStatusChanged(const FText& Message, bool bIsError);

    /** Bind-compatible names for a project UMG subclass. Native fallback creates these automatically. */
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Frontend|Bindings") TObjectPtr<UTextBlock> TitleText;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Frontend|Bindings") TObjectPtr<UTextBlock> SubtitleText;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Frontend|Bindings") TObjectPtr<UEditableTextBox> UsernameInput;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Frontend|Bindings") TObjectPtr<UEditableTextBox> PasswordInput;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Frontend|Bindings") TObjectPtr<UTextBlock> StatusText;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Frontend|Bindings") TObjectPtr<UButton> LoginButton;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Frontend|Bindings") TObjectPtr<UButton> CreateAccountButton;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Frontend|Bindings") TObjectPtr<UButton> QuitButton;
protected:
    virtual void NativeOnInitialized() override;
private:
    UPROPERTY(Transient) TObjectPtr<AARPGFrontendPlayerController> FrontendController;
    UFUNCTION() void HandleLogin();
    UFUNCTION() void HandleCreateAccount();
    UFUNCTION() void HandleQuit();
    void EnsureNativeLayoutOrBindings();
};

UCLASS(BlueprintType, Blueprintable)
class AKUMASRPGFRAMEWORK_API UARPGMainMenuWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="ARPG|Frontend") void InitializeMainMenuWidget(AARPGFrontendPlayerController* InController);
    UFUNCTION(BlueprintCallable, Category="ARPG|Frontend") void RefreshMainMenuWidget();
    UFUNCTION(BlueprintCallable, Category="ARPG|Frontend") void SetStatusMessage(FText Message, bool bIsError=false);
    UFUNCTION(BlueprintImplementableEvent, Category="ARPG|Frontend", meta=(DisplayName="On ARPG Main Menu Refreshed")) void BP_OnMainMenuRefreshed(const FString& Username, FARPGFrontendSessionSettings Settings);
    UFUNCTION(BlueprintImplementableEvent, Category="ARPG|Frontend", meta=(DisplayName="On ARPG Main Menu Status Changed")) void BP_OnMainMenuStatusChanged(const FText& Message, bool bIsError);

    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Frontend|Bindings") TObjectPtr<UTextBlock> TitleText;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Frontend|Bindings") TObjectPtr<UTextBlock> AccountText;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Frontend|Bindings") TObjectPtr<UTextBlock> StatusText;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Frontend|Bindings") TObjectPtr<UButton> SinglePlayerButton;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Frontend|Bindings") TObjectPtr<UButton> HostAndPlayButton;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Frontend|Bindings") TObjectPtr<UEditableTextBox> ListenPortInput;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Frontend|Bindings") TObjectPtr<UCheckBox> LANCheckBox;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Frontend|Bindings") TObjectPtr<UEditableTextBox> JoinAddressInput;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Frontend|Bindings") TObjectPtr<UButton> JoinByIPButton;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Frontend|Bindings") TObjectPtr<UButton> LogoutButton;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Frontend|Bindings") TObjectPtr<UButton> QuitButton;
protected:
    virtual void NativeOnInitialized() override;
private:
    UPROPERTY(Transient) TObjectPtr<AARPGFrontendPlayerController> FrontendController;
    UFUNCTION() void HandleSinglePlayer();
    UFUNCTION() void HandleHostAndPlay();
    UFUNCTION() void HandleJoinByIP();
    UFUNCTION() void HandleLogout();
    UFUNCTION() void HandleQuit();
    void EnsureNativeLayoutOrBindings();
    int32 ReadListenPort() const;
};
