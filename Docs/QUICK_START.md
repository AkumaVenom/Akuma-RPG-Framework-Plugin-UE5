# Quick Start — Akuma's RPG Framework

## 1. Install and compile

Copy the plugin to `YourProject/Plugins/AkumasRPGFramework`, register its Primary Asset types, regenerate project files, then compile the Development Editor target with UE 5.8.

## 2. Create the player

Create a Blueprint derived from **`ARPGCharacter`**. The base character already owns the framework's primary RPG components.

Recommended first edits:

- Set skeletal mesh and animation Blueprint.
- Assign a class definition through `ARPGClassComponent`.
- Keep the default faction as `Player`, or assign another faction ID.
- Tune inventory size, respawn delay and skill/character caps under **Project Settings > Game > Akuma's RPG Framework**.
- Bind your UMG widgets to inventory, quests, stats, chat, battle pets, skills and interaction delegates.

## 3. Use the supplied GameMode/Controller/GameState

For the complete default flow, derive project classes from:

- `ARPGGameMode`
- `ARPGGameState`
- `ARPGPlayerController`

This gives you the persistent-world hooks and unified chat routing used by the framework.

## 4. Create content as Data Assets

Create Data Assets for the systems you use:

- Item Definition
- Class Definition
- Quest Definition
- Skill Definition
- Slayer Master Definition
- Faction Definition
- Vendor Definition
- Loot Table Definition
- Battle Pet Definition
- Boss Definition
- Dungeon Definition
- Recipe Definition
- Crafting Station Definition
- Build Piece Definition
- Mount Definition

Give every definition a stable, unique `DefinitionId`. Treat that ID as save-game API: do not casually rename IDs after shipping saves.

## 5. Items and inventory

Create `ARPGItemDefinition` assets, configure stack limits/value/tags, and optionally equipment slot, level/class requirements and equipped Gameplay Effect.

Use `ARPGInventoryComponent` for authoritative inventory state and `ARPGEquipmentComponent` for validated equip/unequip requests.

## 6. Quests

Create an `ARPGQuestDefinition` with objectives and rewards. Add `ARPGQuestGiverComponent` to an NPC and place the quest asset in its `Quests` array.

From the player UI/interact input call the player's `ARPGInteractionComponent`:

- `Accept Quest`
- `Turn In Quest`

Kill/loot/craft/build/skill/reputation/pet/dungeon events can be routed through `ARPGEventRouterComponent` so objectives update automatically.

## 7. Slayer

Create a `Skill Definition` with `DefinitionId = Slayer` and your desired XP curve.

Create `ARPGSlayerMasterDefinition` assets with weighted task options. Add `ARPGSlayerMasterComponent` to Slayer-master NPCs and assign the definition.

Use the player interaction component's `Request Slayer Task` / `Cancel Slayer Task` nodes. Eligible kills should call `Report Kill` with the killed creature's Slayer category gameplay tag.

## 8. Factions and reputation

Create faction definitions and assign `ARPGFactionComponent` to relevant NPCs/AI. The ready player defaults to faction ID `Player`.

Use `ARPGFactionOwnershipComponent` on interactable/faction-owned world actors. Player-placed build pieces automatically receive both character ownership and faction ownership when configured to inherit builder faction.

## 9. Vendors

Add `ARPGVendorComponent` to an NPC and assign a Vendor Definition.

The player should use `ARPGInteractionComponent` to buy, sell and buy back items. This keeps the RPC on the player-owned actor/component and leaves the vendor server authoritative.

## 10. AI spawner

Place `ARPGAISpawner` in the world.

Configure:

- weighted spawn entries
- exact or min/max group size
- spawn radius/shape
- NavMesh projection
- respawn mode/delay
- `Stay In Range`
- leash/home distance

Use a `NavMeshBoundsVolume` over areas where AI should navigate.

## 11. Bosses and dungeons

Add `ARPGBossComponent` to a boss and assign a Boss Definition. Configure boss type, phases, enrage, leash, scaling and world-respawn options.

Place `ARPGDungeonManager` in a dungeon/raid map and assign a Dungeon Definition. Encounter state is server authoritative and can be persisted by the world save system.

## 12. Battle pets

Create Battle Pet Definitions and give the player `ARPGBattlePetComponent`/`ARPGBattlePetBattleComponent` (already present on `ARPGCharacter`).

Use collection/team APIs for owned pets and battle APIs for wild encounters, abilities, swaps and capture attempts.

## 13. Building

Create a Build Piece Definition for each foundation, wall, floor, roof, door, station, chest, etc.

Assign:

- actor class
- material/resource cost
- snap/grid values
- placement bounds
- support requirement
- health
- faction/reputation restrictions
- faction access/damage policy

The player uses `ARPGBuildingComponent.RequestPlacePiece`.

For chests/stations, use classes derived from `ARPGStorageActor` / `ARPGCraftingStationActor`, which already inherit the persistent build-piece actor.

## 14. Furnace / production station

Create a Crafting Station Definition. For a furnace-style station:

- enable `Use Station Inventory For Inputs` if ore should be deposited into the station first
- keep `Fuel Comes From Station Inventory` enabled
- give fuel items a gameplay tag such as `Item.Fuel.Wood` or `Item.Fuel.Coal`
- set each smelting recipe's `Consumes Fuel`, `Fuel Tag`, `Fuel Per Craft`, craft seconds and output ingots

Crafted outputs go to the station Output Inventory. Recipe completion can award skill XP and route craft quest progress.

## 15. Mounts

Create an `ARPGMountDefinition` with its mount pawn class and movement/usage settings. Unlock and summon through the player's `ARPGMountComponent`.

## 16. Saving

For single-player, enable auto-load/auto-save in Project Settings. The ready character's persistence component resolves account/character slots and the GameMode can load/save runtime player-built world state.

Always provide an explicit menu/save-point save before a controlled application exit. Periodic autosave remains useful as crash-loss protection.

## 17. Login and direct IP

Use the Account Subsystem for local profile creation/login.

Use the Network Subsystem for:

- `Host Listen Server`
- `Join By IP`
- `Disconnect To Map`

Example LAN address: `192.168.1.25:7777`.

For public Internet games, replace local-profile trust with a real identity/backend provider and account for firewall/NAT/session discovery requirements.
