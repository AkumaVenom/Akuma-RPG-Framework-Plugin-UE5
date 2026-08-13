# Complete Inventory + Quick Access UI — v2.12.2-alpha


## v2.13 direct item use

The ready Inventory panel now includes a context-sensitive primary action button. Selecting a usable item shows **Use**; selecting equipment shows **Equip** or **Unequip**. Right-click performs the same action. Usable items call the inherited `ItemUse` component directly and therefore do not need to be assigned to Quick Access first.

Custom Inventory Widget Blueprints can bind standard children named `PrimaryActionButton` and `PrimaryActionText` to keep this behavior without Blueprint glue. See `ITEM_USE.md` for Health Potion setup and per-item Blueprint behavior classes.

v2.12 adds a ready-to-use local player Inventory + Quick Access presentation layer over the existing replicated `ARPGInventoryComponent`, `ARPGEquipmentComponent` and `ARPGQuickAccessComponent`. It does not create a second inventory or hotbar authority path.
## v2.12.2 viewport hit-test ownership fix

The Quick Access HUD must remain above Inventory in viewport Z-order so Inventory items can be dropped onto the hotbar. Because the Quick Access `UUserWidget` is full-screen, the top-level widget itself must be `SelfHitTestInvisible`; setting only its inner Canvas that way is not sufficient. `SelfHitTestInvisible` keeps the actual Quick Access slot descendants interactive while allowing clicks in all other full-screen HUD space to reach the lower-Z Inventory panel.

## v2.12.1 Inventory interaction fix

Inventory slots are hosted inside a `ScrollBox`, while Quick Access slots are not. v2.12.1 routes Inventory selection/right-click/drag arming through the slot widget's preview/tunneling mouse-down handler so the item receives the press before the ScrollBox can consume the gesture. The decorative full-screen dim layer is hit-test invisible and layout-only wrappers pass hit testing through to the real interactive slot.

This fixes the PIE symptom where Inventory icons rendered correctly but could not be selected or dragged, while the Quick Access bar remained fully interactive.


## Fastest setup

Every `ARPGCharacter` now inherits **InventoryUI**. The native widgets are already assigned by default.

Bind one player input action:

```text
Inventory Pressed -> Toggle Inventory UI
```

The Quick Access HUD auto-creates after local possession/client restart. Existing number-key gameplay bindings continue to use the normal character helpers:

```text
1 Pressed -> Quick Access Pressed(1)
2 Pressed -> Quick Access Pressed(2)
...
```

## Native Inventory panel

The default `ARPGInventoryPanelWidget` displays:

- Inventory capacity / used slots
- every real runtime `FARPGInventoryEntry`
- resolved Item Definition icon
- Display Name and Description
- rarity styling
- stack quantity
- durability
- Bound state
- Equipped state
- selected-item details
- built-in Close button

Empty capacity is rendered as real empty visual slots without inserting fake entries into the replicated Inventory array.

Right-click an equippable item to toggle normal Equipment equip/unequip. If a usable item is already assigned to Quick Access, the same context action can activate its existing Quick Access slot. Unassigned consumables are intentionally not given hidden ownership or a hidden hotbar assignment; drag them to the hotbar explicitly.

## Native Quick Access HUD

The default `ARPGQuickAccessBarWidget` displays all configured Quick Access slots and automatically refreshes:

- slot number
- item icon/name tooltip
- total owned quantity
- active slot highlight
- equipped state
- unavailable assignment state
- consumable cooldown progress

The HUD is local-only and uses the existing owner-only replicated Quick Access state. Remote players do not receive another player's UI state.

## Drag/drop rules

The supplied `ARPGInventoryItemSlotWidget` and `ARPGInventoryDragDropOperation` implement these rules with no Blueprint graph:

