# Akuma's RPG Framework — UE 5.8

<img width="1672" height="941" alt="AumaRPGFWSplash" src="https://github.com/user-attachments/assets/046f6018-b205-4f0a-82f7-5950b2e7877d" />

**Akuma's RPG Framework** is a Blueprint-first, data-driven Unreal Engine 5.8 gameplay framework for building large single-player and multiplayer RPGs without rebuilding the same core systems for every project.

Create content with **Data Assets**, configure inherited components in the editor, and let the shared C++ runtime handle authoritative gameplay, replication, persistence, inventories, combat, AI, crafting, building, world interaction and UI plumbing.

| Current release | Engine target | Project state |
|---|---|---|
| **v2.16.10-alpha** | **Unreal Engine 5.8 / 5.8.1** | Source framework — active development |

> **Latest release:** v2.16.10 adds contextual settlement-worker tool presentation. Assign an Axe `ARPGItemDefinition` to the Settlement Definition and villagers automatically hold its existing equipped mesh/socket/transform while `Going To Work` / `Woodcutting`, then remove it as soon as they return to roaming/home states. The visual is reconstructed from replicated work state and never grants an item, mutates Inventory/equipment slots or consumes durability. v2.16.9 build-aware Tree replacement/respawn suppression, the confirmed **2x2+** settlement loop, native Dynamic Recast stairs, world save v8, Lights and protected v2.15.53 Stair/Wall-family placement semantics remain intact. See [`Docs/WOODCUTTING.md`](Docs/WOODCUTTING.md), [`Docs/SETTLEMENTS.md`](Docs/SETTLEMENTS.md) and [`Docs/CHANGELOG.md`](Docs/CHANGELOG.md).

## Start here

- **New to the framework?** Read [`Docs/QUICK_START.md`](Docs/QUICK_START.md).
- **Setting up items/equipment?** Read [`Docs/EQUIPMENT_INVENTORY.md`](Docs/EQUIPMENT_INVENTORY.md).
- **Setting up crafting/durability/repair?** Read [`Docs/CRAFTING_DURABILITY_REPAIR.md`](Docs/CRAFTING_DURABILITY_REPAIR.md).
- **Setting up Settlement Hubs, homes, Beds or villagers?** Read [`Docs/SETTLEMENTS.md`](Docs/SETTLEMENTS.md).
- **Setting up structural building, buildable lights, storage or furnaces?** Read [`Docs/BUILDING_CRAFTING.md`](Docs/BUILDING_CRAFTING.md).
- **Want the full system status?** Read [`Docs/FEATURE_MATRIX.md`](Docs/FEATURE_MATRIX.md).
- **Want release history?** Read [`Docs/CHANGELOG.md`](Docs/CHANGELOG.md).

## What the framework is built around

The framework follows a few rules consistently:

- **Blueprint-first authoring** — common gameplay is exposed through inherited components, Data Assets and simple Blueprint-callable functions.
- **Native working defaults** — important systems include functional C++ defaults so you can test before creating custom Widget Blueprints or presentation logic.
- **Data-driven content** — items, recipes, classes, factions, skills, vendors, bosses, mounts, build pieces and other definitions live in reusable Data Assets.
- **Server-authoritative gameplay** — inventory mutations, equipment, combat, crafting, building, storage, production and world interaction validate on authority.
- **Persistent runtime identity** — item instances, durability, equipment state, structures, storage and crafting state are preserved instead of being reconstructed cosmetically.
- **Performance-aware runtime design** — systems prefer events, timers and short-lived ticks instead of permanent per-actor ticking.
- **Project-owned presentation** — native UIs are ready to use, while exposed Widget Classes and events let projects reskin them without replacing gameplay logic.

## Feature overview

### Player, items and progression

