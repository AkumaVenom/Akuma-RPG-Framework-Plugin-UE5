from pathlib import Path
import json
import re
import sys

plugin_root = Path(__file__).resolve().parents[1]
root = plugin_root / "Source" / "AkumasRPGFramework"
issues = []
warnings = []
source_files = [p for p in root.rglob("*") if p.suffix in {".h", ".cpp"}]

readme_path_global = plugin_root / "README.md"
changelog_path_global = plugin_root / "Docs" / "CHANGELOG.md"
readme_text_global = readme_path_global.read_text(errors="replace") if readme_path_global.exists() else ""
changelog_text_global = changelog_path_global.read_text(errors="replace") if changelog_path_global.exists() else ""

def release_documented(token: str) -> bool:
    """Release history may live in the concise README or the dedicated changelog."""
    return token in readme_text_global or token in changelog_text_global


for p in source_files:
    s = p.read_text(errors="replace")
    t = re.sub(r"//.*", "", s)
    t = re.sub(r'"(?:\\.|[^"\\])*"', '""', t)
    if t.count("{") != t.count("}"):
        issues.append(f"{p.relative_to(root)} brace mismatch {t.count('{')} vs {t.count('}')}")
    if p.suffix == ".h" and '.generated.h"' in s:
        includes = [m.group(1) for m in re.finditer(r'^#include\s+"([^"]+)"', s, re.M)]
        generated = [x for x in includes if x.endswith(".generated.h")]
        if len(generated) != 1:
            issues.append(f"{p.relative_to(root)} generated include count={len(generated)}")
        elif includes[-1] != generated[0]:
            issues.append(f"{p.relative_to(root)} generated include is not the final include")

classes = {}
for p in root.rglob("*.h"):
    s = p.read_text(errors="replace")
    for m in re.finditer(r"\bclass\s+AKUMASRPGFRAMEWORK_API\s+(\w+)\s*[:{]", s):
        classes.setdefault(m.group(1), []).append(p.relative_to(root).as_posix())
for name, locations in classes.items():
    if len(locations) > 1:
        issues.append(f"duplicate exported class {name}: {locations}")

cpp_text = "\n".join(p.read_text(errors="replace") for p in root.rglob("*.cpp"))
for hp in root.rglob("*.h"):
    s = hp.read_text(errors="replace")
    for m in re.finditer(r"UFUNCTION\s*\(([^)]*\b(?:Server|Client|NetMulticast)\b[^)]*)\)\s*(?:virtual\s+)?[^;{]+?\b(\w+)\s*\([^;{]*\)\s*;", s, re.S):
        fn = m.group(2)
        if not re.search(r"::" + re.escape(fn) + r"_Implementation\s*\(", cpp_text):
            issues.append(f"{hp.relative_to(root)} RPC {fn} missing _Implementation")

for p in source_files:
    for line_no, line in enumerate(p.read_text(errors="replace").splitlines(), 1):
        m = re.match(r'\s*#include\s+"([^"]+)"', line)
        if not m:
            continue
        inc = m.group(1)
        if inc.endswith(".generated.h") or "ARPG" not in Path(inc).name:
            continue
        if not ((root / "Public" / inc).exists() or (root / "Private" / inc).exists() or (p.parent / inc).exists()):
            issues.append(f"{p.relative_to(root)}:{line_no} missing plugin include {inc}")

for p in root.rglob("*.cpp"):
    s = p.read_text(errors="replace")
    if re.search(r"SetTimer(?:ForNextTick)?\([^\n]*&\w+::(?:SaveNow|LoadNow|SavePersistentWorld|LoadPersistentWorld)", s):
        issues.append(f"{p.relative_to(root)} timer appears bound to a bool-returning method")

    # MSVC/UE can reject a conditional expression that mixes TSubclassOf<T> with a raw StaticClass() UClass*.
    if re.search(r"TSubclassOf<[^>]+>[^;=]*=\s*[^;?]+\?[^;:]+:\s*[^;]+StaticClass\(\)", s):
        issues.append(f"{p.relative_to(root)} ambiguous TSubclassOf/StaticClass conditional expression")


ai_character_cpp = root / "Private" / "Actors" / "ARPGAICharacter.cpp"
if ai_character_cpp.exists():
    ai_text = ai_character_cpp.read_text(errors="replace")
    if "DeathPresentation.bUseRagdollOnDeath = true" not in ai_text:
        issues.append("ARPGAICharacter must enable automatic ragdoll death by default")
    if "DeathPresentation.bFallbackToDeathMontage = true" not in ai_text:
        issues.append("ARPGAICharacter must keep Death montage fallback enabled by default")

combat_cpp = root / "Private" / "Components" / "ARPGCombatComponent.cpp"
if combat_cpp.exists():
    combat_text = combat_cpp.read_text(errors="replace")
    for required in ("TryStartRagdollLocal", "MulticastBeginDeathPresentation", "ResetDeathPresentationLocal"):
        if required not in combat_text:
            issues.append(f"Combat death presentation missing required ragdoll path: {required}")

# UE 5.8.1 AIController.h can leave EPathFollowingRequestResult as a forward declaration.
# Any spline source that uses concrete request-result enumerators must include the defining header.
spline_cpp = root / "Private" / "Components" / "ARPGAISplineComponent.cpp"
if spline_cpp.exists():
    spline_text = spline_cpp.read_text(errors="replace")
    if "EPathFollowingRequestResult::" in spline_text and '#include "Navigation/PathFollowingComponent.h"' not in spline_text:
        issues.append("ARPGAISplineComponent uses EPathFollowingRequestResult enumerators without Navigation/PathFollowingComponent.h")



# First-class AI spline route requirements.
ai_character_cpp = root / "Private" / "Actors" / "ARPGAICharacter.cpp"
if ai_character_cpp.exists():
    ai_text = ai_character_cpp.read_text(errors="replace")
    if "CreateDefaultSubobject<UARPGAISplineComponent>" not in ai_text:
        issues.append("ARPGAICharacter must create the automatic AI spline movement component by default")

spline_cpp = root / "Private" / "Components" / "ARPGAISplineComponent.cpp"
if spline_cpp.exists():
    spline_text = spline_cpp.read_text(errors="replace")
    for required in ("ProjectPointToNavigation", "MoveToLocation", "NotifyCombatStarted", "NotifyCombatEnded", "RejoinNearestRouteLocation"):
        if required not in spline_text:
            issues.append(f"AI spline movement missing required NavMesh/combat path: {required}")
    for forbidden in ("AttachToActor", "AttachToComponent", "SetActorLocation("):
        if forbidden in spline_text:
            issues.append(f"AI spline movement must not attach/teleport pawns along the route: found {forbidden}")

route_cpp = root / "Private" / "Actors" / "ARPGAISplineRoute.cpp"
if route_cpp.exists():
    route_text = route_cpp.read_text(errors="replace")
    if "GetLocationAtDistanceAlongSpline" not in route_text or "GetDistanceAlongSplineAtLocation" not in route_text:
        issues.append("AI spline route must expose native distance/location spline sampling")

spawner_cpp = root / "Private" / "Actors" / "ARPGAISpawner.cpp"
if spawner_cpp.exists():
    spawner_text = spawner_cpp.read_text(errors="replace")
    if "AssignedSplineRoute" not in spawner_text or "SplineMovement->SetRoute" not in spawner_text:
        issues.append("AI spawner must support automatic spline-route assignment")

ai_combat_cpp = root / "Private" / "Components" / "ARPGAICombatComponent.cpp"
if ai_combat_cpp.exists():
    ai_combat_text = ai_combat_cpp.read_text(errors="replace")
    if "SplineMovement->NotifyCombatStarted" not in ai_combat_text or "SplineMovement->NotifyCombatEnded" not in ai_combat_text:
        issues.append("AI combat must suspend/resume spline movement automatically")


# v1.5 polished AI route/group/free-roam requirements.
route_header = root / "Public" / "Actors" / "ARPGAISplineRoute.h"
if route_header.exists():
    route_header_text = route_header.read_text(errors="replace")
    if "bLoopRoute = true" not in route_header_text:
        issues.append("AI spline route must expose Loop Route enabled by default")
    if "bReverseAtOpenEnds = true" not in route_header_text:
        issues.append("AI spline route must expose open-end reverse looping by default")

spline_header = root / "Public" / "Components" / "ARPGAISplineComponent.h"
if spline_header.exists():
    spline_header_text = spline_header.read_text(errors="replace")
    for required in ("bUseRouteTraversalSettings = true", "GroupDirectionLeader", "SetGroupDirectionLeader"):
        if required not in spline_header_text:
            issues.append(f"AI spline group/route traversal missing required API: {required}")

spawner_header = root / "Public" / "Actors" / "ARPGAISpawner.h"
if spawner_header.exists():
    spawner_header_text = spawner_header.read_text(errors="replace")
    for required in ("bStayTogether = true", "GroupCohesionRadius", "bSynchronizeSplineGroupDirection = true", "EARPGSpawnerMovementMode", "FreeRoamRadius", "bFreeRoamLeashedToSpawner = true"):
        if required not in spawner_header_text:
            issues.append(f"AI spawner polished movement missing required setting: {required}")

if spawner_cpp.exists():
    spawner_text = spawner_cpp.read_text(errors="replace")
    for required in ("CheckGroupCohesion", "ConfigureFreeRoamPawn", "RefreshSplineGroupDirectionLeaders", "SetGroupDirectionLeader"):
        if required not in spawner_text:
            issues.append(f"AI spawner polished movement missing required runtime path: {required}")

wanderer_cpp = root / "Private" / "Components" / "ARPGWandererComponent.cpp"
if wanderer_cpp.exists():
    wanderer_text = wanderer_cpp.read_text(errors="replace")
    for required in ("GetRandomReachablePointInRadius", "CurrentTarget", "ForceReturnHome", "EnsureThinkTimer", "ScheduleMovementRetry", "RequestSuccessful", "VerifyNavigationMovement", "ARPGWanderMovementProofDistance"):
        if required not in wanderer_text:
            issues.append(f"Wanderer/free-roam missing required runtime path: {required}")
    if "EPathFollowingRequestResult::" in wanderer_text and '#include "Navigation/PathFollowingComponent.h"' not in wanderer_text:
        issues.append("Wanderer uses EPathFollowingRequestResult enumerators without Navigation/PathFollowingComponent.h")
    if "bHasEstablishedFreeRoam = true" not in wanderer_text or "Travelled2D >= ARPGWanderMovementProofDistance" not in wanderer_text:
        issues.append("v2.7.2 Wanderer must require real translation before Free Roam is established")

if spawner_cpp.exists():
    spawner_safety_text = spawner_cpp.read_text(errors="replace")
    if "AdjustIfPossibleButAlwaysSpawn" in spawner_safety_text:
        issues.append("v2.7.2 spawner must never force AI into blocking geometry with AlwaysSpawn")
    for required in ("AdjustIfPossibleButDontSpawnIfColliding", "MaxSafeSpawnAttempts = 10", "SpawnShape == EARPGSpawnerShape::Point && Attempt > 0"):
        if required not in spawner_safety_text:
            issues.append(f"v2.7.2 collision-safe spawned AI missing: {required}")

if ai_character_cpp.exists():
    ai_text = ai_character_cpp.read_text(errors="replace")
    if "CreateDefaultSubobject<UARPGWandererComponent>" not in ai_text or "AIWanderer->bEnabled = false" not in ai_text:
        issues.append("ARPGAICharacter must include a disabled-by-default AI Wanderer for spawner free roam")

# v1.6 combat-feel polish requirements.
targeting_header = root / "Public" / "Components" / "ARPGTargetingComponent.h"
targeting_cpp = root / "Private" / "Components" / "ARPGTargetingComponent.cpp"
if targeting_header.exists():
    targeting_header_text = targeting_header.read_text(errors="replace")
    for required in ("bFaceContinuouslyWhileLocked = true", "bLockCameraToTarget = true", "bAutoConfigureCameraRigForLock = true", "bOverrideMovementFacingWhileLocked = true"):
        if required not in targeting_header_text:
            issues.append(f"Targeting polish missing required default: {required}")
if targeting_cpp.exists():
    targeting_text = targeting_cpp.read_text(errors="replace")
    for required in ("SetControlRotation", "bUsePawnControlRotation", "ApplyLockedMovementFacing", "UpdateCameraLock"):
        if required not in targeting_text:
            issues.append(f"Targeting polish missing runtime path: {required}")

combat_types_header = root / "Public" / "Combat" / "ARPGCombatTypes.h"
if combat_types_header.exists():
    combat_types_text = combat_types_header.read_text(errors="replace")
    for required in ("FARPGCombatFXSettings", "FARPGCombatAudioSettings", "FARPGStaggerSettings", "CriticalStaggerChance", "HitNiagara", "HitCascadeFallback"):
        if required not in combat_types_text:
            issues.append(f"Combat feedback/stagger definition missing: {required}")

if combat_cpp.exists():
    combat_text = combat_cpp.read_text(errors="replace")
    for required in ("SpawnSystemAtLocation", "SpawnEmitterAtLocation", "PlaySoundAtLocation", "LaunchCharacter", "TryApplyCriticalStaggerAuthority", "MulticastPlayCombatCue"):
        if required not in combat_text:
            issues.append(f"Combat feedback/stagger runtime missing: {required}")
    for required_include in ('#include "NiagaraComponentPoolMethodEnum.h"', '#include "Particles/WorldPSCPool.h"'):
        if required_include not in combat_text:
            issues.append(f"Combat feedback requires explicit UE pooling enum include: {required_include}")

if ai_combat_cpp.exists():
    ai_combat_text = ai_combat_cpp.read_text(errors="replace")
    if "MyCombat->bIsStaggered" not in ai_combat_text or "AI->StopMovement()" not in ai_combat_text:
        issues.append("AI combat must stop movement while staggered so knockback is not immediately overridden")

day_night_cpp_v201 = root / "Private" / "World" / "ARPGDayNightCycle.cpp"
if day_night_cpp_v201.exists():
    day_night_text_v201 = day_night_cpp_v201.read_text(errors="replace")
    if "NetUpdateFrequency =" in day_night_text_v201:
        issues.append("v2.0.1 Day/Night must use SetNetUpdateFrequency() instead of deprecated public NetUpdateFrequency access")
    if "SetNetUpdateFrequency(2.f)" not in day_night_text_v201:
        issues.append("v2.0.1 Day/Night must retain the 2 Hz network update frequency through SetNetUpdateFrequency()")

build_cs = root / "AkumasRPGFramework.Build.cs"
if build_cs.exists() and '"Niagara"' not in build_cs.read_text(errors="replace"):
    issues.append("AkumasRPGFramework.Build.cs must depend on Niagara for combat feedback")


# v1.7 automatic AI retaliation / ally assist requirements.
ai_combat_header = root / "Public" / "Components" / "ARPGAICombatComponent.h"
if ai_combat_header.exists():
    ai_combat_header_text = ai_combat_header.read_text(errors="replace")
    for required in (
        "bRetaliateWhenAttacked = true",
        "bRetaliationOverridesNeutralFaction = true",
        "bRetaliateWhenFactionUnknown = true",
        "bCallForHelpWhenAttacked = true",
        "bAssistSameSpawnGroup = true",
        "bAssistSameClassWhenFactionUnknown = true",
        "IsTargetConsideredHostile",
        "ReceiveAggroCall",
    ):
        if required not in ai_combat_header_text:
            issues.append(f"AI retaliation/assist missing required setting/API: {required}")

if ai_combat_cpp.exists():
    ai_combat_text = ai_combat_cpp.read_text(errors="replace")
    for required in (
        "OnCombatHitReceived.AddDynamic",
        "HandleCombatHitReceived",
        "RememberAggression",
        "CallForHelp",
        "ReceiveAggroCall",
        "IsTargetConsideredHostile(CurrentTarget)",
    ):
        if required not in ai_combat_text:
            issues.append(f"AI retaliation/assist runtime missing: {required}")

if combat_cpp.exists():
    combat_text = combat_cpp.read_text(errors="replace")
    if "AICombat->IsTargetConsideredHostile(Target)" not in combat_text:
        issues.append("Combat damage legality must honor temporary AI retaliation hostility")

if spawner_cpp.exists():
    spawner_text = spawner_cpp.read_text(errors="replace")
    if "AICombat->SetSpawnGroupOwner(this)" not in spawner_text:
        issues.append("AI spawner must register spawned AI for spawn-group ally assistance")

faction_header = root / "Public" / "Data" / "ARPGFactionDefinition.h"
if faction_header.exists() and "DefaultRelationshipToUnlistedFactions" not in faction_header.read_text(errors="replace"):
    issues.append("Faction Definition must expose a default relationship for unlisted factions")

