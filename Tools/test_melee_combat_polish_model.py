#!/usr/bin/env python3
"""Small regression model/static guard for v2.3 melee combat polish."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
COMBAT = ROOT / "Source/AkumasRPGFramework/Private/Components/ARPGCombatComponent.cpp"
AI = ROOT / "Source/AkumasRPGFramework/Private/Components/ARPGAICombatComponent.cpp"

combat = COMBAT.read_text(errors="replace")
ai = AI.read_text(errors="replace")

required_combat = [
    "const bool bCanPlayLightHitReact",
    "!bIsAttacking",
    "Info.Result != EARPGCombatHitResult::Blocked",
    "AI->StopMovement();",
    "Move->StopMovementImmediately();",
    "Character->LaunchCharacter(KnockbackVelocity, true, true);",
    "MulticastPlayCombatCue(EARPGCombatFeedbackCue::Stagger",
]
for token in required_combat:
    assert token in combat, f"missing combat polish path: {token}"

required_ai = [
    "if (MyCombat->bIsAttacking)",
    "if (bMelee && Now < NextAttackSlotEligibleAt) return;",
    "EstimatedAttackDuration",
    "EstimatedAttackDuration + FMath::Max(0.f, AttackSlotCooldownAfterAttack)",
]
for token in required_ai:
    assert token in ai, f"missing AI pacing path: {token}"

# Model the intended pacing: recovery/impact duration plus breathing room.
def next_attack_at(commit, recovery, impact, cooldown):
    duration = max(max(0.05, recovery), max(0.0, impact) + 0.05)
    return commit + duration + max(0.0, cooldown)

assert abs(next_attack_at(10.0, 0.65, 0.25, 0.75) - 11.40) < 1e-6
assert abs(next_attack_at(10.0, 0.50, 0.80, 1.00) - 11.85) < 1e-6

# Preserve the original design: stagger remains a chance on critical hits, not every light hit.
def effective_stagger_rate(critical_chance, critical_stagger_chance):
    return max(0.0, min(1.0, critical_chance)) * max(0.0, min(1.0, critical_stagger_chance))

assert abs(effective_stagger_rate(0.05, 0.35) - 0.0175) < 1e-9
assert effective_stagger_rate(1.0, 1.0) == 1.0

print("melee combat polish model: PASS")
