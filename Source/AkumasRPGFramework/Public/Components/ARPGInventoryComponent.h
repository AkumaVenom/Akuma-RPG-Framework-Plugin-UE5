#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ARPGTypes.h"
#include "ARPGInventoryComponent.generated.h"

class UARPGItemDefinition;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FARPGOnInventoryChanged);

UCLASS(ClassGroup=(ARPG), meta=(BlueprintSpawnableComponent))
class AKUMASRPGFRAMEWORK_API UARPGInventoryComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UARPGInventoryComponent();

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Inventory", meta=(ClampMin="1")) int32 MaxSlots = 64;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Inventory", meta=(ClampMin="1")) int32 FallbackMaxStack = 999;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, ReplicatedUsing=OnRep_Items, SaveGame, Category="Inventory") TArray<FARPGInventoryEntry> Items;
    UPROPERTY(BlueprintAssignable) FARPGOnInventoryChanged OnInventoryChanged;

    UFUNCTION(BlueprintCallable, Category="ARPG|Inventory", meta=(BlueprintAuthorityOnly)) bool AddItem(FName ItemId, int32 Quantity=1);
    UFUNCTION(BlueprintCallable, Category="ARPG|Inventory", meta=(BlueprintAuthorityOnly)) bool AddItemDefinition(const UARPGItemDefinition* Item, int32 Quantity=1);
    UFUNCTION(BlueprintCallable, Category="ARPG|Inventory", meta=(BlueprintAuthorityOnly)) bool RemoveItem(FName ItemId, int32 Quantity=1);
    UFUNCTION(BlueprintCallable, Category="ARPG|Inventory", meta=(BlueprintAuthorityOnly)) bool RemoveItemInstance(FGuid InstanceId, int32 Quantity=1);
    UFUNCTION(BlueprintCallable, Category="ARPG|Inventory", meta=(BlueprintAuthorityOnly)) bool TransferItemTo(UARPGInventoryComponent* Destination, FName ItemId, int32 Quantity=1);
    UFUNCTION(BlueprintPure, Category="ARPG|Inventory") int32 GetItemCount(FName ItemId) const;
    UFUNCTION(BlueprintPure, Category="ARPG|Inventory") bool HasItem(FName ItemId, int32 Quantity=1) const;
    UFUNCTION(BlueprintPure, Category="ARPG|Inventory") int32 GetFreeSlots() const { return FMath::Max(0, MaxSlots - Items.Num()); }
    UFUNCTION(BlueprintPure, Category="ARPG|Inventory") bool CanAddItem(FName ItemId, int32 Quantity=1) const;
    UFUNCTION(BlueprintCallable, Category="ARPG|Inventory", meta=(BlueprintAuthorityOnly)) bool SetEquipped(const FGuid& InstanceId, bool bEquipped, FGameplayTag Slot);
    UFUNCTION(BlueprintCallable, Category="ARPG|Inventory", meta=(BlueprintAuthorityOnly)) void ReplaceInventory(const TArray<FARPGInventoryEntry>& NewItems);

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
protected:
    UFUNCTION() void OnRep_Items();

    bool AddItemAuthority(FName ItemId, int32 Quantity, int32 ExplicitMaxStack=0);
    bool RemoveItemAuthority(FName ItemId, int32 Quantity);
    bool RemoveItemInstanceAuthority(FGuid InstanceId, int32 Quantity);
    int32 ResolveMaxStack(FName ItemId) const;
};
