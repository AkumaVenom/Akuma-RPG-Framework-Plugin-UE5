#!/usr/bin/env python3
"""Regression guard for v2.5.3 NPC scaling editor exposure/runtime opt-in."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
h = (ROOT / "Source/AkumasRPGFramework/Public/Components/ARPGStatsComponent.h").read_text(errors="replace")
types = (ROOT / "Source/AkumasRPGFramework/Public/Stats/ARPGStatTypes.h").read_text(errors="replace")
cpp = (ROOT / "Source/AkumasRPGFramework/Private/Components/ARPGStatsComponent.cpp").read_text(errors="replace")

# The outer nested struct must not be disabled by Enable JRPG Stats; otherwise UE greys out the
# master bScaleToPlayer checkbox and the user cannot opt into the feature from that section.
line = next(line for line in h.splitlines() if "FARPGNPCLevelScalingSettings NPCLevelScalingSettings" in line)
assert 'EditCondition="bEnableJRPGStatSystem"' not in line, line
assert "ShowOnlyInnerProperties" in line

# Master toggle itself remains unconditional; child fields continue to depend on it.
master = next(line for line in types.splitlines() if "bool bScaleToPlayer" in line)
assert "EditCondition" not in master, master
assert 'DisplayName="Scale NPC To Player"' in master
assert types.count('EditCondition="bScaleToPlayer"') >= 8

# Runtime must honor the explicit scaling opt-in even if the separate JRPG checkbox was forgotten,
# but must never auto-convert a player-controlled pawn.
for token in (
    "NPCLevelScalingSettings.bScaleToPlayer && !bEnableJRPGStatSystem",
    "!OwnerPawn->IsPlayerControlled()",
    "bEnableJRPGStatSystem = true;",
):
    assert token in cpp, f"missing runtime safety token: {token}"

print("NPC scaling editor exposure model: PASS")
