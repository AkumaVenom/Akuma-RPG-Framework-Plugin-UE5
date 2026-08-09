#pragma once
#include "CoreMinimal.h"
#include "ARPGTypes.h"
#include "Data/ARPGDefinitionBase.h"
#include "ARPGMountDefinition.generated.h"

UCLASS(BlueprintType)
class AKUMASRPGFRAMEWORK_API UARPGMountDefinition : public UARPGDefinitionBase
{
    GENERATED_BODY()
public:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Mount") TSoftClassPtr<APawn> MountPawnClass;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Mount") EARPGMountMovementType MovementType = EARPGMountMovementType::Ground;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Mount", meta=(ClampMin="0.0")) float SpeedMultiplier = 1.6f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Mount") bool bAllowCombat = false;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Mount") bool bAllowGathering = false;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Mount") bool bAllowIndoors = false;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Mount") TSoftObjectPtr<UAnimMontage> MountMontage;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Mount") TSoftObjectPtr<UAnimMontage> DismountMontage;
};
