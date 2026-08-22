from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "Source" / "AkumasRPGFramework"

def text(rel):
    return (SRC / rel).read_text(errors="replace")

recipe = text("Public/Data/ARPGRecipeDefinition.h")
item = text("Public/Data/ARPGItemDefinition.h")
craft_h = text("Public/Components/ARPGCraftingComponent.h")
craft = text("Private/Components/ARPGCraftingComponent.cpp")
inv_h = text("Public/Components/ARPGInventoryComponent.h")
inv = text("Private/Components/ARPGInventoryComponent.cpp")
equip = text("Private/Components/ARPGEquipmentComponent.cpp")
combat = text("Private/Components/ARPGCombatComponent.cpp")
wood = text("Private/Components/ARPGWoodcuttingComponent.cpp")
ui_h = text("Public/UI/ARPGInventoryWidgets.h")
ui = text("Private/UI/ARPGInventoryWidgets.cpp")
craft_ui_h = text("Public/UI/ARPGCraftingWidgets.h")
craft_ui = text("Private/UI/ARPGCraftingWidgets.cpp")
ui_comp_h = text("Public/Components/ARPGInventoryUIComponent.h")
ui_comp = text("Private/Components/ARPGInventoryUIComponent.cpp")
station = text("Private/Crafting/ARPGCraftingStationActor.cpp")
char_h = text("Public/Actors/ARPGCharacter.h")
char_cpp = text("Private/Actors/ARPGCharacter.cpp")
quick = text("Private/Components/ARPGQuickAccessComponent.cpp")
save_h = text("Public/Save/ARPGSaveGame.h")
save_cpp = text("Private/Subsystems/ARPGSaveSubsystem.cpp")
types_h = text("Public/ARPGTypes.h")

# Recipe authoring / shared format.
assert "TObjectPtr<UARPGItemDefinition> Item" in recipe
assert "Item Id (Legacy / Optional)" in recipe
assert "bAllowPlayerCrafting" in recipe and "MaxBatchSize" in recipe
assert "ARPGResolveRecipeAmountId" in station and "Amount.Item" in station

# Player crafting authority/security/timing.
for token in ["PlayerRecipes", "bAllowUnlistedRecipeRequests = false", "ServerCraftRecipe", "ServerCancelCrafting", "ServerRepairItem", "COND_OwnerOnly"]:
    assert token in craft_h or token in craft
assert "PrimaryComponentTick.bCanEverTick = false" in craft
assert "Recipe->RequiredStationTag.IsValid()" in craft
assert "bCurrentInputsCommitted" in craft and "RefundInputs" in craft
assert "SetTimer(CraftCompletionTimer" in craft
assert "Simulate the exact runtime order craft-by-craft" in craft
assert "CanFitCommittedOutputs" in craft_h and "CanFitCommittedOutputs" in craft
assert "inputs for this craft were already removed" in craft.lower()
assert "Recipes with no ingredients can only be crafted one at a time" in craft
assert "AggregateAmounts" in craft_h and "AggregateAmounts" in craft
assert "This recipe has no valid crafted output." in craft
assert "This recipe contains an invalid ingredient entry." in craft

# Durability is exact-instance, unique and context-authorized.
for token in ["bUsesDurability", "MaxDurability", "bLoseDurabilityOnCombatHit", "bLoseDurabilityOnGatheringHit", "RepairInputs"]:
    assert token in item
for token in ["GetItemDurability", "DamageItemDurability", "RepairItemDurability", "RepairItemToFull", "OnItemDurabilityChanged"]:
    assert token in inv_h and token in inv
assert "ExplicitDefinition && ExplicitDefinition->bUsesDurability" in inv
assert "Definition->bUsesDurability && Entry.Durability <= KINDA_SMALL_NUMBER" in inv

# Durable storage/container transfer must preserve exact runtime instance state rather than
# reconstructing a fresh full-durability item through AddItemAuthority.
transfer_pos = inv.find("bool UARPGInventoryComponent::TransferItemTo")
transfer_end = inv.find("int32 UARPGInventoryComponent::GetItemCount", transfer_pos)
assert transfer_pos >= 0 and transfer_end > transfer_pos
transfer_body = inv[transfer_pos:transfer_end]
for token in (
    "if (Definition && Definition->bUsesDurability)",
    "GetUnequippedItemCount(ItemId) < Quantity",
    "FARPGInventoryEntry Moved = Entry;",
    "Destination->Items.Append(MovedEntries);",
):
    assert token in transfer_body
