# 2.4.2-alpha — AI dodge authoring + stagger escape

- Added `AI Dodge Chance` directly to `Combat -> Dodge`, so NPC dodge probability is visible alongside Direction Montages, Duration, Distance and invulnerability settings instead of being hidden on the separate AICombat component.
- Retained the existing AICombat `DodgeChance` property unchanged for serialized/Blueprint compatibility; customized legacy values remain a fallback while the new profile chance is left at its default.
- Added `Dodge Cancels Stagger` (enabled by default). A valid dodge may now start while staggered, atomically clears the stagger timer/tag/state, broadcasts stagger-end, clears residual stagger knockback velocity, then begins the dodge.
- Automatic AI defence can therefore dodge out of stagger when a new incoming attack creates a valid reaction opportunity.
- Improved reaction scheduling for fast attacks so `ThinkInterval` cannot make an otherwise valid dodge window disappear between AI defence ticks.
- Preserved v2.4.1 navigation-abort and code-driven lateral displacement behavior.
- Public reflected changes are intentionally limited to the Dodge settings struct; no Quick Access, Inventory, Equipment, Spawner, Character or Day/Night API was changed.

# 2.4.1-alpha — AI dodge movement reliability

- Fixed automatic NPC dodges being selected and animated while active AI path following could immediately fight the dodge displacement, making left/right evasion appear stationary.
- `PerformDodgeAuthority` now aborts an AI controller's active path before dodge presentation/movement begins.
- Code-driven dodges clear current CharacterMovement velocity and apply a clean lateral launch derived from the existing authored `Distance / Duration` settings.
- `ARPGAICombatComponent` resets cached combat MoveTo state when committing/holding a dodge, preventing stale path state from blocking pursuit after the dodge finishes.
- Root-motion-only dodge behavior remains opt-in and unchanged; navigation is still suspended so root motion can move the NPC without path-following interference.
- No Blueprint-facing public header/schema changes versus v2.4.0-alpha.
- Added `Tools/test_ai_dodge_movement_model.py` regression coverage.

# 2.4.0-alpha — Day/night AI population swapping

- Added opt-in `Enable Midnight Population Swap` to `ARPGAISpawner`; disabled spawners retain the pre-2.4 preserved population lifecycle unchanged.
- The existing weighted `Spawn Table` is the daylight/default population. Added a weighted `Midnight Spawn Table` used from 00:00 until `ARPGDayNightCycle::DayStartHour`.
- At midnight, loaded spawners cleanly remove daylight leftovers and immediately spawn midnight entries. At morning/day start, midnight leftovers are removed and daylight entries return.
- Phase cleanup removes `OnDestroyed` bindings before destroying phase NPCs, so a time-of-day swap cannot be mistaken for combat death, group defeat, or schedule an obsolete respawn.
- Respawn timers, pending whole-group state, preserved immediate counts and desired group size are reset atomically on a population-phase transition.
- Distance-streamed spawners update their phase while unloaded without spawning; the next relevance activation uses only the correct current phase table instead of restoring the old phase.
- Added optional `Day Night Cycle Override` and automatic first-cycle discovery, plus separate midnight Min/Max group-size authoring.
- Added event-driven hour/day bindings with recurring spawner checks as a safety net, so simulated/fixed clock jumps that skip directly across midnight still synchronize correctly.
- Added `Refresh Day Night Population Now` and `Is Midnight Population Active` Blueprint utilities for testing/diagnostics.
- Added `Tools/test_day_night_spawner_model.py` regression coverage and dedicated setup documentation.

# 2.3.0-alpha — Melee combat polish

- Fixed ordinary Hit React montages interrupting an already-running attack montage while the authoritative attack timer continued, which made player combos look cancelled even though damage still resolved. Light hits now preserve an active attack; true stagger/guard-break/parry/death remain legitimate interruption states.
- Critical stagger now stops AI path following immediately, zeroes character movement before launch, applies the full configured launch vector, and emits the Stagger cue from the successful stagger path itself.
- Automatic melee AI no longer submits Basic Attack again while already attacking, preventing the Think loop from auto-buffering endless combo attacks.
- `Attack Slot Cooldown After Attack` now gates solo melee AI as well as coordinated groups, and is measured after estimated attack recovery for readable spacing between attacks.
- No Blueprint-facing public header/schema changes in this release.

# Changelog

## 2.2.3-alpha.2 — 2026-08-11

- Blueprint-compatibility hotfix based directly on the known-compiling 2.2.3-alpha.1 public API/schema.
- Reverted the 2.2.4 reflected `UARPGQuickAccessComponent` property addition so existing Blueprint casts, component references, and serialized nodes do not see a changed native reflection schema.
- Active-slot replacement auto-equip is implemented privately in `ARPGQuickAccessComponent.cpp`: dropping/assigning a different Equip-action weapon/tool into the currently active Quick Access slot immediately performs the existing exclusive equipment handoff.
- Consumables are never auto-used by assignment.
- No public header, UFUNCTION, UPROPERTY, USTRUCT, or delegate signature changes versus 2.2.3-alpha.1.

