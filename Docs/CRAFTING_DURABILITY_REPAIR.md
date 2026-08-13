# Player Crafting, Durability & Repair — v2.14.0-alpha

## Architecture

Every `AARPGCharacter` owns `Crafting`, a replicated `UARPGCraftingComponent`. Gameplay mutation is server-authoritative; UI is local presentation through the inherited `InventoryUI`. Personal crafting reuses `UARPGRecipeDefinition`, so station and player crafting share one content format.

Personal recipe requests are allow-listed by `Crafting -> Player Recipes` by default. `Allow Unlisted Recipe Requests` exists for projects that intentionally provide their own secure recipe-discovery layer, but remains disabled by default. Any recipe with `Required Station Tag` is rejected by personal crafting and remains for `ARPGCraftingStationActor`.

## Recipe authoring

Create **Miscellaneous > Data Asset > ARPGRecipeDefinition**. Each input/output line can select the actual `ARPGItemDefinition` asset under **Item**. `Item Id (Legacy / Optional)` remains supported for old ID-authored recipes.

Example Stone Axe:

```text
Display Name: Stone Axe
Allow Player Crafting: true
Crafting Category: Tools
Inputs:
  Wood x2
  Stone x3
Outputs:
  Stone Axe x1
Craft Seconds: 1.5
Max Batch Size: 10
```

Example Health Potion:

```text
Inputs: Herb x2, Water/Vial x1
Outputs: Health Potion x1
Craft Seconds: 1.0
```

The output Health Potion continues to use the v2.13 Item Use pipeline; crafting does not duplicate consumable logic.

## Craft transaction rules

`Craft Recipe` validates recipe allow-list, station requirement, skill requirement, quantity cap, unequipped ingredient availability and output capacity. Inputs for only the current craft are committed. The completion timer is authoritative and replicated owner-only state supplies synchronized client progress. On success outputs are granted and skill/EventRouter rewards are reported. A batch repeats the same transaction. Cancel/failure refunds only the currently committed craft.

Output-capacity validation simulates the exact runtime order craft-by-craft: consume that craft's unequipped ingredients, free any emptied stacks, then place that craft's outputs before simulating the next batch entry. A full inventory can therefore still craft when the current craft's ingredient consumption genuinely frees the slot required by its result, without incorrectly borrowing capacity from ingredients that would only be consumed by a later batch entry. Equipped ingredient stacks are never consumed.

Ingredient and repair-cost rows are normalized by stable ItemId before validation/consumption. If the same material is authored more than once, the quantities are combined rather than checked independently. Invalid ingredient rows or recipes with no valid crafted output are rejected as invalid recipe data instead of entering a partial transaction.

## Functional durability

On an Item Definition enable **Uses Durability** and set **Max Durability**. Durable items are forced to runtime MaxStack 1 because current durability belongs to an exact `InstanceId`.

Wear contexts are explicit:

```text
Lose Durability On Combat Hit
  Combat Wear Per Successful Hit

Lose Durability On Gathering Hit
  Gathering Wear Per Successful Hit
```

This prevents unrelated durable armor from accidentally wearing down because a weapon/tool action occurred.

### Sword

Enable `Uses Durability` + `Lose Durability On Combat Hit`. The Equipment component charges the active/held exact equipment instance only when Combat reports `AppliedDamage > 0`. Misses, dodges/zero-damage results and cancelled attacks do not wear the item.

### Axe / woodcutting

Enable `Uses Durability` + `Lose Durability On Gathering Hit`. Woodcutting captures the exact selected axe `InstanceId` and charges it only after `Tree->ApplyChop()` succeeds. A broken durable axe is excluded from valid-tool selection.

### Future mining/pickaxe

Mining can use the same authority API after a successful rock hit:

```text
Inventory -> Damage Item Durability(ExactPickaxeInstanceId, WearAmount)
```

The framework also exposes `Get Item Durability`, `Is Item Broken`, `Set Item Durability`, `Repair Item Durability`, and `Repair Item To Full`.

## Broken equipment

At zero durability the item becomes Broken. Broken durable equipment cannot be equipped through Equipment or direct Inventory equipped-state validation. Quick Access activation rejects a broken equippable **before** changing the active slot or unequipping the currently valid held item, so a broken hotbar weapon cannot disturb working equipment. If **Unequip When Broken** is enabled, reaching zero automatically routes through normal Equipment unequip so effects/visuals are removed correctly. Inventory and Quick Access display real Current / Max durability and percentage; the native slot can use `DurabilityBar` and `BrokenText`.

## Repair authoring

On the durable equipment Item Definition:

```text
Can Be Repaired: true
Repair Inputs:
  Iron Ingot x4
  Wood x1
Allow Free Repair: false
Scale Repair Cost By Missing Durability: true
```

The authored quantities represent a fully broken -> full repair. With proportional scaling, a sword missing 25% durability consumes approximately 25% of each full cost, rounded up to at least one of each configured material. Repair materials must be available as unequipped inventory entries.

`Repair Item` validates the exact owned `InstanceId`, repair configuration, missing durability and materials on authority, consumes the calculated cost and restores the item to `MaxDurability`. If repair cannot complete, consumed material is refunded.

## Ready Item Management UI

`Toggle Inventory UI` opens one shared shell. Top-level pages:

```text
INVENTORY
CRAFTING & REPAIR
```

The Crafting page internally switches between Craft and Repair. Craft mode provides recipe list, availability, ingredient/output/requirement details, quantity minus/plus/MAX, Craft, Cancel and progress. Repair mode lists actual damaged repairable item instances with condition, calculated material cost and Repair action.

To open directly on Crafting:

```text
Open Crafting UI
```

### Reskin classes

On inherited `InventoryUI`:

```text
Inventory Widget Class
Inventory Slot Widget Class
Quick Access Widget Class
Quick Access Slot Widget Class
Crafting Widget Class
Crafting Recipe Row Widget Class
Repair Item Row Widget Class
```

Use Widget Blueprint subclasses of `ARPGCraftingPanelWidget`, `ARPGCraftingRecipeRowWidget`, and `ARPGRepairItemRowWidget`. Standard child names/native Blueprint events let visual-only reskins retain the framework behavior without rebuilding RPCs, recipe validation, repair logic or timers.

## Networking / performance

Craft and repair gameplay is authority-only. Personal crafting runtime state is owner-only replicated. UI itself is non-replicated and never runs on dedicated servers. Craft completion is timer-driven; UI uses the existing short local refresh timer only while a craft is active (or another live cooldown requires it). There is no permanent Crafting or Inventory UI Tick.
