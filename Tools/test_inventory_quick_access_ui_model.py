#!/usr/bin/env python3
"""Static/deterministic guard for v2.12.2 ready Inventory + Quick Access UI."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PUB = ROOT / "Source" / "AkumasRPGFramework" / "Public"
PRI = ROOT / "Source" / "AkumasRPGFramework" / "Private"

component_h = (PUB / "Components" / "ARPGInventoryUIComponent.h").read_text(errors="replace")
component_cpp = (PRI / "Components" / "ARPGInventoryUIComponent.cpp").read_text(errors="replace")
widgets_h = (PUB / "UI" / "ARPGInventoryWidgets.h").read_text(errors="replace")
widgets_cpp = (PRI / "UI" / "ARPGInventoryWidgets.cpp").read_text(errors="replace")
qa_h = (PUB / "Components" / "ARPGQuickAccessComponent.h").read_text(errors="replace")
qa_cpp = (PRI / "Components" / "ARPGQuickAccessComponent.cpp").read_text(errors="replace")
character_h = (PUB / "Actors" / "ARPGCharacter.h").read_text(errors="replace")
character_cpp = (PRI / "Actors" / "ARPGCharacter.cpp").read_text(errors="replace")
readme = (ROOT / "README.md").read_text(errors="replace")
changelog = (ROOT / "Docs" / "CHANGELOG.md").read_text(errors="replace")
doc = (ROOT / "Docs" / "INVENTORY_UI.md").read_text(errors="replace")

# Base character integration and one-call input API.
assert "TObjectPtr<UARPGInventoryUIComponent> InventoryUI" in character_h
assert 'CreateDefaultSubobject<UARPGInventoryUIComponent>(TEXT("InventoryUI"))' in character_cpp
for fn in ("OpenInventoryUI", "CloseInventoryUI", "ToggleInventoryUI", "IsInventoryUIOpen"):
    assert fn in character_h and f"AARPGCharacter::{fn}" in character_cpp
assert "InventoryUI->HandleOwnerControlChanged()" in character_cpp

# Local-only, no permanent Tick or UI replication.
assert "PrimaryComponentTick.bCanEverTick = false" in component_cpp
assert "SetIsReplicatedByDefault(false)" in component_cpp
assert "IsLocallyControlled()" in component_cpp
assert "IsLocalController()" in component_cpp
assert "NM_DedicatedServer" in component_cpp
assert "DOREPLIFETIME" not in component_cpp

# UE5.8 UHT must not receive unsupported pixel unit metadata on reflected floats.
assert 'Units="px"' not in component_h

# Ready native classes and exposed Blueprint replacement points.
for token in (
    "TSubclassOf<UARPGInventoryPanelWidget> InventoryWidgetClass",
    "TSubclassOf<UARPGQuickAccessBarWidget> QuickAccessWidgetClass",
    "TSubclassOf<UARPGInventoryItemSlotWidget> InventorySlotWidgetClass",
    "TSubclassOf<UARPGInventoryItemSlotWidget> QuickAccessSlotWidgetClass",
    "bAutoCreateQuickAccessHUD = true",
    "bShowQuickAccessHUD = true",
    "QuickAccessZOrder = 100",
    "InventoryZOrder = 80",
):
    assert token in component_h, token
for token in (
    "InventoryWidgetClass = UARPGInventoryPanelWidget::StaticClass()",
    "QuickAccessWidgetClass = UARPGQuickAccessBarWidget::StaticClass()",
    "InventorySlotWidgetClass = UARPGInventoryItemSlotWidget::StaticClass()",
    "QuickAccessSlotWidgetClass = UARPGInventoryItemSlotWidget::StaticClass()",
):
    assert token in component_cpp

# Inventory projection must come from real runtime entries and resolved definitions.
for token in (
    "Inventory->Items[Index]",
    "Entry.InstanceId",
    "Entry.Quantity",
    "Inventory->ResolveItemDefinition(Entry)",
    "OutView.bEquipped = Entry.bEquipped",
    "OutView.Durability = Entry.Durability",
):
    assert token in component_cpp, token

# Hotbar projection stays on existing canonical Quick Access view path.
assert "QuickAccess->GetSlotView(SlotNumber, QuickView)" in component_cpp
for token in ("QuickView.bAssigned", "QuickView.bOwned", "QuickView.bActive", "QuickView.CooldownRemaining"):
    assert token in component_cpp

# Functional drag/drop rules, not decorative slots.
assert "UARPGInventoryDragDropOperation" in widgets_h
assert "NativeOnDragDetected" in widgets_h and "NativeOnDrop" in widgets_h and "NativeOnDragCancelled" in widgets_h
assert "AssignInventoryItemToQuickAccess(Operation->ItemInstanceId" in widgets_cpp
assert "SwapQuickAccessSlots(Operation->SourceSlotNumber" in widgets_cpp
assert "ClearQuickAccessSlot(Operation->SourceSlotNumber" in widgets_cpp
assert "DetectDragIfPressed" in widgets_cpp
assert "NativeOnPreviewMouseButtonDown" in widgets_h and "UARPGInventoryItemSlotWidget::NativeOnPreviewMouseButtonDown" in widgets_cpp
assert "SlotView.Source != EARPGInventoryUISlotSource::Inventory" in widgets_cpp
assert "InventoryUIScreenDim" in widgets_cpp and "ScreenDim->SetVisibility(ESlateVisibility::HitTestInvisible)" in widgets_cpp
assert "InventoryGrid->SetVisibility(ESlateVisibility::SelfHitTestInvisible)" in widgets_cpp
assert "SlotSize->SetVisibility(ESlateVisibility::SelfHitTestInvisible)" in widgets_cpp
assert "DefaultDragVisual" in widgets_cpp
assert "Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible)" in widgets_cpp
# The higher-Z Quick Access UUserWidget itself spans the viewport. It must also be pass-through,
# not merely its inner Canvas, or it blocks every lower-Z Inventory control.
assert "UARPGQuickAccessBarWidget::NativeOnInitialized" in widgets_cpp
assert "SetVisibility(ESlateVisibility::SelfHitTestInvisible);" in widgets_cpp
assert "ActiveQuickAccessWidget->SetVisibility(bShowQuickAccessHUD ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed)" in component_cpp
assert "ActiveQuickAccessWidget->SetVisibility(ESlateVisibility::SelfHitTestInvisible)" in component_cpp
assert "ActiveQuickAccessWidget->SetVisibility(bShowQuickAccessHUD ? ESlateVisibility::Visible" not in component_cpp
assert "CreateWidget<UARPGInventoryItemSlotWidget>(PC, GetClass())" in widgets_cpp

# Drag-away active equipment must be one atomic Quick Access authority path.
assert "ClearSlotAndUnequipActive" in qa_h
assert "ServerClearSlotAndUnequipActive" in qa_h
assert "ClearSlotAndUnequipActiveAuthority" in qa_h
assert "const FGuid EquippedInstanceId = Entry->InstanceId" in qa_cpp
assert "Equipment->UnequipItem(EquippedInstanceId)" in qa_cpp
assert "return ClearSlotAuthority(SlotNumber);" in qa_cpp
assert "ServerClearSlotAndUnequipActive_Implementation" in qa_cpp
assert "QuickAccess->ClearSlotAndUnequipActive(SlotNumber)" in component_cpp

# Quick Access remains layered above Inventory so inventory-to-hotbar drag is reachable.
assert "QuickAccessZOrder = 100" in component_h and "InventoryZOrder = 80" in component_h
assert "AddToPlayerScreen(QuickAccessZOrder)" in component_cpp
assert "AddToPlayerScreen(InventoryZOrder)" in component_cpp

# Input/cursor and Close button work immediately but remain opt-out.
for token in ("bManageInputMode", "bShowMouseCursorWhileOpen", "bRestoreGameOnlyInputOnClose"):
    assert token in component_h
assert "FInputModeGameAndUI" in component_cpp
assert "FInputModeGameOnly" in component_cpp
assert "CloseButton->OnClicked.AddUniqueDynamic" in widgets_cpp
assert "RequestCloseInventoryUI" in widgets_cpp

# Event-driven normal refresh + timer only for real cooldown animation.
assert "OnInventoryChanged.AddUniqueDynamic" in component_cpp
assert "OnQuickAccessChanged.AddUniqueDynamic" in component_cpp
assert "OnActiveQuickAccessSlotChanged.AddUniqueDynamic" in component_cpp
assert "bNeedsCooldownRefresh" in component_cpp
assert "SetTimer(CooldownRefreshTimer" in component_cpp
assert "ClearTimer(CooldownRefreshTimer)" in component_cpp

# Custom zero-graph standard child bindings/events.
for token in (
    "InventoryGrid", "CapacityText", "SelectedItemNameText", "SelectedItemDetailsText", "CloseButton",
    "QuickAccessBox", "SlotBorder", "ItemIcon", "QuantityText", "SlotNumberText", "EquippedText", "CooldownBar",
    "BP_OnInventoryUIRefreshed", "BP_OnInventorySelectionChanged", "BP_OnQuickAccessUIRefreshed", "BP_OnInventorySlotUpdated",
):
    assert token in widgets_h and token in widgets_cpp, token

# README splash stays protected at the top and v2.12 docs exist.
lines = readme.splitlines()
splash = '<img width="1672" height="941" alt="AumaRPGFWSplash"'
assert any(splash in line for line in lines[:6])
assert "v2.12.2-alpha — Inventory Viewport Hit-Test Ownership Fix" in changelog
for token in ("Inventory item -> Quick Access slot", "Quick Access slot -> Quick Access slot", "Clear Slot And Unequip Active", "No permanent UI/component Tick"):
    assert token in doc, token

print("Complete Inventory + Quick Access UI model: PASS")
