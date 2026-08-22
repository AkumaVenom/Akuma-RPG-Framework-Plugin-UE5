#include "Subsystems/ARPGAccountSubsystem.h"

#include "Kismet/GameplayStatics.h"
#include "Misc/SecureHash.h"
#include "Containers/StringConv.h"

const FString UARPGAccountSubsystem::AccountIndexSlot = TEXT("ARPG_LocalAccounts");

FString UARPGAccountSubsystem::NormalizeUsername(const FString& Username) const
{
    return Username.TrimStartAndEnd().ToLower();
}

FString UARPGAccountSubsystem::NormalizeUsernameForDisplay(const FString& Username) const
{
    return NormalizeUsername(Username).Left(32);
}

FString UARPGAccountSubsystem::MakeVerifier(const FString& Username, const FString& Password, const FString& Salt) const
{
    FString Material = Salt + TEXT("|") + NormalizeUsername(Username) + TEXT("|") + Password;
    // Local profile protection only. Public Internet authentication must use a backend/platform provider.
    for (int32 Round = 0; Round < 4096; ++Round)
    {
        FTCHARToUTF8 Bytes(*Material);
        uint8 Hash[20];
        FSHA1::HashBuffer(Bytes.Get(), Bytes.Length(), Hash);
        Material = BytesToHex(Hash, 20) + Salt;
    }
    return Material.LeftChop(Salt.Len());
}

UARPGAccountIndexSave* UARPGAccountSubsystem::LoadIndex() const
{
    if (UGameplayStatics::DoesSaveGameExist(AccountIndexSlot, 0))
        if (UARPGAccountIndexSave* Existing = Cast<UARPGAccountIndexSave>(UGameplayStatics::LoadGameFromSlot(AccountIndexSlot, 0))) return Existing;
    return Cast<UARPGAccountIndexSave>(UGameplayStatics::CreateSaveGameObject(UARPGAccountIndexSave::StaticClass()));
}

bool UARPGAccountSubsystem::SaveIndex(UARPGAccountIndexSave* Index) const
{
    return Index && UGameplayStatics::SaveGameToSlot(Index, AccountIndexSlot, 0);
}

FString UARPGAccountSubsystem::GetAccountProfileSlotName(FGuid AccountId) const
{
    return AccountId.IsValid()
        ? FString::Printf(TEXT("ARPG_Account_%s"), *AccountId.ToString(EGuidFormats::Digits))
        : FString();
}

bool UARPGAccountSubsystem::CreateOrRepairAccountProfile(const FARPGLocalAccountRecord& Account, bool bUpdateLastLogin) const
{
    if (!Account.AccountId.IsValid()) return false;
    const FString Slot = GetAccountProfileSlotName(Account.AccountId);
    UARPGAccountProfileSave* Profile = Cast<UARPGAccountProfileSave>(UGameplayStatics::LoadGameFromSlot(Slot, 0));
    if (!Profile) Profile = Cast<UARPGAccountProfileSave>(UGameplayStatics::CreateSaveGameObject(UARPGAccountProfileSave::StaticClass()));
    if (!Profile) return false;

    Profile->AccountId = Account.AccountId;
    Profile->Username = Account.Username;
    if (Profile->CreatedUtc.GetTicks() <= 0) Profile->CreatedUtc = Account.CreatedUtc.GetTicks() > 0 ? Account.CreatedUtc : FDateTime::UtcNow();
    if (bUpdateLastLogin) Profile->LastLoginUtc = FDateTime::UtcNow();
    return UGameplayStatics::SaveGameToSlot(Profile, Slot, 0);
}