faction_cpp = root / "Private" / "Components" / "ARPGFactionComponent.cpp"
if faction_cpp.exists():
    faction_text = faction_cpp.read_text(errors="replace")
    if "ShouldAttackOnSight" not in faction_text or "bAttackHostileOnSight" not in faction_text:
        issues.append("Faction attack-on-sight flag must be consumed by runtime faction logic")


# v1.8 target-death disposition reset + coordinated melee group combat requirements.
if ai_combat_header.exists():
    ai_combat_header_text = ai_combat_header.read_text(errors="replace")
    for required in (
        "bRestoreOriginalDispositionAfterTargetDeath = true",
        "bClearThreatAgainstDeadTargets = true",
        "bEnableGroupCombatCoordination = true",
        "MaxSimultaneousMeleeAttackers = 3",
        "EARPGGroupCombatRole",
        "CurrentGroupCombatRole",
        "bHasMeleeAttackSlot",
        "ForgetTemporaryAggressionAgainst",
        "ForgetAllTemporaryAggression",
    ):
        if required not in ai_combat_header_text:
            issues.append(f"v1.8 AI target reset/group combat missing setting or API: {required}")

if ai_combat_cpp.exists():
    ai_combat_text = ai_combat_cpp.read_text(errors="replace")
    for required in (
        "BindTargetLifeState",
        "UnbindTargetLifeState",
        "HandleTargetLifeStateChanged",
        "OnLifeStateChanged.AddDynamic",
        "ForgetTemporaryAggressionAgainst(DeadTarget",
        "GatherCoordinatedMeleeAttackers",
        "EvaluateMeleeAttackSlot",
        "ComputeCombatRingPosition",
        "ProjectCombatPositionToNavigation",
        "MoveToCombatPosition",
        "SetFocus(CurrentTarget",
        "EARPGGroupCombatRole::WaitingOrbit",
        "EARPGGroupCombatRole::ActiveAttacker",
    ):
        if required not in ai_combat_text:
            issues.append(f"v1.8 AI target reset/group combat runtime missing: {required}")


# v1.9 host-synchronized day/night requirements.
daynight_header = root / "Public" / "World" / "ARPGDayNightCycle.h"
daynight_cpp = root / "Private" / "World" / "ARPGDayNightCycle.cpp"
time_lib_header = root / "Public" / "Utilities" / "ARPGWorldTimeLibrary.h"
time_lib_cpp = root / "Private" / "Utilities" / "ARPGWorldTimeLibrary.cpp"
if not daynight_header.exists() or not daynight_cpp.exists():
    issues.append("v1.9 requires the first-class ARPGDayNightCycle actor")
else:
    dh = daynight_header.read_text(errors="replace")
    dc = daynight_cpp.read_text(errors="replace")
    for required in (
        "HostSystemClock",
        "bUseBuiltInLightingRig = true",
        "UDirectionalLightComponent",
        "USkyLightComponent",
        "USkyAtmosphereComponent",
        "UExponentialHeightFogComponent",
        "IsDay() const",
        "IsNight() const",
        "GetDayNightPhase() const",
        "OnDawnStarted",
        "OnNightStarted",
        "ReplicatedHostDateTime",
    ):
        if required not in dh:
            issues.append(f"v1.9 day/night header missing: {required}")
    for required in (
        "FDateTime::Now()",
        "DOREPLIFETIME(AARPGDayNightCycle, ReplicatedHostDateTime)",
        "GetRealTimeSeconds",
        "SetAtmosphereSunLight(true)",
        "SetRealTimeCapture",
        "SetFogDensity",
        "SetWorldRotation",
    ):
        if required not in dc:
            issues.append(f"v1.9 day/night runtime missing: {required}")
    if "RecaptureSky(" in dc:
        issues.append("v1.9 day/night must not repeatedly use costly RecaptureSky; use Real Time Capture")
    if '#include "Engine/SkyAtmosphere.h"' in dc:
        issues.append('ARPGDayNightCycle uses obsolete UE5.8 include Engine/SkyAtmosphere.h; use Components/SkyAtmosphereComponent.h')
    if '#include "Components/SkyAtmosphereComponent.h"' not in dc:
        issues.append('ARPGDayNightCycle must include Components/SkyAtmosphereComponent.h for UE5.8 Sky Atmosphere types')

if not time_lib_header.exists() or not time_lib_cpp.exists():
    issues.append("v1.9 requires global ARPGWorldTimeLibrary pure Blueprint nodes")
else:
    th = time_lib_header.read_text(errors="replace")
    for required in ('DisplayName=\"Is Day\"', 'DisplayName=\"Is Night\"', 'DisplayName=\"Get World Hour\"', 'DisplayName=\"Get World Date Time\"', 'DisplayName=\"Get Day Night Phase\"', 'DisplayName=\"Get Daylight Amount\"'):
        if required not in th:
            issues.append(f"v1.9 world-time library missing pure node: {required}")



# v1.10 distance-streamed AI spawner population requirements.
spawner_header = root / "Public" / "Actors" / "ARPGAISpawner.h"
spawner_cpp = root / "Private" / "Actors" / "ARPGAISpawner.cpp"
if spawner_header.exists() and spawner_cpp.exists():
    sh = spawner_header.read_text(errors="replace")
    sc = spawner_cpp.read_text(errors="replace")
    for required in (
        "bEnableDistanceBasedPopulation = true",
        "bAutoSpawnWhenPlayerIsNear = true",
        "SpawnActivationRadius = 6000.f",
        "DespawnRadius = 8000.f",
        "DistanceDespawnDelay = 3.f",
        "bKeepLoadedNearSpawnedPawns = true",
        "IsPopulationActive() const",
        "EvaluatePopulationRelevanceNow",
        "OnPopulationActivated",
        "OnPopulationDeactivated",
    ):
        if required not in sh:
            issues.append(f"v1.10 AI spawner distance population missing setting/API: {required}")
    for required in (
        "GetPlayerControllerIterator",
        "CheckPopulationRelevance",
        "ActivateDistancePopulation",
        "DeactivateDistancePopulation",
        "FindNearestRelevantPlayerDistance",
        "bKeepLoadedNearSpawnedPawns",
        "PreservedImmediatePopulationCount",
        "RespawnNotBeforeTime",
        "EARPGRespawnMode::Never",
        "RemoveDynamic(this, &AARPGAISpawner::HandlePawnDestroyed)",
        "StopActiveRuntimeTimers",
        "ClearTimer(LeashTimer)",
        "ClearTimer(GroupCohesionTimer)",
    ):
        if required not in sc:
            issues.append(f"v1.10 AI spawner distance population runtime missing: {required}")
    if "PrimaryActorTick.bCanEverTick = true" in sc:
        issues.append("v1.10 AI spawner distance population must remain timer-driven, not Actor Tick driven")
    if "const APlayerController* PC = It->Get();" not in sc:
        issues.append("v1.10 player-controller iteration must unwrap the FConstPlayerControllerIterator TWeakObjectPtr with It->Get()")
else:
    issues.append("v1.10 requires ARPGAISpawner source/header")


# v2.0 Woodcutting / harvestable-tree requirements.
woodcut_header = root / "Public" / "Components" / "ARPGWoodcuttingComponent.h"
woodcut_cpp = root / "Private" / "Components" / "ARPGWoodcuttingComponent.cpp"
tree_header = root / "Public" / "Gathering" / "ARPGTree.h"
tree_cpp = root / "Private" / "Gathering" / "ARPGTree.cpp"
item_header = root / "Public" / "Data" / "ARPGItemDefinition.h"
character_header = root / "Public" / "Actors" / "ARPGCharacter.h"
character_cpp = root / "Private" / "Actors" / "ARPGCharacter.cpp"

if not woodcut_header.exists() or not woodcut_cpp.exists():
    issues.append("v2.0 requires the first-class ARPGWoodcuttingComponent")
else:
    wh = woodcut_header.read_text(errors="replace")
    wc = woodcut_cpp.read_text(errors="replace")
    for required in (
        'SkillId = TEXT("Woodcutting")',
        "StartWoodcuttingFromView",
        "FindWoodcuttingTreeInView",
        "ServerStartWoodcutting",
        "ServerStopWoodcutting",
        "GetWoodcuttingLevel() const",
        "GetWoodcuttingXPForNextLevel() const",
        "GetWoodcuttingLevelProgress() const",
        "HasValidToolForTree",
        "HasEquippedWoodcuttingTool",
        "TryChopTreeWithBasicAttack",
        "TryHandleBasicAttackAsWoodcutting",
        "bAutoChopTreesWithBasicAttack = true",
        "bBasicAttackRequiresEquippedAxe = true",
        "bUseCombatMeleeMontageAsChopFallback = true",
        "CalculateChopPower",
        "AwardWoodcuttingXP",
    ):
        if required not in wh:
            issues.append(f"v2.0 Woodcutting header missing: {required}")
    for required in (
        "SweepSingleByChannel",
        "AddSkillXPFromDefinition",
        "AddSkillXP(SkillId",
        "GatheringToolTags",
        "GatheringToolTier",
        "GatheringPower",
        "MulticastPlayChopMontage",
        "ResolveBasicAttackTree",
        "BeginWoodcuttingAuthority(Tree, true)",
        "LastBasicAttackChopAt",
        "PickRandomAttackMontage(false, false)",
        "SetTimer(SwingTimer",
        "SetTimer(ImpactTimer",
    ):
        if required not in wc:
            issues.append(f"v2.0 Woodcutting runtime missing: {required}")

combat_cpp_v202 = root / "Private" / "Components" / "ARPGCombatComponent.cpp"
if not combat_cpp_v202.exists():
    issues.append("v2.0.2 Basic Attack Woodcutting integration requires ARPGCombatComponent source")
else:
    combat_text_v202 = combat_cpp_v202.read_text(errors="replace")
    for required in (
        '#include "Components/ARPGWoodcuttingComponent.h"',
        "TryHandleBasicAttackAsWoodcutting",
        "bHandledAsWoodcutting",
    ):
        if required not in combat_text_v202:
            issues.append(f"v2.0.2 context-sensitive Basic Attack integration missing: {required}")

if not tree_header.exists() or not tree_cpp.exists():
    issues.append("v2.0 requires the Blueprintable ARPGTree harvestable actor")
else:
    th = tree_header.read_text(errors="replace")
    tc = tree_cpp.read_text(errors="replace")
    for required in (
        "Tree Mesh Variations",
        "Wood Item",
        "RequiredWoodcuttingLevel",
        "bRequireWoodcuttingTool",
        "MinimumToolTier",
        "XPPerSuccessfulChop",
        "XPOnFell",
        "FallDuration",
        "FallenTreeVisibleSeconds",
        "RespawnSeconds",
        "BonusDrops",
        "ApplyChop",
        "FellTree",
        "ForceRespawn",
        "SelectRandomTreeMesh",
        "bRandomizeTreeMeshScale = true",
        "MinimumMeshScale = 0.90f",
        "MaximumMeshScale = 1.10f",
        "SelectedTreeMeshScale",
        "GetSelectedTreeMeshScale",
        "SelectRandomTreeMeshScale",
        "SetTreeMeshScale",
        "OnTreeChopped",
        "OnTreeFelled",
        "OnTreeRewardGranted",
    ):
        if required not in th:
            issues.append(f"v2.0 Tree header missing: {required}")
    for required in (
        "PrimaryActorTick.bStartWithTickEnabled = false",
        "AwardWoodcuttingXP",
        "AddItemDefinition",
        "ReportItemLooted",
        "CanAddItemDefinition",
        "MulticastBeginTreeFall",
        "FQuat(Axis",
        "TreeState = EARPGTreeState::Stump",
        "DOREPLIFETIME(AARPGTree, SelectedTreeMeshIndex)",
        "DOREPLIFETIME(AARPGTree, SelectedTreeMeshScale)",
        "FMath::FRandRange(MinScale, MaxScale)",
        "ApplySelectedTreeMeshScale",
        "SetRelativeScale3D(BaseFallPivotScale * Scale)",
        "SetRelativeScale3D(BaseStumpMeshScale * Scale)",
        "DOREPLIFETIME(AARPGTree, TreeState)",
        "SpawnSystemAtLocation",
        "SpawnEmitterAtLocation",
    ):
        if required not in tc:
            issues.append(f"v2.0 Tree runtime missing: {required}")
    if "SetActorLocation(" in tc:
        issues.append("v2.0 tree fall must rotate the trunk around FallPivot, not teleport the tree")
    ambiguous_tree_mesh_ternary = "TreeMeshes.IsValidIndex(SelectedTreeMeshIndex) ? TreeMeshes[SelectedTreeMeshIndex].Get()"
    if ambiguous_tree_mesh_ternary in tc:
        issues.append("v2.0.1 ARPGTree GetSelectedTreeMesh must not mix TObjectPtr<UStaticMesh> and UStaticMesh* in one ternary")
    if "if (TreeMeshes.IsValidIndex(SelectedTreeMeshIndex))" not in tc or "return TreeMeshes[SelectedTreeMeshIndex].Get();" not in tc:
        issues.append("v2.0.1 ARPGTree GetSelectedTreeMesh must use an explicit raw-pointer branch for UE5.8.1/MSVC")
    for required_include in ('#include "NiagaraComponentPoolMethodEnum.h"', '#include "Particles/WorldPSCPool.h"'):
        if required_include not in tc:
            issues.append(f"v2.0 Tree feedback requires explicit UE pooling enum include: {required_include}")

inventory_header_v2 = root / "Public" / "Components" / "ARPGInventoryComponent.h"
inventory_cpp_v2 = root / "Private" / "Components" / "ARPGInventoryComponent.cpp"
if not inventory_header_v2.exists() or not inventory_cpp_v2.exists():
    issues.append("v2.0 requires inventory integration for Woodcutting rewards")
else:
    if "CanAddItemDefinition" not in inventory_header_v2.read_text(errors="replace") or "CanAddItemDefinition" not in inventory_cpp_v2.read_text(errors="replace"):
        issues.append("v2.0 Woodcutting rewards require definition-aware inventory capacity preflight")

if item_header.exists():
    ih = item_header.read_text(errors="replace")
    for required in ("GatheringToolTags", "GatheringPower", "GatheringToolTier"):
        if required not in ih:
            issues.append(f"v2.0 Item Definition gathering metadata missing: {required}")
else:
    issues.append("v2.0 requires ARPGItemDefinition")

if character_header.exists() and character_cpp.exists():
    ch = character_header.read_text(errors="replace")
    cc = character_cpp.read_text(errors="replace")
    if "TObjectPtr<UARPGWoodcuttingComponent> Woodcutting" not in ch:
        issues.append("v2.0 ARPGCharacter must expose inherited Woodcutting component")
    if "CreateDefaultSubobject<UARPGWoodcuttingComponent>" not in cc:
        issues.append("v2.0 ARPGCharacter must create Woodcutting automatically")
else:
    issues.append("v2.0 requires ARPGCharacter source/header")


# v2.1 designer inventory + equipment presentation polish.
equipment_header_v21 = root / "Public" / "Components" / "ARPGEquipmentComponent.h"
equipment_cpp_v21 = root / "Private" / "Components" / "ARPGEquipmentComponent.cpp"
equipment_visual_header_v21 = root / "Public" / "Equipment" / "ARPGEquipmentVisualActor.h"
equipment_visual_cpp_v21 = root / "Private" / "Equipment" / "ARPGEquipmentVisualActor.cpp"

if inventory_header_v2.exists() and inventory_cpp_v2.exists():
    ih21 = inventory_header_v2.read_text(errors="replace")
    ic21 = inventory_cpp_v2.read_text(errors="replace")
    for required in (
        "FARPGStartingInventoryItem",
        "Starting Items",
        "bEquipOnSpawn",
        "bGrantStartingItemsOnBeginPlay = true",
        "bOnlyGrantStartingItemsWhenEmpty = true",
        "ApplyStartingItems(bool bForce=false)",
        "Runtime Items",
    ):
        if required not in ih21:
            issues.append(f"v2.1 designer starting inventory missing: {required}")
    for required in (
        "ApplyStartingItems(false)",
        "AddItemDefinition(Starting.Item",
        "Equipment->EquipItem",
        "bStartingItemsApplied = true",
    ):
        if required not in ic21:
            issues.append(f"v2.1 starting inventory runtime missing: {required}")
    if 'UPROPERTY(EditAnywhere, BlueprintReadOnly, ReplicatedUsing=OnRep_Items' in ih21:
        issues.append("v2.1 runtime Items array must remain read-only; designers should author Starting Items instead")

if not equipment_visual_header_v21.exists() or not equipment_visual_cpp_v21.exists():
    issues.append("v2.1 requires the automatic ARPGEquipmentVisualActor")
