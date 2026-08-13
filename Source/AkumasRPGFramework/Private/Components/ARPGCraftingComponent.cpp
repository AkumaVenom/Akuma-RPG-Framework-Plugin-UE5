#include "Components/ARPGCraftingComponent.h"

#include "ARPGTypes.h"
#include "Actors/ARPGCharacter.h"
#include "Components/ARPGEventRouterComponent.h"
#include "Components/ARPGInventoryComponent.h"
#include "Components/ARPGSkillComponent.h"
#include "Data/ARPGItemDefinition.h"
#include "Data/ARPGSkillDefinition.h"
#include "Utilities/ARPGAssetLibrary.h"
#include "GameFramework/GameStateBase.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"

UARPGCraftingComponent::UARPGCraftingComponent()
{
    SetIsReplicatedByDefault(true);
    PrimaryComponentTick.bCanEverTick = false;
}

void UARPGCraftingComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(CraftCompletionTimer);
    Super::EndPlay(EndPlayReason);
}

UARPGInventoryComponent* UARPGCraftingComponent::GetInventory() const
{
    return GetOwner() ? GetOwner()->FindComponentByClass<UARPGInventoryComponent>() : nullptr;
}

FName UARPGCraftingComponent::ResolveAmountItemId(const FARPGItemAmount& Amount) const
{
    if (Amount.Item)
        return Amount.Item->DefinitionId.IsNone() ? Amount.Item->GetFName() : Amount.Item->DefinitionId;
    return Amount.ItemId;
}

UARPGItemDefinition* UARPGCraftingComponent::ResolveAmountItemDefinition(const FARPGItemAmount& Amount) const
{
    if (Amount.Item) return Amount.Item;
    const FName ItemId = ResolveAmountItemId(Amount);
    return ItemId.IsNone() ? nullptr : Cast<UARPGItemDefinition>(UARPGAssetLibrary::ResolveDefinitionById(UARPGItemDefinition::StaticClass(), ItemId));
}

bool UARPGCraftingComponent::AggregateAmounts(const TArray<FARPGItemAmount>& Amounts, int32 Multiplier, TArray<FARPGItemAmount>& OutAggregated) const
{
    OutAggregated.Reset();
    if (Multiplier <= 0) return false;

    // Normalize duplicate authoring rows by stable ItemId while preserving first-authored order/reference.
    // This makes validation, consumption and repair costing agree even if the same material is listed twice.
    TMap<FName, int32> IndexByItemId;
    for (const FARPGItemAmount& Amount : Amounts)
    {
        const FName ItemId = ResolveAmountItemId(Amount);
        if (ItemId.IsNone() || Amount.Quantity <= 0) return false;

        const int64 AddedQuantity = static_cast<int64>(Amount.Quantity) * static_cast<int64>(Multiplier);
        if (AddedQuantity <= 0 || AddedQuantity > MAX_int32) return false;

        if (const int32* ExistingIndex = IndexByItemId.Find(ItemId))
        {
            FARPGItemAmount& Existing = OutAggregated[*ExistingIndex];
            const int64 Combined = static_cast<int64>(Existing.Quantity) + AddedQuantity;
            if (Combined > MAX_int32) return false;
            Existing.Quantity = static_cast<int32>(Combined);
        }
        else
        {
            FARPGItemAmount Aggregated = Amount;
            Aggregated.Quantity = static_cast<int32>(AddedQuantity);
            IndexByItemId.Add(ItemId, OutAggregated.Add(Aggregated));
        }
    }
    return true;
}

int32 UARPGCraftingComponent::GetAvailableIngredientCount(FName ItemId) const
{
    const UARPGInventoryComponent* Inventory = GetInventory();
    return Inventory && !ItemId.IsNone() ? Inventory->GetUnequippedItemCount(ItemId) : 0;
}

bool UARPGCraftingComponent::IsRecipeAvailableToPlayer(const UARPGRecipeDefinition* Recipe) const
{
    if (!Recipe || !Recipe->bAllowPlayerCrafting || Recipe->RequiredStationTag.IsValid()) return false;
    return bAllowUnlistedRecipeRequests || PlayerRecipes.Contains(Recipe);
}

bool UARPGCraftingComponent::MeetsSkillRequirement(const UARPGRecipeDefinition* Recipe) const
{
    if (!Recipe || Recipe->RequiredSkillId.IsNone()) return true;
    const UARPGSkillComponent* Skills = GetOwner() ? GetOwner()->FindComponentByClass<UARPGSkillComponent>() : nullptr;
    return Skills && Skills->GetSkillLevel(Recipe->RequiredSkillId) >= FMath::Max(1, Recipe->RequiredSkillLevel);
}

bool UARPGCraftingComponent::HasInputs(const UARPGRecipeDefinition* Recipe, int32 Count) const
{
    if (!Recipe || Count <= 0) return false;
    TArray<FARPGItemAmount> Required;
    if (!AggregateAmounts(Recipe->Inputs, Count, Required)) return Recipe->Inputs.Num() == 0;
    for (const FARPGItemAmount& Input : Required)
    {
        const FName ItemId = ResolveAmountItemId(Input);
        if (GetAvailableIngredientCount(ItemId) < Input.Quantity) return false;
    }
    return true;
}

