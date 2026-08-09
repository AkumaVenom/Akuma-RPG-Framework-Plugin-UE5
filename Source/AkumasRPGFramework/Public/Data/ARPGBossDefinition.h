#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "ARPGTypes.h"
#include "Data/ARPGDefinitionBase.h"
#include "ARPGBossDefinition.generated.h"

USTRUCT(BlueprintType)
struct AKUMASRPGFRAMEWORK_API FARPGBossPhaseDefinition
{
    GENERATED_BODY()
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) FName PhaseId = NAME_None;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta=(ClampMin="0.0", ClampMax="1.0")) float StartsAtHealthPercent = 1.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) FGameplayTagContainer GrantedPhaseTags;
};

UCLASS(BlueprintType)
class AKUMASRPGFRAMEWORK_API UARPGBossDefinition : public UARPGDefinitionBase
{
    GENERATED_BODY()
public:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Boss") EARPGBossType BossType = EARPGBossType::Elite;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Boss") bool bWorldBoss = false;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Boss") bool bScaleWithPlayers = false;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Boss") int32 MaxScalingPlayers = 40;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Boss") float EnrageSeconds = 0.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Boss") float LeashDistance = 15000.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Boss") float MinWorldRespawnSeconds = 0.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Boss") float MaxWorldRespawnSeconds = 0.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Boss", meta=(ClampMin="0.0", ClampMax="1.0")) float MinimumContributionPercent = 0.01f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Boss") bool bResetHealthOnLeash = true;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Boss") TArray<FARPGBossPhaseDefinition> Phases;
};