else:
    evh = equipment_visual_header_v21.read_text(errors="replace")
    evc = equipment_visual_cpp_v21.read_text(errors="replace")
    for required in ("AARPGEquipmentVisualActor", "UStaticMeshComponent", "USkeletalMeshComponent", "ConfigureFromItem"):
        if required not in evh:
            issues.append(f"v2.1 equipment visual actor header missing: {required}")
    for required in ("bReplicates = false", "SetCollisionEnabled(ECollisionEnabled::NoCollision)", "EquippedStaticMesh", "EquippedSkeletalMesh", "LoadSynchronous"):
        if required not in evc:
            issues.append(f"v2.1 equipment visual actor runtime missing: {required}")

if not equipment_header_v21.exists() or not equipment_cpp_v21.exists():
    issues.append("v2.1 requires polished Equipment Component presentation")
else:
    eh21 = equipment_header_v21.read_text(errors="replace")
    ec21 = equipment_cpp_v21.read_text(errors="replace")
    for required in (
        "bAutoCreateEquipmentVisuals = true",
        "RefreshEquipmentVisuals",
        "GetEquipmentVisual",
        "OnEquipmentVisualChanged",
        "MulticastPlayEquipmentPresentation",
        "PlayEquippedCombatSwingSoundLocal",
    ):
        if required not in eh21:
            issues.append(f"v2.1 Equipment header missing: {required}")
    for required in (
        "Inventory->OnInventoryChanged.AddDynamic",
        "RefreshEquipmentVisuals();",
        "SpawnActor<AARPGEquipmentVisualActor>",
        "AttachToComponent",
        "EquippedRelativeTransform",
        "SetReplicates(false)",
        "EquipSound",
        "UnequipSound",
        "EquipMontage",
        "UnequipMontage",
    ):
        if required not in ec21:
            issues.append(f"v2.1 Equipment runtime missing: {required}")

if item_header.exists():
    ih21_item = item_header.read_text(errors="replace")
    for required in (
        "EquippedVisualActorClass",
        "EquippedStaticMesh",
        "EquippedSkeletalMesh",
        "EquippedRelativeTransform",
        "EquipSound",
        "UnequipSound",
        "CombatSwingSound",
        "GatheringSwingSound",
        "GatheringHitSound",
        "EquipmentAudioVolume",
    ):
        if required not in ih21_item:
            issues.append(f"v2.1 Item Definition equipment presentation missing: {required}")

if woodcut_cpp.exists():
    wc21 = woodcut_cpp.read_text(errors="replace")
    for required in ("GatheringSwingSound", "CombatSwingSound", "EquipmentAudioVolume"):
        if required not in wc21:
            issues.append(f"v2.1 Woodcutting tool-swing audio missing: {required}")
if tree_cpp.exists():
    tc21 = tree_cpp.read_text(errors="replace")
    for required in ("MulticastPlayChopFeedback(Harvester", "GatheringHitSound", "GetBestEquippedWoodcuttingTool"):
        if required not in tc21:
            issues.append(f"v2.1 tree/tool impact audio missing: {required}")
if combat_cpp_v202.exists():
    combat21 = combat_cpp_v202.read_text(errors="replace")
    if "PlayEquippedCombatSwingSoundLocal" not in combat21:
        issues.append("v2.1 combat must prefer equipped-item swing audio when supplied")


# v2.1.1 runtime inventory/equipment/Woodcutting consistency fixes.
asset_library_v211 = root / "Private" / "Utilities" / "ARPGAssetLibrary.cpp"
if asset_library_v211.exists():
    al211 = asset_library_v211.read_text(errors="replace")
    for required in ("TObjectIterator<UARPGDefinitionBase>", "Candidate->DefinitionId.IsNone() ? Candidate->GetFName()"):
        if required not in al211:
            issues.append(f"v2.1.1 loaded-definition fallback missing: {required}")

arpgt_types_v211 = root / "Public" / "ARPGTypes.h"
if arpgt_types_v211.exists():
    types211 = arpgt_types_v211.read_text(errors="replace")
    if "TSoftObjectPtr<UARPGItemDefinition> ItemDefinition" not in types211:
        issues.append("v2.1.1 runtime inventory entries must retain the exact Item Definition soft reference")
else:
    issues.append("v2.1.1 requires ARPGTypes.h")

if inventory_header_v2.exists() and inventory_cpp_v2.exists():
    ih211 = inventory_header_v2.read_text(errors="replace")
    ic211 = inventory_cpp_v2.read_text(errors="replace")
    for required in (
        "ResolveItemDefinition(const FARPGInventoryEntry& Entry) const",
        "GetItemDefinitionForInstance",
        "IsItemInstanceEquipped",
    ):
        if required not in ih211:
            issues.append(f"v2.1.1 Inventory API missing: {required}")
    for required in (
        "Entry.ItemDefinition.LoadSynchronous()",
        "Starting.Item->GetFName()",
        "StableId = Item->DefinitionId.IsNone() ? Item->GetFName()",
        "BackfillDefinitionReference",
        "Slot != Definition->EquipmentSlot",
        "OccupiedEquipmentSlots",
    ):
        if required not in ic211:
            issues.append(f"v2.1.1 exact runtime Item Definition path missing: {required}")
    if "Entry.ItemDefinition = ExplicitDefinition" not in ic211 and "FSoftObjectPath(ExplicitDefinition)" not in ic211:
        issues.append("v2.1.1 exact runtime Item Definition path missing: soft reference assignment from ExplicitDefinition")
    if "if (!Item || Item->DefinitionId.IsNone()" in ic211:
        issues.append("v2.1.1 inventory must allow Item Definition asset-name fallback when DefinitionId is blank")

if equipment_header_v21.exists() and equipment_cpp_v21.exists():
    eh211 = equipment_header_v21.read_text(errors="replace")
    ec211 = equipment_cpp_v21.read_text(errors="replace")
    for required in (
        "IsValidEquippedEntry",
        "FallbackHandSockets",
        "bAutoFindFallbackHandSocket = true",
        "MulticastPlayEquipmentPresentation(UARPGItemDefinition* Definition",
    ):
        if required not in eh211:
            issues.append(f"v2.1.1 Equipment header missing: {required}")
    for required in (
        "Inventory->ResolveItemDefinition(Entry)",
        "Entry.InstanceId.IsValid()",
        "Entry.Quantity <= 0",
        "Entry.EquipmentSlot == Definition->EquipmentSlot",
        "ResolveAttachSocket",
        "DoesSocketExist",
        "MulticastPlayEquipmentPresentation(Definition, true)",
    ):
        if required not in ec211:
            issues.append(f"v2.1.1 Equipment runtime missing: {required}")
    if "MulticastPlayEquipmentPresentation(FName ItemId" in eh211 or "MulticastPlayEquipmentPresentation_Implementation(FName ItemId" in ec211:
        issues.append("v2.1.1 equipment presentation must use the exact Item Definition instead of re-resolving ItemId")

if woodcut_cpp.exists():
    wc211 = woodcut_cpp.read_text(errors="replace")
    for required in (
        "Inventory->ResolveItemDefinition(Entry)",
        "Entry.InstanceId.IsValid()",
        "Entry.Quantity <= 0",
        "Entry.EquipmentSlot.IsValid()",
        "Def->bEquippable",
        "Entry.EquipmentSlot != Def->EquipmentSlot",
        "GetBestEquippedWoodcuttingToolInstanceId",
        "MulticastPlayChopMontage(CurrentTree, FindBestToolForTree(CurrentTree))",
    ):
        if required not in wc211:
            issues.append(f"v2.1.1 strict equipped Woodcutting tool path missing: {required}")

if tree_cpp.exists():
    tc211 = tree_cpp.read_text(errors="replace")
    for required in (
        "EquippedTool = Woodcutting->GetBestEquippedWoodcuttingTool()",
        "MulticastPlayChopFeedback(Harvester, ImpactLocation, EquippedTool)",
        "EquippedTool->GatheringHitSound",
    ):
        if required not in tc211:
            issues.append(f"v2.1.1 exact equipped-tool hit presentation missing: {required}")

crafting_cpp_v211 = root / "Private" / "Crafting" / "ARPGCraftingStationActor.cpp"
if crafting_cpp_v211.exists():
    crafting211 = crafting_cpp_v211.read_text(errors="replace")
    for required in (
        "InventoryComponent->ResolveItemDefinition(Entry)",
        "FuelInventory->ResolveItemDefinition(Entry)",
    ):
        if required not in crafting211:
            issues.append(f"v2.1.1 crafting must consume exact runtime Item Definitions: {required}")

storage_header = root / "Public" / "Crafting" / "ARPGStorageActor.h"
if storage_header.exists() and "TObjectPtr<UARPGFactionOwnershipComponent> Ownership" in storage_header.read_text():
    issues.append("Storage actor duplicates base building Ownership component")


# v2.2/v2.2.3 first-class Quick Access / hotbar switching, consumable use, runtime-instance uniqueness and exclusive active-equipment handoff.
quick_header_v22 = root / "Public" / "Components" / "ARPGQuickAccessComponent.h"
quick_cpp_v22 = root / "Private" / "Components" / "ARPGQuickAccessComponent.cpp"
character_header_v22 = root / "Public" / "Actors" / "ARPGCharacter.h"
character_cpp_v22 = root / "Private" / "Actors" / "ARPGCharacter.cpp"
save_cpp_v22 = root / "Private" / "Subsystems" / "ARPGSaveSubsystem.cpp"
if not quick_header_v22.exists() or not quick_cpp_v22.exists():
    issues.append("v2.2 requires ARPGQuickAccessComponent")
else:
    qh22 = quick_header_v22.read_text(errors="replace")
    qc22 = quick_cpp_v22.read_text(errors="replace")
    for required in (
        "MaxQuickAccessSlots = 8",
        "QuickAccessSlots",
        "ActiveSlotNumber",
        "AssignItemToSlot",
        "AssignItemIdToSlot",
        "ActivateSlot",
        "UseActiveSlot",
        "ActivateNextSlot",
        "ActivatePreviousSlot",
        "GetSlotView",
        "FindSlotForItemInstance",
        "ReplaceQuickAccessState",
        "COND_OwnerOnly",
    ):
        if required not in qh22 and required not in qc22:
            issues.append(f"v2.2 Quick Access missing required API/runtime path: {required}")
    item_use_cpp_v213 = root / "Private" / "Components" / "ARPGItemUseComponent.cpp"
    item_use_runtime = item_use_cpp_v213.read_text(errors="replace") if item_use_cpp_v213.exists() else ""
    for required in (
        "ResolveOwnedEntry",
        "Inventory->ResolveItemDefinition(*Entry)",
        "Inventory->IsItemInstanceEquipped",
        "Equipment->EquipItem",
        "Definition->bUsable",
        "MulticastPlayItemUsePresentation",
        "CooldownEndByItemId",
        "FindOwnedEntryByIdExcluding",
        "bSameRuntimeInstance",
        "ClaimedInstanceIds",
        "AssignmentRevision",
        "IsCanonicalSlotForView",
    ):
        if required not in qc22:
            issues.append(f"v2.2 Quick Access runtime missing: {required}")
    # v2.13 centralizes actual consumable execution in ItemUse; accept either the old Quick Access
    # location or the new shared authority component while keeping every legacy guarantee checked.
    for required in (
        "Definition->bConsumeOnUse",
        "Inventory->RemoveItemInstance",
        "Definition->UseGameplayEffect",
        "Stats->Heal",
        "Stats->RestoreMana",
        "Stats->RestoreStamina",
    ):
        if required not in qc22 and required not in item_use_runtime:
            issues.append(f"v2.2 consumable runtime missing after ItemUse centralization: {required}")
    if "ResolveDefinitionById" in qc22:
        issues.append("v2.2 Quick Access must resolve actions from owned runtime inventory entries, not project Data Asset lookup alone")
    # v2.2.1 invariant: the exact same runtime inventory GUID can never occupy multiple Quick Access slots.
    if "OtherSlot.ItemInstanceId == Entry->InstanceId" not in qc22:
        issues.append("v2.2.1 Quick Access assignment must clear prior slots containing the exact runtime InstanceId")
    if "FindOwnedEntryByIdExcluding(Slot.ItemId, ClaimedInstanceIds)" not in qc22:
        issues.append("v2.2.2 Quick Access repair must rebind only to unclaimed runtime instances")
    if "The exact same runtime instance is always unique" not in qh22:
        issues.append("v2.2.2 Quick Access duplicate policy must document exact runtime-instance uniqueness")
    for required in ("RepairRuntimeBindingsAuthority(SlotNumber)", "IsCanonicalSlotForView", "PreferredSlotNumber"):
        if required not in qh22 and required not in qc22:
            issues.append(f"v2.2.2 Quick Access revision-aware duplicate repair missing: {required}")
    for required in (
        "bExclusiveActiveQuickAccessEquipment = true",
        "LastQuickAccessEquippedInstanceId",
        "CaptureTrackedQuickAccessEquipmentFromSlotAuthority",
        "UnequipPreviousQuickAccessEquipmentAuthority",
    ):
        if required not in qh22 and required not in qc22:
            issues.append(f"v2.2.3 Quick Access exclusive active-equipment handoff missing: {required}")
    if "Equipment->UnequipItem(PreviousInstanceId)" not in qc22:
        issues.append("v2.2.3 Quick Access must unequip the previous runtime equipment instance before switching held items")
    if "if (!IsCanonicalSlotForView(SlotNumber)) return EARPGQuickAccessResult::EmptySlot;" in qc22:
        # This exact guard belongs to enum-returning selection functions, never bool ClearSlotAuthority.
        clear_start = qc22.find("bool UARPGQuickAccessComponent::ClearSlotAuthority")
        clear_end = qc22.find("bool UARPGQuickAccessComponent::SwapSlots", clear_start)
        if clear_start >= 0 and clear_end > clear_start and "EARPGQuickAccessResult::" in qc22[clear_start:clear_end]:
            issues.append("v2.2.3-alpha.1 Quick Access ClearSlotAuthority must return bool, not EARPGQuickAccessResult")

if arpgt_types_v211.exists():
    types22 = arpgt_types_v211.read_text(errors="replace")
    for required in ("EARPGQuickAccessAction", "EARPGQuickAccessResult", "FARPGQuickAccessSlot", "QuickAccessSlots", "ActiveQuickAccessSlotNumber"):
        if required not in types22:
            issues.append(f"v2.2 shared/save types missing: {required}")
    if "AssignmentRevision" not in types22:
        issues.append("v2.2.2 Quick Access slot state must persist assignment revision")

if item_header.exists():
    item22 = item_header.read_text(errors="replace")
    for required in (
        "bAllowQuickAccess = true",
        "QuickAccessAction",
        "bUsable = false",
        "bConsumeOnUse = true",
        "ConsumeQuantity = 1",
        "UseCooldownSeconds",
        "RestoreHealth",
        "RestoreMana",
        "RestoreStamina",
        "UseGameplayEffect",
        "UseMontage",
        "UseSound",
    ):
        if required not in item22:
            issues.append(f"v2.2 Item Definition quick-use metadata missing: {required}")

if character_header_v22.exists() and character_cpp_v22.exists():
    ch22 = character_header_v22.read_text(errors="replace")
    cc22 = character_cpp_v22.read_text(errors="replace")
    for required in ("UARPGQuickAccessComponent", "QuickAccessPressed", "QuickAccessNext", "QuickAccessPrevious", "UseActiveQuickAccessItem"):
        if required not in ch22:
            issues.append(f"v2.2 Character Quick Access API missing: {required}")
    if "CreateDefaultSubobject<UARPGQuickAccessComponent>" not in cc22:
        issues.append("v2.2 ready ARPGCharacter must create QuickAccess by default")

if inventory_header_v2.exists() and inventory_cpp_v2.exists():
    ih22 = inventory_header_v2.read_text(errors="replace")
    ic22 = inventory_cpp_v2.read_text(errors="replace")
    if "QuickAccessSlot = 0" not in ih22:
        issues.append("v2.2 Starting Items must expose optional Quick Access Slot authoring")
    for required in ("QuickAccessAssignments", "AssignItemIdToSlot", "PreferredActiveQuickAccessSlot"):
        if required not in ic22:
            issues.append(f"v2.2 Starting Item -> Quick Access integration missing: {required}")

if save_cpp_v22.exists():
    save22 = save_cpp_v22.read_text(errors="replace")
    for required in ("D.QuickAccessSlots", "D.ActiveQuickAccessSlotNumber", "ReplaceQuickAccessState"):
        if required not in save22:
            issues.append(f"v2.2 Quick Access persistence missing: {required}")

