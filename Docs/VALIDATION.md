# Source Validation — Akuma's RPG Framework v1.10.0-alpha

Package: **Akuma's RPG Framework — 1.10.0-alpha Distance-Streamed AI Population**  
Target: **Unreal Engine 5.8 / 5.8.1**

## Important limitation

This generation environment does not contain Epic's UE 5.8.1 build toolchain, so the package is **not** claimed to have completed a real Unreal Development Editor compile here. The user's local UE 5.8.1 build remains authoritative. Static/source validation is used to catch structural and known source regressions before that build.

All previous real-build fixes through v1.4.1 are retained, including the explicit `Navigation/PathFollowingComponent.h` dependency used by the AI spline component.



## v1.10 distance-population checks

The static validator additionally checks that:

- `ARPGAISpawner` exposes default-on distance population streaming, separate activation/despawn radii, unload delay, player-distance mode and route-aware spawned-pawn relevance;
- the runtime iterates server player controllers using UE world player-controller iteration rather than per-frame Actor Tick;
- activation/deactivation paths exist and clean unload removes the pawn destruction delegate before destroying streamed actors;
- inactive population runtime timers are stopped;
- desired/alive-count preservation and pending respawn timing state exist;
- `Respawn Mode = Never` is explicitly handled so streaming cannot resurrect permanent kills;
- Blueprint status, evaluation/control helpers and activation/deactivation delegates remain exposed.

## v1.9.1 UE 5.8.1 compile fix

The user's UE 5.8.1 Development Editor build reached the new day/night translation unit and failed on `#include "Engine/SkyAtmosphere.h"`. UE 5.8 declares both `ASkyAtmosphere` and `USkyAtmosphereComponent` in `Components/SkyAtmosphereComponent.h`. The invalid Engine include has been removed, and the source validator now checks this exact regression.

## v1.9 day/night checks

The static validator additionally checks that:

- the framework contains the placeable replicated `ARPGDayNightCycle` actor and global `ARPGWorldTimeLibrary` Blueprint-pure query library;
- Host System Clock remains the default source and the runtime uses `FDateTime::Now()` on authority;
- the authority clock is replicated and clients extrapolate from replicated time using real elapsed seconds;
- the built-in rig contains Sun, Moon, Sky Light, Sky Atmosphere and Exponential Height Fog components;
- Sky Light Real Time Capture is used for dynamic time-of-day rather than repeated `RecaptureSky()` calls;
- pure Is Day / Is Night / world-hour / date-time / phase / daylight nodes and phase events remain present.

### v1.9 runtime cases

1. Place one `ARPG Day Night Cycle`, leave Host System Clock selected, start PIE and verify the displayed/getter time matches the host PC local clock.
2. Test around configured Day Start / Night Start using Fixed Time; confirm Is Day / Is Night and Dawn/Day/Dusk/Night phase nodes/events change at the authored boundaries.
3. Use Simulated Clock at a high rate; verify sun/moon rotation, Sky Light fill and fog transition smoothly through sunrise/sunset.
4. Listen-server + client: verify both machines report the host's world hour even if the client PC clock differs.
5. Disconnect/reconnect a client and verify the replicated time snaps to the host once, then advances smoothly rather than stepping every replication interval.
6. Disable Built-In Lighting Rig, assign external Sun/Moon/Sky/Fog actors and confirm the same clock drives the project lighting rig.


## v1.8 target-reset + group-combat checks

The static validator now additionally checks that:

- temporary retaliation is configured to restore original disposition on target death by default;
- dead temporary targets can have stale threat removed by default;
- AI binds/unbinds the current target's Combat `OnLifeStateChanged` delegate and has an explicit target-life-state cleanup path;
- coordinated group combat is enabled by default with a finite simultaneous melee-attacker cap;
- coordinated melee grouping, engagement-slot evaluation, combat-ring positioning, Navigation projection and focused waiting/orbit behavior exist in the runtime component;
- runtime group-combat role/slot state is Blueprint-readable.

## v1.7 AI aggro / faction fallback checks

The validator verifies that:

- AI Combat binds automatically to `OnCombatHitReceived`;
- `Retaliate When Attacked` is enabled by default;
- neutral-faction retaliation override is enabled by default;
- missing-faction retaliation fallback is enabled by default;
- nearby ally assistance is enabled by default;
- same-spawner-group and same-class-when-faction-unknown assistance fallbacks exist;
- received aggression is remembered and can remain a valid combat target even when the faction relationship itself is neutral;
- Combat damage legality honors an active AI retaliation target so a class that disallows neutral damage can still fight back;
- ARPG AI Spawner registers itself as the spawned AI's runtime Spawn Group Owner;
- Faction Definitions expose `Default Relationship To Unlisted Factions`;
- `Attack Hostile On Sight` is consumed by runtime automatic target acquisition;
- v1.6 targeting/camera/combat-feedback/stagger requirements remain intact;
- v1.5 spline/group/free-roam requirements remain intact;
- v1.3 automatic NPC ragdoll + death montage fallback remains intact;
- v1.2 target marker compile regression coverage remains intact.

