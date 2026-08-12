# Akuma's RPG Framework — UE 5.8

<img width="1672" height="941" alt="AumaRPGFWSplash" src="https://github.com/user-attachments/assets/42618c54-4728-4a36-9d18-9e2b8181c455" />

**Akuma's RPG Framework** is a source-first Unreal Engine 5.8 gameplay framework for building large single-player RPGs while keeping authoritative multiplayer paths available from day one.

The design goal is simple: create content with Data Assets, assign components/definitions in the editor, and let shared C++ systems handle the repetitive RPG plumbing—saving, replication, item movement, quest progress, factions, crafting, spawning, death/respawn, chat routing, and cross-system events.

> **Build status:** source framework alpha. The code has passed repository-level structural validation in the generation environment, but it has **not** been compiled against your local UE 5.8 installation here. A real UE 5.8 Development Editor build and in-project PIE/runtime QA are required before shipping.


## 2.10.1-alpha automatic proximity character / NPC info popups — runtime visibility fix

Every `AARPGCharacter` now includes an inherited **CharacterInfo** component. Select it in any player/NPC Character Blueprint, choose a **Widget Class**, and the framework automatically presents replicated character name, effective level and health above the character only while a local player is nearby.

The system defaults to NPC-only visibility, automatically positions from capsule height, hides during the v2.9 ground-rise entrance, uses show/hide distance hysteresis, and keeps proximity decisions on staggered local timers. v2.10.1 fixes screen-space rendering by preserving UWidgetComponent's native TickComponent capability and enabling it only while a popup is visible; distant popups disable that component tick. Dedicated-server UI work is skipped, and far-away widget instances can still be retired. Custom widgets can either derive from `ARPGCharacterInfoWidget` or use the exposed standard child-name mapping for zero-graph Name/Level/Health binding. See `Docs/NPC_INFO_POPUP.md`.

## 2.9.0-alpha polished AI spawner ground-rise entrances

`ARPGAISpawner` now gives every spawned framework AI an automatic replicated **Ground Rise Entrance** by default. The accepted collision-safe pawn/capsule remains at its final NavMesh position while only the skeletal mesh starts below the floor and eases upward, preserving the v2.7.2 spawn-collision proof instead of physically spawning capsules underground. During the short reveal, CharacterMovement, AIController pathing, Free Roam, Spline travel, combat AI and ambient social behaviour are held; each owner is restored independently afterward. The entrance runs for initial groups, respawns, distance reloads and Day/Night population swaps. Depth can auto-scale from capsule height and duration/delay/easing/behaviour locking are exposed directly on each spawner. See `Docs/AI_SPAWNER_GROUND_RISE.md`.

## 2.8.0-alpha automatic replicated footsteps

Every `ARPGCharacter` now includes an inherited **Footsteps** component, so players and `ARPGAICharacter` NPCs gain automatic grounded travel-distance footsteps with no animation notify wiring. Each due foot contact traces the actual floor, resolves its UE Physical Surface, selects from exposed randomized sound pools, and supports optional attenuation/concurrency assets. Multiplayer uses server-authoritative unreliable multicast cues plus owner-side local prediction for responsive player audio; NPCs remain server-driven. No permanent Tick is used. See `Docs/FOOTSTEPS.md`.

## 2.7.2-alpha spawned AI navigation readiness

v2.7.1 hardens Free Roam startup for spawned NPCs. Spawner-owned Free Roam/Spline pawns explicitly ensure their default AI controller exists before movement configuration. Wanderer no longer assumes its first synchronous `MoveToLocation` succeeded: it checks Unreal's path-following request result, performs short server-only retries while AI possession/NavMesh/path following are still becoming ready, and only marks Free Roam established after a real navigation request is accepted. Social AI waits for that locomotion handshake before reserving a Free-Roam NPC, preventing the misleading state where a pawn can rotate/focus on another actor but never translate. No reflected Blueprint API changed.

## 2.7.0-alpha dynamic Day/Night street lights

`AARPGDynamicStreetLight` is a new Blueprint-derivable lamp/world-light actor that reads the existing `ARPGDayNightCycle`. It exposes an inherited movable Point Light plus Niagara and Cascade components, can auto-discover or explicitly bind a cycle, applies the correct state immediately at startup, and then reacts to Day/Night events without a permanent Tick. By default lamps are on during Night + Dawn and off during Day + Dusk; each phase is independently configurable. Niagara can be preferred with automatic Cascade fallback, or both/one/none can be selected. Blueprint-added light components can optionally follow the same state, and an overridable state-change event supports emissive/audio/custom presentation. See `Docs/DYNAMIC_STREET_LIGHTS.md`.


## 2.6.1-alpha Free-Roam / social handoff reliability

v2.6.1 hardens the movement ownership boundary between **Spawner Free Roam**, **ambient social interactions**, **group cohesion recovery**, **Spline routes**, and combat. Temporary systems no longer disable the Wanderer itself. Instead they acquire/release independent native pause reasons, so one system cannot accidentally restore or permanently disable another system's movement state.

