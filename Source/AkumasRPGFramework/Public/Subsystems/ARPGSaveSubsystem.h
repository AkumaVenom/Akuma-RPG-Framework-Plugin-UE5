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

    UFUNCTION(BlueprintCallable, Category="ARPG|Save") FString MakeCharacterSlotName(const FGuid& CharacterId) const;
    UFUNCTION(BlueprintCallable, Category="ARPG|Save") FString MakeWorldSlotName(FString WorldId) const;
    /** Serialized synchronous character save. Automatic callers debounce state changes before invoking it. */
    UFUNCTION(BlueprintCallable, Category="ARPG|Save") bool SaveCharacter(AActor* CharacterActor, FString SlotOverride=TEXT(""));
    /** Explicit synchronous alias retained for controlled shutdown/EndPlay and existing Blueprint/API callers. */
    UFUNCTION(BlueprintCallable, Category="ARPG|Save") bool SaveCharacterImmediate(AActor* CharacterActor, FString SlotOverride=TEXT(""));
    UFUNCTION(BlueprintCallable, Category="ARPG|Save") bool LoadCharacter(AActor* CharacterActor, FString SlotOverride=TEXT(""));
    UFUNCTION(BlueprintCallable, Category="ARPG|Save") bool SaveWorld(FString WorldId=TEXT("DefaultWorld"), FString SlotOverride=TEXT(""));
    UFUNCTION(BlueprintCallable, Category="ARPG|Save") bool LoadWorld(FString WorldId=TEXT("DefaultWorld"), FString SlotOverride=TEXT(""));
    UFUNCTION(BlueprintPure, Category="ARPG|Save") bool DoesCharacterSaveExist(const FGuid& CharacterId) const;
    UFUNCTION(BlueprintPure, Category="ARPG|Save") bool DoesWorldSaveExist(FString WorldId=TEXT("DefaultWorld")) const;
protected:
    AARPGCharacter* ResolveSaveableCharacter(AActor* CharacterActor) const;
    UARPGSaveGame* BuildCharacterSave(AARPGCharacter* Character);
    UFUNCTION() void HandleAsyncSaveComplete(const FString& SlotName, const int32 UserIndex, bool bSuccess);
};
