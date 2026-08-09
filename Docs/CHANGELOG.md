# Changelog

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