| System | Status | Highlights |
|---|---|---|
| Inventory | **Implemented** | Runtime item instances, stacking, capacity, replication, saving, exact Item Definition references and transfer support. |
| Equipment | **Implemented** | Requirements, effects, held visuals/audio, physical-socket exclusivity, broken-item rejection and authoritative equipment state. |
| Quick Access | **Implemented** | Persistent hotbar, exact runtime-instance assignment, drag/drop, active equipment handoff, consumables and cooldown display. |
| Generic Item Use | **Implemented** | Health/Mana/Stamina restore, Gameplay Effects, custom Blueprint item behaviors, cooldowns, consume-after-success and full-vitals protection. |
| Crafting | **Implemented** | Data-driven personal recipes, authoritative timed/batch crafting, quantity controls, cancellation/refund and station restrictions. |
| Durability & Repair | **Implemented** | Per-instance durability, combat/gathering wear, broken state, material repair costs and persistence through saves/storage. |
| JRPG Stats | **Implemented** | Six primary attributes, derived stats, attribute points, progression integration and ready stats UI. |
| Skills | **Implemented** | Generic XP/level curves, unlock metadata and persistence. |
| Woodcutting | **Implemented** | Skill progression, axe/tool checks, harvestable trees, successful-chop durability wear, drops and persistence. |
| Slayer | **Implemented** | Masters, weighted assignments, kill credit, XP, points, streaks and persistent tasks. |
| Quests | **Implemented** | Objectives, prerequisites, chains, rewards, repeatables, auto-complete and persistent objective state. |
| Currencies & Loot | **Implemented** | Currency balances, loot tables and item/currency/XP rewards. |

### Combat and character presentation

| System | Status | Highlights |
|---|---|---|
| Combat | **Implemented** | Melee/ranged/magic basics, combos, traces/projectiles, crit/armor, dodge, block/parry/guard break, death and respawn. |
| Lock-on Targeting | **Implemented** | Z-target-style acquisition, camera tracking, facing/strafe, switching, marker UI, LOS/range validation and GAS helpers. |
| Gameplay Ability System | **Implemented foundation** | Ability System Component, Attribute Set and framework bridge for project abilities/effects. |
| Equipment presentation | **Implemented** | Static/skeletal/custom held visuals, sockets, transforms and equipment audio. |
| Automatic Footsteps | **Implemented** | Player/NPC footsteps, physical-surface sound pools, replication and no animation-notify requirement. |
| NPC Info Popup | **Implemented** | Automatic name/level/health display, proximity visibility and reskinnable native UI. |
| Ragdoll Death | **Implemented** | Automatic NPC ragdoll with collision handling, impulses, replication and respawn reset. |

### Settlement building and production

v2.15 promotes the earlier building backend into a complete player-facing settlement workflow.

| System | Status | Highlights |
|---|---|---|
| Build Catalogue | **Implemented** | Character `Build Catalog`, categories, material costs, buildable counts and ready native menu. |
| Placement Preview | **Implemented** | Local ghost mesh, valid/invalid materials, live status, rotation, next/previous pieces and placement HUD. |
| Pivot-Aware Ground Placement | **Implemented** | Real mesh bounds anchor bottom/center/corner-pivot modular pieces correctly to landscape/support surfaces. |
| Structural Snapping | **Implemented** | Foundations, wall/window/door families, upper floors/ceilings/roofs, stairs/pillars and custom snaps, with logical structural occupancy, multi-storey build-order symmetry and multi-cell-aware facing. |
| Authoritative Placement | **Implemented** | Server revalidates catalogue membership, snap access, transform, range, support, collision, resources, factions and territory. |
| Construction | **Implemented** | Instant or timed builds, synchronized progress, mesh reveal/growth, material progress parameters and construction audio. |
| Buildable Lighting | **Implemented** | Ground/floor stick torches and Wall-family surface lights, replicated/persistent button toggles, smooth light/emissive fades, Point/Spot sources, Niagara/Cascade FX and non-blocking fixture semantics. |
| Doors | **Implemented** | Replicated smooth open/close, ownership/faction access, persistent state, moving slab collision, data-driven left/right hinge side and hosted-insert structural transparency. |
| Demolition | **Implemented** | Authoritative modification checks and configurable build-cost refund. |
| Storage | **Implemented** | Persistent containers, ready transfer UI and exact runtime-instance transfer for durable items. |
| Production / Furnaces | **Implemented** | Input/fuel/output inventories, recipe queues, tagged fuel, transaction-safe processing and ready production UI. |
| Building Persistence | **Implemented** | Structure identity, transform, health, construction progress, Door/Window/Light state, storage contents and station state. |

Supported standard build-piece kinds:

`Foundation` · `Wall` · `WindowWall` · `Window` · `Doorway` · `Door` · `Floor` · `Ceiling` · `Roof` · `Stair` · `Pillar` · `Storage` · `Production` · `Decoration` · `Custom` · `Light`

Common structural pieces can be authored with an **`ARPGBuildPieceDefinition` + Static Mesh** without creating an Actor Blueprint. Door, Window, Light, Storage and Production pieces automatically use their specialised native actors when no custom Actor Class is supplied.

