# Quick Access / Active Item Slots — v2.2.3-alpha.2


## v2.13 unified Item Use authority

Quick Access still resolves `Auto` as Equip for equippable items and Use for `bUsable` items, but the Use branch now delegates to the same `ARPGItemUseComponent` used by direct Inventory activation. This preserves hotbar Blueprint action-result events while removing duplicate consumable logic. Item-type cooldowns are synchronized back to Quick Access slots for the existing UI projection.

Per-item custom behavior is authored on the Item Definition through **Item Use Behavior Class**. See `ITEM_USE.md`.

## Active-slot replacement (Blueprint-compatible hotfix)

Replacing the item in the currently active slot with another Equip-action item immediately refreshes the held weapon/tool. This behavior is implemented only in the private component `.cpp`; the public/reflected Quick Access header is unchanged from 2.2.3-alpha.1 to preserve existing Blueprint nodes and casts. Consumables are not auto-used by assignment.

Akuma's RPG Framework v2.2 adds a first-class player hotbar for **current weapons, gathering tools, food, potions and other quick-use items**. The system is intentionally integrated with the existing Inventory and Equipment runtime state instead of creating a second fake inventory.

The ready `ARPGCharacter` owns `ARPGQuickAccessComponent` automatically.

## Core runtime rule

A Quick Access slot does **not** make an item owned.

Each slot stores:

- the stable `ItemId` used to remember the player's hotbar assignment; and
- the exact currently-bound runtime Inventory `InstanceId` GUID.

Every select/equip/use request resolves back to a real `FARPGInventoryEntry` with a valid instance GUID and positive quantity. The exact runtime entry then resolves its Item Definition through the v2.1.1 inventory path.

This means a `DA_StoneAxe` or potion Data Asset merely existing in Content can never make slot 1 usable. The character must actually own a runtime inventory instance.

If a consumable stack is depleted and another owned stack of the same ItemId exists, authority can rebind the saved hotbar bookmark to that other runtime instance. If the character owns none, the ItemId assignment can remain visible to UI as unavailable, but activation is rejected until the item is acquired again.

## Recommended input wiring

Slots are deliberately **1-based**. This keeps Blueprint wiring obvious:

```text
Keyboard 1 Pressed -> Quick Access Pressed (Slot Number = 1)
Keyboard 2 Pressed -> Quick Access Pressed (Slot Number = 2)
Keyboard 3 Pressed -> Quick Access Pressed (Slot Number = 3)
...

Gamepad D-pad Right -> Quick Access Next
Gamepad D-pad Left  -> Quick Access Previous
```

You no longer need to manually connect number keys to `Equip Item` or `Set Equipped` for normal quick switching.

The generic `Quick Access Pressed` function calls `Activate Slot` on the inherited component. The resolved Item Definition decides what activation means.

For a control scheme where slot selection and item use are separate, use:

```text
Select Slot (N)
Use Active Quick Access Item
```

## Quick Access Action

Every `ARPGItemDefinition` now has:

```text
Quick Access
  Allow Quick Access = true
  Quick Access Action = Auto
```

`Auto` is the recommended default:

- **Equippable item** -> Equip/switch through `ARPGEquipmentComponent`.
- **Usable non-equipment item** -> Use immediately.
- **Other item** -> Select only.

Explicit action values are also available:

- `Equip`
- `Use`
- `Select Only`

This lets unusual items override the automatic behavior without changing component code.

## Weapons and gathering tools

For a sword, axe, pickaxe or other held tool, keep the normal Equipment setup:

```text
Equipment
  Equippable = true
  Equipment Slot = Equipment.Weapon.MainHand
```

With `Quick Access Action = Auto`, pressing its hotbar slot calls the existing Equipment component using the exact runtime item instance.

Items that should replace one another in the player's hand should use the same Equipment Slot tag. The Equipment component remains the authority for exclusivity, level/class requirements, Gameplay Effects, held visuals, equip/unequip presentation and replicated equipped state. Quick Access does not bypass any of those rules.

Pressing a slot for an item that is already equipped does not deliberately unequip it or create a separate hotbar equipment state.

## Food and potions

A basic health potion can be authored directly on its `ARPGItemDefinition`:

```text
Quick Access
  Allow Quick Access = true
  Quick Access Action = Auto

Use
  Usable = true
  Consume On Use = true
  Consume Quantity = 1
  Use Cooldown Seconds = 1.0

Use > Vitals
  Restore Health = 40
  Restore Mana = 0
  Restore Stamina = 0

Use > Presentation
  Use Montage = optional drinking montage
  Use Sound = optional potion sound
```

A food item might restore Health and Stamina. A mana potion can use `Restore Mana`.

The authority refuses a direct-vitals consumable when none of its configured restoration can do useful work. For example, a health-only potion at full health is not consumed just because the key was pressed.

## GAS effects and buffs

`Use Gameplay Effect` is optional. Assign a Gameplay Effect when the item should apply a project-specific buff/debuff/effect in addition to, or instead of, direct framework vital restoration.

```text
Use > Effects
  Use Gameplay Effect = GE_FoodBuff
  Use Gameplay Effect Level = 1.0
```

The effect is created/applied on authority through the owner's Ability System Component. Direct Health/Mana/Stamina restoration remains available for projects that do not want to build a Gameplay Effect for every potion.

## Starting items directly on the hotbar

`Inventory -> Starting Items` now has:

```text
Item
Quantity
Equip On Spawn
Quick Access Slot (0 = None)
```

Example:

