# Changelog

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
