## Full-Vitals Consumable Guard (v2.13.2)

| Feature | Status | Notes |
|---|---|---|
| Full Health/Mana/Stamina protection | Implemented | Pure vital restoratives cannot succeed when all configured vital targets are already full. |
| Inventory local preflight | Implemented | The ready Use button disables and direct Inventory use returns false before sending an unnecessary RPC. |
| Quick Access local preflight | Implemented | Hotbar activation refuses a pure vital consumable locally when it has no useful restoration target. |
| Authority delta validation | Implemented | Consumption/cooldown/presentation require an actual positive vital change or another independent successful effect. |
| Mixed-effect compatibility | Implemented | GAS/custom behavior items remain eligible at full vitals because their independent effect may still be useful. |

## Equipment Physical-Socket Exclusivity (v2.13.1)

- Central Equipment authority prevents two visible equipment items from owning the same resolved skeletal-mesh socket even when logical EquipmentSlot tags differ.
- Inventory, Quick Access, starting equipment and direct Blueprint equip requests share the same replacement rule.
- Authority self-repairs duplicate same-socket equipped state from old saves/inconsistent data, with active Quick Access preferred when resolving a tie.
- Visual projection suppresses duplicate same-socket actors during recovery.
- Different sockets remain independently equippable.

## Generic Item Use (v2.13.0)

| Feature | Status | Notes |
|---|---|---|
| Direct Inventory item use | Implemented | Ready Inventory Use button/right-click and Character Blueprint helpers; no hotbar assignment required. |
| Built-in consumables | Implemented | Health/Mana/Stamina restore, GAS effect, consume quantity, cooldown, montage and sound. |
| Per-item Blueprint behavior | Implemented | Assign an `ARPGItemUseBehavior` class on an Item Definition for custom authoritative logic and cosmetic presentation. |
| Unified Quick Access use | Implemented | Hotbar Use delegates to the same ItemUse authority path and shares cooldown/consumption validation. |

## Ready Inventory + Quick Access UI (v2.12.2)

| System | Status | Notes |
|---|---|---|
| Inherited InventoryUI component | Implemented | Present on every `AARPGCharacter`; local player only, non-replicated, dedicated-server UI suppressed. |
| Native Inventory panel | Implemented | Runtime items, Item Definition icons/names/descriptions, quantity, rarity, durability, bound/equipped state, capacity, selection detail and Close button. |
| Native Quick Access HUD | Implemented | Auto-created after local possession; slot numbers, icons, owned quantity, active/equipped highlighting and live cooldown display. |
| Inventory -> Quick Access drag/drop | Implemented | Assigns the exact owned runtime Inventory `InstanceId`; existing Quick Access duplicate/ownership validation remains authoritative. |
| Quick Access rearrange | Implemented | Slot-to-slot drag uses existing server-authoritative `SwapSlots`. |
| Drag-away clear / unequip | Implemented | Quick Access -> Inventory/outside clears the assignment; optional atomic authority path unequips the active held item first while preserving Inventory ownership. |
| Custom Widget Blueprint support | Implemented | Exposed panel/bar/slot Widget Classes plus standard child-name bindings and update events. |
| Performance | Implemented | Event-driven refresh, no permanent Tick; short local timer only while a visible consumable cooldown is active. |

## Automatic proximity character / NPC info popups (v2.10.1)

| System | Status | Notes |
|---|---|---|
| Inherited CharacterInfo component | Implemented | Present on every `AARPGCharacter`; select the component and assign a Widget Class directly in the Character Blueprint. |
| Automatic Name / Level / Health | Implemented | Reads replicated `RPGCharacterName`, effective runtime level, current/max health and health percent. |
| Custom Widget Blueprint support | Implemented | `ARPGCharacterInfoWidget` base/event path plus zero-graph TextBlock/ProgressBar child-name mapping for ordinary UserWidgets. |
| Local proximity visibility | Implemented | Per-client show/hide distances with hysteresis, optional line of sight, local-player self hiding, dead hiding and NPC/player filters. |
| Spawn presentation integration | Implemented | Popup remains hidden through the v2.9 replicated ground-rise entrance by default. |
| Performance | Implemented | No permanent component Tick, staggered local timers, no dedicated-server UI, lazy widget construction and delayed far-widget release. |

## Polished AI spawner entrances (v2.9.0)

