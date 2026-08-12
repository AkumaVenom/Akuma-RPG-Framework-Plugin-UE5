#!/usr/bin/env python3
"""Static/deterministic guard for v2.9.0 polished replicated spawner ground-rise entrances."""
from pathlib import Path

root = Path(__file__).resolve().parents[1]
public = root / "Source" / "AkumasRPGFramework" / "Public"
private = root / "Source" / "AkumasRPGFramework" / "Private"

spawner_h = (public / "Actors" / "ARPGAISpawner.h").read_text(errors="replace")
spawner_cpp = (private / "Actors" / "ARPGAISpawner.cpp").read_text(errors="replace")
ai_h = (public / "Actors" / "ARPGAICharacter.h").read_text(errors="replace")
ai_cpp = (private / "Actors" / "ARPGAICharacter.cpp").read_text(errors="replace")
entrance_h = (public / "Components" / "ARPGSpawnEntranceComponent.h").read_text(errors="replace")
entrance_cpp = (private / "Components" / "ARPGSpawnEntranceComponent.cpp").read_text(errors="replace")

# Editor-facing spawner defaults are automatic but fully tunable.
for required in (
    "bEnableGroundRiseEntrance = true",
    "bAutoCalculateGroundRiseDepth = true",
    "GroundRiseDepth = 200.f",
    "ExtraGroundRiseDepth = 18.f",
    "GroundRiseDuration = 1.15f",
    "GroundRiseStartDelay = 0.05f",
    "GroundRiseEaseExponent = 2.25f",
    "bSuspendAIBehaviourDuringGroundRise = true",
    "bLockActorLocationDuringGroundRise = true",
):
    assert required in spawner_h, f"Missing ground-rise authoring default: {required}"

# Preserve v2.7.2 collision-safe final placement; presentation is layered on after normal movement configuration.
assert "AdjustIfPossibleButDontSpawnIfColliding" in spawner_cpp
assert "AdjustIfPossibleButAlwaysSpawn" not in spawner_cpp
spawn_one_start = spawner_cpp.index("APawn* AARPGAISpawner::SpawnOne()")
spawn_one_end = spawner_cpp.index("void AARPGAISpawner::SetAssignedSplineRoute", spawn_one_start)
spawn_one_body = spawner_cpp[spawn_one_start:spawn_one_end]
configure_pos = spawn_one_body.index("ConfigureSpawnedPawn(Pawn);")
rise_pos = spawn_one_body.index("BeginGroundRiseEntrance(Pawn);")
assert configure_pos < rise_pos, "Ground-rise must lock an already configured movement owner"
assert "Capsule->GetScaledCapsuleHalfHeight() * 2.f" in spawner_cpp

# Every framework AI has a replicated entrance component by default.
assert "TObjectPtr<UARPGSpawnEntranceComponent> SpawnEntrance" in ai_h
assert 'CreateDefaultSubobject<UARPGSpawnEntranceComponent>(TEXT("SpawnEntrance"))' in ai_cpp
assert "SetIsReplicatedByDefault(true)" in entrance_cpp
assert "DOREPLIFETIME(UARPGSpawnEntranceComponent, RepState)" in entrance_cpp
assert "GetServerWorldTimeSeconds" in entrance_cpp

# Critical invariant: visual mesh rises; actor/capsule stays at the accepted spawn location.
assert "BaseMeshRelativeLocation" in entrance_h
assert "Mesh->SetRelativeLocation(BaseMeshRelativeLocation + FVector(0.f, 0.f, ZOffset)" in entrance_cpp
assert "LockedActorLocation = Owner->GetActorLocation()" in entrance_cpp
assert "Owner->SetActorLocation(LockedActorLocation" in entrance_cpp
assert "RepState.Depth" in entrance_cpp

# All framework movement owners are held until reveal completion and independently restored.
combat_disable_pos = entrance_cpp.index("AICombat->SetAICombatEnabled(false)")
movement_disable_pos = entrance_cpp.index("Movement->DisableMovement()")
spline_pause_pos = entrance_cpp.index("Spline->PauseRoute(true)")
assert combat_disable_pos < movement_disable_pos < spline_pause_pos, "Higher-level AI must suspend before final locomotion locks"

for required in (
    "Movement->DisableMovement()",
    "AI->StopMovement()",
    "AcquireMovementPause(ARPGSpawnEntranceWanderPauseReason",
    "Spline->PauseRoute(true)",
    "AICombat->SetAICombatEnabled(false)",
    "Social->SetSocialInteractionsEnabled(false)",
    "ReleaseMovementPause(ARPGSpawnEntranceWanderPauseReason",
    "Spline->ResumeRoute()",
    "Movement->SetMovementMode",
):
    assert required in entrance_cpp, f"Missing entrance movement ownership path: {required}"

# Rise is short-lived: the component owns Tick only while the replicated entrance is active.
assert "PrimaryComponentTick.bStartWithTickEnabled = false" in entrance_cpp
assert "SetComponentTickEnabled(true)" in entrance_cpp
assert "SetComponentTickEnabled(false)" in entrance_cpp

print("Spawner ground-rise model: PASS")
