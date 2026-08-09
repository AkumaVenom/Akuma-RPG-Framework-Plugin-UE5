# Akuma's RPG Framework — UE 5.8

**Akuma's RPG Framework** is a source-first Unreal Engine 5.8 gameplay framework for building large single-player RPGs while keeping authoritative multiplayer paths available from day one.

The design goal is simple: create content with Data Assets, assign components/definitions in the editor, and let shared C++ systems handle the repetitive RPG plumbing—saving, replication, item movement, quest progress, factions, crafting, spawning, death/respawn, chat routing, and cross-system events.

> **Build status:** source framework alpha. The code has passed repository-level structural validation in the generation environment, but it has **not** been compiled against your local UE 5.8 installation here. A real UE 5.8 Development Editor build and in-project PIE/runtime QA are required before shipping.


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
- Stats, melee/ranged/magic-friendly combat hooks, death and respawn.
- Character progression, classes, abilities/effects and animation-set hooks.
- Inventory, equipment, item definitions, loot and currencies.
- Quests with objective-level persistence, prerequisites, chains, rewards, repeatables, auto-complete and quest-giver helpers.
- Generic skill progression with configurable curves/unlocks.
- First reference skill: **Slayer**, including Slayer masters, weighted assignments, kill credit, XP, points, streaks and persistent tasks.
- Factions and reputation for players, NPCs, wanderers, bosses and buildings.
- Vendors with stock, restock, restrictions, reputation pricing, selling, buyback and Blueprint service hooks.
- Advanced AI group spawner with weighted entries, exact/ranged counts, NavMesh projection, respawn modes and optional home/leash behavior.
- Optional autonomous wanderer AI foundation for populating single-player worlds.
- Threat/aggro utilities and loot-table components.
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

3. Confirm the built-in **Gameplay Abilities** plugin is enabled. `GameplayTags` and `GameplayTasks` are module dependencies handled by the plugin Build.cs; they are not separate `.uplugin` dependencies.
4. Register the framework's Primary Data Asset types in **Project Settings > Game > Asset Manager**. See `Docs/ASSET_MANAGER.md`.
5. Right-click the `.uproject` and regenerate Visual Studio project files if your workflow requires it.
6. Build the project's **Development Editor** target for Win64 using your UE 5.8 toolchain.
7. Open the project and enable **Akuma's RPG Framework** if it is not already enabled.
8. For the fastest start, derive your player Blueprint from `ARPGCharacter` and your GameMode from `ARPGGameMode`.

See **`Docs/QUICK_START.md`** for the editor workflow.

## Core architecture

The framework deliberately separates three things:

1. **Definitions** — `UPrimaryDataAsset` subclasses define items, quests, classes, factions, skills, pets, bosses, recipes, vendors, mounts, build pieces and dungeons.
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
- `Docs/FEATURE_MATRIX.md` — what each system currently provides.
- `Docs/ASSET_MANAGER.md` — required Primary Asset registration.
- `Docs/BUILDING_CRAFTING.md` — building, faction ownership, storage and furnaces.
- `Docs/NETWORK_AND_AUTH.md` — login, saves and direct-IP networking boundaries.
- `Docs/VALIDATION.md` — validation performed on this source package.

