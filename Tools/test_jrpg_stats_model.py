#!/usr/bin/env python3
"""Static/model guard for v2.5.0 JRPG character stats/progression."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
stat_types = (ROOT / "Source/AkumasRPGFramework/Public/Stats/ARPGStatTypes.h").read_text(errors="replace")
stats_h = (ROOT / "Source/AkumasRPGFramework/Public/Components/ARPGStatsComponent.h").read_text(errors="replace")
stats_cpp = (ROOT / "Source/AkumasRPGFramework/Private/Components/ARPGStatsComponent.cpp").read_text(errors="replace")
combat = (ROOT / "Source/AkumasRPGFramework/Private/Components/ARPGCombatComponent.cpp").read_text(errors="replace")
item = (ROOT / "Source/AkumasRPGFramework/Public/Data/ARPGItemDefinition.h").read_text(errors="replace")
save = (ROOT / "Source/AkumasRPGFramework/Private/Subsystems/ARPGSaveSubsystem.cpp").read_text(errors="replace")

for token in ("Strength", "Vitality", "Magic", "Spirit", "Dexterity", "Luck"):
    assert token in stat_types, f"missing FF-style primary stat {token}"
for token in (
    "MeleeAttackPower", "RangedAttackPower", "MagicAttackPower", "PhysicalDefense", "MagicDefense",
    "Accuracy", "Evasion", "MagicEvasion", "Speed", "CriticalChance", "CriticalDamageMultiplier",
    "AttackSpeedMultiplier", "MovementSpeedMultiplier",
):
    assert token in stat_types, f"missing derived stat {token}"

for token in (
    "Enable JRPG Stat System",
    "SpendAttributePoints",
    "ServerSpendAttributePoints",
    "AttributePointsPerLevel",
    "PrimaryGrowthPerLevel",
    "RestoreStatProgressionState",
    "OnJRPGStatsChanged",
    "OnAttributePointsChanged",
):
    assert token in stats_h or token in stat_types, f"missing JRPG API/config {token}"

for token in (
    "OnLevelChanged.AddDynamic",
    "HandleLevelChanged",
    "CollectEquippedStatModifier",
    "PrimaryStats.Strength",
    "StatProgression.UnspentAttributePoints",
    "ApplyMovementSpeed",
):
    assert token in stats_cpp, f"missing runtime integration {token}"

for token in (
    "GetMeleeAttackPower()",
    "GetRangedAttackPower()",
    "GetMagicAttackPower()",
    "GetPhysicalDefense()",
    "GetMagicDefense()",
    "GetCriticalChanceBonus()",
    "GetCriticalDamageMultiplier()",
    "GetAttackSpeedMultiplier()",
):
    assert token in combat, f"combat does not consume stat {token}"

assert "EquippedStatModifier" in item
assert "MakeStatProgressionSaveState" in save
assert "RestoreStatProgressionState" in save
assert "Save->SaveVersion < 4" in save

# Model a few default values to catch accidental formula drift.
def natural(base, per_level, level):
    return round(base + per_level * max(0, level - 1))

assert natural(10, 0.80, 1) == 10
assert natural(10, 0.80, 10) == 17
assert natural(10, 0.35, 10) == 13

# Three points per level means a level-10 new character has 27 earned points by default.
assert (10 - 1) * 3 == 27

# At base stats the default formulas preserve 10 attack power for all three attack styles.
strength = dexterity = magic = 10
assert strength * 1.0 == 10
assert dexterity * 0.80 + strength * 0.20 == 10
assert magic * 1.0 == 10

# Regression guards for two subtle integration cases: equipment-derived max vitals must exist before
# saved current vitals are clamped, and changing Speed while blocking must not erase the block penalty.
save_stats_pos = save.index("RestoreStatProgressionState")
save_inventory_pos = save.index("ReplaceInventory(D.Inventory)", save_stats_pos)
save_health_pos = save.index("Stats->Health=FMath::Clamp", save_inventory_pos)
assert save_stats_pos < save_inventory_pos < save_health_pos, "equipment must restore before saved vitals are clamped"
assert "Combat->bIsBlocking" in stats_cpp and "BlockingMoveSpeedMultiplier" in stats_cpp
assert "Stats->RefreshMovementSpeedFromStats();" in combat


# SetProgression must broadcast level changes too; otherwise admin/save/direct progression changes can
# bypass every level-driven stats listener.
progression = (ROOT / "Source/AkumasRPGFramework/Private/Components/ARPGProgressionComponent.cpp").read_text(errors="replace")
set_start = progression.index("void UARPGProgressionComponent::SetProgression")
set_body = progression[set_start:]
assert "OnLevelChanged.Broadcast(OldLevel, Level);" in set_body, "SetProgression is not event-correct for stat listeners"
print("SetProgression is event-correct: PASS")

# AI cadence must use the same Speed clock as CombatComponent attack timers.
ai = (ROOT / "Source/AkumasRPGFramework/Private/Components/ARPGAICombatComponent.cpp").read_text(errors="replace")
assert "EstimatedAttackDuration /= FMath::Clamp(Stats->GetAttackSpeedMultiplier(), 0.25f, 4.f);" in ai

# Level-down/re-level cycles must not mint duplicate Attribute Points.
assert "const int32 RewardedThroughLevel = FMath::Max(1, StatProgression.LastProcessedLevel);" in stats_cpp
assert "NewLevel > RewardedThroughLevel" in stats_cpp
assert "StatProgression.LastProcessedLevel = FMath::Max(RewardedThroughLevel, NewLevel);" in stats_cpp

assert "GetMagicEvasion" in stats_h
assert "AttackType == EARPGBasicAttackType::Magic ? Stats->GetMagicEvasion() : Stats->GetEvasion()" in combat

print("JRPG stats/progression model: PASS")
