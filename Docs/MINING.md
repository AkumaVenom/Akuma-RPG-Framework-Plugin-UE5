# Mining — 1–99 Progression, Mineable Rocks, Ores, Stone and Gems

Akuma's RPG Framework v2.17.0 adds Mining as a first-class gathering profession beside Woodcutting. The ready `AARPGCharacter` owns a replicated `UARPGMiningComponent`, and `AARPGMineableRock` is a Blueprintable resource actor intended for hand placement or Unreal **Actor Foliage**.

The system deliberately combines three useful RPG mining patterns without hard-wiring project content:

- **RuneScape-style progression:** Mining defaults to the familiar cumulative 1–99 XP model, with level-gated resource nodes and optional Skill Definition unlocks.
- **Dragonwilds / action-RPG interaction:** the player repeatedly swings a real equipped pickaxe at a persistent world node, with animation, impact timing, durability and resource feedback separated cleanly.
- **Palworld-style node harvesting:** a rock has replicated Mining Health; every successful strike can yield material and the final depletion can award a separate payload. Rare bonus finds can roll independently.

All gameplay mutation is server-authoritative. The local client chooses intent and plays replicated presentation; the server revalidates range, node state, Mining level, exact equipped runtime tool, tool tier, durability, reward rolls, Inventory capacity and depletion.

## 1. Ready character setup

`AARPGCharacter` creates `UARPGMiningComponent` automatically. Projects deriving from `AARPGCharacter` do not need to add another component.

Important component defaults:

| Setting | Native default | Purpose |
|---|---:|---|
| Skill Id | `Mining` | Generic persistent Skill-state key when no Skill Definition is assigned. |
| Use RuneScape Style XP Without Skill Definition | true | Gives Mining a ready 1–99 progression without another asset. |
| Native Mining Max Level | 99 | Maximum level for the native no-asset progression. |
| Base Mining Power | 20 | Base node damage before tool/skill multipliers. |
| Skill Power Per Level | 0.01 | Additive Mining-power growth per level above 1. |
| Max Mining Distance | 450 cm | Authority range gate. |
| Swing Interval | 1.0 s | Repeated Interact-mining cadence and Basic Attack throttle. |
| Swing Impact Delay | 0.25 s | Delays the authoritative hit so impact can line up with animation. |
| Auto Repeat Mining | true | `Start Mining From View` keeps mining until stopped/depleted/invalid. |
| Preferred Mining Tool Tag | `Item.Tool.Pickaxe` | Default exact equipped-tool family. |
| Basic Attack Auto Mines Rocks | true | Context-sensitive Basic Attack -> one mining strike. |
| Basic Attack Requires Equipped Pickaxe | true | Prevents empty-handed combat input from being stolen by rocks. |

All values are exposed in the inherited Mining component Details panel and remain Blueprint-readable/callable.

## 2. 1–99 XP progression

`EARPGSkillXPModel` adds two generic built-in models:

- `Framework Power Curve` — the pre-v2.17 behavior and default for existing Skill Definitions.
- `RuneScape-Style 1-99 Curve` — cumulative familiar 1–99 thresholds converted into per-current-level deltas so the existing `FARPGSkillState` save representation remains unchanged.

The native Mining component uses the RuneScape-style model when no Skill Definition is assigned. Representative cumulative thresholds are:

| Level reached | Total XP |
|---:|---:|
| 1 | 0 |
| 2 | 83 |
| 3 | 174 |
| 10 | 1,154 |
| 50 | 101,333 |
| 99 | 13,034,431 |

Existing Skill assets are backward-compatible. `XP Model` defaults to `Framework Power Curve`, and **any authored `XP Required Per Level` curve keys remain authoritative** over the built-in selector.

### Optional Mining Skill Definition

Create **Miscellaneous -> Data Asset -> ARPGSkillDefinition** and author, for example:

- `Definition Id = Mining`
- `Max Level = 99`
- `XP Model = RuneScape-Style 1-99 Curve`
- optional `Skill Tags`: `Skill.Mining`
- optional `Unlocks`: `Mining.Copper`, `Mining.Iron`, `Mining.Gold`, `Mining.Gems`, etc.

Assign it to `AARPGCharacter -> Mining -> Skill Definition`. The Mining component then uses the asset's stable Definition Id, maximum level, XP model/custom curve and unlock array.

Mining level/XP is stored by the existing generic replicated/saved `UARPGSkillComponent`. Character save schema therefore remains **v5**.

## 3. Create a Pickaxe Item Definition

Mining consumes a real **equipped runtime Inventory instance**. Merely having a Pickaxe Data Asset in the Content Browser never satisfies the tool check.

Create or configure an `ARPGItemDefinition`:

- `Equippable = true`
- choose the project's hand `Equipment Slot`
- add `Item.Tool.Pickaxe` to **Gathering Tool Tags** (Item Tags are also accepted for compatibility)
- `Gathering Power` — strike multiplier
- `Gathering Tool Tier` — progression gate against a rock's `Minimum Tool Tier`
- optional durability:
  - `Uses Durability = true`
  - `Lose Durability On Gathering Hit = true`
  - `Gathering Durability Loss Per Successful Hit`
- optional presentation/audio:
  - equipped Static/Skeletal visual and socket
  - `Gathering Swing Sound`
  - `Gathering Hit Sound`
  - Combat Swing Sound is used as a fallback when Gathering Swing Sound is empty.

Recommended tier convention is project-defined. A simple progression is:

| Pickaxe | Tool tier | Example Gathering Power |
|---|---:|---:|
| Stone Pickaxe | 1 | 1.00 |
| Copper/Bronze Pickaxe | 2 | 1.15 |
| Iron Pickaxe | 3 | 1.35 |
| Steel Pickaxe | 4 | 1.60 |
| Mithril/advanced Pickaxe | 5+ | project balance |

The server resolves the best valid equipped instance by tool tier, then Gathering Power. Broken tools are rejected. Durability is removed **only after a successful authoritative mining strike** from that exact runtime instance GUID.

## 4. Create a Mineable Rock Blueprint

Create a Blueprint class derived from **`ARPGMineableRock`**. Example names:

- `BP_MineableRock_Stone`
- `BP_MineableRock_Copper`
- `BP_MineableRock_Iron`
- `BP_MineableRock_Coal`
- `BP_MineableRock_Gold`
- `BP_MineableRock_Gem`

The inherited Components are:

- `Root`
- `VisualRoot`
- `RockMesh`
- `DepletedMesh`

Do not create custom health, timers, reward logic or replication just to make a normal resource node. Configure the inherited Details categories instead.

## 5. Randomized visual variation

Under **Rock | Visual**:

- **Rock Mesh Variations** — array of Static Meshes. One authority-selected index is replicated to all clients.
- `Randomize Rock Mesh`
- `Reroll Rock Mesh On Respawn`
- optional **Depleted / Rubble Mesh** shown while depleted.

Under **Rock | Visual | Scale**:

- `Randomize Rock Mesh Scale`
- `Minimum Mesh Scale`
- `Maximum Mesh Scale`
- `Reroll Mesh Scale On Respawn`

Under **Rock | Visual | Rotation**:

- `Randomize Rock Yaw`
- `Minimum Random Yaw Degrees`
- `Maximum Random Yaw Degrees`
- `Reroll Rock Yaw On Respawn`

Mesh index, scale and yaw are authority-selected and replicated, so every player sees the same resource variation. The source mesh/component's authored relative transform is preserved beneath the randomized `VisualRoot` transform.

## 6. Mining-health and progression gates

Under **Rock | Mining**:

- `Max Mining Health` — repeated-strike durability of the node.
- `Required Mining Level` — hard profession gate.
- `Mining Resistance` — divides incoming Mining Power; higher values take more strikes.
- `Require Mining Tool` — normally true.
- `Required Tool Tag` — defaults to `Item.Tool.Pickaxe`.
- `Minimum Tool Tier` — rejects lower-tier equipped pickaxes.
- `XP Per Successful Strike` — Palworld/action-style steady progression.
- `XP On Depletion` — final-node XP bonus.
- `Impact Height Fallback` — used only when a useful mesh-bounds contact point cannot be resolved.
- `Mining Montage Override` — optional per-rock animation override.

Effective strike power is:

`Base Mining Power * Equipped Gathering Power * (1 + (MiningLevel - 1) * SkillPowerPerLevel)`

Node damage then applies `Mining Resistance`.

This keeps level progression valuable without making the Mining level itself a substitute for a required pickaxe tier.

## 7. Normal drops: per-strike and depletion payloads

### Successful Strike Drops

`Rock | Drops -> Successful Strike Drops` is evaluated after **every successful strike**. Each array entry exposes:

- Item
- Min Quantity
- Max Quantity
- Chance (0..1)

Use this for repeated stone fragments, ore chips, coal, etc.

Example Stone node:

- Stone, 1..2, Chance 1.0

Example Copper node:

- Copper Ore, 1..1, Chance 0.75
- Stone, 1..1, Chance 0.35

### Depletion Drops

`Rock | Drops -> Depletion Drops` is evaluated once when Mining Health reaches zero.

Use it for the primary final payload, for example:

- Iron Ore, 2..4, Chance 1.0
- Stone, 1..3, Chance 0.75

The final strike can legitimately grant both its Successful Strike drop and the separate Depletion payload.

Rewards are added through the normal definition-aware Inventory path. Successful additions are reported through the Event Router as normal item-loot events, so Collect objectives can progress without special Mining quest code.

