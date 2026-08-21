# Woodcutting & Harvestable Trees — v2.1.1-alpha

Akuma's RPG Framework v2.1 retains the v2.0 first-class Woodcutting profession and adds context-sensitive axe Basic Attacks plus replicated per-tree size variation to the reusable `ARPGTree` resource actor. The normal workflow is intentionally content-first: create a Wood item Data Asset, create a Blueprint child of `ARPGTree`, fill its tree-mesh variation array, assign the Wood item, and place the tree.

The ready `ARPGCharacter` already owns `ARPGWoodcuttingComponent`, so normal player Blueprints do not need to recreate progression, networking, chop timers or rewards.

> **UE5.8.1 compatibility retained:** v2.0.2 keeps the v2.0.1 explicit `UStaticMesh*` return branch in `GetSelectedTreeMesh()` and the Day/Night `SetNetUpdateFrequency()` correction.


## v2.1.1 equipped-axe rule

Basic Attack only redirects into Woodcutting when the authority finds a real equipped runtime inventory instance whose exact Item Definition is a valid axe/tool. A Data Asset merely existing or being open in the editor does not count as ownership or equipment. The runtime entry must have a valid instance GUID, positive quantity, matching equipped slot and the required tool tag/tier.

The exact equipped tool is passed into chop swing/hit presentation, so its Gathering Swing/Hit sounds do not depend on a second client-side ItemId lookup. `Get Best Equipped Woodcutting Tool Instance Id` is exposed for debugging the exact runtime instance that Woodcutting is using.

## Fastest setup

### 1. Create the wood item

Create **Content Browser > Miscellaneous > Data Asset > ARPGItemDefinition**.

Example:

```text
Definition Id = AshLogs
Max Stack = 99
Item Tags = Item.Resource.Wood
```

This is the exact same item definition used by Inventory, Crafting and Building. To spend the logs on a wall or foundation, put `AshLogs` in that Build Piece Definition's `Build Cost`.

### 2. Create a tree Blueprint

Create a Blueprint child of **`ARPGTree`**, for example `BP_AshTree`.

Recommended class defaults:

```text
Tree > Visual
  Tree Mesh Variations
    [0] SM_AshTree_01
    [1] SM_AshTree_02
    [2] SM_AshTree_03
  Stump Mesh Asset = SM_AshStump
  Randomize Tree Mesh = true

Tree > Visual > Scale
  Randomize Tree Mesh Scale = true
  Minimum Mesh Scale = 0.90
  Maximum Mesh Scale = 1.10

Tree > Woodcutting
  Max Chop Health = 100
  Required Woodcutting Level = 1
  Chop Resistance = 1
  XP Per Successful Chop = 5
  XP On Fell = 25

Tree > Drops
  Wood Item = DA_AshLogs
  Min Wood Quantity = 2
  Max Wood Quantity = 4

Tree > Fall
  Fall Duration = 1.4
  Fall Angle Degrees = 88
  Fallen Tree Visible Seconds = 3

Tree > Respawn
  Respawn = true
  Respawn Seconds = 180
```

Place as many instances as needed. The server selects and replicates one entry from `Tree Mesh Variations`, so every client sees the same variation.

The actor origin/Fall Pivot should be at the base of the trunk. If a source mesh has an unusual pivot, adjust the `TreeMesh` relative transform in the derived Blueprint so `FallPivot` remains at the ground/trunk base.

## Player input — one call

The easiest input is:

```text
Interact / Chop Pressed
    -> Start Woodcutting From View
```

The inherited Woodcutting component performs a forgiving sphere trace from the player's view, finds `ARPGTree`, validates range/level/tool requirements on the server, and starts the automatic chop loop.

You can also use:

```text
Start Woodcutting(Tree)
Chop Tree Once(Tree)
Stop Woodcutting
```

`Start Woodcutting` repeats swings automatically by default until the tree falls, the player moves out of range, the player dies, or the target becomes invalid.

## Woodcutting progression

The default skill id is `Woodcutting`. It uses the existing persistent `ARPGSkillComponent`, so the player's Woodcutting level/XP saves with the normal character skill state.