## Expected static result

Run:

```text
python Tools/validate_source.py
```

The machine-readable result is emitted separately as `AkumasRPGFramework_validation_v1.10.0.json` during packaging.

## Recommended UE 5.8.1 test pass

1. Replace the complete previous plugin folder with v1.10.0 and clear project/plugin `Intermediate` plus old plugin `Binaries`.
2. Build Development Editor / Win64.
3. Place one `ARPG Day Night Cycle` and test Host System Clock, Fixed Time and accelerated Simulated Clock before the retained combat/AI regression pass.
4. In a listen-server + client PIE session, verify both peers report the host world time and the client lighting transitions smoothly between clock syncs.
5. Create/use a passive `ARPGAICharacter` chicken with no faction configured. Leave `Retaliate When Attacked`, `Retaliate When Faction Unknown`, `Call For Help When Attacked`, and `Assist Same Class When Faction Unknown` enabled. Attack one chicken and confirm it immediately stops free roam, targets the player, chases and attacks.
6. Place another chicken inside `Ally Assist Radius`; confirm it joins the fight even with no faction Data Asset.
7. Spawn several chickens from one `ARPGAISpawner`; confirm same-spawner-group assistance works even when `Stay Together` is disabled.
8. Assign a Chicken Faction Data Asset and keep Player relationship at `0`; confirm the chicken remains passive until hit, then retaliates because neutral retaliation override is enabled.
9. Set Chicken -> Player relationship negative with `Attack Hostile On Sight = true`; confirm proactive acquisition works.
10. Set `Attack Hostile On Sight = false` while keeping the negative relationship; confirm the chicken does not initiate combat but still retaliates when attacked.
11. Set `Fallback: Attack Players On Sight = true` with missing/neutral faction data; confirm proactive faction-free monster behavior works.
12. Re-test free roam/spline resumption after combat, ragdoll death, lock-on camera, hit FX/audio and stagger/knockback.

### v1.8 runtime cases

1. Passive neutral chicken attacks only after being hit, kills the player, then immediately clears that temporary hostility; after respawn it ignores the player until attacked again.
2. Truly hostile faction NPC kills the player and is allowed to reacquire after respawn because its authored faction/fallback remains hostile.
3. Spawn at least 8 allied melee NPCs against one player: no more than the configured simultaneous attacker cap should actively commit while the remainder distribute on the waiting ring.
4. Observe waiting NPCs: they should keep facing/focus on the player, orbit/reposition on NavMesh, and rotate into openings after attacks/yields instead of forming one stationary pile.
5. Put an obstacle in one attack slot: the blocked NPC must yield after the slot lease and another NPC should receive an opening.

## Retained compile fixes

The v1.10 tree retains prior fixes discovered through real UE 5.8.1 Development Editor builds: GameplayTags/GameplayTasks descriptor correction, AttributeSet replication fix, battle-pet cooldown fix, CharacterMovement/PlayerState includes, mount Controller shadow naming, targeting `TSubclassOf` ambiguity fix, and AI spline Path Following include fix.

## v1.10 detailed distance-population runtime cases

1. Place an `ARPGAISpawner` with `Enable Distance Based Population` on, Activation Radius 3000 and Despawn Radius 4500. Start farther than 4500 units away and confirm no spawned pawns exist.
2. Walk inside 3000 units and confirm the configured group appears and `Is Population Active` becomes true.
3. Walk between 3000 and 4500 units and confirm the population stays loaded (hysteresis).
4. Walk beyond 4500 units and remain there longer than `Distance Despawn Delay`; confirm the actors are destroyed without `On Spawn Group Defeated` firing.
5. Return inside 3000 units and confirm the group is rebuilt from the spawner.
6. On a long spline, follow a spawned NPC far beyond the spawner's Despawn Radius with `Keep Loaded Near Spawned Pawns` on; confirm it remains loaded while the player is near that NPC.
7. Kill one member of an Individual-respawn group, leave before its respawn delay expires, then return immediately; confirm previously living members return while the killed member waits for the remaining cooldown.
8. Repeat with `Respawn Mode = Never`; confirm killed members do not return after distance unload/reload.
9. With the spawner inactive, profile that its leash/cohesion timers are stopped and only the staggered population relevance timer remains.
