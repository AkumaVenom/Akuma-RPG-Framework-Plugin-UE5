# JRPG Character Stats / Attribute Points — v2.5.0-alpha

This release adds an opt-in, server-authoritative character-stat progression layer to the existing `ARPGStatsComponent`. It is inspired by classic PS1-era JRPG stat separation: a small set of primary character attributes grows naturally with level, while combat-facing values are derived from those primaries and can also receive equipment bonuses.

## Backward compatibility

`Enable JRPG Stat System` is **disabled by default** on `ARPGStatsComponent`.

When disabled, the framework continues using the existing values exactly as before:

- `AttackPower` for melee/ranged basic attacks;
- `SpellPower` for magic basic attacks;
- `Armor` for non-magic mitigation;
- existing Max Health / Mana / Stamina values;
- attack montage/timer speed remains unchanged.

This lets existing player/NPC Blueprints migrate deliberately instead of having a new balance model forced onto every character.

## Primary stats

The enabled system owns six classic primary stats:

- **Strength** — main melee damage scaling and a secondary part of ranged damage.
- **Vitality** — physical defense, Max Health and part of Max Stamina.
- **Magic** — magic damage and Max Mana.
- **Spirit** — magic defense, magic Evasion and part of Max Mana.
- **Dexterity** — ranged damage, Accuracy, physical Evasion and the character's derived Speed rating.
- **Luck** — critical chance, critical damage growth and part of Evasion.

`Growth Settings -> Base Primary Stats` defines level-1 values. `Primary Growth Per Level` defines deterministic natural progression. Whole-number rounding is enabled by default, producing the familiar pattern where a fractional growth rate causes some levels to gain a point and others not to.

Default primary cap: **255**.

## Derived stats

The runtime `Derived Stats` block exposes:

- Melee Attack Power
- Ranged Attack Power
- Magic Attack Power
- Physical Defense
- Magic Defense
- Accuracy
- Physical Evasion
- Magic Evasion
- Speed
- Critical Chance
- Critical Damage Multiplier
- Attack Speed Multiplier
- Movement Speed Multiplier

Max Health / Mana / Stamina are also derived from the configured growth/vital formulas and are available through `Get Derived Stat Value` / `Get Stat Snapshot`.

### Default damage mapping

- Melee Attack Power = Strength × 1.0
- Ranged Attack Power = Dexterity × 0.80 + Strength × 0.20
- Magic Attack Power = Magic × 1.0
- Physical Defense = Vitality × 1.0
- Magic Defense = Spirit × 1.0

All coefficients are exposed under `JRPG Stats -> Setup -> Derived Formula`.

Combat consumes the appropriate derived power based on `Basic Attack Type`. Existing `AttackPower`, `SpellPower` and `Armor` fields are kept synchronized as legacy aliases while the JRPG system is enabled, preventing older UI/Blueprint reads from becoming meaningless.

## Criticals

Luck contributes an additive critical-chance bonus and a multiplicative critical-damage bonus. Combat combines those values with the existing Combat Profile `Critical Chance` and `Critical Multiplier` settings.

This means class/weapon profiles still define their authored baseline critical behavior while character progression can improve it.

## Speed

Dexterity produces a derived `Speed` rating. Speed drives:

1. **Attack speed** — attack montage play rate, impact timing, combo queue windows and recovery timers are scaled together so animation and authoritative damage stay synchronized.
2. **Movement speed** — the character's original `MaxWalkSpeed` is cached as the baseline and multiplied by the derived movement-speed multiplier.

Both multipliers have exposed minimum/maximum clamps to prevent extreme progression/equipment values from destabilizing animation or locomotion.

The movement integration is combat-state aware: recalculating Speed while blocking preserves the block movement penalty, and ending the block restores the latest derived full movement speed rather than a stale pre-stat-change value. AI attack-cadence estimation also uses the same attack-speed multiplier as `ARPGCombatComponent`, keeping NPC animation, damage timing, recovery and re-attack eligibility synchronized.

## Accuracy / Evasion

Accuracy, Physical Evasion and Magic Evasion are always calculated and exposed for UI, abilities and project-specific logic. When classic hit checks are enabled, magic attacks use Magic Evasion while melee/ranged attacks use Physical Evasion.

`Enable Accuracy Evasion Hit Checks` is **off by default** because Akuma RPG Framework uses real-time traces/projectiles: silently missing after a visibly connected sword trace is not always desirable in an action RPG.

Projects that want classic JRPG hit/miss resolution can enable it. The authoritative target then resolves:

`Hit Chance = (Attacker Accuracy - Target Evasion) / 100`

clamped by the exposed Minimum / Maximum Hit Chance.

## Automatic level growth

`ARPGStatsComponent` binds to the existing `ARPGProgressionComponent::OnLevelChanged` event.

When XP raises the character from one level to another:

1. natural primary stats are recalculated for the new level;
2. Max Health / Mana / Stamina are recalculated;
3. derived combat stats are rebuilt;
4. configured Attribute Points are granted for every level gained, including multi-level XP awards;
5. movement speed is refreshed;
6. replicated stat/UI events are emitted.

`Restore Vitals On Level Up` is exposed but disabled by default. With it off, the component preserves the character's existing vital percentage as maximums change.

## Attribute Points

Default rules:

- Starting Attribute Points: 0
- Attribute Points Per Level: 3
- Stat Value Per Point: 1
- Max Allocated Points Per Stat: 255

Blueprint UI should call:

`Stats -> Spend Attribute Points(Primary Stat, Points)`

The owning client may call this directly. The request is validated on the authority before any allocation changes. Validation includes:

- JRPG stat system enabled;
- positive requested point count;
- enough unspent points;
- per-stat allocation limit;
- final primary-stat cap.

Useful UI nodes/events:

- `Get Stat Snapshot`
- `Get Primary Stat Value`
- `Get Derived Stat Value`
- `Get Unspent Attribute Points`
- `Get Allocated Attribute Points`
- `On JRPG Stats Changed`
- `On Attribute Points Changed`

`Refund All Attribute Points` is authority-only and returns every manually allocated point to the unspent pool without removing natural level growth.

External reward systems can call authority-only `Add Attribute Points` for quest rewards, special items, milestones, etc.

The component stores a **last rewarded level high-water mark**. If a game mode temporarily lowers a character's level, returning to a previously rewarded level does not grant the same Attribute Points again; only levels above the prior high-water mark award new points.

## Equipment stat bonuses

Every `ARPGItemDefinition` now exposes:

`Equipment -> Stats -> Equipped Stat Modifier`

It supports additive bonuses to all six primaries plus flat derived/vital bonuses, including:

- melee/ranged/magic attack power;
- physical/magic defense;
- Accuracy / Physical Evasion / Magic Evasion / Speed;
- critical chance / critical damage multiplier bonus;
- attack/movement-speed multiplier bonuses;
- Max Health / Mana / Stamina.

Only an actual runtime inventory entry with `bEquipped == true` contributes its modifier. Inventory/equipment changes automatically recalculate the owner Stats component. This preserves the framework's runtime-instance ownership rule: having an Item Definition asset in Content is never enough to receive its stat bonus.

## Persistence / migration

Character save version is now **4**.

Saved JRPG progression includes:

- six per-stat point allocations;
- unspent Attribute Points;
- total Attribute Points earned;
- last processed character level.

Current Health / Mana / Stamina continue to save through the existing character save path.

On load, equipped inventory state is restored before saved current vitals are clamped, so an item that increases Max Health/Mana/Stamina cannot cause the saved current value to be incorrectly truncated against an unequipped maximum.

When loading a pre-v2.5 save (save version < 4), the character receives a clean migrated stat-allocation state for its saved level. Existing XP, inventory, equipment and vitals are preserved.

## Multiplayer authority

Natural growth, equipment modifiers, derived-stat recalculation and point spending are authoritative. Primary/derived state, point state and a stat revision are replicated to clients. The owning client's Attribute Point spend request is an RPC; clients cannot authoritatively manufacture stat points locally.

## Suggested first player setup

On the player Blueprint's inherited `Stats` component:

1. Enable `Enable JRPG Stat System`.
2. Keep the default Base Primary Stats at 10 for the first test.
3. Keep Attribute Points Per Level at 3.
4. Leave Accuracy/Evasion hit checks off during the first action-combat test.
5. Compile and play.

Then grant enough XP to level once. Expected default behavior:

- primary natural stats increase according to their growth rates;
- 3 unspent Attribute Points appear;
- Max vitals and derived stats increase;
- spending a point on Strength immediately raises Melee Attack Power;
- spending Dexterity raises Ranged Attack Power, Speed, attack speed and movement speed;
- spending Magic raises Magic Attack Power and Max Mana;
- spending Vitality raises Physical Defense, Max Health and Max Stamina;
- spending Spirit raises Magic Defense and Max Mana;
- spending Luck raises critical chance and critical damage scaling.