## 2.2.3-alpha.1 — 2026-08-11

- Fixed UE 5.8 C++ compilation in `UARPGQuickAccessComponent::ClearSlotAuthority`: the non-canonical-slot guard now returns `false` from the `bool` function instead of `EARPGQuickAccessResult::EmptySlot`.
- No Quick Access gameplay or Blueprint API behavior was changed by this compile-only correction.

## 2.2.3-alpha — 2026-08-11

- Fixed Quick Access weapon/tool activation leaving the previously active held item equipped when the new item used a different logical `EquipmentSlot` tag (for example Tool vs Weapon), which could render both meshes in the same hand.
- Quick Access now tracks the last item it activated as equipment and explicitly unequips that runtime instance before equipping a different Quick Access weapon/tool.
- Replacing or clearing the currently active hotbar slot preserves the previous held-item runtime GUID long enough for the next activation to perform a clean handoff, so drag-replacing an active Axe with a Sword cannot orphan the Axe in-hand.
- Added `Exclusive Active Quick Access Equipment` (enabled by default) to keep Quick Access as one active held-item channel without changing the general Equipment system's support for independent armor/offhand slots.
- Consumable activation intentionally does not unequip the currently held weapon/tool.
- Added v2.2.3 source validation for the exclusive active-equipment handoff path.

## 2.2.2-alpha — 2026-08-11

- Reworked Quick Access duplicate handling around a deterministic per-slot `AssignmentRevision`. The most recently assigned target slot now wins duplicate repair instead of repair depending on array iteration order.
- Quick Access assignment is now treated as an atomic move: all prior claims for the runtime `InstanceId` (and same `ItemId` when duplicate item types are disabled) are cleared before the target is written.
- `Get Slot View`, item lookup helpers and availability queries now defensively suppress non-canonical duplicate slots even if legacy, replicated or corrupted state reaches the owner UI.
- Save/load repair is revision-aware and backward-compatible: older slots default to revision 0 and are normalized deterministically.
- Added v2.2.2 regression validation for assignment revisions, target-preferred repair, and defensive UI projection.

## 2.2.1-alpha — 2026-08-11

- Fixed Quick Access allowing the exact same runtime Inventory `InstanceId` to remain assigned to multiple hotbar slots in configurations where same-item-type duplicates were enabled/serialized.
- The exact runtime inventory GUID is now **always unique across Quick Access slots**. Reassigning an owned item instance clears every previous slot holding that same instance before the target slot is written, so inventory-to-hotbar drag/drop behaves as a true move.
- Clarified `Allow Same Item Type In Multiple Slots`: when enabled it permits only different owned runtime instances/stacks of the same `ItemId`; it never permits cloning one runtime instance across multiple slots.
- Hardened runtime/save repair so duplicate or stale bindings select only unclaimed positive-quantity owned instances. Existing duplicate v2.2.0 state is repaired automatically on BeginPlay/load/inventory refresh.
- Fallback `ItemId` rebinding no longer steals an instance already claimed by another Quick Access slot.
- Added Blueprint-pure `Find Slot For Item Instance` for exact runtime-GUID diagnostics and UI workflows.
- Active-slot notifications now refresh correctly when the contents of the already-active target slot change during reassignment.
- Added v2.2.1 source-regression validation for strict runtime-instance uniqueness and package version integrity.

## 2.2.0-alpha — 2026-08-10

