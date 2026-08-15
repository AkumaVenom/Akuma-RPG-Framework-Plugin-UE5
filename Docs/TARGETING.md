# Lock-On Targeting — Akuma's RPG Framework 1.2

`ARPGTargetingComponent` is the framework's first-class player lock-on layer. It is already created by `ARPGCharacter`, so the standard setup does not require a targeting Blueprint graph, a target-marker spawn graph, or per-ability rotation logic.

## Fastest player setup

Create/keep your player Blueprint derived from `ARPGCharacter`.

Select the inherited **Targeting** component and configure the values you care about. Defaults are already usable.

Bind your lock-on input to:

```text
Toggle Lock On
```

Optional target-cycle inputs:

```text
Target Left
Target Right
```

That is the normal setup.

## What Toggle Lock On does

When unlocked, `Toggle Lock On` searches Pawn collision candidates around the player, filters invalid/dead/non-hostile actors, applies the configured distance and line-of-sight rules, then scores remaining targets by camera-center angle and distance.

When already locked, the same function releases the target.

The selected actor is also sent to `ARPGCombatComponent::CombatTarget`. In multiplayer the server re-validates the target and distance before accepting it and confirms/reconciles the local selection.

## Hostile faction filtering

`Only Hostile Targets` defaults to true. If both actors have `ARPGFactionComponent`, the relationship must be hostile for the candidate to be selected.

This means a typical game should author a negative relationship between the Player faction and enemy factions. Set `Only Hostile Targets` false if your game intentionally permits locking neutral/damageable actors.

## v2.15.40 relog target-integrity fix

Lock-on candidate filtering itself was not the relog root cause. The inherited player `Persistence` component could run on `AARPGAICharacter`, assign the account `LastCharacterId`, and load the player's character save into an enemy. That corrupted the enemy's runtime faction/state before targeting evaluated it. v2.15.40 makes account character persistence player-only, so reloaded/spawned NPCs retain their authored/spawner faction and remain valid hostile candidates after relog.

From v2.15.39, hostile filtering also handles two runtime integrity cases: an empty/unresolved faction identity no longer counts as a resolved neutral relationship merely because both actors own Faction components, and an NPC AI that explicitly considers the player hostile can be reciprocally lockable even when the base faction relation is temporarily neutral/unknown.

## Target marker

The marker is automatic and local to the owning player's UI. Remote clients do not spawn another player's target marker.

Exposed Targeting Component properties include:

- `Auto Create Target Marker`
- `Target Marker Widget Class`
- `Target Marker Texture`
- `Target Marker Material`
- `Target Marker Color`
- `Target Marker Size`
- `Target Marker Height Offset`
- `Target Marker Socket Name`
- acquire animation duration
- pulse speed/scale
- release animation duration

If neither texture nor material is assigned, `ARPGTargetMarkerWidget` provides a native fallback reticle so the system is immediately visible in PIE.

### Custom marker UI

You can create a Widget Blueprint derived from `ARPGTargetMarkerWidget` and assign it to `Target Marker Widget Class`.

The native base exposes Blueprint events:

- `On ARPG Marker Configured`
- `On ARPG Target Acquired`
- `On ARPG Target Released`

This allows a project to replace or augment the default scale/fade/pulse presentation with a custom Widget Animation while keeping automatic spawning and target routing.

## Aim and marker sockets

`Target Aim Socket Name` controls the point used for target direction/LOS/facing when a target is a Character. `Target Marker Socket Name` controls marker placement.

For example, a humanoid project may use a head or upper-spine socket. Leave either property `None` to use automatic actor-bounds placement instead.

## Losing a target

A lock is automatically invalidated when the target:

- dies through `ARPGCombatComponent`
- is destroyed
- moves past `Max Maintain Distance`
- ceases to meet the faction/damage rules
- loses line of sight longer than the configured grace period when maintain-LOS is enabled

`Auto Reacquire When Target Lost` optionally tries to select the next best target after an automatic loss.

## Target switching

`Target Left` and `Target Right` select the nearest appropriate screen-side target relative to the current target. `Wrap Target Switching` can wrap to the opposite side when there is no further target in the requested direction.

