# NPC Player Level Scaling — v2.5.3-alpha

## Goal

`Scale NPC To Player` keeps selected enemies relevant as the player levels without flattening every creature into the same stat block.

The system separates two concepts:

- **Base Level** — the NPC's authored `ARPGProgressionComponent::Level`. This is never overwritten by scaling.
- **Effective Level** — the replicated runtime level used by the JRPG stats component while player scaling is active.

All natural primary/derived/vital calculations use Effective Level. Saving, Attribute Point ownership and authored progression continue to use Base Level.

## Required setup

On the NPC's `Stats` component:

1. Open `JRPG Stats -> NPC Player Scaling`.
2. Enable `Scale NPC To Player`.
3. For normal authoring, also enable `JRPG Stats -> Setup -> Enable JRPG Stat System` so the Growth/Derived settings are editable before PIE.

`Scale NPC To Player` is now always editable. If an AI/non-player Pawn enters play with scaling enabled while `Enable JRPG Stat System` was accidentally left off, the authoritative runtime automatically enables the JRPG stat layer so the scaling request cannot silently fail. Player-controlled pawns are intentionally rejected by the NPC scaling path and are never auto-converted by this setting.

## Preserving each NPC's identity

Scaling changes the **level input**, not the character template.

For a scaled effective level `L`, the normal JRPG formulas still begin with that NPC's own:

- Base Primary Stats
- Primary Growth Per Level
- Base Max Health / Mana / Stamina
- per-level and per-primary vital coefficients
- derived stat formula settings
- equipped runtime item modifiers
- permanent Attribute Point allocations (if any)

Example:

- A Chicken can use low Strength/Vitality bases and modest growth.
- A Winged Demon can use much higher Magic/Spirit and stronger growth.

At Effective Level 30 both are level 30 for stat evaluation, but the demon remains substantially more dangerous because its authored template is stronger. The player's own stats are never copied into the NPC.

## Effective-level formula

The target reference is:

`Reference Player Level + Level Offset`

Then Allow Scale Up/Down constraints are applied, followed by:

`lerp(Base Level, Target Level, Level Match Strength)`

and finally Minimum/Maximum Scaled Level clamps (also respecting the NPC Progression component's Max Level).

Examples for a base level 10 NPC versus a level 30 player:

- Match Strength `1.0`, Offset `0` -> Effective Level 30.
- Match Strength `0.5`, Offset `0` -> Effective Level 20.
- Match Strength `1.0`, Offset `+2` -> Effective Level 32.
- Maximum Scaled Level `25` -> Effective Level 25.
- Allow Scale Up disabled -> remains level 10.

This gives designers both modern full level matching and softer zone-style scaling from the same system.

## Multiplayer reference policy

A shared authoritative NPC can only own one actual health/stat state at a time, so the component exposes an explicit policy rather than silently choosing a player:

- **Combat Target, Then Nearest Player** — recommended default. If fighting a player, use that player; otherwise use the nearest eligible player.
- **Nearest Player** — useful for open-world solo/coop proximity scaling.
- **Highest-Level Player In Range** — safest shared challenge for high-level groups.
- **Lowest-Level Player In Range** — protects the lowest-level member from an impossible shared creature.
- **Average Nearby Player Level** — balanced shared-party scaling.

`Reference Search Radius = 0` means unlimited distance. Direct combat-target selection is intentionally allowed outside that radius so an already engaged player remains a valid reference.

Dead players can be excluded with `Ignore Dead Players`.

## Encounter stability

`Lock Level While In Combat` is enabled by default.

When combat begins, the next scaling refresh resolves the combat reference and snapshots the resulting Effective Level. While the NPC retains a live Combat Target:

- a different player moving closer does not change the NPC's level;
- a group member joining does not abruptly change max health/damage;
- the reference player leveling up does not change the enemy in the middle of the swing;
- max vitals and speed do not oscillate from multiplayer target changes.

When combat ends, normal reference selection resumes.

## Runtime refresh and performance

Scaling is authoritative on the server. Enabled NPCs use a lightweight repeating timer (default `0.75 s`) and iterate **player controllers only**. They do not call `GetAllActorsOfClass` or scan the entire actor population.

Initial timer delays are randomized so many NPCs spawned together do not all refresh on the same frame.

The replicated runtime state contains:

- Scaling Applied
- Has Player Reference
- Encounter Level Locked
- Base Level
- Reference Player Level
- Effective Level

Use `On NPC Level Scaling Changed` for event-driven nameplates/debug UI.

## Stat coverage

Changing Effective Level rebuilds the complete enabled JRPG stat layer:

- Strength
- Vitality
- Magic
- Spirit
- Dexterity / Speed
- Luck
- Melee Attack Power
- Ranged Attack Power
- Magic Attack Power
- Physical Defense
- Magic Defense
- Accuracy
- Physical Evasion
- Magic Evasion
- Critical Chance
- Critical Damage Multiplier
- Attack Speed Multiplier
- Movement Speed Multiplier
- Max Health
- Max Mana
- Max Stamina

Current HP/MP/Stamina follow the existing JRPG `Preserve Vital Percent When Max Changes` policy. The default preserves percentage, avoiding free healing or arbitrary damage when an out-of-combat rescale occurs.

## Attribute Points and anti-exploit behavior

Effective Level is never passed into Attribute Point initialization/reward accounting.

A base level 1 NPC scaled to Effective Level 50 still owns level-1 progression rewards. Scaling therefore cannot manufacture Attribute Points or mutate saved character progression.

## Persistence

NPC scaling runtime state is not saved. This is intentional: it is contextual world state determined by the currently relevant player(s).

On load, the NPC restores its normal authored/saved progression and stat allocation, then the authoritative runtime scaling timer resolves a fresh Effective Level from active players.

## Suggested presets

### Fully player-matched normal enemy

- Scale NPC To Player: true
- Level Offset: `0`
- Level Match Strength: `1.0`
- Allow Scale Up: true
- Allow Scale Down: true
- Lock Level While In Combat: true

### Elite

- Level Offset: `+2`
- Level Match Strength: `1.0`

Keep stronger Base Primary Stats/Growth as well; Offset does not replace archetype tuning.

### Zone bracket

- Minimum Scaled Level: `20`
- Maximum Scaled Level: `30`
- Level Match Strength: `1.0`

### Soft scaling

- Level Match Strength: `0.5` to `0.75`

The NPC moves toward the player's level but keeps more of the significance of its authored Base Level.

### Boss floor

- Allow Scale Down: false
- Allow Scale Up: true
- Level Offset: `+2` or higher as desired

The boss never becomes weaker than its authored Base Level but can remain relevant to later players.


## v2.5.2 base-level authoring / testing

Select the NPC's inherited `Progression` component and set `Progression -> Level -> Base Character Level` to author the creature's real level. This is the Base Level used by player-relative scaling; the scaler never overwrites it.

For functional PIE tests, `Progression -> Testing -> Enable Manual Level Override` and `Manual Test Level` can force a real authoritative base level at BeginPlay. A scaled NPC then computes its Effective Level from that test Base Level plus the normal reference-player rules. Likewise, applying a manual test level to the player changes the level NPCs read as their reference.