- Added inherited `ARPGQuickAccessComponent` to the ready `ARPGCharacter`, providing persistent numbered active-item slots for weapons, gathering tools, food, potions and project-specific items.
- Added one-button `Activate Slot`: `Quick Access Action = Auto` equips equippable items through the existing Equipment component, uses `bUsable` items immediately, and falls back to selection-only for other items.
- Added 1-based Blueprint input wrappers on `ARPGCharacter`: `Quick Access Pressed`, `Quick Access Next`, `Quick Access Previous` and `Use Active Quick Access Item`, so keyboard number keys and gamepad D-pad/wheel controls do not need hand-written Equip/SetEquipped graphs.
- Quick-access assignments retain both stable ItemId and the exact owned runtime inventory instance GUID. Runtime activation never treats an Item Definition Data Asset as ownership.
- Added authoritative hotbar rebinding when a stack instance disappears: a slot may reconnect to another positive-quantity owned runtime stack of the same ItemId; if none exists the assignment remains as an unavailable bookmark and cannot be activated.
- Quick Access slot layout and active slot are SaveGame-backed and owner-only replicated; remote players continue to receive only the normal equipment/inventory presentation state they actually need.
- Added optional duplicate-assignment prevention, slot swapping/clearing, next/previous cycling, active-slot events, action-result events, item-used events, quantity/availability helpers and a UMG-ready `Quick Access Slot View`.
- Added generic Item Definition use metadata for food/potions: `Usable`, consume-on-use/quantity, cooldown, Health/Mana/Stamina restoration, optional GAS Gameplay Effect and use montage/sound.
- Consumables are server authoritative, reject unavailable/insufficient/full-vitals-no-effect uses without consuming an item, apply cooldown per item type, and consume the exact active runtime stack only after the use path succeeds.
- Added `Restore Mana` to the Stats component so generic consumables can restore all three framework vitals without custom Blueprint mutation.
- Starting Inventory entries now expose optional `Quick Access Slot (0 = None)`. An auto-equipped starter assigned to a quick slot also becomes the initial active hotbar slot.
- Character saves now persist Quick Access slots and active slot after Inventory, allowing load-time runtime-instance repair against the restored inventory. Save version advanced to 3.
- Added `Docs/QUICK_ACCESS.md` and v2.2 source-regression validation.

## 2.1.1-alpha — 2026-08-10

- Fixed v2.1 equipment/Woodcutting runtime-definition desynchronization by storing the exact Item Definition soft reference inside each runtime `FARPGInventoryEntry` alongside its stable ItemId.
- `Add Item Definition`, Starting Items, tree rewards and future saves now preserve the exact authored Data Asset instead of requiring Equipment/Woodcutting to rediscover it later through Asset Manager.
- Older ID-only inventory saves remain supported: runtime entries backfill from Starting Items and the existing stable-ID resolver when possible.
- Item `DefinitionId` is now optional for inventory authoring; when blank, the Data Asset name (for example `DA_StoneAxe`) becomes the stable runtime ItemId automatically.
- Tightened equipped-tool validation: an axe only counts when a real runtime entry has a valid instance GUID, positive quantity, `bEquipped=true`, a valid slot, and that slot matches the resolved equippable Item Definition. Merely having an axe Data Asset in the project can never satisfy Woodcutting.
- Equipment effects, held visuals, combat swing audio and Woodcutting now resolve from the exact runtime inventory entry first.
- Tagged crafting/fuel checks now resolve Item Definitions from the runtime entry as well, keeping newly-authored project items consistent outside Equipment/Woodcutting.
- Equip/unequip presentation RPCs now carry the exact Item Definition asset instead of only an ItemId, removing another late-resolution failure path.
- Woodcutting swing and tree-hit presentation now receive the exact equipped tool selected by authority, so gathering swing/hit audio no longer depends on client-side definition rediscovery timing.
- Equipment visuals now auto-fallback through common right-hand socket/bone names when the configured socket is missing, with a runtime warning instead of silently placing the weapon at an unusable location.
- Added Blueprint inventory diagnostics for resolving an item definition by instance and checking runtime equipped state.
- Added v2.1.1 regression validation covering exact runtime item references, strict equipped-tool gates, exact-tool audio presentation and descriptor/version integrity.

## 2.1.0-alpha — 2026-08-10

- Added editable `Starting Items` to `ARPGInventoryComponent`, using Item Definition asset pickers, quantity and optional Equip On Spawn; retained `Runtime Items` as protected replicated/save state.
- Added authority `Apply Starting Items` and default-on begin-play seeding with empty-inventory protection; automatic seeding is deferred past the persistence auto-load pass so existing saves take precedence cleanly.
- Added native Blueprintable `ARPGEquipmentVisualActor` with static/skeletal mesh support and no collision/tick/replication by default.
- Item Definitions now expose Equipped Visual Actor Class, Equipped Static Mesh, Equipped Skeletal Mesh and Equipped Relative Transform in addition to the existing attach socket.
- Equipment automatically creates/destroys held visuals from replicated equipped inventory state and exposes refresh/query/visual-change Blueprint hooks.
- Existing Equip/Unequip Montage fields are now consumed by the runtime Equipment component.
- Added per-item Equip, Unequip, Combat Swing, Gathering Swing and Gathering Hit sounds with volume/pitch tuning.
- Ordinary melee prefers an equipped item's Combat Swing Sound when configured; class/profile melee audio remains the fallback.
- Woodcutting automatically uses Gathering Swing Sound (Combat Swing fallback), and successful tree chops can play the equipped axe/tool Gathering Hit Sound alongside tree feedback.
- Equipment replacement in the same slot now presents the displaced item's unequip cue before the new item's equip cue.
- Added `Docs/EQUIPMENT_INVENTORY.md` and v2.1 source-regression validation.