# v2.4 day/night AI population swapping. Only ARPGAISpawner intentionally gains new reflected
# authoring/runtime API; existing SpawnTable remains the daylight/default path when the feature is disabled.
if spawner_header.exists():
    spawner24h = spawner_header.read_text(errors="replace")
    for required in (
        "bEnableMidnightPopulationSwap = false",
        "MidnightSpawnTable",
        "bUseSeparateMidnightGroupSize = false",
        "MidnightMinGroupSize",
        "MidnightMaxGroupSize",
        "DayNightCycleOverride",
        "bMidnightPopulationActive = false",
        "RefreshDayNightPopulationNow",
        "IsMidnightPopulationActive",
    ):
        if required not in spawner24h:
            issues.append(f"v2.4 day/night AI spawner missing authoring/runtime API: {required}")

if spawner_cpp.exists():
    spawner24c = spawner_cpp.read_text(errors="replace")
    for required in (
        "InitializeDayNightPopulation",
        "ResolveAndBindDayNightCycle",
        "OnHourChanged.AddDynamic",
        "OnDayStarted.AddDynamic",
        "IsWorldTimeInMidnightPopulationWindow",
        "ApplyDayNightPopulationPhase",
        "GetActiveSpawnTable",
        "GetActiveGroupSizeRange",
        "DespawnAll(true);",
        "GetWorldHour() < MorningHour",
        "CheckDayNightPopulationPhase();",
    ):
        if required not in spawner24c:
            issues.append(f"v2.4 day/night AI spawner missing runtime path: {required}")
    if "Pawn->OnDestroyed.RemoveDynamic(this, &AARPGAISpawner::HandlePawnDestroyed);" not in spawner24c:
        issues.append("v2.4 phase population cleanup must remain death/respawn-neutral through DespawnAll delegate removal")

# v2.3 retains the Blueprint-compatible Quick Access reflection API from the 2.2.3-alpha.2 baseline.
if quick_header_v22.exists():
    qh_compat = quick_header_v22.read_text(errors="replace")
    if "bAutoEquipReplacementInActiveSlot" in qh_compat:
        issues.append("v2.3 must not reintroduce reflected active-slot auto-equip state to the public Quick Access header")
if quick_cpp_v22.exists():
    qc_compat = quick_cpp_v22.read_text(errors="replace")
    if "bReplacedCurrentlyActiveSlot" not in qc_compat or "ActivateSlotAuthority(SlotNumber, ActivatedInstanceId)" not in qc_compat:
        issues.append("v2.3 must retain the private active-slot replacement handoff")

# v2.3 melee combat polish: light hits cannot visually cancel active attacks, stagger must stop
# navigation immediately, and AI attack pacing must not auto-buffer combo input every Think.
if combat_cpp.exists():
    combat23 = combat_cpp.read_text(errors="replace")
    for required in (
        "const bool bCanPlayLightHitReact",
        "Info.Result != EARPGCombatHitResult::Blocked",
        "!bIsAttacking",
        "AI->StopMovement();",
        "Move->StopMovementImmediately();",
        "LaunchCharacter(KnockbackVelocity, true, true)",
        "MulticastPlayCombatCue(EARPGCombatFeedbackCue::Stagger",
    ):
        if required not in combat23:
            issues.append(f"v2.3 melee combat polish missing combat path: {required}")
if ai_combat_cpp.exists():
    ai23 = ai_combat_cpp.read_text(errors="replace")
    for required in (
        "if (MyCombat->bIsAttacking)",
        "if (bMelee && Now < NextAttackSlotEligibleAt) return;",
        "EstimatedAttackDuration",
        "EstimatedAttackDuration + FMath::Max(0.f, AttackSlotCooldownAfterAttack)",
    ):
        if required not in ai23:
            issues.append(f"v2.3 melee AI pacing missing runtime path: {required}")


# v2.5.4 packaged-build compatibility: avoid editor-only DirectionalLight component access and
# UE 5.8 soft-pointer assignment deprecations observed by the real Windows packaging toolchain.
if day_night_cpp_v201.exists():
    package_daynight = day_night_cpp_v201.read_text(errors="replace")
    for forbidden in ("ExternalSunLight->GetComponent()", "ExternalMoonLight->GetComponent()"):
        if forbidden in package_daynight:
            issues.append(f"v2.5.4 packaged build must not use editor-only ADirectionalLight access: {forbidden}")
    for required in (
        "ExternalSunLight->FindComponentByClass<UDirectionalLightComponent>()",
        "ExternalMoonLight->FindComponentByClass<UDirectionalLightComponent>()",
    ):
        if required not in package_daynight:
            issues.append(f"v2.5.4 packaged build runtime light resolution missing: {required}")

if inventory_cpp_v2.exists():
    inventory254 = inventory_cpp_v2.read_text(errors="replace")
    if "Entry.ItemDefinition = ExplicitDefinition" in inventory254:
        issues.append("v2.5.4 must avoid deprecated TSoftObjectPtr assignment from const raw item-definition pointers")
    if "FSoftObjectPath(ExplicitDefinition)" not in inventory254:
        issues.append("v2.5.4 inventory soft-reference assignment should construct through FSoftObjectPath")

try:
    descriptor = json.loads((plugin_root / "AkumasRPGFramework.uplugin").read_text())
    if descriptor.get("Version") != 21523 or descriptor.get("VersionName") != "2.15.23-alpha":
        issues.append("package descriptor must identify v2.15.23-alpha")
    plugin_refs = {entry.get("Name") for entry in descriptor.get("Plugins", []) if isinstance(entry, dict)}
    for module_only_name in ("GameplayTags", "GameplayTasks"):
        if module_only_name in plugin_refs:
            issues.append(f".uplugin incorrectly declares runtime module {module_only_name} as a plugin dependency")
    if "Niagara" not in plugin_refs:
        issues.append(".uplugin must enable Niagara for v1.6 combat feedback")
except Exception as exc:
    issues.append(f"invalid .uplugin JSON: {exc}")


# v2.6 ambient NPC social interaction requirements.
social_header = root / "Public" / "Components" / "ARPGAISocialComponent.h"
social_cpp = root / "Private" / "Components" / "ARPGAISocialComponent.cpp"
if not social_header.exists() or not social_cpp.exists():
    issues.append("v2.6 ambient NPC social component source files are missing")
else:
    social_header_text = social_header.read_text(errors="replace")
    for required in (
        "bEnableSocialInteractions = false",
        "InteractionChance = 0.35f",
        "bAllowSameFaction = true",
        "bAllowFriendlyFactions = true",
        "bAllowNeutralFactions = true",
        "SocialIdentityTags",
        "InteractionPool",
        "ForceSocialInteractionWith",
        "OnSocialLineSpoken",
    ):
        if required not in social_header_text:
            issues.append(f"AI social authoring/API missing required path: {required}")
    social_cpp_text = social_cpp.read_text(errors="replace")
    for required in (
        "OverlapMultiByObjectType",
        "PassesFactionRules",
        "MineToTheirs < 0 || TheirsToMine < 0",
        "PauseAmbientMovementForSocial",
        "RestoreAmbientMovementAfterSocial",
        "PauseRoute(true)",
        "ResumeRoute()",
        "HandleAICombatTargetChanged",
        "HandleCombatHitReceived",
        "MulticastSocialStarted",
        "MulticastSocialBeat",
        "MulticastSocialEnded",
    ):
        if required not in social_cpp_text:
            issues.append(f"AI social runtime missing required path: {required}")

if ai_character_cpp.exists():
    ai_character_text_v26 = ai_character_cpp.read_text(errors="replace")
    if "CreateDefaultSubobject<UARPGAISocialComponent>" not in ai_character_text_v26 or "AISocial->bEnableSocialInteractions = false" not in ai_character_text_v26:
        issues.append("ARPGAICharacter must include a disabled-by-default AISocial component")


# v2.6.1 spawned Free-Roam/social/cohesion handoff reliability.
wanderer_header_261 = root / "Public" / "Components" / "ARPGWandererComponent.h"
wanderer_cpp_261 = root / "Private" / "Components" / "ARPGWandererComponent.cpp"
if wanderer_header_261.exists() and wanderer_cpp_261.exists():
    wh261 = wanderer_header_261.read_text(errors="replace")
    wc261 = wanderer_cpp_261.read_text(errors="replace")
    for required in ("AcquireMovementPause", "ReleaseMovementPause", "MovementPauseReasons", "HasMovementPauseOtherThan"):
        if required not in wh261:
            issues.append(f"v2.6.1 Wanderer movement ownership missing native API/state: {required}")
    for required in ("IsMovementPaused()", "EnsureThinkTimer();", "Spline->IsRouteActive()"):
        if required not in wc261:
            issues.append(f"v2.6.1 Wanderer reliability missing runtime guard: {required}")

if social_cpp.exists():
    social261 = social_cpp.read_text(errors="replace")
    for required in (
        'ARPGSocialWanderPauseReason(TEXT("SocialInteraction"))',
        "AcquireMovementPause(ARPGSocialWanderPauseReason",
        "ReleaseMovementPause(ARPGSocialWanderPauseReason",
        "HasMovementPauseOtherThan(ARPGSocialWanderPauseReason)",
        "NextOpportunityAt = GetWorld()->GetTimeSeconds() + FMath::FRandRange(DelayMin, DelayMax)",
    ):
        if required not in social261:
            issues.append(f"v2.6.1 social/Wanderer handoff missing: {required}")
    if "SetWandererEnabled(false)" in social261 or "SetWandererEnabled(true)" in social261:
        issues.append("v2.6.1 social AI must not toggle persistent Wanderer enablement for temporary interactions")

if spawner_cpp.exists():
    spawner261 = spawner_cpp.read_text(errors="replace")
    for required in (
        'ARPGSpawnerCohesionWanderPauseReason(TEXT("SpawnerGroupCohesion"))',
        "AcquireMovementPause(ARPGSpawnerCohesionWanderPauseReason",
        "ReleaseMovementPause(ARPGSpawnerCohesionWanderPauseReason",
        "if (Social->IsSociallyEngaged()) continue;",
        "if (bRecovering)",
        "Distance <= RecoveryRadius",
    ):
        if required not in spawner261:
            issues.append(f"v2.6.1 spawner Free-Roam/cohesion reliability missing: {required}")


# v2.8 automatic replicated physical-surface footsteps.
footstep_header = root / "Public" / "Components" / "ARPGFootstepComponent.h"
footstep_cpp = root / "Private" / "Components" / "ARPGFootstepComponent.cpp"
if not footstep_header.exists() or not footstep_cpp.exists():
    issues.append("v2.8 automatic footstep component source files are missing")
else:
    fh28 = footstep_header.read_text(errors="replace")
    fc28 = footstep_cpp.read_text(errors="replace")
    for required in (
        "FARPGFootstepSurfaceAudio",
        "bEnableFootsteps = true",
        "bAutomaticFootsteps = true",
        "bPredictOwningPlayer = true",
        "DefaultSounds",
        "SurfaceAudio",
        "AttenuationSettings",
        "ConcurrencySettings",
        "UFUNCTION(Server, Unreliable)",
        "UFUNCTION(NetMulticast, Unreliable)",
    ):
        if required not in fh28:
            issues.append(f"v2.8 footstep authoring/network API missing: {required}")
    for required in (
        "PrimaryComponentTick.bCanEverTick = false",
        "SetIsReplicatedByDefault(true)",
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
    ):
        if required not in fc28:
            issues.append(f"v2.8 footstep runtime missing: {required}")
    if "TickComponent(" in fh28 or "TickComponent(" in fc28:
        issues.append("v2.8 footsteps must remain timer/event based without permanent component Tick")
    if "Reliable) void MulticastPlayFootstep" in fh28:
        issues.append("v2.8 transient footstep multicast must remain unreliable")

character_header_28 = root / "Public" / "Actors" / "ARPGCharacter.h"
character_cpp_28 = root / "Private" / "Actors" / "ARPGCharacter.cpp"
if character_header_28.exists() and character_cpp_28.exists():
    ch28 = character_header_28.read_text(errors="replace")
    cc28 = character_cpp_28.read_text(errors="replace")
    for required in ("TObjectPtr<UARPGFootstepComponent> Footsteps", "PawnClientRestart"):
        if required not in ch28:
            issues.append(f"v2.8 base character footstep integration missing: {required}")
    for required in ("CreateDefaultSubobject<UARPGFootstepComponent>(TEXT(\"Footsteps\"))", "Footsteps->RefreshFootstepRuntime()"):
        if required not in cc28:
            issues.append(f"v2.8 base character footstep runtime integration missing: {required}")

if build_cs.exists() and '"PhysicsCore"' not in build_cs.read_text(errors="replace"):
    issues.append("AkumasRPGFramework.Build.cs must depend on PhysicsCore for physical-surface footsteps")

# v2.9 polished replicated AI-spawner ground-rise entrances.
spawn_entrance_header = root / "Public" / "Components" / "ARPGSpawnEntranceComponent.h"
spawn_entrance_cpp = root / "Private" / "Components" / "ARPGSpawnEntranceComponent.cpp"
if not spawn_entrance_header.exists() or not spawn_entrance_cpp.exists():
    issues.append("v2.9 spawn entrance component source files are missing")
else:
    seh29 = spawn_entrance_header.read_text(errors="replace")
    sec29 = spawn_entrance_cpp.read_text(errors="replace")
    for required in (
        "FARPGSpawnEntranceRepState",
        "ReplicatedUsing=OnRep_RepState",
        "StartGroundRise",
        "IsGroundRiseActive",
        "TickComponent",
    ):
        if required not in seh29:
            issues.append(f"v2.9 spawn entrance replicated API/state missing: {required}")
    for required in (
        "SetIsReplicatedByDefault(true)",
        "DOREPLIFETIME(UARPGSpawnEntranceComponent, RepState)",
        "GetServerWorldTimeSeconds",
        "Mesh->SetRelativeLocation",
        "Movement->DisableMovement()",
        "AI->StopMovement()",
        "AcquireMovementPause(ARPGSpawnEntranceWanderPauseReason",
        "Spline->PauseRoute(true)",
        "AICombat->SetAICombatEnabled(false)",
        "Social->SetSocialInteractionsEnabled(false)",
        "ReleaseMovementPause(ARPGSpawnEntranceWanderPauseReason",
        "Movement->SetMovementMode",
        "Spline->ResumeRoute()",
        "PrimaryComponentTick.bStartWithTickEnabled = false",
    ):
        if required not in sec29:
            issues.append(f"v2.9 spawn entrance runtime missing: {required}")

if spawner_header.exists() and spawner_cpp.exists():
    sh29 = spawner_header.read_text(errors="replace")
    sc29 = spawner_cpp.read_text(errors="replace")
    for required in (
        "bEnableGroundRiseEntrance = true",
        "bAutoCalculateGroundRiseDepth = true",
        "GroundRiseDuration = 1.15f",
        "GroundRiseEaseExponent = 2.25f",
        "bSuspendAIBehaviourDuringGroundRise = true",
        "bLockActorLocationDuringGroundRise = true",
    ):
        if required not in sh29:
            issues.append(f"v2.9 spawner ground-rise authoring missing: {required}")
    for required in (
        "BeginGroundRiseEntrance(Pawn)",
        "Capsule->GetScaledCapsuleHalfHeight() * 2.f",
        "Entrance->StartGroundRise",
    ):
        if required not in sc29:
            issues.append(f"v2.9 spawner ground-rise integration missing: {required}")
    if sc29.find("ConfigureSpawnedPawn(Pawn);") > sc29.find("BeginGroundRiseEntrance(Pawn);"):
        issues.append("v2.9 spawner must configure movement ownership before applying the ground-rise lock")

if ai_character_cpp.exists():
    aic29 = ai_character_cpp.read_text(errors="replace")
    if 'CreateDefaultSubobject<UARPGSpawnEntranceComponent>(TEXT("SpawnEntrance"))' not in aic29:
        issues.append("v2.9 ARPGAICharacter must include the replicated SpawnEntrance component by default")


# v2.10 automatic local proximity character/NPC info popups.
character_info_header = root / "Public" / "Components" / "ARPGCharacterInfoComponent.h"
character_info_cpp = root / "Private" / "Components" / "ARPGCharacterInfoComponent.cpp"
character_info_widget_header = root / "Public" / "UI" / "ARPGCharacterInfoWidget.h"
character_info_widget_cpp = root / "Private" / "UI" / "ARPGCharacterInfoWidget.cpp"
if not character_info_header.exists() or not character_info_cpp.exists() or not character_info_widget_header.exists() or not character_info_widget_cpp.exists():
    issues.append("v2.10 character info popup source files are missing")
