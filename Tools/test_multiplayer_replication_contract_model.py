"""High-level replication/authority regression audit for the ready multiplayer framework — v2.18.1."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
P = ROOT / "Source/AkumasRPGFramework/Private"
H = ROOT / "Source/AkumasRPGFramework/Public"

def read(path):
    return path.read_text(errors="replace")

# The ready player and core mutable components must be network-capable by default.
character = read(P / "Actors/ARPGCharacter.cpp")
assert "bReplicates = true" in character

replicated_components = {
    "Inventory": P / "Components/ARPGInventoryComponent.cpp",
    "QuickAccess": P / "Components/ARPGQuickAccessComponent.cpp",
    "Equipment": P / "Components/ARPGEquipmentComponent.cpp",
    "Combat": P / "Components/ARPGCombatComponent.cpp",
    "Stats": P / "Components/ARPGStatsComponent.cpp",
    "Progression": P / "Components/ARPGProgressionComponent.cpp",
    "Interaction": P / "Components/ARPGInteractionComponent.cpp",
    "Building": P / "Building/ARPGBuildingComponent.cpp",
    "Mining": P / "Components/ARPGMiningComponent.cpp",
    "Woodcutting": P / "Components/ARPGWoodcuttingComponent.cpp",
    "Skills": P / "Components/ARPGSkillComponent.cpp",
    "Quests": P / "Components/ARPGQuestComponent.cpp",
    "Currencies": P / "Components/ARPGCurrencyComponent.cpp",
    "Crafting": P / "Components/ARPGCraftingComponent.cpp",
    "ItemUse": P / "Components/ARPGItemUseComponent.cpp",
    "Faction": P / "Components/ARPGFactionComponent.cpp",
    "Slayer": P / "Components/ARPGSlayerComponent.cpp",
    "BattlePets": P / "Components/ARPGBattlePetComponent.cpp",
}
for name, path in replicated_components.items():
    text = read(path)
    assert "SetIsReplicatedByDefault(true)" in text, f"{name} component lost default replication"

# Private owner state stays owner-only where intended.
quick = read(P / "Components/ARPGQuickAccessComponent.cpp")
assert "DOREPLIFETIME_CONDITION(UARPGQuickAccessComponent, QuickAccessSlots, COND_OwnerOnly)" in quick
assert "DOREPLIFETIME_CONDITION(UARPGQuickAccessComponent, ActiveSlotNumber, COND_OwnerOnly)" in quick
crafting = read(P / "Components/ARPGCraftingComponent.cpp")
assert "COND_OwnerOnly" in crafting

# Intent-changing client calls must cross server RPCs for the principal ready gameplay paths.
for path, tokens in {
    H / "Components/ARPGQuickAccessComponent.h": ("UFUNCTION(Server, Reliable)", "ServerSelectSlot", "ServerActivateSlot"),
    H / "Components/ARPGEquipmentComponent.h": ("UFUNCTION(Server, Reliable)", "ServerEquipItem"),
    H / "Components/ARPGCombatComponent.h": ("UFUNCTION(Server, Reliable)", "ServerPerformBasicAttack"),
    H / "Components/ARPGInteractionComponent.h": ("UFUNCTION(Server, Reliable)", "ServerDepositToStorage", "ServerQueueCraft"),
    H / "Components/ARPGMiningComponent.h": ("UFUNCTION(Server, Reliable)", "ServerStartMining"),
    H / "Components/ARPGWoodcuttingComponent.h": ("UFUNCTION(Server, Reliable)", "ServerStartWoodcutting"),
    H / "Building/ARPGBuildingComponent.h": ("UFUNCTION(Server, Reliable)", "ServerPlacePiece"),
}.items():
    text = read(path)
    for token in tokens:
        assert token in text, f"{path.name}: missing {token}"

# Principal persistent/interactive world actors replicate authoritative state.
for path in (
    P / "Building/ARPGBuildPieceActor.cpp",
    P / "Gathering/ARPGTree.cpp",
    P / "Gathering/ARPGMineableRock.cpp",
    P / "Settlement/ARPGSettlementHubActor.cpp",
):
    assert "bReplicates = true" in read(path), f"{path.name} lost actor replication"

# Manual SaveNow is a client->host request, but native AI explicitly avoids this replicated subobject overhead.
persistence_h = read(H / "Components/ARPGPersistenceComponent.h")
persistence_cpp = read(P / "Components/ARPGPersistenceComponent.cpp")
ai_cpp = read(P / "Actors/ARPGAICharacter.cpp")
assert "SetIsReplicatedByDefault(true)" in persistence_cpp
assert "ServerRequestSaveNow" in persistence_h + persistence_cpp
assert "Persistence->SetIsReplicated(false)" in ai_cpp

# This audit intentionally protects framework-owned network contracts, not arbitrary project Blueprint variables.
print("multiplayer replication/authority contract model: PASS")
