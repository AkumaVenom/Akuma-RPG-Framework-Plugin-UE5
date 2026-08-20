## Combat, Targeting & Relog Persistence Integrity (v2.15.40)

| Feature | Status | Notes |
|---|---|---|
| Faction persistence safety | Implemented | Empty/legacy saved faction ids no longer erase a valid authored/default player faction during load. |
| Reciprocal hostile AI combat | Implemented | Player damage permission and lock-on can recognize hostility owned by the target AI, preventing hostile NPCs from being untargetable/undamageable when the base relation is neutral or temporarily unresolved. |
| Player-only character persistence | Implemented | v2.15.40 prevents `AARPGAICharacter` from consuming `LastCharacterId` or reading/writing the player account character slot. NPCs keep authored/spawner state across relog instead of inheriting player faction/state. |

## Settlement Building, Storage & Production (v2.15.52)

| Feature | Status | Notes |
|---|---|---|
| Player build mode | Implemented | Inherited Building + local BuildingUI, ready catalog, local ghost, live validation, rotate/next/previous/confirm/cancel. |
| Pivot-aware ground placement | Implemented | v2.15.2+ anchors the active Build visual bounds to the traced surface so bottom/center/corner pivots place flush; validation/support/vertical snaps use the same mesh-aware anchor. |
| Data-driven mesh orientation | Implemented | v2.15.3 exposes per-piece `Mesh Relative Transform` so imported art can be rotated/offset/scaled inside the native build actor without reimporting meshes; ghost/final presentation and transformed bounds stay in parity while structural snapping retains stable logical actor axes. |
| Static/Skeletal build visuals | Implemented | v2.15.44 keeps the established Static `Build Mesh` path and adds optional `Build Skeletal Mesh` / `Preview Skeletal Mesh`. The active asset drives the same transformed bounds, ghost, construction presentation and snap math; a Static preview proxy may be used for a skeletal final piece, and plain skeletal build/preview components remain Tick-dormant until animation is explicitly needed. v2.15.46 prepares the existing global preview materials for Skeletal Mesh usage so a skeletal ghost does not remain on Unreal's grey/default fallback. |
| Native skeletal Window shell | Implemented | `Window` resolves to `ARPGBuildWindowActor` with bounds-driven gameplay collision independent of imported mesh/Physics Asset collision. v2.15.47 adds a separate non-blocking Visibility interaction target so an open Window remains button-interactable; v2.15.48 also resolves a hosted Window when conservative WindowWall collision is the first trace hit. |
| Structural snapping | Implemented | Standard modular snaps for foundations, wall/window/door families, floors/ceilings/roofs, stairs/pillars and custom snap transforms. Preview and authority resolve the same structural graph; full-view semantic acquisition is used for Door/Window openings. |
| Multi-storey build-order symmetry | Implemented | Upper slabs and Wall-family pieces can be authored in either order when they describe the same story seam. v2.15.42 uses the finished slab top/walking surface as the story plane and recesses slab thickness downward, so physical thickness does not create vertical gaps or cumulative storey drift. |
| Finished-surface story lattice | Implemented | **Confirmed v2.15.43 baseline:** Foundation/Floor/Ceiling/Roof finished tops own `0/300/600/900...` story surfaces; slab thickness extends downward and Wall-family bottoms use the same surface. Mesh height/thickness cannot redefine the next storey. |
| Logical structural occupancy | Implemented | Build-vs-build blocking uses authored `PlacementBounds`/semantic grid slots rather than decorative mesh collision. Valid wall continuations, L-corners, vertical stacks and Wall/Floor seams are accepted; duplicate/interior-crossing logical occupancy remains blocked. |
| Multi-cell Wall-family facing | Implemented | v2.15.23 resolves perimeter ownership from occupied horizontal cells and uses the owning support's **native Wall socket yaw** for authored facing. Shared interior edges preserve vertical/selected native facing instead of inventing an exterior. |
| Hosted Door/Window inserts | Implemented | Doors/Windows are hosted by Doorway/WindowWall sockets and are structurally transparent to later valid host seams. v2.15.45 preserves Door bottom-aligned Z but centers suspended Windows in X/Y/Z; `WindowWall -> Window Insert Offset` provides a host-local correction for intentionally off-centre apertures. v2.15.46 makes the WindowWall→Window socket intrinsic and hardens direct-hit / bounded-view-corridor acquisition for third-person and hollow-frame aiming. Authority still reacquires/validates the host; duplicate inserts and unrelated conflicts remain blocked. v2.15.53 makes a verified hosted Door/Window inherit only its host's accepted Stair structural boundary (parallel side or perpendicular exact LOW/HIGH endpoint), preventing the insert collider from independently vetoing a legal host relationship. |
| Stair structural snapping | Implemented | **Confirmed v2.15.43 geometry baseline:** the `334 × 300 × 278` Wood Stair is traversal art inside a `300 × 300 × 300` structural flight; structural anchors remain `±150 cm`, LOW-departure starts on the current walking surface, and Stair chains/landing cells advance exactly 300 cm. **v2.15.53 boundary completion:** one shared bidirectional classifier governs `Wall` / `WindowWall` / `Doorway` beside Stairs in either build order and across all four cardinal rotations, including Stair-chain hosts. Exact parallel side boundaries use local `Y = ±150 cm` plus longitudinal overlap; exact perpendicular LOW/HIGH endpoint boundaries use local `X = ±150 cm` plus lateral overlap. Verified hosted `Window` / `Door` inserts inherit only that exact host boundary in both directions. Interior/centreline/distant geometry and unrelated conflicts remain blocked. |
| Authoritative placement | Implemented | Server re-resolves transform/snap and validates catalog membership, snap-target modification access, resources, range, collision, support/slope, faction and territory before spawn. |
| Timed construction | Implemented | Instant or synchronized timed builds, upward reveal, material progress parameters, audio, construction collision policy, persistence. |
| Functional doors | Implemented | Replicated animated open/close, access policy, optional auto-close and world-save state. v2.15.10 fixes completed-build Tick ownership and adds a native moving slab collider; v2.15.11 makes the physical hinge side data-driven (`Left` / `Right`) from transformed visible bounds so art kits rotate around the correct jamb without mesh-pivot hacks. |
| Functional windows | Implemented | v2.15.47 adds server-authoritative replicated open/closed state, Data-Asset Open/Close skeletal animation (automatic reverse-close fallback), optional sounds, transition-safe collision, same-button `Interact Built Structure` routing and world-save v6 persistence. Skeletal component Tick is active only during transitions. v2.15.48 adds exact-host interaction recovery when the WindowWall itself occludes the Window trace. |
| Demolition/refund | Implemented | Ready view action routes through authoritative Interaction, requires modify access and applies configurable build-cost refund. |
| Persistent storage UI | Implemented | Ready Player/Storage transfer panel; exact runtime InstanceId transfer preserves durable item identity/condition. |
| Production/furnace | Implemented | Production build piece selects Crafting Station Definition; ready Player/Input+Fuel/Recipes/Output UI, fuel tags, queues, offline processing. |
| Furnace transaction safety | Implemented | Strict station tag, aggregated inputs, transactional input/fuel/output, whole-output capacity simulation. |
| Building UI reskinning | Implemented | Build menu/row, placement HUD, storage panel/item row, station panel/recipe row classes exposed on BuildingUI. |
| Building persistence | Implemented | IDs, transform, health, ownership, construction remaining time, door/window open state, storage, production input/output/queue. v2.15.12 persists a stable Guest Character ID and safely migrates the unambiguous single-local-player legacy Guest ownership case so loaded structures remain valid modification/snap targets after restart. |

