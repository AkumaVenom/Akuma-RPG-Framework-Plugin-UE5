#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Data/ARPGDefinitionBase.h"
#include "ARPGSlayerDefinition.generated.h"

USTRUCT(BlueprintType)
struct AKUMASRPGFRAMEWORK_API FARPGSlayerTaskOption
{
    GENERATED_BODY()
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) FName TaskId = NAME_None;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) FGameplayTag TargetCategory;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) int32 MinimumSlayerLevel = 1;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) int32 MinKills = 10;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) int32 MaxKills = 20;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) int32 XPPerKill = 10;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) int32 CompletionPoints = 1;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) float Weight = 1.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) bool bBossTask = false;
};

UCLASS(BlueprintType)
class AKUMASRPGFRAMEWORK_API UARPGSlayerMasterDefinition : public UARPGDefinitionBase
{
    GENERATED_BODY()
public:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Slayer") int32 MinimumCombatLevel = 1;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Slayer") TArray<FARPGSlayerTaskOption> Tasks;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Slayer") int32 FreeRerollsPerStreak = 0;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Slayer") int32 CancelPointCost = 0;
};