| System | Status | Notes |
|---|---|---|
| Automatic ground-rise entrance | Implemented | Enabled by default on `ARPGAISpawner`; every `SpawnOne()` path applies the same entrance to framework AI. |
| Collision-safe presentation | Implemented | Final actor/capsule remains at the v2.7.2 accepted spawn position; only the skeletal mesh is temporarily offset below ground. |
| Multiplayer timing | Implemented | Replicated compact entrance state + synchronized server world time; no per-frame transform replication stream. |
| Movement ownership | Implemented | CharacterMovement, AIController, Wanderer, Spline and optional Combat/Social behaviour are held and independently restored after the reveal. |
| Size-aware authoring | Implemented | Automatic full-capsule visual depth plus exposed manual depth, extra depth, delay, duration, easing and actor-location lock. |
| Performance | Implemented | SpawnEntrance component Tick is disabled except for the short active reveal. |

## Automatic replicated footsteps (v2.8.0)

| System | Status | Notes |
|---|---|---|
| Player + NPC automatic footsteps | Implemented | Inherited `Footsteps` component on `ARPGCharacter`; grounded real-distance cadence, left/right alternation and no animation notify requirement. |
| Physical Surface audio | Implemented | Per-surface randomized sound pools with Default Sounds fallback, pitch/volume variation and real ground tracing. |
| Multiplayer presentation | Implemented | Server-authoritative unreliable multicast; owning player local prediction suppresses duplicate return audio; NPC cadence stays server-driven. |
| 3D/crowd audio authoring | Implemented | Optional Sound Attenuation and Sound Concurrency assets exposed on the component. |
| Performance | Implemented | No permanent Tick; staggered timers and traces only when a footstep is due. |

## Dynamic world lighting (v2.7.0)

| System | Status | Notes |
|---|---|---|
| Blueprint-derivable dynamic street lamp | Implemented | Inherited movable Point Light, Niagara and Cascade components; add project mesh/components in child Blueprints. |
| ARPGDayNightCycle integration | Implemented | Immediate startup phase sync, event-driven phase/hour updates, optional explicit cycle override and auto-discovery retry. |
| Niagara / Cascade fallback | Implemented | Niagara-preferred fallback, Niagara-only, Cascade-only, both or none. |
| Multi-light Blueprint support | Implemented | Optional control of all LightComponents owned by the lamp actor, including Blueprint-added Spot/Rect/Point lights. |
| Blueprint presentation hooks | Implemented | State delegate + overridable event for emissive materials, sounds, animation and project-specific FX. |
| Large-town runtime cost | Implemented | No permanent per-lamp Tick and no duplicate per-lamp replicated clock; derives cosmetic state from replicated Day/Night cycle. |

# Feature Matrix — v2.6.0 Ambient NPC Social Interactions

Legend:

- **Implemented** — C++ runtime path exists in this package.
- **Foundation** — core runtime/data hooks exist; project-specific content/UI/online service is still required.

