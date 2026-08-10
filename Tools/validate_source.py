from pathlib import Path
import json
import re
import sys

plugin_root = Path(__file__).resolve().parents[1]
root = plugin_root / "Source" / "AkumasRPGFramework"
issues = []
warnings = []
source_files = [p for p in root.rglob("*") if p.suffix in {".h", ".cpp"}]

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
    for required in ("GetRandomReachablePointInRadius", "CurrentTarget", "ForceReturnHome", "EnsureThinkTimer"):
        if required not in wanderer_text:
            issues.append(f"Wanderer/free-roam missing required runtime path: {required}")

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
        "Entry.ItemDefinition = ExplicitDefinition",
        "BackfillDefinitionReference",
        "Slot != Definition->EquipmentSlot",
        "OccupiedEquipmentSlots",
    ):
        if required not in ic211:
            issues.append(f"v2.1.1 exact runtime Item Definition path missing: {required}")
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

try:
    descriptor = json.loads((plugin_root / "AkumasRPGFramework.uplugin").read_text())
    if descriptor.get("Version") != 2101 or descriptor.get("VersionName") != "2.1.1-alpha":
        issues.append("package descriptor must identify v2.1.1-alpha")
    plugin_refs = {entry.get("Name") for entry in descriptor.get("Plugins", []) if isinstance(entry, dict)}
    for module_only_name in ("GameplayTags", "GameplayTasks"):
        if module_only_name in plugin_refs:
            issues.append(f".uplugin incorrectly declares runtime module {module_only_name} as a plugin dependency")
    if "Niagara" not in plugin_refs:
        issues.append(".uplugin must enable Niagara for v1.6 combat feedback")
except Exception as exc:
    issues.append(f"invalid .uplugin JSON: {exc}")

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
