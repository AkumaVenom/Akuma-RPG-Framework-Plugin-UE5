# Source Validation — Akuma's RPG Framework v2.2.1-alpha

Package: **Akuma's RPG Framework — 2.2.1-alpha Quick Access / Active Item Slots**  
Target: **Unreal Engine 5.8 / 5.8.1**

## Important limitation

This generation environment does not contain Epic's UE 5.8.1 build toolchain, so the package is **not** claimed to have completed a real Unreal Development Editor compile here. The user's local UE 5.8.1 build remains authoritative. Static/source validation is used to catch structural and known source regressions before that build.

All previous real-build fixes through v1.4.1 are retained, including the explicit `Navigation/PathFollowingComponent.h` dependency used by the AI spline component.




## v2.2.1 Quick Access duplicate-instance regression

1. Start with one owned Stone Axe runtime entry (quantity 1) assigned to slot 1. Drag/assign that exact `InstanceId` to slots 4, 7 and 2 in succession. After every assignment, exactly one slot may contain that runtime GUID and the newest target slot must win.
2. Repeat with `Allow Same Item Type In Multiple Slots` enabled. The same exact runtime GUID must still move rather than duplicate.
3. Add two separately owned runtime entries with the same ItemId and different valid `InstanceId` values. With same-item-type duplicates enabled, each distinct instance may occupy its own slot; with the option disabled, assigning either item type moves/removes the other ItemId assignment.
4. Load/replace a deliberately duplicated legacy Quick Access state containing the same runtime GUID in multiple slots. Authority repair must normalize it to unique runtime bindings and must never resolve two slots to the same owned instance.
5. Deplete/remove a bound stack and add a replacement stack of the same ItemId. Rebinding may choose only an unclaimed positive-quantity runtime instance.
6. Verify `Find Slot For Item Instance` returns the exact 1-based slot for a valid bound GUID and 0 for an invalid/unassigned GUID.

## v2.2 Quick Access / consumable checks

The repository validator now asserts that:

- `ARPGCharacter` creates `ARPGQuickAccessComponent` by default and exposes the 1-based input wrappers;
- Quick Access activation resolves real owned runtime Inventory entries and never calls global Item Definition lookup as an ownership substitute;
- equippable slot activation routes through Equipment, while usable items route through the authoritative consumable path;
- consumables support Health/Mana/Stamina restoration, optional Gameplay Effect, exact-stack consumption, cooldown and multicast presentation;
- slot/active state persists in character save data and is owner-only replicated;
- Starting Items can assign a Quick Access slot after their real runtime entry exists.

Recommended in-editor smoke test:

1. Put a sword/axe in Starting Items with `Equip On Spawn = true`, `Quick Access Slot = 1`; put a stack of health potions in slot 2.
2. Bind keyboard `1` to `Quick Access Pressed(1)` and `2` to `Quick Access Pressed(2)`.
3. Press 1 and confirm the exact runtime weapon/tool equips and its normal held visual/audio path remains correct.
4. Damage the character, press 2, and confirm Health restores, quantity decrements, use montage/sound plays and cooldown rejects spam.
5. At full Health, press a health-only potion and confirm it is rejected without consuming quantity.
6. Exhaust a potion stack while another stack exists and confirm the hotbar assignment rebinds to the remaining owned runtime stack.
7. Save/reload and confirm slot assignments/active slot restore against the loaded Inventory GUIDs.
8. In multiplayer PIE, confirm clients cannot create/use an item by assigning a Data Asset alone and remote players still see switched equipment normally.

## v2.1 inventory/equipment usability checks

The static validator now also checks that:

- `Runtime Items` remains read-only editor state while `Starting Items` exposes Item Definition, quantity and Equip On Spawn authoring;
- starting items are converted through the normal authority `Add Item Definition` path and optional auto-equip routes through the Equipment component;
- `ARPGEquipmentVisualActor` exists with static/skeletal mesh support, no collision and no replication by default;
- Item Definitions expose held mesh/custom visual class, socket/relative transform and equipment/tool audio fields;
- Equipment reacts to replicated Inventory changes, creates/attaches/destroys local visuals, and multicasts equip/unequip presentation;
- ordinary melee can prefer equipped-item swing audio while retaining combat-profile fallback;
- Woodcutting uses item gathering swing audio and tree impact can use the equipped tool's gathering-hit audio.

