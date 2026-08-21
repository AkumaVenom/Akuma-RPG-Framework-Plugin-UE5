> **v2.17.0 Mining note:** derive harvestable Stone/Ore/Gem nodes from `ARPGMineableRock`, equip a real `Item.Tool.Pickaxe` runtime item, and use Basic Attack for one free strike or `InteractWorld` / `Start Mining From View` for repeated harvesting. Native Mining uses the RuneScape-style 1–99 curve by default. See `Docs/MINING.md`.

> **v2.16.10 settlement worker note:** in `ARPGSettlementDefinition -> Settlement | Woodcutting | Tool Presentation`, assign your Axe Item Definition to `Villager Woodcutting Tool Item`. Its existing equipped mesh/socket/relative transform is shown automatically while the villager travels to/chops a tree and removed when roaming/home; no villager Inventory item is required.

> **v2.16.9 tree/build note:** Foundations can be aimed/snapped/placed through `AARPGTree` resources. A build piece occupying a tree's trunk-root regeneration volume suppresses that resource automatically, hides both tree/stump collision, and prevents respawn until the final blocker is removed. No Foundation Data Asset flag is required; tune the Tree's `Respawn | Building Suppression` settings only for unusual tree sizes.

> **v2.16.8 runtime navigation note:** with Recast `Runtime Generation = Dynamic`, structural build pieces dirty/update their local navigation tiles automatically as construction completes or is removed. Stairs now use their real rasterized Recast surface only; automatic off-mesh Stair links were removed because they can compete with a correctly configured Supported Agent and cause AI oscillation. Configure the project Supported Agent to match your AI/collision profile and verify the Stair is green with **P**.

> **v2.16.3 housing validation note:** real PIE testing exposed raw actor-origin assumptions in the first settlement home validator. Foundation cells, overhead cover and Wall-family perimeter recognition now use transformed build bounds and canonical structural planes. The default minimum home is now **2x2 Foundations** and remains configurable in `ARPGSettlementDefinition`.

> **v2.16.2 compile note:** UE5.8 settlement-UI panel-state validity access is now implemented out-of-line so forward-declared widget types are complete before `IsValid()` is evaluated. The v2.16.1 UMG `Slot` name-hiding fix is retained. Settlement setup/behavior remains the v2.16.0 contract below; no Data Asset migration is required.

> **v2.16.0 note:** Settlement simulation is now an explicit opt-in system. Add a `Piece Kind = SettlementHub` with an `ARPGSettlementDefinition`, then build a complete configurable 2x2+ home and interact with a `Piece Kind = Bed` to mark it as a Villager Bed. Without a completed Hub, ordinary buildings stay ordinary and no villagers are recruited. The character's native Settlement UI provides a proximity HUD plus Hub/Bed panels and can be reskinned with Widget subclasses. See `Docs/SETTLEMENTS.md`. The confirmed v2.15.53 Stair/Wall-family and v2.15.54 buildable-light behavior remain protected. — Akuma's RPG Framework

# Quick Start

> **v2.15.54 note:** The v2.15.53 Stair/Wall-family boundary behavior is preserved unchanged. New `Piece Kind = Light` fixtures use a separate surface-placement path: stick/floor lights can seat on terrain, Foundations and Floors; wall lights can mount on either face of completed `Wall`, `WindowWall` and `Doorway`. The existing Interact button toggles replicated/persistent light + Niagara/Cascade state with smooth fading and no fuel requirement. The confirmed Wood Window remains `Window Insert Offset Z = +20`, and finished Foundation/Floor/Ceiling story planes remain `0,300,600...`. — Akuma's RPG Framework


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

## 2D. Settlement Building + Buildable Lights + Storage + Furnace

Every `AARPGCharacter` owns inherited **Building** and **BuildingUI** components. Create `ARPGBuildPieceDefinition` Data Assets, assign either the existing Static **Build Mesh** or the optional **Build Skeletal Mesh** plus a resource Build Cost, then add them to `Building -> Build Catalog`. Leave Actor Class empty for standard pieces; the framework automatically selects native Door, Window, Light, Storage and Production actors when appropriate.

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


A good structural smoke-test order for the confirmed v2.15.43 baseline is:

```text
Foundation
  -> Wall / Doorway
  -> Door
  -> upper Floor
  -> Wood Stair
  -> upper Wall / Doorway
  -> next Floor
```

For a `300 × 300 × 18` upper-floor mesh, use `Placement Bounds = 150,150,9`, keep `Placement Offset = 0,0,0`, and copy `Snap Size` / `Standard Wall Height` from the existing modular kit. Upper Walls use the slab finished top/story plane, while the 18 cm slab extends downward. Do not add a manual Z lift for Floor thickness.

Doors and Windows are hosted inserts. A Door already placed in a Doorway must not prevent a later valid upper Floor seam around that host. On connected Foundation/Floor footprints, perimeter Wall-family facing is resolved from occupied cells and the owning support's native Wall socket; shared interior edges preserve the established/selected native facing.