See [`Docs/BUILDING_CRAFTING.md`](Docs/BUILDING_CRAFTING.md) for the complete Wood/Stone/Metal, snapping, storage and furnace workflow.

### AI and world simulation

| System | Status | Highlights |
|---|---|---|
| AI Combat | **Implemented** | Hostile acquisition, retaliation, threat, chase, facing, attacks, defence and allied combat coordination. |
| Advanced AI Spawner | **Implemented** | Weighted groups, collision-safe/NavMesh placement, movement modes, respawn, distance streaming and day/night population swapping. |
| Ground-Rise Spawn Entrance | **Implemented** | Collision-safe mesh-only emergence, synchronized presentation and temporary locomotion ownership. |
| Spline Patrol / Travel | **Implemented** | NavMesh-aware route following, waits/events, loops, group direction, combat suspension and route rejoin. |
| Free Roam / Wanderer | **Foundation + implemented movement layer** | Autonomous roaming hooks with spawner/social/spline ownership arbitration. |
| Ambient NPC Social | **Implemented** | Compatibility checks, approach/conversation behavior, montage/audio/text hooks, combat interruption and cooldowns. |
| Threat / Aggro | **Implemented** | Threat tables, taunt/set/clear utilities, ally assist and disposition restoration. |
| Bosses | **Implemented** | Boss types, encounter state, phases, enrage, leash/reset, scaling, contribution and respawn. |
| Day/Night | **Implemented** | Host-synchronized world time plus dynamic street lights and optional spawner population swaps. |

### World, social and online foundations

| System | Status | Highlights |
|---|---|---|
| Factions & Reputation | **Implemented** | Relationships, reputation, attack rules, ownership and faction-aware world interactions. |
| Vendors | **Implemented** | Stock/restock, restrictions, reputation pricing, selling and buyback. |
| Mounts | **Implemented** | Unlock, summon/ride, movement capability flags, presentation hooks and persistence. |
| Groups | **Foundation** | Replicated party/raid membership, roles/subgroups and chat integration. |
| Dungeons / Raids | **Foundation** | Encounter states, wipes, checkpoints, completion and persistence. |
| Battle Pets | **Foundation** | Collection/team/XP, turns, abilities, swaps, capture and persistence. |
| Unified Chat / Event Log | **Implemented** | Player, NPC, boss, system, quest, loot and event message routing. |
| Local Profiles | **Implemented** | Local username/password profile identities and character association. |
| Direct IP Networking | **Implemented** | Listen-server hosting and direct join-by-IP helpers. |

## Ready native UI

The framework includes working native UI for the systems that need an immediate player-facing test path. Projects can use these directly or replace them with Widget Blueprint subclasses through exposed Widget Class properties.

Current ready UI includes:

- **Item Management** — Inventory + Crafting & Repair tabs.
- **Quick Access HUD** — 8-slot default hotbar with active/equipped/cooldown state.
- **JRPG Stats** — complete stats/progression panel with authoritative attribute-point spending.
- **Character / NPC Info** — automatic name, level and health popup.
- **Build Menu** — build catalogue, categories, costs and buildable counts.
- **Build Placement HUD** — selected piece, requirements, placement validity and controls.
- **Storage** — Player ↔ Container item transfer.
- **Production / Furnace** — Player, Input/Fuel, Recipes, Output and queue progress.

The gameplay logic does not depend on the native visual style. Exposed widget classes, standard child bindings and update events allow project-specific reskins without rebuilding the server-authoritative systems underneath.

## The ready `AARPGCharacter`

`AARPGCharacter` comes with the main framework components already attached.

**Core gameplay**

- `Stats`
- `Combat`
- `Progression`
- `Class`
- `AbilityBridge`
- `Targeting`
- `Threat`
- `AICombat`

**Items and progression**

- `Inventory`
- `ItemUse`
- `Crafting`
- `Equipment`
- `QuickAccess`
- `Currencies`
- `Quests`
- `Skills`
- `Slayer`
- `Woodcutting`

**World and social**

- `Faction`
- `BattlePets`
- `BattlePetBattle`
- `Interaction`
- `Building`
- `Mounts`
- `Group`

**Framework services**

- `EventRouter`
- `Persistence`

**Presentation / local UI**

