#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ARPGTypes.h"
#include "ARPGInventoryComponent.generated.h"

class UARPGItemDefinition;

USTRUCT(BlueprintType)
struct AKUMASRPGFRAMEWORK_API FARPGStartingInventoryItem
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Starting Item") TObjectPtr<UARPGItemDefinition> Item = nullptr;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Starting Item", meta=(ClampMin="1")) int32 Quantity = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Starting Item", meta=(DisplayName="Equip On Spawn")) bool bEquipOnSpawn = false;
    // 0 = do not pin. 1..N matches the player's Quick Access slot numbers exactly.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Starting Item", meta=(ClampMin="0", DisplayName="Quick Access Slot (0 = None)")) int32 QuickAccessSlot = 0;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FARPGOnInventoryChanged);

UCLASS(ClassGroup=(ARPG), meta=(BlueprintSpawnableComponent))
class AKUMASRPGFRAMEWORK_API UARPGInventoryComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UARPGInventoryComponent();

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Inventory", meta=(ClampMin="1")) int32 MaxSlots = 64;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Inventory", meta=(ClampMin="1")) int32 FallbackMaxStack = 999;

    // Designer-facing defaults. The framework converts these definitions into real runtime entries with stable instance GUIDs.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Inventory|Starting Items", meta=(DisplayName="Starting Items")) TArray<FARPGStartingInventoryItem> StartingItems;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Inventory|Starting Items", meta=(DisplayName="Grant Starting Items On Begin Play")) bool bGrantStartingItemsOnBeginPlay = true;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Inventory|Starting Items", meta=(EditCondition="bGrantStartingItemsOnBeginPlay", DisplayName="Only When Inventory Is Empty")) bool bOnlyGrantStartingItemsWhenEmpty = true;

    // Runtime/save state. Intentionally read-only in Class Defaults: author Starting Items above instead of hand-making GUID entries.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, ReplicatedUsing=OnRep_Items, SaveGame, Category="Inventory|Runtime", meta=(DisplayName="Runtime Items")) TArray<FARPGInventoryEntry> Items;
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
    UFUNCTION(BlueprintPure, Category="ARPG|Inventory") bool CanAddItemDefinition(const UARPGItemDefinition* Item, int32 Quantity=1) const;
    UFUNCTION(BlueprintCallable, Category="ARPG|Inventory", meta=(BlueprintAuthorityOnly)) bool SetEquipped(const FGuid& InstanceId, bool bEquipped, FGameplayTag Slot);
    UFUNCTION(BlueprintCallable, Category="ARPG|Inventory", meta=(BlueprintAuthorityOnly)) void ReplaceInventory(const TArray<FARPGInventoryEntry>& NewItems);
    UFUNCTION(BlueprintCallable, Category="ARPG|Inventory|Starting Items", meta=(BlueprintAuthorityOnly)) bool ApplyStartingItems(bool bForce=false);

    // Resolve the exact definition owned by a runtime inventory entry. New entries carry a soft asset reference;
    // older ID-only saves fall back to the framework resolver. Runtime ownership/equipment is never inferred
    // merely because a Data Asset exists in the project.
    UARPGItemDefinition* ResolveItemDefinition(const FARPGInventoryEntry& Entry) const;
    UFUNCTION(BlueprintPure, Category="ARPG|Inventory") UARPGItemDefinition* GetItemDefinitionForInstance(FGuid InstanceId) const;
    UFUNCTION(BlueprintPure, Category="ARPG|Inventory") bool IsItemInstanceEquipped(FGuid InstanceId) const;

    virtual void BeginPlay() override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
protected:
    UFUNCTION() void OnRep_Items();

    bool AddItemAuthority(FName ItemId, int32 Quantity, int32 ExplicitMaxStack=0, const UARPGItemDefinition* ExplicitDefinition=nullptr);
    bool RemoveItemAuthority(FName ItemId, int32 Quantity);
    bool RemoveItemInstanceAuthority(FGuid InstanceId, int32 Quantity);
    int32 ResolveMaxStack(FName ItemId) const;
    void BackfillDefinitionReference(FARPGInventoryEntry& Entry);
    void ApplyStartingItemsDeferred();
    bool bStartingItemsApplied = false;
    bool bStartingItemsDelayPrimed = false;
};