Out of the box, the generic framework XP formula and level-99 cap work without creating another asset. For a custom curve/unlocks, create an `ARPGSkillDefinition` with `DefinitionId = Woodcutting` and assign it to the player's Woodcutting component.

Pure Blueprint queries:

```text
Get Woodcutting Level
Get Woodcutting XP
Get Woodcutting XP For Next Level
Get Woodcutting XP Remaining
Get Woodcutting Level Progress
Has Woodcutting Unlock
```

Each successful chop can award XP, and felling the tree can award an additional completion bonus. Higher Woodcutting level increases native chop power through the exposed `Skill Power Per Level` multiplier.

## Basic Attack automatically chops with an axe

The normal `AARPGCharacter -> Basic Attack` path is now context-sensitive by default. No extra input graph is required.

```text
Basic Attack pressed
    -> valid combat / lock-on target?  YES -> normal combat attack
    -> otherwise tree in Woodcutting view trace?
    -> equipped axe / Woodcutting tool?
    -> tree level + tool tier valid?
    -> one authoritative Woodcutting chop
```

The Woodcutting component exposes:

```text
Basic Attack Auto Chops Trees = true
Basic Attack Requires Equipped Axe = true
Use Combat Melee Montage As Chop Fallback = true
```

This path performs **one chop per Basic Attack press** and respects `Swing Interval Seconds`, preventing click/input spam from outperforming the authored gathering cadence. If a dedicated Tree `Chop Montage Override` or component `Default Chop Montage` is assigned it is used; otherwise the normal melee combat montage can be used as the fallback.

The existing `Start Woodcutting From View` interaction is deliberately preserved. It remains the convenience path for automatic repeated chopping until the tree falls. That means projects can support both action-style axe swings and an interact-to-auto-chop control at the same time.

Pure/callable helpers include `Has Equipped Woodcutting Tool` and `Try Chop Tree With Basic Attack`.

## Axe / tool progression

`ARPGItemDefinition` now includes reusable gathering-tool metadata:

```text
Gathering Tool Tags
Gathering Power
Gathering Tool Tier
```

Example Stone Axe:

```text
Item Tags / Gathering Tool Tags = Item.Tool.Axe
Gathering Power = 1.0
Gathering Tool Tier = 1
```

Example Iron Axe:

```text
Gathering Tool Tags = Item.Tool.Axe
Gathering Power = 1.5
Gathering Tool Tier = 2
```

On a tree you can enable:

```text
Require Woodcutting Tool = true
Required Tool Tag = Item.Tool.Axe
Minimum Tool Tier = 2
```

The Woodcutting component automatically searches the player's equipped inventory for the best matching tool, preferring higher tier and then higher gathering power.

Leaving `Require Woodcutting Tool` off is useful for prototype trees and special magical/hand-harvest resources. Equipped axes can still increase chop power even when the tree does not strictly require one.

## Axe visuals and tool-specific sound

In v2.1 an axe Item Definition can own its held presentation directly. For a static mesh axe:

```text
DA_StoneAxe
  Equippable = true
  Gathering Tool Tags = Item.Tool.Axe
  Equipped Static Mesh = SM_Stoneaxe
  Attach Socket = your weapon/right-hand socket
  Equipped Relative Transform = tune as required
  Gathering Swing Sound = optional
  Gathering Hit Sound = optional
```

The Equipment component automatically creates the held visual when that inventory instance is equipped and removes it when unequipped. `Gathering Swing Sound` plays with the Woodcutting swing; if it is empty, `Combat Swing Sound` is the fallback. `Gathering Hit Sound` plays at the tree impact in addition to any tree-specific Chop Sound.

To start the player with the axe already equipped, add `DA_StoneAxe` to the inherited Inventory component's **Starting Items** and tick **Equip On Spawn**. See `Docs/EQUIPMENT_INVENTORY.md`.

## Per-tree mesh scale variation

`ARPGTree` can now vary size automatically even when all source tree meshes are close to the same height. The default visual range is:

```text
Randomize Tree Mesh Scale = true
Minimum Mesh Scale = 0.90
Maximum Mesh Scale = 1.10
Reroll Mesh Scale On Respawn = true
```

