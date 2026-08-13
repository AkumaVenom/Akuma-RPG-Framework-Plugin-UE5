#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "ARPGTypes.h"
#include "Data/ARPGDefinitionBase.h"
#include "Data/ARPGRecipeDefinition.h"
#include "Equipment/ARPGEquipmentVisualActor.h"
#include "Stats/ARPGStatTypes.h"
#include "ARPGItemDefinition.generated.h"

class UGameplayEffect;
class UAnimMontage;
class UStaticMesh;
class USkeletalMesh;
class USoundBase;
class UARPGItemUseBehavior;

UCLASS(BlueprintType)
class AKUMASRPGFRAMEWORK_API UARPGItemDefinition : public UARPGDefinitionBase
{
    GENERATED_BODY()
public:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Item") EARPGRarity Rarity = EARPGRarity::Common;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Item", meta=(ClampMin="1")) int32 MaxStack = 1;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Item") int64 BaseValue = 0;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Item") FGameplayTagContainer ItemTags;

    // Quick-access / hotbar behavior. Existing equippable items work immediately through Auto without
    // requiring every older Item Definition to be re-authored. Designers can disable individual items.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Quick Access") bool bAllowQuickAccess = true;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Quick Access") EARPGQuickAccessAction QuickAccessAction = EARPGQuickAccessAction::Auto;

    // Generic item-use path for food, potions and other consumables. Direct vital restoration works with
    // the framework Stats component; UseGameplayEffect supports project-specific GAS effects/buffs.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Use") bool bUsable = false;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Use", meta=(EditCondition="bUsable")) bool bConsumeOnUse = true;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Use", meta=(EditCondition="bUsable && bConsumeOnUse", ClampMin="1")) int32 ConsumeQuantity = 1;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Use", meta=(EditCondition="bUsable", ClampMin="0.0")) float UseCooldownSeconds = 0.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Use|Vitals", meta=(EditCondition="bUsable", ClampMin="0.0")) float RestoreHealth = 0.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Use|Vitals", meta=(EditCondition="bUsable", ClampMin="0.0")) float RestoreMana = 0.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Use|Vitals", meta=(EditCondition="bUsable", ClampMin="0.0")) float RestoreStamina = 0.f;
    /**
     * Safe-by-default consumable rule. When this item restores any framework vital, at least one configured
     * vital must actually be missing before the item can be used. Enable only for intentionally mixed items
     * whose independent Gameplay Effect/custom behavior should remain usable even while all restored vitals are full.
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Use|Vitals", meta=(EditCondition="bUsable", DisplayName="Allow Other Effects When Restored Vitals Are Full"))
    bool bAllowOtherEffectsWhenRestoredVitalsFull = false;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Use|Effects", meta=(EditCondition="bUsable")) TSubclassOf<UGameplayEffect> UseGameplayEffect;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Use|Effects", meta=(EditCondition="bUsable", ClampMin="0.01")) float UseGameplayEffectLevel = 1.f;
    /** Optional item-specific Blueprint logic. Create a Blueprint Class derived from ARPGItemUseBehavior and assign it here. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Use|Custom Behavior", meta=(EditCondition="bUsable", DisplayName="Item Use Behavior Class")) TSubclassOf<UARPGItemUseBehavior> UseBehaviorClass;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Use|Presentation", meta=(EditCondition="bUsable")) TSoftObjectPtr<UAnimMontage> UseMontage;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Use|Presentation", meta=(EditCondition="bUsable")) TSoftObjectPtr<USoundBase> UseSound;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Use|Presentation", meta=(EditCondition="bUsable", ClampMin="0.0")) float UseAudioVolume = 1.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Use|Presentation", meta=(EditCondition="bUsable", ClampMin="0.01")) float UseAudioPitchMin = 0.97f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Use|Presentation", meta=(EditCondition="bUsable", ClampMin="0.01")) float UseAudioPitchMax = 1.03f;

    // Generic gathering-tool metadata. Woodcutting uses these values automatically for equipped axes,
    // and future gathering professions can reuse the same item definition instead of creating tool-specific item classes.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Gathering") FGameplayTagContainer GatheringToolTags;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Gathering", meta=(ClampMin="0.01")) float GatheringPower = 1.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Gathering", meta=(ClampMin="0")) int32 GatheringToolTier = 0;
    // Runtime durability. Durability belongs to the exact inventory InstanceId, not the Data Asset.
    // Wear is authority-only and is applied only after a successful gameplay action.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Durability", meta=(DisplayName="Uses Durability")) bool bUsesDurability = false;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Durability", meta=(EditCondition="bUsesDurability", ClampMin="1.0")) float MaxDurability = 100.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Durability|Wear", meta=(EditCondition="bUsesDurability", DisplayName="Lose Durability On Combat Hit")) bool bLoseDurabilityOnCombatHit = false;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Durability|Wear", meta=(EditCondition="bUsesDurability && bLoseDurabilityOnCombatHit", ClampMin="0.0", DisplayName="Combat Wear Per Successful Hit")) float CombatDurabilityLossPerSuccessfulHit = 1.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Durability|Wear", meta=(EditCondition="bUsesDurability", DisplayName="Lose Durability On Gathering Hit")) bool bLoseDurabilityOnGatheringHit = false;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Durability|Wear", meta=(EditCondition="bUsesDurability && bLoseDurabilityOnGatheringHit", ClampMin="0.0", DisplayName="Gathering Wear Per Successful Hit")) float GatheringDurabilityLossPerSuccessfulHit = 1.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Durability|Broken", meta=(EditCondition="bUsesDurability", DisplayName="Unequip When Broken")) bool bUnequipWhenBroken = true;

    // Repair ingredient quantities represent the cost to repair from fully broken to full durability.
    // With proportional scaling enabled, a 25%-damaged item consumes roughly 25% of each authored full-repair cost.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Durability|Repair", meta=(EditCondition="bUsesDurability", DisplayName="Can Be Repaired")) bool bCanBeRepaired = true;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Durability|Repair", meta=(EditCondition="bUsesDurability && bCanBeRepaired")) TArray<FARPGItemAmount> RepairInputs;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Durability|Repair", meta=(EditCondition="bUsesDurability && bCanBeRepaired", DisplayName="Allow Free Repair")) bool bAllowFreeRepair = false;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Durability|Repair", meta=(EditCondition="bUsesDurability && bCanBeRepaired", DisplayName="Scale Repair Cost By Missing Durability")) bool bScaleRepairCostByMissingDurability = true;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Equipment") bool bEquippable = false;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Equipment") FGameplayTag EquipmentSlot;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Equipment") int32 RequiredLevel = 1;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Equipment") FName RequiredClassId = NAME_None;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Equipment") TSubclassOf<UGameplayEffect> EquippedGameplayEffect;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Equipment|Stats", meta=(ShowOnlyInnerProperties)) FARPGStatModifier EquippedStatModifier;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Equipment|Visual") FName AttachSocket = NAME_None;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Equipment|Visual") TSubclassOf<AARPGEquipmentVisualActor> EquippedVisualActorClass;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Equipment|Visual", meta=(DisplayName="Equipped Static Mesh")) TSoftObjectPtr<UStaticMesh> EquippedStaticMesh;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Equipment|Visual", meta=(DisplayName="Equipped Skeletal Mesh")) TSoftObjectPtr<USkeletalMesh> EquippedSkeletalMesh;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Equipment|Visual") FTransform EquippedRelativeTransform = FTransform::Identity;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Equipment|Animation") TSoftObjectPtr<UAnimMontage> EquipMontage;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Equipment|Animation") TSoftObjectPtr<UAnimMontage> UnequipMontage;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Equipment|Audio") TSoftObjectPtr<USoundBase> EquipSound;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Equipment|Audio") TSoftObjectPtr<USoundBase> UnequipSound;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Equipment|Audio") TSoftObjectPtr<USoundBase> CombatSwingSound;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Equipment|Audio") TSoftObjectPtr<USoundBase> GatheringSwingSound;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Equipment|Audio") TSoftObjectPtr<USoundBase> GatheringHitSound;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Equipment|Audio", meta=(ClampMin="0.0")) float EquipmentAudioVolume = 1.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Equipment|Audio", meta=(ClampMin="0.01")) float EquipmentAudioPitchMin = 0.97f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Equipment|Audio", meta=(ClampMin="0.01")) float EquipmentAudioPitchMax = 1.03f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="World") TSoftClassPtr<AActor> WorldActorClass;
};
