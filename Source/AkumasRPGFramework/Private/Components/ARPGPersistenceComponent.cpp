#include "Components/ARPGPersistenceComponent.h"
#include "Subsystems/ARPGSaveSubsystem.h"
#include "Subsystems/ARPGAccountSubsystem.h"
#include "Actors/ARPGCharacter.h"
#include "Actors/ARPGAICharacter.h"
#include "Components/ARPGInventoryComponent.h"
#include "Components/ARPGQuickAccessComponent.h"
#include "AkumasRPGFramework.h"
#include "Engine/GameInstance.h"
#include "TimerManager.h"

namespace
{
    /**
     * Account character persistence belongs to the playable character only. AARPGAICharacter inherits
     * AARPGCharacter (and therefore this component), but an NPC must never consume the account's
     * LastCharacterId or read/write the player's character save slot. Doing so makes spawned/reloaded
     * enemies inherit player faction/state, which simultaneously breaks hostile lock-on and damage.
     */
    bool ARPGIsAccountCharacterPersistenceOwner(const AActor* Owner)
    {
        const AARPGCharacter* Character = Cast<AARPGCharacter>(Owner);
        if (!Character) return false;

        // Normal player characters are always eligible. AARPGAICharacter remains excluded while it is
        // actually AI-controlled, but a project is allowed to derive a playable pawn from that class and
        // possess it with a PlayerController. Class inheritance alone must never deadlock persistence.
        return !Character->IsA<AARPGAICharacter>() || Character->IsPlayerControlled();
    }
}

UARPGPersistenceComponent::UARPGPersistenceComponent() { PrimaryComponentTick.bCanEverTick = false; }

void UARPGPersistenceComponent::BeginPlay()
{
    Super::BeginPlay();
    if (!GetOwner() || (GetOwner()->GetNetMode() == NM_Client && !GetOwner()->HasAuthority())) return;
    if (!Cast<AARPGCharacter>(GetOwner())) return;

    // Bind before the bootstrap decision. Load-time Inventory/QuickAccess broadcasts are ignored until
    // the state leaves Unresolved; a fresh-character starter grant is then allowed to become dirty state.
    // Do NOT gate BeginPlay on the actor's exact native class. Projects may use a player-controlled subclass
    // of AARPGAICharacter; the previous hard class exclusion left Inventory waiting forever when Auto Load
    // was enabled because Persistence returned before resolving the startup transaction.
    BindAutomaticStateSaveDelegates();

    if (bAutoLoadOnBeginPlay && GetWorld())
    {
        GetWorld()->GetTimerManager().SetTimerForNextTick(this, &UARPGPersistenceComponent::AttemptAutoLoad);
    }
    else
    {
        InitialCharacterPersistenceState = EARPGInitialCharacterPersistenceState::AutoLoadDisabled;
        bInitialAutoLoadResolved = true;
    }

    if (bAutoSave && AutoSaveIntervalSeconds > 0.f && GetWorld()) GetWorld()->GetTimerManager().SetTimer(AutoSaveTimer, this, &UARPGPersistenceComponent::HandleAutoSave, AutoSaveIntervalSeconds, true);
}