- `Footsteps`
- `CharacterInfo`
- `StatsUI`
- `InventoryUI`
- `BuildingUI`

Use `AARPGCharacter` directly as the base of a player/NPC Blueprint, or use the individual framework components in a custom architecture where appropriate.

## Data-driven architecture

The framework separates content definitions from mutable runtime state and persistence.

### 1. Definitions

`UPrimaryDataAsset` subclasses define reusable content such as:

- Items and equipment
- Recipes
- Classes
- Skills
- Quests
- Factions
- Vendors
- Bosses
- Battle pets
- Mounts
- Build pieces
- Crafting stations
- Dungeons/encounters

Create these as **Miscellaneous → Data Asset** instances of the relevant native `ARPG...Definition` class.

### 2. Runtime state

Replicated Actor Components hold mutable gameplay state such as Health, XP, Inventory instances, durability, equipped state, active quests, reputation, crafting progress and building state.

### 3. Persistence

SaveGame records persist stable IDs and runtime values rather than depending on transient UObject pointers. This keeps content definitions editable while long-lived character/world state remains recoverable.

## Automatic cross-system events

`UARPGEventRouterComponent` is the shared event bridge for progression and quest integration. It can route events such as:

- Kill
- Item looted
- Item crafted
- Structure built
- Skill level reached
- Pet captured
- Pet battle won
- Dungeon completed
- Raid boss defeated
- Mount unlocked
- Faction reputation changed

This avoids building a separate Blueprint counter network for every gameplay system.

## Multiplayer authority model

Gameplay mutations are designed to happen on the authoritative server.

Examples include:

- Inventory add/remove/transfer
- Equipment and durability
- Item use
- Crafting and repair
- Build placement/demolition
- Storage transfer
- Production queues/fuel/output
- Vendor and quest-giver interaction
- Combat state and damage

World interactions that would otherwise target unowned actors are routed through the player-owned replicated `ARPGInteractionComponent` where appropriate.

The local UI and placement ghost are presentation/prediction only; they do not bypass authoritative validation.

## Persistence

Character persistence covers the major player-state systems, including identity, transform, class, faction, vitals, progression, JRPG stat allocation, Inventory/equipment, durability, Quick Access, quests, skills, Slayer, reputation, currencies, battle pets, groups, mounts and active personal crafting state.

World persistence covers runtime player structures and their ownership, health, construction state, doors, storage contents, production inputs/outputs/queues and dungeon encounter progress.

See [`Docs/NETWORK_AND_AUTH.md`](Docs/NETWORK_AND_AUTH.md) for networking/authentication boundaries.

## Performance model

The framework intentionally avoids turning every feature into a permanent Tick.

Examples:

- Build placement ticks only while the local player is actively placing a piece.
- Completed ordinary structures are tick-free.
- Timed structures tick only during construction.
- Doors tick only while moving.
- Production stations tick only while a queue is active.
- UI progress timers exist only while the relevant interface is open.
- Spawn entrance ticks only during the short reveal.
- Footsteps and NPC visibility use staggered/event/timer-driven sampling rather than permanent component ticks.

Always profile with your actual content, target platform and multiplayer population before shipping.

## Installation

1. Close Unreal Editor.
2. Copy the complete `AkumasRPGFramework` folder into:

   `YourProject/Plugins/AkumasRPGFramework`

3. Confirm Unreal's built-in **Gameplay Abilities** and **Niagara** plugins are enabled.
4. Register the framework Primary Data Asset types in **Project Settings → Game → Asset Manager**. See [`Docs/ASSET_MANAGER.md`](Docs/ASSET_MANAGER.md).
5. Regenerate project files if required by your workflow.
6. Build the project's **Development Editor** target using the UE 5.8 toolchain.
7. Open the project and enable **Akuma's RPG Framework** if needed.
8. For the fastest start, derive the player Blueprint from `ARPGCharacter` and the GameMode from `ARPGGameMode`.

Then follow [`Docs/QUICK_START.md`](Docs/QUICK_START.md).

## Current release — v2.16.10-alpha

v2.16.10 polishes autonomous settlement workers with a contextual held-tool layer. `ARPGSettlementDefinition -> Settlement | Woodcutting | Tool Presentation` can reference the project's existing Axe Item Definition. Residents reconstruct a local `AARPGEquipmentVisualActor` from that Item's normal equipped static/skeletal mesh, socket and relative transform while `Going To Work` or `Woodcutting`; it is removed immediately when the resident returns to `Roaming`, `At Home`, `Returning Home` or `Homeless`. Optional Item equip/unequip montage and audio can be reused. This presentation never creates an Inventory entry, changes an equipment slot, applies equipment stats/effects or wears durability, and is not saved because replicated resident work state is the source of truth.

