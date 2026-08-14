# Quick Start — Akuma's RPG Framework



## Definition assets: create Data Assets, not Blueprint Classes

Framework definition types (`ARPGClassDefinition`, `ARPGItemDefinition`, `ARPGQuestDefinition`, `ARPGFactionDefinition`, etc.) are native Primary Data Asset types. Create them with **Content Browser > Miscellaneous > Data Asset**, then choose the requested `ARPG...Definition` class.

Do not make a normal Blueprint Class just to hold definition values. Version 1.1 marks the shared definition base `NotBlueprintable` so the editor workflow is harder to misuse while keeping the definition objects readable in Blueprint variables and pickers.

## Quick Access — number keys without manual Equip graphs

The ready `ARPGCharacter` now includes `QuickAccess`. Slots are 1-based, so player input can be wired directly:

```text
1 Pressed -> Quick Access Pressed(1)
2 Pressed -> Quick Access Pressed(2)
D-pad Right -> Quick Access Next
D-pad Left -> Quick Access Previous
```

For a starter weapon/tool, set its Inventory **Starting Item -> Quick Access Slot** to `1` (and optionally `Equip On Spawn`). For food/potions, enable **Usable** on the Item Definition and configure vital restoration or an optional Gameplay Effect. See `Docs/QUICK_ACCESS.md`.

## Combat — player in four input calls

Assign an `ARPGClassDefinition` to the Class Component. Put melee attacks in the Class Definition's `Animation Set -> Melee Attacks` array in combo order, then use:

```text
Attack Pressed -> Basic Attack
Dodge Pressed -> Dodge (Auto)
Block Pressed -> Block Pressed
Block Released -> Block Released
```

The Class Definition `Combat Profile` controls attack type, damage/range/timing, ranged projectile or hitscan behavior, dodge montages/windows and shield block/parry/guard-break behavior. Leaving `Detailed Combo Steps` empty uses the montage array as the automatic combo. See `Docs/COMBAT.md`.

For an automatic NPC, use a Blueprint child of `ARPGAICharacter`, assign Class Definition + Faction, put it on NavMesh, and author hostile faction relationships. The included AI Combat component can acquire, chase, attack, dodge, block and use configured GAS ability tags without a Behavior Tree. `ARPGAICharacter` also ragdolls automatically on death; give the Skeletal Mesh a Physics Asset. If ragdoll is unavailable, its configured Death montage is used automatically as the fallback.

As of v1.7, NPC self-defence does not depend on perfect faction authoring: `AICombat -> Retaliate When Attacked` is enabled by default, neutral/missing faction attackers can temporarily become valid hostile targets, and nearby allies can assist automatically. See `Docs/AI_AGGRO_ASSIST.md`.

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

## 2A. Complete JRPG Stats panel

Every `ARPGCharacter` now owns an inherited **StatsUI** component. The supplied panel works without creating a Widget Blueprint. Bind one input action on the player to:

```text
Toggle Stats UI
```

The panel opens with Character Name, Level/XP, Health/Mana/Stamina, all JRPG primary/derived stats and Attribute Point `+` buttons, and includes its own **Close** button. To use custom art, create a Widget Blueprint derived from `ARPGStatsPanelWidget` and select it under `StatsUI -> Stats Widget Class`. See `Docs/JRPG_STATS_UI.md`.

## 2B. Ready Inventory + Quick Access UI

Every `ARPGCharacter` also owns an inherited **InventoryUI** component. The Quick Access HUD auto-creates for the local player by default. Bind one Inventory input action to:

```text
Toggle Inventory UI
```

The supplied native Inventory panel and hotbar work without Widget Blueprints. Drag an Inventory item onto a Quick Access slot to assign it, drag hotbar slots onto one another to rearrange them, and drag a Quick Access item back over the Inventory panel or away from the hotbar to clear it. Clearing the active held weapon/tool through this UI also unequips it while leaving it in Inventory. Right-click an equippable Inventory item to equip/unequip it.

For custom art, derive Widget Blueprints from `ARPGInventoryPanelWidget`, `ARPGQuickAccessBarWidget` and/or `ARPGInventoryItemSlotWidget`, then select them on the inherited `InventoryUI` component. See `Docs/INVENTORY_UI.md`.

## 2C. Player Crafting + Equipment Durability/Repair

Select the inherited **Crafting** component on your player Blueprint and add recipe Data Assets to **Player Recipes**. Open the existing Inventory UI and select the **CRAFTING & REPAIR** top tab, or bind input directly to:

```text
Open Crafting UI
```

For a Stone Axe recipe, create `ARPGRecipeDefinition`, set Inputs to Wood x2 and Stone x3, Output to Stone Axe x1, and leave Required Station Tag empty for personal crafting. For the Stone Axe Item Definition enable **Uses Durability** and **Lose Durability On Gathering Hit**, set Max Durability/wear, then configure repair materials under `Durability -> Repair`. For a sword, enable **Lose Durability On Combat Hit**.