## 2.0.2-alpha — 2026-08-10

- Added automatic Basic Attack -> Woodcutting integration: when no real combat target is supplied, an equipped axe and an `ARPGTree` in the Woodcutting view trace redirect the normal Basic Attack input into one authoritative chop.
- Real combat/lock-on targets keep priority, so holding an axe does not steal attacks away from enemies.
- The separate `Start Woodcutting From View` interaction remains unchanged and continues to support automatic repeated chopping.
- Basic-attack chopping requires an equipped Woodcutting tool by default, respects each tree's level/tool-tier gates, and respects the normal Woodcutting swing interval so input spam cannot harvest faster than the authored cadence.
- Added `Has Equipped Woodcutting Tool` and `Try Chop Tree With Basic Attack` Blueprint helpers.
- If no dedicated chop montage is assigned, Woodcutting can now fall back to the character's normal melee combat montage automatically.
- Added replicated per-instance tree size variation with exposed `Minimum Mesh Scale` / `Maximum Mesh Scale`, randomize/reroll toggles and runtime selected scale.
- Tree scale multiplies the existing authored visual scale and applies consistently to both the falling trunk and stump.
- Added `Get Selected Tree Mesh Scale`, `Select Random Tree Mesh Scale` and `Set Tree Mesh Scale` Blueprint APIs.
- Retained the v2.0.1 UE5.8.1 `TObjectPtr` compile fix and Day/Night network-frequency deprecation fix.

## 2.0.1-alpha — 2026-08-10

- Fixed the real UE5.8.1 `ARPGTree.cpp` C2445 compile error in `GetSelectedTreeMesh()` caused by a conditional expression mixing `UStaticMesh*` with `TObjectPtr<UStaticMesh>`.
- Replaced the mixed ternary with an explicit validity branch returning `TreeMeshes[SelectedTreeMeshIndex].Get()` or the TreeMesh component's raw static-mesh pointer fallback.
- Added validator regression coverage that rejects the exact mixed `TObjectPtr`/raw-pointer tree-mesh ternary pattern.
- Replaced the deprecated direct `NetUpdateFrequency` member write in `ARPGDayNightCycle` with `SetNetUpdateFrequency(2.f)` as requested by the UE5.8.1 compiler warning.
- No Woodcutting, tree-fall, reward, skill, AI, day/night, combat, targeting, save, or networking behavior was removed.

## 2.0.0-alpha — 2026-08-10

- Added inherited `ARPGWoodcuttingComponent` to the ready player character.
- Added automatic `Start Woodcutting From View` targeting plus direct Start/Stop/Chop Once APIs.
- Added persistent Woodcutting progression through the existing Skill Component, with level/XP/progress/unlock pure nodes and optional Skill Definition curve/unlocks.
- Added gathering metadata to Item Definitions: tool tags, gathering power and gathering tool tier.
- Added native Blueprintable `ARPGTree` with tree-mesh variation arrays, replicated variation selection, stump support, chop health/resistance, required Woodcutting level and optional axe/tool-tier gates.
- Added server-authoritative repeated chop timing with exposed swing interval and impact delay plus optional chop montage.
- Added direct Wood Item picker, min/max wood rewards, bonus-drop array, inventory-capacity preflight and automatic item-looted quest routing.
- Added smooth replicated tree fall away from the harvester, fallen-trunk hold, stump state and timed tree respawn.
- Added Niagara/Cascade chop/fell feedback and exposed sounds.
- Added tree Blueprint calls/events for custom harvesting extensions.
- Added Woodcutting gameplay tags and source-regression validation.

## 1.10.0-alpha — 2026-08-10

- Added first-class distance-based AI population streaming directly to `ARPGAISpawner`, enabled by default for performance.
- Unloaded spawners automatically create their NPC population when a player-controlled pawn enters the exposed Spawn Activation Radius.
- Loaded populations automatically despawn after all players remain beyond the larger Despawn Radius for the exposed delay, providing hysteresis and preventing boundary pop/thrash.
- Active spline/free-roam populations can remain relevant while a player is near any spawned NPC, preventing long-route NPCs from disappearing beside the player merely because they travelled away from their spawner origin.
- Distance unload is a clean streaming despawn: it removes destruction callbacks first, so no kill credit, loot, quest kills, group-defeated events or false respawn scheduling are generated.
- Inactive spawners stop leash/group-cohesion timers; only a low-frequency server relevance timer remains, with randomized first-delay staggering across many spawners.
- Added exposed optional combat retention, 2D/3D relevance distance, activation/despawn radii, unload delay, check interval and group-size re-roll policy.
- Distance reload preserves desired group size by default and preserves real pending respawn cooldowns; members that were alive can return immediately while still-dead members wait out their remaining delay.
- `Respawn Mode = Never` remains permanent across unload/reload and cannot be bypassed by leaving/re-entering the area.
- Added Blueprint runtime status/controls: `Is Population Active`, `Get Nearest Relevant Player Distance`, `Is Player Inside Spawn Activation Radius`, `Evaluate Population Relevance Now`, `Set Population Active`, plus population activated/deactivated events.
- Added `Docs/AI_SPAWNER_PERFORMANCE.md` and validator coverage for distance population streaming invariants.

