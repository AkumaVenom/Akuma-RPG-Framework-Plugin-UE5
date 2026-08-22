#include "Frontend/ARPGFrontendPlayerController.h"

#include "ARPGDeveloperSettings.h"
#include "Frontend/ARPGFrontendWidgets.h"
#include "Subsystems/ARPGAccountSubsystem.h"
#include "Subsystems/ARPGNetworkSubsystem.h"
#include "Blueprint/UserWidget.h"
#include "Engine/GameInstance.h"
#include "Kismet/KismetSystemLibrary.h"

AARPGFrontendPlayerController::AARPGFrontendPlayerController()
{
    bShowMouseCursor = true;
    LoginWidgetClass = UARPGLoginWidget::StaticClass();
    MainMenuWidgetClass = UARPGMainMenuWidget::StaticClass();
}

void AARPGFrontendPlayerController::BeginPlay()
{
    Super::BeginPlay();
    if (!IsLocalController()) return;
    LoadFrontendDefaultsAndPreferences();
    ApplyFrontendInputMode();

    UARPGAccountSubsystem* Accounts = GetGameInstance() ? GetGameInstance()->GetSubsystem<UARPGAccountSubsystem>() : nullptr;
    if (!bRequireLogin || (Accounts && Accounts->IsLoggedIn())) ShowMainMenuScreen();
    else ShowLoginScreen();
}

void AARPGFrontendPlayerController::LoadFrontendDefaultsAndPreferences()
{
    if (const UARPGDeveloperSettings* Settings = GetDefault<UARPGDeveloperSettings>())
    {
        if (MainMenuMap.IsNone()) MainMenuMap = Settings->DefaultMainMenuMap;
        if (GameplayMap.IsNone()) GameplayMap = Settings->DefaultGameplayMap;
        DefaultListenPort = FMath::Clamp(Settings->DefaultListenPort, 1, 65535);
        bDefaultLAN = Settings->bDefaultLANListenServer;
    }

    if (UARPGAccountSubsystem* Accounts = GetGameInstance() ? GetGameInstance()->GetSubsystem<UARPGAccountSubsystem>() : nullptr)
    {
        if (UARPGAccountProfileSave* Profile = Accounts->LoadCurrentAccountProfile())
        {
            DefaultListenPort = FMath::Clamp(Profile->LastListenPort, 1, 65535);
            bDefaultLAN = Profile->bLastLAN;
            if (GameplayMap.IsNone() && !Profile->LastGameplayMap.IsNone()) GameplayMap = Profile->LastGameplayMap;
        }
    }

    if (UARPGNetworkSubsystem* Network = GetGameInstance() ? GetGameInstance()->GetSubsystem<UARPGNetworkSubsystem>() : nullptr)
    {
        Network->ConfigureFrontendMaps(MainMenuMap, GameplayMap);
        if (const UARPGDeveloperSettings* Settings = GetDefault<UARPGDeveloperSettings>())
            Network->ConfigureGameplayGameMode(Settings->DefaultGameplayGameMode);
    }
}

void AARPGFrontendPlayerController::ApplyFrontendInputMode()
{
    bShowMouseCursor = true;
    FInputModeUIOnly Mode;
    if (ActiveFrontendWidget) Mode.SetWidgetToFocus(ActiveFrontendWidget->TakeWidget());
    SetInputMode(Mode);
}

void AARPGFrontendPlayerController::RemoveActiveFrontendWidget()
{
    if (ActiveFrontendWidget)
    {
        ActiveFrontendWidget->RemoveFromParent();
        ActiveFrontendWidget = nullptr;
    }
}

void AARPGFrontendPlayerController::PrepareForGameplayTravel()
{
    // Frontend uses UIOnly input. Explicitly restore gameplay input before travel because viewport/input
    // state can survive long enough across local travel to leave the newly possessed pawn unable to move/look.
    RemoveActiveFrontendWidget();
    FInputModeGameOnly Mode;
    SetInputMode(Mode);
    bShowMouseCursor = false;
    SetIgnoreMoveInput(false);
    SetIgnoreLookInput(false);
}

void AARPGFrontendPlayerController::ShowLoginScreen()
{
    if (!IsLocalController()) return;
    RemoveActiveFrontendWidget();
    TSubclassOf<UARPGLoginWidget> UseClass = LoginWidgetClass;
    if (!UseClass) UseClass = UARPGLoginWidget::StaticClass();
    UARPGLoginWidget* Widget = CreateWidget<UARPGLoginWidget>(this, UseClass);
    if (!Widget) return;
    ActiveFrontendWidget = Widget;
    Widget->AddToViewport(1000);
    Widget->InitializeLoginWidget(this);
    ApplyFrontendInputMode();
    OnFrontendScreenChanged.Broadcast();
}

void AARPGFrontendPlayerController::ShowMainMenuScreen()
{
    if (!IsLocalController()) return;
    LoadFrontendDefaultsAndPreferences();
    RemoveActiveFrontendWidget();
    TSubclassOf<UARPGMainMenuWidget> UseClass = MainMenuWidgetClass;
    if (!UseClass) UseClass = UARPGMainMenuWidget::StaticClass();
    UARPGMainMenuWidget* Widget = CreateWidget<UARPGMainMenuWidget>(this, UseClass);
    if (!Widget) return;
    ActiveFrontendWidget = Widget;
    Widget->AddToViewport(1000);
    Widget->InitializeMainMenuWidget(this);
    ApplyFrontendInputMode();
    OnFrontendScreenChanged.Broadcast();
}

