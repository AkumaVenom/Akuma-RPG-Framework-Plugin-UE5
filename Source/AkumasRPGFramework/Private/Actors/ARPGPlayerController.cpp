#include "Actors/ARPGPlayerController.h"

#include "Actors/ARPGGameMode.h"
#include "Actors/ARPGGameState.h"
#include "Frontend/ARPGFrontendTypes.h"
#include "Subsystems/ARPGAccountSubsystem.h"
#include "Subsystems/ARPGNetworkSubsystem.h"
#include "Net/UnrealNetwork.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerState.h"
#include "TimerManager.h"

void AARPGPlayerController::BeginPlay()
{
    Super::BeginPlay();
    if (!IsLocalController()) return;

    UGameInstance* GI = GetGameInstance();
    UARPGNetworkSubsystem* Network = GI ? GI->GetSubsystem<UARPGNetworkSubsystem>() : nullptr;
    const bool bFrontendTravelSession = Network && Network->CurrentPlayMode != EARPGFrontendPlayMode::None;
    const bool bGameplayAuthority = HasAuthority() && GetWorld() && GetWorld()->GetAuthGameMode<AARPGGameMode>() != nullptr;
    if (!bFrontendTravelSession && !bGameplayAuthority) return;

    ApplyGameplayInputModeIfNeeded();

    if (HasAuthority())
    {
        InitializeAuthorityProfileIdentityFromLocalAccount();
    }
    else
    {
        if (Network) Network->NotifyProfileHandshakeStarted();
        SubmitLocalProfileIdentity();
    }
}


void AARPGPlayerController::ApplyGameplayInputModeIfNeeded()
{
    if (!bRestoreGameplayInputOnBeginPlay || !IsLocalController()) return;

    UARPGNetworkSubsystem* Network = GetGameInstance() ? GetGameInstance()->GetSubsystem<UARPGNetworkSubsystem>() : nullptr;
    const bool bGameplayAuthority = HasAuthority() && GetWorld() && GetWorld()->GetAuthGameMode<AARPGGameMode>() != nullptr;
    const bool bFrontendTravelSession = Network && Network->CurrentPlayMode != EARPGFrontendPlayMode::None;
    if (!bGameplayAuthority && !bFrontendTravelSession) return;

    FInputModeGameOnly Mode;
    SetInputMode(Mode);
    bShowMouseCursor = false;
    SetIgnoreMoveInput(false);
    SetIgnoreLookInput(false);
}

FString AARPGPlayerController::SanitizeProfileUsername(const FString& InUsername) const
{
    FString Result;
    const FString Trimmed = InUsername.TrimStartAndEnd().Left(32);
    Result.Reserve(Trimmed.Len());
    for (const TCHAR C : Trimmed)
    {
        if (FChar::IsAlnum(C) || C == TEXT('_') || C == TEXT('-') || C == TEXT('.') || C == TEXT(' ')) Result.AppendChar(C);
    }
    return Result.TrimStartAndEnd();
}

bool AARPGPlayerController::SubmitLocalProfileIdentity()
{
    if (!IsLocalController()) return false;
    UARPGAccountSubsystem* Accounts = GetGameInstance() ? GetGameInstance()->GetSubsystem<UARPGAccountSubsystem>() : nullptr;
    if (!Accounts || !Accounts->IsLoggedIn() || !Accounts->CurrentAccountId.IsValid())
    {
        const FText Message = FText::FromString(TEXT("A logged-in local profile is required before entering gameplay."));
        OnProfileIdentityResult.Broadcast(false, Message, FGuid(), FGuid());
        if (UARPGNetworkSubsystem* Network = GetGameInstance() ? GetGameInstance()->GetSubsystem<UARPGNetworkSubsystem>() : nullptr)
            Network->NotifyProfileHandshakeRejected(Message);
        return false;
    }

    const FGuid RequestedCharacterId = Accounts->GetOrCreateLastCharacterId();
    if (!RequestedCharacterId.IsValid())
    {
        const FText Message = FText::FromString(TEXT("A stable character identity could not be created for this profile."));
        OnProfileIdentityResult.Broadcast(false, Message, FGuid(), FGuid());
        return false;
    }
    if (HasAuthority())
    {
        FText Message;
        const bool bAccepted = AcceptProfileIdentityOnAuthority(Accounts->CurrentAccountId, Accounts->CurrentUsername, RequestedCharacterId, Message);
        ClientProfileIdentityResult_Implementation(bAccepted, Message, AccountId, ProfileCharacterId);
        return bAccepted;
    }

    ServerSubmitProfileIdentity(Accounts->CurrentAccountId, Accounts->CurrentUsername, RequestedCharacterId);
    return true;
}

