#include "Crafting/ARPGCraftingStationActor.h"
#include "Actors/ARPGCharacter.h"
#include "Components/ARPGEventRouterComponent.h"
#include "Components/ARPGInventoryComponent.h"
#include "Components/ARPGSkillComponent.h"
#include "Data/ARPGCraftingStationDefinition.h"
#include "Data/ARPGItemDefinition.h"
#include "Data/ARPGRecipeDefinition.h"
#include "Data/ARPGSkillDefinition.h"
#include "Utilities/ARPGAssetLibrary.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"

AARPGCraftingStationActor::AARPGCraftingStationActor()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval = 0.1f;
    OutputInventory = CreateDefaultSubobject<UARPGInventoryComponent>(TEXT("OutputInventory"));
    OutputInventory->MaxSlots = 24;
}

void AARPGCraftingStationActor::BeginPlay()
{
    Super::BeginPlay();
    if (StationDefinition)
    {
        if (Inventory) Inventory->MaxSlots = FMath::Max(1, StationDefinition->InputSlots);
        if (OutputInventory) OutputInventory->MaxSlots = FMath::Max(1, StationDefinition->OutputSlots);
    }
    if (HasAuthority()) ProcessOfflineElapsed();
}

UARPGInventoryComponent* AARPGCraftingStationActor::ResolveInputInventory(AActor* Crafter) const
{
    if (StationDefinition && StationDefinition->bUseStationInventoryForInputs) return Inventory;
    return Crafter ? Crafter->FindComponentByClass<UARPGInventoryComponent>() : nullptr;
}

UARPGInventoryComponent* AARPGCraftingStationActor::ResolveFuelInventory(AActor* Crafter) const
{
    if (!StationDefinition || StationDefinition->bFuelComesFromStationInventory) return Inventory;
    return Crafter ? Crafter->FindComponentByClass<UARPGInventoryComponent>() : nullptr;
}

static int32 ARPGCountTaggedItems(const UARPGInventoryComponent* InventoryComponent, const FGameplayTag& Tag)
{
    if (!InventoryComponent || !Tag.IsValid()) return 0;
    int32 Count = 0;
    for (const FARPGInventoryEntry& Entry : InventoryComponent->Items)
    {
        const UARPGItemDefinition* Item = InventoryComponent->ResolveItemDefinition(Entry);
        if (Item && Item->ItemTags.HasTag(Tag)) Count += FMath::Max(0, Entry.Quantity);
    }
    return Count;
}

bool AARPGCraftingStationActor::HasFuelForCraft(AActor* Crafter, const UARPGRecipeDefinition* Recipe) const
{
    if (!Recipe || !Recipe->bConsumesFuel) return true;
    if (!Recipe->FuelTag.IsValid()) return false;
    const int32 RequiredFuel = FMath::Max(1, FMath::CeilToInt(Recipe->FuelPerCraft));
    return ARPGCountTaggedItems(ResolveFuelInventory(Crafter), Recipe->FuelTag) >= RequiredFuel;
}

bool AARPGCraftingStationActor::ConsumeFuelForCraft(AActor* Crafter, const UARPGRecipeDefinition* Recipe)
{
    if (!Recipe || !Recipe->bConsumesFuel) return true;
    UARPGInventoryComponent* FuelInventory = ResolveFuelInventory(Crafter);
    if (!FuelInventory || !Recipe->FuelTag.IsValid()) return false;
    int32 Remaining = FMath::Max(1, FMath::CeilToInt(Recipe->FuelPerCraft));
    if (ARPGCountTaggedItems(FuelInventory, Recipe->FuelTag) < Remaining) return false;

    TArray<TPair<FName, int32>> Removals;
    for (const FARPGInventoryEntry& Entry : FuelInventory->Items)
    {
        if (Remaining <= 0) break;
        const UARPGItemDefinition* Item = FuelInventory->ResolveItemDefinition(Entry);
        if (!Item || !Item->ItemTags.HasTag(Recipe->FuelTag)) continue;
        const int32 Take = FMath::Min(Remaining, Entry.Quantity);
        Removals.Emplace(Entry.ItemId, Take);
        Remaining -= Take;
    }
    if (Remaining > 0) return false;
    for (const TPair<FName, int32>& Removal : Removals)
        if (!FuelInventory->RemoveItem(Removal.Key, Removal.Value)) return false;
    return true;
}

