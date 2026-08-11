from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
H = (ROOT / "Source/AkumasRPGFramework/Public/World/ARPGDynamicStreetLight.h").read_text()
CPP = (ROOT / "Source/AkumasRPGFramework/Private/World/ARPGDynamicStreetLight.cpp").read_text()
BUILD = (ROOT / "Source/AkumasRPGFramework/AkumasRPGFramework.Build.cs").read_text()
DAY = (ROOT / "Source/AkumasRPGFramework/Public/World/ARPGDayNightCycle.h").read_text()

required_header = [
    "class AKUMASRPGFRAMEWORK_API AARPGDynamicStreetLight",
    "bEnableAutomaticDayNightControl = true",
    "DayNightCycleOverride",
    "bAutoDiscoverDayNightCycle = true",
    "bActiveDuringDawn = true",
    "bActiveDuringDay = false",
    "bActiveDuringDusk = false",
    "bActiveDuringNight = true",
    "NiagaraPreferredWithCascadeFallback",
    "NiagaraAndCascade",
    "LampLight",
    "NiagaraEffect",
    "CascadeEffect",
    "OnStreetLightStateChanged",
    "ReceiveStreetLightStateChanged",
    "RefreshFromDayNightCycleNow",
]
for token in required_header:
    assert token in H, token

required_cpp = [
    "PrimaryActorTick.bCanEverTick = false",
    "TActorIterator<AARPGDayNightCycle>",
    "OnPhaseChanged.AddUniqueDynamic",
    "OnHourChanged.AddUniqueDynamic",
    "OnDestroyed.AddUniqueDynamic",
    "GetFXSystemAsset()",
    "Activate(bResetEffectsOnActivation)",
    "DeactivateImmediate()",
    "GetComponents(LightComponents)",
    "ScheduleCycleResolveRetry",
    "ClearCycleResolveRetry",
]
for token in required_cpp:
    assert token in CPP, token

# Existing Day/Night cycle is the semantic source of truth.
for token in ("EARPGDayNightPhase", "OnPhaseChanged", "OnHourChanged", "GetDayNightPhase"):
    assert token in DAY, token

# Niagara was already a first-class module/plugin dependency before this feature.
assert '"Niagara"' in BUILD

# Default schedule model: late-night through dawn is lit; daytime/dusk is dark.
def should_on(phase: str) -> bool:
    return {
        "Dawn": True,
        "Day": False,
        "Dusk": False,
        "Night": True,
    }[phase]

assert should_on("Night") is True
assert should_on("Dawn") is True
assert should_on("Day") is False
assert should_on("Dusk") is False

# Niagara-preferred fallback semantics.
def fx(mode: str, has_niagara: bool, has_cascade: bool):
    n = c = False
    if mode == "Preferred":
        n = has_niagara
        c = (not has_niagara) and has_cascade
    elif mode == "NiagaraOnly":
        n = has_niagara
    elif mode == "CascadeOnly":
        c = has_cascade
    elif mode == "Both":
        n, c = has_niagara, has_cascade
    return n, c

assert fx("Preferred", True, True) == (True, False)
assert fx("Preferred", False, True) == (False, True)
assert fx("Preferred", False, False) == (False, False)
assert fx("Both", True, True) == (True, True)
assert fx("NiagaraOnly", False, True) == (False, False)

print("PASS: dynamic street-light phase/FX/source integration model")