The server/host chooses the uniform scale and replicates `Selected Tree Mesh Scale`, so all clients see the same tree size. The selected value multiplies the component-authored base scale rather than replacing it, and it is applied to both the falling trunk hierarchy and the stump. Minimum/maximum order is sanitized automatically, so accidentally entering them backwards is safe.

Blueprint tree APIs include:

```text
Get Selected Tree Mesh Scale
Select Random Tree Mesh Scale   [Authority]
Set Tree Mesh Scale             [Authority]
```

Set Minimum and Maximum to the same value when a species should have a fixed size.

## Falling-tree presentation

The tree does not simply disappear at zero health.

On the final chop the server determines a fall direction, normally away from the harvester, replicates that direction, and starts a short kinematic trunk fall. The actor's `FallPivot` rotates the full tree mesh while the stump remains at the base. After `Fallen Tree Visible Seconds`, the fallen trunk hides and the stump remains until respawn.

This is intentionally a deterministic transform-driven fall rather than requiring every tree mesh to have a physics-ready collision setup. It gives reliable multiplayer results even for foliage/tree assets that cannot simulate rigid-body physics correctly.

Fall settings are fully exposed:

```text
Fall Away From Harvester
Fall Duration
Fall Angle Degrees
Fall Direction Random Degrees
Fallen Tree Visible Seconds
```

## Effects and sound

`ARPGTree` supports optional feedback without Blueprint spawning graphs:

```text
Chop Niagara
Chop Cascade Fallback
Fell Niagara
Fell Cascade Fallback
Chop Sound
Fell Sound
Feedback Scale
```

Niagara is preferred automatically when assigned; Cascade is the fallback.

The player Woodcutting component also exposes `Default Chop Montage`, and each tree class can override it with `Chop Montage Override`.

## Rewards and building integration

The primary reward is deliberately a direct `Wood Item` picker in the tree Details panel. When the tree falls, a random quantity between `Min Wood Quantity` and `Max Wood Quantity` is added to the harvester's authoritative Inventory. The reward preflights inventory capacity first, so a full inventory cannot receive a silent partial stack while the event reports failure. `On Tree Reward Granted` exposes whether each configured reward was actually added.

`Bonus Drops` can add sap, bark, seeds, rare wood, resin or any other `ARPGItemDefinition` using min/max quantity and chance.

Successful inventory rewards also call the existing `Report Item Looted` event route, so Collect quest objectives can progress from Woodcutting without a custom quest graph.

Because Building costs use the same item IDs, no conversion layer is needed:

```text
Tree Wood Item = AshLogs
        -> Inventory receives AshLogs
        -> Build Piece Build Cost consumes AshLogs
```

## Tree Blueprint API

Useful tree calls:

```text
Is Standing
Is Felled
Get Chop Health Percent
Get Selected Tree Mesh
Get Selected Tree Mesh Scale
Get Chop Impact Location
Can Be Chopped By
Apply Chop                  [Authority]
Fell Tree                   [Authority]
Force Respawn               [Authority]
Select Random Tree Mesh     [Authority]
Set Tree Mesh Index         [Authority]
Select Random Tree Mesh Scale [Authority]
Set Tree Mesh Scale         [Authority]
```

Events:

```text
On Tree Chopped
On Tree Felled
On Tree Respawned
On Tree State Changed
On Tree Reward Granted
```

These make special trees easy to extend: quest trees, cursed trees, boss resources, trees that spawn creatures when felled, rare-drop trees, seasonal trees or trees that alter the world when harvested can stay Blueprint-level while the standard harvesting behavior remains native.

## Multiplayer model

The player-owned Woodcutting component owns the server RPC. The tree itself does not trust client-authoritative chop damage.

The server validates:

- target still exists and is standing;
- harvester is alive;
- maximum chopping range;
- required Woodcutting level;
- required equipped tool tag/tier;
- calculated chop power;
- XP and inventory rewards;
- final fall/respawn state.

Tree health, state, selected mesh and fall direction replicate to clients. Chop/fell presentation is cosmetic multicast feedback.

## Recommended species progression