bool UARPGCraftingComponent::CanFitOutputs(const UARPGRecipeDefinition* Recipe, int32 Count) const
{
    const UARPGInventoryComponent* Inventory = GetInventory();
    if (!Inventory || !Recipe || Count <= 0) return false;

    struct FSimStack { FName Id; int32 Quantity = 0; int32 MaxStack = 1; bool bEquipped = false; };
    TArray<FSimStack> Sim;
    Sim.Reserve(Inventory->Items.Num() + Recipe->Outputs.Num() * Count);
    for (const FARPGInventoryEntry& Entry : Inventory->Items)
    {
        const UARPGItemDefinition* Def = Inventory->ResolveItemDefinition(Entry);
        FSimStack Stack;
        Stack.Id = Entry.ItemId;
        Stack.Quantity = FMath::Max(0, Entry.Quantity);
        Stack.MaxStack = Def && Def->bUsesDurability ? 1 : (Def ? FMath::Max(1, Def->MaxStack) : FMath::Max(1, Inventory->FallbackMaxStack));
        Stack.bEquipped = Entry.bEquipped;
        Sim.Add(Stack);
    }

    auto RemoveSimulated = [&](FName ItemId, int32 Quantity) -> bool
    {
        int32 Remaining = Quantity;
        for (int32 Index = Sim.Num() - 1; Index >= 0 && Remaining > 0; --Index)
        {
            FSimStack& Stack = Sim[Index];
            if (Stack.Id != ItemId || Stack.bEquipped || Stack.Quantity <= 0) continue;
            const int32 Take = FMath::Min(Remaining, Stack.Quantity);
            Stack.Quantity -= Take;
            Remaining -= Take;
        }
        Sim.RemoveAll([](const FSimStack& Stack){ return Stack.Quantity <= 0; });
        return Remaining <= 0;
    };

    auto AddSimulated = [&](FName ItemId, int32 Quantity, int32 MaxStack) -> bool
    {
        int32 Remaining = Quantity;
        for (FSimStack& Stack : Sim)
        {
            if (Stack.Id != ItemId || Stack.bEquipped || Stack.Quantity >= MaxStack) continue;
            const int32 Added = FMath::Min(Remaining, MaxStack - Stack.Quantity);
            Stack.Quantity += Added;
            Remaining -= Added;
            if (Remaining <= 0) return true;
        }
        while (Remaining > 0)
        {
            if (Sim.Num() >= Inventory->MaxSlots) return false;
            FSimStack NewStack;
            NewStack.Id = ItemId;
            NewStack.MaxStack = MaxStack;
            NewStack.Quantity = FMath::Min(Remaining, MaxStack);
            Remaining -= NewStack.Quantity;
            Sim.Add(NewStack);
        }
        return true;
    };

    // Simulate the exact runtime order craft-by-craft: consume one craft's available ingredients,
    // then place that craft's outputs. This prevents both false InventoryFull rejections and batch
    // requests that would only fit after consuming ingredients from a future craft in the batch.
    for (int32 CraftIndex = 0; CraftIndex < Count; ++CraftIndex)
    {
        for (const FARPGItemAmount& Input : Recipe->Inputs)
        {
            const FName InputId = ResolveAmountItemId(Input);
            if (InputId.IsNone() || Input.Quantity <= 0 || !RemoveSimulated(InputId, Input.Quantity)) return false;
        }
        for (const FARPGItemAmount& Output : Recipe->Outputs)
        {
            const FName ItemId = ResolveAmountItemId(Output);
            UARPGItemDefinition* Definition = ResolveAmountItemDefinition(Output);
            if (ItemId.IsNone() || Output.Quantity <= 0) return false;
            const int32 MaxStack = Definition && Definition->bUsesDurability ? 1 : (Definition ? FMath::Max(1, Definition->MaxStack) : FMath::Max(1, Inventory->FallbackMaxStack));
            if (!AddSimulated(ItemId, Output.Quantity, MaxStack)) return false;
        }
    }
    return true;
}

