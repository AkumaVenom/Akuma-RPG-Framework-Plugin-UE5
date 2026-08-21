#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ARPGPersistenceComponent.generated.h"

UENUM(BlueprintType)
enum class EARPGInitialCharacterPersistenceState : uint8
{
    Unresolved UMETA(DisplayName="Unresolved"),
    AutoLoadDisabled UMETA(DisplayName="Auto Load Disabled"),
    FreshCharacterNoSave UMETA(DisplayName="Fresh Character - No Save"),
    LoadedExistingSave UMETA(DisplayName="Loaded Existing Save"),
    ExistingSaveLoadFailed UMETA(DisplayName="Existing Save - Load Failed")
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FARPGInitialCharacterPersistenceResolved, bool, bLoadedExistingSave, bool, bExistingSaveFound);

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

    /**
     * Automatically persists authoritative Inventory / equipment / Quick Access mutations.
     * Changes are debounced into one complete character snapshot, so rapid transfers, mining rewards
     * and hotbar edits do not issue a disk write for every individual event.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Persistence|Automatic State Save", meta=(DisplayName="Save Inventory And Quick Access Changes Automatically")) bool bAutoSaveCharacterStateChanges = true;
    /** Quiet period after the most recent state mutation before the automatic character snapshot is committed. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Persistence|Automatic State Save", meta=(ClampMin="0.05", UIMin="0.05", Units="s", EditCondition="bAutoSaveCharacterStateChanges")) float CharacterStateSaveDebounceSeconds = 1.5f;

    /** True while an Inventory / Quick Access mutation is waiting for its debounced automatic save. */
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Persistence|Runtime") bool bCharacterStateSavePending = false;
    /** Number of successful automatic character-state commits made by this component this session. */
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Persistence|Runtime") int32 AutomaticCharacterStateSaveCount = 0;

    /** Explicit result of the BeginPlay persistence bootstrap. This state always leaves Unresolved on authority. */
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Persistence|Runtime") EARPGInitialCharacterPersistenceState InitialCharacterPersistenceState = EARPGInitialCharacterPersistenceState::Unresolved;
    /** True after the automatic BeginPlay character-load decision has completed. */
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Persistence|Runtime") bool bInitialAutoLoadResolved = false;
    /** True only when the initial automatic load successfully restored an existing character save. */
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Persistence|Runtime") bool bLoadedExistingCharacterSave = false;
    /** True when a character save slot existed, even if loading it failed. Existing saves suppress starter seeding. */
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Persistence|Runtime") bool bInitialCharacterSaveFound = false;
    /** Automatic saves are suppressed after a detected existing save fails to load, preventing destructive overwrite. */
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Persistence|Runtime") bool bAutomaticSaveSuppressedAfterLoadFailure = false;
    /** Stable CharacterId used for the initial save lookup/creation. Useful when diagnosing project-side PIE identity. */
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Persistence|Runtime") FGuid InitialResolvedCharacterId;
    /** Exact character slot used for the initial lookup/creation. */
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Persistence|Runtime") FString InitialResolvedCharacterSaveSlot;
    UPROPERTY(BlueprintAssignable, Category="Persistence|Runtime") FARPGInitialCharacterPersistenceResolved OnInitialCharacterPersistenceResolved;

    /**
     * Writes a complete character snapshot synchronously. The ready automatic persistence path deliberately
     * serializes character writes so an older async snapshot can never finish after a newer Inventory state.
     */
    UFUNCTION(BlueprintCallable, Category="ARPG|Persistence") bool SaveNow();
    /** Explicit alias for the same synchronous character commit used by automatic persistence. */
    UFUNCTION(BlueprintCallable, Category="ARPG|Persistence") bool SaveNowImmediate();
    /** Immediately commits any pending debounced Inventory / Quick Access save. */
    UFUNCTION(BlueprintCallable, Category="ARPG|Persistence") bool FlushPendingCharacterStateSave();
    UFUNCTION(BlueprintCallable, Category="ARPG|Persistence") bool LoadNow();
    UFUNCTION(BlueprintPure, Category="ARPG|Persistence") bool HasInitialAutoLoadResolved() const { return bInitialAutoLoadResolved; }
    UFUNCTION(BlueprintPure, Category="ARPG|Persistence") bool DidInitialCharacterSaveExist() const { return bInitialCharacterSaveFound; }
    UFUNCTION(BlueprintPure, Category="ARPG|Persistence") EARPGInitialCharacterPersistenceState GetInitialCharacterPersistenceState() const { return InitialCharacterPersistenceState; }

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
protected:
    FTimerHandle AutoSaveTimer;
    FTimerHandle CharacterStateSaveTimer;
    bool bDeferredGuestIdentityRecoveryOnce = false;
    bool bPersistenceDelegatesBound = false;

    void AttemptAutoLoad();
    void ResolveInitialAutoLoad(EARPGInitialCharacterPersistenceState NewState);
    void HandleAutoSave();
    void BindAutomaticStateSaveDelegates();
    void UnbindAutomaticStateSaveDelegates();
    void ScheduleCharacterStateSave();
    void HandleCharacterStateSaveTimer();

    UFUNCTION() void HandleInventoryChangedForPersistence();
    UFUNCTION() void HandleQuickAccessChangedForPersistence();
    UFUNCTION() void HandleActiveQuickAccessChangedForPersistence(int32 SlotNumber, FName ItemId, FGuid ItemInstanceId);
};
