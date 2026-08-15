# Akuma's RPG Framework — UE 5.8

<img width="1672" height="941" alt="AumaRPGFWSplash" src="https://github.com/user-attachments/assets/046f6018-b205-4f0a-82f7-5950b2e7877d" />

**Akuma's RPG Framework** is a Blueprint-first, data-driven Unreal Engine 5.8 gameplay framework for building large single-player and multiplayer RPGs without rebuilding the same core systems for every project.

Create content with **Data Assets**, configure inherited components in the editor, and let the shared C++ runtime handle authoritative gameplay, replication, persistence, inventories, combat, AI, crafting, building, world interaction and UI plumbing.

| Current release | Engine target | Project state |
|---|---|---|
| **v2.15.40-alpha** | **Unreal Engine 5.8 / 5.8.1** | Source framework — active development |

> **Latest:** v2.15.40 is the current framework baseline. It preserves the v2.15.39 combat/faction safeguards and fixes the actual relog root cause: `AARPGAICharacter` inherits the player `Persistence` component, so enemies could consume the account's `LastCharacterId` and load the player's character save after relog. Account character persistence is now strictly player-character scoped at the AI defaults, persistence component, and save-subsystem boundaries. NPCs retain their authored/spawner faction and combat state, restoring lock-on and hit registration after relog without changing the v2.15.38 Stair/building runtime. See [`Docs/CHANGELOG.md`](Docs/CHANGELOG.md) for the release-by-release history.

## Start here

- **New to the framework?** Read [`Docs/QUICK_START.md`](Docs/QUICK_START.md).
- **Setting up items/equipment?** Read [`Docs/EQUIPMENT_INVENTORY.md`](Docs/EQUIPMENT_INVENTORY.md).
- **Setting up crafting/durability/repair?** Read [`Docs/CRAFTING_DURABILITY_REPAIR.md`](Docs/CRAFTING_DURABILITY_REPAIR.md).
- **Setting up settlement building, storage or furnaces?** Read [`Docs/BUILDING_CRAFTING.md`](Docs/BUILDING_CRAFTING.md).
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
| Doors | **Implemented** | Replicated smooth open/close, ownership/faction access, persistent state, moving slab collision, data-driven left/right hinge side and hosted-insert structural transparency. |
| Demolition | **Implemented** | Authoritative modification checks and configurable build-cost refund. |
| Storage | **Implemented** | Persistent containers, ready transfer UI and exact runtime-instance transfer for durable items. |
| Production / Furnaces | **Implemented** | Input/fuel/output inventories, recipe queues, tagged fuel, transaction-safe processing and ready production UI. |
| Building Persistence | **Implemented** | Structure identity, transform, health, construction progress, door state, storage contents and station state. |

Supported standard build-piece kinds:

`Foundation` · `Wall` · `WindowWall` · `Window` · `Doorway` · `Door` · `Floor` · `Ceiling` · `Roof` · `Stair` · `Pillar` · `Storage` · `Production` · `Decoration` · `Custom`

Common structural pieces can be authored with an **`ARPGBuildPieceDefinition` + Static Mesh** without creating an Actor Blueprint. Door, Storage and Production pieces automatically use their specialised native actors when no custom Actor Class is supplied.

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

## Current release — v2.15.40-alpha

v2.15.40 is the current framework baseline. It preserves the current Settlement Building/Stair runtime while fixing player/NPC persistence scope so relogged enemies cannot consume the player account save.

**Relog persistence/combat integrity**

- Account `SaveCharacter` / `LoadCharacter` persistence is player-character only. `AARPGAICharacter` cannot consume the account `LastCharacterId` or alias the player's character save slot.
- AI defaults disable inherited character auto-load, auto-save and save-on-EndPlay. Spawner/world systems remain responsible for NPC state.
- The v2.15.39 reciprocal hostility/faction safeguards remain as defense-in-depth, but v2.15.40 fixes the earlier source of the corrupted enemy runtime identity itself.


**Structural workflow**

