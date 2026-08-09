#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "ARPGTypes.h"
#include "ARPGSlayerComponent.generated.h"

class UARPGSlayerMasterDefinition;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FARPGOnSlayerTaskChanged, const FARPGSlayerTask&, Task);

UCLASS(ClassGroup=(ARPG), meta=(BlueprintSpawnableComponent))
class AKUMASRPGFRAMEWORK_API UARPGSlayerComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UARPGSlayerComponent();
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, SaveGame) FARPGSlayerTask ActiveTask;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, SaveGame) int32 SlayerPoints = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, SaveGame) int32 TaskStreak = 0;
    UPROPERTY(BlueprintAssignable) FARPGOnSlayerTaskChanged OnSlayerTaskChanged;

    UFUNCTION(BlueprintCallable, Category="ARPG|Slayer", meta=(BlueprintAuthorityOnly)) bool RequestTask(const UARPGSlayerMasterDefinition* Master, int32 SlayerLevel);
    UFUNCTION(BlueprintCallable, Category="ARPG|Slayer", meta=(BlueprintAuthorityOnly)) bool RegisterKill(FGameplayTag SlayerCategory);
    UFUNCTION(BlueprintCallable, Category="ARPG|Slayer", meta=(BlueprintAuthorityOnly)) bool CancelTask(int32 PointCost=0);
    UFUNCTION(BlueprintPure, Category="ARPG|Slayer") bool HasActiveTask() const { return !ActiveTask.TaskId.IsNone() && !ActiveTask.bComplete; }
    UFUNCTION(BlueprintCallable, Category="ARPG|Slayer", meta=(BlueprintAuthorityOnly)) void RestoreSlayerState(const FARPGSlayerTask& Task, int32 Points, int32 Streak);

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