For a skeletal `Window`, set `Piece Kind = Window`, leave `Actor Class` empty, assign the asset to **Build Skeletal Mesh**, and normally leave **Preview Skeletal Mesh** empty so the placement ghost falls back to the same skeletal asset. You may instead assign a lightweight Static **Preview Mesh**. The framework uses the transformed active-asset bounds for the pivot-aware `WindowWall -> Window` snap and spawns the native `ARPGBuildWindowActor` collision wrapper automatically. v2.15.45 centers the suspended Window on X/Y/Z; keep the **WindowWall host's** `Window Insert Offset = 0,0,0` for a centered opening and adjust that host-local field only for deliberately raised/off-centre apertures. Door Z behavior remains bottom-aligned.

The **Valid Preview Material** and **Invalid Preview Material** remain global settings on the character's `ARPGBuildingComponent`; the Window Data Asset does not need its own ghost material. In v2.15.46 a Skeletal Window preview automatically checks/prepares those assigned materials for Unreal's Skeletal Mesh usage permutation in editor/PIE, so a previously Static-only ghost material does not silently remain grey. `WindowWall -> Window` also has an intrinsic hosted socket and a dedicated third-person acquisition path; do not add custom Window snap points or fake `Placement Offset` values just to make the host selectable.

For v2.15.48 Window interaction, assign **Window Open Animation** on the Window Data Asset and optionally assign **Window Close Animation** (leave Close empty to reverse Open). Keep **Disable Window Collision When Open** enabled for a normal passable opening. Wire the player's normal interaction input action to `Interact Built Structure`; the same call toggles Doors and Windows through server authority. If a WindowWall collision shell covers the opening, no Blueprint workaround is required: the framework resolves the Window occupying that exact host socket.

For v2.15.54 buildable lighting, use `Piece Kind = Light` and leave **Actor Class** empty so the native `ARPGBuildLightActor` is selected. For a stick/standing torch choose **Light Placement Mode = Ground / Foundation / Floor**; enable **Allow Ground Placement** if terrain placement is wanted, and the same definition can also seat on completed Foundations and Floors. For a wall torch/lantern choose **Light Placement Mode = Built Wall Surface**; placement is resolved directly against completed Wall / WindowWall / Doorway faces, with actor local **+Y pointing outward from the wall**. Use **Light Surface Offset** for a small deliberate stand-off rather than changing structural snap data.

Choose **Point** or **Spot** light, configure intensity/radius/color/temperature/shadow properties, and assign a Niagara system and/or legacy Cascade particle system under the Light FX fields. The native actor fades light intensity and optional emissive material scalar smoothly when toggled, replicates the On/Off state, and persists it in world saves. The normal `Interact Built Structure` input toggles a completed build light through server authority and consumes **no fuel or inventory item**. Build-light visuals are intentionally non-blocking so torches and lanterns cannot become hidden Stair/Wall/Floor/Window collision regressions; minimum fixture spacing is enforced semantically instead.

For the current `334 × 300 × 278` Wood Stair on the 300-unit grid, use `Piece Kind = Stair`, `Placement Bounds = 167,150,139`, `Requires Snap Target = true`, `Allow Ground Placement = false`, `Placement Offset = 0,0,0`, and the same `Snap Size = 300` / `Standard Wall Height = 300` as the structural kit. Treat the mesh as **art inside a 300 cm structural flight cell**: the 334 cm run overhangs by 17 cm per end, but Stair chains and Stair-owned landing cells advance exactly 300 cm.

For an upward Stair starting on finished surface `S`, the Stair LOW end is `S`, the rendered HIGH end is `S+278`, the next 18 cm Floor occupies `S+282..S+300`, and the next finished walking surface / next Stair LOW is exactly `S+300`. This is the v2.15.43 rule that prevents the old `+22 cm` Stair lift from accumulating on every storey. Landscape support, tiled-deck overhang seams, Stair-to-Stair chains, Stair-to-Floor/Ceiling landings and parallel Stair-side Wall/Doorway seams remain part of the standard topology. Do not scale the Stair or add manual Placement Offset/custom sockets to compensate.


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

## 5B. Mining — create a Stone/Ore/Gem node in minutes

The ready `ARPGCharacter` already contains `ARPGMiningComponent`. Native Mining uses the built-in RuneScape-style 1–99 curve by default.

1. Create a Pickaxe `ARPGItemDefinition`: Equippable, a real Equipment Slot, `Item.Tool.Pickaxe` in Gathering Tool Tags, Gathering Power and Gathering Tool Tier.
2. Create Item Definitions for Stone, Ore and/or Gems.
3. Create a Blueprint child of `ARPGMineableRock`.
4. Add one or more Static Meshes to `Rock Mesh Variations`; optional random scale and yaw are replicated automatically.
5. Set Mining Health/Resistance, Required Mining Level, Minimum Tool Tier and respawn.
6. Add common materials to `Successful Strike Drops` and/or `Depletion Drops`.
7. Add rare Gems to `Bonus Chance Drops`; each entry can gate and scale its chance by Mining level/tool tier.
8. Place the node or use the Blueprint with Unreal Actor Foliage.
9. With a valid equipped pickaxe, the existing Basic Attack performs one free Mining strike. Bind Interact to `InteractWorld` (or use `Start Mining From View`) for automatic repeated Mining.

Mining XP is saved through the generic Skills component. A project `ARPGSkillDefinition` is optional; assign one when you want custom unlock metadata or another XP curve/model. See `Docs/MINING.md` for the complete authoring, networking, bonus-drop and Actor Foliage workflow.

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