EARPGCraftingResult UARPGCraftingComponent::ValidateCraftRequest(const UARPGRecipeDefinition* Recipe, int32 Count, FText& OutFailureReason) const
{
    OutFailureReason = FText::GetEmpty();
    if (!Recipe)
    {
        OutFailureReason = NSLOCTEXT("AkumasRPGFramework", "CraftInvalidRecipe", "No valid recipe was selected.");
        return EARPGCraftingResult::InvalidRecipe;
    }
    TArray<FARPGItemAmount> NormalizedOutputs;
    if (Recipe->Outputs.Num() == 0 || !AggregateAmounts(Recipe->Outputs, 1, NormalizedOutputs))
    {
        OutFailureReason = NSLOCTEXT("AkumasRPGFramework", "CraftInvalidOutputs", "This recipe has no valid crafted output.");
        return EARPGCraftingResult::InvalidRecipe;
    }
    if (Recipe->Inputs.Num() > 0)
    {
        TArray<FARPGItemAmount> NormalizedInputs;
        if (!AggregateAmounts(Recipe->Inputs, 1, NormalizedInputs))
        {
            OutFailureReason = NSLOCTEXT("AkumasRPGFramework", "CraftInvalidInputs", "This recipe contains an invalid ingredient entry.");
            return EARPGCraftingResult::InvalidRecipe;
        }
    }
    if (Count <= 0 || Count > FMath::Min(FMath::Max(1, MaxCraftRequestCount), FMath::Max(1, Recipe->MaxBatchSize)))
    {
        OutFailureReason = NSLOCTEXT("AkumasRPGFramework", "CraftInvalidQuantity", "That craft quantity is not allowed.");
        return EARPGCraftingResult::InvalidQuantity;
    }
    if (Recipe->Inputs.Num() == 0 && Count > 1)
    {
        OutFailureReason = NSLOCTEXT("AkumasRPGFramework", "CraftFreeBatchRestricted", "Recipes with no ingredients can only be crafted one at a time.");
        return EARPGCraftingResult::InvalidQuantity;
    }
    if (!Recipe->bAllowPlayerCrafting || (!bAllowUnlistedRecipeRequests && !PlayerRecipes.Contains(Recipe)))
    {
        OutFailureReason = NSLOCTEXT("AkumasRPGFramework", "CraftRecipeUnavailable", "This recipe is not available to this character.");
        return EARPGCraftingResult::RecipeNotAvailable;
    }
    if (Recipe->RequiredStationTag.IsValid())
    {
        OutFailureReason = NSLOCTEXT("AkumasRPGFramework", "CraftRequiresStation", "This recipe requires a crafting station.");
        return EARPGCraftingResult::RequiresStation;
    }
    if (!MeetsSkillRequirement(Recipe))
    {
        OutFailureReason = FText::Format(NSLOCTEXT("AkumasRPGFramework", "CraftMissingSkill", "Requires {0} level {1}."), FText::FromName(Recipe->RequiredSkillId), FText::AsNumber(FMath::Max(1, Recipe->RequiredSkillLevel)));
        return EARPGCraftingResult::MissingSkill;
    }
    if (!HasInputs(Recipe, Count))
    {
        OutFailureReason = NSLOCTEXT("AkumasRPGFramework", "CraftMissingIngredients", "You do not have enough available ingredients.");
        return EARPGCraftingResult::MissingIngredients;
    }
    if (!CanFitOutputs(Recipe, Count))
    {
        OutFailureReason = NSLOCTEXT("AkumasRPGFramework", "CraftInventoryFull", "There is not enough inventory space for the crafted output.");
        return EARPGCraftingResult::InventoryFull;
    }
    return EARPGCraftingResult::Success;
}

bool UARPGCraftingComponent::CanCraftRecipe(const UARPGRecipeDefinition* Recipe, int32 Count, FText& OutFailureReason) const
{
    return ValidateCraftRequest(Recipe, Count, OutFailureReason) == EARPGCraftingResult::Success;
}

int32 UARPGCraftingComponent::GetMaxCraftableCount(const UARPGRecipeDefinition* Recipe) const
{
    if (!Recipe || !IsRecipeAvailableToPlayer(Recipe) || !MeetsSkillRequirement(Recipe)) return 0;
    TArray<FARPGItemAmount> NormalizedOutputs;
    if (Recipe->Outputs.Num() == 0 || !AggregateAmounts(Recipe->Outputs, 1, NormalizedOutputs)) return 0;
    int32 MaxCount = FMath::Min(FMath::Max(1, MaxCraftRequestCount), FMath::Max(1, Recipe->MaxBatchSize));
    if (Recipe->Inputs.Num() == 0) MaxCount = FMath::Min(MaxCount, 1); // Prevent accidental free infinite batch recipes.
    TArray<FARPGItemAmount> RequiredPerCraft;
    if (Recipe->Inputs.Num() > 0 && !AggregateAmounts(Recipe->Inputs, 1, RequiredPerCraft)) return 0;
    for (const FARPGItemAmount& Input : RequiredPerCraft)
    {
        const FName Id = ResolveAmountItemId(Input);
        MaxCount = FMath::Min(MaxCount, GetAvailableIngredientCount(Id) / Input.Quantity);
    }
    while (MaxCount > 0 && !CanFitOutputs(Recipe, MaxCount)) --MaxCount;
    return FMath::Max(0, MaxCount);
}

bool UARPGCraftingComponent::ConsumeInputs(const TArray<FARPGItemAmount>& Inputs, int32 Multiplier, TArray<FARPGItemAmount>* OutConsumed)
{
    UARPGInventoryComponent* Inventory = GetInventory();
    if (!Inventory || !GetOwner() || !GetOwner()->HasAuthority() || Multiplier <= 0) return false;

    TArray<FARPGItemAmount> Required;
    if (Inputs.Num() > 0 && !AggregateAmounts(Inputs, Multiplier, Required)) return false;
    for (const FARPGItemAmount& Input : Required)
    {
        const FName Id = ResolveAmountItemId(Input);
        if (!Inventory->HasUnequippedItem(Id, Input.Quantity)) return false;
    }

    TArray<FARPGItemAmount> Consumed;
    for (const FARPGItemAmount& Input : Required)
    {
        const FName Id = ResolveAmountItemId(Input);
        if (!Inventory->RemoveUnequippedItem(Id, Input.Quantity))
        {
            RefundInputs(Consumed, 1);
            return false;
        }
        Consumed.Add(Input);
    }
    if (OutConsumed) *OutConsumed = Consumed;
    return true;
}

