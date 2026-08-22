#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "ARPGTypes.h"
#include "ARPGSaveGame.generated.h"

UCLASS()
class AKUMASRPGFRAMEWORK_API UARPGSaveGame : public USaveGame
{
    GENERATED_BODY()
public:
    // v5 adds exact per-instance durability to character Inventory entries.
    UPROPERTY(SaveGame, BlueprintReadWrite) int32 SaveVersion = 5;
    UPROPERTY(SaveGame, BlueprintReadWrite) FGuid AccountId;
    UPROPERTY(SaveGame, BlueprintReadWrite) FARPGCharacterSaveData Character;
    UPROPERTY(SaveGame, BlueprintReadWrite) FDateTime SavedAtUtc;
};

UCLASS()
class AKUMASRPGFRAMEWORK_API UARPGWorldSaveGame : public USaveGame
{
    GENERATED_BODY()
public:
    // v10 binds a world snapshot to its authoritative local account scope. v9 Settlement Path, v8 Bed/residents,
    // v7 Light, v6 Window and v5 construction/Door migration behavior remains unchanged.
    UPROPERTY(SaveGame, BlueprintReadWrite) int32 SaveVersion = 10;
    /** Invalid for Guest/dedicated-server legacy scope; valid for logged-in Single Player and listen-host worlds. */
    UPROPERTY(SaveGame, BlueprintReadWrite) FGuid ScopeAccountId;
    UPROPERTY(SaveGame, BlueprintReadWrite) FARPGWorldSaveData World;
};

USTRUCT(BlueprintType)
struct AKUMASRPGFRAMEWORK_API FARPGLocalAccountRecord
{
    GENERATED_BODY()
    UPROPERTY(SaveGame, BlueprintReadOnly) FGuid AccountId;
    UPROPERTY(SaveGame, BlueprintReadOnly) FString Username;
    UPROPERTY(SaveGame) FString Salt;
    UPROPERTY(SaveGame) FString PasswordVerifier;
    UPROPERTY(SaveGame, BlueprintReadOnly) TArray<FGuid> CharacterIds;
    UPROPERTY(SaveGame, BlueprintReadOnly) FGuid LastCharacterId;
    UPROPERTY(SaveGame, BlueprintReadOnly) FDateTime CreatedUtc;
};

UCLASS()
class AKUMASRPGFRAMEWORK_API UARPGAccountIndexSave : public USaveGame
{
    GENERATED_BODY()
public:
    UPROPERTY(SaveGame) TArray<FARPGLocalAccountRecord> Accounts;
    /** Stable local identity used by the no-login/Guest profile so character/build ownership survives restarts. */
    UPROPERTY(SaveGame) FGuid GuestCharacterId;
};

/**
 * Per-account local profile metadata. Password material deliberately remains in the private local
 * account index; this file contains only non-secret frontend/persistence preferences.
 */
UCLASS()
class AKUMASRPGFRAMEWORK_API UARPGAccountProfileSave : public USaveGame
{
    GENERATED_BODY()
public:
    UPROPERTY(SaveGame, BlueprintReadOnly) int32 SaveVersion = 1;
    UPROPERTY(SaveGame, BlueprintReadOnly) FGuid AccountId;
    UPROPERTY(SaveGame, BlueprintReadOnly) FString Username;
    UPROPERTY(SaveGame, BlueprintReadOnly) FDateTime CreatedUtc;
    UPROPERTY(SaveGame, BlueprintReadOnly) FDateTime LastLoginUtc;
    UPROPERTY(SaveGame, BlueprintReadOnly) FString LastJoinAddress;
    UPROPERTY(SaveGame, BlueprintReadOnly) int32 LastListenPort = 7777;
    UPROPERTY(SaveGame, BlueprintReadOnly) bool bLastLAN = true;
    UPROPERTY(SaveGame, BlueprintReadOnly) FName LastGameplayMap = NAME_None;
};
