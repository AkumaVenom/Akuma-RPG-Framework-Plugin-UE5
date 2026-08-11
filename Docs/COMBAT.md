# Combat Setup — Akuma's RPG Framework 1.1

This combat layer is designed so the **player and NPCs use the same combat rules**. You configure the class once, then player input or automatic AI calls the same `ARPGCombatComponent` API.

## Fastest player setup

1. Make a Data Asset of type `ARPGClassDefinition`.
2. On the Class Definition, open **Animation Set**.
3. Add attack montages in the order you want them to combo:
   - `Melee Attacks[0]` = Attack 1
   - `Melee Attacks[1]` = Attack 2
   - `Melee Attacks[2]` = Attack 3
4. Open **Combat Profile** and set `Basic Attack Type` to `Melee`.
5. Leave `Detailed Combo Steps` empty for the automatic default combo, or add steps for per-hit timing/damage/range tuning.
6. Assign the Class Definition to the character's `ARPGClassComponent`.

For input, the ready `ARPGCharacter` exposes these Blueprint-callable functions:

```text
Attack button pressed  -> Basic Attack
Dodge button pressed   -> Dodge (Auto)
Block button pressed   -> Block Pressed
Block button released  -> Block Released
```

The component handles attack state, montage selection, combo progression, resource costs, trace/projectile resolution, damage, defence and replication.

## Automatic combo behavior

When `Detailed Combo Steps` is empty, the framework uses the ordered montage array from the Class Definition. Repeated `Basic Attack` calls advance the combo. Inputs made during the queue window are buffered into the next attack, and a later press can continue the combo until `Combo Reset Time` expires.

For more control, add `Detailed Combo Steps`. Each step can override:

- montage
- damage multiplier
- impact delay
- recovery time
- combo queue open/close time
- range
- trace radius
- stamina/mana cost
- block/parry eligibility
- optional melee trace sockets

`Automatic Timed Impact` is enabled by default, so attacks work without animation notifies. For montage-authored timing, disable it and place the built-in **ARPG Combat Impact** Anim Notify exactly on the contact/fire frame. The notify calls the combat impact path automatically on authority; no Blueprint trace graph is required. `Combat Impact / Trace Now` remains available for custom animation logic.

## Melee

Melee attacks use a server-authoritative sphere sweep. By default it starts in front of the character and uses `Default Range` + `Default Trace Radius`.

For weapon-accurate traces, set `Trace Start Socket` and `Trace End Socket` on a detailed combo step. If both sockets exist on the character mesh, that step sweeps along those socket positions instead of the generic forward trace.

`Max Melee Targets` controls how many valid actors one swing may hit.

## Ranged

Set `Basic Attack Type = Ranged`.

- Put firing montages in `Animation Set -> Ranged Attacks`.
- Set `Ranged Max Range`.
- If `Projectile Class` is empty, the basic attack uses hitscan/line-of-sight resolution.
- For a visible projectile, make a Blueprint child of `ARPGCombatProjectile`, add your mesh/VFX, then assign it to `Projectile Class`.
- Set `Projectile Spawn Socket`, speed and homing as needed.

## Magic basic attacks

Set `Basic Attack Type = Magic` and use `Animation Set -> Magic Casts`.

Magic uses `Spell Power Scale` instead of Attack Power. It can use hitscan-like resolution or the same `ARPGCombatProjectile` path for fireballs, bolts, etc. Complex class abilities remain ideal GAS abilities and can coexist with this automatic basic attack layer.

## Damage, crits and armor

The basic damage path is driven by the Class Definition combat profile:

- `Base Damage`
- `Attack Power Scale`
- `Spell Power Scale`
- `Damage Variance`
- `Critical Chance`
- `Critical Multiplier`

Physical melee/ranged damage receives armor mitigation. Magic basic damage bypasses the simple armor mitigation path so project-specific magic resistance can be handled through GAS/effects if desired.

