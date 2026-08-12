from pathlib import Path

plugin = Path(__file__).resolve().parents[1]
public = plugin / "Source" / "AkumasRPGFramework" / "Public"
private = plugin / "Source" / "AkumasRPGFramework" / "Private"

header = (public / "Components" / "ARPGFootstepComponent.h").read_text(errors="replace")
cpp = (private / "Components" / "ARPGFootstepComponent.cpp").read_text(errors="replace")
char_h = (public / "Actors" / "ARPGCharacter.h").read_text(errors="replace")
char_cpp = (private / "Actors" / "ARPGCharacter.cpp").read_text(errors="replace")
build = (plugin / "Source" / "AkumasRPGFramework" / "AkumasRPGFramework.Build.cs").read_text(errors="replace")

required_header = [
    "FARPGFootstepSurfaceAudio",
    "bEnableFootsteps = true",
    "bAutomaticFootsteps = true",
    "bPredictOwningPlayer = true",
    "DefaultSounds",
    "SurfaceAudio",
    "AttenuationSettings",
    "ConcurrencySettings",
    "TriggerFootstep",
    "UFUNCTION(Server, Unreliable)",
    "UFUNCTION(NetMulticast, Unreliable)",
]
required_cpp = [
    "SetIsReplicatedByDefault(true)",
    "PrimaryComponentTick.bCanEverTick = false",
    "IsMovingOnGround()",
    "Params.bReturnPhysicalMaterial = true",
    "UPhysicalMaterial::DetermineSurfaceType",
    "LineTraceSingleByChannel",
    "PickSoundAvoidingImmediateRepeat",
    "MulticastPlayFootstep",
    "GetNetMode() == NM_DedicatedServer",
    "Character->IsLocallyControlled()",
    "TeleportResetDistance",
    "HasAnyConfiguredSound()",
]
required_character = [
    "TObjectPtr<UARPGFootstepComponent> Footsteps",
    "CreateDefaultSubobject<UARPGFootstepComponent>(TEXT(\"Footsteps\"))",
    "PawnClientRestart",
    "Footsteps->RefreshFootstepRuntime()",
]

issues = []
for token in required_header:
    if token not in header:
        issues.append(f"header missing: {token}")
for token in required_cpp:
    if token not in cpp:
        issues.append(f"runtime missing: {token}")
for token in required_character:
    if token not in char_h and token not in char_cpp:
        issues.append(f"character integration missing: {token}")
if '"PhysicsCore"' not in build:
    issues.append("PhysicsCore dependency missing")
if "TickComponent(" in header or "TickComponent(" in cpp:
    issues.append("footstep system must not add permanent component Tick")
if "Reliable) void MulticastPlayFootstep" in header:
    issues.append("transient footsteps must not use reliable multicast")

if issues:
    raise SystemExit("FOOTSTEP MODEL FAILED:\n- " + "\n- ".join(issues))

print("Footstep replication model checks passed")

# Deterministic cadence model mirroring the native distance accumulation rules.
def cadence(samples, speed, min_speed=70.0, walk_dist=145.0, run_dist=185.0, run_speed=600.0, teleport_reset=260.0):
    accum = 0.0
    steps = 0
    if speed < min_speed:
        return 0
    alpha = max(0.0, min(1.0, (speed - min_speed) / max(1.0, run_speed - min_speed)))
    required = max(20.0, walk_dist + (run_dist - walk_dist) * alpha)
    for distance in samples:
        if distance > max(50.0, teleport_reset):
            accum = 0.0
            continue
        accum += distance
        if accum >= required:
            accum %= required
            steps += 1
    return steps

# 600 cm/s sampled at 20 Hz -> 30 cm/sample; ~3 steps/sec with 185 cm stride.
assert cadence([30.0] * 20, 600.0) == 3
# Stopped/creeping movement cannot leak partial stride into a later audible contact.
assert cadence([10.0] * 100, 50.0) == 0
# A teleport/correction must reset rather than burst a footstep.
assert cadence([40.0, 40.0, 300.0, 40.0, 40.0], 600.0) == 0
# Long continuous movement continues producing one contact at a time.
assert cadence([30.0] * 40, 600.0) == 6

print("Footstep cadence model checks passed")
