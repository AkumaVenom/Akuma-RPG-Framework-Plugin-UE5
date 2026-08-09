#include "Components/ARPGLootComponent.h"
#include "Data/ARPGLootTableDefinition.h"
#include "Components/ARPGInventoryComponent.h"
#include "Components/ARPGCurrencyComponent.h"
#include "Components/ARPGProgressionComponent.h"
#include "Components/ARPGEventRouterComponent.h"

bool UARPGLootComponent::GrantLootTo(AActor* Recipient)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || !Recipient || !LootTable) return false;
    UARPGInventoryComponent* Inventory = Recipient->FindComponentByClass<UARPGInventoryComponent>();
    UARPGCurrencyComponent* Currency = Recipient->FindComponentByClass<UARPGCurrencyComponent>();
    bool bAny = false;
    if (Inventory)
    {
        for (const FARPGLootEntry& Entry : LootTable->Entries)
        {
            if (Entry.ItemId.IsNone() || FMath::FRand() > FMath::Clamp(Entry.DropChance, 0.f, 1.f)) continue;
            const int32 Qty = FMath::RandRange(FMath::Min(Entry.MinQuantity, Entry.MaxQuantity), FMath::Max(Entry.MinQuantity, Entry.MaxQuantity));
            if (Inventory->AddItem(Entry.ItemId, Qty))
            {
                bAny = true;
                if (UARPGEventRouterComponent* Events = Recipient->FindComponentByClass<UARPGEventRouterComponent>()) Events->ReportItemLooted(Entry.ItemId, Qty);
            }
        }
    }
    if (Currency && LootTable->MaxCurrency > 0)
    {
        const int64 Amount = FMath::RandRange(static_cast<int32>(FMath::Clamp<int64>(LootTable->MinCurrency, 0, MAX_int32)), static_cast<int32>(FMath::Clamp<int64>(LootTable->MaxCurrency, 0, MAX_int32)));
        if (Amount > 0) { Currency->AddCurrency(LootTable->CurrencyId, Amount); bAny = true; }
    }
    if (LootTable->CharacterXP > 0)
        if (UARPGProgressionComponent* Progression = Recipient->FindComponentByClass<UARPGProgressionComponent>()) { Progression->AddXP(LootTable->CharacterXP); bAny = true; }
    if (bAny) OnLootGranted.Broadcast(Recipient, LootTable->DefinitionId);
    return bAny;
}
