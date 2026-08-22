#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "ARPGTypes.h"
#include "ARPGPlayerController.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FARPGClientChatMessage, const FARPGChatMessage&, Message);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FARPGProfileIdentityResult, bool, bAccepted, FText, Message, FGuid, AccountId, FGuid, CharacterId);

UCLASS(BlueprintType, Blueprintable)
class AKUMASRPGFRAMEWORK_API AARPGPlayerController : public APlayerController
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category="Identity") FString RPGPlayerName = TEXT("Player");
    /** Server-approved account identity for this connection. Never populated from another player's host GameInstance. */
    UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_ProfileIdentity, Category="Identity") FGuid AccountId;
    /** Server-approved character identity used by character persistence before the pawn starts loading. */
    UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_ProfileIdentity, Category="Identity") FGuid ProfileCharacterId;
    UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_ProfileIdentity, Category="Identity") bool bProfileIdentityAccepted = false;
    UPROPERTY(BlueprintAssignable, Category="Identity") FARPGProfileIdentityResult OnProfileIdentityResult;
    /** Clears frontend UI-only input when this controller enters a gameplay world. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ARPG|Input") bool bRestoreGameplayInputOnBeginPlay = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category="Chat") FName ZoneId = NAME_None;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Chat") TArray<FARPGChatMessage> ChatHistory;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Chat", meta=(ClampMin="10")) int32 ChatHistoryLimit = 300;
    UPROPERTY(BlueprintAssignable, Category="Chat") FARPGClientChatMessage OnChatMessage;

    virtual void BeginPlay() override;

    UFUNCTION(BlueprintCallable, Category="ARPG|Identity") bool SubmitLocalProfileIdentity();
    UFUNCTION(BlueprintPure, Category="ARPG|Identity") bool IsProfileIdentityAccepted() const { return bProfileIdentityAccepted; }
    /** Authority-only local listen/standalone path. Returns true when the local GameInstance profile was accepted. */
    UFUNCTION(BlueprintCallable, Category="ARPG|Identity", meta=(BlueprintAuthorityOnly)) bool InitializeAuthorityProfileIdentityFromLocalAccount();
    UFUNCTION(BlueprintCallable, Category="ARPG|Identity", meta=(BlueprintAuthorityOnly)) void BeginProfileIdentityHandshake(float TimeoutSeconds=15.f);
    UFUNCTION(Client, Reliable) void ClientRequestProfileIdentity();
    UFUNCTION(Server, Reliable) void ServerSubmitProfileIdentity(FGuid InAccountId, const FString& InUsername, FGuid InCharacterId);
    UFUNCTION(Client, Reliable) void ClientProfileIdentityResult(bool bAccepted, const FText& Message, FGuid ApprovedAccountId, FGuid ApprovedCharacterId);

    UFUNCTION(BlueprintCallable, Category="ARPG|Chat") void SendChatMessage(EARPGChatChannel Channel, FText Message, FString TargetName=TEXT(""));
    UFUNCTION(BlueprintCallable, Category="ARPG|Chat") void ClearChatHistory();
    UFUNCTION(Client, Reliable) void ClientReceiveChatMessage(const FARPGChatMessage& Message);
    UFUNCTION(Server, Reliable) void ServerSendChatMessage(EARPGChatChannel Channel, const FString& Message, const FString& TargetName);

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
    bool IsMessageAllowed(EARPGChatChannel Channel, const FString& Message) const;
    UFUNCTION() void OnRep_ProfileIdentity();

private:
    void ApplyGameplayInputModeIfNeeded();
    FString SanitizeProfileUsername(const FString& InUsername) const;
    bool AcceptProfileIdentityOnAuthority(FGuid InAccountId, const FString& InUsername, FGuid InCharacterId, FText& OutMessage);
    FTimerHandle ProfileIdentityTimeoutTimer;
    bool bProfileIdentitySpawnPending = false;
    void HandleProfileIdentityTimeout();
};