bool AARPGPlayerController::InitializeAuthorityProfileIdentityFromLocalAccount()
{
    if (!HasAuthority() || !IsLocalController()) return bProfileIdentityAccepted;
    UARPGAccountSubsystem* Accounts = GetGameInstance() ? GetGameInstance()->GetSubsystem<UARPGAccountSubsystem>() : nullptr;
    if (!Accounts || !Accounts->IsLoggedIn() || !Accounts->CurrentAccountId.IsValid()) return false;

    const FGuid StableCharacterId = Accounts->GetOrCreateLastCharacterId();
    if (!StableCharacterId.IsValid()) return false;

    FText Message;
    const bool bAccepted = AcceptProfileIdentityOnAuthority(Accounts->CurrentAccountId, Accounts->CurrentUsername, StableCharacterId, Message);
    if (bAccepted && Accounts->IsLoggedIn()) Accounts->RegisterCharacterId(ProfileCharacterId);
    if (bAccepted)
        if (UARPGNetworkSubsystem* Network = GetGameInstance() ? GetGameInstance()->GetSubsystem<UARPGNetworkSubsystem>() : nullptr)
            Network->NotifyProfileHandshakeAccepted();
    OnProfileIdentityResult.Broadcast(bAccepted, Message, AccountId, ProfileCharacterId);
    return bAccepted;
}

void AARPGPlayerController::BeginProfileIdentityHandshake(float TimeoutSeconds)
{
    if (!HasAuthority() || bProfileIdentityAccepted) return;
    bProfileIdentitySpawnPending = true;
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(ProfileIdentityTimeoutTimer);
        GetWorld()->GetTimerManager().SetTimer(ProfileIdentityTimeoutTimer, this, &AARPGPlayerController::HandleProfileIdentityTimeout, FMath::Max(3.f, TimeoutSeconds), false);
    }
    ClientRequestProfileIdentity();
}

void AARPGPlayerController::HandleProfileIdentityTimeout()
{
    if (!HasAuthority() || bProfileIdentityAccepted) return;
    bProfileIdentitySpawnPending = false;
    const FText Message = FText::FromString(TEXT("Profile login handshake timed out."));
    ClientProfileIdentityResult(false, Message, FGuid(), FGuid());
}

void AARPGPlayerController::ClientRequestProfileIdentity_Implementation()
{
    if (UARPGNetworkSubsystem* Network = GetGameInstance() ? GetGameInstance()->GetSubsystem<UARPGNetworkSubsystem>() : nullptr)
        Network->NotifyProfileHandshakeStarted();
    SubmitLocalProfileIdentity();
}

void AARPGPlayerController::ServerSubmitProfileIdentity_Implementation(FGuid InAccountId, const FString& InUsername, FGuid InCharacterId)
{
    FText Message;
    const bool bAccepted = AcceptProfileIdentityOnAuthority(InAccountId, InUsername, InCharacterId, Message);
    ClientProfileIdentityResult(bAccepted, Message, bAccepted ? AccountId : FGuid(), bAccepted ? ProfileCharacterId : FGuid());
}

bool AARPGPlayerController::AcceptProfileIdentityOnAuthority(FGuid InAccountId, const FString& InUsername, FGuid InCharacterId, FText& OutMessage)
{
    if (!HasAuthority())
    {
        OutMessage = FText::FromString(TEXT("Profile identity can only be approved by the server."));
        return false;
    }
    if (bProfileIdentityAccepted)
    {
        if (AccountId == InAccountId)
        {
            OutMessage = FText::FromString(TEXT("Profile identity already accepted."));
            return true;
        }
        OutMessage = FText::FromString(TEXT("This connection is already bound to a different profile identity."));
        return false;
    }
    if (!InAccountId.IsValid())
    {
        OutMessage = FText::FromString(TEXT("The submitted account identity is invalid."));
        return false;
    }

    const FString SafeUsername = SanitizeProfileUsername(InUsername);
    if (SafeUsername.Len() < 3)
    {
        OutMessage = FText::FromString(TEXT("The submitted profile name is invalid."));
        return false;
    }

    // A local-profile AccountId is a trusted-direct-IP identity, not cryptographic Internet auth. The server
    // still enforces one active connection per account/character so the same local identity cannot control two
    // simultaneous pawns or alias another active player's persistent ownership.
    if (UWorld* World = GetWorld())
    {
        for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
        {
            const AARPGPlayerController* Other = Cast<AARPGPlayerController>(It->Get());
            if (!Other || Other == this || !Other->bProfileIdentityAccepted) continue;
            if (Other->AccountId == InAccountId)
            {
                OutMessage = FText::FromString(TEXT("That local profile is already connected to this host."));
                return false;
            }
            if (InCharacterId.IsValid() && Other->ProfileCharacterId == InCharacterId)
            {
                OutMessage = FText::FromString(TEXT("That character identity is already active on this host."));
                return false;
            }
        }
    }

    AccountId = InAccountId;
    ProfileCharacterId = InCharacterId.IsValid() ? InCharacterId : FGuid::NewGuid();
    RPGPlayerName = SafeUsername;
    bProfileIdentityAccepted = true;
    if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(ProfileIdentityTimeoutTimer);
    ForceNetUpdate();
    OutMessage = FText::FromString(TEXT("Profile identity accepted."));
    OnProfileIdentityResult.Broadcast(true, OutMessage, AccountId, ProfileCharacterId);

    // Only a remote connection that was explicitly held for its handshake needs the acceptance callback
    // to resume spawning here. Local standalone/listen-host spawn is owned synchronously by GameMode's
    // HandleStartingNewPlayer path, avoiding a re-entrant RestartPlayer/possession sequence.
    const bool bResumeDeferredSpawn = bProfileIdentitySpawnPending;
    bProfileIdentitySpawnPending = false;
    if (bResumeDeferredSpawn)
        if (AARPGGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AARPGGameMode>() : nullptr)
            GameMode->NotifyProfileIdentityAccepted(this);
    return true;
}

