#pragma once
#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Data/ARPGDefinitionBase.h"
#include "ARPGCraftingStationDefinition.generated.h"

class UARPGRecipeDefinition;

UCLASS(BlueprintType)
class AKUMASRPGFRAMEWORK_API UARPGCraftingStationDefinition : public UARPGDefinitionBase
{
    GENERATED_BODY()
public:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Station") FGameplayTag StationTag;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Station") TArray<TObjectPtr<UARPGRecipeDefinition>> Recipes;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Station", meta=(ClampMin="1")) int32 InputSlots = 16;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Station", meta=(ClampMin="1")) int32 OutputSlots = 16;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Station|Inventory") bool bUseStationInventoryForInputs = false;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Station|Inventory") bool bFuelComesFromStationInventory = true;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Station") bool bProcessWhileOffline = true;
};
