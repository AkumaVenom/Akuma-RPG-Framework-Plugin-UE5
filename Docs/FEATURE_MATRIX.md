# Feature Matrix — v1.8 Group Combat Polish

Legend:

- **Implemented** — C++ runtime path exists in this package.
- **Foundation** — core runtime/data hooks exist; project-specific content/UI/online service is still required.

| Area | Status | Included in this source build |
|---|---|---|
| Ready RPG character | Implemented | Main components preinstalled on `AARPGCharacter`. |
| GAS integration | Implemented | Ability-system component/attribute set/ability bridge, `ARPGGameplayAbility` lock-on targeting policy/TargetData helpers and effect-friendly data hooks. |
| Automatic action-RPG combat | Implemented | Class-driven melee/ranged/magic basic attacks, ordered montage combos, timed sphere traces/hitscan/projectiles, damage/crit/armor, dodge, block/parry/guard break, critical-hit stagger/knockback, Niagara/Cascade impact FX, exposed combat audio, hit reactions and combat state tags. |
| Player lock-on targeting | Implemented | `ARPGTargetingComponent` is preinstalled on `ARPGCharacter`: toggle lock, camera-centered hostile acquisition, continuous Z-target camera/control-rotation tracking, continuous player facing/strafe mode, left/right switching, LOS/range validation, animated marker, server target validation and GAS target-data helpers. |
| Classes | Implemented | Data-driven class definition and class component. |
| Inventory | Implemented | Stacking, add/remove/transfer, save/replication and server authority. |
| Equipment | Implemented | Slot validation, level/class requirements, Gameplay Effect application/removal. |
| Loot/currency | Implemented | Loot tables, item/currency/XP rewards, currency balances. |
| Quests | Implemented | Prereqs, objectives, chains via prereqs, rewards, repeatable, auto-complete, persistence. |
| Quest giver NPC | Implemented | `ARPGQuestGiverComponent` plus player-owned RPC interaction route. |
| Generic skills | Implemented | Per-skill XP/level, custom XP curve, unlock metadata and persistence. |
| Slayer | Implemented | Master definitions, weighted tasks, skill/combat requirements, kill count, XP, points/streaks, cancellation and persistence. |
| Factions/reputation | Implemented | NPC/player faction IDs, relationship/reputation checks, attack-on-sight rules, automatic damage retaliation/ally assist fallbacks, and persistence. |
| Player faction/building ownership | Implemented | Player-built structures inherit character/account/faction identity. |
| Vendors | Implemented | Buy/sell, finite/unlimited stock, restock, quest/faction/level gates, discounts and buyback. |
| Vendor services | Foundation | BlueprintNativeEvent service hook for repair/trainer/project-specific services. |
| Automatic NPC combat AI | Implemented | `ARPGAICharacter`/`ARPGAICombatComponent` auto-acquire/retaliate, use threat, chase, face, attack, dodge/block, optionally activate GAS ability tags, immediately restore temporary-retaliation targets to original disposition after death, and coordinate allied melee attack slots/wait-orbit movement without requiring a Behavior Tree. |
| Automatic NPC ragdoll death | Implemented | `ARPGAICharacter` ragdolls by default with capsule/collision handling, inherited velocity, killing-hit impulse, multiplayer death presentation, respawn reset and automatic Death montage fallback when physics is unavailable. |
| AI spline patrol / travel | Implemented | `ARPGAISplineRoute` + inherited `ARPGAISplineComponent`: NavMesh look-ahead following without attachment, route-level Loop/closed/open-end behavior, group direction synchronization, route-point waits/events, Route Id lookup, spawner assignment, combat suspension/leash/rejoin and Wanderer conflict handling. |
| Advanced AI spawner | Implemented | Weighted group spawn, count range, shape/radius, NavMesh projection, independent group-vs-cohesion semantics, synchronized spline direction, selectable Automatic/Spline/Free-Roam/Hold movement, spawner-leashed reachable-point roaming, combat-home leash integration, dead-pawn corpse cleanup and respawn. |
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
| Character saves | Implemented | Character identity/state, vitals, progression, inventory/equipment, quests, skills, Slayer, reputation, currencies, pets, group and mounts. |
| World saves | Implemented | Runtime player buildings, ownership, health/upgrades, storage, crafting queues/output and dungeon progress. |
| Building | Implemented | Snapping, validation, cost consumption, support/collision, health, repair/demolish and faction rules. |
| Build preview UI/material | Foundation | Placement evaluation API is exposed; project supplies preferred ghost mesh/material UX. |
| Storage/chests | Implemented | Shared inventory model, faction-aware access and persistent contents. |
| Crafting/production | Implemented | Recipes, queues, station/player inputs, fuel tags, outputs, skill XP, quest events and offline elapsed processing. |
| Mounts | Implemented | Unlock, summon/ride, movement capability flags, animation hooks and save state. |
| Party/raid groups | Foundation | Replicated group membership/role/subgroup state and chat integration; full matchmaking/social backend remains project-level. |
| UMG/UI skin | Foundation | All key data/delegates are Blueprint exposed; visual UI assets are intentionally supplied by the game. |
