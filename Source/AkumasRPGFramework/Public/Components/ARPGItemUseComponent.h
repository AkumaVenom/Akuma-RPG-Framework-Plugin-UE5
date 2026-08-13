#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Items/ARPGItemUseBehavior.h"
#include "ARPGItemUseComponent.generated.h"

class AARPGCharacter;
class UARPGInventoryComponent;
class UARPGItemDefinition;
class UARPGStatsComponent;

USTRUCT(BlueprintType)
struct AKUMASRPGFRAMEWORK_API FARPGItemUseCooldownState
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="ARPG|Item Use") FName ItemId = NAME_None;
    UPROPERTY(BlueprintReadOnly, Category="ARPG|Item Use") float CooldownEndServerTime = 0.f;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FARPGOnItemUsed, FARPGItemUseContext, Context);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FARPGOnItemUseResult, EARPGItemUseResult, Result, FGuid, ItemInstanceId, UARPGItemDefinition*, ItemDefinition, FText, FailureReason);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FARPGOnItemUseCooldownsChanged);

/**
 * Server-authoritative generic item-use owner shared by Inventory UI, Quick Access and Blueprint calls.
 * It centralizes validation, custom Blueprint behavior, built-in vital/GAS effects, consumption,
 * cooldowns and replicated presentation so every way of using an item follows identical rules.
 */
UCLASS(ClassGroup=(ARPG), BlueprintType, Blueprintable, meta=(BlueprintSpawnableComponent))
class AKUMASRPGFRAMEWORK_API UARPGItemUseComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UARPGItemUseComponent();

    UPROPERTY(BlueprintAssignable, Category="ARPG|Item Use|Events") FARPGOnItemUsed OnItemUsed;
    UPROPERTY(BlueprintAssignable, Category="ARPG|Item Use|Events") FARPGOnItemUseResult OnItemUseResult;
    UPROPERTY(BlueprintAssignable, Category="ARPG|Item Use|Events") FARPGOnItemUseCooldownsChanged OnItemUseCooldownsChanged;

    /** Owner-only runtime cooldown projection. Cooldowns are intentionally not SaveGame state. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, ReplicatedUsing=OnRep_Cooldowns, Category="ARPG|Item Use|Runtime")
    TArray<FARPGItemUseCooldownState> Cooldowns;

    /** Client-safe one-call use by exact owned runtime inventory instance. */
    UFUNCTION(BlueprintCallable, Category="ARPG|Item Use")
    bool UseItem(FGuid ItemInstanceId, EARPGItemUseSource Source = EARPGItemUseSource::Blueprint, int32 QuickAccessSlot = 0);

    /** Convenience helper: uses the first owned stack/instance matching ItemId. */
    UFUNCTION(BlueprintCallable, Category="ARPG|Item Use")
    bool UseFirstItemById(FName ItemId, EARPGItemUseSource Source = EARPGItemUseSource::Blueprint);

    /** Local/client-safe advisory preflight. Authority still revalidates every use request. */
    UFUNCTION(BlueprintPure, Category="ARPG|Item Use") bool CanUseItemNow(FGuid ItemInstanceId) const;
    UFUNCTION(BlueprintPure, Category="ARPG|Item Use") float GetCooldownRemaining(FName ItemId) const;
    UFUNCTION(BlueprintPure, Category="ARPG|Item Use") float GetItemInstanceCooldownRemaining(FGuid ItemInstanceId) const;
    UFUNCTION(BlueprintPure, Category="ARPG|Item Use") bool IsItemOnCooldown(FName ItemId) const { return GetCooldownRemaining(ItemId) > KINDA_SMALL_NUMBER; }

    /** C++ authority path used by Quick Access so hotbar and direct inventory use share one implementation. */
    EARPGItemUseResult UseItemAuthority(FGuid ItemInstanceId, EARPGItemUseSource Source, int32 QuickAccessSlot, FText& OutFailureReason);

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
    UFUNCTION() void OnRep_Cooldowns();
    UFUNCTION(Server, Reliable) void ServerUseItem(FGuid ItemInstanceId, EARPGItemUseSource Source, int32 QuickAccessSlot);
    UFUNCTION(Client, Reliable) void ClientReceiveItemUseResult(EARPGItemUseResult Result, FGuid ItemInstanceId, UARPGItemDefinition* ItemDefinition, const FText& FailureReason);
    UFUNCTION(NetMulticast, Unreliable) void MulticastPlayItemUsePresentation(FGuid ItemInstanceId, UARPGItemDefinition* ItemDefinition, int32 QuantityBeforeUse, int32 ConsumeQuantity, EARPGItemUseSource Source, int32 QuickAccessSlot);

private:
    UARPGInventoryComponent* GetInventory() const;
    AARPGCharacter* GetARPGCharacter() const;
    float GetServerTimeSeconds() const;
    float GetCooldownEnd(FName ItemId) const;
    void SetCooldownAuthority(FName ItemId, float CooldownEndServerTime);
    UARPGItemUseBehavior* CreateUseBehavior(const UARPGItemDefinition* Definition) const;
    bool HasConfiguredBuiltInVitalRestore(const UARPGItemDefinition* Definition) const;
    bool HasUsefulBuiltInVitalRestore(const UARPGItemDefinition* Definition, const UARPGStatsComponent* Stats) const;
    bool ShouldBlockForFullConfiguredVitals(const UARPGItemDefinition* Definition, const UARPGStatsComponent* Stats) const;
    FText BuildFullVitalsFailureReason(const UARPGItemDefinition* Definition) const;
    void PlayPresentationLocal(const FARPGItemUseContext& Context);
    void SendResultToOwner(EARPGItemUseResult Result, FGuid ItemInstanceId, UARPGItemDefinition* Definition, const FText& FailureReason);
};