else:
    cih210 = character_info_header.read_text(errors="replace")
    cic210 = character_info_cpp.read_text(errors="replace")
    ciwh210 = character_info_widget_header.read_text(errors="replace")
    ciwc210 = character_info_widget_cpp.read_text(errors="replace")
    for required in (
        "bEnableInfoPopup = true",
        "bShowOnAICharacters = true",
        "bShowOnPlayerControlledCharacters = false",
        "ShowDistance = 1100.f",
        "HideDistance = 1350.f",
        "bHideDuringSpawnEntrance = true",
        "bLazyCreateWidget = true",
        "NameTextWidgetName",
        "HealthBarWidgetName",
    ):
        if required not in cih210:
            issues.append(f"v2.10 character info authoring missing: {required}")
    for required in (
        "PrimaryComponentTick.bCanEverTick = true",
        "PrimaryComponentTick.bStartWithTickEnabled = false",
        "SetComponentTickEnabled(true)",
        "SetComponentTickEnabled(false)",
        "SetTickMode(ETickMode::Automatic)",
        "RequestRenderUpdate()",
        "RemoveWidgetFromScreen()",
        "SetIsReplicatedByDefault(false)",
        "SetWidgetSpace(EWidgetSpace::Screen)",
        "GameInstance->GetLocalPlayers()",
        "Character->Stats->GetEffectiveLevel()",
        "Entrance->IsGroundRiseActive()",
        "ActiveDistance = bPopupVisible ? SafeHideDistance : SafeShowDistance",
        "SetOwnerPlayer(LocalPlayer)",
        "InitWidget()",
        "GetWidgetFromName(NameTextWidgetName)",
        "GetWidgetFromName(HealthBarWidgetName)",
    ):
        if required not in cic210:
            issues.append(f"v2.10 character info runtime missing: {required}")
    if "PrimaryComponentTick.bCanEverTick = false" in cic210:
        issues.append("v2.10.1 screen-space CharacterInfo must not permanently disable UWidgetComponent TickComponent")
    release_marker = "void UARPGCharacterInfoComponent::ReleaseWidgetInstance()"
    apply_marker = "void UARPGCharacterInfoComponent::ApplySnapshotToWidget"
    if release_marker in cic210 and apply_marker in cic210:
        release_body = cic210.split(release_marker, 1)[1].split(apply_marker, 1)[0]
        if "SetWidgetClass(nullptr)" in release_body:
            issues.append("v2.10.1 far-widget release must preserve the configured WidgetClass")

    for required in ("FARPGCharacterInfoSnapshot", "BP_OnCharacterInfoUpdated", "CharacterNameText", "LevelText", "HealthBar", "HealthText"):
        if required not in ciwh210:
            issues.append(f"v2.10 native info widget API missing: {required}")
    if "DOREPLIFETIME" in cic210:
        issues.append("v2.10 character info proximity/UI state must remain local-only and not add replication")

character_header_210 = root / "Public" / "Actors" / "ARPGCharacter.h"
character_cpp_210 = root / "Private" / "Actors" / "ARPGCharacter.cpp"
if character_header_210.exists() and character_cpp_210.exists():
    ch210 = character_header_210.read_text(errors="replace")
    cc210 = character_cpp_210.read_text(errors="replace")
    if "TObjectPtr<UARPGCharacterInfoComponent> CharacterInfo" not in ch210:
        issues.append("v2.10 AARPGCharacter must expose inherited CharacterInfo component")
    if 'CreateDefaultSubobject<UARPGCharacterInfoComponent>(TEXT("CharacterInfo"))' not in cc210 or "CharacterInfo->SetupAttachment(GetRootComponent())" not in cc210:
        issues.append("v2.10 AARPGCharacter must construct and attach CharacterInfo component")

# v2.11 complete local JRPG Stats UI requirements.
stats_ui_header = root / "Public" / "Components" / "ARPGStatsUIComponent.h"
stats_ui_cpp = root / "Private" / "Components" / "ARPGStatsUIComponent.cpp"
stats_panel_header = root / "Public" / "UI" / "ARPGStatsPanelWidget.h"
stats_panel_cpp = root / "Private" / "UI" / "ARPGStatsPanelWidget.cpp"
if not all(p.exists() for p in (stats_ui_header, stats_ui_cpp, stats_panel_header, stats_panel_cpp)):
    issues.append("v2.11 requires first-class complete JRPG Stats UI component/widget")
else:
    suh = stats_ui_header.read_text(errors="replace")
    suc = stats_ui_cpp.read_text(errors="replace")
    swh = stats_panel_header.read_text(errors="replace")
    swc = stats_panel_cpp.read_text(errors="replace")
    for required in ("Stats Widget Class", "OpenStatsUI", "CloseStatsUI", "ToggleStatsUI", "GetStatsUISnapshot"):
        if required not in suh:
            issues.append(f"v2.11 StatsUI component missing API/config: {required}")
    for required in ("SetIsReplicatedByDefault(false)", "IsLocallyControlled()", "NM_DedicatedServer", "AddToPlayerScreen", "SetTimer(RefreshTimerHandle"):
        if required not in suc:
            issues.append(f"v2.11 StatsUI runtime missing local/performance path: {required}")
    if "PrimaryComponentTick.bCanEverTick = false" not in suc:
        issues.append("v2.11 StatsUI component must remain no-Tick")
    for required in ("FARPGStatsUISnapshot", "BP_OnStatsUIUpdated", "CloseButton", "StrengthPlusButton", "LuckPlusButton"):
        if required not in swh:
            issues.append(f"v2.11 Stats panel missing ready/custom binding: {required}")
    for required in ("SpendAttributePoints(Stat, 1)", "AddUniqueDynamic", "RequestCloseStatsUI", "MeleeAttackPowerText", "MovementSpeedText"):
        if required not in swc:
            issues.append(f"v2.11 Stats panel runtime missing: {required}")

character_header_211 = root / "Public" / "Actors" / "ARPGCharacter.h"
character_cpp_211 = root / "Private" / "Actors" / "ARPGCharacter.cpp"
if character_header_211.exists() and character_cpp_211.exists():
    ch211 = character_header_211.read_text(errors="replace")
    cc211 = character_cpp_211.read_text(errors="replace")
    if "TObjectPtr<UARPGStatsUIComponent> StatsUI" not in ch211:
        issues.append("v2.11 AARPGCharacter must expose inherited StatsUI component")
    if 'CreateDefaultSubobject<UARPGStatsUIComponent>(TEXT("StatsUI"))' not in cc211:
        issues.append("v2.11 AARPGCharacter must construct StatsUI component")

readme_path = plugin_root / "README.md"
if readme_path.exists():
    readme_lines = readme_path.read_text(errors="replace").splitlines()
    splash = '<img width="1672" height="941" alt="AumaRPGFWSplash"'
    if not any(splash in line for line in readme_lines[:6]):
        issues.append("README GitHub splash image must remain at the top")

# v2.12 complete ready-to-use Inventory + Quick Access UI requirements.
inventory_ui_header = root / "Public" / "Components" / "ARPGInventoryUIComponent.h"
inventory_ui_cpp = root / "Private" / "Components" / "ARPGInventoryUIComponent.cpp"
inventory_widgets_header = root / "Public" / "UI" / "ARPGInventoryWidgets.h"
inventory_widgets_cpp = root / "Private" / "UI" / "ARPGInventoryWidgets.cpp"
if not all(p.exists() for p in (inventory_ui_header, inventory_ui_cpp, inventory_widgets_header, inventory_widgets_cpp)):
    issues.append("v2.12 requires first-class ready Inventory + Quick Access UI component/widgets")
else:
    iuh = inventory_ui_header.read_text(errors="replace")
    iuc = inventory_ui_cpp.read_text(errors="replace")
    iwh = inventory_widgets_header.read_text(errors="replace")
    iwc = inventory_widgets_cpp.read_text(errors="replace")
    for required in (
        "Inventory Widget Class",
        "Quick Access Widget Class",
        "Inventory Slot Widget Class",
        "Quick Access Slot Widget Class",
        "OpenInventoryUI",
        "CloseInventoryUI",
        "ToggleInventoryUI",
        "AssignInventoryItemToQuickAccess",
        "ClearQuickAccessSlot",
    ):
        if required not in iuh:
            issues.append(f"v2.12 InventoryUI API/config missing: {required}")
    for required in (
        "PrimaryComponentTick.bCanEverTick = false",
        "SetIsReplicatedByDefault(false)",
        "IsLocallyControlled()",
        "NM_DedicatedServer",
        "OnInventoryChanged.AddUniqueDynamic",
        "OnQuickAccessChanged.AddUniqueDynamic",
        "SetTimer(CooldownRefreshTimer",
        "ClearTimer(CooldownRefreshTimer)",
    ):
        if required not in iuc:
            issues.append(f"v2.12 InventoryUI runtime missing local/event-driven path: {required}")
    if "DOREPLIFETIME" in iuc:
        issues.append("v2.12 InventoryUI presentation component must remain local-only and non-replicated")
    for required in (
        "FARPGInventoryUISlotView",
        "UARPGInventoryDragDropOperation",
        "UARPGInventoryItemSlotWidget",
        "UARPGInventoryPanelWidget",
        "UARPGQuickAccessBarWidget",
        "NativeOnDragDetected",
        "NativeOnDrop",
        "NativeOnDragCancelled",
        "BP_OnInventorySlotUpdated",
    ):
        if required not in iwh:
            issues.append(f"v2.12 Inventory UI widget API missing: {required}")
    for required in (
        "DetectDragIfPressed",
        "AssignInventoryItemToQuickAccess(Operation->ItemInstanceId",
        "SwapQuickAccessSlots(Operation->SourceSlotNumber",
        "ClearQuickAccessSlot(Operation->SourceSlotNumber",
        "Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible)",
        "CreateWidget<UARPGInventoryItemSlotWidget>(PC, GetClass())",
    ):
        if required not in iwc:
            issues.append(f"v2.12 Inventory UI drag/drop/runtime missing: {required}")
    for required in (
        "UARPGInventoryItemSlotWidget::NativeOnPreviewMouseButtonDown",
        "SlotView.Source != EARPGInventoryUISlotSource::Inventory",
        "ScreenDim->SetVisibility(ESlateVisibility::HitTestInvisible)",
        "InventoryGrid->SetVisibility(ESlateVisibility::SelfHitTestInvisible)",
        "SlotSize->SetVisibility(ESlateVisibility::SelfHitTestInvisible)",
    ):
        if required not in iwc:
            issues.append(f"v2.12.2 Inventory interaction fix missing: {required}")
    for required in (
        "SetVisibility(ESlateVisibility::SelfHitTestInvisible);",
    ):
        if required not in iwc:
            issues.append(f"v2.12.2 Quick Access top-level hit-test fix missing from widget: {required}")
    for required in (
        "ActiveQuickAccessWidget->SetVisibility(bShowQuickAccessHUD ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed)",
        "ActiveQuickAccessWidget->SetVisibility(ESlateVisibility::SelfHitTestInvisible)",
    ):
        if required not in iuc:
            issues.append(f"v2.12.2 Quick Access top-level hit-test fix missing from component: {required}")
    if "ActiveQuickAccessWidget->SetVisibility(bShowQuickAccessHUD ? ESlateVisibility::Visible" in iuc:
        issues.append("v2.12.2 Quick Access top-level viewport widget must not be Visible/hit-testable while shown")

quick_access_header_212 = root / "Public" / "Components" / "ARPGQuickAccessComponent.h"
quick_access_cpp_212 = root / "Private" / "Components" / "ARPGQuickAccessComponent.cpp"
if quick_access_header_212.exists() and quick_access_cpp_212.exists():
    qh212 = quick_access_header_212.read_text(errors="replace")
    qc212 = quick_access_cpp_212.read_text(errors="replace")
    for required in ("ClearSlotAndUnequipActive", "ServerClearSlotAndUnequipActive", "ClearSlotAndUnequipActiveAuthority"):
        if required not in qh212:
            issues.append(f"v2.12 atomic Quick Access clear API missing: {required}")
    for required in ("Equipment->UnequipItem(EquippedInstanceId)", "return ClearSlotAuthority(SlotNumber);", "ServerClearSlotAndUnequipActive_Implementation"):
        if required not in qc212:
            issues.append(f"v2.12 atomic Quick Access clear runtime missing: {required}")

character_header_212 = root / "Public" / "Actors" / "ARPGCharacter.h"
character_cpp_212 = root / "Private" / "Actors" / "ARPGCharacter.cpp"
if character_header_212.exists() and character_cpp_212.exists():
    ch212 = character_header_212.read_text(errors="replace")
    cc212 = character_cpp_212.read_text(errors="replace")
    if "TObjectPtr<UARPGInventoryUIComponent> InventoryUI" not in ch212:
        issues.append("v2.12 AARPGCharacter must expose inherited InventoryUI component")
    if 'CreateDefaultSubobject<UARPGInventoryUIComponent>(TEXT("InventoryUI"))' not in cc212:
        issues.append("v2.12 AARPGCharacter must construct InventoryUI component")
    for required in ("OpenInventoryUI", "CloseInventoryUI", "ToggleInventoryUI", "IsInventoryUIOpen"):
        if required not in ch212 or f"AARPGCharacter::{required}" not in cc212:
            issues.append(f"v2.12 AARPGCharacter Inventory UI wrapper missing: {required}")

inventory_ui_doc = plugin_root / "Docs" / "INVENTORY_UI.md"
if not inventory_ui_doc.exists():
    issues.append("v2.12 Inventory UI documentation is missing")
else:
    inv_doc = inventory_ui_doc.read_text(errors="replace")
    for required in ("Inventory item -> Quick Access slot", "Quick Access slot -> Quick Access slot", "Clear Slot And Unequip Active", "No permanent UI/component Tick"):
        if required not in inv_doc:
            issues.append(f"v2.12 Inventory UI documentation missing behavior: {required}")

# v2.13 complete server-authoritative Item Use system + custom Blueprint behavior.
item_use_behavior_h = root / "Public" / "Items" / "ARPGItemUseBehavior.h"
item_use_behavior_cpp = root / "Private" / "Items" / "ARPGItemUseBehavior.cpp"
item_use_h = root / "Public" / "Components" / "ARPGItemUseComponent.h"
item_use_cpp = root / "Private" / "Components" / "ARPGItemUseComponent.cpp"
if not all(p.exists() for p in (item_use_behavior_h, item_use_behavior_cpp, item_use_h, item_use_cpp)):
    issues.append("v2.13 requires ARPGItemUseBehavior and ARPGItemUseComponent public/private sources")
else:
    iubh = item_use_behavior_h.read_text(errors="replace")
    iucpp = item_use_cpp.read_text(errors="replace")
    iuh = item_use_h.read_text(errors="replace")
    for required in (
        "EARPGItemUseResult",
        "FARPGItemUseContext",
        "CanUseItem",
        "ExecuteItemUse",
        "PlayItemUsePresentation",
        "Blueprintable",
    ):
        if required not in iubh:
            issues.append(f"v2.13 custom Item Use behavior missing: {required}")
    for required in (
        "UseItem(FGuid ItemInstanceId",
        "UseFirstItemById",
        "UseItemAuthority",
        "COND_OwnerOnly",
        "ServerUseItem",
        "ClientReceiveItemUseResult",
        "MulticastPlayItemUsePresentation",
    ):
        if required not in iuh and required not in iucpp:
            issues.append(f"v2.13 Item Use component missing API/network path: {required}")
    for required in (
        "CustomBehavior->CanUseItem",
        "CustomBehavior->ExecuteItemUse",
        "Definition->RestoreHealth",
        "Definition->RestoreMana",
        "Definition->RestoreStamina",
        "Definition->UseGameplayEffect",
        "Inventory->RemoveItemInstance",
        "SetCooldownAuthority",
        "QuickAccess->NotifyItemUsedAuthority",
    ):
        if required not in iucpp:
            issues.append(f"v2.13 Item Use authoritative runtime missing: {required}")

if item_header.exists():
    ih213 = item_header.read_text(errors="replace")
    if "UseBehaviorClass" not in ih213 or "Item Use Behavior Class" not in ih213:
        issues.append("v2.13 Item Definition must expose the custom Item Use Behavior Class")

if character_header.exists() and character_cpp.exists():
    ch213 = character_header.read_text(errors="replace")
    cc213 = character_cpp.read_text(errors="replace")
    for required in ("TObjectPtr<UARPGItemUseComponent> ItemUse", "UseInventoryItem(FGuid", "UseFirstInventoryItemById"):
        if required not in ch213:
            issues.append(f"v2.13 ARPGCharacter Item Use exposure missing: {required}")
    if 'CreateDefaultSubobject<UARPGItemUseComponent>(TEXT("ItemUse"))' not in cc213:
        issues.append("v2.13 ARPGCharacter must create ItemUse automatically")