## 1.9.1-alpha — 2026-08-10

- Fixed the UE 5.8.1 Day/Night compile failure caused by the invalid `Engine/SkyAtmosphere.h` include.
- `ASkyAtmosphere` and `USkyAtmosphereComponent` are declared by `Components/SkyAtmosphereComponent.h` in UE 5.8; the day/night implementation now relies on that correct Engine header only.
- Added validator coverage that rejects the obsolete `Engine/SkyAtmosphere.h` include and requires `Components/SkyAtmosphereComponent.h` for the day/night source.
- No Day/Night behavior was removed: host-system-clock authority, replicated world time, built-in sun/moon/sky/fog rig, pure day/night queries, and test clock modes are retained.

## 1.9.0-alpha — 2026-08-10

- Added `ARPGDayNightCycle`, a drop-in replicated day/night actor that follows the authority/host computer's local system clock by default.
- Added a built-in dynamic Sun + Moon + Sky Atmosphere + real-time Sky Light + Exponential Height Fog rig for one-actor level setup.
- Added optional external Sun/Moon/Sky Light/Sky Atmosphere/Fog actor references for projects that already own a lighting rig.
- Added smooth client clock extrapolation between replicated authority snapshots so celestial movement does not visibly step with network updates.
- Added explicit Dawn/Day/Dusk/Night phases, exposed semantic boundaries, phase/hour events and lighting-independent day/night gameplay checks.
- Added global Blueprint-pure `ARPG World Time` nodes: Is Day, Is Night, Get World Hour, Get World Date Time, Get Day Night Phase and Get Daylight Amount.
- Added Fixed Time and accelerated Simulated Clock testing modes plus a designer clock offset.
- Added smooth sun/moon rotation and exposed day/night intensity, color, skylight and fog tuning.
- Uses Sky Light Real Time Capture for dynamic environment lighting rather than repeatedly calling costly manual sky recaptures.


## 1.8.0-alpha — 2026-08-10

- Fixed temporary retaliation persistence across player death/respawn: an AI now binds to its current target's Combat LifeState and immediately forgets temporary aggression when that target leaves Alive state.
- Added `Restore Original Disposition After Target Death` (default ON) and `Clear Threat Against Dead Targets` (default ON), so passive/neutral NPCs return to their original faction/fallback behavior after killing an aggressor.
- Added Blueprint authority helpers to forget one temporary aggressor or all temporary aggression without changing authored faction relationships.
- Added first-class coordinated melee group combat so large packs no longer all path into the exact target location.
- Added a default cap of three simultaneous melee attackers per shared target; excess allied melee NPCs enter a waiting/orbit role and automatically rotate into attack openings.
- Added distributed inner attack positions plus an outer waiting ring, optional continuous orbiting, stable group spacing, NavMesh projection, path-refresh throttling and target-facing AI focus.
- Attack-slot leasing/yield/cooldown prevents one blocked or unreachable NPC from monopolizing an opening and gives waiting actors fair attack opportunities.
- Group coordination uses the existing ally model (faction, spawn group, assist group and configured fallbacks), so independent wildlife groups can coordinate without being physically attached or forced into a formation.
- Added Blueprint-readable `Group Combat Role`, group size, slot index and melee-slot state plus `On Group Combat Role Changed` for animation/UI/debug extensions.
- Added `Docs/GROUP_COMBAT.md`, expanded aggro documentation, and validator coverage for target-death cleanup and coordinated group-combat runtime paths.

## 1.7.0-alpha — 2026-08-10

- Fixed automatic NPC retaliation: received combat hits now create temporary aggression so neutral or unresolved faction attackers remain valid combat targets instead of being discarded on the next AI think.
- Added exposed AICombat retaliation controls for neutral-faction override, missing-faction fallback, friendly-attacker policy, aggression memory duration and retaliation threat bonus.
- Added optional faction-free proactive fallbacks for Attack Players On Sight and Attack Unfactioned Pawns On Sight.
- Added automatic nearby ally assistance when an NPC is attacked, with exposed assist radius/threat and protection against overriding an ally already in combat by default.
- Ally recognition now supports same faction, allied factions, explicit Assist Group Id, same ARPG AI Spawner group, and same-class fallback when faction identity is missing.
- ARPG AI Spawner now registers itself as each spawned AI's runtime Spawn Group Owner, allowing group members to help each other even with Stay Together disabled and without faction data.
- Added temporary AI aggression override to Combat CanDamageActor so retaliation still deals damage when a class profile disallows neutral damage.
- Added `Default Relationship To Unlisted Factions` to Faction Definitions for easier authoring of factions that hate/love all unlisted factions.
- Wired the existing `Attack Hostile On Sight` Faction Data Asset flag into automatic AI acquisition.
- Added Blueprint aggro/assist queries, events and explicit help-call APIs.
- Added `Docs/AI_AGGRO_ASSIST.md` and validator coverage for retaliation/assist integration.