void UARPGCraftingComponent::RefundInputs(const TArray<FARPGItemAmount>& Inputs, int32 Multiplier)
{
    UARPGInventoryComponent* Inventory = GetInventory();
    if (!Inventory || Multiplier <= 0 || !GetOwner() || !GetOwner()->HasAuthority()) return;
    for (const FARPGItemAmount& Input : Inputs)
    {
        const int32 Quantity = FMath::Max(1, Input.Quantity) * Multiplier;
        if (UARPGItemDefinition* Definition = ResolveAmountItemDefinition(Input)) Inventory->AddItemDefinition(Definition, Quantity);
        else
        {
            const FName Id = ResolveAmountItemId(Input);
            if (!Id.IsNone()) Inventory->AddItem(Id, Quantity);
        }
    }
}

bool UARPGCraftingComponent::CanFitCommittedOutputs(const UARPGRecipeDefinition* Recipe) const
{
    const UARPGInventoryComponent* Inventory = GetInventory();
    if (!Inventory || !Recipe || Recipe->Outputs.Num() == 0) return false;

    struct FOutputSimStack { FName Id; int32 Quantity = 0; int32 MaxStack = 1; bool bEquipped = false; };
    TArray<FOutputSimStack> Sim;
    Sim.Reserve(Inventory->Items.Num() + Recipe->Outputs.Num());
    for (const FARPGInventoryEntry& Entry : Inventory->Items)
    {
        const UARPGItemDefinition* Def = Inventory->ResolveItemDefinition(Entry);
        FOutputSimStack Stack;
        Stack.Id = Entry.ItemId;
        Stack.Quantity = FMath::Max(0, Entry.Quantity);
        Stack.MaxStack = Def && Def->bUsesDurability ? 1 : (Def ? FMath::Max(1, Def->MaxStack) : FMath::Max(1, Inventory->FallbackMaxStack));
        Stack.bEquipped = Entry.bEquipped;
        Sim.Add(Stack);
    }

    for (const FARPGItemAmount& Output : Recipe->Outputs)
    {
        const FName ItemId = ResolveAmountItemId(Output);
        UARPGItemDefinition* Definition = ResolveAmountItemDefinition(Output);
        if (ItemId.IsNone() || Output.Quantity <= 0) return false;
        const int32 MaxStack = Definition && Definition->bUsesDurability ? 1 : (Definition ? FMath::Max(1, Definition->MaxStack) : FMath::Max(1, Inventory->FallbackMaxStack));
        int32 Remaining = Output.Quantity;
        for (FOutputSimStack& Stack : Sim)
        {
            if (Stack.Id != ItemId || Stack.bEquipped || Stack.Quantity >= MaxStack) continue;
            const int32 Added = FMath::Min(Remaining, MaxStack - Stack.Quantity);
            Stack.Quantity += Added;
            Remaining -= Added;
            if (Remaining <= 0) break;
        }
        while (Remaining > 0)
        {
            if (Sim.Num() >= Inventory->MaxSlots) return false;
            FOutputSimStack NewStack;
            NewStack.Id = ItemId;
            NewStack.MaxStack = MaxStack;
            NewStack.Quantity = FMath::Min(Remaining, MaxStack);
            Remaining -= NewStack.Quantity;
            Sim.Add(NewStack);
        }
    }
    return true;
}

bool UARPGCraftingComponent::GrantOutputs(const UARPGRecipeDefinition* Recipe)
{
    UARPGInventoryComponent* Inventory = GetInventory();
    // Inputs for this craft were already removed by BeginNextCraftAuthority. Do not run the pre-craft
    // simulator here or it would try to subtract those ingredients a second time.
    if (!Inventory || !Recipe || !CanFitCommittedOutputs(Recipe)) return false;
    for (const FARPGItemAmount& Output : Recipe->Outputs)
    {
        const int32 Quantity = FMath::Max(1, Output.Quantity);
        if (UARPGItemDefinition* Definition = ResolveAmountItemDefinition(Output))
        {
            if (!Inventory->AddItemDefinition(Definition, Quantity)) return false;
        }
        else
        {
            const FName Id = ResolveAmountItemId(Output);
            if (Id.IsNone() || !Inventory->AddItem(Id, Quantity)) return false;
        }
    }
    return true;
}

float UARPGCraftingComponent::GetServerTimeSeconds() const
{
    if (const UWorld* World = GetWorld())
    {
        if (const AGameStateBase* GameState = World->GetGameState()) return GameState->GetServerWorldTimeSeconds();
        return World->GetTimeSeconds();
    }
    return 0.f;
}

