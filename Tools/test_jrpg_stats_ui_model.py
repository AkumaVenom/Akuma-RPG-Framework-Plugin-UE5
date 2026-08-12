#!/usr/bin/env python3
"""Static/model guard for v2.11.0 complete ready-to-use JRPG Stats UI."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
public = ROOT / "Source/AkumasRPGFramework/Public"
private = ROOT / "Source/AkumasRPGFramework/Private"

component_h = (public / "Components/ARPGStatsUIComponent.h").read_text(errors="replace")
component_cpp = (private / "Components/ARPGStatsUIComponent.cpp").read_text(errors="replace")
widget_h = (public / "UI/ARPGStatsPanelWidget.h").read_text(errors="replace")
widget_cpp = (private / "UI/ARPGStatsPanelWidget.cpp").read_text(errors="replace")
character_h = (public / "Actors/ARPGCharacter.h").read_text(errors="replace")
character_cpp = (private / "Actors/ARPGCharacter.cpp").read_text(errors="replace")
readme = (ROOT / "README.md").read_text(errors="replace")

# Inherited authoring + one-call player API.
assert "TObjectPtr<UARPGStatsUIComponent> StatsUI" in character_h
assert 'CreateDefaultSubobject<UARPGStatsUIComponent>(TEXT("StatsUI"))' in character_cpp
for fn in ("OpenStatsUI", "CloseStatsUI", "ToggleStatsUI", "IsStatsUIOpen"):
    assert fn in character_h and f"AARPGCharacter::{fn}" in character_cpp

# Local-only, no permanent tick or replication work.
assert "PrimaryComponentTick.bCanEverTick = false" in component_cpp
assert "SetIsReplicatedByDefault(false)" in component_cpp
assert "IsLocallyControlled()" in component_cpp
assert "IsLocalController()" in component_cpp
assert "NM_DedicatedServer" in component_cpp
assert "SetTimer(RefreshTimerHandle" in component_cpp
assert "ClearTimer(RefreshTimerHandle)" in component_cpp

# Ready-to-use native widget fallback and exposed custom class.
assert "TSubclassOf<UARPGStatsPanelWidget> StatsWidgetClass" in component_h
assert "StatsWidgetClass = UARPGStatsPanelWidget::StaticClass()" in component_cpp
assert "CreateWidget<UARPGStatsPanelWidget>" in component_cpp
assert "AddToPlayerScreen" in component_cpp

# Complete data snapshot coverage.
for token in (
    "CharacterName", "Level", "CurrentXP", "XPRequiredForNextLevel", "Health", "MaxHealth", "Mana", "MaxMana",
    "Stamina", "MaxStamina", "PrimaryStats", "AllocatedPoints", "DerivedStats", "UnspentAttributePoints",
    "TotalAttributePointsEarned", "bJRPGStatSystemEnabled",
):
    assert token in widget_h, f"missing Stats UI snapshot field {token}"

for token in (
    "MeleeAttackPower", "RangedAttackPower", "MagicAttackPower", "PhysicalDefense", "MagicDefense", "Accuracy",
    "Evasion", "MagicEvasion", "Speed", "CriticalChance", "CriticalDamageMultiplier", "AttackSpeedMultiplier",
    "MovementSpeedMultiplier",
):
    assert token in component_cpp or token in widget_cpp, f"derived stat not presented: {token}"

# Native complete panel and zero-graph binding names.
for token in (
    "CharacterNameText", "LevelText", "XPText", "XPBar", "HealthText", "HealthBar", "ManaText", "ManaBar",
    "StaminaText", "StaminaBar", "AttributePointsText", "StrengthText", "VitalityText", "MagicText", "SpiritText",
    "DexterityText", "LuckText", "CloseButton", "StrengthPlusButton", "VitalityPlusButton", "MagicPlusButton",
    "SpiritPlusButton", "DexterityPlusButton", "LuckPlusButton", "BP_OnStatsUIUpdated",
):
    assert token in widget_h and token in widget_cpp, f"missing native/custom binding {token}"

# Close and allocation buttons must be behaviorally wired, not decorative.
assert "RequestCloseStatsUI" in widget_cpp
assert "CloseButton->OnClicked.AddUniqueDynamic" in widget_cpp
for stat in ("Strength", "Vitality", "Magic", "Spirit", "Dexterity", "Luck"):
    assert f"EARPGPrimaryStat::{stat}" in widget_cpp
assert "SpendAttributePoints(Stat, 1)" in widget_cpp

# Existing server-authoritative Stats system remains the source of truth.
assert "Stats->CanSpendAttributePoints" in widget_cpp
assert "Stats->GetEffectiveLevel()" in component_cpp
assert "Stats->GetMeleeAttackPower()" in component_cpp
assert "Stats->GetMagicEvasion()" in component_cpp

# Input handling makes native Close/+ buttons immediately usable while remaining opt-out.
for token in ("bManageInputMode", "bShowMouseCursorWhileOpen", "bRestoreGameOnlyInputOnClose"):
    assert token in component_h
assert "FInputModeGameAndUI" in component_cpp
assert "FInputModeGameOnly" in component_cpp

# README splash is a protected top-of-file contract.
lines = readme.splitlines()
splash = '<img width="1672" height="941" alt="AumaRPGFWSplash" src="https://github.com/user-attachments/assets/42618c54-4728-4a36-9d18-9e2b8181c455" />'
assert splash in lines[:6], "GitHub README splash must remain at the top"
assert "2.11.0-alpha complete ready-to-use JRPG Stats UI" in readme

print("Complete JRPG Stats UI model: PASS")
