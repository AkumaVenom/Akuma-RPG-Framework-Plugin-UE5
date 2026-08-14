#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ARPGPersistenceComponent.generated.h"

UCLASS(ClassGroup=(ARPG), meta=(BlueprintSpawnableComponent))
class AKUMASRPGFRAMEWORK_API UARPGPersistenceComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UARPGPersistenceComponent();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Persistence") bool bAutoLoadOnBeginPlay = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Persistence") bool bAutoSave = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Persistence", meta=(ClampMin="0.0")) float AutoSaveIntervalSeconds = 120.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Persistence") bool bSaveOnEndPlay = true;

    UFUNCTION(BlueprintCallable, Category="ARPG|Persistence") bool SaveNow();
    UFUNCTION(BlueprintCallable, Category="ARPG|Persistence") bool LoadNow();

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
protected:
    FTimerHandle AutoSaveTimer;
    bool bDeferredGuestIdentityRecoveryOnce = false;
    void AttemptAutoLoad();
    void HandleAutoSave();
};