```text
DA_StoneAxe
Quantity = 1
Equip On Spawn = true
Quick Access Slot = 1
```

The normal runtime item is created first, then slot 1 is assigned to that owned instance. Because `Equip On Spawn` is also enabled, slot 1 becomes the initial active Quick Access slot.

A second starter could be:

```text
DA_HealthPotion
Quantity = 5
Equip On Spawn = false
Quick Access Slot = 2
```

## Inventory UI / drag-and-drop APIs

For an inventory widget, drag a real runtime inventory entry into a hotbar slot and call:

```text
Assign Item To Slot
  Slot Number
  Item Instance Id
```

Or, when you already have an ItemId but still want server-side ownership validation:

```text
Assign Item Id To Slot
```

Other layout APIs:

```text
Clear Slot
Clear All Slots
Swap Slots
Find Slot For Item Id
```

Duplicate ItemId assignments are prevented by default. Assigning the same item type to another slot moves the assignment rather than creating redundant hotbar copies. In v2.2.1, the exact same runtime Inventory `InstanceId` is **always unique**, regardless of duplicate policy.

In v2.2.2 each slot also stores an assignment revision. Duplicate repair keeps the newest/explicit drop target, and `Get Slot View` defensively suppresses any older duplicate projection. This makes inventory-to-hotbar drag/drop an atomic move even when migrating stale Quick Access data.

`Allow Same Item Type In Multiple Slots` may be enabled when a project wants two separate owned stacks/copies of the same ItemId on the bar. For example, two distinct potion stacks with different runtime GUIDs can occupy separate slots. Dragging the *same* runtime potion stack, sword or axe to another slot always moves it and clears its old assignment.

For exact-instance Blueprint diagnostics, use `Find Slot For Item Instance`. `Find Slot For Item Id` remains useful for item-type queries.

## UMG hotbar data

Use `Get Slot View` for each visible slot. It returns a Blueprint-friendly structure containing:

- Slot Number
- Assigned
- Owned
- Active
- ItemId
- exact current Item Instance Id
- total owned Quantity for that ItemId
- exact Item Definition (for Display Name/Icon)
- resolved Quick Access Action
- cooldown remaining

Useful events:

```text
On Quick Access Changed
On Active Quick Access Slot Changed
On Quick Access Action Result
On Quick Access Item Used
```

A normal UMG hotbar can refresh its icon/count/cooldown/highlight from those events without polling every Tick.

`On Quick Access Changed` also fires client-side when Inventory changes so stack counts refresh even when the actual hotbar assignment array did not change.

## Cycling

`Activate Next Slot` and `Activate Previous Slot` wrap around the configured slot count.

`Cycle Skips Unavailable Slots` is enabled by default, so empty/out-of-stock assignments do not interrupt weapon/tool cycling.

## Cooldowns

Consumable cooldowns are authoritative and keyed by ItemId. Assigning the same consumable elsewhere cannot bypass its cooldown.

The owner receives cooldown end-time state with the Quick Access slots. `Get Slot Cooldown Remaining` uses synchronized server world time, making it suitable for a UMG radial/fill indicator without making the client authoritative over use timing.

Cooldowns are runtime anti-spam state and intentionally do not persist across SaveGame reloads.

## Save/load behavior

Character save data now includes:

```text
Quick Access Slots
Active Quick Access Slot Number
```

Inventory is restored first. Quick Access is restored second and repairs each saved assignment against the loaded runtime inventory instances.

This is important because hotbar assignment remains a view/control layer over the real inventory rather than a second source of item ownership.

Older saves have no Quick Access array and therefore load with an empty hotbar while retaining their existing inventory/equipment state.

## Multiplayer model

All assignment, selection, equipment switching and item use mutations are server authoritative.

The Quick Access slot layout and current slot replicate **owner-only** because other players do not need another player's private hotbar. Actual equipped inventory state remains replicated through the existing Inventory/Equipment systems, so remote players still see the correct held weapon/tool visual and presentation.

Consumable use presentation can multicast the configured use montage/sound while quantity mutation remains authoritative inventory state.

## Recommended setup for the current player Blueprint

For the number-key graph shown in the player Blueprint, replace the manual `Equip Item` / `Set Equipped` idea with the inherited character wrappers:

```text
1 Pressed -> Quick Access Pressed(1)
2 Pressed -> Quick Access Pressed(2)
3 Pressed -> Quick Access Pressed(3)
4 Pressed -> Quick Access Pressed(4)
```

Then assign the actual runtime items to those slots through Starting Items or the inventory UI.

That keeps Blueprint input extremely small while the plugin owns all runtime GUID validation, equipment switching, consumable use, replication, save/load repair, cooldown and presentation behavior.

## Exclusive active weapon/tool handoff

As of **v2.13.1**, the central Equipment component also enforces physical attachment exclusivity outside Quick Access. An item equipped from Inventory cannot remain visible in the same hand when a different Quick Access item takes that same resolved socket, and the reverse direction is protected as well.

Quick Access is an active held-item channel by default. With **Exclusive Active Quick Access Equipment** enabled, activating a different equippable Quick Access item first unequips the runtime item previously activated through Quick Access, even when the two Item Definitions use different logical `EquipmentSlot` tags such as Tool and Weapon. This prevents two hand-held visuals/effects from remaining active at once while leaving unrelated armor/offhand equipment untouched.

Replacing or clearing the currently active slot does not lose track of the item already in-hand; the component retains that runtime GUID until the next Quick Access equipment activation can perform the clean handoff. Food/potion use does not disturb the held weapon/tool.
