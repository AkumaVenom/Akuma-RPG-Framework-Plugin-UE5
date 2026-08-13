#pragma once
#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Data/ARPGDefinitionBase.h"
#include "ARPGRecipeDefinition.generated.h"

class UARPGItemDefinition;
class USoundBase;

/** One ingredient/output line. New content can select Item directly; ItemId remains for backwards-compatible ID-authored recipes. */
USTRUCT(BlueprintType)
struct AKUMASRPGFRAMEWORK_API FARPGItemAmount
{
    GENERATED_BODY()

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Item") TObjectPtr<UARPGItemDefinition> Item = nullptr;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Item", meta=(DisplayName="Item Id (Legacy / Optional)")) FName ItemId = NAME_None;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Item", meta=(ClampMin="1")) int32 Quantity = 1;
};

UCLASS(BlueprintType)
class AKUMASRPGFRAMEWORK_API UARPGRecipeDefinition : public UARPGDefinitionBase
{
    GENERATED_BODY()
public:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Recipe") TArray<FARPGItemAmount> Inputs;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Recipe") TArray<FARPGItemAmount> Outputs;

    /** Recipes with no required station can be crafted from the player's inherited Crafting component. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Recipe|Player Crafting", meta=(DisplayName="Allow Player Crafting")) bool bAllowPlayerCrafting = true;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Recipe|Player Crafting") FName CraftingCategory = TEXT("General");
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Recipe|Player Crafting", meta=(ClampMin="1", ClampMax="999")) int32 MaxBatchSize = 99;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Recipe|Requirements") FGameplayTag RequiredStationTag;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Recipe|Requirements") FName RequiredSkillId = NAME_None;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Recipe|Requirements", meta=(ClampMin="1")) int32 RequiredSkillLevel = 1;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Recipe|Rewards", meta=(ClampMin="0")) int64 SkillXP = 0;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Recipe|Timing", meta=(ClampMin="0.0", Units="s")) float CraftSeconds = 1.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Recipe|Presentation") TSoftObjectPtr<USoundBase> CraftCompleteSound;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Recipe|Station Fuel") bool bConsumesFuel = false;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Recipe|Station Fuel") FGameplayTag FuelTag;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Recipe|Station Fuel", meta=(ClampMin="0.0")) float FuelPerCraft = 1.f;
};
