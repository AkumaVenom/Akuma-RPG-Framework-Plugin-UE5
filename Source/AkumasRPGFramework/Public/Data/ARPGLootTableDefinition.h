#pragma once
#include "CoreMinimal.h"
#include "Data/ARPGDefinitionBase.h"
#include "ARPGLootTableDefinition.generated.h"

USTRUCT(BlueprintType)
struct AKUMASRPGFRAMEWORK_API FARPGLootEntry
{
    GENERATED_BODY()
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) FName ItemId = NAME_None;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta=(ClampMin="1")) int32 MinQuantity = 1;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta=(ClampMin="1")) int32 MaxQuantity = 1;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta=(ClampMin="0.0", ClampMax="1.0")) float DropChance = 1.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) bool bUniquePerKill = true;
};

UCLASS(BlueprintType)
class AKUMASRPGFRAMEWORK_API UARPGLootTableDefinition : public UARPGDefinitionBase
{
    GENERATED_BODY()
public:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Loot") TArray<FARPGLootEntry> Entries;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Loot") int64 CharacterXP = 0;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Loot") FName CurrencyId = TEXT("Gold");
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Loot") int64 MinCurrency = 0;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Loot") int64 MaxCurrency = 0;
};