inventory_ui_h213 = root / "Public" / "Components" / "ARPGInventoryUIComponent.h"
inventory_ui_cpp213 = root / "Private" / "Components" / "ARPGInventoryUIComponent.cpp"
inventory_widgets_h213 = root / "Public" / "UI" / "ARPGInventoryWidgets.h"
inventory_widgets_cpp213 = root / "Private" / "UI" / "ARPGInventoryWidgets.cpp"
if all(p.exists() for p in (inventory_ui_h213, inventory_ui_cpp213, inventory_widgets_h213, inventory_widgets_cpp213)):
    iuih213 = inventory_ui_h213.read_text(errors="replace")
    iuic213 = inventory_ui_cpp213.read_text(errors="replace")
    iwh213 = inventory_widgets_h213.read_text(errors="replace")
    iwc213 = inventory_widgets_cpp213.read_text(errors="replace")
    for required in ("ActivateInventoryItem", "UseInventoryItem"):
        if required not in iuih213 or required not in iuic213:
            issues.append(f"v2.13 Inventory UI direct-use action missing: {required}")
    for required in ("PrimaryActionButton", "PrimaryActionText", "HandlePrimaryActionClicked"):
        if required not in iwh213 or required not in iwc213:
            issues.append(f"v2.13 ready Inventory Use button missing: {required}")

# v2.13.1 equipment physical-socket exclusivity: Inventory, Quick Access and direct Equipment requests
# must converge on one physical attachment owner even when logical EquipmentSlot tags differ.
equipment_h_2131 = root / "Public" / "Components" / "ARPGEquipmentComponent.h"
equipment_cpp_2131 = root / "Private" / "Components" / "ARPGEquipmentComponent.cpp"
if not equipment_h_2131.exists() or not equipment_cpp_2131.exists():
    issues.append("v2.13.1 equipment physical-socket exclusivity source is missing")
else:
    eh2131 = equipment_h_2131.read_text(errors="replace")
    ec2131 = equipment_cpp_2131.read_text(errors="replace")
    for required in (
        "HasEquipmentVisualIntent",
        "SharesExclusiveVisualAttachment",
        "RepairExclusiveVisualAttachmentStateAuthority",
    ):
        if required not in eh2131 or required not in ec2131:
            issues.append(f"v2.13.1 equipment exclusivity helper missing: {required}")
    for required in (
        "const bool bSamePhysicalAttachment = SharesExclusiveVisualAttachment",
        "Other.bEquipped = false",
        "Other.EquipmentSlot = FGameplayTag()",
        "bNeedsExplicitClear",
        "DesiredOwnerBySocket",
        "PreferredActiveQuickAccessInstance",
        "Suppressed duplicate equipment visual",
        "bRepairingExclusiveVisualState",
        "Repaired conflicting equipped item",
    ):
        if required not in ec2131:
            issues.append(f"v2.13.1 equipment exclusivity runtime path missing: {required}")

readme_2131 = plugin_root / "README.md"
if readme_2131.exists():
    readme_text_2131 = readme_2131.read_text(errors="replace")
    splash = '<img width="1672" height="941" alt="AumaRPGFWSplash"'
    if splash not in readme_text_2131[:500]:
        issues.append("README GitHub splash must remain at the top of the document")
    if not release_documented("2.13.1-alpha — Equipment Physical-Socket Exclusivity Fix"):
        issues.append("README or Docs/CHANGELOG.md must document v2.13.1 equipment exclusivity fix")

# v2.13.2 full-vitals consumable guard: local preflight and authority must agree that pure
# Health/Mana/Stamina restoratives cannot be used when none of their configured vitals can increase.
item_use_h_2132 = root / "Public" / "Components" / "ARPGItemUseComponent.h"
item_use_cpp_2132 = root / "Private" / "Components" / "ARPGItemUseComponent.cpp"
stats_cpp_2132 = root / "Private" / "Components" / "ARPGStatsComponent.cpp"
quick_cpp_2132 = root / "Private" / "Components" / "ARPGQuickAccessComponent.cpp"
inv_ui_h_2132 = root / "Public" / "Components" / "ARPGInventoryUIComponent.h"
inv_ui_cpp_2132 = root / "Private" / "Components" / "ARPGInventoryUIComponent.cpp"
widgets_cpp_2132 = root / "Private" / "UI" / "ARPGInventoryWidgets.cpp"
if all(p.exists() for p in (item_use_h_2132, item_use_cpp_2132, stats_cpp_2132, quick_cpp_2132, inv_ui_h_2132, inv_ui_cpp_2132, widgets_cpp_2132)):
    iuh2132 = item_use_h_2132.read_text(errors="replace")
    iuc2132 = item_use_cpp_2132.read_text(errors="replace")
    stc2132 = stats_cpp_2132.read_text(errors="replace")
    qac2132 = quick_cpp_2132.read_text(errors="replace")
    iuih2132 = inv_ui_h_2132.read_text(errors="replace")
    iuic2132 = inv_ui_cpp_2132.read_text(errors="replace")
    iwc2132 = widgets_cpp_2132.read_text(errors="replace")
    for required in ("CanUseItemNow(FGuid", "HasUsefulBuiltInVitalRestore"):
        if required not in iuh2132 or required not in iuc2132:
            issues.append(f"v2.13.2 ItemUse full-vital preflight missing: {required}")
    for required in (
        "ARPGItemUseVitalTolerance",
        "ARPGCanRestoreVital",
        "Health is already full.",
        "Mana is already full.",
        "Stamina is already full.",
        "Stats->Health > Before + KINDA_SMALL_NUMBER",
        "Stats->Mana > Before + KINDA_SMALL_NUMBER",
        "Stats->Stamina > Before + KINDA_SMALL_NUMBER",
        "if (!CanUseItemNow(ItemInstanceId)) return false",
    ):
        if required not in iuc2132:
            issues.append(f"v2.13.2 authority/delta guard missing: {required}")
    if "AppliedDelta <= KINDA_SMALL_NUMBER" not in stc2132:
        issues.append("v2.13.2 Heal must report success only for a real positive Health delta")
    if "CanUseItemNow(Entry->InstanceId)" not in qac2132:
        issues.append("v2.13.2 Quick Access must locally preflight pure consumables")
    if "CanUseInventoryItemNow" not in iuih2132 or "CanUseInventoryItemNow" not in iuic2132:
        issues.append("v2.13.2 Inventory UI must expose/use local consumable preflight")
    if "CanUseInventoryItemNow(SelectedSlotView.ItemInstanceId)" not in iwc2132:
        issues.append("v2.13.2 Inventory Use button must disable when the selected consumable has no useful vital target")

readme_2132 = plugin_root / "README.md"
if readme_2132.exists():
    readme_text_2132 = readme_2132.read_text(errors="replace")
    if not release_documented("2.13.2-alpha — Full-Vitals Consumable Guard Fix"):
        issues.append("README or Docs/CHANGELOG.md must document v2.13.2 full-vitals consumable guard")

# v2.13.3 full-vitals hard gate: secondary Gameplay Effects/custom behavior must not silently
# bypass configured Health/Mana/Stamina restoration usefulness unless the Item Definition opts in.
item_def_h_2133 = root / "Public" / "Data" / "ARPGItemDefinition.h"
if item_def_h_2133.exists() and item_use_h_2132.exists() and item_use_cpp_2132.exists():
    idh2133 = item_def_h_2133.read_text(errors="replace")
    iuh2133 = item_use_h_2132.read_text(errors="replace")
    iuc2133 = item_use_cpp_2132.read_text(errors="replace")
    if "bAllowOtherEffectsWhenRestoredVitalsFull = false" not in idh2133:
        issues.append("v2.13.3 Item Definition must default mixed-effect full-vital bypass to disabled")
    for required in ("HasConfiguredBuiltInVitalRestore", "ShouldBlockForFullConfiguredVitals", "BuildFullVitalsFailureReason"):
        if required not in iuh2133 or required not in iuc2133:
            issues.append(f"v2.13.3 full-vital hard-gate helper missing: {required}")
    for required in (
        "if (ShouldBlockForFullConfiguredVitals(Definition, Stats))",
        "bAllowOtherEffectsWhenRestoredVitalsFull",
        "AppliedHandle.WasSuccessfullyApplied()",
    ):
        if required not in iuc2133:
            issues.append(f"v2.13.3 authority/GameplayEffect success guard missing: {required}")
    if iuc2133.find("if (ShouldBlockForFullConfiguredVitals(Definition, Stats))") > iuc2133.find("CustomBehavior->CanUseItem"):
        issues.append("v2.13.3 authority full-vital guard must execute before custom item-use behavior")

readme_2133 = plugin_root / "README.md"
if readme_2133.exists():
    readme_text_2133 = readme_2133.read_text(errors="replace")
    if not release_documented("2.13.3-alpha — Full-Vitals Hard-Gate Fix"):
        issues.append("README or Docs/CHANGELOG.md must document v2.13.3 full-vitals hard-gate fix")
    splash = '<img width="1672" height="941" alt="AumaRPGFWSplash"'
    if splash not in readme_text_2133[:500]:
        issues.append("README GitHub splash must remain at the top of the document")

# v2.14.0 player crafting + durability + repair + tabbed Item Management UI.
craft_h_2140 = root / "Public" / "Components" / "ARPGCraftingComponent.h"
craft_cpp_2140 = root / "Private" / "Components" / "ARPGCraftingComponent.cpp"
craft_ui_h_2140 = root / "Public" / "UI" / "ARPGCraftingWidgets.h"
craft_ui_cpp_2140 = root / "Private" / "UI" / "ARPGCraftingWidgets.cpp"
inv_h_2140 = root / "Public" / "Components" / "ARPGInventoryComponent.h"
inv_cpp_2140 = root / "Private" / "Components" / "ARPGInventoryComponent.cpp"
equip_cpp_2140 = root / "Private" / "Components" / "ARPGEquipmentComponent.cpp"
combat_cpp_2140 = root / "Private" / "Components" / "ARPGCombatComponent.cpp"
wood_cpp_2140 = root / "Private" / "Components" / "ARPGWoodcuttingComponent.cpp"
inv_widgets_h_2140 = root / "Public" / "UI" / "ARPGInventoryWidgets.h"
inv_widgets_cpp_2140 = root / "Private" / "UI" / "ARPGInventoryWidgets.cpp"
inv_ui_h_2140 = root / "Public" / "Components" / "ARPGInventoryUIComponent.h"
inv_ui_cpp_2140 = root / "Private" / "Components" / "ARPGInventoryUIComponent.cpp"
recipe_h_2140 = root / "Public" / "Data" / "ARPGRecipeDefinition.h"
item_h_2140 = root / "Public" / "Data" / "ARPGItemDefinition.h"
station_cpp_2140 = root / "Private" / "Crafting" / "ARPGCraftingStationActor.cpp"
character_h_2140 = root / "Public" / "Actors" / "ARPGCharacter.h"
character_cpp_2140 = root / "Private" / "Actors" / "ARPGCharacter.cpp"
quick_cpp_2140 = root / "Private" / "Components" / "ARPGQuickAccessComponent.cpp"
save_h_2140 = root / "Public" / "Save" / "ARPGSaveGame.h"
save_cpp_2140 = root / "Private" / "Subsystems" / "ARPGSaveSubsystem.cpp"
types_h_2140 = root / "Public" / "ARPGTypes.h"
paths_2140 = (craft_h_2140, craft_cpp_2140, craft_ui_h_2140, craft_ui_cpp_2140, inv_h_2140, inv_cpp_2140, equip_cpp_2140, combat_cpp_2140, wood_cpp_2140, inv_widgets_h_2140, inv_widgets_cpp_2140, inv_ui_h_2140, inv_ui_cpp_2140, recipe_h_2140, item_h_2140, station_cpp_2140, character_h_2140, character_cpp_2140, quick_cpp_2140, save_h_2140, save_cpp_2140, types_h_2140)
if all(p.exists() for p in paths_2140):
    texts = {p.name + str(i): p.read_text(errors="replace") for i,p in enumerate(paths_2140)}
    ch = craft_h_2140.read_text(errors="replace"); cc = craft_cpp_2140.read_text(errors="replace")
    cuh = craft_ui_h_2140.read_text(errors="replace"); cuc = craft_ui_cpp_2140.read_text(errors="replace")
    ih = inv_h_2140.read_text(errors="replace"); ic = inv_cpp_2140.read_text(errors="replace")
    ec = equip_cpp_2140.read_text(errors="replace"); coc = combat_cpp_2140.read_text(errors="replace"); wc = wood_cpp_2140.read_text(errors="replace")
    iwh = inv_widgets_h_2140.read_text(errors="replace"); iwc = inv_widgets_cpp_2140.read_text(errors="replace")
    iuih = inv_ui_h_2140.read_text(errors="replace"); iuic = inv_ui_cpp_2140.read_text(errors="replace")
    rh = recipe_h_2140.read_text(errors="replace"); idh = item_h_2140.read_text(errors="replace"); stc = station_cpp_2140.read_text(errors="replace")
    charh = character_h_2140.read_text(errors="replace"); charc = character_cpp_2140.read_text(errors="replace")
    qac = quick_cpp_2140.read_text(errors="replace")
    svh = save_h_2140.read_text(errors="replace"); svc = save_cpp_2140.read_text(errors="replace"); th = types_h_2140.read_text(errors="replace")
    for required in ("PlayerRecipes", "ServerCraftRecipe", "ServerRepairItem", "COND_OwnerOnly", "PrimaryComponentTick.bCanEverTick = false", "GetMaxCraftableCount", "GetRepairCost", "bCurrentInputsCommitted", "RefundInputs"):
        if required not in ch and required not in cc: issues.append(f"v2.14.0 player crafting path missing: {required}")
    if "CanFitCommittedOutputs" not in ch or "CanFitCommittedOutputs" not in cc:
        issues.append("v2.14.0 craft completion must use output-only capacity after committed inputs")
    grant_pos = cc.find("bool UARPGCraftingComponent::GrantOutputs")
    if grant_pos >= 0 and cc.find("CanFitOutputs(Recipe, 1)", grant_pos, cc.find("float UARPGCraftingComponent::GetServerTimeSeconds", grant_pos)) >= 0:
        issues.append("v2.14.0 GrantOutputs must not subtract already-committed ingredients again")
    for required in ("AggregateAmounts", "This recipe has no valid crafted output.", "This recipe contains an invalid ingredient entry."):
        if required not in ch and required not in cc: issues.append(f"v2.14.0 crafting transaction/recipe validation missing: {required}")
    for required in ("TObjectPtr<UARPGItemDefinition> Item", "bAllowPlayerCrafting", "CraftingCategory", "MaxBatchSize"):
        if required not in rh: issues.append(f"v2.14.0 recipe authoring missing: {required}")
    for required in ("bUsesDurability", "MaxDurability", "bLoseDurabilityOnCombatHit", "CombatDurabilityLossPerSuccessfulHit", "bLoseDurabilityOnGatheringHit", "GatheringDurabilityLossPerSuccessfulHit", "RepairInputs", "bScaleRepairCostByMissingDurability"):
        if required not in idh: issues.append(f"v2.14.0 durability authoring missing: {required}")
    for required in ("DamageItemDurability", "RepairItemToFull", "GetItemDurability", "GetUnequippedItemCount"):
        if required not in ih or required not in ic: issues.append(f"v2.14.0 inventory durability/repair API missing: {required}")
    transfer_pos = ic.find("bool UARPGInventoryComponent::TransferItemTo")
    transfer_end = ic.find("int32 UARPGInventoryComponent::GetItemCount", transfer_pos)
    transfer_body = ic[transfer_pos:transfer_end] if transfer_pos >= 0 and transfer_end > transfer_pos else ""
    for required in ("if (Definition && Definition->bUsesDurability)", "GetUnequippedItemCount(ItemId) < Quantity", "FARPGInventoryEntry Moved = Entry;", "Destination->Items.Append(MovedEntries);"):
        if required not in transfer_body:
            issues.append(f"v2.14.0 durable storage transfer must preserve runtime item state: {required}")
    if transfer_body and transfer_body.find("if (Definition && Definition->bUsesDurability)") > transfer_body.find("RemoveItemAuthority(ItemId, Quantity)"):
        issues.append("v2.14.0 durable transfer must bypass the legacy remove/re-add path before it can reset durability")
    if "ApplyCombatDurabilityWear" not in ec or "Info.AppliedDamage > KINDA_SMALL_NUMBER" not in coc:
        issues.append("v2.14.0 combat durability must be charged only after real applied damage")
    for required in ("ToolInstanceId", "Tree->ApplyChop", "DamageItemDurability", "GatheringDurabilityLossPerSuccessfulHit"):
        if required not in wc: issues.append(f"v2.14.0 woodcutting durability integration missing: {required}")
    for required in ("EARPGItemManagementTab", "InventoryTabButton", "CraftingTabButton", "MainTabSwitcher", "CraftingPageHost", "DurabilityBar", "BrokenText"):
        if required not in iwh or required not in iwc: issues.append(f"v2.14.0 shared item-management UI missing: {required}")
    for required in ("UARPGCraftingPanelWidget", "UARPGCraftingRecipeRowWidget", "UARPGRepairItemRowWidget", "CraftProgressBar", "RepairButton"):
        if required not in cuh or required not in cuc: issues.append(f"v2.14.0 ready crafting/repair UI missing: {required}")
    for required in ("CraftingWidgetClass", "CraftingRecipeRowWidgetClass", "RepairItemRowWidgetClass", "OpenCraftingUI", "HandleCraftingStateChanged"):
        if required not in iuih or required not in iuic: issues.append(f"v2.14.0 exposed crafting UI integration missing: {required}")
    if "TObjectPtr<UARPGCraftingComponent> Crafting" not in charh:
        issues.append("v2.14.0 inherited Character must expose Crafting component")
    for required in ("CraftRecipe", "RepairInventoryItem", "OpenCraftingUI"):
        if required not in charh or required not in charc: issues.append(f"v2.14.0 inherited character crafting API missing: {required}")
    if "ARPGResolveRecipeAmountId" not in stc or "Amount.Item" not in stc:
        issues.append("v2.14.0 station crafting must remain compatible with direct Item Definition recipe authoring")
    broken_guard = "RequestedDefinition->bUsesDurability && RequestedEntry->Durability <= KINDA_SMALL_NUMBER"
    activation_pos = qac.find("EARPGQuickAccessResult UARPGQuickAccessComponent::ActivateSlotAuthority")
    guard_pos = qac.find(broken_guard, activation_pos)
    previous_handoff_pos = qac.find("FGuid PreviousActiveInstanceId", activation_pos)
    if guard_pos < 0 or previous_handoff_pos < 0 or guard_pos > previous_handoff_pos:
        issues.append("v2.14.0 broken Quick Access equipment must be rejected before active-slot/equipment handoff")
    if "Action == EARPGQuickAccessAction::Equip && Definition->bUsesDurability && Entry->Durability <= KINDA_SMALL_NUMBER" not in qac:
        issues.append("v2.14.0 broken Quick Access equipment should be rejected by local activation preflight")
    if "SaveGame) float Durability" not in th or "SaveVersion = 5" not in svh or "ARPGMigrateLegacyInventoryDurability" not in svc or "Save->SaveVersion < 5" not in svc or "Save->SaveVersion<4" not in svc:
        issues.append("v2.14.0 durability must persist and migrate pre-durability character/world saves")
    for required in ("PersonalCraftingState", "MakeCraftingSaveState", "RestoreCraftingSaveState"):
        if required not in th and required not in ch and required not in cc and required not in svc:
            issues.append(f"v2.14.0 personal craft persistence missing: {required}")
    if "D.PersonalCraftingState = Character->Crafting->MakeCraftingSaveState()" not in svc or "Character->Crafting->RestoreCraftingSaveState(D.PersonalCraftingState)" not in svc:
        issues.append("v2.14.0 save subsystem must persist/resume active personal crafting")
    start_pos_2140 = cc.find("bool UARPGCraftingComponent::StartCraftingAuthority")
    begin_pos_2140 = cc.find("void UARPGCraftingComponent::BeginNextCraftAuthority", start_pos_2140)
    start_body_2140 = cc[start_pos_2140:begin_pos_2140] if start_pos_2140 >= 0 and begin_pos_2140 > start_pos_2140 else ""
    if "BeginNextCraftAuthority();" not in start_body_2140 or "BroadcastCraftingState();" in start_body_2140:
        issues.append("v2.14.0 must not expose/save personal crafting as active before current inputs are committed")