| Area | Status | Included in this source build |
|---|---|---|
| Ready RPG character | Implemented | Main components preinstalled on `AARPGCharacter`. |
| Character JRPG stats / attribute points | Implemented | Opt-in Strength/Vitality/Magic/Spirit/Dexterity/Luck progression, natural level growth, derived melee/ranged/magic/defense/evasion/crit/speed/vitals, server-authoritative spendable Attribute Points, equipment stat modifiers, save migration and replicated UMG-ready snapshots/events. |
| Day / night world time | Implemented | Drop-in `ARPGDayNightCycle` with host-PC local time authority, replicated/smoothed client clock, built-in sun/moon/Sky Atmosphere/real-time Sky Light/fog rig, external-light support, semantic dawn/day/dusk/night phases, events and global Blueprint-pure time/day/night nodes. |
| GAS integration | Implemented | Ability-system component/attribute set/ability bridge, `ARPGGameplayAbility` lock-on targeting policy/TargetData helpers and effect-friendly data hooks. |
| Automatic action-RPG combat | Implemented | Class-driven melee/ranged/magic basic attacks, ordered montage combos, timed sphere traces/hitscan/projectiles, damage/crit/armor, dodge, block/parry/guard break, critical-hit stagger/knockback, Niagara/Cascade impact FX, exposed combat audio, hit reactions and combat state tags. |
| Player lock-on targeting | Implemented | `ARPGTargetingComponent` is preinstalled on `ARPGCharacter`: toggle lock, camera-centered hostile acquisition, continuous Z-target camera/control-rotation tracking, continuous player facing/strafe mode, left/right switching, LOS/range validation, animated marker, server target validation and GAS target-data helpers. |
| Classes | Implemented | Data-driven class definition and class component. |
| Inventory | Implemented | Stacking, add/remove/transfer, exact runtime Item Definition references, save/replication and server authority. |
| Quick Access / hotbar | Implemented | Persistent 1-based active item slots, strict one-slot-per-runtime-instance identity, owned-runtime binding/rebinding, weapon/tool equip switching, food/potion use, cooldowns, cycling, UMG views, owner-only replication and save state. |
| Equipment | Implemented | Slot validation, level/class requirements, Gameplay Effect application/removal, held visuals/audio and authoritative quick-switch integration. |
| Loot/currency | Implemented | Loot tables, item/currency/XP rewards, currency balances. |
| Quests | Implemented | Prereqs, objectives, chains via prereqs, rewards, repeatable, auto-complete, persistence. |
| Quest giver NPC | Implemented | `ARPGQuestGiverComponent` plus player-owned RPC interaction route. |
| Generic skills | Implemented | Per-skill XP/level, custom XP curve, unlock metadata and persistence. |
| Woodcutting / harvestable trees | Implemented | Inherited player Woodcutting component with persistent 1-99 skill XP, level/tool gates, equipped axe power/tier, automatic view targeting/repeated interaction chops, context-sensitive Basic Attack -> single chop with combat-target priority, Blueprintable `ARPGTree` mesh arrays plus replicated min/max size variation, Wood Item/bonus drops, falling trunk/stump/respawn, FX/audio and direct Inventory/Building/Collect-quest integration. |
| Slayer | Implemented | Master definitions, weighted tasks, skill/combat requirements, kill count, XP, points/streaks, cancellation and persistence. |
| Factions/reputation | Implemented | NPC/player faction IDs, relationship/reputation checks, attack-on-sight rules, automatic damage retaliation/ally assist fallbacks, and persistence. |
| Player faction/building ownership | Implemented | Player-built structures inherit character/account/faction identity. |
| Vendors | Implemented | Buy/sell, finite/unlimited stock, restock, quest/faction/level gates, discounts and buyback. |
| Vendor services | Foundation | BlueprintNativeEvent service hook for repair/trainer/project-specific services. |
| Ambient NPC social interactions | Implemented | Opt-in inherited `AISocial` component with faction/tag compatibility, local Pawn opportunity scans, randomized approach/conversation duration, shared interaction ids with per-NPC montage/audio/text content, movement pause/resume, combat interruption, cooldowns, replication and Blueprint events. |
| Automatic NPC combat AI | Implemented | `ARPGAICharacter`/`ARPGAICombatComponent` auto-acquire/retaliate, use threat, chase, face, attack, dodge/block, optionally activate GAS ability tags, immediately restore temporary-retaliation targets to original disposition after death, and coordinate allied melee attack slots/wait-orbit movement without requiring a Behavior Tree. |
| Automatic NPC ragdoll death | Implemented | `ARPGAICharacter` ragdolls by default with capsule/collision handling, inherited velocity, killing-hit impulse, multiplayer death presentation, respawn reset and automatic Death montage fallback when physics is unavailable. |
| AI spline patrol / travel | Implemented | `ARPGAISplineRoute` + inherited `ARPGAISplineComponent`: NavMesh look-ahead following without attachment, route-level Loop/closed/open-end behavior, group direction synchronization, route-point waits/events, Route Id lookup, spawner assignment, combat suspension/leash/rejoin and Wanderer conflict handling. |
| Advanced AI spawner | Implemented | Weighted collision-safe group spawn, capsule-safe replicated ground-rise entrance, count range, shape/radius, NavMesh projection, independent group-vs-cohesion semantics, synchronized spline direction, selectable Automatic/Spline/Free-Roam/Hold movement, spawner-leashed reachable-point roaming, combat-home leash integration, dead-pawn corpse cleanup/respawn, default-on player-distance population streaming, plus opt-in midnight population swapping with a separate weighted enemy table, clean midnight/morning handoff, and phase-aware distance reload preservation. |
| Wanderer AI | Foundation | Optional autonomous roaming/activity foundation; game-specific playerbot brains can extend it. |
| Threat/aggro | Implemented | Threat table, highest target, taunt/set/clear utilities, temporary aggression/ally assist, target-death threat cleanup, original-disposition restoration and automatic AI target preference. |
| Bosses | Implemented | Boss types, encounter state, phases, enrage, leash/reset, scaling, contributions and world respawn. |
| Dungeons/raids | Foundation | Encounter state machine, wipes, checkpoints, completion/persistence and definitions. Online instancing/matchmaking services remain project-level. |
| Battle pets | Foundation | Collection/team/XP, wild battle start, turns/abilities/swaps/capture and persistence. Extend for exact game-specific family/weather/rulesets. |
| Unified chat/event log | Implemented | Player + NPC/boss/system/quest/loot/event message types and per-client routing. UMG presentation is project-facing. |
| Local account/login | Implemented | Username/password local profiles, salted verifier, account/character association. |
| Production Internet auth | Foundation | Must be supplied by a trusted backend/platform provider; intentionally not faked by local SaveGame authentication. |
| Direct IP hosting/join | Implemented | Listen-server open-level flow, configurable port, direct ClientTravel join. |
| LAN discovery/NAT traversal | Foundation | Direct LAN IP works; service/session discovery and NAT traversal require an online subsystem/provider. |
| Character saves | Implemented | Character identity/state, vitals, progression, JRPG stat allocation/unspent Attribute Points, inventory/equipment, Quick Access slots/active slot, quests, skills, Slayer, reputation, currencies, pets, group and mounts. |
| World saves | Implemented | Runtime player buildings, ownership, health/upgrades, storage, crafting queues/output and dungeon progress. |
| Building | Implemented | Snapping, validation, cost consumption, support/collision, health, repair/demolish and faction rules. |
| Build preview UI/material | Foundation | Placement evaluation API is exposed; project supplies preferred ghost mesh/material UX. |
| Storage/chests | Implemented | Shared inventory model, faction-aware access and persistent contents. |
| Crafting/production | Implemented | Recipes, queues, station/player inputs, fuel tags, outputs, skill XP, quest events and offline elapsed processing. |
| Mounts | Implemented | Unlock, summon/ride, movement capability flags, animation hooks and save state. |
| Party/raid groups | Foundation | Replicated group membership/role/subgroup state and chat integration; full matchmaking/social backend remains project-level. |
| UMG/UI skin | Foundation | All key data/delegates are Blueprint exposed; visual UI assets are intentionally supplied by the game. |


