#include "Components/ARPGCurrencyComponent.h"
#include "Net/UnrealNetwork.h"

UARPGCurrencyComponent::UARPGCurrencyComponent() { SetIsReplicatedByDefault(true); }

int64 UARPGCurrencyComponent::GetCurrency(FName CurrencyId) const
{
    for (const FARPGCurrencyBalance& Balance : Balances) if (Balance.CurrencyId == CurrencyId) return Balance.Amount;
    return 0;
}

void UARPGCurrencyComponent::AddCurrency(FName CurrencyId, int64 Amount)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || CurrencyId.IsNone() || Amount == 0) return;
    for (FARPGCurrencyBalance& Balance : Balances)
    {
        if (Balance.CurrencyId == CurrencyId)
        {
            Balance.Amount = FMath::Max<int64>(0, Balance.Amount + Amount);
            OnCurrencyChanged.Broadcast(CurrencyId, Balance.Amount);
            return;
        }
    }
    FARPGCurrencyBalance NewBalance; NewBalance.CurrencyId = CurrencyId; NewBalance.Amount = FMath::Max<int64>(0, Amount);
    Balances.Add(NewBalance); OnCurrencyChanged.Broadcast(CurrencyId, NewBalance.Amount);
}

bool UARPGCurrencyComponent::SpendCurrency(FName CurrencyId, int64 Amount)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || Amount < 0) return false;
    for (FARPGCurrencyBalance& Balance : Balances)
    {
        if (Balance.CurrencyId == CurrencyId && Balance.Amount >= Amount)
        {
            Balance.Amount -= Amount; OnCurrencyChanged.Broadcast(CurrencyId, Balance.Amount); return true;
        }
    }
    return Amount == 0;
}

void UARPGCurrencyComponent::ReplaceBalances(const TArray<FARPGCurrencyBalance>& NewBalances)
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;
    Balances = NewBalances;
}

void UARPGCurrencyComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps); DOREPLIFETIME(UARPGCurrencyComponent, Balances);
}
