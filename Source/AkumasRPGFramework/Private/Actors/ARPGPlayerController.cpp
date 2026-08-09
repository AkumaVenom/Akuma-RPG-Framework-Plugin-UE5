#include "Actors/ARPGPlayerController.h"
#include "Actors/ARPGGameState.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/PlayerState.h"

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
    // Clients may not forge system/NPC/event feeds.
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
    DOREPLIFETIME(AARPGPlayerController, ZoneId);
}
