#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ARPGTypes.h"
#include "ARPGDungeonManager.generated.h"

class UARPGDungeonDefinition;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FARPGEncounterStateChanged, FName, EncounterId, EARPGEncounterState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FARPGDungeonEvent);

UCLASS(BlueprintType, Blueprintable)
class AKUMASRPGFRAMEWORK_API AARPGDungeonManager : public AActor
{
    GENERATED_BODY()
public:
    AARPGDungeonManager();

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dungeon") TObjectPtr<UARPGDungeonDefinition> Definition;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, ReplicatedUsing=OnRep_Encounters, SaveGame, Category="Dungeon") TArray<FARPGEncounterRuntime> Encounters;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category="Dungeon") int32 CurrentCheckpoint = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category="Dungeon") bool bDungeonComplete = false;
    UPROPERTY(BlueprintAssignable) FARPGEncounterStateChanged OnEncounterStateChanged;
    UPROPERTY(BlueprintAssignable) FARPGDungeonEvent OnDungeonCompleted;
    UPROPERTY(BlueprintAssignable) FARPGDungeonEvent OnWipe;

    UFUNCTION(BlueprintCallable, Category="ARPG|Dungeon", meta=(BlueprintAuthorityOnly)) void InitializeFromDefinition();
    UFUNCTION(BlueprintCallable, Category="ARPG|Dungeon", meta=(BlueprintAuthorityOnly)) bool SetEncounterState(FName EncounterId, EARPGEncounterState NewState);
    UFUNCTION(BlueprintPure, Category="ARPG|Dungeon") EARPGEncounterState GetEncounterState(FName EncounterId) const;
    UFUNCTION(BlueprintCallable, Category="ARPG|Dungeon", meta=(BlueprintAuthorityOnly)) void RegisterWipe(FName EncounterId);
    UFUNCTION(BlueprintCallable, Category="ARPG|Dungeon", meta=(BlueprintAuthorityOnly)) void UnlockCheckpoint(int32 CheckpointIndex);
    UFUNCTION(BlueprintPure, Category="ARPG|Dungeon") bool AreRequiredEncountersComplete() const;
    UFUNCTION(BlueprintCallable, Category="ARPG|Dungeon", meta=(BlueprintAuthorityOnly)) void RestoreEncounterProgress(const TArray<FARPGEncounterRuntime>& NewProgress);

    virtual void BeginPlay() override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
protected:
    UFUNCTION() void OnRep_Encounters();
};
