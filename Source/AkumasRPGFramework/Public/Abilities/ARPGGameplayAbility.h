#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "ARPGGameplayAbility.generated.h"

UENUM(BlueprintType)
enum class EARPGAbilityTargetingPolicy : uint8
{
    IgnoreLockOn UMETA(DisplayName="Ignore Lock-On"),
    PreferLockOn UMETA(DisplayName="Prefer Lock-On Target"),
    RequireLockOn UMETA(DisplayName="Require Lock-On Target")
};

/**
 * Optional first-class Akuma RPG Gameplay Ability base.
 * It exposes the current lock-on target directly to ability Blueprints and can require one before activation.
 */
UCLASS(Blueprintable)
class AKUMASRPGFRAMEWORK_API UARPGGameplayAbility : public UGameplayAbility
{
    GENERATED_BODY()
public:
    UARPGGameplayAbility();

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="ARPG|Targeting")
    EARPGAbilityTargetingPolicy TargetingPolicy = EARPGAbilityTargetingPolicy::PreferLockOn;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="ARPG|Targeting")
    bool bAutoFaceLockOnTarget = true;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="ARPG|Targeting", meta=(ClampMin="0.0"))
    float MaximumLockOnTargetRange = 0.f;

    UFUNCTION(BlueprintPure, Category="ARPG|Ability|Targeting") AActor* GetLockOnTarget() const;
    UFUNCTION(BlueprintPure, Category="ARPG|Ability|Targeting") bool HasValidLockOnTarget() const;
    UFUNCTION(BlueprintPure, Category="ARPG|Ability|Targeting") FVector GetLockOnTargetLocation() const;
    UFUNCTION(BlueprintPure, Category="ARPG|Ability|Targeting") FGameplayAbilityTargetDataHandle MakeLockOnTargetData() const;

    virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle,
                                    const FGameplayAbilityActorInfo* ActorInfo,
                                    const FGameplayTagContainer* SourceTags = nullptr,
                                    const FGameplayTagContainer* TargetTags = nullptr,
                                    FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
};