Friendly faction damage is rejected unless `Allow Friendly Fire` is enabled. Neutral damage can be allowed/disabled independently. NPC automatic target acquisition only chooses **hostile faction relationships**.

## Dodge

The Class Definition's `Combat Profile -> Dodge` section exposes:

- Forward montage
- Backward montage
- Left montage
- Right montage
- distance
- duration
- stamina cost
- cooldown
- invulnerability start/end window
- root-motion-only mode
- whether dodge cancels the current basic attack

`Dodge(Auto)` picks a direction from current character movement. With no movement it defaults to a backward dodge. You can call a specific direction directly if your input system prefers it.

Automatic NPC defence chooses left/right dodge directions. Before an AI dodge begins the framework aborts active path following and clears stale combat MoveTo bookkeeping, so navigation cannot cancel the visible displacement. With `Use Root Motion Only = false`, the framework drives the dodge using the authored `Distance / Duration`; with it enabled, the assigned montage must contain usable root motion and the framework intentionally does not add a launch.

## Shield block, perfect block and parry

The `Combat Profile -> Block` section exposes:

- Block Start montage
- Block Loop montage
- Block Hit montage
- Parry montage
- Guard Break montage
- Block End montage
- front-facing block arc
- separate melee/ranged/magic damage reduction
- stamina cost per blocked hit and per incoming damage
- minimum stamina
- perfect-block/parry window
- guard-break duration
- movement-speed multiplier while guarding
- damage types that may be blocked
- optional equipment-tag requirement

For a shield requirement:

1. Add `Equipment.Shield` to the shield Item Definition's `Item Tags`.
2. Enable `Requires Tagged Equipment` on the class block settings.
3. Set `Required Equipment Tag = Equipment.Shield`.

A hit arriving inside the configured perfect-block window can parry the attacker and stagger/guard-break them. If incoming guard stamina cost exceeds remaining stamina, the defender guard-breaks instead.

## Hit reactions and death

The existing Class Definition `Animation Set` is also used for:

- Hit React
- Death
- Revive

`ARPGAICharacter` now enables **automatic ragdoll on death by default**. A valid Physics Asset is enough; no Blueprint physics graph is required. The framework disables the capsule, switches the skeletal mesh to the ragdoll collision profile, transfers movement velocity and can apply a small impulse from the killing-hit direction. If the mesh cannot enter ragdoll, the configured `Death` montage is played automatically as the fallback.

The base player `ARPGCharacter` keeps ragdoll disabled by default so existing player death/revive montage behavior is unchanged. Player ragdoll can be enabled from `Combat -> Death Presentation` when desired. Same-actor respawn restores the mesh relative transform and collision state before revive.

The ready player can automatically respawn after `Respawn Delay`. Use `Set Respawn Transform` for checkpoints, graveyards or dungeon checkpoints. See `Docs/RAGDOLL_DEATH.md` for the complete NPC death flow and exposed settings.

## Automatic NPC combat — no Behavior Tree required

For the fastest NPC:

1. Create a Blueprint child of `ARPGAICharacter`.
2. Assign its Class Definition.
3. Assign its NPC Faction / Faction Definition.
4. Make sure that faction has a hostile relationship toward the Player faction (and vice versa where appropriate).
5. Place the NPC inside a `NavMeshBoundsVolume`, or spawn it through `ARPGAISpawner`.
6. Play.

`ARPGAICharacter` automatically possesses an AI controller and already contains `ARPGAICombatComponent` + `ARPGThreatComponent`.

The AI component can automatically:

- scan nearby Pawns
- choose hostile faction targets
- prefer the highest-threat target
- chase on NavMesh
- stop at the class's preferred melee/ranged/magic range
- face its target
- perform the same basic attack/combo pipeline as the player
- observe incoming attack timing
- react after a configurable human-like delay
- dodge left/right
- hold a shield block
- perfect-block/parry if timing happens to line up
- stop wandering while in combat and resume afterward
- leash back home
- use configured GAS ability tags automatically

