#!/usr/bin/env python3
"""Static/model guard for v2.5.1 player-relative NPC scaling."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
types = (ROOT / "Source/AkumasRPGFramework/Public/Stats/ARPGStatTypes.h").read_text(errors="replace")
h = (ROOT / "Source/AkumasRPGFramework/Public/Components/ARPGStatsComponent.h").read_text(errors="replace")
cpp = (ROOT / "Source/AkumasRPGFramework/Private/Components/ARPGStatsComponent.cpp").read_text(errors="replace")

for token in (
    "Scale NPC To Player", "LevelMatchStrength", "LevelOffset", "MinimumScaledLevel", "MaximumScaledLevel",
    "bAllowScaleUp", "bAllowScaleDown", "bLockLevelWhileInCombat", "bReturnToBaseLevelWithoutPlayer",
    "CombatTargetThenNearest", "HighestLevelPlayer", "LowestLevelPlayer", "AverageNearbyPlayers",
):
    assert token in types, f"missing scaling authoring token: {token}"

for token in (
    "GetEffectiveLevel", "GetBaseProgressionLevel", "RefreshNPCLevelScalingNow",
    "GetNPCLevelScalingState", "OnNPCLevelScalingChanged", "NPCLevelScalingRuntime",
):
    assert token in h, f"missing Blueprint/runtime scaling API: {token}"

for token in (
    "GetPlayerControllerIterator", "ComputeNPCScaledLevel", "ResolveNPCScalingReference",
    "RefreshNPCLevelScalingInternal", "bNPCScalingEncounterLocked", "Combat->CombatTarget",
    "InitializeStatProgressionForLevel(BaseLevel, false)", "const int32 Level = GetEffectiveLevel()",
    "Snapshot.Level = GetEffectiveLevel()", "DOREPLIFETIME(UARPGStatsComponent, NPCLevelScalingRuntime)",
):
    assert token in cpp, f"missing authoritative scaling path: {token}"

# Do not replace the NPC's persisted progression Level with the reference player's level.
assert "Progression->Level =" not in cpp
assert "SetProgression(" not in cpp, "stat scaling must not mutate authored progression"


def effective(base, player, offset=0, match=1.0, allow_up=True, allow_down=True, lo=1, hi=100):
    target = max(1, player + offset)
    if not allow_up:
        target = min(target, base)
    if not allow_down:
        target = max(target, base)
    value = round(base + (target - base) * max(0.0, min(1.0, match)))
    return max(lo, min(hi, value))

assert effective(10, 30) == 30
assert effective(10, 30, match=0.5) == 20
assert effective(10, 30, offset=2) == 32
assert effective(10, 30, hi=25) == 25
assert effective(10, 30, allow_up=False) == 10
assert effective(30, 10, allow_down=False) == 30

# Creature identity: equal effective level does not equalize authored templates.
def natural(base, growth, level):
    return round(base + growth * max(0, level - 1))
chicken_strength = natural(3, 0.35, 30)
demon_strength = natural(18, 0.90, 30)
assert chicken_strength < demon_strength

# Scaling must cover the entire normal JRPG recalculation path, not a health-only multiplier.
for token in (
    "PrimaryStats.Strength", "PrimaryStats.Vitality", "PrimaryStats.Magic", "PrimaryStats.Spirit",
    "PrimaryStats.Dexterity", "PrimaryStats.Luck", "DerivedStats.MeleeAttackPower",
    "DerivedStats.RangedAttackPower", "DerivedStats.MagicAttackPower", "DerivedStats.PhysicalDefense",
    "DerivedStats.MagicDefense", "DerivedStats.Accuracy", "DerivedStats.Evasion", "DerivedStats.MagicEvasion",
    "DerivedStats.CriticalChance", "DerivedStats.CriticalDamageMultiplier", "DerivedStats.AttackSpeedMultiplier",
    "DerivedStats.MovementSpeedMultiplier", "MaxHealth =", "MaxMana =", "MaxStamina =",
):
    assert token in cpp, f"full stat rebuild missing {token}"

print("NPC player level/stat scaling model: PASS")