float UARPGCraftingComponent::GetCraftProgress01() const
{
    if (!IsCrafting()) return 0.f;
    if (ActiveCraftDuration <= KINDA_SMALL_NUMBER) return 1.f;
    return FMath::Clamp((GetServerTimeSeconds() - ActiveCraftStartServerTime) / ActiveCraftDuration, 0.f, 1.f);
}

float UARPGCraftingComponent::GetCraftSecondsRemaining() const
{
    if (!IsCrafting()) return 0.f;
    return FMath::Max(0.f, ActiveCraftDuration - (GetServerTimeSeconds() - ActiveCraftStartServerTime));
}

bool UARPGCraftingComponent::CraftRecipe(UARPGRecipeDefinition* Recipe, int32 Count)
{
    if (!GetOwner() || !Recipe) return false;
    FText Reason;
    if (IsCrafting())
    {
        OnCraftingResult.Broadcast(EARPGCraftingResult::AlreadyCrafting, Recipe, ActiveRemainingCount, NSLOCTEXT("AkumasRPGFramework", "CraftAlreadyActive", "Finish or cancel the current craft first."));
        return false;
    }
    if (!CanCraftRecipe(Recipe, Count, Reason))
    {
        OnCraftingResult.Broadcast(ValidateCraftRequest(Recipe, Count, Reason), Recipe, 0, Reason);
        return false;
    }
    if (!GetOwner()->HasAuthority())
    {
        ServerCraftRecipe(Recipe, Count);
        OnCraftingResult.Broadcast(EARPGCraftingResult::RequestAccepted, Recipe, Count, NSLOCTEXT("AkumasRPGFramework", "CraftRequestSent", "Craft request sent."));
        return true;
    }
    return StartCraftingAuthority(Recipe, Count);
}

bool UARPGCraftingComponent::StartCraftingAuthority(UARPGRecipeDefinition* Recipe, int32 Count)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || IsCrafting()) return false;
    FText Reason;
    const EARPGCraftingResult Validation = ValidateCraftRequest(Recipe, Count, Reason);
    if (Validation != EARPGCraftingResult::Success)
    {
        ClientCraftingResult(Validation, Recipe, 0, Reason);
        return false;
    }
    ActiveRecipe = Recipe;
    ActiveRemainingCount = Count;
    ActiveCraftStartServerTime = 0.f;
    ActiveCraftDuration = 0.f;
    bCurrentInputsCommitted = false;
    // BeginNextCraftAuthority broadcasts only after the current craft's inputs are actually committed.
    // Avoid exposing/saving an "active" state whose ingredients have not yet been removed.
    BeginNextCraftAuthority();
    return true;
}

void UARPGCraftingComponent::BeginNextCraftAuthority()
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || !ActiveRecipe || ActiveRemainingCount <= 0) return;
    if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(CraftCompletionTimer);

    FText Reason;
    const EARPGCraftingResult Validation = ValidateCraftRequest(ActiveRecipe, 1, Reason);
    if (Validation != EARPGCraftingResult::Success)
    {
        FinishCraftingAuthority(Validation, Reason, false);
        return;
    }
    if (!ConsumeInputs(ActiveRecipe->Inputs, 1))
    {
        FinishCraftingAuthority(EARPGCraftingResult::MissingIngredients, NSLOCTEXT("AkumasRPGFramework", "CraftInputsChanged", "The required ingredients are no longer available."), false);
        return;
    }

    bCurrentInputsCommitted = true;
    ActiveCraftDuration = FMath::Max(0.f, ActiveRecipe->CraftSeconds);
    ActiveCraftStartServerTime = GetServerTimeSeconds();
    BroadcastCraftingState();

    if (!GetWorld())
    {
        CompleteCurrentCraftAuthority();
        return;
    }
    if (ActiveCraftDuration <= KINDA_SMALL_NUMBER)
        GetWorld()->GetTimerManager().SetTimerForNextTick(this, &UARPGCraftingComponent::CompleteCurrentCraftAuthority);
    else
        GetWorld()->GetTimerManager().SetTimer(CraftCompletionTimer, this, &UARPGCraftingComponent::CompleteCurrentCraftAuthority, ActiveCraftDuration, false);
}

void UARPGCraftingComponent::CompleteCurrentCraftAuthority()
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || !ActiveRecipe || ActiveRemainingCount <= 0) return;
    UARPGRecipeDefinition* CompletedRecipe = ActiveRecipe;
    if (!GrantOutputs(CompletedRecipe))
    {
        FinishCraftingAuthority(EARPGCraftingResult::InventoryFull, NSLOCTEXT("AkumasRPGFramework", "CraftOutputBlocked", "Crafting stopped because the output no longer fits in Inventory."), true);
        return;
    }

    bCurrentInputsCommitted = false;
    --ActiveRemainingCount;
    AwardRecipeRewards(CompletedRecipe);

    ClientCraftingResult(EARPGCraftingResult::Success, CompletedRecipe, FMath::Max(0, ActiveRemainingCount), NSLOCTEXT("AkumasRPGFramework", "CraftCompleted", "Item crafted."));
    if (ActiveRemainingCount <= 0)
    {
        ActiveRecipe = nullptr;
        ActiveCraftStartServerTime = 0.f;
        ActiveCraftDuration = 0.f;
        BroadcastCraftingState();
        return;
    }
    BeginNextCraftAuthority();
}