```text
Inventory item -> Quick Access slot
    Assign the exact owned runtime InstanceId to that slot.

Quick Access slot -> Quick Access slot
    Swap the two slots through the existing server-authoritative Quick Access API.

Quick Access slot -> Inventory panel / Inventory slot
    Clear the Quick Access assignment; the item remains in Inventory.

Quick Access slot -> outside any accepted UI drop target
    Clear the Quick Access assignment.
```

When **Unequip Active Item When Dragged Off Quick Access** is enabled (default), the UI calls the new `Clear Slot And Unequip Active` path. Authority unequips the active equipped weapon/tool first, then clears the hotbar slot. This makes drag-away semantics atomic instead of relying on two separate UI-side gameplay requests.

Inventory-to-Inventory drag is intentionally not treated as a fake reordering system. `ARPGInventoryComponent` stores compact runtime entries rather than persistent empty-slot indices; the ready UI does not invent non-authoritative holes or a second layout save format.

## Exposed InventoryUI settings

Select the inherited **InventoryUI** component on the player Blueprint:

```text
Inventory UI > Widgets
  Inventory Widget Class
  Quick Access Widget Class
  Inventory Slot Widget Class
  Quick Access Slot Widget Class

Inventory UI > Inventory
  Inventory Columns
  Inventory Slot Size

Inventory UI > Quick Access
  Automatically Create Quick Access HUD
  Show Quick Access HUD
  Quick Access Slot Size
  Unequip Active Item When Dragged Off Quick Access

Inventory UI > Layering
  Inventory Z Order
  Quick Access Z Order

Inventory UI > Input
  Manage Input Mode
  Show Mouse Cursor While Open
  Restore Game Only Input On Close
```

Quick Access defaults to a higher Z-order than the Inventory panel so it remains a valid interactive drop target while Inventory is open.

## One-call Character API

The player Blueprint can call:

```text
Open Inventory UI
Close Inventory UI
Toggle Inventory UI
Is Inventory UI Open
```

The underlying component also exposes:

```text
Ensure Quick Access UI
Set Quick Access HUD Visible
Refresh Inventory UI
Refresh Quick Access UI
Get Inventory Slot View
Get Quick Access Slot View
Assign Inventory Item To Quick Access
Swap Quick Access Slots
Clear Quick Access Slot
Activate Quick Access Slot
Toggle Equip Inventory Item
```

## Custom Widget Blueprints

The native defaults are usable immediately, but all major classes are Blueprintable.

Create custom subclasses of:

- `ARPGInventoryPanelWidget`
- `ARPGQuickAccessBarWidget`
- `ARPGInventoryItemSlotWidget`

Then select them on the inherited `InventoryUI` component.

### Inventory panel standard child names

A custom `ARPGInventoryPanelWidget` can use:

```text
InventoryGrid          (UniformGridPanel)
CapacityText           (TextBlock)
SelectedItemNameText   (TextBlock)
SelectedItemDetailsText(TextBlock)
CloseButton            (Button)
```

It also receives:

```text
On ARPG Inventory UI Refreshed
On ARPG Inventory Selection Changed
```

### Quick Access standard child name

```text
QuickAccessBox         (HorizontalBox)
```

and the event:

```text
On ARPG Quick Access UI Refreshed
```

### Slot standard child names

```text
SlotBorder
ItemIcon
QuantityText
SlotNumberText
ItemNameText
EquippedText
CooldownBar
```

Each slot receives `On ARPG Inventory Slot Updated` with a complete `FARPGInventoryUISlotView`.

## Performance and networking

- `InventoryUI` is non-replicated.
- Dedicated servers create no UMG.
- Inventory and Quick Access refresh from existing replicated component events rather than per-frame polling.
- No permanent UI/component Tick is added.
- A 0.10 s cooldown timer starts only while at least one visible Quick Access slot has an active cooldown and stops automatically when no cooldown remains.
- Item ownership, assignment, equipment and use remain server-authoritative through the existing components.
- Item Definition assets are presentation metadata only; owning an asset in Content never makes an item owned.
