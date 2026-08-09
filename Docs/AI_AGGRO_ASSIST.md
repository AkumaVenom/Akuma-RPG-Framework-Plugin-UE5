# Automatic AI Aggro, Retaliation and Ally Assist

Akuma's RPG Framework v1.7 makes `ARPGAICharacter` defend itself automatically even when a faction relationship is neutral, incomplete or temporarily unavailable.

The inherited **AICombat** component is the normal designer surface. No Behavior Tree or Blueprint damage event is required.

## Passive creature defaults

A passive creature such as a chicken can remain neutral until attacked:

- `bAutoAcquireHostileTargets = true` may stay enabled.
- Faction relationship to Player can remain `0` (Neutral).
- `Retaliate When Attacked = true` (default).
- `Retaliation Overrides Neutral Faction = true` (default).
- `Retaliate When Faction Unknown = true` (default).
- `Fallback: Attack Players On Sight = false` (default).

Result:

`Free Roam -> Player hits chicken -> chicken remembers attacker -> threat/target set -> free roam pauses -> chase/attack -> combat ends -> free roam resumes.`

Retaliation is temporary. `Retaliation Memory Seconds` controls how long a neutral/unknown aggressor is treated as a valid hostile combat target.

## Nearby chickens / allies helping

`Call For Help When Attacked` is enabled by default. The directly attacked AI scans `Ally Assist Radius` for compatible ARPG AI and sends a one-hop help call.

An AI can be considered an ally through any of these designer-safe routes:

1. same ARPG AI Spawner group (`Assist Same Spawn Group`),
2. same Faction Id (`Assist Same Faction`),
3. a positively allied faction (`Assist Allied Factions`),
4. same explicit `Assist Group Id`,
5. same Blueprint/native class when faction identity is missing (`Assist Same Class When Faction Unknown`).

The same-class fallback is useful for placed wildlife that has not had faction data authored yet. It does not override an explicitly hostile faction relationship.

`Assist Can Override Existing Target` defaults off so an ally already fighting something does not constantly retarget because another creature called for help.

## Faction Data Asset setup

`ARPGFactionDefinition` now has `Default Relationship To Unlisted Factions`.

- `0` = neutral to any faction that has no explicit relationship entry.
- negative = hostile to unlisted factions.
- positive = friendly to unlisted factions.

Same faction IDs are always friendly to each other.

Example proactive enemy faction:

- Definition Id: `Enemy.Chicken`
- Relationship: `Player = -100`
- Attack Hostile On Sight: `true`

Example passive wildlife:

- Definition Id: `Wildlife.Chicken`
- Relationship to Player: `0`
- Attack Hostile On Sight: irrelevant while relationship is neutral
- Retaliation defaults handle fighting back after damage

`Attack Hostile On Sight` is now consumed by automatic AI target acquisition. A faction can therefore be hostile but configured not to start fights automatically; it will still retaliate when attacked if retaliation is enabled.

## Faction-free fallback overrides

For a monster that should work before any Faction Data Asset exists, enable on **AICombat**:

- `Fallback: Attack Players On Sight`

For a broad monster/creature rule against pawns with missing faction identity, optionally enable:

- `Fallback: Attack Unfactioned Pawns On Sight`

These are fallback authoring tools. Faction relationships remain the preferred long-term source of allegiance.

## Friendly fire safety

`Retaliate Against Friendly Attackers` defaults to `false`.

This prevents accidental same-faction damage from turning allies against each other. If a game intentionally uses betrayal/friendly-fire aggro, enable it per AI class.

## Spawner integration

`ARPGAISpawner` automatically assigns itself as the runtime Spawn Group Owner of every compatible spawned AI. Therefore spawned group members can assist each other even when faction data is absent and even when `Stay Together` is disabled.

Spawn membership, physical cohesion, and combat assistance are separate concepts.

## Movement integration

When retaliation or ally assist creates a target, the existing combat target path automatically suppresses Free Roam / suspends the spline route. When combat is cleared, the existing movement owner resumes.

## Blueprint/runtime hooks

AICombat exposes:

- `Is Target Considered Hostile`
- `Can Retaliate Against`
- `Is Potential Ally`
- `Receive Aggro Call`
- `Set Spawn Group Owner`
- `On Aggro Triggered`
- `On Ally Assist Triggered`

These hooks are optional; default ARPGAICharacter combat does not require Blueprint wiring.