void UARPGCraftingComponent::AwardRecipeRewards(UARPGRecipeDefinition* Recipe)
{
    if (!Recipe || !GetOwner() || !GetOwner()->HasAuthority()) return;
    if (!Recipe->RequiredSkillId.IsNone() && Recipe->SkillXP > 0)
    {
        if (UARPGSkillComponent* Skills = GetOwner()->FindComponentByClass<UARPGSkillComponent>())
        {
            if (const UARPGSkillDefinition* SkillDef = Cast<UARPGSkillDefinition>(UARPGAssetLibrary::ResolveDefinitionById(UARPGSkillDefinition::StaticClass(), Recipe->RequiredSkillId)))
                Skills->AddSkillXPFromDefinition(SkillDef, Recipe->SkillXP);
            else
                Skills->AddSkillXP(Recipe->RequiredSkillId, Recipe->SkillXP);
        }
    }
    if (UARPGEventRouterComponent* Events = GetOwner()->FindComponentByClass<UARPGEventRouterComponent>()) Events->ReportCrafted(Recipe->DefinitionId, 1);
}

FARPGCraftQueueEntry UARPGCraftingComponent::MakeCraftingSaveState() const
{
    FARPGCraftQueueEntry State;
    State.RemainingCount = 0;
    if (!IsCrafting() || !ActiveRecipe || !bCurrentInputsCommitted) return State;

    State.RecipeId = ActiveRecipe->DefinitionId.IsNone() ? ActiveRecipe->GetFName() : ActiveRecipe->DefinitionId;
    State.RemainingCount = ActiveRemainingCount;
    State.ProgressSeconds = FMath::Clamp(ActiveCraftDuration - GetCraftSecondsRemaining(), 0.f, FMath::Max(0.f, ActiveCraftDuration));
    State.LastUpdatedUtc = FDateTime::UtcNow();
    return State;
}

void UARPGCraftingComponent::RestoreCraftingSaveState(const FARPGCraftQueueEntry& SavedState)
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;
    if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(CraftCompletionTimer);

    ActiveRecipe = nullptr;
    ActiveRemainingCount = 0;
    ActiveCraftStartServerTime = 0.f;
    ActiveCraftDuration = 0.f;
    bCurrentInputsCommitted = false;

    if (SavedState.RecipeId.IsNone() || SavedState.RemainingCount <= 0)
    {
        BroadcastCraftingState();
        return;
    }

    UARPGRecipeDefinition* Recipe = Cast<UARPGRecipeDefinition>(
        UARPGAssetLibrary::ResolveDefinitionById(UARPGRecipeDefinition::StaticClass(), SavedState.RecipeId));
    if (!Recipe || !Recipe->bAllowPlayerCrafting || Recipe->RequiredStationTag.IsValid())
    {
        // If the recipe still resolves but is no longer valid for personal crafting, its current committed
        // inputs can be returned safely. A missing/deleted recipe cannot be reconstructed from a stale ID.
        if (Recipe) RefundInputs(Recipe->Inputs, 1);
        BroadcastCraftingState();
        return;
    }

    ActiveRecipe = Recipe;
    ActiveRemainingCount = FMath::Clamp(SavedState.RemainingCount, 1, FMath::Min(FMath::Max(1, MaxCraftRequestCount), FMath::Max(1, Recipe->MaxBatchSize)));
    ActiveCraftDuration = FMath::Max(0.f, Recipe->CraftSeconds);
    const float SavedProgress = FMath::Clamp(SavedState.ProgressSeconds, 0.f, ActiveCraftDuration);
    ActiveCraftStartServerTime = GetServerTimeSeconds() - SavedProgress;
    bCurrentInputsCommitted = true;
    BroadcastCraftingState();

    const float RemainingSeconds = FMath::Max(0.f, ActiveCraftDuration - SavedProgress);
    if (!GetWorld())
    {
        CompleteCurrentCraftAuthority();
        return;
    }
    if (RemainingSeconds <= KINDA_SMALL_NUMBER)
        GetWorld()->GetTimerManager().SetTimerForNextTick(this, &UARPGCraftingComponent::CompleteCurrentCraftAuthority);
    else
        GetWorld()->GetTimerManager().SetTimer(CraftCompletionTimer, this, &UARPGCraftingComponent::CompleteCurrentCraftAuthority, RemainingSeconds, false);
}

bool UARPGCraftingComponent::CancelCrafting()
{
    if (!GetOwner() || !IsCrafting()) return false;
    if (!GetOwner()->HasAuthority())
    {
        ServerCancelCrafting();
        return true;
    }
    FinishCraftingAuthority(EARPGCraftingResult::Cancelled, NSLOCTEXT("AkumasRPGFramework", "CraftCancelled", "Crafting cancelled."), true);
    return true;
}

