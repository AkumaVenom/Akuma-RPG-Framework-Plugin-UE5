#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Save/ARPGSaveGame.h"
#include "ARPGAccountSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FARPGLoginResult, bool, bSuccess, FText, Message);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FARPGAccountSessionChanged);

/**
 * Local account/profile owner for single-player and trusted direct-IP play.
 *
 * Passwords never leave this subsystem and are never transmitted to a listen server. Runtime network
 * identity uses the generated AccountId plus CharacterId after the local password verifier has succeeded.
 * This is intentionally a local-profile system, not Internet-grade authentication.
 */
UCLASS()
class AKUMASRPGFRAMEWORK_API UARPGAccountSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    UPROPERTY(BlueprintAssignable, Category="ARPG|Account") FARPGLoginResult OnLoginResult;
    UPROPERTY(BlueprintAssignable, Category="ARPG|Account") FARPGAccountSessionChanged OnAccountSessionChanged;

    UPROPERTY(BlueprintReadOnly, Category="ARPG|Account") bool bLoggedIn = false;
    UPROPERTY(BlueprintReadOnly, Category="ARPG|Account") FGuid CurrentAccountId;
    UPROPERTY(BlueprintReadOnly, Category="ARPG|Account") FString CurrentUsername;

    /** Creates a local account record and its dedicated non-secret account profile save. */
    UFUNCTION(BlueprintCallable, Category="ARPG|Account") bool CreateLocalAccount(const FString& Username, const FString& Password, FText& OutMessage);
    /** Convenience frontend flow: creates the account and immediately opens that new local account session. */
    UFUNCTION(BlueprintCallable, Category="ARPG|Account") bool CreateAndLoginLocalAccount(const FString& Username, const FString& Password, FText& OutMessage);
    UFUNCTION(BlueprintCallable, Category="ARPG|Account") bool LoginLocalAccount(const FString& Username, const FString& Password, FText& OutMessage);
    UFUNCTION(BlueprintCallable, Category="ARPG|Account") void Logout();

    UFUNCTION(BlueprintPure, Category="ARPG|Account") bool IsLoggedIn() const { return bLoggedIn; }
    UFUNCTION(BlueprintPure, Category="ARPG|Account") FString GetCurrentAccountSlotPrefix() const;
    UFUNCTION(BlueprintPure, Category="ARPG|Account") FString GetAccountProfileSlotName(FGuid AccountId) const;
    UFUNCTION(BlueprintPure, Category="ARPG|Account") bool DoesCurrentAccountProfileExist() const;
    UFUNCTION(BlueprintCallable, Category="ARPG|Account") UARPGAccountProfileSave* LoadCurrentAccountProfile() const;
    UFUNCTION(BlueprintCallable, Category="ARPG|Account") bool SaveCurrentFrontendPreferences(const FString& JoinAddress, int32 ListenPort, bool bLAN, FName GameplayMap);

    UFUNCTION(BlueprintCallable, Category="ARPG|Account") bool RegisterCharacterId(FGuid CharacterId);
    UFUNCTION(BlueprintPure, Category="ARPG|Account") TArray<FGuid> GetRegisteredCharacterIds() const;
    UFUNCTION(BlueprintPure, Category="ARPG|Account") FGuid GetLastCharacterId() const;
    /** Returns the current account's stable gameplay CharacterId, creating and persisting one when needed. */
    UFUNCTION(BlueprintCallable, Category="ARPG|Account") FGuid GetOrCreateLastCharacterId();

    /** Returns a display-safe normalized username without altering any account state. */
    UFUNCTION(BlueprintPure, Category="ARPG|Account") FString NormalizeUsernameForDisplay(const FString& Username) const;

private:
    static const FString AccountIndexSlot;
    UARPGAccountIndexSave* LoadIndex() const;
    bool SaveIndex(UARPGAccountIndexSave* Index) const;
    bool CreateOrRepairAccountProfile(const FARPGLocalAccountRecord& Account, bool bUpdateLastLogin) const;
    FString MakeVerifier(const FString& Username, const FString& Password, const FString& Salt) const;
    FString NormalizeUsername(const FString& Username) const;
};
