#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ARPGProgressionComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FARPGOnLevelChanged, int32, OldLevel, int32, NewLevel);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FARPGOnXPChanged, int64, OldXP, int64, NewXP);

UCLASS(ClassGroup=(ARPG), meta=(BlueprintSpawnableComponent))
class AKUMASRPGFRAMEWORK_API UARPGProgressionComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UARPGProgressionComponent();

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, SaveGame, Category="Progression") int32 Level = 1;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, SaveGame, Category="Progression") int64 XP = 0;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Progression") int32 MaxLevel = 100;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Progression") float BaseXP = 100.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Progression") float XPExponent = 1.55f;

    UPROPERTY(BlueprintAssignable) FARPGOnLevelChanged OnLevelChanged;
    UPROPERTY(BlueprintAssignable) FARPGOnXPChanged OnXPChanged;

    UFUNCTION(BlueprintCallable, Category="ARPG|Progression", meta=(BlueprintAuthorityOnly)) void AddXP(int64 Amount);
    UFUNCTION(BlueprintPure, Category="ARPG|Progression") int64 GetXPRequiredForLevel(int32 InLevel) const;
    UFUNCTION(BlueprintPure, Category="ARPG|Progression") float GetLevelProgress01() const;
    UFUNCTION(BlueprintCallable, Category="ARPG|Progression", meta=(BlueprintAuthorityOnly)) void SetProgression(int32 NewLevel, int64 NewXP);

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
