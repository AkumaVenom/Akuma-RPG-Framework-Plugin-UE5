# Primary Asset Manager Setup

Akuma's RPG Framework uses `UPrimaryDataAsset` definitions and stable `DefinitionId` values. Save files store IDs, and runtime systems resolve those IDs through Unreal's Asset Manager.

This means the project's Asset Manager must know where your RPG definitions live.

## Recommended content layout

A clean project layout is:

`/Game/RPG/Data/Items`

`/Game/RPG/Data/Classes`

`/Game/RPG/Data/Quests`

`/Game/RPG/Data/Factions`

`/Game/RPG/Data/Skills`

`/Game/RPG/Data/Slayer`

`/Game/RPG/Data/Vendors`

`/Game/RPG/Data/Loot`

`/Game/RPG/Data/Pets`

`/Game/RPG/Data/Bosses`

`/Game/RPG/Data/Dungeons`

`/Game/RPG/Data/Recipes`

`/Game/RPG/Data/CraftingStations`

`/Game/RPG/Data/Buildings`

`/Game/RPG/Data/Mounts`

## Asset types to register

Under **Project Settings > Game > Asset Manager**, add Primary Asset Type entries for the definition classes you use and point them at your RPG data directories. Enable recursive scanning for your chosen root directory.

Framework definition classes:

- `ARPGItemDefinition`
- `ARPGClassDefinition`
- `ARPGQuestDefinition`
- `ARPGFactionDefinition`
- `ARPGSkillDefinition`
- `ARPGSlayerMasterDefinition`
- `ARPGVendorDefinition`
- `ARPGLootTableDefinition`
- `ARPGBattlePetDefinition`
- `ARPGBossDefinition`
- `ARPGDungeonDefinition`
- `ARPGRecipeDefinition`
- `ARPGCraftingStationDefinition`
- `ARPGBuildPieceDefinition`
- `ARPGMountDefinition`

## Stable IDs

Every Data Asset has `DefinitionId`.

Good IDs:

- `Item_IronOre`
- `Item_IronIngot`
- `Quest_WolvesAtTheGate`
- `Faction_Player`
- `Skill_Slayer` (or simply `Slayer` if that is your project convention)
- `Boss_BlackDragon`

The framework's resolver uses the concrete definition class as the Primary Asset Type and `DefinitionId` as the Primary Asset Name.

Once a game has shipped saves, changing a definition ID is equivalent to changing a database key. Prefer migrations/aliases instead of silently renaming released IDs.