void UARPGPersistenceComponent::AttemptAutoLoad()
{
    AARPGCharacter* Character = Cast<AARPGCharacter>(GetOwner());
    if (!Character || !Character->HasAuthority()) return;

    // A playable project pawn may inherit from AARPGAICharacter. Wait one extra frame for possession before
    // deciding it is truly AI-owned. Native AI actors have auto-load disabled by their constructor and never
    // normally enter this branch. Even if a project enables it accidentally, resolve the transaction instead
    // of leaving Inventory permanently blocked waiting for persistence.
    if (!ARPGIsAccountCharacterPersistenceOwner(Character))
    {
        if (!bDeferredGuestIdentityRecoveryOnce && GetWorld())
        {
            bDeferredGuestIdentityRecoveryOnce = true;
            GetWorld()->GetTimerManager().SetTimerForNextTick(this, &UARPGPersistenceComponent::AttemptAutoLoad);
            return;
        }

        UE_LOG(LogARPG, Warning, TEXT("Character persistence auto-load is enabled for '%s' but the pawn is not an account-player persistence owner. Resolving as fresh runtime state so Starting Items are not deadlocked."), *GetNameSafe(Character));
        ResolveInitialAutoLoad(EARPGInitialCharacterPersistenceState::FreshCharacterNoSave);
        return;
    }

    // Resolve one stable account/Guest CharacterId before touching the character slot.
    if (UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr)
    {
        if (UARPGAccountSubsystem* Accounts = GI->GetSubsystem<UARPGAccountSubsystem>())
        {
            const FGuid Last = Accounts->GetLastCharacterId();
            if (Last.IsValid())
            {
                Character->CharacterId = Last;
            }
            else if (!Accounts->IsLoggedIn())
            {
                // Legacy Guest saves (pre-v2.15.12) may need the GameMode/world loader to recover their
                // previous owner id. Give that recovery exactly one frame before creating/registering a new id.
                if (!bDeferredGuestIdentityRecoveryOnce && GetWorld())
                {
                    bDeferredGuestIdentityRecoveryOnce = true;
                    GetWorld()->GetTimerManager().SetTimerForNextTick(this, &UARPGPersistenceComponent::AttemptAutoLoad);
                    return;
                }
                Character->EnsureCharacterId();
                Accounts->RegisterCharacterId(Character->CharacterId);
            }
        }
    }

    Character->EnsureCharacterId();
    InitialResolvedCharacterId = Character->CharacterId;

    UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
    UARPGSaveSubsystem* Saves = GI ? GI->GetSubsystem<UARPGSaveSubsystem>() : nullptr;
    if (!Saves || !InitialResolvedCharacterId.IsValid())
    {
        // This is not an "existing save failed" case because no concrete existing slot was proven. Treat it
        // as a fresh runtime character so designer Starting Items are never silently lost.
        UE_LOG(LogARPG, Warning, TEXT("Character persistence could not resolve a SaveSubsystem/CharacterId for '%s'. Treating this session as fresh so Starting Items can initialize."), *GetNameSafe(Character));
        ResolveInitialAutoLoad(EARPGInitialCharacterPersistenceState::FreshCharacterNoSave);
        return;
    }

    InitialResolvedCharacterSaveSlot = Saves->MakeCharacterSlotName(InitialResolvedCharacterId);
    const bool bExistingSaveFound = Saves->DoesCharacterSaveExist(InitialResolvedCharacterId);

    // Distinguish "no save exists" from "a save exists but failed to load" BEFORE calling LoadCharacter.
    // A fresh character must always receive its authored Starting Items; only a proven existing-but-unreadable
    // save suppresses defaults to avoid destructive overwrite.
    if (!bExistingSaveFound)
    {
        ResolveInitialAutoLoad(EARPGInitialCharacterPersistenceState::FreshCharacterNoSave);

        // Starting Items + their initial Quick Access assignments have been applied synchronously by the
        // resolve call above. If any automatic persistence mode is enabled, establish the first character
        // snapshot immediately rather than waiting for a later timer or PIE teardown.
        if ((bAutoSaveCharacterStateChanges || bAutoSave || bSaveOnEndPlay) && !bAutomaticSaveSuppressedAfterLoadFailure)
        {
            SaveNowImmediate();
        }
        return;
    }

    if (LoadNow())
    {
        ResolveInitialAutoLoad(EARPGInitialCharacterPersistenceState::LoadedExistingSave);
    }
    else
    {
        ResolveInitialAutoLoad(EARPGInitialCharacterPersistenceState::ExistingSaveLoadFailed);
    }
}

