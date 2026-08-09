# Combat Feel & Feedback — Akuma's RPG Framework 1.6

Version 1.6 is a combat-feel pass built on the existing automatic combat, lock-on, AI and GAS integration. The normal workflow remains data-driven: configure a Class Definition and the inherited runtime components consume those settings automatically.

## Zelda-style lock-on camera and facing

`ARPGCharacter` already owns `ARPGTargetingComponent`. Locking a target now defaults to a constant Z-target style presentation:

- the player character continuously yaws toward the selected target;
- the owning PlayerController control rotation smoothly tracks the target;
- a normal Spring Arm/Camera rig is automatically switched to Pawn Control Rotation while locked and restored on unlock;
- Orient Rotation To Movement and Controller Desired Rotation are temporarily disabled for player characters while locked, allowing movement input to strafe/orbit without pulling the character away from the target;
- basic attacks and target-aware abilities continue using the same locked actor.

The important exposed settings are under **Targeting > Camera Lock** and **Targeting > Facing**:

```text
Face Continuously While Locked = true
Lock Camera To Target = true
Smooth Camera Lock = true
Camera Rotation Interp Speed = 11
Camera Tracks Target Pitch = true
Camera Min Pitch = -60
Camera Max Pitch = 55
Camera Pitch Offset Degrees = 0
Override Movement Facing While Locked = true
Auto Configure Camera Rig For Lock = true
```

The only required Blueprint input remains:

```text
Lock-On button -> Toggle Lock On
```

The component applies its lock-facing tick after the owning Actor tick so ordinary Character Blueprint Tick movement/rotation helpers do not trivially overwrite the native lock-on facing.

## Combat impact FX — Niagara with Cascade fallback

Every Class Definition combat profile now has **Feedback > Impact FX**. Normal and critical hit feedback is taken from the **attacker's** combat profile, so a player/NPC class can define its hit feel once instead of copying the same blood assets onto every victim. Block/parry/stagger reaction cues use the defending target's profile.

Assign any combination of:

- Hit Niagara
- Hit Cascade Fallback
- Critical Hit Niagara
- Critical Hit Cascade Fallback
- Block Niagara / Cascade fallback
- Parry Niagara / Cascade fallback
- Stagger Niagara / Cascade fallback

`Prefer Niagara` is true by default. If a Niagara slot is not assigned or cannot resolve, the corresponding Cascade particle is used automatically. Set `Prefer Niagara` false to reverse the preference while still retaining Niagara as fallback.

For blood, put your blood Niagara system in **Hit Niagara** and your legacy blood particle system in **Hit Cascade Fallback**. Critical can have a larger/special effect, or leave the critical slots empty to reuse the normal hit effect automatically.

Effects spawn at the resolved combat hit location and orient from the incoming hit direction. Block/parry/stagger use their own cue type, so metal sparks, shield flashes, magic barriers and blood do not need to share one asset.

## Combat audio

Every Class Definition combat profile now has **Feedback > Audio** with exposed sounds for:

- melee swing;
- ranged attack;
- magic cast;
- normal hit;
- critical hit (falls back to normal hit when unset);
- block hit;
- parry;
- dodge;
- block start/end;
- guard break;
- stagger;
- death;
- revive.

`Volume Multiplier`, `Pitch Min`, and `Pitch Max` are exposed. Small random pitch variation is applied per cue by default to reduce repetitive combat audio without requiring Blueprint logic.

Combat cues are multicast from authority and spawned cosmetically on each relevant client. The VFX/audio assets themselves are not replicated objects.

## Critical stagger and real knockback

Each Class Definition has **Combat Profile > Defence > Stagger**. A target can be configured to react to a critical hit with a one-hit stagger:

```text
Enabled = true
Critical Hits Can Stagger = true
Critical Stagger Chance = 0.35
Duration = 0.55
Immunity Seconds = 0.35
Interrupt Attacks = true
Apply Knockback = true
Knockback Velocity = 425
Upward Velocity = 80
Stagger Montage = optional
```

The chance is evaluated only on a critical hit that actually damages the target and is not a successful block/parry/dodge. The target then enters `Combat.State.Staggered` for the configured duration.

While staggered the framework prevents basic attack, dodge and block starts. Current attack timing is interrupted when enabled. Character targets receive actual `LaunchCharacter` knockback; physics-root actors can receive an impulse. Automatic NPC combat stops its path movement while staggered so AI navigation does not immediately cancel the visible knockback.

If `Stagger Montage` is empty, the normal class Hit React montage is used as the animation fallback. This makes the mechanic usable before a dedicated stagger animation is authored.

The post-stagger immunity window prevents rapid critical chains from locking an enemy in permanent stagger unless the designer deliberately reduces it.

## Multiplayer model

Damage, critical determination, block/parry and stagger chance remain authority-side. `bIsStaggered` is replicated with a RepNotify for Blueprint/UI state, while the visual/audio feedback is sent as an unreliable cosmetic multicast.

## Recommended starting setup

For a melee enemy Class Definition, a practical first pass is:

```text
Impact FX
  Hit Niagara = blood impact
  Hit Cascade Fallback = legacy blood impact
  Critical Hit Niagara = larger blood/impact (optional)
  Block Niagara = sparks (optional)
  Parry Niagara = stronger sparks (optional)

Audio
  Melee Swing = weapon whoosh
  Hit = flesh impact
  Critical Hit = heavy flesh impact
  Block Hit = shield/metal impact
  Parry = sharp metal parry
  Dodge = movement cloth/whoosh
  Guard Break = heavy guard crack
  Stagger = heavy reaction
  Death = death impact/voice cue

Stagger
  Critical Stagger Chance = 0.35
  Duration = 0.55
  Knockback Velocity = 425
```

No per-NPC Blueprint hit-FX graph, audio graph or knockback graph is required for the normal framework path.