## 1.6.0-alpha — 2026-08-10

- Upgraded player lock-on to a constant Z-target style by default: the owning camera/control rotation smoothly tracks the selected target while the player character continuously faces it.
- Added automatic Spring Arm/direct Camera `Use Pawn Control Rotation` management during lock-on, with the previous camera-rig value restored when the target is released.
- Added lock-on movement-facing override for player characters so Orient Rotation To Movement cannot pull the character away from the target; settings are cached/restored automatically.
- Targeting tick now runs after the owning Actor tick to reduce conflicts with Blueprint movement/rotation helper logic.
- Added first-class combat impact feedback settings to Class Definitions with Niagara-first hit/critical/block/parry/stagger effects and automatic Cascade `UParticleSystem` fallback.
- Added exposed combat audio for melee swing, ranged attack, magic cast, hit, critical hit, block, parry, dodge, block start/end, guard break, stagger, death and revive, with global volume and randomized pitch range.
- Added authority-side critical-hit stagger chance with one-hit stagger duration, immunity window, optional attack interruption, dedicated montage (Hit React fallback), real character knockback and physics impulse support.
- Added replicated `bIsStaggered`, `OnStaggerStateChanged`, `Combat.State.Staggered`, and Blueprint `Is Staggered`.
- Automatic NPC combat now stops path movement while staggered so navigation does not immediately override knockback/reaction presentation.
- Added lightweight multicast combat feedback cues so cosmetic particles/audio spawn on relevant clients while damage/critical/stagger decisions remain server-authoritative.
- Added Niagara runtime dependency and validator coverage for camera lock, Niagara/Cascade fallback, audio, stagger and AI knockback integration.
- Added `Docs/COMBAT_FEEL.md`.

## 1.5.0-alpha — 2026-08-09

- Added route-level `Loop Route` on `ARPGAISplineRoute`, enabled by default so normal patrol routes no longer stop permanently at an endpoint.
- Added `Closed Loop Geometry` and `Reverse At Open Ends` as separate designer concepts: closed splines wrap continuously; open looping routes reverse by default, or can return to the opposite endpoint through NavMesh.
- `ARPGAISplineComponent` uses route traversal settings by default while retaining the per-NPC Once / Loop / Ping-Pong override for advanced cases.
- Added spawner synchronized spline direction: one spawned group member acts as direction leader so an open Ping-Pong group reverses together instead of splitting and running both ways.
- Group direction synchronization is independent of physical `Stay Together` cohesion, so a loose group can still share one sensible route travel direction.
- Implemented functional `Group Cohesion` settings on `ARPGAISpawner`: Stay Together, cohesion radius/check interval, shared spline direction and forward/reverse/random-per-group route direction.
- Added first-class spawner `Movement Mode`: Automatic, Spline Route, Free Roam, or No Automatic Travel. Automatic preserves prior behavior by selecting spline travel whenever a route is assigned.
- Added spawner Free Roam settings with NavMesh reachable-point wandering, roam radius, think interval and spawner-centered leash.
- Spawn grouping is now independent from movement cohesion: groups still share desired count/defeat/respawn behavior when Stay Together is disabled, while each member can roam independently.
- When Stay Together + Free Roam are both enabled, a group leader roams around the spawner and followers use cohesion recovery before resuming random travel. Combat always has higher movement priority.
- `ARPGAICharacter` now includes a lightweight `AIWanderer` component by default, disabled until requested by free-roam behavior. Custom APawn classes can receive a runtime wanderer component from the spawner when needed.
- Hardened `ARPGWandererComponent` with combat/death awareness, home-return enforcement, timer-on-demand behavior, configurable acceptance radius and explicit home/return Blueprint APIs.

## 1.4.1-alpha — 2026-08-09

- Fixed the UE 5.8.1 AI spline compile failure by explicitly including `Navigation/PathFollowingComponent.h` in `ARPGAISplineComponent.cpp`.
- `AIController.h` only forward-declares `EPathFollowingRequestResult::Type` for this translation unit, so the concrete request-result enumerators require the Path Following header before `EPathFollowingRequestResult::Failed` can be used.
- Kept the original symbolic request-result handling and retry behavior; no magic numeric enum workaround is used.
- Added validator coverage requiring the Path Following header whenever the spline component references `EPathFollowingRequestResult` enumerators.