Durability is stored on the exact runtime Inventory item instance. Broken equipment cannot equip and can automatically unequip when it reaches zero. See `Docs/CRAFTING_DURABILITY_REPAIR.md`.

## 2D. Settlement Building + Storage + Furnace

Every `AARPGCharacter` owns inherited **Building** and **BuildingUI** components. Create `ARPGBuildPieceDefinition` Data Assets, assign a Build Mesh + resource Build Cost, then add them to `Building -> Build Catalog`. Leave Actor Class empty for standard pieces; the framework automatically selects native Door, Storage and Production actors when appropriate.

Recommended input calls:

```text
Build Menu -> Toggle Build Menu UI
Place -> Confirm Build Placement
Rotate -> Rotate Build Placement (1)
Next/Previous -> Next Build Piece / Previous Build Piece
Cancel -> Cancel Build Placement
Interact -> Interact Built Structure
Demolish -> Demolish Built Structure
```

Keep **Allow Unlisted Build Requests** disabled and **Require Snap Target Modification Access** enabled for the normal multiplayer-safe defaults. `Demolish Built Structure` only succeeds for a modifiable runtime build and uses the Build Piece Definition's configured refund.


A good structural smoke-test order for v2.15.23 is:

```text
Foundation
  -> Wall / Doorway
  -> Door
  -> upper Floor
  -> upper Wall / Doorway
  -> next Floor
```

For a `300 × 300 × 18` upper-floor mesh, use `Placement Bounds = 150,150,9`, keep `Placement Offset = 0,0,0`, and copy `Snap Size` / `Standard Wall Height` from the existing modular kit. Upper Walls use the slab bottom/story plane, so do not add a manual Z lift for Floor thickness.

Doors and Windows are hosted inserts. A Door already placed in a Doorway must not prevent a later valid upper Floor seam around that host. On connected Foundation/Floor footprints, perimeter Wall-family facing is resolved from occupied cells and the owning support's native Wall socket; shared interior edges preserve the established/selected native facing.


For a furnace, tag Wood with `Item.Fuel.Wood`, create a station-only recipe such as Metal Ore x2 -> Metal Ingot x1 with Required Station Tag `Station.Furnace`, enable fuel consumption, add that recipe to an `ARPGCraftingStationDefinition`, then assign the Station Definition to a `Production` Build Piece. The built station's ready UI handles Player/Input+Fuel/Recipes/Output transfer and queue progress. See `Docs/BUILDING_CRAFTING.md`.

## 3. Use the supplied GameMode/Controller/GameState

For the complete default flow, derive project classes from:

- `ARPGGameMode`
- `ARPGGameState`
- `ARPGPlayerController`

This gives you the persistent-world hooks and unified chat routing used by the framework.

## 3A. Add the automatic day / night cycle

Place **`ARPG Day Night Cycle`** once in the persistent level. For the fastest setup, leave **Use Built-In Lighting Rig** enabled and remove/disable duplicate project Sun / Sky Light / Sky Atmosphere actors.

Default behavior is **Host System Clock**: in standalone/listen-server play the world follows the host PC's local time; connected clients follow the replicated host clock. No Level Blueprint Tick graph is required.

Global Blueprint-pure nodes are available anywhere with world context:

```text
Is Day
Is Night
Get World Hour
Get World Date Time
Get Day Night Phase
Get Daylight Amount
```

Bind the placed actor's `On Dawn Started`, `On Day Started`, `On Dusk Started`, `On Night Started`, `On Phase Changed` or `On Hour Changed` delegates when event-driven gameplay is preferred.

For testing, change **Time Source** to Fixed Time or Simulated Clock. See `Docs/DAY_NIGHT.md`.

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

## 5A. Woodcutting — create a tree in minutes

The ready `ARPGCharacter` already contains `ARPGWoodcuttingComponent`.

1. Create an `ARPGItemDefinition` for the logs, for example `AshLogs`.
2. Create a Blueprint child of `ARPGTree`.
3. In `Tree Mesh Variations`, add one or more tree Static Mesh assets.
4. Set `Minimum Mesh Scale` / `Maximum Mesh Scale` if you want each placed tree to receive a replicated random size (0.90-1.10 by default).
5. Assign an optional Stump Mesh.
6. Set `Wood Item` to the log Item Definition and choose Min/Max Wood Quantity.
7. Tune Required Woodcutting Level, health/resistance, fall and respawn settings.
8. Place the tree. The existing `Start Woodcutting From View` input gives automatic repeated chopping.

For axe progression, put `Item.Tool.Axe` in an equipped Item Definition's `Gathering Tool Tags`, then set its `Gathering Power` and `Gathering Tool Tier`. Trees can optionally require that tool tag and a minimum tier. **Basic Attack Auto Chops Trees** is enabled by default on the Woodcutting component: with an axe equipped and no real combat/lock-on target, your existing `Basic Attack` input automatically performs one chop on the tree in view.

