#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ARPGSlayerMasterComponent.generated.h"

class UARPGSlayerMasterDefinition;

UCLASS(ClassGroup=(ARPG), meta=(BlueprintSpawnableComponent))
class AKUMASRPGFRAMEWORK_API UARPGSlayerMasterComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UARPGSlayerMasterComponent();

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Slayer Master") TObjectPtr<UARPGSlayerMasterDefinition> MasterDefinition;

    UFUNCTION(BlueprintPure, Category="ARPG|Slayer Master") bool CanAssignTaskTo(AActor* Character) const;
    UFUNCTION(BlueprintCallable, Category="ARPG|Slayer Master", meta=(BlueprintAuthorityOnly)) bool RequestTaskFor(AActor* Character);
    UFUNCTION(BlueprintCallable, Category="ARPG|Slayer Master", meta=(BlueprintAuthorityOnly)) bool CancelTaskFor(AActor* Character);
};