bool UARPGAccountSubsystem::CreateLocalAccount(const FString& Username, const FString& Password, FText& OutMessage)
{
    const FString Clean = NormalizeUsername(Username);
    if (Clean.Len() < 3 || Clean.Len() > 32 || Password.Len() < 4 || Password.Len() > 128)
    {
        OutMessage = FText::FromString(TEXT("Username must be 3-32 characters and password 4-128 characters."));
        return false;
    }

    UARPGAccountIndexSave* Index = LoadIndex();
    if (!Index)
    {
        OutMessage = FText::FromString(TEXT("Could not open the local account database."));
        return false;
    }
    if (Index->Accounts.ContainsByPredicate([&](const FARPGLocalAccountRecord& A){ return A.Username.Equals(Clean, ESearchCase::IgnoreCase); }))
    {
        OutMessage = FText::FromString(TEXT("That username already exists."));
        return false;
    }

    FARPGLocalAccountRecord NewAccount;
    NewAccount.AccountId = FGuid::NewGuid();
    NewAccount.Username = Clean;
    NewAccount.Salt = FGuid::NewGuid().ToString(EGuidFormats::Digits);
    NewAccount.PasswordVerifier = MakeVerifier(Clean, Password, NewAccount.Salt);
    NewAccount.CreatedUtc = FDateTime::UtcNow();
    // Establish a stable character identity at account creation, before any map travel or pawn spawn.
    // The character SaveGame itself is still created by Persistence on first gameplay entry.
    NewAccount.LastCharacterId = FGuid::NewGuid();
    NewAccount.CharacterIds.Add(NewAccount.LastCharacterId);
    Index->Accounts.Add(NewAccount);

    // Treat creation as one local transaction. Establish the independent profile first, then publish the
    // username in the account index. If the index commit fails, remove the orphan profile. This ordering
    // cannot leave a successfully-persisted account index entry pointing at a missing profile save.
    const FString NewProfileSlot = GetAccountProfileSlotName(NewAccount.AccountId);
    if (!CreateOrRepairAccountProfile(NewAccount, false))
    {
        Index->Accounts.RemoveAll([&](const FARPGLocalAccountRecord& A){ return A.AccountId == NewAccount.AccountId; });
        OutMessage = FText::FromString(TEXT("Failed to create the account save file."));
        return false;
    }
    if (!SaveIndex(Index))
    {
        Index->Accounts.RemoveAll([&](const FARPGLocalAccountRecord& A){ return A.AccountId == NewAccount.AccountId; });
        UGameplayStatics::DeleteGameInSlot(NewProfileSlot, 0);
        OutMessage = FText::FromString(TEXT("Failed to create the account save file."));
        return false;
    }

    OutMessage = FText::FromString(TEXT("Account created."));
    return true;
}

bool UARPGAccountSubsystem::CreateAndLoginLocalAccount(const FString& Username, const FString& Password, FText& OutMessage)
{
    if (!CreateLocalAccount(Username, Password, OutMessage))
    {
        OnLoginResult.Broadcast(false, OutMessage);
        return false;
    }
    return LoginLocalAccount(Username, Password, OutMessage);
}

bool UARPGAccountSubsystem::LoginLocalAccount(const FString& Username, const FString& Password, FText& OutMessage)
{
    const FString Clean = NormalizeUsername(Username);
    UARPGAccountIndexSave* Index = LoadIndex();
    FARPGLocalAccountRecord* Account = Index ? Index->Accounts.FindByPredicate([&](const FARPGLocalAccountRecord& A){ return A.Username.Equals(Clean, ESearchCase::IgnoreCase); }) : nullptr;
    if (!Account || Account->PasswordVerifier != MakeVerifier(Clean, Password, Account ? Account->Salt : TEXT("invalid")))
    {
        OutMessage = FText::FromString(TEXT("Invalid username or password."));
        OnLoginResult.Broadcast(false, OutMessage);
        return false;
    }

    // Existing pre-frontend accounts are migrated lazily by creating the new non-secret per-account profile.
    if (!CreateOrRepairAccountProfile(*Account, true))
    {
        OutMessage = FText::FromString(TEXT("Login was verified, but the account profile save could not be opened."));
        OnLoginResult.Broadcast(false, OutMessage);
        return false;
    }

    bLoggedIn = true;
    CurrentAccountId = Account->AccountId;
    CurrentUsername = Account->Username;
    if (!GetOrCreateLastCharacterId().IsValid())
    {
        bLoggedIn = false;
        CurrentAccountId.Invalidate();
        CurrentUsername.Reset();
        OutMessage = FText::FromString(TEXT("Login succeeded, but a stable character identity could not be persisted."));
        OnLoginResult.Broadcast(false, OutMessage);
        return false;
    }
    OutMessage = FText::FromString(TEXT("Login successful."));
    OnLoginResult.Broadcast(true, OutMessage);
    OnAccountSessionChanged.Broadcast();
    return true;
}

void UARPGAccountSubsystem::Logout()
{
    bLoggedIn = false;
    CurrentAccountId.Invalidate();
    CurrentUsername.Reset();
    OnAccountSessionChanged.Broadcast();
}

FString UARPGAccountSubsystem::GetCurrentAccountSlotPrefix() const
{
    return bLoggedIn ? FString::Printf(TEXT("ARPG_%s"), *CurrentAccountId.ToString(EGuidFormats::Digits)) : TEXT("ARPG_Guest");
}

