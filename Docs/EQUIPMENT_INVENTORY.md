# Equipment & Starting Inventory — v2.2.1-alpha

## Physical attachment exclusivity (v2.13.1)

Visible equipment now has a second exclusivity rule in addition to logical `EquipmentSlot`: two equipped items cannot own the same resolved character attachment socket at the same time. This is automatic and source-agnostic. If a Stone Axe uses `Equipment.Tool.MainHand` and an Iron Sword uses `Equipment.Weapon.MainHand` but both resolve to `weapon_r`, equipping either one retires the other before the replacement visual is created.

This applies to Inventory actions, Quick Access activation, starting equipment and direct `Equip Item` Blueprint calls. Legacy/saved state is repaired on authority if two visible equipped entries already claim one socket, and the visual projection separately refuses to display both during recovery. Items on different physical sockets remain independent, so normal weapon/offhand/armor combinations are unaffected. For deterministic authoring, set the intended `Attach Socket` on each visible equipment Item Definition; fallback hand sockets remain supported.


Akuma's RPG Framework v2.1 makes item/equipment authoring designer-facing. The replicated `Runtime Items` array remains read-only because it contains generated instance GUIDs, equipped state and save data. Use `Starting Items` to author default player/NPC loadouts.


## Important v2.1.1 runtime rule

An Item Definition asset existing in the Content Browser does **not** mean the character owns or has that item equipped. Equipment/Woodcutting only consume a real runtime inventory instance with a valid GUID, positive quantity, equipped flag and matching equipment slot.

Each runtime inventory entry now also stores a soft reference to the exact Item Definition that created it. This prevents a newly-authored project asset such as `DA_StoneAxe` from being added successfully but then failing to resolve for its held mesh, tool checks or sounds. Stable `ItemId` is retained for saves/backward compatibility.
The same exact-entry resolver is also used by tagged crafting/fuel checks, so a project-authored item does not work in Equipment but mysteriously fail when another framework system asks for its tags.

`DefinitionId` on an Item Definition is optional for normal authoring. If you leave it blank, the asset name is used automatically as the stable ItemId. You may still assign an explicit DefinitionId when you want a rename-resistant custom ID.

## Give the player a starting axe

Select the inherited **Inventory** component on your `ARPGCharacter` Blueprint. Under **Inventory > Starting Items**, press **+**, then configure:

```text
Item = DA_StoneAxe
Quantity = 1
Equip On Spawn = true
Quick Access Slot (0 = None) = 1
```

`Grant Starting Items On Begin Play` is enabled by default. `Only When Inventory Is Empty` is also enabled by default, preventing the starter list from being stacked repeatedly onto an existing runtime inventory. The authority converts each Item Definition into normal `FARPGInventoryEntry` instances with fresh GUIDs and uses the normal Equipment component for optional auto-equip.

Existing saved inventories still load through the normal save system and replace runtime state. Starting Items are authoring defaults, not a second inventory format. Automatic starter seeding is deliberately deferred until after the normal Begin Play persistence-load pass, preventing saved characters from briefly receiving/equipping starter gear before their saved inventory is restored.

When `Quick Access Slot` is greater than zero, the framework first creates the normal runtime Inventory entry and then pins that **owned instance** to the inherited Quick Access component. If the same starting item also has `Equip On Spawn`, that hotbar slot becomes the initial active slot. See `Docs/QUICK_ACCESS.md` for switching and consumables.

## Create `DA_StoneAxe`

Create **Miscellaneous > Data Asset > ARPGItemDefinition** and configure the normal identity/item fields, then:

```text
Equipment
  Equippable = true
  Equipment Slot = Equipment.Weapon.MainHand   (or your project's slot tag)

Gathering
  Gathering Tool Tags = Item.Tool.Axe
  Gathering Tool Tier = 1
  Gathering Power = 1.0

Equipment > Visual
  Attach Socket = your right-hand/weapon socket
  Equipped Static Mesh = SM_Stoneaxe
  Equipped Skeletal Mesh = none
  Equipped Relative Transform = tune location/rotation/scale if needed

Equipment > Audio
  Equip Sound = optional
  Unequip Sound = optional
  Combat Swing Sound = optional ordinary melee swing
  Gathering Swing Sound = optional Woodcutting swing; falls back to Combat Swing Sound
  Gathering Hit Sound = optional axe/tree impact
  Equipment Audio Volume = 1.0
```

A static axe asset does not require another Blueprint. Assign it directly to **Equipped Static Mesh**. If the weapon has a skeletal mesh, use **Equipped Skeletal Mesh** instead.

## Optional custom visual actor

For a weapon/tool that needs extra components, Niagara, lights, custom Blueprint behavior or multiple meshes, create a Blueprint child of `ARPGEquipmentVisualActor` and assign it to **Equipped Visual Actor Class**. The base actor still exposes native Static Mesh and Skeletal Mesh components and fires `On Equipment Visual Configured` for custom extension logic.

Equipment visuals are presentation actors reconstructed locally from the replicated Inventory/equipment state. The exact Item Definition soft reference replicated with that runtime entry is used first, so project Data Assets do not require a second lookup just to display. They do not add another replicated weapon-state authority path. The server remains authoritative over which inventory instance is equipped.

## Socket alignment

`Attach Socket` should be a socket/bone available on the character skeletal mesh, such as the project-specific right-hand weapon socket. Use **Equipped Relative Transform** to correct marketplace assets whose local axes/pivot do not match the character socket.

## Equip/unequip presentation

The existing `Equip Montage` and `Unequip Montage` Item Definition fields are now consumed automatically. Equip/unequip sounds are multicasted with the authoritative equipment request.

Normal melee combat prefers an equipped item's `Combat Swing Sound` when one is configured; otherwise it falls back to the existing Class/Combat Profile melee swing sound. Woodcutting uses `Gathering Swing Sound` (or Combat Swing as fallback) and can layer the tool's `Gathering Hit Sound` with the tree's own Chop Sound.

## Runtime/Blueprint APIs

Inventory:

```text
Apply Starting Items
Add Item Definition
Can Add Item Definition
Runtime Items (read only)
```

Equipment:

```text
Equip Item
Unequip Item
Refresh Equipment Visuals
Get Equipment Visual
Get Equipped Item Definition In Slot
On Equipment Changed
On Equipment Visual Changed
```

## Multiplayer model

Inventory entries and equipped flags remain replicated/save-backed state. Equipment requests are server authoritative. Each machine creates lightweight non-replicated visual actors from the replicated equipped state, avoiding a second replicated actor for each held item while keeping remote presentation correct. Dedicated servers skip cosmetic visual/audio work.


## Runtime verification helpers

For testing a loadout, the Inventory component exposes `Get Item Definition For Instance` and `Is Item Instance Equipped`. The Equipment component and Woodcutting still perform stricter validation on top of that state before using a tool. If an authored attach socket is missing, Equipment tries its exposed fallback hand-socket list and logs a warning.
