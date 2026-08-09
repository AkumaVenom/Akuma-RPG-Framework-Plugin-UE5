#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "ARPGTypes.h"
#include "Data/ARPGDefinitionBase.h"
#include "ARPGClassDefinition.generated.h"

UCLASS(BlueprintType)
class AKUMASRPGFRAMEWORK_API UARPGClassDefinition : public UARPGDefinitionBase
{
    GENERATED_BODY()
public:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Class") EARPGGroupRole PreferredRole = EARPGGroupRole::Damage;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Class") FGameplayTagContainer ClassTags;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Class") TArray<TSubclassOf<class UGameplayAbility>> GrantedAbilities;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Class") FARPGCombatMontageSet AnimationSet;
};
