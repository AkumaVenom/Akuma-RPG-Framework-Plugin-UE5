#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "ARPGTypes.h"
#include "ARPGQuestComponent.generated.h"

class UARPGQuestDefinition;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FARPGOnQuestChanged, FName, QuestId);

UCLASS(ClassGroup=(ARPG), meta=(BlueprintSpawnableComponent))
class AKUMASRPGFRAMEWORK_API UARPGQuestComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UARPGQuestComponent();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, ReplicatedUsing=OnRep_Quests, SaveGame) TArray<FARPGQuestRuntime> Quests;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quests") int32 MaxActiveQuests = 35;
    UPROPERTY(BlueprintAssignable) FARPGOnQuestChanged OnQuestChanged;

    UFUNCTION(BlueprintCallable, Category="ARPG|Quest", meta=(BlueprintAuthorityOnly)) bool AcceptQuest(const UARPGQuestDefinition* Quest);
    UFUNCTION(BlueprintPure, Category="ARPG|Quest") bool CanAcceptQuest(const UARPGQuestDefinition* Quest) const;
    UFUNCTION(BlueprintCallable, Category="ARPG|Quest", meta=(BlueprintAuthorityOnly)) int32 ProgressByTag(EARPGQuestObjectiveType Type, FGameplayTag TargetTag, int32 Amount=1);
    UFUNCTION(BlueprintCallable, Category="ARPG|Quest", meta=(BlueprintAuthorityOnly)) int32 ProgressById(EARPGQuestObjectiveType Type, FName TargetId, int32 Amount=1);
    UFUNCTION(BlueprintCallable, Category="ARPG|Quest", meta=(BlueprintAuthorityOnly)) int32 SetProgressByIdAtLeast(EARPGQuestObjectiveType Type, FName TargetId, int32 AbsoluteValue);
    UFUNCTION(BlueprintCallable, Category="ARPG|Quest", meta=(BlueprintAuthorityOnly)) bool CompleteQuest(FName QuestId);
    UFUNCTION(BlueprintPure, Category="ARPG|Quest") bool IsQuestComplete(FName QuestId) const;
    UFUNCTION(BlueprintPure, Category="ARPG|Quest") bool HasActiveQuest(FName QuestId) const;
    const UARPGQuestDefinition* ResolveQuestDefinition(FName QuestId) const;
    UFUNCTION(BlueprintCallable, Category="ARPG|Quest", meta=(BlueprintAuthorityOnly)) void ReplaceQuests(const TArray<FARPGQuestRuntime>& NewQuests);

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
protected:
    UFUNCTION() void OnRep_Quests();
    int32 GetActiveQuestCount() const;
    void ReevaluateQuest(FARPGQuestRuntime& Runtime);
    bool GrantRewards(const UARPGQuestDefinition* Quest);
};
