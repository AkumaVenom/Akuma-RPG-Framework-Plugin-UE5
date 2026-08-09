#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ARPGQuestGiverComponent.generated.h"

class UARPGQuestDefinition;

UENUM(BlueprintType)
enum class EARPGQuestGiverStatus : uint8
{
    Unavailable,
    Available,
    InProgress,
    ReadyToTurnIn,
    Completed
};

UCLASS(ClassGroup=(ARPG), meta=(BlueprintSpawnableComponent))
class AKUMASRPGFRAMEWORK_API UARPGQuestGiverComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UARPGQuestGiverComponent();

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest Giver") TArray<TObjectPtr<UARPGQuestDefinition>> Quests;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest Giver") bool bAllowTurnIn = true;

    UFUNCTION(BlueprintPure, Category="ARPG|Quest Giver") EARPGQuestGiverStatus GetQuestStatus(AActor* Character, const UARPGQuestDefinition* Quest) const;
    UFUNCTION(BlueprintCallable, Category="ARPG|Quest Giver") TArray<UARPGQuestDefinition*> GetAvailableQuests(AActor* Character) const;
    UFUNCTION(BlueprintCallable, Category="ARPG|Quest Giver", meta=(BlueprintAuthorityOnly)) bool AcceptQuestFor(AActor* Character, UARPGQuestDefinition* Quest);
    UFUNCTION(BlueprintCallable, Category="ARPG|Quest Giver", meta=(BlueprintAuthorityOnly)) bool TurnInQuestFor(AActor* Character, UARPGQuestDefinition* Quest);
    UFUNCTION(BlueprintPure, Category="ARPG|Quest Giver") bool OffersQuest(const UARPGQuestDefinition* Quest) const;
};