v2.16.9 integrates gatherable `AARPGTree` resources with runtime construction. Foundation preview/support traces pierce only encountered ARPG Tree actors and Foundation occupancy ignores Tree collision, so a trunk or stump can no longer prevent a legitimate terrain/grid snap. Once any ARPG Build Piece occupies the tree's configurable trunk-root regeneration volume, the authority suppresses that resource location: tree and stump visuals/collision are disabled, no harvest rewards are fabricated, and regeneration is deferred while one or more blockers remain. Removing the final build piece resumes the original respawn clock; if the natural respawn time already elapsed, the tree returns on the next bounded recheck. Suppression is replicated and Blueprint-readable/callable, while normal standing trees keep zero permanent polling cost. World save remains v8 because suppression is derived from persisted building occupancy rather than serialized as duplicate state.

A `UARPGSettlementDefinition` drives settlement radius/HUD range, deterministic non-overlap policy, configurable housing requirements (2x2 Foundations by default), recruitment/population, villager class/names/wander cadence, woodcutting policy and Hub stockpile size. `AARPGSettlementHubActor` validates homes semantically from completed Foundations, full overhead cover, Wall-family perimeter, Doorway and an installed Door. `AARPGBuildBedActor` supports Unassigned/Player/Villager roles through the existing authoritative Interact path.

Residents are real `AARPGSettlementVillagerCharacter` pawns derived from the existing AI character. They inherit Hub owner/faction identity, persist by resident/Hub/Bed IDs, roam through the existing AI wander system, reserve existing `ARPGTree` actors, chop through the normal tree durability/reward path and can deposit harvested resources into the Hub's native persistent stockpile.

`UARPGSettlementUIComponent` supplies a ready proximity Settlement HUD plus Hub, Bed and resident UI with exposed Widget Classes and Blueprint refresh hooks. The Hub panel can open its stockpile through the existing Storage UI. World save advances to v8 while retaining v6 Window and v7 Light migration behavior. See [`Docs/SETTLEMENTS.md`](Docs/SETTLEMENTS.md).

## Current release — v2.15.54-alpha

v2.15.54 adds an additive player-built lighting layer without changing the confirmed structural snapping code. `Light` is appended after `Custom` in `EARPGBuildPieceKind`, and common fixtures use the native `ARPGBuildLightActor` when `Actor Class` is empty.

- **Stick/freestanding lights:** `Light Placement Mode = Ground / Foundation / Floor` seats the transformed visible bottom on terrain or completed Foundation/Floor top surfaces.
- **Wall lights:** `Light Placement Mode = Built Wall Surface` follows the actually aimed face of completed `Wall`, `WindowWall` and `Doorway` modules; actor-local `+Y` points outward from the surface.
- **Interaction:** the existing `Interact Built Structure` path toggles authoritative replicated On/Off state. No fuel/wood is consumed by toggling.
- **Presentation:** native Point or Spot light, smooth fade duration, color/temperature/shadows/source radius, optional emissive scalar fade, Niagara/Cascade/fallback modes, optional sounds and independent light/FX relative transforms.
- **Persistence:** world-save schema v7 stores `bLightOn`; v6 Window migration remains intact.
- **Structural safety:** fixture visual collision is intentionally disabled and semantic spacing/interaction is used instead, so player-built lights cannot become new Stair/Wall/Floor/Window placement blockers. The v2.15.53 Stair/Wall-family boundary functions are unchanged and hash-locked by regression coverage.

See `Docs/BUILDING_CRAFTING.md` for Editor authoring examples for a stick torch, wall torch and hanging wall lantern.

## Previous release — v2.15.53-alpha

v2.15.53 fixes the final Stair/Wall-family rotation that remained red after v2.15.52. The shared classifier was build-order symmetric but still side-only: it accepted a Wall-family module only when its run axis was parallel to the Stair and its anchor sat on `local Y = +/-150`. In the reported remaining rotation, the same authored perimeter module lands on the Stair's exact LOW/HIGH boundary (`local X = +/-150`) with a perpendicular run axis, so it incorrectly fell through to generic collision.