Recommended PIE test: put `DA_StoneAxe` in the player's Starting Items with Equip On Spawn, assign `SM_Stoneaxe`, a valid hand socket and gathering sounds, then confirm host/client both see the axe, Basic Attack chops a tree, Interact still auto-chops, and unequip removes the visual.

## v2.0 Woodcutting / harvestable-tree checks

The static validator additionally checks that:

- `ARPGCharacter` creates the Woodcutting component automatically;
- Item Definitions expose reusable Gathering Tool Tags, Gathering Power and Gathering Tool Tier;
- the Woodcutting component has automatic view targeting, server start/stop RPCs, repeated swing timing, level/XP/progress pure queries and equipped-tool resolution;
- `ARPGTree` exposes Tree Mesh Variations, direct Wood Item rewards, Required Woodcutting Level, optional tool tag/tier gates, fall/stump/respawn settings and Blueprint tree calls/events;
- tree fall is transform-driven and does not teleport the actor; Actor Tick starts disabled and is used only while a trunk is actively falling;
- server chop/fell paths award Woodcutting XP, preflight definition-aware Inventory capacity before adding the selected Item Definition, and route successful item acquisition into normal Collect quest progress;
- tree mesh/state/health/fall direction are replicated and chop/fell cosmetic cues use Niagara with Cascade fallback;
- the explicit UE5.8 Niagara/Cascade pooling enum headers remain present in the new tree feedback translation unit.

### v2.0 recommended runtime cases

1. Create `DA_AshLogs`, create `BP_AshTree : ARPGTree`, add 2-3 tree meshes to Tree Mesh Variations, assign Wood Item and place several trees. Confirm different instances can select different meshes while all clients agree on each selection.
2. Bind player input to `Start Woodcutting From View`; confirm one press begins repeated chopping and leaving range automatically stops it.
3. Fell a tree; confirm per-chop/fell Woodcutting XP is awarded, the selected log item enters Inventory, and the trunk visibly falls away from the player before the stump-only state.
4. Put the log Item Definition into a Build Piece Build Cost and confirm chopped logs can immediately pay that cost.
5. Add a Collect quest objective for the log DefinitionId and confirm successful tree rewards progress it.
6. Create/equip two axe Item Definitions with `Item.Tool.Axe`, different Gathering Tool Tier/Power values, require the tag on the tree, and confirm the best valid equipped tool is used and low-tier tools are rejected when Minimum Tool Tier is raised.
7. Raise Required Woodcutting Level above the player level and confirm the server rejects chopping with the exposed failure result; add XP until the level gate is met and confirm it becomes harvestable.
8. Observe a felled tree through its respawn timer; confirm the stump remains, health resets, and the tree can optionally reroll its visual variation on respawn.
9. Fill the player Inventory so the complete configured wood reward cannot fit; confirm no partial reward is silently added and `On Tree Reward Granted` reports failure.
10. In listen-server + client PIE, chop from the client and confirm damage/rewards are server authoritative while fall/FX/audio are visible on both peers.


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

## v2.0.2 Woodcutting polish checks

The validator now also verifies that:

- normal `ARPGCombatComponent` Basic Attack calls the Woodcutting context redirect before starting an ordinary attack;
- auto-chop from Basic Attack is enabled by default and requires an equipped axe/Woodcutting tool by default;
- real combat/lock-on targets retain priority because explicit non-tree targets are not redirected;
- Basic Attack chops are single-swing and rate-limited by the authored Woodcutting swing interval;
- chop animation can fall back to the normal melee combat montage when no dedicated chop montage is assigned;
- `ARPGTree` exposes minimum/maximum mesh scale, replicated selected scale and Blueprint scale APIs;
- the authority randomizes the tree scale and both trunk/fall hierarchy and stump receive the same multiplier;
- selected tree scale is included in lifetime replication.

## v2.0.1 UE5.8.1 compile-regression checks

The validator also rejects the exact `ARPGTree::GetSelectedTreeMesh()` mixed `TObjectPtr<UStaticMesh>` / raw `UStaticMesh*` conditional expression that produced MSVC C2445 in the user's real UE5.8.1 build. It additionally rejects a direct `NetUpdateFrequency =` write in `ARPGDayNightCycle`, keeping the code on UE5.8's setter API.

