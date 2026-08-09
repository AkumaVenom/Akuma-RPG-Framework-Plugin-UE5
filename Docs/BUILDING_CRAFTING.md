# Building, Factions, Storage and Crafting

## Player and faction ownership

Every runtime build piece has a stable Building ID and an `ARPGFactionOwnershipComponent`.

A player-built structure can inherit:

- owning account ID
- owning character ID
- owning faction ID

The default ready player faction is `Player`.

Build definitions expose faction/reputation placement requirements and access/damage policy. This lets the same rules drive player houses, faction forts, doors, chests, crafting stations, guards and future turrets.

## Territory

Use `ARPGFactionTerritoryVolume` to restrict placement in a region. A territory can allow/deny the owning faction, allies, neutrals and hostiles, with optional reputation requirements.

## Placement

`ARPGBuildingComponent` provides:

- range validation
- resource validation/consumption
- faction/territory validation
- grid snapping
- collision/bounds validation
- optional support validation
- authoritative spawn
- owner/faction initialization
- build event routing

Use Blueprint/UMG to draw your preferred ghost/preview material while calling `EvaluatePlacement` continuously; only the final `RequestPlacePiece` is authoritative.

## Build actors

`ARPGBuildPieceActor` provides persistent IDs, health, upgrade level, repair/demolish and faction-aware damage/modify checks.

`ARPGStorageActor` derives from it, so a placeable chest is both a structure and an inventory container.

`ARPGCraftingStationActor` derives from storage, so a furnace/workbench can also have persistent structure ownership, input storage and output storage.

## Furnace recipe pattern

Create item definitions:

- Ore item, e.g. `Item_IronOre`
- Fuel item with a tag such as `Item.Fuel.Coal`
- Output item, e.g. `Item_IronIngot`

Create a recipe:

- Inputs: Iron Ore x2
- Outputs: Iron Ingot x1
- Required Station Tag: `Station.Furnace`
- Craft Seconds: your desired duration
- Consumes Fuel: true
- Fuel Tag: `Item.Fuel.Coal`
- Fuel Per Craft: 1

Create a crafting station definition:

- Station Tag: `Station.Furnace`
- Use Station Inventory For Inputs: true
- Fuel Comes From Station Inventory: true
- Process While Offline: true/false as desired

Players can deposit ore/fuel through the same storage interaction flow, queue the recipe, and withdraw finished output from the Output Inventory.

## Persistence

Runtime build pieces and their linked containers are stored in the world save. The save record includes transform, health, upgrade level, player/faction ownership, storage items, station output and crafting queue state.

When offline processing is enabled, elapsed time is restored into the queue when the station is loaded. Fuel/output capacity can still block completion, which prevents offline processing from inventing resources or overflowing the output inventory.
