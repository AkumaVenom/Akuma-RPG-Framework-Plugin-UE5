# Coordinated Group Combat — v1.8

Akuma's RPG Framework coordinates allied melee `ARPGAICharacter` actors that are fighting the same target so a crowd feels like a combat group instead of every pawn issuing the same direct chase order. The system is part of the inherited `AICombat` component and is enabled by default. No Behavior Tree is required for the standard path.

## Default encounter behavior

`Enable Group Combat Coordination` is ON and `Max Simultaneous Melee Attackers` defaults to **3**. When four or more compatible allied melee NPCs share one target, the framework divides them into two runtime roles:

- **Active Attacker** — owns one of the current melee attack openings and moves toward a distributed inner attack-ring position.
- **Waiting / Orbiting** — stays on a wider ring, keeps focus/facing on the target, repositions/orbits through NavMesh, and waits for an opening.

After an active attacker commits an attack, a short slot cooldown gives another eligible NPC a chance to rotate in. A slot also has a maximum lease; a pawn that cannot use an opening eventually yields it instead of blocking the group indefinitely.

## Designer settings

On `ARPGAICharacter > AICombat > Group Combat` the important defaults are:

```text
Enable Group Combat Coordination = True
Coordinate Only With Allies = True
Max Simultaneous Melee Attackers = 3
Group Combat Coordination Radius = 2600

Attack Approach Radius Multiplier = 0.72
Minimum Attack Approach Radius = 90
Attack Position Tolerance = 120
Attack Slot Max Hold Seconds = 4.0
Attack Slot Cooldown After Attack = 0.75
Attack Slot Yield Seconds = 1.25

Orbit While Waiting = True
Waiting Ring Radius Min = 375
Waiting Ring Radius Max = 650
Orbit Degrees Per Second = 18
Face Target While Waiting = True

Combat Move Goal Refresh Seconds = 0.45
Combat Move Goal Refresh Distance = 100
Project Combat Positions To NavMesh = True
```

Small creatures can use a smaller waiting ring and more simultaneous attackers. Large humanoids or bosses usually feel better with fewer simultaneous melee attackers and a wider waiting ring.

## Who coordinates together

With `Coordinate Only With Allies` enabled, the system reuses the framework's normal ally logic. NPCs can coordinate when they are allied through faction relationships, the same ARPG AI Spawner cohort, explicit assist groups, or the configured same-class/faction fallback rules. This is separate from the spawner's `Stay Together` option: a loose free-roam group can still fight intelligently once several members engage the same target.

## Navigation and facing

Attack and waiting positions are world-space ring goals around the target. The framework projects these goals to NavMesh before issuing movement. Waiting NPCs keep AI focus on the target, allowing them to watch/fight-read the target while strafing or repositioning rather than turning away to run blindly.

The component throttles replacement `MoveToLocation` requests: a new path request is only issued when the desired ring goal has moved enough or the refresh interval expires. This reduces movement churn around a moving player.

## Interaction with defence and abilities

Dodging, blocking/parrying, stagger and death remain higher-priority state transitions. A staggered NPC stops movement; a dead/ragdolled NPC leaves the active group naturally. Waiting melee AI does not auto-cast combat abilities by default (`Allow Abilities While Waiting For Melee Slot = False`), but the option is exposed when a game wants supporting ranged/buff behavior from queued melee units.

## Runtime/Blueprint hooks

For debugging, UI or advanced animation logic, `AICombat` exposes:

- `Current Group Combat Role`
- `Engagement Group Size`
- `Engagement Slot Index`
- `Has Melee Attack Slot`
- `On Group Combat Role Changed`

These are runtime observations; the native system works without consuming them in Blueprint.

## Target death and combat reset

When the shared target dies, each AI clears the current combat target. Temporary retaliation aggression/threat is removed when `Restore Original Disposition After Target Death` / `Clear Threat Against Dead Targets` are enabled (both default ON). Group-combat runtime state then resets to `Solo / Uncoordinated`, and each NPC resumes its ordinary spline, free-roam or home behavior.
