# Akuma's RPG Framework — New Chat Handoff Context

## Project identity
- Plugin display name: **Akuma's RPG Framework**
- Plugin folder: `AkumasRPGFramework`
- Main runtime module: `AkumasRPGFramework`
- Blueprint/C++ naming prefix: `ARPG`
- Target engine: **Unreal Engine 5.8 / 5.8.1**
- User priority: **single-player first**, but architecture must be **multiplayer-ready/server-authoritative** wherever relevant.
- Core product goal: a very polished, AAA-style, easy-to-use RPG/MMO foundation that is heavily Blueprint exposed, data driven, and mostly automatic once configured in-editor.

## Core architecture decisions
- Use Unreal Gameplay Ability System (GAS) underneath the easy Blueprint-facing RPG API.
- GameplayAbilities is the engine plugin dependency; `GameplayTags` and `GameplayTasks` are Build.cs modules, NOT `.uplugin` plugin dependencies.
- Designer definitions live in Data Assets / Primary Assets; mutable runtime state lives in replicated/saveable structs/components.
- Stable IDs / Primary Asset IDs / GUIDs are used for saves instead of fragile UObject pointers.
- Replicated/server-authoritative components own runtime state where multiplayer matters.
- Save system should use Unreal asynchronous SaveGame APIs and restore the complete character/world state with minimal manual Blueprint wiring.
- Do not duplicate RPG state across competing systems. GAS owns ability/effect state; ARPG components expose simple RPG functions/events.
- Ready-made `AARPGCharacter` plus modular components for existing character Blueprints.

## Required major systems

### Combat / character RPG core
- Melee, ranged, and magic combat.
- Attack, hit, damage, death, resurrection/respawn.
- Extensive animation montage hooks for attacks, casting, hit reactions, death, mount/dismount, gathering, etc.
- Stats, derived stats, resources, leveling, XP, classes/archetypes, class/spec hooks, abilities, cooldowns.
- Buffs/debuffs, DoTs/HoTs, shields, crowd control, Gameplay Tags/Effects integration.
- Threat/aggro system including taunts, threat modifiers/drops and boss target selectors.
- Loot, currencies, inventory, equipment, durability/repair hooks.

### Quests
- First-class WoW-style quest system.
- Quest chains, prerequisites, objectives, rewards, repeatables/dailies, auto accept/complete options, quest tracking.
- NPC quest giver state/components.
- Persistent objective counters, completion/turn-in state, timestamps/repeat metadata, tracked quests, chain progression.
- Quest definitions stay data driven; runtime state saves separately.
- Quest objectives should update automatically from combat, item acquisition, Slayer, dungeons, bosses, pets, skills, etc.

### Saving / loading / profiles / login
- Very easy automatic save/load.
- Save character identity, transform, vitals/resources, level/XP, class, inventory/equipment, quests/objectives, skills, Slayer, reputation, currencies, battle pets, mounts, groups, placed buildings, containers, crafting queues, dungeon/raid state where appropriate.
- Account/profile system above save layer.
- Simple local username/password entry for single-player profiles.
- NEVER store raw/plaintext passwords; local profile uses salted password verifier/hash plus account GUID.
- Real multiplayer authentication is a backend/provider boundary; expose provider hooks rather than pretending local files are secure Internet auth.
- Account can own multiple characters.
- Configurable account-wide vs character-specific data (battle pets/achievements/currencies etc.).
- Remember last character so login can automatically resolve the correct save.

### Multiplayer / networking
- Direct hosting/joining for LAN and Internet.
- Blueprint/UI exposed host controls, IP/hostname + port entry, Join By IP/address, LAN discovery hooks, connection status/errors, leave session.
- Listen-server hosting; dedicated-server hooks.
- Direct Internet hosting may require NAT/firewall/port-forwarding; future Steam/EOS/session provider integration should fit without rewriting RPG systems.
- Server-authoritative transactions, combat state, spawning, capture, building placement, item movement, etc.

### Vendors
- Data-driven vendor component/definitions.
- Buy/sell, limited/unlimited stock, timed restock, multiple currencies, level/quest/faction/reputation restrictions, buyback history, discounts/markups, rotating/special stock.
- Repair/trainer/service hooks.
- All prices/requirements validated server-side.
- Player-owned interaction component routes vendor RPC requests because vendor world actors may not be client-owned.

