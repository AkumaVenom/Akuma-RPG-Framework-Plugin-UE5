#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "GameplayEffectTypes.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "ARPGAbilityBridgeComponent.generated.h"

class UGameplayEffect;

UCLASS(ClassGroup=(ARPG), meta=(BlueprintSpawnableComponent))
class AKUMASRPGFRAMEWORK_API UARPGAbilityBridgeComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="ARPG|Abilities") bool TryActivateAbilityByTag(FGameplayTag AbilityTag);
    UFUNCTION(BlueprintCallable, Category="ARPG|Abilities") bool TryActivateAbilityByTagWithTarget(FGameplayTag AbilityTag, AActor* ExplicitTarget);
    UFUNCTION(BlueprintPure, Category="ARPG|Abilities|Targeting") AActor* GetPreferredAbilityTarget() const;
    UFUNCTION(BlueprintPure, Category="ARPG|Abilities|Targeting") FGameplayAbilityTargetDataHandle MakePreferredAbilityTargetData() const;
    UFUNCTION(BlueprintCallable, Category="ARPG|Abilities", meta=(BlueprintAuthorityOnly)) FActiveGameplayEffectHandle ApplyEffectToOwner(TSubclassOf<UGameplayEffect> EffectClass, float Level=1.f);
    UFUNCTION(BlueprintCallable, Category="ARPG|Abilities", meta=(BlueprintAuthorityOnly)) int32 RemoveEffectsWithGrantedTags(FGameplayTagContainer Tags);
};
