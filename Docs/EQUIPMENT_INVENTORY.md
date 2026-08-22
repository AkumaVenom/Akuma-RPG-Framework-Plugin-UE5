> **v2.18.4:** Frontend local travel now requires **Default Gameplay GameMode** and explicitly forces that `ARPGGameMode` child through Unreal `game=` URL options for Single Player/Host & Play. This supersedes runtime-only GameMode guessing; v2.18.3 recovery remains a secondary fail-safe.

> **v2.18.3:** Added a destination-GameMode fail-safe for UE5.8 PIE: if a gameplay map loads but `ARPGFrontendGameMode` is still instantiated, the frontend reads that destination World Settings `DefaultGameMode` and performs one guarded absolute reopen with the authored gameplay class explicitly forced. v2.18.2 input/identity/bootstrap behavior remains unchanged.

# Equipment & Starting Inventory — v2.2.1-alpha

> **v2.18.2:** Frontend-to-gameplay travel now explicitly restores gameplay input and guarantees a stable account CharacterId before pawn spawn/persistence; fresh Starting Items/Quick Access bootstrap remains deterministic.


## Multiplayer save namespace (v2.18.1 retaining v2.18.0 behavior)

v2.18.0 does not change Inventory replication or item-instance authority. It changes **which character save namespace authority writes** in listen-server multiplayer: after the pre-pawn profile handshake, remote players resolve character saves from their accepted PlayerController AccountId + CharacterId instead of the listen host's local account. Inventory restoration therefore remains exact per player, including durable runtime instance GUIDs and intentionally empty saved inventories. See `Docs/FRONTEND_LOGIN_NETWORKING.md`.


## Physical attachment exclusivity (v2.13.1)

Visible equipment now has a second exclusivity rule in addition to logical `EquipmentSlot`: two equipped items cannot own the same resolved character attachment socket at the same time. This is automatic and source-agnostic. If a Stone Axe uses `Equipment.Tool.MainHand` and an Iron Sword uses `Equipment.Weapon.MainHand` but both resolve to `weapon_r`, equipping either one retires the other before the replacement visual is created.

This applies to Inventory actions, Quick Access activation, starting equipment and direct `Equip Item` Blueprint calls. Legacy/saved state is repaired on authority if two visible equipped entries already claim one socket, and the visual projection separately refuses to display both during recovery. Items on different physical sockets remain independent, so normal weapon/offhand/armor combinations are unaffected. For deterministic authoring, set the intended `Attach Socket` on each visible equipment Item Definition; fallback hand sockets remain supported.


Akuma's RPG Framework v2.1 makes item/equipment authoring designer-facing. The replicated `Runtime Items` array remains read-only because it contains generated instance GUIDs, equipped state and save data. Use `Starting Items` to author default player/NPC loadouts.


## Important v2.1.1 runtime rule

An Item Definition asset existing in the Content Browser does **not** mean the character owns or has that item equipped. Equipment/Woodcutting/Mining only consume a real runtime inventory instance with a valid GUID, positive quantity, equipped flag and matching equipment slot.

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

`Grant Starting Items On Begin Play` is enabled by default. `Only When Inventory Is Empty` still protects explicit/manual starter grants from stacking onto a populated runtime Inventory. The authority converts each Item Definition into normal `FARPGInventoryEntry` instances with fresh GUIDs and uses the normal Equipment component for optional auto-equip.

### Automatic save/load ownership — v2.17.3

Starting Items are **character-creation defaults**, not a refill policy. Persistence first resolves the stable account/Guest CharacterId and whether an existing character save is present. Existing saved state is authoritative even when the saved Inventory contains zero entries. A genuinely fresh character receives Starting Items once; a detected existing save that fails to load suppresses both starter seeding and automatic overwrite for that session.

From v2.17.3 the decision is an explicit bootstrap state rather than a timing inference. `Fresh Character - No Save` grants the authored Starting Items, auto-equip and Quick Access assignments and immediately establishes the first snapshot when automatic persistence is enabled. `Loaded Existing Save` never grants defaults, including when the saved Inventory is intentionally empty. `Existing Save - Load Failed` grants nothing and blocks automatic overwrite. Runtime `Initial Resolved Character Id` and `Initial Resolved Character Save Slot` show exactly which identity/slot was used.

The write side is state-driven from v2.17.2. On the inherited **Persistence** component, `Save Inventory And Quick Access Changes Automatically` defaults to **true**. Authoritative Inventory changes (including equipment flags/durability/quantities/transfers) and Quick Access edits restart a single debounce timer; after the default 1.5-second quiet period one complete character snapshot is committed. This means the player does not need to wait for the 120-second periodic autosave or rely on PIE shutdown for an Inventory change to become persistent.

Character snapshots are serialized synchronously so an older async write cannot finish after a newer Inventory state. `SaveNow`, periodic character autosave, debounced state saves and `Save On EndPlay` share that serialized path. EndPlay commits for every player-character teardown reason, including explicit destruction. World saving remains a separate asynchronous system. `Flush Pending Character State Save` can be called from a project save-point/menu when an immediate checkpoint is desired. Character save schema remains v5.

### Pickaxes use the same gathering contract

For Mining, create the tool exactly the same way but use `Item.Tool.Pickaxe` in **Gathering Tool Tags**. `Gathering Power`, `Gathering Tool Tier`, held visual/socket/audio and durability fields are shared deliberately. `ARPGMiningComponent` resolves the highest-tier/power valid **equipped runtime instance**, rejects broken tools, enforces each rock's Minimum Tool Tier and charges Gathering durability only after the server confirms a successful Mining strike. See `Docs/MINING.md`.

## Optional custom visual actor

For a weapon/tool that needs extra components, Niagara, lights, custom Blueprint behavior or multiple meshes, create a Blueprint child of `ARPGEquipmentVisualActor` and assign it to **Equipped Visual Actor Class**. The base actor still exposes native Static Mesh and Skeletal Mesh components and fires `On Equipment Visual Configured` for custom extension logic.

Equipment visuals are presentation actors reconstructed locally from the replicated Inventory/equipment state. The exact Item Definition soft reference replicated with that runtime entry is used first, so project Data Assets do not require a second lookup just to display. They do not add another replicated weapon-state authority path. The server remains authoritative over which inventory instance is equipped.

## Socket alignment

`Attach Socket` should be a socket/bone available on the character skeletal mesh, such as the project-specific right-hand weapon socket. Use **Equipped Relative Transform** to correct marketplace assets whose local axes/pivot do not match the character socket.

## Equip/unequip presentation

The existing `Equip Montage` and `Unequip Montage` Item Definition fields are now consumed automatically. Equip/unequip sounds are multicasted with the authoritative equipment request.

Normal melee combat prefers an equipped item's `Combat Swing Sound` when one is configured; otherwise it falls back to the existing Class/Combat Profile melee swing sound. Woodcutting and Mining use `Gathering Swing Sound` (or Combat Swing as fallback) and can layer the tool's `Gathering Hit Sound` with the resource actor's own impact sound.

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

For testing a loadout, the Inventory component exposes `Get Item Definition For Instance` and `Is Item Instance Equipped`. The Equipment, Woodcutting and Mining components still perform stricter validation on top of that state before using a tool. If an authored attach socket is missing, Equipment tries its exposed fallback hand-socket list and logs a warning.
