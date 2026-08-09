#pragma once

#include "CoreMinimal.h"
#include "ARPGTypes.h"
#include "Data/ARPGDefinitionBase.h"
#include "ARPGDungeonDefinition.generated.h"

USTRUCT(BlueprintType)
struct AKUMASRPGFRAMEWORK_API FARPGDungeonEncounterDefinition
{
    GENERATED_BODY()
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) FName EncounterId = NAME_None;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) FText DisplayName;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) bool bRequiredForCompletion = true;
};

UCLASS(BlueprintType)
class AKUMASRPGFRAMEWORK_API UARPGDungeonDefinition : public UARPGDefinitionBase
{
    GENERATED_BODY()
public:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Instance") FName MapName = NAME_None;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Instance") bool bRaid = false;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Instance") int32 MinPlayers = 1;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Instance") int32 MaxPlayers = 5;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Instance") int32 RequiredLevel = 1;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Instance") bool bScaleToGroupSize = false;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Instance") bool bUseLockout = false;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Instance") float LockoutHours = 0.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Instance") TArray<FARPGDungeonEncounterDefinition> Encounters;
};