Spawned Free-Roam NPCs use the Wanderer startup path when the spawner enables roaming; v2.7.1 further hardens this with navigation-readiness retries and accepted-request verification. Social AI also waits through an initial opportunity grace window before its first ambient pairing, giving newly spawned actors time to receive their movement configuration and disperse naturally. Group-cohesion recovery continues issuing its leader recovery request until the configured recovery radius is actually reached, and social interactions do not compete with that recovery ownership.

This patch does not add or rename reflected Blueprint fields/functions. Existing `Free Roam`, `Stay Together`, social, faction, combat and spline authoring remains intact.

## 2.6.0-alpha ambient NPC social interactions

`AARPGAICharacter` now includes an optional inherited **AISocial** component. It is disabled by default. When enabled, compatible nearby NPCs can naturally pause, approach, face one another and perform randomized ambient exchanges before resuming their previous Wanderer or Spline movement. Social pairing respects same/friendly/neutral/factionless authoring and **always rejects hostile relationships**, supports optional GameplayTag archetype matching, and requires a shared Interaction Id while allowing each NPC/skeleton to use its own local montage/audio/text content.

The system is server-authoritative, timer/overlap based rather than permanently ticking, uses opportunity and partner cooldowns to prevent crowd spam, and immediately yields to combat target acquisition, incoming damage, defensive/combat states or death. Blueprint events expose interaction start/end and spoken lines for speech bubbles/subtitles or project-specific presentation. See `Docs/AI_SOCIAL_INTERACTIONS.md`.


## 2.5.4-alpha packaged-build compile fix

A real UE 5.8.1 Windows packaging test exposed two `ARPGDayNightCycle.cpp` compile errors that Editor/PIE use had not surfaced. External Sun/Moon directional-light resolution now uses the runtime-safe actor component lookup path, removing the non-editor `ADirectionalLight::GetComponent()` dependency. The same package log also exposed two non-fatal UE 5.8 soft-pointer deprecation warnings in Inventory; those assignments now construct the soft reference through `FSoftObjectPath` without changing Inventory's public schema or ownership semantics.

No reflected/public header changed in this patch. Re-run a clean Windows package after installing v2.5.4-alpha; that real Unreal package is the acceptance test for this correction.



## 2.5.3-alpha NPC scaling editor exposure fix

- `Scale NPC To Player` is now always editable in the Stats component instead of being greyed out by the outer JRPG enable edit condition.
- Explicitly opting an AI NPC into player scaling now auto-enables the JRPG stat layer at runtime if the separate JRPG checkbox was accidentally left off, while player-controlled pawns are never auto-converted.
- v2.5.2 manual Base Character Level / PIE testing controls remain fully intact.

Every player/NPC has one authoritative `Progression` component. Select it in the Blueprint and set `Progression -> Level -> Base Character Level` to author the character level directly; enabled JRPG Stats reads that value automatically and rebuilds natural/derived stats from it.

For rapid PIE validation, enable `Progression -> Testing -> Enable Manual Level Override` and set `Manual Test Level`. The override applies through the real progression event path at BeginPlay, so Stats, player-relative NPC scaling and other level consumers all observe the same value. Disable it for normal save/XP-driven play. Runtime Blueprint tests can also use `Set Level` or `Apply Manual Test Level Now` (also exposed as a Details-panel Call In Editor button).

## 2.5.1-alpha player-relative NPC level/stat scaling

NPCs using the JRPG stat layer can now opt into **Scale NPC To Player**. Scaling does not copy the player's Strength/health/damage and does not overwrite the NPC's real Progression Level. Instead it computes a replicated runtime **Effective Level**, then reevaluates the NPC's own authored Strength/Vitality/Magic/Spirit/Dexterity/Luck growth and every derived stat at that level. This preserves creature identity while keeping enabled enemies relevant to the player.

The system supports level offsets (for elites or intentionally weaker creatures), partial level matching, min/max level brackets, scale-up/down controls, multiplayer reference policies, dead-player filtering, no-player fallback and an encounter lock that prevents stats changing in the middle of combat. Server refreshes iterate player controllers only and are staggered to avoid synchronized AI spikes. Attribute Points remain tied to the NPC's authored base level, so runtime scaling cannot mint progression rewards.

For nameplates/UI, use `Stats -> Get Effective Level` (or `Get Stat Snapshot`, whose Level is effective while scaling is active). Use `Get Base Progression Level` when the authored template level is required. See `Docs/NPC_PLAYER_LEVEL_SCALING.md`.

## 2.5.0-alpha classic JRPG stats + attribute-point progression

`ARPGStatsComponent` now has an opt-in six-primary-stat progression model: **Strength, Vitality, Magic, Spirit, Dexterity and Luck**. Natural stats grow automatically from the existing character Level, while derived Melee/Ranged/Magic Attack Power, Physical/Magic Defense, Accuracy, Physical/Magic Evasion, Speed, critical chance/damage, attack speed, movement speed and max vitals are rebuilt authoritatively.

Level-ups now grant configurable **Attribute Points** that players can spend on the six primary stats through a server-validated Blueprint API. Allocations and unspent points save/replicate, old pre-v2.5 saves receive a clean migration state, and equipped runtime items can contribute first-class primary/derived/vital stat modifiers. Combat now consumes separate melee/ranged/magic power, physical/magic defense, Luck-driven critical bonuses and synchronized Speed-driven attack timing. Existing projects remain safe because `Enable JRPG Stat System` is disabled by default and the old `AttackPower` / `SpellPower` / `Armor` behavior remains unchanged until a character opts in.

