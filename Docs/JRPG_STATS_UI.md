# Complete JRPG Stats UI — v2.11.0-alpha

v2.11.0-alpha adds a ready-to-use local player stats panel on top of the existing `ARPGStatsComponent` and `ARPGProgressionComponent`. It is intentionally presentation-only: the existing replicated/server-authoritative stat systems remain the source of truth.

## Fastest setup

Every `AARPGCharacter` now inherits a **StatsUI** component. The default **Stats Widget Class** is the native `ARPGStatsPanelWidget`, so no Widget Blueprint is required for the first test.

Bind one player input action to:

```text
Toggle Stats UI
```

That is enough to open and close the supplied panel. You can also call `Open Stats UI` and `Close Stats UI` separately.

The panel only opens on a **locally controlled player character**. NPC copies of the inherited component remain dormant and do not create widgets, timers or replication traffic.

## What the supplied panel shows

The native panel presents:

- Character Name
- Effective Level
- Current XP, XP required for the next level and an XP progress bar
- Health / Max Health and progress bar
- Mana / Max Mana and progress bar
- Stamina / Max Stamina and progress bar
- JRPG Stat System enabled/disabled state
- Available Attribute Points and total earned Attribute Points
- Strength
- Vitality
- Magic
- Spirit
- Dexterity / Speed
- Luck
- Allocated Attribute Points for each primary stat
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
- Critical Damage multiplier
- Attack Speed multiplier
- Movement Speed multiplier

When the JRPG system is disabled on an older character, the panel still reports the character's live legacy combat values where the framework has an equivalent getter.

## Built-in Attribute Point allocation

The supplied widget includes a `+` button for all six primary stats. These buttons call the existing `ARPGStatsComponent::SpendAttributePoints` API.

That means multiplayer ownership remains unchanged:

```text
Local Stats UI button
  -> client preflight
  -> ServerSpendAttributePoints RPC
  -> server validation
  -> replicated stat revision / progression state
  -> UI refresh
```

The buttons automatically disable when:

- JRPG Stats is disabled;
- there are no unspent points;
- the requested stat has reached its allocation/cap rules.

The UI does not grant points, calculate authoritative stats or bypass server validation.

## Built-in Close button

The native panel includes a **Close** button. It calls the owning `StatsUI` component, removes the panel and restores the configured input/cursor state.

Custom Widget Blueprints can also call:

```text
Request Close Stats UI
```

from any custom button.

## Input behavior

By default, opening the panel:

- switches the local PlayerController to **Game and UI** input mode;
- shows the mouse cursor;
- keeps the game unpaused;
- remembers the previous mouse-cursor visibility.

Closing the panel restores **Game Only** input and the previous cursor visibility.

These behaviors are exposed on the inherited **StatsUI** component:

- `Manage Input Mode`
- `Show Mouse Cursor While Open`
- `Restore Game Only Input On Close`

If the project already has a master menu/input-stack manager, disable `Manage Input Mode` and let that system own input state.

## Performance

`ARPGStatsUIComponent` has **no Tick** and is not replicated.

Nothing runs while the panel is closed. While open, one lightweight local refresh timer runs at `0.15 s` by default. This keeps current Mana/Stamina/Health/XP presentation live without adding permanent Character or Widget Tick work.

The timer is cleared immediately when the panel closes or the pawn ends play.

## Custom Widget Blueprint

For custom art, create a Widget Blueprint derived from:

```text
ARPGStatsPanelWidget
```

Then select the player Blueprint's inherited:

```text
StatsUI -> Stats Widget Class
```

and choose your Widget Blueprint.

### Completely custom layout

Use the Blueprint event:

```text
On ARPG Stats UI Updated
```

It receives the complete `ARPGStatsUISnapshot` structure.

Useful helper nodes on the widget include:

- `Get Stats UI Snapshot`
- `Get Stats Character`
- `Refresh Stats UI`
- `Request Close Stats UI`

### Zero-graph standard-name binding

A custom Widget Blueprint can also use the following child names and let the native base populate them automatically:

```text
CharacterNameText
LevelText
SystemStateText
XPText
XPBar
HealthText
HealthBar
ManaText
ManaBar
StaminaText
StaminaBar
AttributePointsText

StrengthText
VitalityText
MagicText
SpiritText
DexterityText
LuckText

StrengthAllocatedText
VitalityAllocatedText
MagicAllocatedText
SpiritAllocatedText
DexterityAllocatedText
LuckAllocatedText

MeleeAttackPowerText
RangedAttackPowerText
MagicAttackPowerText
PhysicalDefenseText
MagicDefenseText
AccuracyText
EvasionText
MagicEvasionText
SpeedText
CriticalChanceText
CriticalDamageText
AttackSpeedText
MovementSpeedText

CloseButton
StrengthPlusButton
VitalityPlusButton
MagicPlusButton
SpiritPlusButton
DexterityPlusButton
LuckPlusButton
```

You do not need to use every standard child. Missing optional fields are simply ignored.

## Blueprint input example

For an Enhanced Input action such as `IA_Stats`:

```text
IA_Stats (Started)
    -> Toggle Stats UI
```

The call can be made directly on the `ARPGCharacter` because v2.11 adds player-friendly wrappers around its inherited `StatsUI` component.

## Multiplayer ownership

The stats panel is deliberately local UI:

- it does not replicate;
- it does not exist on dedicated servers;
- it only opens for a locally controlled `AARPGCharacter`;
- it reads the existing replicated Stats/Progression state;
- Attribute Point changes still use the existing server-authoritative RPC path.

This avoids adding a second gameplay-state authority inside UMG.