### Unified WoW-style chat/event box
- One filterable chat/event log for players, NPCs, system/events.
- Channels: World/Global, Zone, Local/Say, Yell, Whisper, Party, Raid, Guild/group hooks, Faction, System/custom.
- NPC Say/Yell/Whisper/scripted dialogue.
- Boss/dungeon/world announcements.
- Quest updates, loot/currency/reputation notices, join/leave/group events, optional combat notifications.
- Tabs/filters, timestamps, message styles/colors/prefixes, history limits, fade/scroll/wrap, moderation/profanity/mute/block hooks.
- Do NOT replicate private/party messages blindly to all clients; server routes only to eligible receivers.
- Optional simulated wanderer/NPC chat for single-player world ambience.

### Factions / reputation
- First-class faction component for all NPCs, creatures, bosses, wanderers, pets/companions, vendors, quest givers, etc.
- Primary + optional secondary affiliations.
- Faction relationship table and Friendly/Neutral/Unfriendly/Hostile/Hated style relationships.
- Player reputation tiers configurable (WoW-like defaults acceptable).
- Factions automatically affect combat permission, aggro/threat, guards, vendors/prices, quests, dialogue, dungeons/raids, wanderers, settlements, building access, etc.
- Temporary faction overrides for disguises, mind control, quests, PvP, transformations.
- Dedicated configurable **Player faction** (`Faction.Player` concept).
- Player can later join/replace with another faction if the game wants.

### Building factions / ownership
- Player-built structures inherit BOTH owning Player/Character/Account ID and owning faction by default.
- Building faction rules govern use, doors, storage, crafting, repair, upgrade, demolition, damage, guards/turrets, territory and capture.
- Same-faction/allied/neutral/hostile access policies plus per-building overrides.
- Faction territory volumes can restrict who can build and require reputation.
- Optional faction capture/ownership transfer for forts/bases/towers/settlements.

### Advanced AI spawner
- `ARPG_AI_Spawner` style class/component.
- Select AI class/data definition.
- Exact spawn count or min/max group size.
- Weighted spawn entries (e.g. grunt/archer/elite percentages).
- Spawn radius/shape, formations/spread, respawn timing, max alive, initial delay, individual/group respawn.
- Optional `Stay In Range` / leash/home anchor behavior.
- Max roam distance, combat leash distance, return/reset behavior.
- Group cohesion/linked aggro optional.
- Spawner provides home location, spawn group ID, spawner ID, encounter affiliation, faction, level/scaling context.
- Dungeon/raid integration: spawn on encounter, reset on wipe, no respawn after boss, instance reset, etc.
- Server owns spawning/despawning/leash decisions.

### Autonomous wanderer/playerbot-style AI
- Optional MMO-like adventurer population inspired by SPPNXT wanderers.
- AI adventurers can roam zones, travel, fight, rest, visit towns/vendors/inns, equip upgrades, level, use classes/abilities, react to danger, assist allies and optionally group.
- Configurable activity profiles (questing, hunting, town/social, dungeon seeker, etc.).
- World wanderers + lightweight simulated/off-screen wanderers to avoid full AI cost.
- Persistent named wanderers with GUID/name/class/equipment/level/history.
- Can respawn at settlements/graveyards.
- Distance-based AI LOD, reduced think rate, dormancy/sleep and population budgets.
- Optional chat participation.

### Bosses
- First-class boss component/encounter framework.
- Boss archetypes: rare/named, elite, world boss, dungeon boss, raid boss, custom.
- Bosses use same stats/abilities/factions/threat/loot/quests/achievements/save foundation as normal RPG actors.
- Health/timed phase transitions, enrage/soft enrage, adds, hazards, interruptible casts, telegraphs, invulnerability, scripted sequences, target selectors.
- Multi-boss encounters, council/twins, linked/shared health, sequential activation.
- Reusable Boss Mechanics Data Assets.
- World boss respawn windows, announcements, contribution tracking, minimum eligibility, personal loot, scaling and leash/reset.
- Dungeon bosses integrate with instance state/checkpoints/wipes/lockouts.
- Raid bosses support roles, raid resources, battle-res limits, difficulty mechanics.
- Runtime debug panel/hooks for phase, threat, mechanics, timers, enrage, scaling.