See `Docs/JRPG_STATS.md`.

## 2.4.2-alpha AI dodge authoring + stagger escape

- Added **AI Dodge Chance** directly to `Combat -> Dodge`, matching the editor location where dodge montages, Duration, Distance, invulnerability and root-motion settings are authored.
- Added **Dodge Cancels Stagger** (enabled by default). A successful dodge can now break an active stagger cleanly, clear its timer/state and residual knockback velocity, then take over movement/animation as the new defensive state.
- Fast incoming attacks now receive a usable automatic-defence reaction window even with the AI combat Think timer, reducing cases where a valid dodge opportunity was skipped between ticks.
- The pre-2.4.2 AICombat `DodgeChance` field is retained unchanged for serialized compatibility; the new Combat-profile field is the primary control.
- v2.4.1 navigation-abort and lateral dodge displacement fixes remain intact.

## 2.4.1-alpha AI dodge movement reliability

- Fixed NPC dodge decisions playing/entering dodge state without reliable visible lateral displacement while an AI `MoveTo` was active.
- AI navigation is aborted before dodge motion begins, CharacterMovement velocity is cleared before the configured launch is applied, and combat MoveTo bookkeeping is reset so pursuit resumes cleanly after the dodge.
- Root-motion-only dodge authoring is preserved: when `Use Root Motion Only` is enabled the framework does not inject code-driven displacement, but AI path following is still suspended so montage root motion is not fought by navigation.
- No Blueprint-facing public header/schema changes versus v2.4.0-alpha.

## 2.4.0-alpha day/night AI population swapping

`ARPGAISpawner` can now optionally swap its runtime population at **true midnight (00:00)** and restore the normal/daylight population when the authoritative `ARPGDayNightCycle` reaches its configured **Day Start Hour**. The existing `Spawn Table` remains the daylight/default table, while a new weighted `Midnight Spawn Table` accepts night-only enemies such as undead, nocturnal wildlife or event creatures.

Enable `Enable Midnight Population Swap` on only the spawners that should participate. When enabled, a phase transition cleanly removes the old phase population without treating the cleanup as death/defeat, cancels stale respawn state, then spawns the new phase population if the spawner is currently loaded. Distance-streamed spawners that are unloaded simply remember the new phase and spawn the correct table when a player returns. When the option is disabled, the existing preserved population/respawn/distance-streaming behavior is unchanged.

The spawner automatically finds the first `ARPGDayNightCycle` in the world, with an optional per-instance override for multi-clock/test maps. Midnight can optionally use its own Min/Max group-size range. See `Docs/AI_SPAWNER_DAY_NIGHT.md`.


## 2.3.0-alpha melee combat polish

This release fixes three combat-feel problems without changing the framework's Blueprint-facing public headers:

- ordinary damage no longer plays a Hit React montage over an attack that is already in progress; only real interruption states such as critical stagger, parry/guard break, dodge/death can cancel the visible attack;
- critical stagger stops AI path following immediately, clears current movement velocity before launch, applies the full configured knockback vector, and owns its stagger audio/FX cue at the exact successful-stagger point;
- automatic melee AI no longer repeatedly calls `PerformBasicAttack` while already attacking (which previously filled the combo queue every Think), and the existing `Attack Slot Cooldown After Attack` now also paces solo melee AI after attack recovery.

For a chicken or other quick wildlife attacker, a good starting point is `Attack Slot Cooldown After Attack = 0.8-1.25`. The actual interval is the attack step's recovery/impact duration plus this breathing-room value.

Critical stagger remains intentionally **critical-hit-only**, preserving the original framework design. Remember that the effective frequency is `Critical Chance × Critical Stagger Chance`; for a deterministic editor test, temporarily set both to `1.0`, verify stagger montage/sound/knockback, then restore gameplay values.


## 2.2.3-alpha.2 Blueprint compatibility hotfix

This release preserves the exact public Quick Access C++ header/reflection schema from 2.2.3-alpha.1 while implementing active-slot replacement auto-equip entirely in private C++ logic. Existing Blueprint casts and serialized nodes therefore do not need to be reconstructed for this gameplay fix.


## 2.2.3-alpha.1 UE 5.8 compile correction

Corrected `UARPGQuickAccessComponent::ClearSlotAuthority` so its canonical-slot guard returns the function's actual `bool` type (`false`) instead of an `EARPGQuickAccessResult` enum value. This is a compile-only correction to the 2.2.3 active-equipment handoff release; gameplay behavior and Blueprint APIs are otherwise unchanged.

## 2.2.3-alpha Quick Access active-equipment handoff fix

Quick Access now enforces one active held weapon/tool by default. Switching from an Axe to a Sword (or replacing the active slot and then activating its new item) explicitly unequips the previously Quick-Access-activated runtime item first, even if Tool and Weapon use different logical Equipment Slot tags. Armor and unrelated equipment remain untouched, and consumables do not unequip the held item.

## 2.2.2-alpha Quick Access atomic-move integrity fix

