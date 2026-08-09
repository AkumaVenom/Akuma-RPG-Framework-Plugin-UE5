#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ARPGSaveSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FARPGSaveResult, FString, SlotName, bool, bSuccess);

UCLASS()
class AKUMASRPGFRAMEWORK_API UARPGSaveSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    UPROPERTY(BlueprintAssignable) FARPGSaveResult OnSaveComplete;

    UFUNCTION(BlueprintCallable, Category="ARPG|Save") FString MakeCharacterSlotName(const FGuid& CharacterId) const;
    UFUNCTION(BlueprintCallable, Category="ARPG|Save") FString MakeWorldSlotName(FString WorldId) const;
    UFUNCTION(BlueprintCallable, Category="ARPG|Save") bool SaveCharacter(AActor* CharacterActor, FString SlotOverride=TEXT(""));
    UFUNCTION(BlueprintCallable, Category="ARPG|Save") bool LoadCharacter(AActor* CharacterActor, FString SlotOverride=TEXT(""));
    UFUNCTION(BlueprintCallable, Category="ARPG|Save") bool SaveWorld(FString WorldId=TEXT("DefaultWorld"), FString SlotOverride=TEXT(""));
    UFUNCTION(BlueprintCallable, Category="ARPG|Save") bool LoadWorld(FString WorldId=TEXT("DefaultWorld"), FString SlotOverride=TEXT(""));
    UFUNCTION(BlueprintPure, Category="ARPG|Save") bool DoesCharacterSaveExist(const FGuid& CharacterId) const;
    UFUNCTION(BlueprintPure, Category="ARPG|Save") bool DoesWorldSaveExist(FString WorldId=TEXT("DefaultWorld")) const;
protected:
    UFUNCTION() void HandleAsyncSaveComplete(const FString& SlotName, const int32 UserIndex, bool bSuccess);
};
