#include "Subsystems/ARPGAccountSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/SecureHash.h"
#include "Containers/StringConv.h"

const FString UARPGAccountSubsystem::AccountIndexSlot = TEXT("ARPG_LocalAccounts");

FString UARPGAccountSubsystem::NormalizeUsername(const FString& Username) const
{
    return Username.TrimStartAndEnd().ToLower();
}

FString UARPGAccountSubsystem::MakeVerifier(const FString& Username, const FString& Password, const FString& Salt) const
{
    FString Material = Salt + TEXT("|") + NormalizeUsername(Username) + TEXT("|") + Password;
    // Local profile protection only. Real Internet authentication should use a backend/provider.
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

bool UARPGAccountSubsystem::CreateLocalAccount(const FString& Username, const FString& Password, FText& OutMessage)
{
    const FString Clean = NormalizeUsername(Username);
    if (Clean.Len() < 3 || Password.Len() < 4) { OutMessage = FText::FromString(TEXT("Username must be at least 3 characters and password at least 4.")); return false; }
    UARPGAccountIndexSave* Index = LoadIndex(); if (!Index) { OutMessage = FText::FromString(TEXT("Could not open local account database.")); return false; }
    if (Index->Accounts.ContainsByPredicate([&](const FARPGLocalAccountRecord& A){ return A.Username.Equals(Clean, ESearchCase::IgnoreCase); }))
    { OutMessage = FText::FromString(TEXT("That username already exists.")); return false; }
    FARPGLocalAccountRecord NewAccount; NewAccount.AccountId = FGuid::NewGuid(); NewAccount.Username = Clean; NewAccount.Salt = FGuid::NewGuid().ToString(EGuidFormats::Digits);
    NewAccount.PasswordVerifier = MakeVerifier(Clean, Password, NewAccount.Salt); NewAccount.CreatedUtc = FDateTime::UtcNow(); Index->Accounts.Add(NewAccount);
    if (!SaveIndex(Index)) { OutMessage = FText::FromString(TEXT("Failed to save the new account.")); return false; }
    OutMessage = FText::FromString(TEXT("Account created.")); return true;
}

bool UARPGAccountSubsystem::LoginLocalAccount(const FString& Username, const FString& Password, FText& OutMessage)
{
    const FString Clean = NormalizeUsername(Username); UARPGAccountIndexSave* Index = LoadIndex();
    const FARPGLocalAccountRecord* Account = Index ? Index->Accounts.FindByPredicate([&](const FARPGLocalAccountRecord& A){ return A.Username.Equals(Clean, ESearchCase::IgnoreCase); }) : nullptr;
    if (!Account || Account->PasswordVerifier != MakeVerifier(Clean, Password, Account ? Account->Salt : TEXT("invalid")))
    { OutMessage = FText::FromString(TEXT("Invalid username or password.")); OnLoginResult.Broadcast(false, OutMessage); return false; }
    bLoggedIn = true; CurrentAccountId = Account->AccountId; CurrentUsername = Account->Username; OutMessage = FText::FromString(TEXT("Login successful.")); OnLoginResult.Broadcast(true, OutMessage); return true;
}

void UARPGAccountSubsystem::Logout() { bLoggedIn = false; CurrentAccountId.Invalidate(); CurrentUsername.Reset(); }
FString UARPGAccountSubsystem::GetCurrentAccountSlotPrefix() const { return bLoggedIn ? FString::Printf(TEXT("ARPG_%s"), *CurrentAccountId.ToString(EGuidFormats::Digits)) : TEXT("ARPG_Guest"); }


bool UARPGAccountSubsystem::RegisterCharacterId(FGuid CharacterId)
{
    if (!CharacterId.IsValid()) return false;
    UARPGAccountIndexSave* Index = LoadIndex(); if (!Index) return false;

    // The framework supports a no-login local Guest profile. Before v2.15.12 Guest characters were
    // never indexed, so BeginPlay generated a different CharacterId after every restart. Building
    // ownership is intentionally CharacterId-based when no AccountId exists, which made correctly
    // loaded Guest structures look foreign and therefore fail modification/snap access checks.
    if (!bLoggedIn)
    {
        if (Index->GuestCharacterId == CharacterId) return true;
        Index->GuestCharacterId = CharacterId;
        return SaveIndex(Index);
    }

    FARPGLocalAccountRecord* Account = Index->Accounts.FindByPredicate([&](const FARPGLocalAccountRecord& A){ return A.AccountId == CurrentAccountId; });
    if (!Account) return false;
    Account->CharacterIds.AddUnique(CharacterId); Account->LastCharacterId = CharacterId; return SaveIndex(Index);
}

TArray<FGuid> UARPGAccountSubsystem::GetRegisteredCharacterIds() const
{
    UARPGAccountIndexSave* Index = LoadIndex(); if (!Index) return {};
    if (!bLoggedIn)
    {
        TArray<FGuid> GuestIds;
        if (Index->GuestCharacterId.IsValid())
        {
            GuestIds.Add(Index->GuestCharacterId);
        }
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