Quick Access assignment now carries a per-slot assignment revision and canonicalizes duplicate state with the **newly dropped target slot as the winner**. This removes array-order ambiguity from legacy/save/replication repair. UI-facing `Get Slot View` also refuses to expose a non-canonical duplicate, so one runtime item cannot render as multiple hotbar entries even if stale state is encountered during migration.

## 2.2.1-alpha Quick Access duplicate-instance integrity fix

Quick Access now enforces a strict runtime identity invariant: **one owned runtime item instance GUID can occupy at most one hotbar slot**. Dragging the same Stone Axe, sword, tool, food stack or potion stack to another slot is therefore a real move at the authoritative component layer, not a UI convention. This remains true even if `Allow Same Item Type In Multiple Slots` is enabled; that option now only permits *different owned runtime instances/stacks* of the same ItemId in separate slots.

Assignment, load-time repair and fallback rebinding all exclude runtime instances already claimed by another slot. Existing duplicate state from older v2.2.0 saves/runtime sessions is normalized automatically on authority. `Find Slot For Item Instance` is also available to Blueprint for exact-GUID diagnostics/UI. See `Docs/QUICK_ACCESS.md`.

## 2.2.0-alpha Quick Access / active item slots

The ready `ARPGCharacter` now includes a first-class **Quick Access** component for the familiar action-RPG hotbar workflow: assign owned runtime inventory items to numbered slots, press a slot to make it active, and let the framework perform the correct action automatically. Equippable weapons/tools route through the existing authoritative Equipment component; usable food/potions route through the new generic item-use path; selection-only items can still be pinned for project-specific behavior.

Quick Access is built on top of the v2.1.1 runtime-item rule rather than around it. A slot stores a stable ItemId bookmark plus the exact currently-bound inventory instance GUID. If a consumable stack is depleted or rebuilt, the authority can rebind the slot to another **owned** runtime stack of the same item. Merely having an Item Definition Data Asset in Content never makes the hotbar item available. Slot assignments and the active slot save with the character and replicate owner-only, while actual equipment remains normal replicated Inventory/Equipment state for other players.

Item Definitions now expose `Quick Access Action`, `Usable`, consume quantity, use cooldown, Health/Mana/Stamina restoration, optional GAS Gameplay Effect, use montage and use sound. This lets a health potion or food item work without a one-off Blueprint graph while still leaving GAS available for buffs and project-specific effects. Starter inventory entries can optionally specify a Quick Access slot so a starting sword/axe can spawn directly into slot 1, slot 2, etc.

For player input, the clean path is simply `Quick Access Pressed(1)`, `Quick Access Pressed(2)`, and so on. `Quick Access Next` / `Quick Access Previous` are included for D-pad or wheel cycling, and `Use Active Quick Access Item` supports a separate use button when desired. See `Docs/QUICK_ACCESS.md`.

## 2.1.1-alpha equipment + Woodcutting runtime fix

Runtime inventory entries now retain the exact Item Definition soft reference that created them. Equipment visuals/audio and Woodcutting consume validated equipped runtime instances instead of treating a definition lookup as equipment state. Item Definition assets can also use their asset name automatically when `DefinitionId` is left blank.

This release closes the editor-authoring gap around items and equipment. `Inventory -> Runtime Items` remains intentionally read-only because those entries contain generated instance GUIDs, equipped state and save data; designers now get an editable `Inventory -> Starting Items` array that accepts `ARPGItemDefinition` assets directly, supports quantities, and can auto-equip selected starting gear. Automatic seeding is delayed until after the normal persistence auto-load pass so saved characters keep their stored inventory without a transient starter-loadout flash.

Equipped items can now own presentation directly from the Item Definition: static/skeletal held mesh, optional custom `ARPGEquipmentVisualActor` class, attach socket, relative transform, equip/unequip montage and equip/unequip/combat/gathering audio. Equipment visuals are rebuilt locally from replicated inventory state, keeping equipment authority on the server without replicating an extra cosmetic weapon actor. Woodcutting automatically consumes the equipped tool's gathering swing/hit sounds, and ordinary melee can prefer the equipped weapon's swing sound before falling back to the Class/Combat Profile sound.

See `Docs/EQUIPMENT_INVENTORY.md` and `Docs/WOODCUTTING.md`.

## 2.0.2-alpha Woodcutting combat + tree-variation polish

This polish release keeps the full v2.0 Woodcutting system and makes it feel much more natural in ordinary action-RPG play. With **Basic Attack Auto Chops Trees** enabled (default), the normal Basic Attack input automatically becomes one Woodcutting swing when the character has an equipped axe and is aiming at an `ARPGTree`. A valid combat/lock-on target keeps priority, while the existing `Start Woodcutting From View` interaction remains available for automatic repeated chopping.

`ARPGTree` also now owns replicated per-instance size variation. Every derived tree Blueprint can expose **Minimum Mesh Scale** and **Maximum Mesh Scale** (defaults 0.90-1.10), and the authority chooses one uniform size for each tree so nearby variants do not all look cloned even when their source meshes are similar. The same selected scale is applied to the falling trunk and stump and can optionally reroll when the tree respawns.

The retained v2.0 Woodcutting feature set includes:

- inherited `ARPGWoodcuttingComponent` with persistent 1-99 progression, XP/unlocks and level-based chop power;
- equipped gathering-tool tags, tier and power for axe progression;
- `ARPGTree` mesh arrays plus replicated mesh index **and replicated mesh scale**;
- direct Wood Item / bonus-drop rewards into the normal Inventory for Building/Crafting/Collect quests;
- server-authoritative chop validation, falling trunk, stump and timed respawn;
- Niagara/Cascade/audio feedback and dedicated-or-combat-montage chopping animation;
- Blueprint APIs/events for custom tree behavior;
- the v2.0.1 UE5.8.1 compile corrections remain intact.

See `Docs/WOODCUTTING.md`.

## 1.10.0-alpha distance-streamed AI populations

`ARPGAISpawner` now treats NPC population as a streamed gameplay resource instead of leaving every spawned AI alive forever:

- distance population streaming is enabled by default; unloaded spawners wait until a player enters the exposed activation radius before spawning their group;
- a larger despawn radius plus delay creates hysteresis so players near the edge do not cause rapid spawn/despawn cycling;
- active patrol/free-roam groups remain loaded when a player is close to one of the spawned NPCs, even if that NPC travelled far away from the spawner;
- unloading is not death and cannot grant loot/XP/quest credit or fire group-defeated events;
- inactive spawners stop their ordinary leash/cohesion work and keep only a staggered low-frequency server relevance timer;
- pending individual/whole-group respawn cooldowns survive distance unloads, and Never-respawn populations stay permanently defeated;
- Blueprint exposes population state, nearest-player distance, force/evaluate helpers and activation/deactivation events.

See `Docs/AI_SPAWNER_PERFORMANCE.md`.

## 1.9.1-alpha host-synchronized day/night world time

This revision adds a first-class `ARPGDayNightCycle` world actor designed to work out of the box:

- `Host System Clock` is the default source and uses the authority machine's local PC time, including its time-zone/DST behavior;
- clients receive the authority clock and extrapolate smoothly between replicated clock samples so multiplayer worlds share one time of day;
- the placed actor includes a Sun, Moon, Sky Atmosphere, real-time-capture Sky Light and Exponential Height Fog, while external lighting actors can be assigned instead;
- sun/moon rotation, intensity, sky fill and fog transition continuously through the 24-hour clock;
- Dawn / Day / Dusk / Night phase boundaries are designer-exposed and separate from visual brightness;
- global Blueprint-pure green nodes provide `Is Day`, `Is Night`, `Get World Hour`, `Get World Date Time`, `Get Day Night Phase` and `Get Daylight Amount`;
- phase/hour delegates provide dawn, day, dusk, night and hourly gameplay events;
- Fixed Time and accelerated Simulated Clock modes are included for PIE/testing without changing the host PC clock.

See `Docs/DAY_NIGHT.md`.

## 1.8.0-alpha disposition reset + coordinated group combat

This revision completes the retaliation lifecycle and upgrades multi-NPC melee encounters:

- temporary neutral/missing-faction retaliation is cleared immediately when the aggressor dies, so a respawned player is evaluated from the NPC's original faction/fallback settings instead of inheriting stale hostility;
- stale temporary threat against dead targets is cleared with the aggression memory by default;
- genuine authored hostile factions remain genuine hostiles and can reacquire a respawned player normally;
- allied melee AI attacking the same target share engagement openings, with three simultaneous active attackers by default;
- waiting attackers spread around an outer combat ring, stay focused/facing the target, orbit/reposition through NavMesh, and rotate into openings after committed attacks or yielded/unreachable slots;
- attack and waiting positions are projected to Navigation and move goals are throttled so the system does not restart a path every AI think;
- Blueprint runtime state exposes each NPC's combat role, group size, slot index and whether it currently owns a melee attack opening.

See `Docs/GROUP_COMBAT.md` and `Docs/AI_AGGRO_ASSIST.md`.

## 1.7.0-alpha automatic aggro / retaliation / ally assist

This revision makes `ARPGAICharacter` reliably defend itself and help nearby allies even when faction authoring is neutral, incomplete, or temporarily missing:

- received combat hits automatically create temporary aggression and an immediate retaliation target;
- neutral-faction and missing-faction retaliation are enabled by default, while friendly retaliation remains opt-in;
- nearby allies can assist through same faction, allied faction, same spawner group, explicit Assist Group Id, or same-class fallback when faction identity is missing;
- spawner group assistance works even when physical `Stay Together` cohesion is disabled;
- faction-free `Attack Players On Sight` and `Attack Unfactioned Pawns On Sight` fallbacks are exposed for monster-style AI;
- Faction Definitions now expose a default relationship for unlisted factions, and `Attack Hostile On Sight` is consumed by automatic AI acquisition;
- temporary aggression is honored by combat damage legality so retaliation still works when neutral damage is otherwise disabled by a combat profile.

See `Docs/AI_AGGRO_ASSIST.md`.

## 1.6.0-alpha combat-feel polish

This revision turns the player lock-on into a stronger classic Z-target combat mode and adds a unified feedback/stagger layer:

- while locked, the player character continuously faces the selected target and the owning camera/control rotation smoothly tracks it;
- Spring Arm/Camera `Use Pawn Control Rotation` is enabled automatically for the lock and restored on unlock;
- player movement-facing flags are temporarily adjusted so strafing/orbit movement does not rotate away from the target;
- Class Definition combat profiles now expose Niagara-first hit/critical/block/parry/stagger FX with Cascade fallback;
- combat audio is exposed for attack, impact, defence, dodge, stagger, death and revive cues;
- critical hits can trigger a configurable one-hit stagger with a montage/Hit React fallback and real knockback;
- automatic NPC combat pauses navigation while staggered.

See `Docs/COMBAT_FEEL.md`.

## 1.5.0-alpha polished AI routes, groups and free roam

**UE 5.8.1 compatibility retained:** the v1.4.1 `Navigation/PathFollowingComponent.h` spline compile correction remains in this build.

`ARPGAICharacter` now inherits an **AI Spline Movement** component. Place an `ARPG AI Spline Route`, edit its spline, assign that route to the NPC (or to an `ARPG_AI_Spawner`), and the NPC follows it automatically through NavMesh movement. The pawn is never attached to the spline. The route now owns a default-on **Loop Route** setting, and spawner groups can synchronize travel direction so they do not split at open-route endpoints. `ARPGAICharacter` also contains a disabled-by-default **AI Wanderer** that the spawner enables automatically for first-class Free Roam.

- native editable route actor + inherited NPC route component
- NavMesh projection + normal `AAIController` path following
- Ping-Pong, Loop and Once modes
- nearest/first/last/explicit/random start positions
- point waits, random waits and point-id events
- lateral/random lane offsets for shared routes
- automatic stalled-move recovery
- reusable Route Id lookup for Blueprint-spawned AI
- automatic spawner route assignment
- automatic combat break-off, route-local chase leash and post-combat rejoin
- Wanderer/spawner-leash conflict prevention

See `Docs/AI_SPLINE.md` for the complete editor workflow.

## 1.3.0-alpha automatic NPC ragdoll death

NPC death is now automatic and physics-driven by default on `ARPGAICharacter`:

- a dead NPC attempts full-body skeletal ragdoll automatically; no Blueprint death physics graph is required.
- movement velocity carries into the ragdoll and the final hit can add a configurable physical impulse at the nearest Physics Asset body.
- capsule collision is disabled while the corpse is physical, while the mesh uses an exposed `Ragdoll` collision profile.
- if the skeletal mesh has no usable Physics Asset or physics cannot start, the existing Class Definition/Combat death montage plays automatically as the fallback.
- same-actor respawn cleanly restores mesh/capsule collision, attachment/relative transform and animation control.
- the base player `ARPGCharacter` remains animation-first by default; player ragdoll is an exposed opt-in on the Combat Component.
- spawner corpse lifetime/respawn behavior remains compatible with ragdolled NPC bodies.

See `Docs/RAGDOLL_DEATH.md` for setup and tuning.

## 1.2.1-alpha lock-on targeting

This revision adds a first-class, automatic player lock-on system designed for action-RPG/MMO combat:

- `ARPGTargetingComponent` is preinstalled on `ARPGCharacter`; the normal player Blueprint only needs to bind a button to `Toggle Lock On`.
- camera-centered target acquisition scores hostile candidates by angle and distance, with configurable range, field of view and line-of-sight rules.
- `Target Left` / `Target Right` switch between nearby valid targets with optional wrap-around.
- the selected target is mirrored into `ARPGCombatComponent::CombatTarget`, so basic attacks automatically use the lock-on target and the server validates the target.
- player rotation automatically faces the locked target during basic attacks and Gameplay Ability activation, with smooth rotation speed/duration exposed in the Targeting Component. Continuous face-target mode is optional.
- any GAS ability can benefit from automatic facing. The new optional `ARPGGameplayAbility` base adds `Ignore / Prefer / Require Lock-On` targeting policy plus Blueprint nodes for locked actor, target location and `GameplayAbilityTargetData`.
- a native screen-space target marker is created automatically on the selected enemy. Texture/material, color/tint, size, height/socket, pulse and acquire/release animation timing are exposed in the player Character Blueprint.
- no UI asset is required for first use: the native widget supplies a fallback reticle. Projects can subclass `ARPGTargetMarkerWidget` for custom visuals/Widget Animations while retaining the automatic spawn/routing path.
- faction hostility, dead-target invalidation, maximum range, line-of-sight grace, automatic unlock and optional reacquisition are built in.

See `Docs/TARGETING.md` for setup and ability integration.

## 1.1.0-alpha combat overhaul

This revision promotes combat from basic death/respawn plumbing into an automatic action-RPG layer:

