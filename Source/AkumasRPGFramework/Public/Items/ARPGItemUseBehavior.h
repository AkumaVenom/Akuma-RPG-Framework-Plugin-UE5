#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "ARPGItemUseBehavior.generated.h"

class AARPGCharacter;
class UARPGItemDefinition;

/** Where a successful item-use request originated. Gameplay authority is identical for every source. */
UENUM(BlueprintType)
enum class EARPGItemUseSource : uint8
{
    Direct,
    InventoryUI,
    QuickAccess,
    Blueprint
};

/** Authoritative result for the generic item-use pipeline. */
UENUM(BlueprintType)
enum class EARPGItemUseResult : uint8
{
    Success,
    InvalidItem,
    ItemUnavailable,
    ItemNotUsable,
    OnCooldown,
    InsufficientQuantity,
    NoUsefulEffect,
    CustomUseRejected,
    UseFailed
};

/** Complete Blueprint-facing context for one use attempt. */
USTRUCT(BlueprintType)
struct AKUMASRPGFRAMEWORK_API FARPGItemUseContext
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="ARPG|Item Use") TObjectPtr<AARPGCharacter> User = nullptr;
    UPROPERTY(BlueprintReadOnly, Category="ARPG|Item Use") TObjectPtr<UARPGItemDefinition> ItemDefinition = nullptr;
    UPROPERTY(BlueprintReadOnly, Category="ARPG|Item Use") FGuid ItemInstanceId;
    UPROPERTY(BlueprintReadOnly, Category="ARPG|Item Use") FName ItemId = NAME_None;
    UPROPERTY(BlueprintReadOnly, Category="ARPG|Item Use") int32 QuantityBeforeUse = 0;
    UPROPERTY(BlueprintReadOnly, Category="ARPG|Item Use") int32 ConsumeQuantity = 0;
    UPROPERTY(BlueprintReadOnly, Category="ARPG|Item Use") EARPGItemUseSource Source = EARPGItemUseSource::Direct;
    /** 1-based Quick Access slot when Source == QuickAccess; otherwise 0. */
    UPROPERTY(BlueprintReadOnly, Category="ARPG|Item Use") int32 QuickAccessSlot = 0;
};

/**
 * Optional per-item Blueprint behavior.
 * Create a Blueprint Class derived from ARPGItemUseBehavior, implement the events, then assign that
 * class on an Item Definition under Use -> Custom Behavior. Execute Item Use runs on authority only;
 * Play Item Use Presentation runs on replicated clients only after the authoritative use succeeds.
 */
UCLASS(Abstract, BlueprintType, Blueprintable)
class AKUMASRPGFRAMEWORK_API UARPGItemUseBehavior : public UObject
{
    GENERATED_BODY()
public:
    /** Validation-only hook. Do not mutate gameplay state here. */
    UFUNCTION(BlueprintNativeEvent, Category="ARPG|Item Use", meta=(DisplayName="Can Use Item"))
    bool CanUseItem(const FARPGItemUseContext& Context, FText& OutFailureReason);
    virtual bool CanUseItem_Implementation(const FARPGItemUseContext& Context, FText& OutFailureReason);

    /** Authority-only effect hook. Return true when this behavior successfully applied its effect. */
    UFUNCTION(BlueprintNativeEvent, Category="ARPG|Item Use", meta=(DisplayName="Execute Item Use"))
    bool ExecuteItemUse(const FARPGItemUseContext& Context);
    virtual bool ExecuteItemUse_Implementation(const FARPGItemUseContext& Context);

    /** Cosmetic hook invoked after a successful authoritative use. Safe for VFX/UI/audio-only Blueprint work. */
    UFUNCTION(BlueprintNativeEvent, Category="ARPG|Item Use", meta=(DisplayName="Play Item Use Presentation"))
    void PlayItemUsePresentation(const FARPGItemUseContext& Context);
    virtual void PlayItemUsePresentation_Implementation(const FARPGItemUseContext& Context);
};