bool AARPGFrontendPlayerController::LoginLocalProfile(const FString& Username, const FString& Password, FText& OutMessage)
{
    UARPGAccountSubsystem* Accounts = GetGameInstance() ? GetGameInstance()->GetSubsystem<UARPGAccountSubsystem>() : nullptr;
    if (!Accounts)
    {
        OutMessage = FText::FromString(TEXT("Account subsystem is unavailable."));
        return false;
    }
    const bool bSuccess = Accounts->LoginLocalAccount(Username, Password, OutMessage);
    if (bSuccess) ShowMainMenuScreen();
    return bSuccess;
}

bool AARPGFrontendPlayerController::CreateLocalProfileAndLogin(const FString& Username, const FString& Password, FText& OutMessage)
{
    UARPGAccountSubsystem* Accounts = GetGameInstance() ? GetGameInstance()->GetSubsystem<UARPGAccountSubsystem>() : nullptr;
    if (!Accounts)
    {
        OutMessage = FText::FromString(TEXT("Account subsystem is unavailable."));
        return false;
    }
    const bool bSuccess = Accounts->CreateAndLoginLocalAccount(Username, Password, OutMessage);
    if (bSuccess) ShowMainMenuScreen();
    return bSuccess;
}

void AARPGFrontendPlayerController::LogoutToLogin()
{
    if (UARPGAccountSubsystem* Accounts = GetGameInstance() ? GetGameInstance()->GetSubsystem<UARPGAccountSubsystem>() : nullptr)
        Accounts->Logout();
    ShowLoginScreen();
}

bool AARPGFrontendPlayerController::StartSinglePlayer()
{
    if (GameplayMap.IsNone()) return false;
    UGameInstance* GI = GetGameInstance();
    UARPGAccountSubsystem* Accounts = GI ? GI->GetSubsystem<UARPGAccountSubsystem>() : nullptr;
    UARPGNetworkSubsystem* Network = GI ? GI->GetSubsystem<UARPGNetworkSubsystem>() : nullptr;
    if ((bRequireLogin && (!Accounts || !Accounts->IsLoggedIn())) || !Network) return false;
    if (Accounts && !Accounts->GetOrCreateLastCharacterId().IsValid()) return false;
    if (Accounts) Accounts->SaveCurrentFrontendPreferences(TEXT(""), DefaultListenPort, bDefaultLAN, GameplayMap);
    PrepareForGameplayTravel();
    return Network->StartSinglePlayer(GameplayMap);
}

bool AARPGFrontendPlayerController::HostAndPlay(int32 ListenPort, bool bLAN)
{
    if (GameplayMap.IsNone()) return false;
    UGameInstance* GI = GetGameInstance();
    UARPGAccountSubsystem* Accounts = GI ? GI->GetSubsystem<UARPGAccountSubsystem>() : nullptr;
    UARPGNetworkSubsystem* Network = GI ? GI->GetSubsystem<UARPGNetworkSubsystem>() : nullptr;
    if ((bRequireLogin && (!Accounts || !Accounts->IsLoggedIn())) || !Network) return false;
    if (Accounts && !Accounts->GetOrCreateLastCharacterId().IsValid()) return false;
    DefaultListenPort = FMath::Clamp(ListenPort, 1, 65535);
    bDefaultLAN = bLAN;
    if (Accounts) Accounts->SaveCurrentFrontendPreferences(TEXT(""), DefaultListenPort, bDefaultLAN, GameplayMap);
    PrepareForGameplayTravel();
    return Network->HostListenServer(GameplayMap, DefaultListenPort, bDefaultLAN);
}

bool AARPGFrontendPlayerController::JoinDirectIP(const FString& Address, int32 DefaultPort)
{
    UGameInstance* GI = GetGameInstance();
    UARPGAccountSubsystem* Accounts = GI ? GI->GetSubsystem<UARPGAccountSubsystem>() : nullptr;
    UARPGNetworkSubsystem* Network = GI ? GI->GetSubsystem<UARPGNetworkSubsystem>() : nullptr;
    if ((bRequireLogin && (!Accounts || !Accounts->IsLoggedIn())) || !Network) return false;
    if (Accounts && !Accounts->GetOrCreateLastCharacterId().IsValid()) return false;
    DefaultListenPort = FMath::Clamp(DefaultPort, 1, 65535);
    const FString Normalized = Network->NormalizeAddress(Address, DefaultListenPort);
    if (Normalized.IsEmpty()) return false;
    if (Accounts) Accounts->SaveCurrentFrontendPreferences(Normalized, DefaultListenPort, bDefaultLAN, GameplayMap);
    PrepareForGameplayTravel();
    return Network->JoinByIP(Normalized, DefaultListenPort);
}

void AARPGFrontendPlayerController::QuitGame()
{
    UKismetSystemLibrary::QuitGame(this, this, EQuitPreference::Quit, false);
}

FARPGFrontendSessionSettings AARPGFrontendPlayerController::GetFrontendSessionSettings() const
{
    FARPGFrontendSessionSettings Result;
    Result.GameplayMap = GameplayMap;
    Result.MainMenuMap = MainMenuMap;
    Result.ListenPort = DefaultListenPort;
    Result.bLAN = bDefaultLAN;
    if (const UARPGAccountSubsystem* Accounts = GetGameInstance() ? GetGameInstance()->GetSubsystem<UARPGAccountSubsystem>() : nullptr)
        if (UARPGAccountProfileSave* Profile = Accounts->LoadCurrentAccountProfile())
            Result.JoinAddress = Profile->LastJoinAddress;
    return Result;
}

FString AARPGFrontendPlayerController::GetLoggedInUsername() const
{
    const UARPGAccountSubsystem* Accounts = GetGameInstance() ? GetGameInstance()->GetSubsystem<UARPGAccountSubsystem>() : nullptr;
    return Accounts && Accounts->IsLoggedIn() ? Accounts->CurrentUsername : FString();
}
