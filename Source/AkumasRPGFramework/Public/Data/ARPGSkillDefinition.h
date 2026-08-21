#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Curves/CurveFloat.h"
#include "Data/ARPGDefinitionBase.h"
#include "ARPGSkillDefinition.generated.h"

/**
 * Built-in XP requirement models used when XPRequiredPerLevel has no authored keys.
 * Existing Skill Definitions keep FrameworkPower by default; an authored XP curve still takes priority.
 */
UENUM(BlueprintType)
enum class EARPGSkillXPModel : uint8
{
    FrameworkPower UMETA(DisplayName="Framework Power Curve"),
    RuneScapeStyle99 UMETA(DisplayName="RuneScape-Style 1-99 Curve")
};

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
    /** Used only when XP Required Per Level has no keys. Existing assets retain Framework Power Curve behavior. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Skill", meta=(DisplayName="XP Model")) EARPGSkillXPModel XPModel = EARPGSkillXPModel::FrameworkPower;
    /** Optional per-current-level XP requirement curve. Any authored key overrides XP Model for backward compatibility. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Skill") FRuntimeFloatCurve XPRequiredPerLevel;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Skill") TArray<FARPGSkillUnlock> Unlocks;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Skill") FGameplayTagContainer SkillTags;
};