## v2.2 Quick Access additions

| System | Status | Notes |
|---|---|---|
| Active item slots | Implemented | 1-based persistent hotbar with `Activate Slot`, current slot, next/previous cycling and clean character input wrappers. |
| Runtime ownership binding | Implemented | Stable ItemId bookmark + exact runtime instance GUID; never grants availability from Data Asset presence. |
| Consumable use | Implemented | Food/potions can restore framework vitals, apply an optional GAS effect, consume quantities, enforce item-type cooldown and play use montage/audio. |
| UMG integration | Implemented | Slot-view struct exposes assigned/owned/active state, icon definition, quantity, resolved action and cooldown remaining plus change delegates. |

## v2.1 usability additions

| System | Status | Notes |
|---|---|---|
| Starting inventory authoring | Implemented | Editable Item Definition/quantity/Equip On Spawn array; runtime GUID entries remain protected. |
| Equipment visuals | Implemented | Automatic local held static/skeletal/custom visual actor from replicated equipped state, socket + relative transform. |
| Equipment presentation audio | Implemented | Equip/unequip/combat swing/gathering swing/gathering hit audio on Item Definitions with combat/Woodcutting integration. |

### Ambient AI movement coordination (v2.6.1)
- Spawned Free Roam immediate first destination: **Implemented**
- Temporary Wanderer pause ownership without mutating persistent enablement: **Implemented**
- Social ↔ Free Roam deterministic pause/resume handoff: **Implemented**
- Social ↔ Stay-Together cohesion ownership arbitration: **Implemented**
- Cohesion hysteresis recovery reissue until recovery radius: **Implemented**
- Wanderer yields to active Spline route: **Implemented**

## v2.11 Player Stats UI

- Inherited local-player `StatsUI` component: **Implemented**
- Ready-to-use native complete JRPG stats panel: **Implemented**
- One-call Open / Close / Toggle Blueprint API: **Implemented**
- Built-in Close button: **Implemented**
- Server-authoritative Attribute Point `+` buttons: **Implemented**
- Complete primary / allocation / derived / vitals / XP snapshot: **Implemented**
- Custom Widget Blueprint subclass + standard-name auto binding: **Implemented**
- No permanent UI/component Tick and no replicated UI state: **Implemented**