A project can author any progression. A simple starting structure could be:

```text
Ash / normal tree    Required Level 1   Tool Tier 0-1
Oak                  Required Level 15  Tool Tier 1
Willow               Required Level 30  Tool Tier 2
Yew                  Required Level 50  Tool Tier 3
Ancient / magical    Required Level 70+ Tool Tier 4+
```

Those values are examples, not hard-coded framework rules. The purpose of the native system is to let each derived tree Blueprint define its own level, durability, drop item, quantity, visuals and tool gate in the Details panel.


## Settlement villager woodcutting — v2.16.0

Settlement workers reuse `ARPGTree`; they do not generate resources through a separate passive timer. `ARPGTree` now exposes **Allow Settlement Villager Harvest**. When a resident's Settlement Definition permits the worker requirement bypass, `CanBeChoppedBy` accepts that resident without requiring the player axe/skill loadout, then every chop still goes through the tree's normal authoritative `ApplyChop` durability and reward path.

Residents select standing trees within the Hub's configured Woodcutting Radius, reject trees already reserved by another settlement resident, path to the tree, optionally multicast a chop montage, and apply the configured Villager Chop Power at the configured interval. When the tree falls, its configured Wood item and bonus-drop item IDs can be transferred from the worker inventory into the Hub's normal persistent stockpile.

Configure worker cadence, maximum concurrent woodcutters, search/acceptance radius, duty chance, montage, chop power and stockpile deposit policy on `ARPGSettlementDefinition`. See `Docs/SETTLEMENTS.md` for the complete settlement setup.


## Settlement villager contextual axe presentation — v2.16.10

Settlement workers can now visibly hold the same Axe art used by the normal equipment system without receiving a fake gameplay item. In the `ARPGSettlementDefinition` set:

```text
Settlement | Woodcutting | Tool Presentation
Villager Woodcutting Tool Item = your Axe ARPGItemDefinition
Show Tool While Going To Work = true
Play Tool Equip / Unequip Presentation = true
```

The referenced Item Definition supplies its `Equipped Static Mesh` or `Equipped Skeletal Mesh`, `Attach Socket`, `Equipped Relative Transform`, optional visual-actor subclass, and optional equip/unequip montage/sounds. `Going To Work` and `Woodcutting` display the tool by default; `Roaming`, `At Home`, `Returning Home` and `Homeless` remove it.

This visual does not call Inventory `EquipItem`, does not create/remove an item instance, does not apply equipment Gameplay Effects/stat modifiers and does not spend durability. It is a local presentation actor rebuilt from replicated settlement work state, so the woodcutting-tool visual still adds no save payload; the current world SaveVersion remains v9; v2.16.11 introduced Settlement Path geometry persistence and v2.16.12 changes only tangent runtime semantics.

## Building occupancy and tree respawn — v2.16.9

Gatherable trees now participate cleanly in the player-building lifecycle. Foundations may be placed through an `AARPGTree`; authority then suppresses any Tree whose trunk/root regeneration volume is occupied by the resulting build piece. The resource is hidden and non-colliding rather than destroyed as loot.

Important behavior:

- Construction suppression does **not** call `FellTree`, grant Woodcutting XP, generate drops or fire a fake harvest.
- Settlement villagers stop targeting a build-suppressed Tree and do not treat that suppression as a rewarded fell/stockpile deposit.
- A naturally felled Tree keeps its original respawn eligibility while a structure blocks it.
- `Force Respawn` respects building occupancy; a Tree cannot be forced into a Foundation/house.
- Multiple blocking build pieces are tracked. Respawn becomes possible only after the final one clears.
- A suppressed Tree rechecks occupancy on `Building Respawn Recheck Seconds` (default 1 second); standing Trees have no permanent suppression polling cost.
- Suppression state is replicated but not separately saved. Persisted build actors reconstruct it on world load, avoiding stale tree/build divergence.

For ordinary trees, keep `Suppress Respawn While Built Over = True` and the default `Building Respawn Block Radius = 85 cm`. Increase/decrease the radius only when a tree asset's trunk/root footprint is unusually large/small; the system intentionally does not use canopy width for horizontal suppression.