No Behavior Tree is required for this default behavior. A project can still replace or supplement it with Behavior Trees/EQS for bespoke enemies and bosses.

## AI tuning

On `ARPG AI Combat Component` tune:

- Detection Radius
- Lose Target Radius
- Desired Range Override
- Think Interval
- Dodge Chance
- Block Chance
- Reaction Time Min/Max
- Block Hold Seconds
- Ability Try Interval
- Home Leash / Max Chase Distance

AI does not receive an instant perfect response when the player attacks. It observes a new attack serial, waits its randomized reaction time, then decides whether to defend.

## AI abilities

Add Gameplay Tags to `Combat Profile -> AI Auto Ability Tags`. The AI periodically asks `ARPGAbilityBridgeComponent` to activate abilities matching those tags. GAS still owns ability cost/cooldown/activation rules.

## Spawner combat integration

`ARPGAISpawner` now passes its home/leash configuration into `ARPGAICombatComponent`. Dead combat pawns are excluded from the alive count, receive the configured corpse-despawn delay, and then enter the spawner's existing individual/group respawn flow.

## Kill credit, Slayer, quests and loot

On an NPC's Combat Component you may set:

- `Creature Id`
- `Slayer Category`
- `Character XP Reward`
- `Grant Loot To Killer`

When that NPC dies to framework combat, the killer's Event Router receives kill credit. This can update kill quests and Slayer automatically, while the victim's Loot Component can grant its Loot Table to the killer.

If your Loot Table already awards character XP, avoid also setting a Combat `Character XP Reward` unless you intentionally want both.

## Combat Gameplay Tags

The plugin ships standard tags:

```text
Combat.State.Attacking
Combat.State.Dodging
Combat.State.Blocking
Combat.State.GuardBroken
Combat.State.Dead
Combat.Ability.BasicAttack
Combat.Ability.Dodge
Combat.Ability.Block
Equipment.Shield
```

The combat component mirrors its main runtime states into loose GAS tags on the authoritative actor, making it easy to block abilities or drive project-specific effects from combat state.

## 1.6 combat feedback, critical stagger and knockback

Class Definition `Combat Profile` now contains `Impact FX`, `Audio`, and `Stagger` settings. Hit/critical/block/parry/stagger effects support a preferred Niagara asset and a Cascade particle fallback. Combat sounds are exposed for swing, ranged/magic attacks, hit/critical, defence, stagger, death and revive.

Critical hits can trigger a configurable one-hit stagger. The target enters `Combat.State.Staggered`, can have its current attack interrupted, plays its dedicated Stagger Montage (or Hit React fallback), and receives real character launch/physics impulse knockback. Automatic NPC combat stops navigation while staggered so the reaction is visible instead of being immediately overwritten by path following.

See `Docs/COMBAT_FEEL.md` for the complete editor setup and tuning reference.


## Automatic retaliation and ally assist

`ARPGAICharacter` now binds its AI Combat component to received combat-hit events. By default, neutral or missing-faction attackers are remembered temporarily and become valid retaliation targets, so passive wildlife fights back instead of returning immediately to free roam. Nearby allies can join through faction, spawn-group, explicit Assist Group Id, or same-class fallback rules. See `AI_AGGRO_ASSIST.md`.

## 2.3 melee interruption and NPC attack cadence

Ordinary damaging hits no longer replace an attack montage that is already playing. This is deliberate poise/state synchronization: damage still lands, but the attack only visually interrupts when a real interrupt state occurs (critical stagger, parry/guard break, death, etc.).

Automatic melee AI also no longer calls Basic Attack again while its current attack is active. This prevents automatic combo-buffer spam. `Attack Slot Cooldown After Attack` now provides post-recovery breathing room for solo melee NPCs too, so fast wildlife such as chickens can be slowed without Blueprint timers.

Critical stagger is still critical-only. For testing, use `Critical Chance = 1.0` on the attacker and `Critical Stagger Chance = 1.0` on the defender; after confirming montage/audio/knockback, restore balanced values.