void UARPGCraftingComponent::FinishCraftingAuthority(EARPGCraftingResult Result, const FText& Message, bool bRefundCommittedInputs)
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;
    if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(CraftCompletionTimer);
    UARPGRecipeDefinition* OldRecipe = ActiveRecipe;
    if (bRefundCommittedInputs && bCurrentInputsCommitted && OldRecipe) RefundInputs(OldRecipe->Inputs, 1);
    bCurrentInputsCommitted = false;
    ActiveRecipe = nullptr;
    ActiveRemainingCount = 0;
    ActiveCraftStartServerTime = 0.f;
    ActiveCraftDuration = 0.f;
    BroadcastCraftingState();
    ClientCraftingResult(Result, OldRecipe, 0, Message);
}

bool UARPGCraftingComponent::GetRepairCost(FGuid ItemInstanceId, TArray<FARPGItemAmount>& OutRepairCost) const
{
    OutRepairCost.Reset();
    const UARPGInventoryComponent* Inventory = GetInventory();
    if (!Inventory || !ItemInstanceId.IsValid()) return false;
    const FARPGInventoryEntry* Entry = Inventory->Items.FindByPredicate([&](const FARPGInventoryEntry& Candidate){ return Candidate.InstanceId == ItemInstanceId; });
    if (!Entry) return false;
    const UARPGItemDefinition* Definition = Inventory->ResolveItemDefinition(*Entry);
    if (!Definition || !Definition->bUsesDurability || !Definition->bCanBeRepaired) return false;
    const float MaxDurability = FMath::Max(1.f, Definition->MaxDurability);
    const float MissingFraction = FMath::Clamp((MaxDurability - FMath::Clamp(Entry->Durability, 0.f, MaxDurability)) / MaxDurability, 0.f, 1.f);
    if (MissingFraction <= KINDA_SMALL_NUMBER) return true;

    TArray<FARPGItemAmount> FullRepairCost;
    if (Definition->RepairInputs.Num() > 0 && !AggregateAmounts(Definition->RepairInputs, 1, FullRepairCost)) return false;
    for (const FARPGItemAmount& FullCost : FullRepairCost)
    {
        FARPGItemAmount Cost = FullCost;
        Cost.Quantity = Definition->bScaleRepairCostByMissingDurability
            ? FMath::Max(1, FMath::CeilToInt(static_cast<float>(FullCost.Quantity) * MissingFraction))
            : FullCost.Quantity;
        OutRepairCost.Add(Cost);
    }
    return true;
}

EARPGCraftingResult UARPGCraftingComponent::ValidateRepair(FGuid ItemInstanceId, TArray<FARPGItemAmount>& OutCost, FText& OutFailureReason) const
{
    OutFailureReason = FText::GetEmpty();
    OutCost.Reset();
    const UARPGInventoryComponent* Inventory = GetInventory();
    if (!Inventory || !ItemInstanceId.IsValid())
    {
        OutFailureReason = NSLOCTEXT("AkumasRPGFramework", "RepairInvalid", "No valid item was selected for repair.");
        return EARPGCraftingResult::InvalidRepairItem;
    }
    const FARPGInventoryEntry* Entry = Inventory->Items.FindByPredicate([&](const FARPGInventoryEntry& Candidate){ return Candidate.InstanceId == ItemInstanceId; });
    if (!Entry)
    {
        OutFailureReason = NSLOCTEXT("AkumasRPGFramework", "RepairNotOwned", "That item is no longer in your Inventory.");
        return EARPGCraftingResult::InvalidRepairItem;
    }
    const UARPGItemDefinition* Definition = Inventory->ResolveItemDefinition(*Entry);
    if (!Definition || !Definition->bUsesDurability || !Definition->bCanBeRepaired)
    {
        OutFailureReason = NSLOCTEXT("AkumasRPGFramework", "RepairUnsupported", "This item cannot be repaired.");
        return EARPGCraftingResult::InvalidRepairItem;
    }
    const float MaxDurability = FMath::Max(1.f, Definition->MaxDurability);
    if (Entry->Durability >= MaxDurability - KINDA_SMALL_NUMBER)
    {
        OutFailureReason = NSLOCTEXT("AkumasRPGFramework", "RepairAlreadyFull", "This item is already at full durability.");
        return EARPGCraftingResult::AlreadyFullyRepaired;
    }
    if (!GetRepairCost(ItemInstanceId, OutCost))
    {
        OutFailureReason = NSLOCTEXT("AkumasRPGFramework", "RepairInvalidCostConfigured", "This item's repair material configuration is invalid.");
        return EARPGCraftingResult::RepairNotConfigured;
    }
    if (OutCost.Num() == 0 && !Definition->bAllowFreeRepair)
    {
        OutFailureReason = NSLOCTEXT("AkumasRPGFramework", "RepairNoCostConfigured", "No repair materials are configured for this item.");
        return EARPGCraftingResult::RepairNotConfigured;
    }
    for (const FARPGItemAmount& Cost : OutCost)
    {
        const FName Id = ResolveAmountItemId(Cost);
        if (Id.IsNone() || Inventory->GetUnequippedItemCount(Id) < Cost.Quantity)
        {
            OutFailureReason = NSLOCTEXT("AkumasRPGFramework", "RepairMissingMaterials", "You do not have enough repair materials.");
            return EARPGCraftingResult::MissingRepairMaterials;
        }
    }
    return EARPGCraftingResult::Success;
}