## Player Crafting, Durability, Repair & Item Management UI (v2.14.0)

| Feature | Status | Notes |
|---|---|---|
| Personal player crafting | Implemented | Inherited `Crafting` component, server-authoritative, no permanent Tick, recipes explicitly exposed per character. |
| Shared recipe format | Implemented | Uses existing `ARPGRecipeDefinition`; direct Item Definition ingredient/output selection plus legacy ItemId compatibility. |
| Timed/batch crafting | Implemented | Owner-only synchronized progress, per-craft commits, cancellation refund, skill requirements/XP and output-capacity checks. |
| Instance durability | Implemented | Exact Inventory `InstanceId`, unique durable stacks, max/current/percent/broken state, broken equip rejection/auto-unequip. |
| Combat wear | Implemented | Explicit per-item opt-in; wear only after positive applied damage. |
| Gathering wear | Implemented | Explicit per-item opt-in; woodcutting charges exact axe instance only after a successful chop; generic API supports future pickaxes. |
| Equipment repair | Implemented | Full-repair material recipe, optional proportional scaling, authoritative material consumption and full durability restoration. |
| Shared Item Management UI | Implemented | Top-level Inventory and Crafting & Repair tabs with future-page switcher architecture. |
| Ready Crafting/Repair UI | Implemented | Recipe rows, detail summaries, batch quantity, Craft/Cancel/progress, repair list/cost/action. |
| Reskin support | Implemented | Exposed Crafting panel, Recipe row and Repair row Widget Classes plus standard child bindings/events. |
| Save/storage persistence | Implemented | Exact durability survives saves/container transfers; legacy saves migrate once; active personal craft progress resumes without double-consuming committed inputs. |

