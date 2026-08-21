#!/usr/bin/env python3
"""Small regression model/static guard for v2.3 melee combat polish."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
COMBAT = ROOT / "Source/AkumasRPGFramework/Private/Components/ARPGCombatComponent.cpp"
AI = ROOT / "Source/AkumasRPGFramework/Private/Components/ARPGAICombatComponent.cpp"
TARGETING = ROOT / "Source/AkumasRPGFramework/Private/Components/ARPGTargetingComponent.cpp"
SAVE = ROOT / "Source/AkumasRPGFramework/Private/Subsystems/ARPGSaveSubsystem.cpp"
PERSISTENCE = ROOT / "Source/AkumasRPGFramework/Private/Components/ARPGPersistenceComponent.cpp"
AI_CHARACTER = ROOT / "Source/AkumasRPGFramework/Private/Actors/ARPGAICharacter.cpp"

combat = COMBAT.read_text(errors="replace")
ai = AI.read_text(errors="replace")
targeting = TARGETING.read_text(errors="replace")
save = SAVE.read_text(errors="replace")
persistence = PERSISTENCE.read_text(errors="replace")
ai_character = AI_CHARACTER.read_text(errors="replace")

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


# v2.15.39 combat/targeting integrity: faction restoration and reciprocal AI hostility must remain symmetric.
required_integrity = [
    (combat, "TargetAI->IsTargetConsideredHostile(GetOwner())"),
    (targeting, "TargetAI->IsTargetConsideredHostile(GetOwner())"),
    (targeting, "bBothHaveFactionIdentity"),
    (save, "if (!D.PrimaryFactionId.IsNone())"),
    (save, "DefaultPlayerFactionId"),
]
for source, token in required_integrity:
    assert token in source, f"missing v2.15.39 combat/targeting integrity path: {token}"

# Legacy/partial saves must not erase an already-authored/default faction with NAME_None.
def restore_faction(runtime_id, saved_id, default_id):
    if saved_id:
        return saved_id
    if runtime_id:
        return runtime_id
    return default_id or None

assert restore_faction("Player", None, "Player") == "Player"
assert restore_faction(None, None, "Player") == "Player"
assert restore_faction("Player", "Bandits", "Player") == "Bandits"

# If an AI explicitly considers the attacker hostile, player combat/lock-on must reciprocate that state.
def can_damage(relationship, allow_friendly, allow_neutral, target_ai_hostile):
    if target_ai_hostile:
        return True
    if relationship > 0:
        return allow_friendly
    if relationship == 0:
        return allow_neutral
    return True

assert can_damage(0, False, False, True)
assert not can_damage(0, False, False, False)
assert can_damage(-100, False, False, False)


# v2.15.40 relog root fix, retained through v2.17.3: account character persistence must never run
# on an actually AI-controlled AARPGAICharacter. A project may, however, derive a playable pawn from
# that class and possess it with a PlayerController; runtime ownership, not inheritance alone, is authoritative.
required_player_only_persistence = [
    (persistence, 'ARPGIsAccountCharacterPersistenceOwner'),
    (persistence, '!Character->IsA<AARPGAICharacter>() || Character->IsPlayerControlled()'),
    (save, 'Character->IsA<AARPGAICharacter>() && !Character->IsPlayerControlled()'),
    (ai_character, 'Persistence->bAutoLoadOnBeginPlay = false;'),
    (ai_character, 'Persistence->bAutoSave = false;'),
    (ai_character, 'Persistence->bSaveOnEndPlay = false;'),
]
for source, token in required_player_only_persistence:
    assert token in source, f"missing v2.15.40 player-only persistence guard: {token}"

def account_character_persistence_allowed(character_kind, player_controlled=False):
    return character_kind == "player" or (character_kind == "ai_subclass" and player_controlled)

assert account_character_persistence_allowed("player")
assert account_character_persistence_allowed("ai_subclass", player_controlled=True)
assert not account_character_persistence_allowed("ai_subclass", player_controlled=False)
assert not account_character_persistence_allowed("ai")

print("melee combat polish model: PASS")
