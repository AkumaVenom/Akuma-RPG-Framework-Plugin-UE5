#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "ARPGTypes.h"
#include "Data/ARPGDefinitionBase.h"
#include "ARPGBattlePetDefinition.generated.h"

USTRUCT(BlueprintType)
struct AKUMASRPGFRAMEWORK_API FARPGPetAbilityDefinition
{
    GENERATED_BODY()
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) FName AbilityId = NAME_None;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) FText DisplayName;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) FGameplayTag AbilityTag;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) int32 UnlockLevel = 1;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) float Power = 10.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta=(ClampMin="0.0", ClampMax="1.0")) float Accuracy = 1.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) int32 Priority = 0;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) int32 CooldownTurns = 0;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) EARPGPetAbilityTarget Target = EARPGPetAbilityTarget::Enemy;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) bool bHealing = false;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) FGameplayTag AppliedStatusTag;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta=(ClampMin="0")) int32 StatusTurns = 0;
};

UCLASS(BlueprintType)
class AKUMASRPGFRAMEWORK_API UARPGBattlePetDefinition : public UARPGDefinitionBase
{
    GENERATED_BODY()
public:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Pet") FGameplayTag FamilyTag;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Pet") int32 MaxLevel = 25;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Pet") float BaseHealth = 100.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Pet") float BasePower = 10.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Pet") float BaseSpeed = 10.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Pet") TArray<FARPGPetAbilityDefinition> Abilities;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Pet") TSoftClassPtr<AActor> WorldPetClass;
};
