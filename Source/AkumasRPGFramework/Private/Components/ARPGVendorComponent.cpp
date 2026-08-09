#include "Components/ARPGVendorComponent.h"
#include "Components/ARPGInventoryComponent.h"
#include "Components/ARPGCurrencyComponent.h"
#include "Components/ARPGProgressionComponent.h"
#include "Components/ARPGFactionComponent.h"
#include "Components/ARPGQuestComponent.h"
#include "Actors/ARPGCharacter.h"
#include "Data/ARPGVendorDefinition.h"
#include "Data/ARPGItemDefinition.h"
#include "Utilities/ARPGAssetLibrary.h"
#include "Net/UnrealNetwork.h"

UARPGVendorComponent::UARPGVendorComponent() { SetIsReplicatedByDefault(true); }

void UARPGVendorComponent::BeginPlay()
{
    Super::BeginPlay();
    if (GetOwner() && GetOwner()->HasAuthority() && RuntimeStock.Num() == 0) InitializeFromDefinition();
}

const FARPGVendorItemDefinition* UARPGVendorComponent::FindStockDefinition(FName ItemId) const
{
    return VendorDefinition ? VendorDefinition->Stock.FindByPredicate([&](const FARPGVendorItemDefinition& X){ return X.ItemId == ItemId; }) : nullptr;
}

void UARPGVendorComponent::InitializeFromDefinition()
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || !VendorDefinition) return;
    RuntimeStock.Reset();
    for (const FARPGVendorItemDefinition& Source : VendorDefinition->Stock)
    {
        FARPGVendorStockEntry Entry;
        Entry.ItemId = Source.ItemId;
        Entry.Quantity = Source.InitialStock;
        Entry.InitialQuantity = Source.InitialStock;
        Entry.UnitPrice = Source.UnitPrice;
        Entry.CurrencyId = Source.CurrencyId;
        Entry.RestockSeconds = Source.RestockSeconds;
        RuntimeStock.Add(Entry);
    }
    OnStockChanged.Broadcast();
}

void UARPGVendorComponent::RefreshRestock(FARPGVendorStockEntry& Entry)
{
    if (Entry.Quantity < 0 || Entry.RestockSeconds <= 0.f) return;
    const FDateTime Now = FDateTime::UtcNow();
    if (Entry.NextRestockUtc.GetTicks() > 0 && Now >= Entry.NextRestockUtc)
    {
        Entry.Quantity = Entry.InitialQuantity;
        Entry.NextRestockUtc = FDateTime();
        OnStockChanged.Broadcast();
    }
}

float UARPGVendorComponent::GetReputationPriceMultiplier(AActor* Customer, const FARPGVendorItemDefinition* StockDef) const
{
    if (!Customer || !StockDef || StockDef->RequiredFactionId.IsNone() || !VendorDefinition) return 1.f;
    const UARPGFactionComponent* Factions = Customer->FindComponentByClass<UARPGFactionComponent>();
    if (!Factions) return 1.f;
    const int32 Rep = Factions->GetReputation(StockDef->RequiredFactionId);
    int32 PositiveTiers = 0;
    if (Rep >= 3000) ++PositiveTiers;
    if (Rep >= 9000) ++PositiveTiers;
    if (Rep >= 21000) ++PositiveTiers;
    if (Rep >= 42000) ++PositiveTiers;
    const float Discount = FMath::Min(VendorDefinition->MaxReputationDiscount, PositiveTiers * VendorDefinition->ReputationDiscountPerPositiveTier);
    return FMath::Clamp(1.f - Discount, 0.05f, 1.f);
}

bool UARPGVendorComponent::MeetsPurchaseRequirements(AActor* Customer, FName ItemId) const
{
    if (!Customer) return false;
    const FARPGVendorItemDefinition* Def = FindStockDefinition(ItemId);
    if (!Def) return false;
    if (const UARPGProgressionComponent* Progression = Customer->FindComponentByClass<UARPGProgressionComponent>())
        if (Progression->Level < Def->RequiredLevel) return false;
    if (!Def->RequiredFactionId.IsNone())
    {
        const UARPGFactionComponent* Factions = Customer->FindComponentByClass<UARPGFactionComponent>();
        if (!Factions || Factions->GetReputation(Def->RequiredFactionId) < Def->RequiredReputation) return false;
    }
    if (!Def->RequiredQuestId.IsNone())
    {
        const UARPGQuestComponent* Quests = Customer->FindComponentByClass<UARPGQuestComponent>();
        if (!Quests || !Quests->IsQuestComplete(Def->RequiredQuestId)) return false;
    }
    return true;
}