## Expected static result

Run:

```text
python Tools/validate_source.py
```

The machine-readable result is emitted separately as `AkumasRPGFramework_validation_v2.1.1.json` during packaging.

## Recommended UE 5.8.1 test pass

1. Replace the complete previous plugin folder with v2.1.1 and clear project/plugin `Intermediate` plus old plugin `Binaries`.
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

### v2.1.1 Equipment / inventory / Woodcutting regression cases

1. Leave `Inventory > Starting Items` empty and use a fresh character/save. `Runtime Items` must remain empty; aiming at a tree and pressing Basic Attack must **not** be treated as an axe chop.
2. Create `DA_StoneAxe` with `DefinitionId` left blank, `Equippable=true`, a valid Equipment Slot, `Item.Tool.Axe`, Static Mesh and gathering sounds. Add it to Starting Items with Equip On Spawn. The runtime ItemId should fall back to `DA_StoneAxe`, and the generated entry must have a valid instance GUID and exact Item Definition soft reference.
3. Confirm the runtime axe entry is actually equipped and `Get Best Equipped Woodcutting Tool Instance Id` returns that same GUID. Merely opening/creating another axe Data Asset must not change this result.
4. Confirm the held visual appears on the authored socket. Enter a nonexistent socket name and verify the component falls back to a valid configured/common hand socket when available and logs a warning instead of silently behaving as if visual setup succeeded.
5. With the valid equipped axe, Basic Attack at an `ARPGTree` must reduce replicated `Current Chop Health` once per allowed swing interval. Unequip/remove the axe and confirm Basic Attack no longer redirects into Woodcutting.
6. Assign Gathering Swing and Gathering Hit sounds on the axe. Confirm the swing cue plays on chop start and the hit cue plays at the successful tree impact; the exact authority-selected tool is used for presentation.
7. Save/load the equipped axe. Confirm the soft Item Definition path survives the new save. For an older ID-only save, configure the matching item in Starting Items and confirm the loaded runtime entry is backfilled without granting a duplicate.
8. Confirm a valid saved equipped axe remains equipped even when Starting Items is empty; saved runtime inventory is authoritative and should not be deleted merely because the default loadout changed.

### v2.0.2 Woodcutting runtime cases

1. Equip an Item Definition tagged `Item.Tool.Axe`, stand in front of an `ARPGTree`, clear lock-on/combat target, and press the existing `Basic Attack`: one chop should occur without wiring a new attack input.
2. Press Basic Attack repeatedly faster than `Swing Interval Seconds`; the tree must not harvest faster than the configured cadence.
3. Lock onto a hostile NPC while holding the axe with a tree also in view; Basic Attack must attack the NPC, not steal the input for Woodcutting.
4. Use `Start Woodcutting From View`; automatic repeated chopping must still work independently of Basic Attack integration.
5. Place several copies of one tree Blueprint with `Minimum Mesh Scale = 0.90` and `Maximum Mesh Scale = 1.10`; runtime instances should receive different sizes while all multiplayer peers agree on each tree's size.
6. Fell a scaled tree and confirm the trunk and stump retain matching scale through the fall/stump transition and optional respawn reroll.

### v1.8 runtime cases

1. Passive neutral chicken attacks only after being hit, kills the player, then immediately clears that temporary hostility; after respawn it ignores the player until attacked again.
2. Truly hostile faction NPC kills the player and is allowed to reacquire after respawn because its authored faction/fallback remains hostile.
3. Spawn at least 8 allied melee NPCs against one player: no more than the configured simultaneous attacker cap should actively commit while the remainder distribute on the waiting ring.
4. Observe waiting NPCs: they should keep facing/focus on the player, orbit/reposition on NavMesh, and rotate into openings after attacks/yields instead of forming one stationary pile.
5. Put an obstacle in one attack slot: the blocked NPC must yield after the slot lease and another NPC should receive an opening.

## Retained compile fixes

The v2.0 tree retains prior fixes discovered through real UE 5.8.1 Development Editor builds: GameplayTags/GameplayTasks descriptor correction, AttributeSet replication fix, battle-pet cooldown fix, CharacterMovement/PlayerState includes, mount Controller shadow naming, targeting `TSubclassOf` ambiguity fix, and AI spline Path Following include fix.

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