- One `ARPGIsStairWallFamilyBoundarySeam()` predicate now owns both legal boundary forms in both build orders.
- **Side boundary:** parallel/180-equivalent run axis, exact `local Y = +/-SnapSize/2`, and longitudinal overlap against Stair `PlacementBounds.X`.
- **LOW/HIGH endpoint boundary:** perpendicular run axis, exact `local X = +/-SnapSize/2`, and lateral overlap against Stair `PlacementBounds.Y`.
- The calculation is performed in the final Stair-local structural frame, so yaw `0/90/180/270` receives the same result.
- `Wall`, `WindowWall`, `Doorway` and their verified hosted `Window`/`Door` inserts share the same boundary semantics.
- Walls through the Stair interior/centreline, distant modules, unrelated inserts and other real non-boundary conflicts remain blocked.
- No Editor authoring changes are required: keep the confirmed Stair bounds/snap settings and Wood `Window Insert Offset Z = +20`.

## Previous release — v2.15.52-alpha

v2.15.52 removed the forward/reverse Stair-side classifier split and made Stair-chain hosting use the same rule, but project-side PIE then exposed one remaining cardinal orientation that requires exact endpoint-boundary acceptance. v2.15.53 supersedes the side-only boundary contract while preserving the unified architecture.


## Previous release — v2.15.51-alpha

v2.15.51 corrected the 334-vs-300 cm endpoint-overhang calculation but project-side PIE subsequently proved the wider subsystem was still asymmetric across build order and Stair-chain hosting. v2.15.52 supersedes those separate forward/reverse seam decisions with one shared classifier.

## Previous release — v2.15.50-alpha

v2.15.50 replaced the old exact longitudinal actor-centre test with Wall-family span classification, but project-side PIE subsequently proved its comparison against the 300 cm structural cell was still insufficient for the 334 cm Stair's 17 cm endpoint collision overhang. v2.15.51 supersedes that forward seam calculation.

## Previous release — v2.15.49-alpha

v2.15.49 fixes a hosted-insert collision layer where a completed `Window` or `Door` could independently return **Blocked by another object** beside an otherwise-valid host seam. v2.15.51 retains that exact host-socket inheritance on top of the corrected Stair-side endpoint-overhang classifier.

## Previous release — v2.15.48-alpha

v2.15.48 fixes a real-world Window interaction occlusion case without changing the confirmed Window placement or animation setup. Conservative/simple collision on some WindowWall meshes can cover the visual aperture, causing the player view trace to hit the structural WindowWall before the hosted Window. The interaction layer now resolves the completed Window occupying that exact native WindowWall socket and toggles it through the same authoritative interaction RPC. It never traces through arbitrary blockers.

The v2.15.47 replicated/persistent Skeletal Window interaction, animation, collision and save behavior remains current and unchanged.

- `ARPGBuildWindowActor` now replicates authoritative open/closed state and persists it in world-save schema v6. Older worlds load safely with Windows closed.
- Window Build Piece Data Assets expose an Open Animation, optional Close Animation, playback rate, optional open/close sounds, and an open-collision policy. Leaving Close Animation empty automatically reverses the Open Animation.
- The active Skeletal Mesh ticks only while a Window is transitioning, then returns to a dormant final pose. Authority rejects another toggle while a transition is already running.
- Native `WindowCollision` is the gameplay blocker rather than imported mesh/Physics Asset collision. Opening removes it immediately; closing restores it only when fully closed.
- A separate Visibility-only interaction box remains targetable while open but never blocks Pawn movement, so the same `Interact Built Structure` button can close the Window again.
- `AARPGCharacter::InteractBuiltStructure()` now handles Doors, Windows, Storage and Production through the existing player-owned interaction authority path. No new hard-coded key is introduced; wire your project's normal Interact input action to that wrapper.
- v2.15.46 ghost/WindowWall acquisition, v2.15.45 X/Y/Z Window centering / `Window Insert Offset`, and the confirmed v2.15.43 structural lattice are protected.

## Previous release — v2.15.46-alpha

v2.15.46 hardens the first real Skeletal Window editor/PIE workflow without changing the confirmed structural baseline.

- Skeletal placement ghosts prepare the existing global Valid/Invalid Preview Materials for Skeletal Mesh material usage before applying them.
- `WindowWall -> Window` local acquisition accepts direct compatible host hits and a bounded hollow-frame/third-person view corridor while retaining the host's native socket as the only final transform.
- The semantic Window socket remains available even when generic standard snap generation is disabled.
- Authority still reacquires/validates the host and all v2.15.45/v2.15.43 geometry contracts remain unchanged.