The same Wood Item can be put directly into a Build Piece Definition's `Build Cost`, so chopped logs immediately work as building resources. See `Docs/WOODCUTTING.md`.

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

### Spawner performance / distance population

Distance population streaming is enabled by default on `ARPGAISpawner`. Recommended starting values are:

- `Enable Distance Based Population` = true
- `Auto Spawn When Player Is Near` = true
- `Spawn Activation Radius` = 6000
- `Despawn Radius` = 8000
- `Population Check Interval` = 1.25 seconds
- `Distance Despawn Delay` = 3 seconds
- `Keep Loaded Near Spawned Pawns` = true

An inactive spawner creates its group when a player enters the activation radius. After activation, the larger despawn radius and delay prevent edge thrashing. Spline/free-roam NPCs also count as relevance anchors by default so a player following an NPC away from its spawner does not watch it vanish. Distance unload is not treated as death and preserves normal respawn semantics. See `Docs/AI_SPAWNER_PERFORMANCE.md`.

### AI spline patrol / travel

`ARPGAICharacter` already owns **AI Spline Movement**. For a placed patrol NPC:

1. Place `ARPG AI Spline Route`.
2. Edit the route spline over NavMesh.
3. Select the NPC instance -> `AI Spline Movement` -> assign `Route`.
4. Leave `Enabled` + `Auto Start` on.

The NPC follows the route through Navigation; it is not attached to the spline. Combat automatically suspends the route, uses the route departure point as the chase leash anchor, and rejoins after combat. `ARPGAISpawner` can also assign `Assigned Spline Route` automatically to spawned NPCs. See `Docs/AI_SPLINE.md`.

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

## Player lock-on targeting

`ARPGCharacter` already owns an `ARPGTargetingComponent`. You do **not** create the marker widget or find targets manually.

In the player Character Blueprint, select **Targeting** and set any presentation overrides you want:

- `Target Marker Texture` or `Target Marker Material`
- `Target Marker Color`
- marker size and height offset
- optional target/marker socket names
- acquire animation duration and pulse settings
- acquire/maintain distance and LOS rules
- facing interpolation speed

Then bind input:

```text
Lock-On Pressed -> Toggle Lock On
Optional Previous Target -> Target Left
Optional Next Target -> Target Right
```

The existing `Basic Attack` Character function automatically uses the locked target. GAS ability activation automatically requests facing as well. For the cleanest target-aware ability Blueprints, create the Gameplay Ability with parent class `ARPGGameplayAbility`, select its lock-on policy, and use `Get Lock On Target` or `Make Lock On Target Data`.

The default targeting filter is hostile factions. Make sure the Player faction and enemy faction relationship is negative, or disable `Only Hostile Targets` for a different project rule.

## v1.8 group-combat defaults

Normal `ARPGAICharacter` NPCs require no extra Blueprint graph for coordinated melee combat. Leave `Enable Group Combat Coordination` ON and the default `Max Simultaneous Melee Attackers = 3`: when a crowd shares one target, up to three melee NPCs commit while the rest spread/orbit around the target through NavMesh and rotate into openings.

For passive retaliation creatures, also leave `Restore Original Disposition After Target Death` and `Clear Threat Against Dead Targets` ON. If a neutral chicken kills a player who attacked it, the temporary aggression is forgotten immediately; the respawned player is evaluated from the chicken's original faction/fallback settings again.

See `Docs/GROUP_COMBAT.md`.

## v1.5 spawner movement modes

For `ARPGAISpawner`, choose `Movement Mode`:

- **Automatic**: uses Assigned Spline Route when present.
- **Spline Route**: forces route travel. Route actor `Loop Route` is enabled by default.
- **Free Roam**: enables the NPC's AI Wanderer and selects reachable NavMesh destinations inside `Free Roam Radius`.
- **No Automatic Travel**: leaves idle travel to your own logic.

`Stay Together` is now independent from spawn grouping. Turn it OFF to keep the group count/respawn semantics while allowing all members to roam independently. Turn it ON for group-leader cohesion recovery. Spline groups also synchronize route direction by default so they do not split and run both ways at an endpoint. See `Docs/AI_SPAWNER_MOVEMENT.md`.

## v2.1 starting items + visible equipment

On an `ARPGCharacter` Blueprint select the inherited **Inventory** component. Do not edit `Runtime Items`; it is deliberately read-only replicated/save state. Add Item Definition assets under **Starting Items** instead. `Quantity` and `Equip On Spawn` are exposed per entry.

For a visible axe, configure `DA_StoneAxe` with `Equippable`, a valid Equipment Slot, `Item.Tool.Axe`, `Equipped Static Mesh = SM_Stoneaxe`, the character's hand/weapon `Attach Socket`, and optional relative transform/audio. The Equipment component creates the held visual automatically.