else:
    issues.append("v2.14.0 crafting/durability/UI source set is incomplete")

if any('Units="px"' in p.read_text(errors="replace") for p in source_files):
    issues.append('unsupported UHT Units="px" metadata must not return')

readme_2140 = plugin_root / "README.md"
if readme_2140.exists():
    rt = readme_2140.read_text(errors="replace")
    if not release_documented("2.14.0-alpha"): issues.append("README or Docs/CHANGELOG.md must document v2.14.0 crafting/durability/repair update")
    splash = '<img width="1672" height="941" alt="AumaRPGFWSplash"'
    if splash not in rt[:500]: issues.append("README GitHub splash must remain at the top of the document")


# v2.15.0 settlement building + structural snapping + storage/production ready UI.
build_def_2150 = root / "Public" / "Data" / "ARPGBuildPieceDefinition.h"
build_h_2150 = root / "Public" / "Building" / "ARPGBuildingComponent.h"
build_cpp_2150 = root / "Private" / "Building" / "ARPGBuildingComponent.cpp"
build_actor_h_2150 = root / "Public" / "Building" / "ARPGBuildPieceActor.h"
build_actor_cpp_2150 = root / "Private" / "Building" / "ARPGBuildPieceActor.cpp"
preview_cpp_2150 = root / "Private" / "Building" / "ARPGBuildPreviewActor.cpp"
door_h_2150 = root / "Public" / "Building" / "ARPGBuildDoorActor.h"
door_cpp_2150 = root / "Private" / "Building" / "ARPGBuildDoorActor.cpp"
bui_h_2150 = root / "Public" / "Components" / "ARPGBuildingUIComponent.h"
bui_cpp_2150 = root / "Private" / "Components" / "ARPGBuildingUIComponent.cpp"
bwidgets_h_2150 = root / "Public" / "UI" / "ARPGBuildingWidgets.h"
bwidgets_cpp_2150 = root / "Private" / "UI" / "ARPGBuildingWidgets.cpp"
interaction_h_2150 = root / "Public" / "Components" / "ARPGInteractionComponent.h"
interaction_cpp_2150 = root / "Private" / "Components" / "ARPGInteractionComponent.cpp"
character_h_2150 = root / "Public" / "Actors" / "ARPGCharacter.h"
character_cpp_2150 = root / "Private" / "Actors" / "ARPGCharacter.cpp"
station_h_2150 = root / "Public" / "Crafting" / "ARPGCraftingStationActor.h"
station_cpp_2150 = root / "Private" / "Crafting" / "ARPGCraftingStationActor.cpp"
paths_2150 = (build_def_2150, build_h_2150, build_cpp_2150, build_actor_h_2150, build_actor_cpp_2150,
              preview_cpp_2150, door_h_2150, door_cpp_2150, bui_h_2150, bui_cpp_2150,
              bwidgets_h_2150, bwidgets_cpp_2150, interaction_h_2150, interaction_cpp_2150, character_h_2150, character_cpp_2150, station_h_2150, station_cpp_2150)