int64 UARPGVendorComponent::GetPurchasePriceForCustomer(AActor* Customer, FName ItemId, int32 Quantity) const
{
    const FARPGVendorItemDefinition* Def = FindStockDefinition(ItemId);
    if (!Def || Quantity <= 0 || !VendorDefinition) return -1;
    const double Price = static_cast<double>(Def->UnitPrice) * VendorDefinition->BuyPriceMultiplier * GetReputationPriceMultiplier(Customer, Def) * Quantity;
    return FMath::Max<int64>(0, FMath::RoundToInt64(Price));
}

bool UARPGVendorComponent::Purchase(AActor* Customer, FName ItemId, int32 Quantity)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || !Customer || Quantity <= 0 || !MeetsPurchaseRequirements(Customer, ItemId)) return false;
    UARPGInventoryComponent* Inventory = Customer->FindComponentByClass<UARPGInventoryComponent>();
    UARPGCurrencyComponent* Currency = Customer->FindComponentByClass<UARPGCurrencyComponent>();
    FARPGVendorStockEntry* Entry = RuntimeStock.FindByPredicate([&](const FARPGVendorStockEntry& X){ return X.ItemId == ItemId; });
    if (!Inventory || !Currency || !Entry) return false;
    RefreshRestock(*Entry);
    if (Entry->Quantity >= 0 && Entry->Quantity < Quantity) return false;
    if (!Inventory->CanAddItem(ItemId, Quantity)) return false;
    const int64 Total = GetPurchasePriceForCustomer(Customer, ItemId, Quantity);
    if (Total < 0 || !Currency->SpendCurrency(Entry->CurrencyId, Total)) return false;
    if (!Inventory->AddItem(ItemId, Quantity)) { Currency->AddCurrency(Entry->CurrencyId, Total); return false; }
    if (Entry->Quantity >= 0)
    {
        Entry->Quantity -= Quantity;
        if (Entry->RestockSeconds > 0.f && Entry->NextRestockUtc.GetTicks() <= 0)
            Entry->NextRestockUtc = FDateTime::UtcNow() + FTimespan::FromSeconds(Entry->RestockSeconds);
    }
    OnTransaction.Broadcast(Customer, ItemId, Quantity);
    OnStockChanged.Broadcast();
    return true;
}

FGuid UARPGVendorComponent::ResolveCustomerId(AActor* Customer) const
{
    if (const AARPGCharacter* Character = Cast<AARPGCharacter>(Customer)) return Character->CharacterId;
    return FGuid();
}