bool UARPGAccountSubsystem::DoesCurrentAccountProfileExist() const
{
    if (!bLoggedIn || !CurrentAccountId.IsValid()) return false;
    return UGameplayStatics::DoesSaveGameExist(GetAccountProfileSlotName(CurrentAccountId), 0);
}

UARPGAccountProfileSave* UARPGAccountSubsystem::LoadCurrentAccountProfile() const
{
    if (!bLoggedIn || !CurrentAccountId.IsValid()) return nullptr;
    return Cast<UARPGAccountProfileSave>(UGameplayStatics::LoadGameFromSlot(GetAccountProfileSlotName(CurrentAccountId), 0));
}

bool UARPGAccountSubsystem::SaveCurrentFrontendPreferences(const FString& JoinAddress, int32 ListenPort, bool bLAN, FName GameplayMap)
{
    if (!bLoggedIn || !CurrentAccountId.IsValid()) return false;
    UARPGAccountProfileSave* Profile = LoadCurrentAccountProfile();
    if (!Profile) return false;
    Profile->LastJoinAddress = JoinAddress.TrimStartAndEnd().Left(128);
    Profile->LastListenPort = FMath::Clamp(ListenPort, 1, 65535);
    Profile->bLastLAN = bLAN;
    Profile->LastGameplayMap = GameplayMap;
    return UGameplayStatics::SaveGameToSlot(Profile, GetAccountProfileSlotName(CurrentAccountId), 0);
}

bool UARPGAccountSubsystem::RegisterCharacterId(FGuid CharacterId)
{
    if (!CharacterId.IsValid()) return false;
    UARPGAccountIndexSave* Index = LoadIndex(); if (!Index) return false;

    if (!bLoggedIn)
    {
        if (Index->GuestCharacterId == CharacterId) return true;
        Index->GuestCharacterId = CharacterId;
        return SaveIndex(Index);
    }

    FARPGLocalAccountRecord* Account = Index->Accounts.FindByPredicate([&](const FARPGLocalAccountRecord& A){ return A.AccountId == CurrentAccountId; });
    if (!Account) return false;
    Account->CharacterIds.AddUnique(CharacterId);
    Account->LastCharacterId = CharacterId;
    return SaveIndex(Index);
}

TArray<FGuid> UARPGAccountSubsystem::GetRegisteredCharacterIds() const
{
    UARPGAccountIndexSave* Index = LoadIndex(); if (!Index) return {};
    if (!bLoggedIn)
    {
        TArray<FGuid> GuestIds;
        if (Index->GuestCharacterId.IsValid()) GuestIds.Add(Index->GuestCharacterId);
        return GuestIds;
    }
    const FARPGLocalAccountRecord* Account = Index->Accounts.FindByPredicate([&](const FARPGLocalAccountRecord& A){ return A.AccountId == CurrentAccountId; });
    return Account ? Account->CharacterIds : TArray<FGuid>();
}

FGuid UARPGAccountSubsystem::GetLastCharacterId() const
{
    UARPGAccountIndexSave* Index = LoadIndex(); if (!Index) return FGuid();
    if (!bLoggedIn) return Index->GuestCharacterId;
    const FARPGLocalAccountRecord* Account = Index->Accounts.FindByPredicate([&](const FARPGLocalAccountRecord& A){ return A.AccountId == CurrentAccountId; });
    return Account ? Account->LastCharacterId : FGuid();
}

FGuid UARPGAccountSubsystem::GetOrCreateLastCharacterId()
{
    UARPGAccountIndexSave* Index = LoadIndex();
    if (!Index) return FGuid();

    if (!bLoggedIn)
    {
        if (!Index->GuestCharacterId.IsValid())
        {
            Index->GuestCharacterId = FGuid::NewGuid();
            if (!SaveIndex(Index)) return FGuid();
        }
        return Index->GuestCharacterId;
    }

    FARPGLocalAccountRecord* Account = Index->Accounts.FindByPredicate([&](const FARPGLocalAccountRecord& A)
    {
        return A.AccountId == CurrentAccountId;
    });
    if (!Account) return FGuid();

    if (!Account->LastCharacterId.IsValid())
    {
        Account->LastCharacterId = FGuid::NewGuid();
        Account->CharacterIds.AddUnique(Account->LastCharacterId);
        if (!SaveIndex(Index)) return FGuid();
    }
    else if (!Account->CharacterIds.Contains(Account->LastCharacterId))
    {
        Account->CharacterIds.Add(Account->LastCharacterId);
        if (!SaveIndex(Index)) return FGuid();
    }

    return Account->LastCharacterId;
}
