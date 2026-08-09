# Automatic NPC Ragdoll Death — Akuma's RPG Framework 1.3

`ARPGAICharacter` now uses physical ragdoll automatically when it dies. No Blueprint death event, physics nodes or Behavior Tree logic are required.

## Default NPC behavior

For any Blueprint whose parent is `ARPGAICharacter`:

1. Framework combat reaches zero Health.
2. Combat enters `Dead`, clears attack/block/dodge state and disables movement.
3. The character mesh attempts ragdoll using its assigned Physics Asset.
4. The capsule is disabled for corpse collision and the skeletal mesh switches to the configured ragdoll collision profile.
5. Existing movement velocity is transferred to the body so running/falling enemies do not unnaturally stop before collapsing.
6. The final hit direction can apply a small configurable physical impulse at the nearest physics body to the hit location.
7. If ragdoll cannot start, the normal Class Definition / Combat `Death` montage plays instead.
8. Spawner-owned NPCs remain compatible with the existing corpse despawn and respawn flow.

## Required art setup

For physical ragdoll, the NPC Skeletal Mesh needs a valid **Physics Asset** with usable bodies/constraints. Most humanoid UE characters already have one. If it does not, the framework deliberately falls back to the configured death montage rather than leaving the NPC in a broken physics state.

## Exposed Combat -> Death settings

`Death Presentation` is available on `ARPGCombatComponent`:

- `Use Ragdoll On Death` — enabled automatically on `ARPGAICharacter`.
- `Fallback To Death Montage` — enabled automatically on `ARPGAICharacter`.
- `Disable Capsule Collision` — prevents the dead navigation capsule from fighting the ragdoll body.
- `Transfer Character Velocity` — carries running/falling velocity into the corpse.
- `Apply Hit Direction Impulse` — adds physical response from the killing hit.
- `Hit Direction Impulse Velocity` — strength of that response.
- `Ragdoll Collision Profile` — defaults to `Ragdoll`.
- `Simulate On Dedicated Server` — off by default to avoid spending dedicated-server CPU on visual corpse physics; listen server and clients simulate normally.

## Death montage fallback

The fallback montage is resolved exactly like the existing combat animation system:

1. Class Definition -> Animation Set -> Death, when class-driven combat is enabled.
2. Combat Component -> Animation -> Death, when using the component override.

The montage is **not** played on a successful NPC ragdoll. It is only used when ragdoll is disabled or cannot start.

## Player ragdoll

The base `ARPGCharacter` deliberately leaves `Use Ragdoll On Death` off so current player death/respawn animation behavior is unchanged. To give the player the same physical death system, enable it under the player's inherited Combat Component -> Death Presentation.

## Respawn safety

When the same character actor is respawned, the framework stops physics and restores the cached mesh/capsule state before teleport/revive. This prevents a respawned character from remaining simulated, offset from the capsule, or stuck with corpse collision.