bool UARPGVendorComponent::SellToVendor(AActor* Customer, FName ItemId, int32 Quantity)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || !Customer || Quantity <= 0 || !VendorDefinition) return false;
    UARPGInventoryComponent* Inventory = Customer->FindComponentByClass<UARPGInventoryComponent>();
    UARPGCurrencyComponent* Currency = Customer->FindComponentByClass<UARPGCurrencyComponent>();
    const UARPGItemDefinition* ItemDef = Cast<UARPGItemDefinition>(UARPGAssetLibrary::ResolveDefinitionById(UARPGItemDefinition::StaticClass(), ItemId));
    if (!Inventory || !Currency || !ItemDef || !Inventory->HasItem(ItemId, Quantity)) return false;
    const int64 UnitSellPrice = FMath::Max<int64>(0, FMath::RoundToInt64(ItemDef->BaseValue * VendorDefinition->SellPriceMultiplier));
    if (!Inventory->RemoveItem(ItemId, Quantity)) return false;
    Currency->AddCurrency(TEXT("Gold"), UnitSellPrice * static_cast<int64>(Quantity));

    if (VendorDefinition->bSupportsBuyback)
    {
        FARPGVendorBuybackEntry Entry;
        Entry.BuybackId = FGuid::NewGuid();
        Entry.CharacterId = ResolveCustomerId(Customer);
        Entry.ItemId = ItemId;
        Entry.Quantity = Quantity;
        Entry.UnitPrice = UnitSellPrice;
        Entry.CurrencyId = TEXT("Gold");
        Entry.SoldAtUtc = FDateTime::UtcNow();
        BuybackHistory.Add(Entry);
        const int32 Limit = FMath::Max(1, VendorDefinition->BuybackLimit);
        TArray<int32> CustomerIndices;
        for (int32 Index = 0; Index < BuybackHistory.Num(); ++Index)
            if (BuybackHistory[Index].CharacterId == Entry.CharacterId) CustomerIndices.Add(Index);
        while (CustomerIndices.Num() > Limit)
        {
            BuybackHistory.RemoveAt(CustomerIndices[0]);
            CustomerIndices.Reset();
            for (int32 Index = 0; Index < BuybackHistory.Num(); ++Index)
                if (BuybackHistory[Index].CharacterId == Entry.CharacterId) CustomerIndices.Add(Index);
        }
    }
    OnTransaction.Broadcast(Customer, ItemId, -Quantity);
    return true;
}

TArray<FARPGVendorBuybackEntry> UARPGVendorComponent::GetBuybackItems(AActor* Customer) const
{
    TArray<FARPGVendorBuybackEntry> Result;
    const FGuid CustomerId = ResolveCustomerId(Customer);
    if (!CustomerId.IsValid()) return Result;
    for (const FARPGVendorBuybackEntry& Entry : BuybackHistory)
        if (Entry.CharacterId == CustomerId) Result.Add(Entry);
    Result.Sort([](const FARPGVendorBuybackEntry& A, const FARPGVendorBuybackEntry& B){ return A.SoldAtUtc > B.SoldAtUtc; });
    return Result;
}

bool UARPGVendorComponent::BuybackFromVendor(AActor* Customer, FGuid BuybackId)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || !Customer || !VendorDefinition || !VendorDefinition->bSupportsBuyback || !BuybackId.IsValid()) return false;
    const FGuid CustomerId = ResolveCustomerId(Customer);
    const int32 Index = BuybackHistory.IndexOfByPredicate([&](const FARPGVendorBuybackEntry& Entry){ return Entry.BuybackId == BuybackId && Entry.CharacterId == CustomerId; });
    if (Index == INDEX_NONE) return false;
    const FARPGVendorBuybackEntry Entry = BuybackHistory[Index];
    UARPGInventoryComponent* Inventory = Customer->FindComponentByClass<UARPGInventoryComponent>();
    UARPGCurrencyComponent* Currency = Customer->FindComponentByClass<UARPGCurrencyComponent>();
    if (!Inventory || !Currency || !Inventory->CanAddItem(Entry.ItemId, Entry.Quantity)) return false;
    const int64 Cost = Entry.UnitPrice * static_cast<int64>(Entry.Quantity);
    if (!Currency->SpendCurrency(Entry.CurrencyId, Cost)) return false;
    if (!Inventory->AddItem(Entry.ItemId, Entry.Quantity)) { Currency->AddCurrency(Entry.CurrencyId, Cost); return false; }
    BuybackHistory.RemoveAt(Index);
    OnTransaction.Broadcast(Customer, Entry.ItemId, Entry.Quantity);
    return true;
}

int32 UARPGVendorComponent::GetAvailableStock(FName ItemId) const
{
    const FARPGVendorStockEntry* Entry = RuntimeStock.FindByPredicate([&](const FARPGVendorStockEntry& X){ return X.ItemId == ItemId; });
    return Entry ? Entry->Quantity : 0;
}

bool UARPGVendorComponent::PerformVendorService_Implementation(AActor* Customer, FGameplayTag ServiceTag) { return false; }
void UARPGVendorComponent::OnRep_Stock() { OnStockChanged.Broadcast(); }
void UARPGVendorComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(UARPGVendorComponent, RuntimeStock);
}
