#pragma once
#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "ARPGAbilitySystemComponent.generated.h"
UCLASS(ClassGroup=(AkumaRPG),meta=(BlueprintSpawnableComponent)) class AKUMASRPGFRAMEWORK_API UARPGAbilitySystemComponent:public UAbilitySystemComponent { GENERATED_BODY() public: UARPGAbilitySystemComponent();
UFUNCTION(BlueprintCallable,Category="Akuma RPG|Abilities") void GrantAbilities(const TArray<TSubclassOf<UGameplayAbility>>& Abilities,int32 AbilityLevel=1);
UFUNCTION(BlueprintCallable,Category="Akuma RPG|Abilities") void RemoveAllGrantedAbilities();
private: UPROPERTY() TArray<FGameplayAbilitySpecHandle> FrameworkGrantedHandles; };