void UARPGPersistenceComponent::ResolveInitialAutoLoad(EARPGInitialCharacterPersistenceState NewState)
{
    if (bInitialAutoLoadResolved) return;

    InitialCharacterPersistenceState = NewState;
    bInitialAutoLoadResolved = true;
    bLoadedExistingCharacterSave = NewState == EARPGInitialCharacterPersistenceState::LoadedExistingSave;
    bInitialCharacterSaveFound = NewState == EARPGInitialCharacterPersistenceState::LoadedExistingSave
        || NewState == EARPGInitialCharacterPersistenceState::ExistingSaveLoadFailed;
    bAutomaticSaveSuppressedAfterLoadFailure = NewState == EARPGInitialCharacterPersistenceState::ExistingSaveLoadFailed;

    if (bAutomaticSaveSuppressedAfterLoadFailure)
    {
        UE_LOG(LogARPG, Error, TEXT("Character persistence found existing slot '%s' for '%s' but could not load it. Starting Items and automatic writes are suppressed to protect the existing save."), *InitialResolvedCharacterSaveSlot, *GetNameSafe(GetOwner()));
    }
    else if (NewState == EARPGInitialCharacterPersistenceState::FreshCharacterNoSave)
    {
        UE_LOG(LogARPG, Log, TEXT("Character persistence found no existing save for '%s' (CharacterId=%s, Slot='%s'). Applying authored Starting Items as first-character state."), *GetNameSafe(GetOwner()), *InitialResolvedCharacterId.ToString(EGuidFormats::DigitsWithHyphens), *InitialResolvedCharacterSaveSlot);
    }

    if (UARPGInventoryComponent* Inventory = GetOwner() ? GetOwner()->FindComponentByClass<UARPGInventoryComponent>() : nullptr)
    {
        // Only an actually loaded/existing save owns Inventory initialization. Fresh/no-save state must seed
        // the designer-authored defaults even when automatic persistence is enabled.
        Inventory->ResolveStartingItemsAfterInitialPersistence(bInitialCharacterSaveFound);
    }

    OnInitialCharacterPersistenceResolved.Broadcast(bLoadedExistingCharacterSave, bInitialCharacterSaveFound);
}

void UARPGPersistenceComponent::HandleAutoSave()
{
    // Never allow an automatic write to race ahead of the initial load decision. This also protects
    // unusually short project-authored autosave intervals during startup.
    if (bAutoLoadOnBeginPlay && !bInitialAutoLoadResolved) return;
    if (!bAutomaticSaveSuppressedAfterLoadFailure) SaveNow();
}

void UARPGPersistenceComponent::BindAutomaticStateSaveDelegates()
{
    if (bPersistenceDelegatesBound || !GetOwner() || !GetOwner()->HasAuthority()) return;

    if (UARPGInventoryComponent* Inventory = GetOwner()->FindComponentByClass<UARPGInventoryComponent>())
        Inventory->OnInventoryChanged.AddDynamic(this, &UARPGPersistenceComponent::HandleInventoryChangedForPersistence);

    if (UARPGQuickAccessComponent* QuickAccess = GetOwner()->FindComponentByClass<UARPGQuickAccessComponent>())
    {
        QuickAccess->OnQuickAccessChanged.AddDynamic(this, &UARPGPersistenceComponent::HandleQuickAccessChangedForPersistence);
        QuickAccess->OnActiveQuickAccessSlotChanged.AddDynamic(this, &UARPGPersistenceComponent::HandleActiveQuickAccessChangedForPersistence);
    }

    bPersistenceDelegatesBound = true;
}

void UARPGPersistenceComponent::UnbindAutomaticStateSaveDelegates()
{
    if (!bPersistenceDelegatesBound || !GetOwner()) return;

    if (UARPGInventoryComponent* Inventory = GetOwner()->FindComponentByClass<UARPGInventoryComponent>())
        Inventory->OnInventoryChanged.RemoveDynamic(this, &UARPGPersistenceComponent::HandleInventoryChangedForPersistence);

    if (UARPGQuickAccessComponent* QuickAccess = GetOwner()->FindComponentByClass<UARPGQuickAccessComponent>())
    {
        QuickAccess->OnQuickAccessChanged.RemoveDynamic(this, &UARPGPersistenceComponent::HandleQuickAccessChangedForPersistence);
        QuickAccess->OnActiveQuickAccessSlotChanged.RemoveDynamic(this, &UARPGPersistenceComponent::HandleActiveQuickAccessChangedForPersistence);
    }

    bPersistenceDelegatesBound = false;
}

