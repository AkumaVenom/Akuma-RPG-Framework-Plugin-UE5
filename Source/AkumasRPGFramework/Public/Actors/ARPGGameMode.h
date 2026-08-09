#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "ARPGGameMode.generated.h"

UCLASS(BlueprintType, Blueprintable)
class AKUMASRPGFRAMEWORK_API AARPGGameMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    AARPGGameMode();
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ARPG|Persistence") bool bAutoLoadPersistentWorld = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ARPG|Persistence") bool bAutoSavePersistentWorld = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ARPG|Persistence") FString PersistentWorldId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ARPG|Persistence", meta=(ClampMin="0.0")) float WorldAutoSaveIntervalSeconds = 180.f;
    UFUNCTION(BlueprintCallable, Category="ARPG|Persistence") bool SavePersistentWorld();
    UFUNCTION(BlueprintCallable, Category="ARPG|Persistence") bool LoadPersistentWorld();
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
private:
    FTimerHandle WorldAutoSaveTimer;
    FString ResolveWorldId() const;
    void HandleAutoLoadWorld();
    void HandleAutoSaveWorld();
};
