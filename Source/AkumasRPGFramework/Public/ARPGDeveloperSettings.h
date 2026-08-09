#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "ARPGDeveloperSettings.generated.h"

UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Akuma's RPG Framework"))
class AKUMASRPGFRAMEWORK_API UARPGDeveloperSettings : public UDeveloperSettings
{
    GENERATED_BODY()
public:
    UARPGDeveloperSettings();

    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Saving") FString DefaultSaveSlot = TEXT("ARPG_AutoSave");
    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Saving") bool bAutoLoadOnBeginPlay = true;
    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Saving") FString DefaultWorldSaveSlot = TEXT("ARPG_World_AutoSave");
    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Saving") bool bAutoSaveOnShutdown = true;
    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Saving", meta=(ClampMin="0.0")) float AutoSaveIntervalSeconds = 120.f;

    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Character", meta=(ClampMin="0.0")) float DefaultRespawnDelay = 5.f;
    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Character") FName DefaultPlayerFactionId = TEXT("Player");
    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Character", meta=(ClampMin="1")) int32 DefaultInventorySlots = 64;
    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Quests", meta=(ClampMin="1")) int32 MaxActiveQuests = 35;
    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Battle Pets", meta=(ClampMin="1")) int32 MaxBattlePetTeamSize = 3;

    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Networking") int32 DefaultListenPort = 7777;
    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Networking") int32 MaxPlayers = 8;
    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Chat", meta=(ClampMin="10")) int32 ChatHistoryLimit = 200;

    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Progression", meta=(ClampMin="1")) int32 DefaultMaxCharacterLevel = 100;
    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Progression", meta=(ClampMin="1")) int32 DefaultMaxSkillLevel = 99;
    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Progression", meta=(ClampMin="1.0")) float BaseCharacterXP = 100.f;
    UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Progression", meta=(ClampMin="1.0")) float CharacterXPExponent = 1.55f;

    virtual FName GetCategoryName() const override { return TEXT("Game"); }
};