### Dungeons / raids
- Data Asset definitions: map/level, player limit, level/item requirements, difficulties, bosses, encounters, checkpoints, rewards, lockouts.
- Parties/raid groups, leadership/assistants/subgroups, ready checks, roles, group markers, loot rules.
- Instance lifecycle: create/enter/leave/reconnect/reset/abandon/complete/destroy.
- Encounter state machine: NotStarted/InProgress/Failed/Completed.
- Checkpoints, wipe detection/reset, encounter boundaries, trash packs, boss reset.
- Normal/Heroic/Mythic-style custom difficulties, optional group scaling.
- Dungeon objectives, quests, personal/group loot, first-kill/completion rewards, achievements, currencies.
- LFG/LFR/matchmaking hooks and premade/manual groups.
- Entrance portals/instance gates and resurrection/checkpoint rules.
- Timed/mythic-plus-like hooks and affix/modifier extensibility.
- Save/lockout/reconnect state where appropriate.

### Battle pets
- First-class WoW-like collectible battle pet subsystem.
- Pet journal/collection, discovered/owned/captured/favorites/custom names/species/variant/level/XP/quality.
- Wild encounter spawns, level/family/rarity tables, spawn/respawn rules.
- Capture items/traps and configurable capture formula (health/rarity modifiers).
- Turn-based pet battles: teams, swaps, speed/initiative, hit/miss/crit, cooldowns, multi-turn abilities, interrupts/pass.
- Data-driven pet abilities: damage/heal/shield/buffs/debuffs/DoT/HoT/stun/root/weather/field/summon/conditions.
- Families/types with configurable strengths/weaknesses/passives.
- XP/levels/stat growth/ability unlocks/quality upgrades.
- Saved teams/loadouts, trainer NPCs, battle AI, victory/defeat/surrender/revival/rewards.
- Quest/achievement hooks.
- Server-authoritative battle/capture validation.

### Skills / professions
- Generic extensible skill system for RuneScape-like progression.
- Skills such as Woodcutting, Mining, Fishing, Cooking, Smithing, Prayer/Bone Offering, Gathering, Crafting, Survival, etc.
- Per-skill XP/level, configurable XP curves/caps/unlocks/perks, tool/resource requirements, skill checks, timed actions, nodes, rewards.
- Gathering node component/actors with depletion, respawn, use counts, shared vs individual multiplayer state, rare drops.
- Actions can consume items and award skill XP (e.g. bury bones -> Prayer XP).
- Unlocks can grant Gameplay Tags, recipes, abilities, stats or Blueprint events.
- Inventory/equipment/stats/abilities/quests/achievements/factions/vendors/dungeons/wanderers/save integration.

### First implemented skill: Slayer
- Initial reference skill should be RuneScape-style **Slayer**.
- Slayer Masters / assignment providers.
- Data-driven monster categories/tags and task tables.
- Kill count ranges, Slayer level requirements, kill credit, XP, task completion.
- Rerolls/cancellations, blocks/preferences, streaks, Slayer points, reward-shop hooks, unlocks, elite/boss tasks.
- Multiplayer kill-credit policy: player damage, party proximity, contribution threshold, etc.
- Save assigned master/task ID/progress/streak/points/Slayer XP.
- Implement through generic skill framework, not as a hard-coded special case.

### Palworld-style building / construction
- Modular construction system for user's wood/stone/metal building assets.
- Building piece Data Assets for foundations/walls/floors/roofs/stairs/doors etc.
- Placement preview, rotation, snap/grid placement, free placement, collision/support/distance validation, resource costs.
- Construction, repair, upgrade, demolish.
- Server-authoritative placement/resource consumption.
- Persistent IDs, transforms, health, upgrades, ownership and inventories.
- Ownership modes/hooks: personal, party, guild/group, faction, public.

### Crafting / stations / furnaces / storage
- Recipe definitions with ingredients, required skill, station type, duration, outputs, XP, unlock requirements, optional by-products.
- Crafting/production queues.
- Furnace/smelting: input inventory, fuel, output inventory, tagged fuel requirements, timers, ore -> ingot workflows, VFX/audio/animation events.
- Station-held inputs, queue cancellation/refunds where appropriate.
- Chests/storage share inventory architecture; stack split, transfer all, sorting/filters/restrictions.
- Server validates container transfers and simultaneous access.
- Persistent world containers, station contents, crafting queues and fuel state.
- Offline processing configurable (pause or elapsed-real-time calculation).