## Full-Vitals Consumable Guard (v2.13.2)

| Feature | Status | Notes |
|---|---|---|
| Full Health/Mana/Stamina protection | Implemented | Pure vital restoratives cannot succeed when all configured vital targets are already full. |
| Inventory local preflight | Implemented | The ready Use button disables and direct Inventory use returns false before sending an unnecessary RPC. |
| Quick Access local preflight | Implemented | Hotbar activation refuses a pure vital consumable locally when it has no useful restoration target. |
| Authority delta validation | Implemented | Consumption/cooldown/presentation require an actual positive vital change or another independent successful effect. |
| Mixed-effect compatibility | Implemented | v2.13.3 hard-gates configured vital restoration at full vitals by default; intentional mixed items can explicitly opt into secondary-effect use. |

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

## Core Framework Feature Matrix

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
| Character saves | Implemented | Character identity/state, vitals, progression, JRPG stat allocation/unspent Attribute Points, inventory/equipment/durability, Quick Access, quests, skills, Slayer, reputation, currencies, pets, groups, mounts and active personal crafting state. |
| World saves | Implemented | Runtime structures, ownership, health, construction state, doors, storage contents, production input/output/queues and dungeon progress. |
| Building | Implemented | Ready catalogue/ghost placement, pivot-aware ground anchoring, structural snapping, authoritative validation/costs, timed construction, doors, demolition/refunds and faction/territory rules. |
| Build preview UI/material | Implemented | Local ghost actor, valid/invalid material overrides, placement HUD, live placement status and exposed material parameters; project can replace/reskin presentation. |
| Storage/chests | Implemented | Shared inventory model, faction-aware access, persistent contents, ready Player/Storage UI and exact runtime InstanceId transfers. |
| Crafting/production | Implemented | Personal crafting plus station recipes/queues, input/fuel/output inventories, fuel tags, transaction-safe outputs, skill XP, quest events and offline elapsed processing. |
| Mounts | Implemented | Unlock, summon/ride, movement capability flags, animation hooks and save state. |
| Party/raid groups | Foundation | Replicated group membership/role/subgroup state and chat integration; full matchmaking/social backend remains project-level. |
| Ready/reskinnable UI | Implemented | Native defaults exist for Inventory/Quick Access, Stats, NPC Info, Crafting/Repair, Building, Storage and Production; exposed Widget Classes/events support project reskins. |


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

- **v2.15.41:** canonical Wall story lattice removes rendered Wall mesh height from vertical structural progression and seam validation.
