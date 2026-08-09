#include "Actors/ARPGGameState.h"
#include "Actors/ARPGPlayerController.h"
#include "Social/ARPGGroupComponent.h"
#include "Components/ARPGFactionComponent.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"

AARPGGameState::AARPGGameState() { bReplicates = true; }

bool AARPGGameState::ShouldReceivePlayerChannel(const AARPGPlayerController* Sender, const AARPGPlayerController* Receiver, EARPGChatChannel Channel, const FString& TargetName) const
{
    if (!Sender || !Receiver) return false;
    if (Sender == Receiver) return true;
    const APawn* SP = Sender->GetPawn();
    const APawn* RP = Receiver->GetPawn();
    switch (Channel)
    {
        case EARPGChatChannel::Say:
            return SP && RP && FVector::DistSquared(SP->GetActorLocation(), RP->GetActorLocation()) <= FMath::Square(SayRange);
        case EARPGChatChannel::Yell:
            return SP && RP && FVector::DistSquared(SP->GetActorLocation(), RP->GetActorLocation()) <= FMath::Square(YellRange);
        case EARPGChatChannel::Whisper:
            return Receiver->RPGPlayerName.Equals(TargetName, ESearchCase::IgnoreCase) || (Receiver->PlayerState && Receiver->PlayerState->GetPlayerName().Equals(TargetName, ESearchCase::IgnoreCase));
        case EARPGChatChannel::Zone:
            return !Sender->ZoneId.IsNone() && Sender->ZoneId == Receiver->ZoneId;
        case EARPGChatChannel::Party:
        case EARPGChatChannel::Raid:
        {
            const UARPGGroupComponent* SG = SP ? SP->FindComponentByClass<UARPGGroupComponent>() : nullptr;
            const UARPGGroupComponent* RG = RP ? RP->FindComponentByClass<UARPGGroupComponent>() : nullptr;
            return SG && RG && SG->IsInSameGroup(RG, Channel == EARPGChatChannel::Raid);
        }
        case EARPGChatChannel::Guild:
        {
            const UARPGGroupComponent* SG = SP ? SP->FindComponentByClass<UARPGGroupComponent>() : nullptr;
            const UARPGGroupComponent* RG = RP ? RP->FindComponentByClass<UARPGGroupComponent>() : nullptr;
            return SG && RG && !SG->GuildId.IsNone() && SG->GuildId == RG->GuildId;
        }
        case EARPGChatChannel::Faction:
        {
            const UARPGFactionComponent* SF = SP ? SP->FindComponentByClass<UARPGFactionComponent>() : nullptr;
            const UARPGFactionComponent* RF = RP ? RP->FindComponentByClass<UARPGFactionComponent>() : nullptr;
            return SF && RF && !SF->GetPrimaryFactionId().IsNone() && SF->GetPrimaryFactionId() == RF->GetPrimaryFactionId();
        }
        default:
            return true;
    }
}

void AARPGGameState::RoutePlayerChat(AARPGPlayerController* Sender, const FARPGChatMessage& Message)
{
    if (!HasAuthority() || !Sender) return;
    OnServerChatMessage.Broadcast(Message);
    for (TActorIterator<AARPGPlayerController> It(GetWorld()); It; ++It)
    {
        AARPGPlayerController* Receiver = *It;
        if (ShouldReceivePlayerChannel(Sender, Receiver, Message.Channel, Message.TargetName)) Receiver->ClientReceiveChatMessage(Message);
    }
}

void AARPGGameState::BroadcastMessage(const FARPGChatMessage& Message)
{
    if (!HasAuthority()) return;
    FARPGChatMessage Final = Message;
    if (!Final.MessageId.IsValid()) Final.MessageId = FGuid::NewGuid();
    if (Final.TimestampUtc.GetTicks() <= 0) Final.TimestampUtc = FDateTime::UtcNow();
    OnServerChatMessage.Broadcast(Final);
    for (TActorIterator<AARPGPlayerController> It(GetWorld()); It; ++It) (*It)->ClientReceiveChatMessage(Final);
}

void AARPGGameState::SendSystemMessage(FText Message, EARPGChatChannel Channel)
{
    FARPGChatMessage M;
    M.Channel = Channel;
    M.SenderName = TEXT("System");
    M.Message = Message;
    M.bSystemGenerated = true;
    BroadcastMessage(M);
}

void AARPGGameState::SendNPCMessage(AActor* Speaker, FString NPCName, FText Message, EARPGChatChannel Channel)
{
    if (!HasAuthority()) return;
    FARPGChatMessage M;
    M.Channel = Channel;
    M.SenderName = MoveTemp(NPCName);
    M.Message = Message;
    M.bFromNPC = true;
    M.MessageId = FGuid::NewGuid();
    M.TimestampUtc = FDateTime::UtcNow();
    OnServerChatMessage.Broadcast(M);

    const float Range = Channel == EARPGChatChannel::Yell ? YellRange : SayRange;
    for (TActorIterator<AARPGPlayerController> It(GetWorld()); It; ++It)
    {
        AARPGPlayerController* Receiver = *It;
        const APawn* RP = Receiver->GetPawn();
        if (!Speaker || Channel == EARPGChatChannel::World || Channel == EARPGChatChannel::Boss || Channel == EARPGChatChannel::Event || !RP || FVector::DistSquared(Speaker->GetActorLocation(), RP->GetActorLocation()) <= FMath::Square(Range))
            Receiver->ClientReceiveChatMessage(M);
    }
}
