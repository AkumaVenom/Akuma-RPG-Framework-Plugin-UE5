#pragma once
#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Data/ARPGDefinitionBase.h"
#include "ARPGRecipeDefinition.generated.h"

USTRUCT(BlueprintType)
struct AKUMASRPGFRAMEWORK_API FARPGItemAmount
{
    GENERATED_BODY()
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) FName ItemId = NAME_None;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta=(ClampMin="1")) int32 Quantity = 1;
};

UCLASS(BlueprintType)
class AKUMASRPGFRAMEWORK_API UARPGRecipeDefinition : public UARPGDefinitionBase
{
    GENERATED_BODY()
public:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Recipe") TArray<FARPGItemAmount> Inputs;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Recipe") TArray<FARPGItemAmount> Outputs;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Recipe") FGameplayTag RequiredStationTag;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Recipe") FName RequiredSkillId = NAME_None;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Recipe", meta=(ClampMin="1")) int32 RequiredSkillLevel = 1;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Recipe", meta=(ClampMin="0")) int64 SkillXP = 0;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Recipe", meta=(ClampMin="0.0")) float CraftSeconds = 1.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Recipe") bool bConsumesFuel = false;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Recipe") FGameplayTag FuelTag;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Recipe", meta=(ClampMin="0.0")) float FuelPerCraft = 1.f;
};
