#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "ARPGWandererComponent.generated.h"

UCLASS(ClassGroup=(ARPG), meta=(BlueprintSpawnableComponent))
class AKUMASRPGFRAMEWORK_API UARPGWandererComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UARPGWandererComponent();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wanderer") bool bEnabled = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wanderer") bool bStayNearHome = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wanderer", meta=(ClampMin="100")) float WanderRadius = 5000.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wanderer", meta=(ClampMin="0.1")) float ThinkInterval = 5.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wanderer") FGameplayTagContainer ActivityTags;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Wanderer") FVector HomeLocation = FVector::ZeroVector;

    UFUNCTION(BlueprintCallable, Category="ARPG|Wanderer") void SetWandererEnabled(bool bNewEnabled);
    UFUNCTION(BlueprintCallable, Category="ARPG|Wanderer") void ForceChooseNewDestination();

    virtual void BeginPlay() override;
protected:
    FTimerHandle ThinkTimer;
    void Think();
};
