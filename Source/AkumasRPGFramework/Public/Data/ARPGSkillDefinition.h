#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Curves/CurveFloat.h"
#include "Data/ARPGDefinitionBase.h"
#include "ARPGSkillDefinition.generated.h"

USTRUCT(BlueprintType)
struct AKUMASRPGFRAMEWORK_API FARPGSkillUnlock
{
    GENERATED_BODY()
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) int32 RequiredLevel = 1;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) FGameplayTag UnlockTag;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) FText UnlockDescription;
};

UCLASS(BlueprintType)
class AKUMASRPGFRAMEWORK_API UARPGSkillDefinition : public UARPGDefinitionBase
{
    GENERATED_BODY()
public:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Skill", meta=(ClampMin="1")) int32 MaxLevel = 99;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Skill") FRuntimeFloatCurve XPRequiredPerLevel;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Skill") TArray<FARPGSkillUnlock> Unlocks;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Skill") FGameplayTagContainer SkillTags;
};