- `Perform Basic Attack` uses the assigned Class Definition and its ordered melee/ranged/magic montage arrays.
- ordered melee montages work as a combo automatically; optional detailed combo steps expose damage, timing, range, trace radius, costs and block/parry flags.
- automatic timed melee sphere traces, ranged hitscan or optional replicated `ARPGCombatProjectile`, and magic basic attacks; a built-in `ARPG Combat Impact` Anim Notify provides exact montage-authored impact timing when desired.
- class-driven dodge with four directional montages, stamina/cooldown, movement or root-motion mode and configurable invulnerability window.
- hold-to-block shield/guard logic with front arc, physical/ranged/magic reductions, stamina damage, perfect-block/parry window and guard break.
- `AARPGCharacter` exposes direct input helpers: `Basic Attack`, `Dodge`, `Block Pressed`, `Block Released`.
- new `ARPGAICombatComponent` and ready `ARPGAICharacter`: assign Class Definition + Faction, place on NavMesh, and hostile NPC combat can run without a Behavior Tree.
- NPCs acquire hostile faction targets, use threat, chase, face, attack, react to incoming attack timing, dodge/block, and can auto-activate configured GAS ability tags.
- combat kill credit routes to quests/Slayer/XP/loot, and AI spawners now understand dead combat actors for corpse cleanup/respawn.
- Player faction defaults are now applied only to player-controlled characters, avoiding accidental Player-faction NPCs.

See `Docs/COMBAT.md` for the complete editor workflow.

## 1.0.2-alpha compile-fix notes

This source revision incorporates fixes from a real UE 5.8.1 / Visual Studio 2022 Development Editor build log:

- corrected GAS `UAttributeSet` replication registration and RepNotify definitions
- fixed battle-pet cooldown variable scope/shadowing
- added the complete `UCharacterMovementComponent` include used by combat death/respawn logic
- added the complete `APlayerState` include used by unified chat routing
- removed `APawn::Controller` shadow-variable warnings in the mount implementation
- retains the 1.0.1 fix that removes invalid `GameplayTags` / `GameplayTasks` plugin-descriptor references

A fresh local UE 5.8.1 build is still the authoritative validation step after installing this revision.

## Major systems included

- Ready-made `AARPGCharacter` with the main RPG components already attached.
- Gameplay Ability System bridge and attribute set foundations.
- Automatic melee/ranged/magic basic combat, ordered montage combos, hit traces/projectiles, crit/armor, dodge, shield block/parry/guard-break, death and respawn.
- Z-target-style toggle lock-on targeting with automatic camera tracking, continuous character facing, animated marker UI, target switching and GAS target-data helpers.
- Character progression, classes, abilities/effects and animation-set hooks.
- Inventory, equipment, item definitions, loot and currencies.
- Quests with objective-level persistence, prerequisites, chains, rewards, repeatables, auto-complete and quest-giver helpers.
- Generic skill progression with configurable curves/unlocks.
- First reference skill: **Slayer**, including Slayer masters, weighted assignments, kill credit, XP, points, streaks and persistent tasks.
- Factions and reputation for players, NPCs, wanderers, bosses and buildings.
- Vendors with stock, restock, restrictions, reputation pricing, selling, buyback and Blueprint service hooks.
- Advanced AI group spawner with weighted entries, exact/ranged counts, NavMesh projection, respawn modes and optional home/leash behavior.
- Optional autonomous wanderer AI foundation, automatic hostile NPC combat AI, and first-class NavMesh spline patrol/travel routes with combat rejoin.
- Threat/aggro utilities, automatic hostile target acquisition and loot-table components.
- Rare/world/dungeon/raid boss foundation with encounter state, phases, enrage, scaling, leash/reset, contribution and world-boss respawn.
- Dungeon/raid encounter manager with encounter states, wipe handling, checkpoints and persistent completion state.
- Battle-pet collection, teams, XP, capture and turn-battle foundation.
- Unified WoW-style chat/event message model for player, NPC, boss, system, quest, loot and event traffic.
- Local username/password profile accounts with salted password verifiers and per-account character IDs.
- Direct listen-server hosting and direct LAN/Internet join-by-IP helpers.
- Character/world save system with automatic character persistence and persistent player-built world actors.
- Palworld-style modular building foundation: placement validation, grid snapping, support/collision checks, material costs, repair/demolish and faction ownership.
- Player faction inheritance for player-built structures.
- Faction territory volumes and building access/damage permissions.
- Storage/chests backed by the same inventory model as characters.
- Crafting/production stations with recipe queues, station or player inputs, tagged fuel, output inventory and offline elapsed processing.
- Mount collection, unlock, summon/ride and persistent mount state.
- Party/raid group membership foundation and channel-aware social state.

## Installation

1. Close Unreal Editor.
2. Copy the complete `AkumasRPGFramework` folder into:

   `YourProject/Plugins/AkumasRPGFramework`

3. Confirm the built-in **Gameplay Abilities** and **Niagara** plugins are enabled. `GameplayTags` and `GameplayTasks` are module dependencies handled by the plugin Build.cs; they are not separate `.uplugin` dependencies.
4. Register the framework's Primary Data Asset types in **Project Settings > Game > Asset Manager**. See `Docs/ASSET_MANAGER.md`.
5. Right-click the `.uproject` and regenerate Visual Studio project files if your workflow requires it.
6. Build the project's **Development Editor** target for Win64 using your UE 5.8 toolchain.
7. Open the project and enable **Akuma's RPG Framework** if it is not already enabled.
8. For the fastest start, derive your player Blueprint from `ARPGCharacter` and your GameMode from `ARPGGameMode`.

See **`Docs/QUICK_START.md`** for the editor workflow.

## Core architecture

The framework deliberately separates three things:

1. **Definitions** — `UPrimaryDataAsset` subclasses define items, quests, classes, factions, skills, pets, bosses, recipes, vendors, mounts, build pieces and dungeons.
   Create these as **Miscellaneous > Data Asset** instances of the native `ARPG...Definition` class. Framework definition classes are intentionally `NotBlueprintable` in 1.1 to prevent accidentally creating a Blueprint Class where a Data Asset instance is required.
2. **Runtime state** — replicated actor components hold mutable game state such as HP, XP, inventory stacks, active quests, reputation, Slayer tasks and pet teams.
3. **Persistence** — SaveGame records store stable IDs plus mutable values rather than transient UObject pointers.

That separation is important for long-lived RPG projects: content definitions can change while character/world saves remain based on stable definition IDs.

## Ready character

`AARPGCharacter` includes:

- Ability System
- Stats
- Combat
- Progression
- Inventory
- Equipment
- Currencies
- Quests
- Skills
- Slayer
- Faction
- Battle Pets
- Battle-Pet Battle
- Class
- Ability Bridge
- Event Router
- Persistence
- Interaction
- Building
- Mounts
- Group
- Threat
- AI Combat
- Targeting

You can use that class directly as the base of a Blueprint, or attach individual components to your own character architecture.

## Automatic event routing

`UARPGEventRouterComponent` is the central cross-system bridge. It can route events such as:

- kill
- item looted
- pet captured
- pet battle won
- dungeon completed
- raid boss defeated
- item crafted
- structure built
- skill level reached
- mount unlocked
- faction reputation changed

Those events feed quest objectives and related RPG progression without requiring a separate web of Blueprint counters for each feature.

## Saving and profiles

`UARPGPersistenceComponent` and `UARPGSaveSubsystem` persist character state including identity, transform, class, faction, vitals, level/XP, inventory/equipment state, quests/objectives, skills, Slayer state, reputation, currencies, battle pets, group state and mounts.

The world save layer persists runtime player-built structures, ownership/faction data, structure health/upgrades, chest inventories, station output, craft queues and dungeon encounter progress.

The local account system remembers the most recently used character ID so single-player login can resolve the correct character save with minimal Blueprint setup.

## Multiplayer authority model

Mutating gameplay actions are designed to happen on the authoritative server. Player-to-world interactions that would otherwise target unowned world actors—vendors, storage, crafting stations, quest givers and Slayer masters—are routed through the player's replicated `ARPGInteractionComponent`.

Direct IP hosting/joining is intentionally simple. Internet hosting can still require router/firewall/NAT configuration. Steam/EOS or a custom session/backend provider can be layered above the networking helpers.

## Security boundary

The built-in username/password implementation is for **local profiles / trusted local single-player usage**. It stores a salted verifier and does not write the raw password to the RPG save.

It is **not** a substitute for a production Internet authentication service. A public multiplayer game should authenticate identities using a real server/backend or platform identity provider and only pass trusted account identity into the RPG framework.

## Content not included

This is a source framework, not an art/content pack. It does not include your meshes, animations, UI art, abilities, maps, building meshes, mount meshes or backend service credentials. The C++ layer exposes the hooks/Data Assets for those project-specific assets.

## Important first-build boundaries

The package contains functional foundations for all major requested areas, but "AAA" quality ultimately requires project-specific content, UE compilation, network testing, save migration QA, performance profiling, security review and gameplay balancing. In particular:

- Battle-pet combat is a reusable turn/capture foundation, not a clone of every live WoW pet rule.
- Wanderers provide autonomous world-population behavior/hooks; a full SPPNXT-scale playerbot brain still requires game-specific goals, nav data and content policies.
- Dungeon/raid state and encounter orchestration are present; cloud matchmaking, globally persistent lockout databases and distributed instance servers are external online-service concerns.
- Direct IP networking does not perform NAT traversal.
- UMG screens are intentionally project-facing; the framework exposes events/data needed to build your desired UI style.

## Documentation

- `Docs/QUICK_START.md` — fastest editor setup.
- `Docs/WOODCUTTING.md` — Woodcutting progression, axes/tools, harvestable tree Blueprints, falling trunks, drops and building-resource integration.
- `Docs/QUICK_ACCESS.md` — active hotbar slots, weapon/tool switching, consumables, input wiring, UMG data and persistence/network behavior.
- `Docs/COMBAT.md` — automatic player/NPC combat, combos, dodge, block/parry and ranged/magic setup.
- `Docs/AI_SPLINE.md` — automatic NavMesh spline patrol/travel, route looping, synchronized group direction, route points, spawner assignment and combat rejoin.
- `Docs/AI_SPAWNER_MOVEMENT.md` — spawn groups versus cohesion, movement modes, independent/group free roam and spawner-centered wandering.
- `Docs/RAGDOLL_DEATH.md` — automatic NPC ragdoll, Physics Asset fallback behavior and death/respawn tuning.
- `Docs/FEATURE_MATRIX.md` — what each system currently provides.
- `Docs/ASSET_MANAGER.md` — required Primary Asset registration.
- `Docs/BUILDING_CRAFTING.md` — building, faction ownership, storage and furnaces.
- `Docs/NETWORK_AND_AUTH.md` — login, saves and direct-IP networking boundaries.
- `Docs/VALIDATION.md` — validation performed on this source package.