## Previous release — v2.15.45-alpha

v2.15.45 corrects the native hosted-Window vertical socket while preserving the confirmed Door and structural behavior.

- `Doorway -> Door` remains floor-standing: visible bounds are centered in X/Y and bottom-aligned in Z exactly as before.
- `WindowWall -> Window` now centers transformed visible bounds in X/Y/Z. A 95 cm-high Window inside a 271 cm-high WindowWall therefore resolves 88 cm above a shared bottom plane by default instead of being forced to the bottom of the wall.
- `WindowWall` definitions expose **Window Insert Offset** under `Building | Window`. Keep it `0,0,0` for centered openings; use the host-local offset only for deliberately high/low/off-centre window apertures. Do not compensate with the incoming Window's generic `Placement Offset`.
- The rule is visual-type agnostic: Static and Skeletal Windows use the same `Mesh Relative Transform`-aware bounds calculation. v2.15.44 skeletal preview/final visual support and `ARPGBuildWindowActor` collision remain unchanged.
- Existing already-placed Window transforms are not migrated. Test the new socket with fresh Window placements after compiling.
- The confirmed v2.15.43 Foundation/Wall/WindowWall/Doorway/Floor/Stair story lattice and all Door behavior remain protected.

## Earlier release — v2.15.44-alpha

v2.15.44 adds a fully native **Skeletal Mesh visual path for building pieces** without replacing or renaming the established Static Mesh workflow.

- `ARPGBuildPieceDefinition` now exposes optional **Build Skeletal Mesh** and **Preview Skeletal Mesh** fields. Existing `Build Mesh` / `Preview Mesh` Static Mesh fields and existing Data Assets remain unchanged.
- A valid Build Skeletal Mesh is an explicit opt-in and takes presentation/bounds precedence over Build Mesh. If no skeletal asset is assigned, the original Static Mesh path is used exactly as before.
- Placement preview supports Skeletal Mesh ghosts. An explicit Static `Preview Mesh` may still be used as a lighter proxy for a skeletal final piece.
- Pivot-aware ground placement, transformed visible bounds, structural snapping, hosted `WindowWall -> Window` centering, collision validation and construction reveal all use the same active Static/Skeletal visual bounds. Skeletal art therefore does not require hand-authored snap offsets merely because its pivot differs.
- `Window` now resolves to a native `ARPGBuildWindowActor` with bounds-driven `WindowCollision`, so an imported Skeletal Window does not need a Physics Asset just to participate in authoritative duplicate-insert/occupancy collision. v2.15.47 layers replicated Data-Asset-driven open/close animation and persistence on this native Window actor without changing its placement bounds/sockets.
- Native Door presentation also accepts a skeletal visual beneath the existing moving `DoorPivot`; the confirmed Static Door behavior is preserved.
- Construction scaling/collision/material progress is applied through the active mesh component for either visual type; plain skeletal build visuals remain component-Tick dormant until a dedicated animation/interaction actor needs animation work.

The **v2.15.43 structural snapping and story/Stair behavior is intentionally unchanged** in this release. There is no save-schema migration. Existing Static Mesh build definitions require no edits.

For a Skeletal Window Data Asset, leave `Actor Class` empty, set `Piece Kind = Window`, assign the skeletal asset to **Build Skeletal Mesh**, clear `Build Mesh` if the definition was duplicated from a Static Mesh piece, and keep using `Mesh Relative Transform`, `Placement Bounds`, `Snap Size` and `Standard Wall Height` exactly as normal.

## Project boundaries

Akuma's RPG Framework is a **source gameplay framework**, not an art/content pack or hosted online backend.

It does not provide your final project-specific meshes, animations, maps, audio library, UI art, building art kit, abilities, balance data or online service credentials.

Some systems are intentionally foundations rather than attempts to reproduce every rule of a live service game. In particular:

- Battle-pet combat provides reusable collection/turn/capture foundations rather than every game-specific family/weather/rule combination.
- Wanderer AI provides autonomous movement/activity foundations; sophisticated playerbot-style goals remain game-specific.
- Dungeon/raid state is present, while global matchmaking, distributed instance servers and cloud lockout databases belong to an online backend.
- Direct IP networking does not provide NAT traversal.
- Local username/password profiles are for trusted/local profile use, not production Internet authentication.

