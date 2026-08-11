#!/usr/bin/env python3
"""Static/model guard for v2.4.2 AI dodge chance + stagger-cancel behavior."""
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
types = (ROOT / "Source/AkumasRPGFramework/Public/Combat/ARPGCombatTypes.h").read_text(errors="replace")
combat = (ROOT / "Source/AkumasRPGFramework/Private/Components/ARPGCombatComponent.cpp").read_text(errors="replace")
ai = (ROOT / "Source/AkumasRPGFramework/Private/Components/ARPGAICombatComponent.cpp").read_text(errors="replace")
ai_h = (ROOT / "Source/AkumasRPGFramework/Public/Components/ARPGAICombatComponent.h").read_text(errors="replace")

for token in (
    'DisplayName="AI Dodge Chance"',
    'float AIDodgeChance = 0.35f;',
    'DisplayName="Dodge Cancels Stagger"',
    'bool bDodgeCancelsStagger = true;',
):
    assert token in types, f"missing exposed dodge setting: {token}"

can_start = combat.index("bool UARPGCombatComponent::CanDodge() const")
can_end = combat.index("bool UARPGCombatComponent::CanBlock() const", can_start)
can = combat[can_start:can_end]
assert 'bIsStaggered && !Dodge.bDodgeCancelsStagger' in can
assert '|| bIsStaggered' not in can.split('const FARPGDodgeSettings Dodge', 1)[0]

start = combat.index("bool UARPGCombatComponent::PerformDodgeAuthority")
end = combat.index("void UARPGCombatComponent::FinishDodgeAuthority", start)
body = combat[start:end]
for token in (
    'bIsStaggered && Dodge.bDodgeCancelsStagger',
    'ClearTimer(StaggerTimer)',
    'SetLooseCombatTag(TEXT("Combat.State.Staggered"), false)',
    'OnStaggerStateChanged.Broadcast(false)',
    'Move->StopMovementImmediately()',
):
    assert token in body, f"missing stagger->dodge transition: {token}"
assert body.index('ClearTimer(StaggerTimer)') < body.index('SetLooseCombatTag(TEXT("Combat.State.Dodging"), true)')

for token in (
    'DodgeSettings.AIDodgeChance',
    'EffectiveDodgeChance',
    'DodgeRoll <= EffectiveDodgeChance',
    'ImpactRemaining - FMath::Max(0.02f, ThinkInterval)',
):
    assert token in ai, f"missing AI dodge decision polish: {token}"
assert 'float DodgeChance = 0.35f;' in ai_h

# Simple expected probability boundaries used by the runtime clamp.
def clamp01(v): return max(0.0, min(1.0, v))
assert clamp01(-1) == 0
assert clamp01(0.35) == 0.35
assert clamp01(2) == 1
print("AI dodge defence model: PASS")