## 8. Bonus Chance Drops — gems and rare finds

`Rock | Drops -> Bonus Chance Drops` is a separate array for gems, geodes, fossils, rare ore, crystals or any other bonus resource.

Each entry exposes:

- Item
- Min / Max Quantity
- Base Chance
- Trigger:
  - `Successful Strike`
  - `Depletion`
  - `Both`
- Required Mining Level
- Minimum Tool Tier
- Scale Chance With Mining Level
- Scale Chance With Tool Tier

Global per-rock scaling is exposed under **Rock | Drops | Bonus Scaling**:

- `Bonus Chance Per Mining Level Above Requirement`
- `Bonus Chance Per Tool Tier Above Minimum`
- `Maximum Effective Bonus Chance`

Effective bonus chance is clamped and can therefore be tuned safely. A rare Sapphire entry could require Mining 20, trigger only on Depletion, start at 2%, and gain a small amount per Mining level above 20.

`Get Effective Bonus Drop Chance` is Blueprint Pure so UI/debug presentation can inspect the authored result for a given harvester.

## 9. Suggested node progression

These are authoring examples, not hard-coded framework rules:

| Node | Required Mining | Min tool tier | Health | Typical rewards |
|---|---:|---:|---:|---|
| Stone | 1 | 1 | 80 | Stone; low gem bonus |
| Copper | 1 | 1 | 100 | Copper Ore + Stone |
| Iron | 15 | 2 | 140 | Iron Ore; occasional gem |
| Coal | 20 | 2 | 130 | Coal + Stone |
| Silver | 30 | 3 | 170 | Silver Ore; gem bonus |
| Gold | 40 | 3 | 200 | Gold Ore; stronger gem chance |
| Crystal / Gem Rock | 50+ | 4+ | 240 | Gems / crystals / rare bonuses |

Use your project economy rather than copying those numbers blindly.

## 10. Basic Attack mining — free gathering strike

No extra attack input is required.

`UARPGCombatComponent` checks context-sensitive gathering **before ordinary attack resource spending**:

1. Woodcutting can claim an `ARPGTree` when its tool/context is valid.
2. Mining can claim an `ARPGMineableRock` when its tool/context is valid.
3. Otherwise the normal combat attack continues.

A claimed Mining Basic Attack performs **one** Mining swing, is rate-limited by `Swing Interval`, and does not spend ordinary combat Stamina/Mana just to harvest the rock.

A genuine explicit non-rock combat target is not converted into Mining. Projects can independently disable `Basic Attack Auto Mines Rocks` or `Basic Attack Requires Equipped Pickaxe` on the Mining component.

## 11. Interact mining

Three direct Blueprint entry points are available on the ready character:

- `Start Mining From View`
- `Mine Rock Once`
- `Stop Mining`

`Start Mining From View` uses the player's camera/eye trace, begins the configured swing, and with `Auto Repeat Mining` enabled continues until the node depletes or Mining is cancelled/invalidated.

### Unified InteractWorld wrapper

v2.17.0 also adds `AARPGCharacter::InteractWorld()` for projects that want one contextual Interact input. It tries, in order:

1. Mineable Rock -> start Mining
2. ARPGTree -> start Woodcutting
3. built structure -> existing Door/Window/Storage/Production/etc interaction

The existing dedicated `InteractBuiltStructure` and Woodcutting/Mining functions remain available. Projects are not forced to replace separate bindings.

## 12. Animation and audio

Mining presentation resolves:

1. rock `Mining Montage Override`, if assigned;
2. Mining component `Default Mining Montage`;
3. optional Combat melee montage fallback.

Tool audio resolves the Item Definition's `Gathering Swing Sound`, with Combat Swing Sound fallback. At impact, the rock can layer:

- tool `Gathering Hit Sound`
- rock `Strike Sound`
- `Strike Niagara` or Cascade fallback.

On depletion it can play a separate Niagara/Cascade effect and `Depleted Sound`.

Feedback multicasts do not own rewards or state; authority already resolved those.

## 13. Respawn and building suppression

Mineable Rocks use the same renewable-resource settlement principle established for `ARPGTree`:

- `Respawn`
- `Respawn Seconds`
- `Suppress Respawn While Built Over`
- `Building Respawn Block Radius`
- `Building Respawn Recheck Seconds`

A player Foundation may pierce a framework-managed `AARPGMineableRock` during placement. Once the build piece occupies the node's logical regeneration volume, the rock becomes hidden/non-colliding and remains suppressed while occupied.

**Building suppression is environmental replacement, not Mining.** It grants no Mining XP, no normal drops, no bonus drops and no depletion reward event.

Removing the final blocking build piece allows the normal time/condition-based respawn path to recover the node. Settlement Paths remain excluded from this resource-occupancy scan.

