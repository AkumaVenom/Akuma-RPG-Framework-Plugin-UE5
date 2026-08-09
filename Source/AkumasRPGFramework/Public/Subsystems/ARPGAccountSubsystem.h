#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Save/ARPGSaveGame.h"
#include "ARPGAccountSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FARPGLoginResult, bool, bSuccess, FText, Message);

UCLASS()
class AKUMASRPGFRAMEWORK_API UARPGAccountSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    UPROPERTY(BlueprintAssignable) FARPGLoginResult OnLoginResult;
    UPROPERTY(BlueprintReadOnly) bool bLoggedIn = false;
    UPROPERTY(BlueprintReadOnly) FGuid CurrentAccountId;
    UPROPERTY(BlueprintReadOnly) FString CurrentUsername;

    UFUNCTION(BlueprintCallable, Category="ARPG|Account") bool CreateLocalAccount(const FString& Username, const FString& Password, FText& OutMessage);
    UFUNCTION(BlueprintCallable, Category="ARPG|Account") bool LoginLocalAccount(const FString& Username, const FString& Password, FText& OutMessage);
    UFUNCTION(BlueprintCallable, Category="ARPG|Account") void Logout();
    UFUNCTION(BlueprintPure, Category="ARPG|Account") bool IsLoggedIn() const { return bLoggedIn; }
    UFUNCTION(BlueprintPure, Category="ARPG|Account") FString GetCurrentAccountSlotPrefix() const;
    UFUNCTION(BlueprintCallable, Category="ARPG|Account") bool RegisterCharacterId(FGuid CharacterId);
    UFUNCTION(BlueprintPure, Category="ARPG|Account") TArray<FGuid> GetRegisteredCharacterIds() const;
    UFUNCTION(BlueprintPure, Category="ARPG|Account") FGuid GetLastCharacterId() const;

private:
    static const FString AccountIndexSlot;
    UARPGAccountIndexSave* LoadIndex() const;
    bool SaveIndex(UARPGAccountIndexSave* Index) const;
    FString MakeVerifier(const FString& Username, const FString& Password, const FString& Salt) const;
    FString NormalizeUsername(const FString& Username) const;
};
