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

    // Single authoritative authored/base level for both players and NPCs. Set this directly on the
    // inherited Progression component when authoring a character or when doing a simple PIE test.
    // JRPG Stats reads this value automatically at BeginPlay and on every runtime level change.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, SaveGame, Category="Progression|Level", meta=(DisplayName="Base Character Level", ClampMin="1", UIMin="1")) int32 Level = 1;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, SaveGame, Category="Progression|Level", meta=(DisplayName="Current XP", ClampMin="0", UIMin="0")) int64 XP = 0;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Progression|Level", meta=(ClampMin="1", UIMin="1")) int32 MaxLevel = 100;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Progression|XP Curve", meta=(ClampMin="1.0")) float BaseXP = 100.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Progression|XP Curve", meta=(ClampMin="0.01")) float XPExponent = 1.55f;

    // Optional development/PIE convenience. It never changes the JRPG formulas; it simply applies a
    // real authoritative progression level at BeginPlay so every dependent system sees the same level.
    // Leave disabled for normal gameplay/save-driven progression.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Progression|Testing", meta=(DisplayName="Enable Manual Level Override")) bool bEnableManualLevelOverride = false;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Progression|Testing", meta=(EditCondition="bEnableManualLevelOverride", DisplayName="Manual Test Level", ClampMin="1", UIMin="1")) int32 ManualLevelOverride = 1;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Progression|Testing", meta=(EditCondition="bEnableManualLevelOverride", DisplayName="Reset XP When Applied")) bool bResetXPWhenApplyingManualLevel = true;

    UPROPERTY(BlueprintAssignable) FARPGOnLevelChanged OnLevelChanged;
    UPROPERTY(BlueprintAssignable) FARPGOnXPChanged OnXPChanged;

    UFUNCTION(BlueprintCallable, Category="ARPG|Progression", meta=(BlueprintAuthorityOnly)) void AddXP(int64 Amount);
    UFUNCTION(BlueprintPure, Category="ARPG|Progression") int64 GetXPRequiredForLevel(int32 InLevel) const;
    UFUNCTION(BlueprintPure, Category="ARPG|Progression") float GetLevelProgress01() const;
    UFUNCTION(BlueprintCallable, Category="ARPG|Progression", meta=(BlueprintAuthorityOnly)) void SetProgression(int32 NewLevel, int64 NewXP);
    /** Sets only the authoritative base character level and emits OnLevelChanged so JRPG Stats recalculates immediately. */
    UFUNCTION(BlueprintCallable, Category="ARPG|Progression", meta=(BlueprintAuthorityOnly)) void SetLevel(int32 NewLevel, bool bResetCurrentXP = false);
    /** Applies Manual Test Level immediately during PIE/runtime. Intended for development validation, not normal progression. */
    UFUNCTION(BlueprintCallable, CallInEditor, Category="ARPG|Progression|Testing", meta=(BlueprintAuthorityOnly, DisplayName="Apply Manual Test Level Now")) bool ApplyManualLevelOverrideNow();

    virtual void BeginPlay() override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