- Foundations provide the ground-level modular grid.
- Wall / WindowWall / Doorway pieces snap to Foundation and upper horizontal edges, stack vertically, continue laterally and form 90-degree corners.
- Door and Window pieces are hosted inserts. They snap into Doorway / WindowWall openings, keep their replicated interaction state, and no longer block later valid Floor/Wall seams around their host.
- Floor / Ceiling / Roof pieces can be inserted before or after surrounding Wall-family pieces. Multi-storey construction is intentionally build-order independent where the same logical structural slot is being authored.
- Build-vs-build placement uses authored logical `PlacementBounds`/semantic structural slots rather than decorative mesh collision, while world/non-building blockers still use the normal collision path.

**Wall-family facing and vertical spacing**

- First-storey Wall-family pieces use the owning Foundation edge's native standard Wall socket.
- Upper-storey Wall-family pieces use the canonical horizontal story plane so slab thickness does not create gaps or accumulate vertical drift.
- Perimeter facing is resolved from occupied Foundation/Floor cells, but the final yaw comes from the owning support's own native Wall socket rather than a hard-coded mesh-front assumption.
- A direct Wall-family piece below preserves vertical facing continuity when the same stack column exists.
- Shared interior edges intentionally have no universal exterior side; the framework preserves the established/selected native facing instead of inventing one.

**Door baseline**

- Native Doors replicate open/closed state, animate smoothly, persist state, enforce ownership/faction access and carry a moving collision slab.
- `Door Hinge Side` is data-driven (`Left` / `Right`) and is resolved from transformed visible Door bounds.
- The existing Character input route remains `Interact Built Structure`; no client-side direct Door state mutation is required.

**Stair baseline**

- `Foundation`, `Floor` and `Ceiling` remain the generic flat Stair landing hosts; `Roof` remains excluded from the standard flat contract.
- v2.15.32 adds a **paired landing anchor** to every compatible horizontal edge. The existing HIGH-end arrival socket is preserved for a flight that lands on the edge and descends outward/down. A new LOW-end departure socket uses the **same edge center and yaw**, allowing the next flight to start on that landing and rise inward/up without lateral drift.
- Completed `Stair` actors now also advertise endpoint-chain sockets: `LOW(incoming) -> HIGH(target)` continues upward and `HIGH(incoming) -> LOW(target)` continues downward. The incoming flight inherits the target Stair's run axis, so chained flights stay on one structural centerline.
- When a lower Stair already arrives at a Foundation/Floor edge, the horizontal host's LOW-departure candidate and the lower Stair's HIGH-end chain candidate resolve to the **same physical transform**. This makes the landing build-order independent and prevents upper flights from being offset from the lower flight.
- Stair capture now ranks paired landing sockets by the candidate envelope as well as target support bounds. Aiming inside/on the deck favors the up-flight departure socket; aiming outside/down favors the down-flight arrival socket, while exact snapped transforms remain authority-reacquirable.
- v2.15.31 Landscape classification remains intact: a verified snapped Stair may embed/blend into `ALandscapeProxy`, while discrete WorldStatic rocks, cliffs and props continue through strict obstruction validation.
- Logical Stair convention after `Mesh Relative Transform`: local `+X` is uphill, local `Y` is width and local `+Z` is up. If imported art rises toward `-X`, rotate the mesh-relative transform 180° once instead of adding custom snap offsets.
- The segmented Stair occupancy profile from v2.15.26 and shared broad/final occupancy contract from v2.15.27 remain active. Side Wall-family seams are derived from whichever Stair endpoint owns the horizontal host edge, so both descending outside flights and ascending inside flights keep correct side-wall topology.
- The current Wood Stair example remains `334 × 300 × 278` on a 300-unit grid with `Placement Bounds = 167,150,139`, zero Placement Offset and standard generated snap points.

No reflected Blueprint API or save-schema migration is required for v2.15.40. Existing placed structures keep their saved transforms; the relog fix only scopes account character persistence away from AI NPCs.

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