bool UARPGCraftingComponent::CanRepairItem(FGuid ItemInstanceId, FText& OutFailureReason) const
{
    TArray<FARPGItemAmount> Cost;
    return ValidateRepair(ItemInstanceId, Cost, OutFailureReason) == EARPGCraftingResult::Success;
}

bool UARPGCraftingComponent::RepairItem(FGuid ItemInstanceId)
{
    if (!GetOwner() || !ItemInstanceId.IsValid()) return false;
    TArray<FARPGItemAmount> Cost;
    FText Reason;
    const EARPGCraftingResult Validation = ValidateRepair(ItemInstanceId, Cost, Reason);
    if (Validation != EARPGCraftingResult::Success)
    {
        OnRepairResult.Broadcast(Validation, ItemInstanceId, Reason);
        return false;
    }
    if (!GetOwner()->HasAuthority())
    {
        ServerRepairItem(ItemInstanceId);
        return true;
    }
    return RepairItemAuthority(ItemInstanceId);
}

bool UARPGCraftingComponent::RepairItemAuthority(FGuid ItemInstanceId)
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return false;
    TArray<FARPGItemAmount> Cost;
    FText Reason;
    const EARPGCraftingResult Validation = ValidateRepair(ItemInstanceId, Cost, Reason);
    if (Validation != EARPGCraftingResult::Success)
    {
        ClientRepairResult(Validation, ItemInstanceId, Reason);
        return false;
    }
    if (Cost.Num() > 0 && !ConsumeInputs(Cost, 1))
    {
        ClientRepairResult(EARPGCraftingResult::MissingRepairMaterials, ItemInstanceId, NSLOCTEXT("AkumasRPGFramework", "RepairMaterialsChanged", "The required repair materials are no longer available."));
        return false;
    }

    UARPGInventoryComponent* Inventory = GetInventory();
    if (!Inventory || !Inventory->RepairItemToFull(ItemInstanceId))
    {
        if (Cost.Num() > 0) RefundInputs(Cost, 1);
        ClientRepairResult(EARPGCraftingResult::Failed, ItemInstanceId, NSLOCTEXT("AkumasRPGFramework", "RepairFailed", "The item could not be repaired."));
        return false;
    }
    ClientRepairResult(EARPGCraftingResult::Success, ItemInstanceId, NSLOCTEXT("AkumasRPGFramework", "RepairSuccess", "Item repaired to full durability."));
    return true;
}

void UARPGCraftingComponent::BroadcastCraftingState()
{
    OnCraftingStateChanged.Broadcast();
}

void UARPGCraftingComponent::OnRep_CraftingState()
{
    OnCraftingStateChanged.Broadcast();
}

void UARPGCraftingComponent::ServerCraftRecipe_Implementation(UARPGRecipeDefinition* Recipe, int32 Count)
{
    if (IsCrafting())
    {
        ClientCraftingResult(EARPGCraftingResult::AlreadyCrafting, Recipe, ActiveRemainingCount, NSLOCTEXT("AkumasRPGFramework", "CraftServerAlreadyActive", "A craft is already in progress."));
        return;
    }
    StartCraftingAuthority(Recipe, Count);
}

void UARPGCraftingComponent::ServerCancelCrafting_Implementation()
{
    if (IsCrafting()) FinishCraftingAuthority(EARPGCraftingResult::Cancelled, NSLOCTEXT("AkumasRPGFramework", "CraftServerCancelled", "Crafting cancelled."), true);
}

void UARPGCraftingComponent::ServerRepairItem_Implementation(FGuid ItemInstanceId)
{
    RepairItemAuthority(ItemInstanceId);
}

void UARPGCraftingComponent::ClientCraftingResult_Implementation(EARPGCraftingResult Result, UARPGRecipeDefinition* Recipe, int32 RemainingCount, const FText& Message)
{
    if (Result == EARPGCraftingResult::Success && Recipe && !Recipe->CraftCompleteSound.IsNull() && GetOwner() && GetOwner()->GetNetMode() != NM_DedicatedServer)
    {
        if (USoundBase* Sound = Recipe->CraftCompleteSound.LoadSynchronous())
            UGameplayStatics::PlaySoundAtLocation(this, Sound, GetOwner()->GetActorLocation());
    }
    OnCraftingResult.Broadcast(Result, Recipe, RemainingCount, Message);
}

void UARPGCraftingComponent::ClientRepairResult_Implementation(EARPGCraftingResult Result, FGuid ItemInstanceId, const FText& Message)
{
    OnRepairResult.Broadcast(Result, ItemInstanceId, Message);
}

void UARPGCraftingComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME_CONDITION(UARPGCraftingComponent, ActiveRecipe, COND_OwnerOnly);
    DOREPLIFETIME_CONDITION(UARPGCraftingComponent, ActiveRemainingCount, COND_OwnerOnly);
    DOREPLIFETIME_CONDITION(UARPGCraftingComponent, ActiveCraftStartServerTime, COND_OwnerOnly);
    DOREPLIFETIME_CONDITION(UARPGCraftingComponent, ActiveCraftDuration, COND_OwnerOnly);
}