# The durable branch must occur before the legacy remove/re-add transfer path.
assert transfer_body.find("if (Definition && Definition->bUsesDurability)") < transfer_body.find("RemoveItemAuthority(ItemId, Quantity)")

# Durability persists in new saves and legacy pre-durability saves are migrated once to authored max.
assert "SaveGame) float Durability" in types_h
assert save_h.count("SaveVersion = 5") >= 1 and "SaveVersion = 10" in save_h  # character schema remains 5; world schema advances independently; v6 Window migration is retained
assert "ARPGMigrateLegacyInventoryDurability" in save_cpp
assert "Save->SaveVersion < 5" in save_cpp
assert "Save->SaveVersion>=6 ? R.bWindowOpen : false" in save_cpp
assert "Save->SaveVersion<4" in save_cpp
assert "PersonalCraftingState" in types_h and "MakeCraftingSaveState" in craft_h and "RestoreCraftingSaveState" in craft_h
assert "D.PersonalCraftingState = Character->Crafting->MakeCraftingSaveState()" in save_cpp
assert "Character->Crafting->RestoreCraftingSaveState(D.PersonalCraftingState)" in save_cpp
assert "bCurrentInputsCommitted" in craft and "SavedState.ProgressSeconds" in craft
# StartCraftingAuthority must not broadcast an active state until BeginNextCraftAuthority commits inputs.
start_pos = craft.find("bool UARPGCraftingComponent::StartCraftingAuthority")
begin_pos = craft.find("void UARPGCraftingComponent::BeginNextCraftAuthority", start_pos)
start_body = craft[start_pos:begin_pos]
assert "BeginNextCraftAuthority();" in start_body
assert "BroadcastCraftingState();" not in start_body
assert "bLoseDurabilityOnCombatHit" in equip
assert "Info.AppliedDamage > KINDA_SMALL_NUMBER" in combat
assert "ToolInstanceId" in wood and "Tree->ApplyChop" in wood and "bLoseDurabilityOnGatheringHit" in wood and "DamageItemDurability" in wood

# Broken Quick Access equipment is rejected before active-slot mutation / previous-equipment handoff.
broken_guard = "RequestedDefinition->bUsesDurability && RequestedEntry->Durability <= KINDA_SMALL_NUMBER"
assert broken_guard in quick
activation_pos = quick.find("EARPGQuickAccessResult UARPGQuickAccessComponent::ActivateSlotAuthority")
assert activation_pos >= 0
assert quick.find(broken_guard, activation_pos) < quick.find("FGuid PreviousActiveInstanceId", activation_pos)
assert "Action == EARPGQuickAccessAction::Equip && Definition->bUsesDurability && Entry->Durability <= KINDA_SMALL_NUMBER" in quick

# Repair transaction.
for token in ["GetRepairCost", "ValidateRepair", "MissingRepairMaterials", "RepairItemToFull", "bScaleRepairCostByMissingDurability"]:
    assert token in craft or token in item
assert "RefundInputs(Cost, 1)" in craft

# Character and shared ready UI integration.
assert "TObjectPtr<UARPGCraftingComponent> Crafting" in char_h
for token in ["CraftRecipe", "RepairInventoryItem", "OpenCraftingUI"]:
    assert token in char_h and token in char_cpp
for token in ["EARPGItemManagementTab", "InventoryTabButton", "CraftingTabButton", "MainTabSwitcher", "CraftingPageHost", "DurabilityBar", "BrokenText"]:
    assert token in ui_h and token in ui
for token in ["UARPGCraftingPanelWidget", "UARPGCraftingRecipeRowWidget", "UARPGRepairItemRowWidget", "CraftProgressBar", "RepairButton"]:
    assert token in craft_ui_h and token in craft_ui
for token in ["CraftingWidgetClass", "CraftingRecipeRowWidgetClass", "RepairItemRowWidgetClass", "OpenCraftingUI", "HandleCraftingStateChanged"]:
    assert token in ui_comp_h and token in ui_comp
assert "Crafting->IsCrafting()" in ui_comp

# README splash regression and major version docs.
readme = (ROOT / "README.md").read_text(errors="replace")
changelog = (ROOT / "Docs" / "CHANGELOG.md").read_text(errors="replace")
assert '<img width="1672" height="941" alt="AumaRPGFWSplash"' in readme[:500]
assert "v2.14.0-alpha — Player Crafting, Functional Durability, Repair & Tabbed Item Management UI" in changelog
assert (ROOT / "Docs/CRAFTING_DURABILITY_REPAIR.md").exists()

print("Player crafting + durability + repair + tabbed Item Management UI model: PASS")