## Basic attacks

Calling the ready Character function:

```text
Basic Attack
```

with no explicit actor automatically resolves the current lock-on target and passes it to combat. The targeting component opens an action-facing window so the player rotates toward the target during the attack. The melee trace also uses target-aware direction when the class combat profile has `Auto Face Combat Target` enabled.

This means the normal player input remains:

```text
Attack Input -> Basic Attack
```

No target actor needs to be wired into the attack call.

## Player facing

Important settings:

- `Auto Face On Basic Attack`
- `Auto Face On Abilities`
- `Face Continuously While Locked`
- `Smooth Facing`
- `Rotation Interp Speed`
- `Action Facing Duration`
- `Ignore Pitch When Facing`

The default behavior leaves ordinary locomotion alone while locked and rotates toward the target during attacks/abilities. Enable `Face Continuously While Locked` for a more constant strafe-facing style.

## Gameplay Ability System integration

The targeting component listens to the owner's `UAbilitySystemComponent::AbilityActivatedCallbacks`, so normal GAS abilities automatically trigger the configured ability-facing window. Unreal's Gameplay Ability System remains the authority for ability execution/replication.

An ability with asset tag:

```text
Combat.Targeting.IgnoreAutoFace
```

will not trigger automatic lock-on facing.

The Character's `ARPGAbilityBridgeComponent` exposes:

- `Get Preferred Ability Target`
- `Make Preferred Ability Target Data`
- `Try Activate Ability By Tag With Target`

## ARPGGameplayAbility

For new target-aware Gameplay Ability Blueprints, `ARPGGameplayAbility` is the recommended parent class.

It exposes `Targeting Policy`:

- **Ignore Lock-On** — the ability does not participate in ARPG automatic lock-on facing.
- **Prefer Lock-On Target** — the ability can consume the current target if present, but does not require one.
- **Require Lock-On Target** — `CanActivateAbility` fails unless a valid lock-on target exists.

It also exposes:

- `Auto Face Lock On Target`
- `Maximum Lock On Target Range` (`0` means no extra ability-specific range cap)
- `Get Lock On Target`
- `Has Valid Lock On Target`
- `Get Lock On Target Location`
- `Make Lock On Target Data`

This allows an ability Blueprint to feed the exact locked actor into Gameplay Effects/Ability Tasks without rebuilding target-selection logic.

## Multiplayer model

Target discovery and the visual marker run locally for responsive UI. The chosen actor is sent through the replicated player-owned targeting/combat path. The server validates that the actor is still a legal target and within maximum maintenance range before accepting it.

`CombatTarget` is replicated and has a RepNotify event, allowing the local Targeting Component to reconcile changes such as server clearing the target on death.

The target marker itself is cosmetic and is intentionally not replicated.

## Blueprint events

`ARPGTargetingComponent` exposes:

- `On Target Changed(Old Target, New Target)`
- `On Lock State Changed(Is Locked On)`

Use these for optional target frames, target health bars, audio cues, controller rumble, accessibility UI or game-specific presentation. The core lock-on does not require them.

## 1.6 Z-target camera polish

The default lock-on presentation is now intentionally stronger than the original 1.2 action-facing window. `Face Continuously While Locked` and `Lock Camera To Target` default to true.

While locked, the owning PlayerController's control rotation continuously tracks the target and the character continuously faces it. For normal third-person rigs the component automatically finds an inherited Spring Arm first (or a direct Camera Component as fallback), caches its existing `Use Pawn Control Rotation` value, enables it for the lock and restores the original setting when the lock is released.

For player characters, movement-facing flags are cached and temporarily changed so `Orient Rotation To Movement` cannot turn the character away from the selected opponent. This produces a much more natural strafe/orbit combat stance. The settings are restored on unlock.

Tune `Camera Rotation Interp Speed`, pitch tracking/clamps and `Rotation Interp Speed` to change the weight of the lock. Disable `Lock Camera To Target` if a project uses a completely custom camera manager and only wants the character-facing/target-selection layer.