### Mounts
- Mount definitions/collection/unlocks/persistence.
- Summon/dismiss, mount/dismount, rider sockets, possession/control handoff, riding movement.
- Ground/flying/aquatic/special capability flags.
- Speed/accel/jump/stamina/health/stats hooks.
- Mount/restriction rules for combat, casting, gathering, zones, dungeons, factions/reputation.
- Extensive animation/montage hooks: mount, dismount, idle, walk/run/sprint, jump/land, attack, hit, death.
- Multiplayer replication.

## Editor usability philosophy
- The framework should be drop-in and data-driven, not Blueprint spaghetti.
- Typical usage should be: add component / select Data Asset / configure exposed properties / play.
- Use player-owned Interaction Component as the client->server bridge for unowned world actors (vendor, chest, crafting station, quest giver, Slayer master etc.).
- Project Settings defaults and Asset Manager scanning should reduce manual setup.
- Blueprint events/delegates for all major state changes.

## Current implementation / release state
Latest package produced in the prior chat:
- **Version:** `v1.0.2-alpha_CompileFix`
- ZIP filename: `AkumasRPGFramework_UE5.8_v1.0.2-alpha_CompileFix.zip`
- Target: UE 5.8.1 source plugin.
- Static validation at packaging time: **66 exported Unreal classes, 113 C++ files, 6,080 source lines, 0 structural issues, 0 warnings**.
- IMPORTANT: static validation is NOT proof of successful Unreal compilation. Real UE5.8.1 compilation is the authoritative test.

## Compile history and fixes already made

### v1.0.0-alpha failure
Unreal failed before C++ compilation because `AkumasRPGFramework.uplugin` incorrectly declared `GameplayTags` as a plugin dependency.
- `GameplayTags` and `GameplayTasks` are modules, not independent plugins.
- Fix made in v1.0.1: remove invalid `.uplugin` entries and keep modules in Build.cs; keep `GameplayAbilities` plugin dependency.

### v1.0.1-alpha compile reached UHT/C++
UE5.8.1 successfully ran UHT and compiled most of the plugin. `Log(4).txt` showed remaining errors in five source areas.

### v1.0.2-alpha fixes applied
1. **ARPGAttributeSet.cpp**
   - Broken `GetLifetimeReplicatedProps` implementation used the wrong variable (`O`) while macro expected `OutLifetimeProps`.
   - Malformed macro/brace layout caused unmatched `{`, local function errors and preprocessor error.
   - Rewritten using normal UE replication pattern and RepNotify handlers.

2. **ARPGBattlePetBattleComponent.cpp**
   - Fixed variable/name collision/indirection error involving `FARPGPetAbilityCooldown CD` after a pointer of the same name existed in scope.

3. **ARPGCombatComponent.cpp**
   - Added complete type include: `GameFramework/CharacterMovementComponent.h` for calls to `DisableMovement()` / `SetMovementMode()`.

4. **ARPGGameState.cpp / ARPGPlayerController.cpp**
   - Added complete type include: `GameFramework/PlayerState.h` for `APlayerState::GetPlayerName()`.

5. **ARPGMountCharacter.cpp**
   - Renamed local `Controller` variables because UE build treats shadowing inherited `APawn::Controller` as an error (`C4458`).

## Next action in a new chat
1. User should install/extract `v1.0.2-alpha_CompileFix` as:
   `YourProject/Plugins/AkumasRPGFramework/AkumasRPGFramework.uplugin`
2. Delete project `Intermediate`; optionally delete `Binaries`, `.vs`, and plugin `Binaries/Intermediate` for a clean rebuild.
3. Regenerate Visual Studio project files.
4. Build **Development Editor / Win64** with UE5.8.1 / VS2022.
5. If build fails, user should upload the newest UBT log. Read the whole log, isolate every compile error, fix the plugin source together, increment version, repackage and revalidate.
6. Do NOT make the user manually patch individual files if we can provide a corrected ZIP.
7. Do not call the framework "fully production/AAA certified" until it actually compiles and passes runtime/PIE/network/save tests. Keep alpha labeling honest.

## User workflow preference
- User wants the plugin to be extremely polished, easy/automatic in editor, Blueprint exposed and feature-rich.
- They prefer receiving corrected complete ZIP builds instead of lengthy manual patch instructions.
- Keep all systems integrated with the same shared data, faction, save, interaction and multiplayer authority architecture.
