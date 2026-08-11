#!/usr/bin/env python3
"""Regression/static guard for v2.4.1 AI dodge movement reliability."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
COMBAT = ROOT / "Source/AkumasRPGFramework/Private/Components/ARPGCombatComponent.cpp"
AI = ROOT / "Source/AkumasRPGFramework/Private/Components/ARPGAICombatComponent.cpp"

combat = COMBAT.read_text(errors="replace")
ai = AI.read_text(errors="replace")

required_combat = [
    "AI->StopMovement();",
    "Move->StopMovementImmediately();",
    "const float DodgeSpeed = FMath::Max(0.f, Dodge.Distance) / FMath::Max(0.05f, Dodge.Duration);",
    "const FVector DodgeVelocity = WorldDirection.GetSafeNormal2D() * DodgeSpeed;",
    "Character->LaunchCharacter(DodgeVelocity, true, false);",
]
for token in required_combat:
    assert token in combat, f"missing reliable dodge movement path: {token}"

# Navigation must be aborted before the code-driven launch is submitted.
dodge_start = combat.index("bool UARPGCombatComponent::PerformDodgeAuthority")
dodge_end = combat.index("void UARPGCombatComponent::FinishDodgeAuthority", dodge_start)
dodge_body = combat[dodge_start:dodge_end]
assert dodge_body.index("AI->StopMovement();") < dodge_body.index("Character->LaunchCharacter(DodgeVelocity, true, false);")
assert dodge_body.index("Move->StopMovementImmediately();") < dodge_body.index("Character->LaunchCharacter(DodgeVelocity, true, false);")

required_ai = [
    "ResetActiveMoveState();",
    "if (MyCombat->bIsDodging)",
    "MyCombat->PerformDodge(Direction);",
]
for token in required_ai:
    assert token in ai, f"missing AI dodge/navigation handoff: {token}"

# Model the existing authored distance/duration conversion used for non-root-motion dodges.
def dodge_speed(distance, duration):
    return max(0.0, distance) / max(0.05, duration)

assert abs(dodge_speed(450.0, 0.55) - 818.1818181818181) < 1e-9
assert dodge_speed(0.0, 0.55) == 0.0
assert dodge_speed(450.0, 0.01) == 9000.0

print("AI dodge movement model: PASS")