bool AARPGCraftingStationActor::CanUseRecipe(AActor* Crafter, const UARPGRecipeDefinition* Recipe) const
{
    if (!Crafter || !Recipe) return false;
    if (StationDefinition)
    {
        if (Recipe->RequiredStationTag.IsValid() && StationDefinition->StationTag.IsValid() && !StationDefinition->StationTag.MatchesTagExact(Recipe->RequiredStationTag)) return false;
        if (StationDefinition->Recipes.Num() > 0 && !StationDefinition->Recipes.Contains(Recipe)) return false;
    }
    if (!Recipe->RequiredSkillId.IsNone())
    {
        const UARPGSkillComponent* Skills = Crafter->FindComponentByClass<UARPGSkillComponent>();
        if (!Skills || Skills->GetSkillLevel(Recipe->RequiredSkillId) < Recipe->RequiredSkillLevel) return false;
    }
    const UARPGInventoryComponent* InputInventory = ResolveInputInventory(Crafter);
    if (!InputInventory) return false;
    for (const FARPGItemAmount& Input : Recipe->Inputs)
        if (!InputInventory->HasItem(Input.ItemId, Input.Quantity)) return false;
    return HasFuelForCraft(Crafter, Recipe);
}

bool AARPGCraftingStationActor::ConsumeRecipeInputs(AActor* Crafter, const UARPGRecipeDefinition* Recipe, int32 Count)
{
    UARPGInventoryComponent* InputInventory = ResolveInputInventory(Crafter);
    if (!InputInventory || !Recipe || Count <= 0) return false;
    for (const FARPGItemAmount& Input : Recipe->Inputs)
        if (!InputInventory->HasItem(Input.ItemId, Input.Quantity * Count)) return false;
    for (const FARPGItemAmount& Input : Recipe->Inputs)
        if (!InputInventory->RemoveItem(Input.ItemId, Input.Quantity * Count)) return false;
    return true;
}

void AARPGCraftingStationActor::RefundRecipeInputs(AActor* Crafter, const UARPGRecipeDefinition* Recipe, int32 Count)
{
    UARPGInventoryComponent* InputInventory = ResolveInputInventory(Crafter);
    if (!InputInventory && StationDefinition && !StationDefinition->bUseStationInventoryForInputs) InputInventory = Inventory;
    if (!InputInventory || !Recipe || Count <= 0) return;
    for (const FARPGItemAmount& Input : Recipe->Inputs) InputInventory->AddItem(Input.ItemId, Input.Quantity * Count);
}

AActor* AARPGCraftingStationActor::FindCrafter(FGuid CharacterId) const
{
    if (!CharacterId.IsValid() || !GetWorld()) return nullptr;
    for (TActorIterator<AARPGCharacter> It(GetWorld()); It; ++It)
        if (It->CharacterId == CharacterId) return *It;
    return nullptr;
}

bool AARPGCraftingStationActor::QueueRecipe(AActor* Crafter, UARPGRecipeDefinition* Recipe, int32 Count)
{
    if (!HasAuthority() || !Crafter || !Recipe || Count <= 0) return false;
    return QueueRecipeAuthority(Crafter, Recipe, Count);
}

bool AARPGCraftingStationActor::QueueRecipeAuthority(AActor* Crafter, UARPGRecipeDefinition* Recipe, int32 Count)
{
    if (!HasAuthority() || !CanUseRecipe(Crafter, Recipe) || !ConsumeRecipeInputs(Crafter, Recipe, Count)) return false;
    FARPGCraftQueueEntry Entry;
    Entry.QueueId = FGuid::NewGuid();
    if (const AARPGCharacter* Character = Cast<AARPGCharacter>(Crafter)) Entry.CrafterCharacterId = Character->CharacterId;
    Entry.RecipeId = Recipe->DefinitionId;
    Entry.RemainingCount = Count;
    Entry.LastUpdatedUtc = FDateTime::UtcNow();
    CraftQueue.Add(Entry);
    StationState = EARPGCraftingStationState::Crafting;
    OnCraftQueueChanged.Broadcast();
    return true;
}

UARPGRecipeDefinition* AARPGCraftingStationActor::ResolveRecipe(FName RecipeId) const
{
    if (StationDefinition)
        for (UARPGRecipeDefinition* Recipe : StationDefinition->Recipes) if (Recipe && Recipe->DefinitionId == RecipeId) return Recipe;
    return Cast<UARPGRecipeDefinition>(UARPGAssetLibrary::ResolveDefinitionById(UARPGRecipeDefinition::StaticClass(), RecipeId));
}

float AARPGCraftingStationActor::GetRecipeSeconds(FName RecipeId) const
{
    const UARPGRecipeDefinition* Recipe = ResolveRecipe(RecipeId);
    return Recipe ? FMath::Max(0.01f, Recipe->CraftSeconds) : 1.f;
}

