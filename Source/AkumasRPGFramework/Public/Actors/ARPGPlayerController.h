#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "ARPGTypes.h"
#include "ARPGPlayerController.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FARPGClientChatMessage, const FARPGChatMessage&, Message);

UCLASS(BlueprintType, Blueprintable)
class AKUMASRPGFRAMEWORK_API AARPGPlayerController : public APlayerController
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category="Identity") FString RPGPlayerName = TEXT("Player");
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category="Identity") FGuid AccountId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category="Chat") FName ZoneId = NAME_None;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Chat") TArray<FARPGChatMessage> ChatHistory;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Chat", meta=(ClampMin="10")) int32 ChatHistoryLimit = 300;
    UPROPERTY(BlueprintAssignable, Category="Chat") FARPGClientChatMessage OnChatMessage;

    UFUNCTION(BlueprintCallable, Category="ARPG|Chat") void SendChatMessage(EARPGChatChannel Channel, FText Message, FString TargetName=TEXT(""));
    UFUNCTION(BlueprintCallable, Category="ARPG|Chat") void ClearChatHistory();
    UFUNCTION(Client, Reliable) void ClientReceiveChatMessage(const FARPGChatMessage& Message);
    UFUNCTION(Server, Reliable) void ServerSendChatMessage(EARPGChatChannel Channel, const FString& Message, const FString& TargetName);

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
protected:
    bool IsMessageAllowed(EARPGChatChannel Channel, const FString& Message) const;
};