void AARPGPlayerController::ClientProfileIdentityResult_Implementation(bool bAccepted, const FText& Message, FGuid ApprovedAccountId, FGuid ApprovedCharacterId)
{
    if (bAccepted)
    {
        AccountId = ApprovedAccountId;
        ProfileCharacterId = ApprovedCharacterId;
        bProfileIdentityAccepted = true;
        if (UARPGAccountSubsystem* Accounts = GetGameInstance() ? GetGameInstance()->GetSubsystem<UARPGAccountSubsystem>() : nullptr)
        {
            // Register the server-approved CharacterId into the already password-verified local account so
            // reconnecting to this host presents the same stable identity before pawn persistence starts.
            if (Accounts->IsLoggedIn() && Accounts->CurrentAccountId == ApprovedAccountId)
                Accounts->RegisterCharacterId(ApprovedCharacterId);
        }
        if (UARPGNetworkSubsystem* Network = GetGameInstance() ? GetGameInstance()->GetSubsystem<UARPGNetworkSubsystem>() : nullptr)
            Network->NotifyProfileHandshakeAccepted();
    }
    else
    {
        bProfileIdentityAccepted = false;
        if (UARPGNetworkSubsystem* Network = GetGameInstance() ? GetGameInstance()->GetSubsystem<UARPGNetworkSubsystem>() : nullptr)
        {
            Network->NotifyProfileHandshakeRejected(Message);
            Network->ReturnToMainMenu();
        }
    }
    OnProfileIdentityResult.Broadcast(bAccepted, Message, ApprovedAccountId, ApprovedCharacterId);
}

void AARPGPlayerController::OnRep_ProfileIdentity()
{
    if (bProfileIdentityAccepted)
        OnProfileIdentityResult.Broadcast(true, FText::FromString(TEXT("Profile identity synchronized.")), AccountId, ProfileCharacterId);
}

void AARPGPlayerController::SendChatMessage(EARPGChatChannel Channel, FText Message, FString TargetName)
{
    const FString S = Message.ToString().Left(512).TrimStartAndEnd();
    if (S.IsEmpty()) return;
    if (HasAuthority()) ServerSendChatMessage_Implementation(Channel, S, TargetName);
    else ServerSendChatMessage(Channel, S, TargetName);
}

bool AARPGPlayerController::IsMessageAllowed(EARPGChatChannel Channel, const FString& Message) const
{
    if (Message.TrimStartAndEnd().IsEmpty() || Message.Len() > 512) return false;
    return Channel != EARPGChatChannel::System && Channel != EARPGChatChannel::Quest && Channel != EARPGChatChannel::Loot &&
           Channel != EARPGChatChannel::Combat && Channel != EARPGChatChannel::NPC && Channel != EARPGChatChannel::Boss && Channel != EARPGChatChannel::Event;
}

void AARPGPlayerController::ServerSendChatMessage_Implementation(EARPGChatChannel Channel, const FString& Message, const FString& TargetName)
{
    if (!IsMessageAllowed(Channel, Message)) return;
    AARPGGameState* GS = GetWorld() ? GetWorld()->GetGameState<AARPGGameState>() : nullptr;
    if (!GS) return;
    FARPGChatMessage Chat;
    Chat.Channel = Channel;
    Chat.SenderName = RPGPlayerName.IsEmpty() ? (PlayerState ? PlayerState->GetPlayerName() : TEXT("Player")) : RPGPlayerName;
    Chat.SenderAccountId = AccountId;
    Chat.TargetName = TargetName.Left(64);
    Chat.Message = FText::FromString(Message);
    Chat.TimestampUtc = FDateTime::UtcNow();
    Chat.MessageId = FGuid::NewGuid();
    GS->RoutePlayerChat(this, Chat);
}

void AARPGPlayerController::ClientReceiveChatMessage_Implementation(const FARPGChatMessage& Message)
{
    ChatHistory.Add(Message);
    while (ChatHistory.Num() > FMath::Max(10, ChatHistoryLimit)) ChatHistory.RemoveAt(0);
    OnChatMessage.Broadcast(Message);
}

void AARPGPlayerController::ClearChatHistory() { ChatHistory.Reset(); }

void AARPGPlayerController::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AARPGPlayerController, RPGPlayerName);
    DOREPLIFETIME(AARPGPlayerController, AccountId);
    DOREPLIFETIME(AARPGPlayerController, ProfileCharacterId);
    DOREPLIFETIME(AARPGPlayerController, bProfileIdentityAccepted);
    DOREPLIFETIME(AARPGPlayerController, ZoneId);
}