if all(p.exists() for p in paths_2150):
    bdh = build_def_2150.read_text(errors="replace"); bh = build_h_2150.read_text(errors="replace"); bc = build_cpp_2150.read_text(errors="replace")
    bah = build_actor_h_2150.read_text(errors="replace"); bac = build_actor_cpp_2150.read_text(errors="replace")
    pc = preview_cpp_2150.read_text(errors="replace"); dh = door_h_2150.read_text(errors="replace"); dc = door_cpp_2150.read_text(errors="replace")
    buih = bui_h_2150.read_text(errors="replace"); buic = bui_cpp_2150.read_text(errors="replace")
    bwh = bwidgets_h_2150.read_text(errors="replace"); bwc = bwidgets_cpp_2150.read_text(errors="replace")
    inh = interaction_h_2150.read_text(errors="replace"); inc = interaction_cpp_2150.read_text(errors="replace")
    ch215 = character_h_2150.read_text(errors="replace"); cc215 = character_cpp_2150.read_text(errors="replace")
    sth = station_h_2150.read_text(errors="replace"); stc = station_cpp_2150.read_text(errors="replace")
    # v2.15.20 inter-story no-gap contract: upper horizontal Wall-family sockets anchor to the
    # slab bottom/story plane, while Foundation walls still anchor to the Foundation top. This keeps
    # Floor-first and Wall-stack-first construction vertically identical and prevents slab thickness
    # from accumulating into facade gaps.
    for required in (
        "const float IncomingWallStoryBaseZ",
        "TargetKind == EARPGBuildPieceKind::Foundation",
        "? IncomingOnTargetTopZ",
        ": AlignBottomPlaneZ",
        "FVector(0.f,  Half, IncomingWallStoryBaseZ)",
        "canonical story seam",
    ):
        if required not in bac:
            issues.append(f"v2.15.20 canonical story-plane Wall snap missing: {required}")
    # v2.15.23 resolves Wall-family facade direction from occupied horizontal cells while preserving
    # each support's own native wall-socket yaw. This makes 1x2/2x2/larger footprints independent of
    # camera side and removes the unsafe mesh-front-axis re-derivation introduced during upper-story polish.
    for required in (
        "ARPGTryGetHorizontalWallFacingClaim",
        "occupied cell tells us WHICH side is outside",
        "own native Wall socket tells us the",
        "actual authored yaw",
        "multi-cell aware",
        "1x2, 2x2 or larger foundation footprint",
        "bFoundHorizontalClaim && !bAmbiguousHorizontalClaim",
        "Rotation.Yaw = FirstNativeYaw",
        "interior partition",
        "Rotation.Yaw = VerticalSupport->GetActorRotation().Yaw",
    ):
        if required not in bc:
            issues.append(f"v2.15.23 multi-cell native Wall-facing resolution missing: {required}")
    # v2.15.22 also validates hosted inserts in reverse so a Door/Window cannot block a later valid
    # Wall/Floor/Ceiling/Roof seam around its verified Doorway/WindowWall host.
    for required in (
        "ARPGInsertActorMatchesHost",
        "ARPGHostedInsertAllowsStructuralNeighbor",
        "Reverse hosted-insert rule",
        "bNeighborIsInsert && bIncomingIsStructural",
        "ARPGIsInsertSnapPair(CandidateHost->Definition->PieceKind, NeighborKind)",
        "!BuildNeighbor->CanActorModify(Owner) || !ResolvedHost->CanActorModify(Owner)",
    ):
        if required not in bc:
            issues.append(f"v2.15.22 reverse hosted-insert structural validation missing: {required}")
    for required in ("Foundation", "Wall", "WindowWall", "Window", "Doorway", "Door", "Floor", "Ceiling", "Roof", "Stair", "Storage", "Production", "BuildMesh", "BuildCost", "ConstructionSeconds", "CustomSnapPoints", "StationDefinition"):
        if required not in bdh: issues.append(f"v2.15.0 build-piece authoring missing: {required}")
    for required in ("BuildCatalog", "BeginBuildMode", "ConfirmPreviewPlacement", "RotatePreview", "NextBuildPiece", "PreviousBuildPiece", "ServerPlacePiece", "FindBestSnapTransform", "bAllowUnlistedBuildRequests", "bRequireSnapTargetModificationAccess"):
        if required not in bh and required not in bc: issues.append(f"v2.15.0 player build mode missing: {required}")
    pa = bc[bc.find("bool UARPGBuildingComponent::PlacePieceAuthority"):]
    for required in ("ResolvePlacementTransform", "EvaluatePlacementInternal", "ConsumeBuildResources"):
        if required not in pa: issues.append(f"v2.15.0 authoritative placement must revalidate: {required}")
    for required in ("BuildCatalog.ContainsByPredicate", "SnapTarget->CanActorModify(Owner)"):
        if required not in bc: issues.append(f"v2.15.0 authority/ownership placement guard missing: {required}")
    for required in ("HasUnequippedItem", "RemoveUnequippedItem", "ARPGAggregateBuildCosts"):
        if required not in bc: issues.append(f"v2.15.0 building resource transaction missing: {required}")
    if '#include "Engine/OverlapResult.h"' not in bc:
        issues.append("v2.15.1 UE5.8.1 compile fix missing: FOverlapResult requires Engine/OverlapResult.h")
    if "UVerticalBox*&OutBox" in bwc:
        issues.append("v2.15.1 UE5.8.1 compile fix missing: reflected TObjectPtr UI bindings cannot bind to UVerticalBox*&")
    if "TObjectPtr<UVerticalBox>&OutBox" not in bwc:
        issues.append("v2.15.1 UE5.8.1 compile fix missing: native Storage/Production UI helper must accept TObjectPtr<UVerticalBox>&")
    if "UARPGStoragePanelWidget*P=" in bwc or "UARPGCraftingStationPanelWidget*P=" in bwc:
        issues.append("v2.15.1 UE5.8.1 compile fix missing: transfer handler local shadowing must not return")
    for required in ("ARPGGetBuildPieceBottomAnchorLocal", "Hit.ImpactPoint - DesiredRotation.RotateVector(BottomAnchorLocal)", "PlacementBoundsCenter", "BottomAnchor + FVector::UpVector * ProbeLift"):
        if required not in bc: issues.append(f"v2.15.2 pivot-aware ground placement missing: {required}")
    if "Hit.ImpactNormal * FMath::Max(0.f, SelectedBuildPiece->PlacementBounds.Z)" in bc:
        issues.append("v2.15.2 ground placement must not blindly lift every mesh by PlacementBounds.Z")
    for required in ("ARPGGetBuildDefinitionLocalBounds", "IncomingOnTargetTopZ", "AlignBottomPlaneZ", "TargetMax.Z + WallHeight - IncomingMin.Z"):
        if required not in bac: issues.append(f"v2.15.2 pivot-aware structural snap math missing: {required}")
    for required in ("MeshRelativeTransform", "FTransform::Identity"):
        if required not in bdh: issues.append(f"v2.15.3 data-driven mesh transform missing: {required}")
    if "RawBox.TransformBy(Piece->MeshRelativeTransform)" not in bc or "RawBox.TransformBy(Piece->MeshRelativeTransform)" not in bac:
        issues.append("v2.15.3 transformed Build Mesh bounds must drive both placement and structural snapping")
    if "BuildMesh->SetRelativeTransform(Definition->MeshRelativeTransform)" not in bac:
        issues.append("v2.15.3 final Build Mesh must apply the Data Asset mesh-relative transform")
    if "PreviewMesh->SetRelativeTransform(Piece->MeshRelativeTransform)" not in pc or "PreviewMesh->SetupAttachment(PreviewRoot)" not in pc:
        issues.append("v2.15.3 preview must apply mesh-relative transform beneath a dedicated scene root")
    for required in ("TargetHalfGrid", "IncomingHalfGrid", "CornerOffsets[]", "FVector( TargetHalfGrid,  IncomingHalfGrid, AlignBottomPlaneZ)", "FVector(-TargetHalfGrid, -IncomingHalfGrid, AlignBottomPlaneZ)", "FRotator(0.f,  90.f, 0.f)", "FRotator(0.f, -90.f, 0.f)"):
        if required not in bac: issues.append(f"v2.15.4 wall L-corner snap graph missing: {required}")
    for required in ("ARPGIsValidSnappedBuildNeighbor", "Neighbor->GetSnapTransformsFor", "PlacementCollisionClearance + 0.5f", "BuildNeighbor->CanActorModify(Owner)"):
        if required not in bc: issues.append(f"v2.15.4 selective snapped-neighbour overlap validation missing: {required}")
    for required in ("FRotator(0.f, -90.f, 0.f), FVector(Half, 0.f, IncomingWallStoryBaseZ)", "FRotator(0.f,  90.f, 0.f), FVector(-Half, 0.f, IncomingWallStoryBaseZ)", "actor local +Y is the", "front/exterior side"):
        if required not in bac: issues.append(f"v2.15.5 directional support-edge wall facing fix missing: {required}")
    for required in ("ARPGGetSnapCandidateSemanticPriority", "ARPGIsHorizontalStructuralKind", "SameSlotTolerance", "bSamePhysicalSlot", "bBetterSemanticOwner", "SemanticPriority < BestSemanticPriority"):
        if required not in bc: issues.append(f"v2.15.5+ same-slot snap semantic priority missing: {required}")
    for required in ("TargetLocalLocation", "HorizontalOffsetSq", "RelativeYawDelta", "bVerticalStackCandidate", "StackFacingToleranceDegrees", "direct wall below remains the second-best semantic owner"):
        if required not in bc: issues.append(f"v2.15.6 vertical wall-stack facing ownership missing: {required}")
    if "inherits the supporting wall's world facing as well as its structural column" not in bac:
        issues.append("v2.15.6 vertical wall stack must explicitly inherit the support wall facing")
    for required in ("ARPGGetCenteredInsertTranslation", "TargetCenter.X - IncomingCenter.X", "TargetCenter.Y - IncomingCenter.Y", "TargetMin.Z - IncomingMin.Z"):
        if required not in bac: issues.append(f"v2.15.7 pivot-aware insert alignment missing: {required}")
    for required in ("ARPGIsInsertSnapPair", "ARPGDistanceSquaredToBuildPieceBounds", "InsertAimDistSq", "CaptureMetricSq", "SemanticTieBreak"):
        if required not in bc: issues.append(f"v2.15.7 insert target-bounds snap capture missing: {required}")
    for required in ("ARPGSegmentIntersectsLocalBox", "ARPGFindViewDirectedInsertSnap", "ARPGHasClearInsertAimPath", "ProbeLocals[]", "TActorIterator<AARPGBuildPieceActor>", "LineTraceSingleByChannel", "FMath::Min(InsertAimDistSq, CandidateDistSq)"):
        if required not in bc: issues.append(f"v2.15.9 deterministic full-view insert targeting missing: {required}")
    for forbidden in ("BlockingHitDistance", "VisibleDistanceLimit"):
        if forbidden in bc: issues.append(f"v2.15.9 insert targeting must not be truncated by generic placement-hit state: {forbidden}")
    for required in ("ARPGIsUpperHorizontalStructuralKind", "ARPGDistanceSquaredToDefinitionBoundsAtTransform", "HorizontalFootprintDistSq", "bHorizontalFootprintCapture", "ARPGRotationsPreservePlacementFootprint", "Piece->PlacementBounds.X - Piece->PlacementBounds.Y", "same physical slot can be advertised"):
        if required not in bc: issues.append(f"v2.15.13 upper-horizontal multi-support placement polish missing: {required}")
    for required in ("ARPGIsValidUpperHorizontalWallSeamNeighbor", "bPreStackedUpperWallAtStorySeam", "bSupportingWallBelow", "bWallBuiltOnSlabTop", "ARPGWallOccupiesHorizontalStructuralEdge", "Floor-first and Wall-stack-first construction produce the same valid result"):
        if required not in bc: issues.append(f"v2.15.14 inter-story Floor/Wall seam validation missing: {required}")
    for required in ("ARPGIsValidWallUnderUpperHorizontalSeamNeighbor", "WallTopZ - HorizontalBottomZ", "Inverse story-bay seam", "upper slab is a legitimate"):
        if required not in bc: issues.append(f"v2.15.15 wall-between-upper-slabs seam validation missing: {required}")
    neighbor_fn = bc[bc.find("static bool ARPGIsValidSnappedBuildNeighbor"):bc.find("UARPGBuildingComponent::UARPGBuildingComponent")]
    if "if (NeighborCandidates.Num() == 0) return false;" in neighbor_fn:
        issues.append("v2.15.16 inter-story seam fallbacks are still unreachable when a neighbour has zero native candidates")
    for required in ("valid inter-story relationships are", "ARPGIsValidUpperHorizontalWallSeamNeighbor(Neighbor, IncomingPiece, IncomingFinal)", "ARPGIsValidWallUnderUpperHorizontalSeamNeighbor(Neighbor, IncomingPiece, IncomingFinal)"):
        if required not in neighbor_fn: issues.append(f"v2.15.16 symmetric inter-story fallback validation missing: {required}")
    for required in ("FARPGPlacementOBB", "ARPGMakePlacementOBB", "ARPGPlacementOBBsOverlap", "ARPGPlacementVolumesOverlapMeaningfully", "ARPGYawAxesEquivalent", "ARPGWallOccupiesHorizontalStructuralEdge", "!ARPGPlacementVolumesOverlapMeaningfully(BuildNeighbor, Piece, Final)"):
        if required not in bc: issues.append(f"v2.15.17 logical structural occupancy collision fix missing: {required}")
    if "Delta - 180.f" not in bc:
        issues.append("v2.15.17 wall structural-axis validation must accept 180-degree front/back equivalence")
    for required in ("EARPGStructuralOccupancyRelation", "ARPGClassifyWallWallStructuralOccupancy", "ARPGClassifyWallHorizontalStructuralOccupancy", "ARPGClassifyStandardStructuralOccupancy", "semantic grid occupancy before falling back to raw OBB penetration"):
        if required not in bc: issues.append(f"v2.15.18 semantic structural-slot collision fix missing: {required}")
    for required in ("GetSnapTransformsFor", "WindowWall", "Doorway", "EARPGBuildPieceKind::Roof && IncomingKind == EARPGBuildPieceKind::Roof", "IncomingKind == EARPGBuildPieceKind::Stair"):
        if required not in bac: issues.append(f"v2.15.0 structural snapping missing: {required}")
    for required in ("ConstructionStartServerTime", "ConstructionDuration", "GetConstructionProgress01"):
        if required not in bah: issues.append(f"v2.15.0 construction replication missing: {required}")
    for required in ("GetServerWorldTimeSeconds", "ConstructionStartScaleZ", "SetScalarParameterValueOnMaterials", "SetActorTickEnabled(true)", "SetActorTickEnabled(false)"):
        if required not in bac: issues.append(f"v2.15.0 construction presentation/performance missing: {required}")
    for required in ("bReplicates = false", "SetActorEnableCollision(false)", "PlacementValid"):
        if required not in pc: issues.append(f"v2.15.0 local placement preview missing: {required}")
    for required in ("ToggleDoor", "ReplicatedUsing=OnRep_DoorOpen", "RestoreDoorOpenState"):
        if required not in dh and required not in dc: issues.append(f"v2.15.0 functional door missing: {required}")
    for required in ("DoorCollision", "UBoxComponent", "RefreshDefinitionPresentation() override", "RefreshConstructionPresentation(bool bForce = false) override"):
        if required not in dh: issues.append(f"v2.15.10 native door collision/presentation hook missing: {required}")
    for required in ("DoorCollision->SetCollisionProfileName(TEXT(\"BlockAll\"))", "Definition->DoorHingeSide == EARPGBuildDoorHingeSide::Left", "bHingeOnLeft ? LocalMax.X : LocalMin.X", "PivotTranslation = DoorHingeLocal - DoorRotation.RotateVector(DoorHingeLocal)", "DoorPivot->SetRelativeLocationAndRotation(PivotTranslation, DoorRotation)", "ECollisionEnabled::QueryAndPhysics"):
        if required not in dc: issues.append(f"v2.15.11 door hinge/motion/collision implementation missing: {required}")
    for required in ("enum class EARPGBuildDoorHingeSide", "Left UMETA(DisplayName=\"Left\")", "Right UMETA(DisplayName=\"Right\")", "DoorHingeSide = EARPGBuildDoorHingeSide::Left"):
        if required not in bdh: issues.append(f"v2.15.11 data-driven Door hinge definition missing: {required}")
    door_tick = bac[bac.find("void AARPGBuildPieceActor::Tick"):bac.find("void AARPGBuildPieceActor::CompleteConstructionAuthority")]
    complete_branch = door_tick[door_tick.find("if (bConstructionComplete)"):door_tick.find("RefreshConstructionPresentation();")] if "if (bConstructionComplete)" in door_tick else ""
    if "SetActorTickEnabled(false)" in complete_branch:
        issues.append("v2.15.10 completed base build Tick must not cancel specialised Door animation Tick")
    for required in ("BuildMenuWidgetClass", "PlacementHUDWidgetClass", "StorageWidgetClass", "CraftingStationWidgetClass", "StructureItemRowWidgetClass", "StationRecipeRowWidgetClass", "DemolishBuiltStructureFromView"):
        if required not in buih: issues.append(f"v2.15.0 exposed BuildingUI reskin class missing: {required}")
    for required in ("UARPGBuildMenuWidget", "UARPGBuildPlacementHUDWidget", "UARPGStoragePanelWidget", "UARPGCraftingStationPanelWidget"):
        if required not in bwh or required not in bwc: issues.append(f"v2.15.0 ready building/structure UI missing: {required}")
    for required in ("TransferItemInstanceTo",):
        if required not in inv_h_2140.read_text(errors="replace") or required not in inv_cpp_2140.read_text(errors="replace"):
            issues.append(f"v2.15.0 exact-instance storage transfer missing: {required}")
    for required in ("DepositToStorageInstance", "WithdrawFromStorageInstance", "WithdrawStationOutputInstance", "ToggleBuiltDoor", "DemolishBuilding", "QueueCraft"):
        if required not in inh or required not in inc: issues.append(f"v2.15.0 authoritative structure interaction missing: {required}")
    for required in ("ServerDemolishBuilding",):
        if required not in inh: issues.append(f"v2.15.0 authoritative structure interaction RPC declaration missing: {required}")
    for required in ("ServerDemolishBuilding_Implementation",):
        if required not in inc: issues.append(f"v2.15.0 authoritative structure interaction RPC implementation missing: {required}")
    for required in ("DemolishBuiltStructure",):
        if required not in ch215 or required not in cc215: issues.append(f"v2.15.0 character building input wrapper missing: {required}")
    for required in ("ARPGAggregateRecipeAmounts", "ARPGCanFitResolvedOutputs", "ConsumeFuelForCraft", "ReplaceInventory(Before)", "StationDefinition->StationTag.MatchesTagExact"):
        if required not in stc: issues.append(f"v2.15.0 production transaction hardening missing: {required}")
    station_can = stc[stc.find("bool AARPGCraftingStationActor::CanUseRecipe"):stc.find("bool AARPGCraftingStationActor::ConsumeRecipeInputs")]
    if "!StationDefinition || !StationDefinition->StationTag.IsValid()" not in station_can:
        issues.append("v2.15.0 station-required recipes must reject missing/wrong station definitions")
    svh215 = save_h_2140.read_text(errors="replace"); svc215 = save_cpp_2140.read_text(errors="replace"); th215 = types_h_2140.read_text(errors="replace")
    for required in ("bConstructionComplete", "ConstructionRemainingSeconds", "bDoorOpen"):
        if required not in th215: issues.append(f"v2.15.0 building persistence state missing: {required}")
    if svh215.count("SaveVersion = 5") < 2:
        issues.append("v2.15.0 world save schema must be v5 while character save remains v5")
    for required in ("RestoreConstructionState", "RestoreDoorOpenState", "ProcessOfflineElapsed()"):
        if required not in svc215: issues.append(f"v2.15.0 world-load integration missing: {required}")

    # v2.15.12 persistent build ownership reload: Guest/no-login identity must survive restarts so
    # loaded build pieces continue to pass the unchanged SnapTarget->CanActorModify authority guard.
    account_cpp_21512 = root / "Private" / "Subsystems" / "ARPGAccountSubsystem.cpp"
    persistence_h_21512 = root / "Public" / "Components" / "ARPGPersistenceComponent.h"
    persistence_cpp_21512 = root / "Private" / "Components" / "ARPGPersistenceComponent.cpp"
    if not account_cpp_21512.exists() or not persistence_h_21512.exists() or not persistence_cpp_21512.exists():
        issues.append("v2.15.12 Guest/build ownership persistence source set is incomplete")
    else:
        acc21512 = account_cpp_21512.read_text(errors="replace")
        ph21512 = persistence_h_21512.read_text(errors="replace")
        pc21512 = persistence_cpp_21512.read_text(errors="replace")
        if "FGuid GuestCharacterId" not in svh215:
            issues.append("v2.15.12 stable GuestCharacterId account-index state is missing")
        for required in ("Index->GuestCharacterId = CharacterId", "if (!bLoggedIn) return Index->GuestCharacterId"):
            if required not in acc21512: issues.append(f"v2.15.12 Guest identity account path missing: {required}")
        if "bDeferredGuestIdentityRecoveryOnce" not in ph21512:
            issues.append("v2.15.12 one-frame legacy Guest identity recovery guard is missing")
        for required in ("if (Last.IsValid())", "Character->CharacterId = Last", "SetTimerForNextTick(this, &UARPGPersistenceComponent::AttemptAutoLoad)", "Accounts->RegisterCharacterId(Character->CharacterId)"):
            if required not in pc21512: issues.append(f"v2.15.12 stable Guest character auto-load missing: {required}")
        if "Last.IsValid() && Accounts->IsLoggedIn()" in pc21512:
            issues.append("v2.15.12 must not discard a valid Guest CharacterId solely because no account login is active")
        for required in ("ARPGRecoverLegacyGuestWorldOwnerIdentity", "PlayerCharacterCount != 1 || !SoleLocalPlayer", "LegacyGuestOwnerIds.Num() != 1", "Accounts->RegisterCharacterId(StableGuestId)", "Record.OwnerCharacterId = StableGuestId", "SoleLocalPlayer->CharacterId = StableGuestId", "ARPGRecoverLegacyGuestWorldOwnerIdentity(W, Save)"):
            if required not in svc215: issues.append(f"v2.15.12 legacy Guest building-owner migration missing: {required}")
else:
    issues.append("v2.15.0 settlement building/storage/production source set is incomplete")

readme_2150 = plugin_root / "README.md"
if readme_2150.exists():
    rt215 = readme_2150.read_text(errors="replace")
    if not release_documented("2.15.0-alpha — Settlement Building"): issues.append("README or Docs/CHANGELOG.md must document v2.15.0 settlement building update")
    if not release_documented("v2.15.12-alpha — Persistent Build Ownership Reload Fix"): issues.append("README or Docs/CHANGELOG.md must document v2.15.12 persistent build ownership reload fix")
    if not release_documented("v2.15.13-alpha — Multi-Support Upper Floor Snap & Collision Polish"): issues.append("README or Docs/CHANGELOG.md must document v2.15.13 upper-floor snap/collision polish")
    if not release_documented("v2.15.14-alpha — Inter-Story Floor/Wall Seam Build-Order Fix"): issues.append("README or Docs/CHANGELOG.md must document v2.15.14 inter-story Floor/Wall seam fix")
    if not release_documented("v2.15.15-alpha — Wall Between Upper Floors Structural Seam Fix"): issues.append("README or Docs/CHANGELOG.md must document v2.15.15 wall-between-upper-slabs seam fix")
    if not release_documented("v2.15.16-alpha — Symmetric Inter-Story Seam Fallback Fix"): issues.append("README or Docs/CHANGELOG.md must document v2.15.16 symmetric inter-story seam fallback fix")
    if not release_documented("v2.15.17-alpha — Logical Structural Occupancy Collision Root Fix"): issues.append("README or Docs/CHANGELOG.md must document v2.15.17 logical structural occupancy collision root fix")
    if not release_documented("v2.15.18-alpha — Semantic Structural Slot Collision Root Fix"): issues.append("README or Docs/CHANGELOG.md must document v2.15.18 semantic structural-slot collision root fix")
    if not release_documented("v2.15.19-alpha - Upper-Story Wall Facing + Insert Host Occupancy Polish"): issues.append("README or Docs/CHANGELOG.md must document v2.15.19 upper-story wall facing and insert host occupancy polish")
    if not release_documented("v2.15.20-alpha — Canonical Story Plane Wall Gap Fix"): issues.append("README or Docs/CHANGELOG.md must document v2.15.20 canonical story-plane wall gap fix")
    if not release_documented("v2.15.21-alpha — Canonical Horizontal Edge Wall Facing Fix"): issues.append("README or Docs/CHANGELOG.md must document v2.15.21 canonical horizontal-edge wall facing fix")
    if not release_documented("v2.15.23-alpha — Multi-Cell Native Wall Facing Root Fix"): issues.append("README or Docs/CHANGELOG.md must document v2.15.23 multi-cell native Wall-facing root fix")
    if not release_documented("v2.15.22-alpha — Hosted Insert Structural Transparency + Wall Facing Continuity Fix"): issues.append("README or Docs/CHANGELOG.md must document v2.15.22 hosted-insert structural transparency and wall-facing continuity fix")
    splash = '<img width="1672" height="941" alt="AumaRPGFWSplash"'
    if splash not in rt215[:500]: issues.append("README GitHub splash must remain at the top of the document")

markers = []
for p in source_files:
    for line_no, line in enumerate(p.read_text(errors="replace").splitlines(), 1):
        if re.search(r"\b(TODO|FIXME|PLACEHOLDER|STUB)\b", line, re.I):
            markers.append(f"{p.relative_to(root)}:{line_no}")
if markers:
    warnings.append(f"development markers found: {markers[:20]}")

line_count = sum(len(p.read_text(errors="replace").splitlines()) for p in source_files)
result = {
    "issues": issues,
    "warnings": warnings,
    "exported_class_count": len(classes),
    "source_file_count": len(source_files),
    "source_line_count": line_count,
}
print(json.dumps(result, indent=2))
sys.exit(1 if issues else 0)