bool AARPGCraftingStationActor::CompleteOneCraft(FARPGCraftQueueEntry& Entry)
{
    UARPGRecipeDefinition* Recipe = ResolveRecipe(Entry.RecipeId);
    if (!Recipe || !OutputInventory) return false;
    AActor* Crafter = FindCrafter(Entry.CrafterCharacterId);
    if (!HasFuelForCraft(Crafter, Recipe)) return false;
    for (const FARPGItemAmount& Output : Recipe->Outputs)
        if (!OutputInventory->CanAddItem(Output.ItemId, Output.Quantity)) return false;
    if (!ConsumeFuelForCraft(Crafter, Recipe)) return false;
    for (const FARPGItemAmount& Output : Recipe->Outputs)
        OutputInventory->AddItem(Output.ItemId, Output.Quantity);

    if (Crafter)
    {
        if (!Recipe->RequiredSkillId.IsNone() && Recipe->SkillXP > 0)
        {
            if (UARPGSkillComponent* Skills = Crafter->FindComponentByClass<UARPGSkillComponent>())
            {
                if (const UARPGSkillDefinition* SkillDef = Cast<UARPGSkillDefinition>(UARPGAssetLibrary::ResolveDefinitionById(UARPGSkillDefinition::StaticClass(), Recipe->RequiredSkillId)))
                    Skills->AddSkillXPFromDefinition(SkillDef, Recipe->SkillXP);
                else
                    Skills->AddSkillXP(Recipe->RequiredSkillId, Recipe->SkillXP);
            }
        }
        if (UARPGEventRouterComponent* Events = Crafter->FindComponentByClass<UARPGEventRouterComponent>()) Events->ReportCrafted(Recipe->DefinitionId, 1);
    }

    --Entry.RemainingCount;
    Entry.ProgressSeconds = 0.f;
    Entry.LastUpdatedUtc = FDateTime::UtcNow();
    OnCraftCompleted.Broadcast(Entry.RecipeId, FMath::Max(0, Entry.RemainingCount));
    return true;
}

void AARPGCraftingStationActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!HasAuthority() || CraftQueue.Num() == 0)
    {
        if (HasAuthority()) StationState = EARPGCraftingStationState::Idle;
        return;
    }
    FARPGCraftQueueEntry& Entry = CraftQueue[0];
    Entry.ProgressSeconds += FMath::Max(0.f, DeltaSeconds);
    Entry.LastUpdatedUtc = FDateTime::UtcNow();
    const float Required = GetRecipeSeconds(Entry.RecipeId);
    while (Entry.ProgressSeconds >= Required && Entry.RemainingCount > 0)
    {
        Entry.ProgressSeconds -= Required;
        if (!CompleteOneCraft(Entry))
        {
            StationState = EARPGCraftingStationState::Blocked;
            return;
        }
    }
    if (Entry.RemainingCount <= 0)
    {
        CraftQueue.RemoveAt(0);
        OnCraftQueueChanged.Broadcast();
    }
    StationState = CraftQueue.Num() > 0 ? EARPGCraftingStationState::Crafting : EARPGCraftingStationState::Idle;
}

bool AARPGCraftingStationActor::CancelQueueEntry(FGuid QueueId, bool bRefundRemaining)
{
    if (!HasAuthority()) return false;
    const int32 Index = CraftQueue.IndexOfByPredicate([&](const FARPGCraftQueueEntry& E){ return E.QueueId == QueueId; });
    if (Index == INDEX_NONE) return false;
    const FARPGCraftQueueEntry Entry = CraftQueue[Index];
    if (bRefundRemaining && Entry.RemainingCount > 0)
        if (UARPGRecipeDefinition* Recipe = ResolveRecipe(Entry.RecipeId)) RefundRecipeInputs(FindCrafter(Entry.CrafterCharacterId), Recipe, Entry.RemainingCount);
    CraftQueue.RemoveAt(Index);
    OnCraftQueueChanged.Broadcast();
    return true;
}

void AARPGCraftingStationActor::ProcessOfflineElapsed()
{
    if (!HasAuthority() || CraftQueue.Num() == 0 || !StationDefinition || !StationDefinition->bProcessWhileOffline) return;
    FARPGCraftQueueEntry& Entry = CraftQueue[0];
    if (Entry.LastUpdatedUtc.GetTicks() <= 0) { Entry.LastUpdatedUtc = FDateTime::UtcNow(); return; }
    const double Elapsed = (FDateTime::UtcNow() - Entry.LastUpdatedUtc).GetTotalSeconds();
    Entry.ProgressSeconds += static_cast<float>(FMath::Max(0.0, Elapsed));
    Entry.LastUpdatedUtc = FDateTime::UtcNow();
}

float AARPGCraftingStationActor::GetCurrentCraftProgress01() const
{
    if (CraftQueue.Num() == 0) return 0.f;
    return FMath::Clamp(CraftQueue[0].ProgressSeconds / GetRecipeSeconds(CraftQueue[0].RecipeId), 0.f, 1.f);
}

void AARPGCraftingStationActor::OnRep_CraftQueue() { OnCraftQueueChanged.Broadcast(); }

void AARPGCraftingStationActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AARPGCraftingStationActor, CraftQueue);
    DOREPLIFETIME(AARPGCraftingStationActor, StationState);
}
