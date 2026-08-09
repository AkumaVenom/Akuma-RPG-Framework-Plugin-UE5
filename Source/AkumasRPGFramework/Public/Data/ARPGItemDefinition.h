#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "ARPGTypes.h"
#include "Data/ARPGDefinitionBase.h"
#include "ARPGItemDefinition.generated.h"

class UGameplayEffect;
class UAnimMontage;

UCLASS(BlueprintType)
class AKUMASRPGFRAMEWORK_API UARPGItemDefinition : public UARPGDefinitionBase
{
    GENERATED_BODY()
public:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Item") EARPGRarity Rarity = EARPGRarity::Common;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Item", meta=(ClampMin="1")) int32 MaxStack = 1;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Item") int64 BaseValue = 0;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Item") FGameplayTagContainer ItemTags;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Equipment") bool bEquippable = false;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Equipment") FGameplayTag EquipmentSlot;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Equipment") int32 RequiredLevel = 1;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Equipment") FName RequiredClassId = NAME_None;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Equipment") TSubclassOf<UGameplayEffect> EquippedGameplayEffect;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Equipment") FName AttachSocket = NAME_None;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Equipment") TSoftObjectPtr<UAnimMontage> EquipMontage;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Equipment") TSoftObjectPtr<UAnimMontage> UnequipMontage;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="World") TSoftClassPtr<AActor> WorldActorClass;
};