## 1.4.0-alpha — 2026-08-09

- Added first-class `ARPGAISplineRoute` world actor with an editable native `USplineComponent` and per-control-point wait/event settings.
- Added `ARPGAISplineComponent` as an inherited default component on every `ARPGAICharacter`; assigning a route is enough to start NavMesh-following automatically.
- Spline-following never attaches or teleports the pawn along the path: it samples look-ahead route locations, projects goals to Navigation, and uses `AAIController::MoveToLocation`.
- Added Once, Loop and Ping-Pong patrol modes plus nearest/first/last/explicit/random start modes.
- Added automatic initial route joining, configurable follow-step distance, acceptance radius, lateral lane offsets, random per-NPC offsets, navigation filters, partial-path policy, and stalled-move recovery.
- Added Route Id auto-resolution for reusable NPC Blueprint classes; when multiple routes share an id the nearest matching spline is selected.
- Added automatic combat suspension/rejoin: NPCs leave the route to fight, use the route departure point as the chase-leash anchor, then rejoin the nearest/previous route progress through NavMesh.
- Corrected AI combat home-leash behavior so it constrains active combat chases instead of breaking long non-combat patrol routes.
- Integrated spline ownership with Wanderer AI so optional roaming cannot issue competing movement requests while a route is active.
- Integrated `ARPGAISpawner` with an exposed Assigned Spline Route, automatic route start for spawned NPCs, and route-aware spawner leash suppression.
- Added per-route-point `PointId`, fixed/random wait time, facing option, and Blueprint route-point events for optional guard/emote/script hooks.
- Added runtime Blueprint APIs/events for route assignment, start/stop/pause/resume, route rejoin, route state, failure and finish notifications.

## 1.3.0-alpha — 2026-08-09

- Added automatic physical ragdoll death presentation for `ARPGAICharacter`; NPCs use ragdoll by default without Blueprint death logic.
- Added `FARPGDeathPresentationSettings` on the Combat Component with exposed ragdoll collision profile, capsule handling, inherited movement velocity, death-hit impulse and dedicated-server simulation policy.
- Death montage behavior is now a true fallback: when an NPC mesh has no usable Physics Asset or ragdoll simulation cannot start, the existing Class Definition / Combat death montage plays instead.
- Player `ARPGCharacter` behavior remains animation-first by default; player ragdoll can be enabled from the same exposed Combat -> Death settings when desired.
- Added reliable multicast death-presentation/reset paths and late-join `LifeState` recovery so ragdoll/fallback presentation is reconstructed on clients.
- Added ragdoll reset for same-actor respawns, restoring capsule collision, mesh collision profile, mesh relative transform and animation control before revive.
- Death ragdolls inherit character movement velocity and can receive a configurable velocity-change impulse at the physics body nearest the final hit location.
- Added `OnRagdollStateChanged` and `IsRagdollActive` Blueprint hooks.
- Added validator coverage requiring the ready AI character to keep ragdoll + montage fallback enabled by default.

## 1.2.1-alpha — 2026-08-09

- Fixed UE 5.8.1/MSVC `C2445` in `ARPGTargetingComponent::CreateTargetMarker` by removing the ambiguous conditional expression between `TSubclassOf<UARPGTargetMarkerWidget>` and `UClass*`.
- Target marker widget selection now copies the configured `TSubclassOf` first, then explicitly falls back to `UARPGTargetMarkerWidget::StaticClass()` only when no override is assigned.
- Added validator coverage to reject this ambiguous `TSubclassOf ? TSubclassOf : StaticClass()` pattern in future releases.
- No targeting behavior or editor settings were removed; custom marker widget subclasses still override the native fallback.

## 1.2.0-alpha — 2026-08-09

