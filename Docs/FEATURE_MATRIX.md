# Feature Matrix — First Source Build

Legend:

- **Implemented** — C++ runtime path exists in this package.
- **Foundation** — core runtime/data hooks exist; project-specific content/UI/online service is still required.

| Area | Status | Included in this source build |
|---|---|---|
| Ready RPG character | Implemented | Main components preinstalled on `AARPGCharacter`. |
| GAS integration | Implemented | Ability-system component/attribute set/ability bridge and effect-friendly data hooks. |
| Stats/combat/death/respawn | Implemented | Replicated vitals, damage/death/respawn and montage hooks. |
| Classes | Implemented | Data-driven class definition and class component. |
| Inventory | Implemented | Stacking, add/remove/transfer, save/replication and server authority. |
| Equipment | Implemented | Slot validation, level/class requirements, Gameplay Effect application/removal. |
| Loot/currency | Implemented | Loot tables, item/currency/XP rewards, currency balances. |
| Quests | Implemented | Prereqs, objectives, chains via prereqs, rewards, repeatable, auto-complete, persistence. |
| Quest giver NPC | Implemented | `ARPGQuestGiverComponent` plus player-owned RPC interaction route. |
| Generic skills | Implemented | Per-skill XP/level, custom XP curve, unlock metadata and persistence. |
| Slayer | Implemented | Master definitions, weighted tasks, skill/combat requirements, kill count, XP, points/streaks, cancellation and persistence. |
| Factions/reputation | Implemented | NPC/player faction IDs, relationship/reputation checks and persistence. |
| Player faction/building ownership | Implemented | Player-built structures inherit character/account/faction identity. |
| Vendors | Implemented | Buy/sell, finite/unlimited stock, restock, quest/faction/level gates, discounts and buyback. |
| Vendor services | Foundation | BlueprintNativeEvent service hook for repair/trainer/project-specific services. |
| Advanced AI spawner | Implemented | Weighted group spawn, count range, shape/radius, NavMesh projection, respawn and leash/home behavior. |
| Wanderer AI | Foundation | Optional autonomous roaming/activity foundation; game-specific playerbot brains can extend it. |
| Threat/aggro | Implemented | Threat table, highest target, taunt/set/clear utilities. |
| Bosses | Implemented | Boss types, encounter state, phases, enrage, leash/reset, scaling, contributions and world respawn. |
| Dungeons/raids | Foundation | Encounter state machine, wipes, checkpoints, completion/persistence and definitions. Online instancing/matchmaking services remain project-level. |
| Battle pets | Foundation | Collection/team/XP, wild battle start, turns/abilities/swaps/capture and persistence. Extend for exact game-specific family/weather/rulesets. |
| Unified chat/event log | Implemented | Player + NPC/boss/system/quest/loot/event message types and per-client routing. UMG presentation is project-facing. |
| Local account/login | Implemented | Username/password local profiles, salted verifier, account/character association. |
| Production Internet auth | Foundation | Must be supplied by a trusted backend/platform provider; intentionally not faked by local SaveGame authentication. |
| Direct IP hosting/join | Implemented | Listen-server open-level flow, configurable port, direct ClientTravel join. |
| LAN discovery/NAT traversal | Foundation | Direct LAN IP works; service/session discovery and NAT traversal require an online subsystem/provider. |
| Character saves | Implemented | Character identity/state, vitals, progression, inventory/equipment, quests, skills, Slayer, reputation, currencies, pets, group and mounts. |
| World saves | Implemented | Runtime player buildings, ownership, health/upgrades, storage, crafting queues/output and dungeon progress. |
| Building | Implemented | Snapping, validation, cost consumption, support/collision, health, repair/demolish and faction rules. |
| Build preview UI/material | Foundation | Placement evaluation API is exposed; project supplies preferred ghost mesh/material UX. |
| Storage/chests | Implemented | Shared inventory model, faction-aware access and persistent contents. |
| Crafting/production | Implemented | Recipes, queues, station/player inputs, fuel tags, outputs, skill XP, quest events and offline elapsed processing. |
| Mounts | Implemented | Unlock, summon/ride, movement capability flags, animation hooks and save state. |
| Party/raid groups | Foundation | Replicated group membership/role/subgroup state and chat integration; full matchmaking/social backend remains project-level. |
| UMG/UI skin | Foundation | All key data/delegates are Blueprint exposed; visual UI assets are intentionally supplied by the game. |
