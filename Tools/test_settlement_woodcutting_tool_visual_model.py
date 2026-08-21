"""Settlement villager contextual woodcutting tool presentation regression — v2.16.10."""
from pathlib import Path

root = Path(__file__).resolve().parents[1]
src = root / "Source" / "AkumasRPGFramework"

def read(rel):
    return (src / rel).read_text(errors="ignore")

def require(text, *tokens):
    for token in tokens:
        assert token in text, f"missing token: {token}"

settlement_h = read("Public/Data/ARPGSettlementDefinition.h")
resident_h = read("Public/Settlement/ARPGSettlementResidentComponent.h")
resident_cpp = read("Private/Settlement/ARPGSettlementResidentComponent.cpp")
equipment_h = read("Public/Components/ARPGEquipmentComponent.h")
equipment_cpp = read("Private/Components/ARPGEquipmentComponent.cpp")

# Data-driven authoring reuses the existing Item Definition visual/socket/transform contract.
require(
    settlement_h,
    "TSoftObjectPtr<UARPGItemDefinition> VillagerWoodcuttingToolItem",
    "bShowWoodcuttingToolWhileGoingToWork = true",
    "bPlayWoodcuttingToolEquipPresentation = true",
    'Category="Settlement|Woodcutting|Tool Presentation"',
)

# Contextual visuals are explicitly presentation-only: they must not alter authoritative Inventory state.
require(
    equipment_h,
    "CreateTransientEquipmentVisual",
    "DestroyTransientEquipmentVisual",
    "Never changes Inventory state",
)
require(
    equipment_cpp,
    "SpawnEquipmentVisualActor",
    "Visual->ConfigureFromItem(Definition)",
    "ResolveAttachSocket(Definition, CharacterMesh)",
    "Visual->SetActorRelativeTransform(Definition->EquippedRelativeTransform)",
    "CreateTransientEquipmentVisual",
    "DestroyTransientEquipmentVisual",
)
transient_start = equipment_cpp.index("AARPGEquipmentVisualActor* UARPGEquipmentComponent::CreateTransientEquipmentVisual")
transient_end = equipment_cpp.index("void UARPGEquipmentComponent::DestroyTransientEquipmentVisual", transient_start)
transient_destroy_end = equipment_cpp.index("void UARPGEquipmentComponent::DestroyEquipmentVisual", transient_end)
transient_block = equipment_cpp[transient_start:transient_destroy_end]
for forbidden in ("SetEquipped(", "EquipAuthority(", "UnequipAuthority(", "AddItem(", "RemoveItem("):
    assert forbidden not in transient_block, f"contextual work tool must not mutate Inventory/equipment state: {forbidden}"

# Resident state + CurrentWorkTree are the replicated truth used to reconstruct the local visual on every client.
require(
    resident_h,
    "ReplicatedUsing=OnRep_CurrentWorkTree",
    "ActiveWoodcuttingToolVisual",
    "OnWoodcuttingToolVisualChanged",
    "RefreshWoodcuttingToolVisual",
    "IsWoodcuttingToolVisualActive() const;",
    "OnRep_CurrentWorkTree",
)
assert "IsWoodcuttingToolVisualActive() const {" not in resident_h, \
    "forward-declared visual UObject validity must not be implemented inline in the public header"

should_start = resident_cpp.index("bool UARPGSettlementResidentComponent::ShouldDisplayWoodcuttingTool() const")
should_end = resident_cpp.index("UARPGItemDefinition* UARPGSettlementResidentComponent::ResolveWoodcuttingToolDefinition", should_start)
should = resident_cpp[should_start:should_end]
require(
    should,
    "!SettlementHub || !CurrentWorkTree",
    "ResidentState == EARPGSettlementResidentState::Woodcutting",
    "bShowWoodcuttingToolWhileGoingToWork",
    "ResidentState == EARPGSettlementResidentState::GoingToWork",
)
assert "EARPGSettlementResidentState::Roaming" not in should
assert "EARPGSettlementResidentState::AtHome" not in should
assert "EARPGSettlementResidentState::ReturningHome" not in should

# Lifecycle: work state creates the visual; leaving work tears it down; replication order is self-healing.
require(
    resident_cpp,
    "SetWoodcuttingToolVisualActive(ShouldDisplayWoodcuttingTool(), true)",
    "Equipment->CreateTransientEquipmentVisual",
    "Equipment->DestroyTransientEquipmentVisual",
    "OnRep_Hub() { RefreshWoodcuttingToolVisual(); }",
    "OnRep_CurrentWorkTree() { RefreshWoodcuttingToolVisual(); }",
)
set_state_start = resident_cpp.index("void UARPGSettlementResidentComponent::SetResidentState")
set_state_end = resident_cpp.index("void UARPGSettlementResidentComponent::MulticastPlayWoodcuttingMontage", set_state_start)
set_state = resident_cpp[set_state_start:set_state_end]
assert set_state.index("ResidentState = NewState;") < set_state.index("RefreshWoodcuttingToolVisual();")

stop_start = resident_cpp.index("void UARPGSettlementResidentComponent::StopWoodcutting")
stop_end = resident_cpp.index("void UARPGSettlementResidentComponent::DepositTreeRewardsToHub", stop_start)
stop = resident_cpp[stop_start:stop_end]
assert stop.index("CurrentWorkTree = nullptr;") < stop.index("RefreshWoodcuttingToolVisual();"), \
    "work tree must be cleared before contextual axe visibility is re-evaluated"

# Minimal state model: axe is held while travelling/chopping and absent during free roam/home states.
def show_tool(state, has_tree=True, show_while_travelling=True):
    if not has_tree:
        return False
    if state == "Woodcutting":
        return True
    return show_while_travelling and state == "GoingToWork"

assert show_tool("GoingToWork")
assert show_tool("Woodcutting")
assert not show_tool("Roaming")
assert not show_tool("AtHome")
assert not show_tool("ReturningHome")
assert not show_tool("Homeless")
assert not show_tool("GoingToWork", has_tree=False)
assert not show_tool("GoingToWork", show_while_travelling=False)
assert show_tool("Woodcutting", show_while_travelling=False)

print("Settlement villager contextual woodcutting tool presentation regression model: PASS")
