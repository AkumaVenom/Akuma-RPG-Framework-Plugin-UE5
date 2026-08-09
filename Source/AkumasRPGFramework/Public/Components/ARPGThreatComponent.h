#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ARPGTypes.h"
#include "ARPGThreatComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FARPGThreatTargetChanged, AActor*, NewTarget);

UCLASS(ClassGroup=(ARPG), meta=(BlueprintSpawnableComponent))
class AKUMASRPGFRAMEWORK_API UARPGThreatComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UARPGThreatComponent();
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Threat") TArray<FARPGThreatEntry> ThreatTable;
    UPROPERTY(BlueprintAssignable) FARPGThreatTargetChanged OnHighestThreatTargetChanged;

    UFUNCTION(BlueprintCallable, Category="ARPG|Threat", meta=(BlueprintAuthorityOnly)) void AddThreat(AActor* Actor, float Amount);
    UFUNCTION(BlueprintCallable, Category="ARPG|Threat", meta=(BlueprintAuthorityOnly)) void SetThreat(AActor* Actor, float Amount);
    UFUNCTION(BlueprintCallable, Category="ARPG|Threat", meta=(BlueprintAuthorityOnly)) void Taunt(AActor* Actor, float BonusThreat=1.f);
    UFUNCTION(BlueprintCallable, Category="ARPG|Threat", meta=(BlueprintAuthorityOnly)) void RemoveActor(AActor* Actor);
    UFUNCTION(BlueprintCallable, Category="ARPG|Threat", meta=(BlueprintAuthorityOnly)) void ClearThreat();
    UFUNCTION(BlueprintPure, Category="ARPG|Threat") float GetThreat(AActor* Actor) const;
    UFUNCTION(BlueprintPure, Category="ARPG|Threat") AActor* GetHighestThreatActor() const;
private:
    TWeakObjectPtr<AActor> LastHighest;
    void NotifyIfHighestChanged();
};
