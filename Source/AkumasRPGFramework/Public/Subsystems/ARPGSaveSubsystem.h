#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ARPGSaveSubsystem.generated.h"

class AARPGCharacter;
class UARPGSaveGame;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FARPGSaveResult, FString, SlotName, bool, bSuccess);

UCLASS()
class AKUMASRPGFRAMEWORK_API UARPGSaveSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    UPROPERTY(BlueprintAssignable) FARPGSaveResult OnSaveComplete;

    /** Local-account convenience path retained for Blueprint/backward compatibility. */
    UFUNCTION(BlueprintCallable, Category="ARPG|Save") FString MakeCharacterSlotName(const FGuid& CharacterId) const;
    /** Explicit account-scoped slot builder used by listen-server remote player persistence. */
    UFUNCTION(BlueprintPure, Category="ARPG|Save") FString MakeCharacterSlotNameForAccount(const FGuid& AccountId, const FGuid& CharacterId) const;
    /** Resolves the account from the owning PlayerController when available; never aliases a remote pawn to the host account. */
    UFUNCTION(BlueprintPure, Category="ARPG|Save") FString MakeCharacterSlotNameForActor(AActor* CharacterActor) const;
    /** Context-aware world slot. Logged-in standalone/listen-host worlds are account-scoped automatically. */
    UFUNCTION(BlueprintCallable, Category="ARPG|Save") FString MakeWorldSlotName(FString WorldId) const;
    /** Explicit world-slot builder. Invalid AccountId retains the legacy Guest/dedicated-server ARPG_World_<WorldId> namespace. */
    UFUNCTION(BlueprintPure, Category="ARPG|Save") FString MakeWorldSlotNameForAccount(const FGuid& AccountId, FString WorldId) const;
    /** Resolves the local authoritative world owner: logged-in standalone/listen host account; invalid on clients/dedicated/Guest. */
    UFUNCTION(BlueprintPure, Category="ARPG|Save") FGuid ResolveWorldSaveAccountId() const;
    /** Serialized synchronous character save. Automatic callers debounce state changes before invoking it. */
    UFUNCTION(BlueprintCallable, Category="ARPG|Save") bool SaveCharacter(AActor* CharacterActor, FString SlotOverride=TEXT(""));
    /** Explicit synchronous alias retained for controlled shutdown/EndPlay and existing Blueprint/API callers. */
    UFUNCTION(BlueprintCallable, Category="ARPG|Save") bool SaveCharacterImmediate(AActor* CharacterActor, FString SlotOverride=TEXT(""));
    UFUNCTION(BlueprintCallable, Category="ARPG|Save") bool LoadCharacter(AActor* CharacterActor, FString SlotOverride=TEXT(""));
    UFUNCTION(BlueprintCallable, Category="ARPG|Save") bool SaveWorld(FString WorldId=TEXT("DefaultWorld"), FString SlotOverride=TEXT(""));
    UFUNCTION(BlueprintCallable, Category="ARPG|Save") bool LoadWorld(FString WorldId=TEXT("DefaultWorld"), FString SlotOverride=TEXT(""));
    /** Stable explicit-account world persistence used by gameplay GameMode after it captures the session's world owner. */
    UFUNCTION(BlueprintCallable, Category="ARPG|Save") bool SaveWorldForAccount(const FGuid& AccountId, FString WorldId=TEXT("DefaultWorld"), FString SlotOverride=TEXT(""));
    UFUNCTION(BlueprintCallable, Category="ARPG|Save") bool LoadWorldForAccount(const FGuid& AccountId, FString WorldId=TEXT("DefaultWorld"), FString SlotOverride=TEXT(""));
    UFUNCTION(BlueprintPure, Category="ARPG|Save") bool DoesCharacterSaveExist(const FGuid& CharacterId) const;
    UFUNCTION(BlueprintPure, Category="ARPG|Save") bool DoesCharacterSaveExistForActor(AActor* CharacterActor) const;
    UFUNCTION(BlueprintPure, Category="ARPG|Save") FGuid ResolveCharacterAccountId(AActor* CharacterActor) const;
    UFUNCTION(BlueprintPure, Category="ARPG|Save") bool DoesWorldSaveExist(FString WorldId=TEXT("DefaultWorld")) const;
    UFUNCTION(BlueprintPure, Category="ARPG|Save") bool DoesWorldSaveExistForAccount(const FGuid& AccountId, FString WorldId=TEXT("DefaultWorld")) const;
protected:
    AARPGCharacter* ResolveSaveableCharacter(AActor* CharacterActor) const;
    UARPGSaveGame* BuildCharacterSave(AARPGCharacter* Character);
    UFUNCTION() void HandleAsyncSaveComplete(const FString& SlotName, const int32 UserIndex, bool bSuccess);
};
