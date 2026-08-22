#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "ARPGGameMode.generated.h"

class AARPGPlayerController;

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

    /** Captured once when the authoritative gameplay world begins. */
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="ARPG|Persistence|Runtime") FString ActiveWorldSaveWorldId;
    /** Standalone/listen-host sessions use the logged-in host account; Guest/dedicated scope is invalid. */
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="ARPG|Persistence|Runtime") FGuid ActiveWorldSaveAccountId;
    /** Exact world slot used by this gameplay session; useful for PIE/network diagnostics. */
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="ARPG|Persistence|Runtime") FString ActiveWorldSaveSlotName;

    /**
     * When enabled, gameplay pawns are not started until their PlayerController has a server-approved
     * local-profile identity. This prevents remote characters from initializing in the listen host's save namespace.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ARPG|Networking|Profile Identity") bool bRequireProfileIdentityBeforeSpawn = true;

    UFUNCTION(BlueprintCallable, Category="ARPG|Persistence") bool SavePersistentWorld();
    UFUNCTION(BlueprintCallable, Category="ARPG|Persistence") bool LoadPersistentWorld();
    /** Called by AARPGPlayerController after the server accepts that connection's profile identity. */
    UFUNCTION(BlueprintCallable, Category="ARPG|Networking", meta=(BlueprintAuthorityOnly)) void NotifyProfileIdentityAccepted(AARPGPlayerController* PlayerController);

    virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;

private:
    FTimerHandle WorldAutoSaveTimer;
    FString ResolveWorldId() const;
    void InitializeWorldPersistenceContext();
    void HandleAutoLoadWorld();
    void HandleAutoSaveWorld();
};