- Added `ARPGTargetingComponent` as a ready component on every `ARPGCharacter`, with player-local camera-centered hostile target acquisition.
- Added one-call `ToggleLockOn`, `TargetLeft`, `TargetRight` and `ClearLockOn` Character Blueprint input helpers.
- Added configurable acquire/maintain/switch ranges, field-of-view scoring, distance weighting, LOS acquisition, LOS grace, auto-unlock and optional auto-reacquisition.
- Lock-on now synchronizes the selected actor into replicated `ARPGCombatComponent::CombatTarget`, with server-side target/distance validation and client reconciliation.
- Basic attacks automatically consume the locked target and use target-aware attack direction while preserving smooth facing.
- Gameplay Ability activation automatically requests target facing through the Ability System Component activation callback; `Combat.Targeting.IgnoreAutoFace` can opt an ability out.
- Added `ARPGGameplayAbility` with `Ignore Lock-On`, `Prefer Lock-On Target` and `Require Lock-On Target` policy, optional range validation, and Blueprint helpers for target actor/location/TargetData.
- Expanded `ARPGAbilityBridgeComponent` with preferred lock-on target access, target-data creation and tag activation with an explicit target.
- Added native `ARPGTargetMarkerWidget` and automatic screen-space marker creation on the selected target.
- Exposed target marker texture, UI material, tint/color, size, height/socket and acquire/pulse/release animation parameters directly on the Targeting Component.
- Added a built-in fallback reticle so lock-on UI works even before the game supplies custom artwork.
- Added Blueprint marker customization events while retaining the default native acquire/release animation.
- Added optional target aim socket and marker socket support for precise character/boss presentation.
- Added `Combat.State.LockedOn` and standard targeting-policy Gameplay Tags.
- Added UMG/Slate runtime dependencies required by the automatic native target marker.

## 1.1.0-alpha — 2026-08-09

- Rebuilt `ARPGCombatComponent` into a complete basic-combat runtime instead of death/respawn-only plumbing.
- Added Class Definition combat profiles with melee/ranged/magic basic attack type, damage scaling, criticals, ranges, projectile settings, detailed combo steps, dodge and block configuration.
- Ordered Class Definition melee/ranged/magic montage arrays now drive combos automatically when detailed combo steps are left empty.
- Added server-authoritative `PerformBasicAttack`, combo queue/reset state, automatic impact timing, melee sphere tracing, ranged hitscan and optional replicated `ARPGCombatProjectile`.
- Added direct character Blueprint input helpers: `BasicAttack`, `Dodge`, `BlockPressed`, `BlockReleased`.
- Added the built-in `ARPG Combat Impact` Anim Notify for montage-authored hit/fire timing without custom Blueprint trace logic.
- Added directional dodge with exposed montages, stamina cost, cooldown, launch/root-motion mode and invulnerability window.
- Added shield/guard blocking with facing arc, damage-type reduction, stamina pressure, perfect-block/parry timing and guard-break stagger.
- Added hit reactions, death/revive montage use, automatic player respawn/checkpoint transform support, kill-credit routing and loot handoff.
- Added standard combat Gameplay Tags plus `Equipment.Shield`.
- Added `ARPGAICombatComponent` and ready `ARPGAICharacter` for automatic faction/threat-driven NPC combat without requiring a Behavior Tree.
- Added AI reaction delay, dodge/block decisions, NavMesh chase, combat range selection and optional GAS auto-ability tags.
- Integrated AI spawner leash/home settings into AI combat and added dead combat pawn corpse cleanup/respawn handling.
- Corrected default Player faction assignment so it only applies to player-controlled characters.
- Marked the shared RPG definition base `NotBlueprintable` so designers create native Data Asset instances instead of accidentally creating incompatible definition Blueprint Classes.

## 1.0.2-alpha — 2026-08-09

- Fixed `UARPGAttributeSet::GetLifetimeReplicatedProps` to use Unreal's required `OutLifetimeProps` replication list and replaced the malformed macro/brace block with explicit `DOREPLIFETIME_CONDITION_NOTIFY` registrations.
- Fixed Gameplay Attribute RepNotify definitions so they compile cleanly with UE 5.8.1.
- Fixed the battle-pet cooldown local variable conflict in `SetCooldown`.
- Added the required `GameFramework/CharacterMovementComponent.h` include for combat death/respawn movement calls.
- Added the required `GameFramework/PlayerState.h` includes for chat player-name access.
- Renamed mount-local controller variables to avoid UE's shadow-variable warning-as-error.
- Updated setup documentation so only Gameplay Abilities is described as a plugin; GameplayTags and GameplayTasks remain Build.cs modules.

## 1.0.1-alpha — 2026-08-09

- Fixed UE 5.8 plugin dependency declaration: `GameplayTags` and `GameplayTasks` are runtime modules, not standalone plugins, so they are no longer listed in `AkumasRPGFramework.uplugin`.
- Kept `GameplayTags` and `GameplayTasks` in `AkumasRPGFramework.Build.cs`, where module dependencies belong.
- Added validator coverage for this descriptor mistake so it cannot silently return in later packages.

## 1.0.0-alpha — 2026-08-09

Initial unified source build of Akuma's RPG Framework for UE 5.8.

Highlights include the ready RPG character, GAS integration, stats/combat/progression, inventory/equipment, quests, generic skills + Slayer, factions/reputation, vendors/buyback, AI spawner/wanderers, boss/dungeon foundations, battle pets, unified chat, local accounts, direct-IP network helpers, character/world persistence, faction-aware modular building, storage/crafting/furnaces and mounts.
