#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "ARPGTypes.h"
#include "ARPGGameState.generated.h"

class AARPGPlayerController;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FARPGOnServerChatMessage, const FARPGChatMessage&, Message);

UCLASS(BlueprintType, Blueprintable)
class AKUMASRPGFRAMEWORK_API AARPGGameState : public AGameStateBase
{
    GENERATED_BODY()
public:
    AARPGGameState();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Chat", meta=(ClampMin="100")) float SayRange = 2500.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Chat", meta=(ClampMin="100")) float YellRange = 8000.f;
    UPROPERTY(BlueprintAssignable, Category="Chat") FARPGOnServerChatMessage OnServerChatMessage;

    UFUNCTION(BlueprintCallable, Category="ARPG|Chat", meta=(BlueprintAuthorityOnly)) void RoutePlayerChat(AARPGPlayerController* Sender, const FARPGChatMessage& Message);
    UFUNCTION(BlueprintCallable, Category="ARPG|Chat", meta=(BlueprintAuthorityOnly)) void BroadcastMessage(const FARPGChatMessage& Message);
    UFUNCTION(BlueprintCallable, Category="ARPG|Chat", meta=(BlueprintAuthorityOnly)) void SendSystemMessage(FText Message, EARPGChatChannel Channel=EARPGChatChannel::System);
    UFUNCTION(BlueprintCallable, Category="ARPG|Chat", meta=(BlueprintAuthorityOnly)) void SendNPCMessage(AActor* Speaker, FString NPCName, FText Message, EARPGChatChannel Channel=EARPGChatChannel::NPC);

protected:
    bool ShouldReceivePlayerChannel(const AARPGPlayerController* Sender, const AARPGPlayerController* Receiver, EARPGChatChannel Channel, const FString& TargetName) const;
};
