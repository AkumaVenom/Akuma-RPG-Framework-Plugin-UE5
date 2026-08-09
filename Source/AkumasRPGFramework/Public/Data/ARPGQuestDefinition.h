#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "ARPGTypes.h"
#include "Data/ARPGDefinitionBase.h"
#include "ARPGQuestDefinition.generated.h"

USTRUCT(BlueprintType)
struct AKUMASRPGFRAMEWORK_API FARPGQuestObjectiveDefinition
{
    GENERATED_BODY()

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) FName ObjectiveId = NAME_None;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) EARPGQuestObjectiveType Type = EARPGQuestObjectiveType::Custom;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) FText Description;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) FGameplayTag TargetTag;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) FName TargetId = NAME_None;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta=(ClampMin="1")) int32 RequiredCount = 1;
};

USTRUCT(BlueprintType)
struct AKUMASRPGFRAMEWORK_API FARPGQuestReward
{
    GENERATED_BODY()

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) int64 CharacterXP = 0;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) FName CurrencyId = TEXT("Gold");
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) int64 CurrencyAmount = 0;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) TMap<FName, int32> ItemRewards;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) TMap<FName, int32> ReputationRewards;
};

UCLASS(BlueprintType)
class AKUMASRPGFRAMEWORK_API UARPGQuestDefinition : public UARPGDefinitionBase
{
    GENERATED_BODY()
public:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Quest") int32 RequiredLevel = 1;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Quest") TArray<FName> PrerequisiteQuestIds;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Quest") TArray<FARPGQuestObjectiveDefinition> Objectives;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Quest") FARPGQuestReward Rewards;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Quest") bool bRepeatable = false;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Quest") bool bAutoAccept = false;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Quest") bool bAutoComplete = false;
};