void UARPGPersistenceComponent::ScheduleCharacterStateSave()
{
    if (!bAutoSaveCharacterStateChanges || !ARPGIsAccountCharacterPersistenceOwner(GetOwner()) || !GetOwner()->HasAuthority()) return;
    if (bAutoLoadOnBeginPlay && !bInitialAutoLoadResolved) return;
    if (bAutomaticSaveSuppressedAfterLoadFailure) return;

    bCharacterStateSavePending = true;
    if (!GetWorld()) return;

    GetWorld()->GetTimerManager().ClearTimer(CharacterStateSaveTimer);
    const float Delay = FMath::Max(0.05f, CharacterStateSaveDebounceSeconds);
    GetWorld()->GetTimerManager().SetTimer(CharacterStateSaveTimer, this, &UARPGPersistenceComponent::HandleCharacterStateSaveTimer, Delay, false);
}

void UARPGPersistenceComponent::HandleCharacterStateSaveTimer()
{
    FlushPendingCharacterStateSave();
}

void UARPGPersistenceComponent::HandleInventoryChangedForPersistence()
{
    ScheduleCharacterStateSave();
}

void UARPGPersistenceComponent::HandleQuickAccessChangedForPersistence()
{
    ScheduleCharacterStateSave();
}

void UARPGPersistenceComponent::HandleActiveQuickAccessChangedForPersistence(int32 SlotNumber, FName ItemId, FGuid ItemInstanceId)
{
    (void)SlotNumber;
    (void)ItemId;
    (void)ItemInstanceId;
    ScheduleCharacterStateSave();
}

bool UARPGPersistenceComponent::FlushPendingCharacterStateSave()
{
    if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(CharacterStateSaveTimer);
    if (!bCharacterStateSavePending) return true;
    if (bAutoLoadOnBeginPlay && !bInitialAutoLoadResolved) return false;
    if (bAutomaticSaveSuppressedAfterLoadFailure) return false;

    bCharacterStateSavePending = false;
    const bool bSaved = SaveNowImmediate();
    if (bSaved) ++AutomaticCharacterStateSaveCount;
    else bCharacterStateSavePending = true;
    return bSaved;
}

bool UARPGPersistenceComponent::SaveNow()
{
    // Character state is intentionally serialized synchronously. Mixing periodic/manual async writes
    // with mutation/end-play commits can allow an older snapshot to finish last and overwrite newer
    // Inventory/QuickAccess state. World saves remain independently asynchronous.
    return SaveNowImmediate();
}

bool UARPGPersistenceComponent::SaveNowImmediate()
{
    if (!ARPGIsAccountCharacterPersistenceOwner(GetOwner())) return false;
    UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr; UARPGSaveSubsystem* Saves = GI ? GI->GetSubsystem<UARPGSaveSubsystem>() : nullptr;
    const bool bSaved = Saves && GetOwner() ? Saves->SaveCharacterImmediate(GetOwner()) : false;
    if (bSaved)
    {
        bCharacterStateSavePending = false;
        if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(CharacterStateSaveTimer);
    }
    return bSaved;
}

bool UARPGPersistenceComponent::LoadNow()
{
    if (!ARPGIsAccountCharacterPersistenceOwner(GetOwner())) return false;
    UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr; UARPGSaveSubsystem* Saves = GI ? GI->GetSubsystem<UARPGSaveSubsystem>() : nullptr;
    return Saves && GetOwner() ? Saves->LoadCharacter(GetOwner()) : false;
}

void UARPGPersistenceComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    (void)EndPlayReason;

    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(AutoSaveTimer);
        GetWorld()->GetTimerManager().ClearTimer(CharacterStateSaveTimer);
    }

    UnbindAutomaticStateSaveDelegates();

    if (bSaveOnEndPlay
        && (!bAutoLoadOnBeginPlay || bInitialAutoLoadResolved)
        && !bAutomaticSaveSuppressedAfterLoadFailure)
    {
        // Save for every player-character EndPlay reason, including explicit destruction. Some PIE,
        // respawn and project-owned teardown paths destroy the pawn before world shutdown; excluding
        // Destroyed made the final Inventory/QuickAccess commit dependent on engine teardown order.
        bCharacterStateSavePending = false;
        SaveNowImmediate();
    }
    Super::EndPlay(EndPlayReason);
}
