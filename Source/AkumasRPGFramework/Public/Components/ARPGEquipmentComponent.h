#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "GameplayEffectTypes.h"
#include "ARPGEquipmentComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FARPGEquipmentChanged, FGameplayTag, Slot, FGuid, ItemInstanceId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FARPGEquipmentRequestResult, bool, bSuccess, FGuid, ItemInstanceId);

UCLASS(ClassGroup=(ARPG), meta=(BlueprintSpawnableComponent))
class AKUMASRPGFRAMEWORK_API UARPGEquipmentComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UARPGEquipmentComponent();
    UPROPERTY(BlueprintAssignable) FARPGEquipmentChanged OnEquipmentChanged;
    UPROPERTY(BlueprintAssignable) FARPGEquipmentRequestResult OnEquipmentRequestResult;
    UFUNCTION(BlueprintCallable, Category="ARPG|Equipment") bool EquipItem(FGuid ItemInstanceId);
    UFUNCTION(BlueprintCallable, Category="ARPG|Equipment") bool UnequipItem(FGuid ItemInstanceId);
    UFUNCTION(BlueprintCallable, Category="ARPG|Equipment", meta=(BlueprintAuthorityOnly)) void RefreshEquipmentEffects();
    UFUNCTION(BlueprintPure, Category="ARPG|Equipment") FGuid GetEquippedItemInSlot(FGameplayTag Slot) const;
    virtual void BeginPlay() override;
protected:
    UFUNCTION(Server, Reliable) void ServerEquipItem(FGuid ItemInstanceId);
    UFUNCTION(Server, Reliable) void ServerUnequipItem(FGuid ItemInstanceId);
    UFUNCTION() void HandleInventoryChanged();
    bool EquipAuthority(FGuid ItemInstanceId);
    bool UnequipAuthority(FGuid ItemInstanceId);
    TMap<FGuid, FActiveGameplayEffectHandle> ActiveEquipmentEffects;
};
