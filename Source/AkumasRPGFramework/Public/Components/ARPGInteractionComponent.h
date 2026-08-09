#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ARPGInteractionComponent.generated.h"

class UARPGVendorComponent;
class AARPGStorageActor;
class AARPGCraftingStationActor;
class UARPGRecipeDefinition;
class UARPGQuestGiverComponent;
class UARPGQuestDefinition;
class UARPGSlayerMasterComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FARPGInteractionResult, bool, bSuccess, FText, Message);

UCLASS(ClassGroup=(ARPG), meta=(BlueprintSpawnableComponent))
class AKUMASRPGFRAMEWORK_API UARPGInteractionComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UARPGInteractionComponent();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction", meta=(ClampMin="100.0")) float MaxInteractionDistance = 600.f;
    UPROPERTY(BlueprintAssignable) FARPGInteractionResult OnInteractionResult;

    UFUNCTION(BlueprintCallable, Category="ARPG|Interaction|Vendor") void BuyFromVendor(UARPGVendorComponent* Vendor, FName ItemId, int32 Quantity=1);
    UFUNCTION(BlueprintCallable, Category="ARPG|Interaction|Vendor") void SellToVendor(UARPGVendorComponent* Vendor, FName ItemId, int32 Quantity=1);
    UFUNCTION(BlueprintCallable, Category="ARPG|Interaction|Vendor") void BuybackFromVendor(UARPGVendorComponent* Vendor, FGuid BuybackId);
    UFUNCTION(BlueprintCallable, Category="ARPG|Interaction|Storage") void DepositToStorage(AARPGStorageActor* Storage, FName ItemId, int32 Quantity=1);
    UFUNCTION(BlueprintCallable, Category="ARPG|Interaction|Storage") void WithdrawFromStorage(AARPGStorageActor* Storage, FName ItemId, int32 Quantity=1);
    UFUNCTION(BlueprintCallable, Category="ARPG|Interaction|Crafting") void QueueCraft(AARPGCraftingStationActor* Station, UARPGRecipeDefinition* Recipe, int32 Count=1);
    UFUNCTION(BlueprintCallable, Category="ARPG|Interaction|Quest") void AcceptQuest(UARPGQuestGiverComponent* Giver, UARPGQuestDefinition* Quest);
    UFUNCTION(BlueprintCallable, Category="ARPG|Interaction|Quest") void TurnInQuest(UARPGQuestGiverComponent* Giver, UARPGQuestDefinition* Quest);
    UFUNCTION(BlueprintCallable, Category="ARPG|Interaction|Slayer") void RequestSlayerTask(UARPGSlayerMasterComponent* Master);
    UFUNCTION(BlueprintCallable, Category="ARPG|Interaction|Slayer") void CancelSlayerTask(UARPGSlayerMasterComponent* Master);

protected:
    UFUNCTION(Server, Reliable) void ServerBuyFromVendor(UARPGVendorComponent* Vendor, FName ItemId, int32 Quantity);
    UFUNCTION(Server, Reliable) void ServerSellToVendor(UARPGVendorComponent* Vendor, FName ItemId, int32 Quantity);
    UFUNCTION(Server, Reliable) void ServerBuybackFromVendor(UARPGVendorComponent* Vendor, FGuid BuybackId);
    UFUNCTION(Server, Reliable) void ServerDepositToStorage(AARPGStorageActor* Storage, FName ItemId, int32 Quantity);
    UFUNCTION(Server, Reliable) void ServerWithdrawFromStorage(AARPGStorageActor* Storage, FName ItemId, int32 Quantity);
    UFUNCTION(Server, Reliable) void ServerQueueCraft(AARPGCraftingStationActor* Station, UARPGRecipeDefinition* Recipe, int32 Count);
    UFUNCTION(Server, Reliable) void ServerAcceptQuest(UARPGQuestGiverComponent* Giver, UARPGQuestDefinition* Quest);
    UFUNCTION(Server, Reliable) void ServerTurnInQuest(UARPGQuestGiverComponent* Giver, UARPGQuestDefinition* Quest);
    UFUNCTION(Server, Reliable) void ServerRequestSlayerTask(UARPGSlayerMasterComponent* Master);
    UFUNCTION(Server, Reliable) void ServerCancelSlayerTask(UARPGSlayerMasterComponent* Master);
    UFUNCTION(Client, Reliable) void ClientInteractionResult(bool bSuccess, const FText& Message);

    bool IsActorInRange(const AActor* Target) const;
};