A production game still requires project-specific UE compilation, multiplayer QA, save-migration testing, performance profiling, gameplay balancing, security review and content polish.

## Documentation

### Getting started

- [`Docs/QUICK_START.md`](Docs/QUICK_START.md) — fastest editor setup.
- [`Docs/ASSET_MANAGER.md`](Docs/ASSET_MANAGER.md) — Primary Asset registration.
- [`Docs/FEATURE_MATRIX.md`](Docs/FEATURE_MATRIX.md) — detailed implementation/status matrix.
- [`Docs/VALIDATION.md`](Docs/VALIDATION.md) — package validation information.
- [`Docs/CHANGELOG.md`](Docs/CHANGELOG.md) — complete release history.

### Player systems

- [`Docs/EQUIPMENT_INVENTORY.md`](Docs/EQUIPMENT_INVENTORY.md) — items, starting inventory and equipment presentation.
- [`Docs/INVENTORY_UI.md`](Docs/INVENTORY_UI.md) — Inventory/Item Management UI and drag/drop.
- [`Docs/QUICK_ACCESS.md`](Docs/QUICK_ACCESS.md) — hotbar assignment, switching and use.
- [`Docs/ITEM_USE.md`](Docs/ITEM_USE.md) — generic consumables and custom Item Use behaviors.
- [`Docs/CRAFTING_DURABILITY_REPAIR.md`](Docs/CRAFTING_DURABILITY_REPAIR.md) — crafting, durability and repair.
- [`Docs/JRPG_STATS.md`](Docs/JRPG_STATS.md) — JRPG stats/progression.
- [`Docs/JRPG_STATS_UI.md`](Docs/JRPG_STATS_UI.md) — ready Stats UI.
- [`Docs/WOODCUTTING.md`](Docs/WOODCUTTING.md) — gathering, axes, trees and resource drops.

### Combat, AI and presentation

- [`Docs/COMBAT.md`](Docs/COMBAT.md) — combat setup and runtime behavior.
- [`Docs/COMBAT_FEEL.md`](Docs/COMBAT_FEEL.md) — combat-feel configuration.
- [`Docs/TARGETING.md`](Docs/TARGETING.md) — lock-on targeting.
- [`Docs/FOOTSTEPS.md`](Docs/FOOTSTEPS.md) — automatic replicated footsteps.
- [`Docs/NPC_INFO_POPUP.md`](Docs/NPC_INFO_POPUP.md) — character/NPC info UI.
- [`Docs/RAGDOLL_DEATH.md`](Docs/RAGDOLL_DEATH.md) — NPC ragdoll death.
- [`Docs/AI_SPLINE.md`](Docs/AI_SPLINE.md) — spline patrol/travel.
- [`Docs/AI_SPAWNER_MOVEMENT.md`](Docs/AI_SPAWNER_MOVEMENT.md) — AI movement modes and free roam.
- [`Docs/AI_SPAWNER_GROUND_RISE.md`](Docs/AI_SPAWNER_GROUND_RISE.md) — spawn entrance presentation.
- [`Docs/AI_SPAWNER_DAY_NIGHT.md`](Docs/AI_SPAWNER_DAY_NIGHT.md) — day/night population swapping.
- [`Docs/AI_SOCIAL_INTERACTIONS.md`](Docs/AI_SOCIAL_INTERACTIONS.md) — ambient NPC interactions.
- [`Docs/GROUP_COMBAT.md`](Docs/GROUP_COMBAT.md) — coordinated AI combat.

### World and settlement

- [`Docs/BUILDING_CRAFTING.md`](Docs/BUILDING_CRAFTING.md) — settlement building, snapping, storage and production/furnaces.
- [`Docs/DAY_NIGHT.md`](Docs/DAY_NIGHT.md) — world time.
- [`Docs/DYNAMIC_STREET_LIGHTS.md`](Docs/DYNAMIC_STREET_LIGHTS.md) — automatic street lights.
- [`Docs/NETWORK_AND_AUTH.md`](Docs/NETWORK_AND_AUTH.md) — profiles, networking and authority boundaries.

## License

Akuma's RPG Framework is distributed under the **MIT License**. See [`LICENSE`](LICENSE) for the full license text.

---

**Akuma's RPG Framework is in active alpha development.** Build and test each update against your own UE 5.8 project before treating it as a shipping baseline.

