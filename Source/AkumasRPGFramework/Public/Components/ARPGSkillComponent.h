#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ARPGTypes.h"
#include "Data/ARPGSkillDefinition.h"
#include "ARPGSkillComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FARPGOnSkillChanged, FName, SkillId, int32, Level, int64, XP);

UCLASS(ClassGroup=(ARPG), meta=(BlueprintSpawnableComponent))
class AKUMASRPGFRAMEWORK_API UARPGSkillComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UARPGSkillComponent();
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, ReplicatedUsing=OnRep_Skills, SaveGame) TArray<FARPGSkillState> Skills;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Skills") int32 DefaultMaxLevel = 99;
    UPROPERTY(BlueprintAssignable) FARPGOnSkillChanged OnSkillChanged;

    UFUNCTION(BlueprintCallable, Category="ARPG|Skills", meta=(BlueprintAuthorityOnly)) void AddSkillXP(FName SkillId, int64 Amount);
    UFUNCTION(BlueprintCallable, Category="ARPG|Skills", meta=(BlueprintAuthorityOnly)) void AddSkillXPFromDefinition(const UARPGSkillDefinition* Skill, int64 Amount);
    /** Adds XP using a built-in model without requiring a Skill Definition asset. Useful for native profession defaults. */
    UFUNCTION(BlueprintCallable, Category="ARPG|Skills", meta=(BlueprintAuthorityOnly)) void AddSkillXPWithModel(FName SkillId, int64 Amount, int32 MaxLevel, EARPGSkillXPModel XPModel);
    UFUNCTION(BlueprintPure, Category="ARPG|Skills") int32 GetSkillLevel(FName SkillId) const;
    UFUNCTION(BlueprintPure, Category="ARPG|Skills") int64 GetSkillXP(FName SkillId) const;
    UFUNCTION(BlueprintPure, Category="ARPG|Skills") int64 GetXPForNextLevel(int32 Level) const;
    UFUNCTION(BlueprintPure, Category="ARPG|Skills") int64 GetXPForNextLevelForModel(int32 Level, EARPGSkillXPModel XPModel) const;
    UFUNCTION(BlueprintPure, Category="ARPG|Skills") int64 GetXPForNextLevelFromDefinition(const UARPGSkillDefinition* Skill, int32 Level) const;
    /** Cumulative RuneScape-style XP threshold required to have reached Level (Level 1 = 0 XP). */
    UFUNCTION(BlueprintPure, Category="ARPG|Skills") int64 GetRuneScapeStyleTotalXPForLevel(int32 Level) const;
    UFUNCTION(BlueprintCallable, Category="ARPG|Skills", meta=(BlueprintAuthorityOnly)) void ReplaceSkills(const TArray<FARPGSkillState>& NewSkills);

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
protected:
    UFUNCTION() void OnRep_Skills();
    void AddSkillXPInternal(FName SkillId, int64 Amount, int32 MaxLevel, const UARPGSkillDefinition* Definition, EARPGSkillXPModel FallbackModel = EARPGSkillXPModel::FrameworkPower);
};
