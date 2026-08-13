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
    // v4 adds exact per-instance durability to persistent container/station Inventory entries.
    UPROPERTY(SaveGame, BlueprintReadWrite) int32 SaveVersion = 4;
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
};