Ordinary static world rocks/props are not ignored by building placement; only the framework renewable resource classes receive this special treatment.

## 14. Actor Foliage workflow

`AARPGMineableRock` is designed so a derived Blueprint can be used with Unreal's **Actor Foliage** workflow, similar to an `ARPGTree` resource actor.

A practical workflow:

1. Create `BP_MineableRock_Stone` derived from `ARPGMineableRock`.
2. Configure its Mesh Variations, drops, level/tool gates and respawn.
3. Create an Actor Foliage asset using that Blueprint class.
4. Paint only the harvestable resource density you actually need.

Actor Foliage creates Actors rather than one Instanced Static Mesh batch. That is necessary for independently replicated health, reward state and interaction, but it is more expensive than decorative foliage. Use ordinary ISM/HISM foliage for non-harvestable background rocks and reserve `ARPGMineableRock` for real gameplay nodes.

`AARPGMineableRock` has no permanent Tick. Normal available/depleted nodes are event/timer driven; only building-suppressed nodes use a periodic self-healing blocker check.

## 15. Blueprint events and runtime API

`AARPGMineableRock` exposes events for:

- On Rock Struck
- On Rock Depleted
- On Rock Respawned
- On Rock State Changed
- On Rock Building Suppression Changed
- On Rock Reward Granted

The reward event includes harvester, Item Definition, quantity, whether Inventory accepted it, whether it was a bonus drop, and whether the reward moment was a Successful Strike or Depletion.

Blueprint-authority functions include:

- Apply Mining Strike
- Deplete Rock
- Force Respawn
- Select / Set Rock Mesh
- Select / Set Rock Mesh Scale
- Select / Set Rock Yaw
- Refresh Building Respawn Suppression

Pure/query helpers expose health percentage, selected art, impact location, eligibility and effective bonus-drop chance.

`UARPGMiningComponent` similarly exposes Mining state, level/XP/progress, unlock checks, equipped pickaxe information, Mining Power and input wrappers.

## 16. Networking and persistence

Authority owns:

- Mining eligibility/range
- exact equipped runtime pickaxe validation
- level and tool-tier gates
- Mining Power and health mutation
- XP
- normal/bonus reward rolls and Inventory adds
- tool durability loss
- depletion/respawn
- building suppression
- random mesh/scale/yaw selection

Replicated node state includes health, availability/depletion, visual variation, visual scale/yaw and building-suppression state. `CurrentRock` is replicated on the Mining component for state/UI hooks.

Mining Skill progression persists through the existing generic Skills save field; no character-save migration is required. World save remains **v9**. Like the existing renewable Tree actor, ordinary Mineable Rock depletion/respawn timing is runtime world-resource state rather than one SaveGame record per foliage node. Player-built structures that suppress resource locations are persistent and re-establish suppression after load.

## 17. Editor acceptance test

For the first project-side test:

1. Create a Pickaxe Item Definition tagged `Item.Tool.Pickaxe`, equip a real runtime instance and give it Gathering Power/Tier.
2. Create Stone/Ore/Gem Item Definitions.
3. Derive `BP_MineableRock_Stone` from `ARPGMineableRock` and assign 2–4 Rock Mesh Variations.
4. Set `Required Mining Level = 1`, `Minimum Tool Tier = 1`.
5. Add Stone to Successful Strike Drops and Depletion Drops; add a low-chance Gem to Bonus Chance Drops.
6. Place several nodes and confirm mesh/scale/yaw variation is stable between server/client.
7. Basic Attack a node: one free Mining strike should occur instead of a combat attack.
8. Press the project's Interact binding wired to `InteractWorld` (or `Start Mining From View`): repeated Mining should continue automatically.
9. Confirm health decreases, per-strike resources arrive, the final payload/bonus rolls occur, XP advances and the exact equipped pickaxe loses configured durability only on successful strikes.
10. Set a node above the player's Mining level or tool tier and verify server rejection/failure text.
11. Deplete it and verify optional rubble, FX/audio and timed respawn.
12. Build a Foundation through an available framework rock and verify the resource is suppressed with **no loot or XP**; demolish the final blocker and verify respawn can recover.
13. Save/reload and confirm Mining level/XP, acquired resources and persistent buildings survive normally.
14. In multiplayer PIE, repeat with a remote client and verify rewards/health/variation are server-owned and consistent.

## 18. Standard tags

v2.17.0 ships these conventions in `Config/Tags/ARPGGameplayTags.ini`:

- `Skill.Mining`
- `Gathering.MineableRock`
- `Item.Tool.Pickaxe`
- `Item.Resource.Stone`
- `Item.Resource.Ore`
- `Item.Resource.Gem`

They are conventions for clean project authoring; resource rewards still reference exact Item Definition assets rather than converting everything into tag-only items.
