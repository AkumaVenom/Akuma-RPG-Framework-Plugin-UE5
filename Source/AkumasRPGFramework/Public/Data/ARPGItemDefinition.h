#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "ARPGTypes.h"
#include "Data/ARPGDefinitionBase.h"
#include "Equipment/ARPGEquipmentVisualActor.h"
#include "ARPGItemDefinition.generated.h"

class UGameplayEffect;
class UAnimMontage;
class UStaticMesh;
class USkeletalMesh;
class USoundBase;

UCLASS(BlueprintType)
class AKUMASRPGFRAMEWORK_API UARPGItemDefinition : public UARPGDefinitionBase
{
    GENERATED_BODY()
public:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Item") EARPGRarity Rarity = EARPGRarity::Common;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Item", meta=(ClampMin="1")) int32 MaxStack = 1;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Item") int64 BaseValue = 0;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Item") FGameplayTagContainer ItemTags;

    // Generic gathering-tool metadata. Woodcutting uses these values automatically for equipped axes,
    // and future gathering professions can reuse the same item definition instead of creating tool-specific item classes.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Gathering") FGameplayTagContainer GatheringToolTags;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Gathering", meta=(ClampMin="0.01")) float GatheringPower = 1.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Gathering", meta=(ClampMin="0")) int32 GatheringToolTier = 0;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Equipment") bool bEquippable = false;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Equipment") FGameplayTag EquipmentSlot;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Equipment") int32 RequiredLevel = 1;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Equipment") FName RequiredClassId = NAME_None;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Equipment") TSubclassOf<UGameplayEffect> EquippedGameplayEffect;
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
