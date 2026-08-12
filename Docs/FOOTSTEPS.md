# Automatic Replicated Footsteps — v2.8.0-alpha

`AARPGCharacter` now includes an inherited **Footsteps** component. Because `AARPGAICharacter` derives from `AARPGCharacter`, the same system is automatically present on players and NPCs without adding components or animation notifies.

## What it does

- Generates footsteps from **real horizontal travel distance**, not animation playback time.
- Requires grounded `CharacterMovement`, so ordinary falling/flying movement does not create fake steps.
- Alternates left/right feet and uses `foot_l` / `foot_r` by default when those sockets/bones exist.
- Falls back to capsule-bottom left/right positions when a skeleton uses different foot names.
- Traces only when a step is due, then reads the hit Physical Material's **Surface Type**.
- Chooses a random sound from the matching surface pool and avoids an immediate repeat when possible.
- Scales volume with movement speed and supports per-surface volume/pitch variation.
- Supports optional Sound Attenuation and Sound Concurrency assets.
- Runs authoritative automatic cadence on the server for players and NPCs.
- Locally predicts the owning player's automatic sound to avoid audible network-input latency, while the server multicasts the step to other relevant clients.
- Uses an **Unreliable NetMulticast** for transient footsteps; no replicated persistent footstep state is added.
- Uses a staggered timer rather than permanent Actor/Component Tick. No automatic sampling timer is created at all until at least one valid sound is configured, and ground traces happen only when a foot contact is actually due.

## Fast setup

1. Open a Blueprint derived from `ARPGCharacter` or `ARPGAICharacter`.
2. Select the inherited **Footsteps** component.
3. Under `Footsteps -> Audio`, add several assets to **Default Sounds**. This alone is enough to hear automatic footsteps on every walkable surface.
4. Assign a **Sound Attenuation** asset so footsteps behave as proper 3D world audio.
5. Optionally assign a **Sound Concurrency** asset to cap dense NPC crowd playback.
6. Play. No Blueprint movement wiring and no animation notify is required.

The framework intentionally ships without hard-coded audio assets. The component is enabled by default, but it remains silent until you assign at least one valid sound.

## Surface-specific footsteps

Unreal surface authoring uses **Project Settings -> Physics -> Physical Surface** plus Physical Material assets.

Example surface labels:

- SurfaceType1 = Grass
- SurfaceType2 = Stone
- SurfaceType3 = Wood
- SurfaceType4 = Metal
- SurfaceType5 = Dirt
- SurfaceType6 = WaterShallow

For each material family:

1. Create/assign a Physical Material to the world material.
2. Set that Physical Material's Surface Type.
3. In the character's **Footsteps -> Audio -> Surface Audio** array, add an entry with the same Surface Type.
4. Add 3-8 variations to that entry's Sounds array.
5. Tune its Volume Multiplier and Min/Max Pitch if desired.

If there is no matching entry, the system falls back to **Default Sounds**.

## Important trace requirement

The contacted floor must block the configured **Trace Channel** (Visibility by default), otherwise that foot contact is intentionally skipped. This avoids playing footsteps when no valid floor can be resolved.

## Automatic cadence tuning

The default cadence is designed as a general third-person RPG baseline:

- Sample Interval: `0.05 s`
- Minimum Ground Speed: `70 cm/s`
- Walk Step Distance: `145 cm`
- Run Step Distance: `185 cm`
- Run Reference Speed: `600 cm/s`
- Teleport Reset Distance: `260 cm`

Cadence is distance-driven. Faster movement covers the required stride distance sooner, while the stride length itself blends between the walk/run values. Large movement corrections/teleports reset accumulated stride distance instead of producing burst footsteps.

## Foot placement

Default names are:

- Left Foot Socket: `foot_l`
- Right Foot Socket: `foot_r`

A skeletal bone can be addressed through Unreal's socket lookup path, so these defaults work with common mannequin-style naming. If your skeleton uses different names, change the two fields. If neither exists, **Fallback Foot Separation** positions traces on either side of the capsule bottom.

## Multiplayer behavior

### Player character

- Server: measures authoritative movement and emits the replicated footstep cue.
- Owning client: independently measures its local movement and immediately plays its own predicted cue.
- Server multicast: plays on other relevant clients, but skips the predicting owning client to prevent a duplicate sound.
- Listen-server host: the server execution plays the cue locally once; there is no separate client prediction path.

### NPC

NPCs use server-only cadence generation. The server resolves ground/surface/audio and multicasts the transient cue to relevant clients.

### Dedicated server

The server performs cadence/ground resolution but never attempts local audio playback.

## Manual / animation-accurate mode

Automatic mode is the default and needs no animation setup. For a project that later wants authored animation contacts:

1. Call `Set Automatic Footsteps Enabled(false)`.
2. From your animation/gameplay path call `Trigger Footstep`, passing Left/Right.

Client manual calls are locally predicted and sent through a rate-limited unreliable Server RPC; the server re-traces the floor and multicasts its authoritative result to everyone else. This keeps surface selection authoritative instead of trusting a client-provided sound/surface/location.

## Performance notes

- No permanent tick.
- Characters with no configured footstep audio do not create an automatic sampling timer.
- Sampling timers are staggered so large NPC groups do not all evaluate on the same frame.
- No ground trace is performed until accumulated movement says a footstep is actually due.
- Replication is event-based and unreliable because old/lost footstep sounds should not queue or arrive late.
- Actor network relevancy remains the outer replication filter; attenuation/concurrency then control actual local audio presentation.

## Recommended content practice

For a polished result, use multiple short mono/stereo-compatible footstep source variations per surface, an attenuation asset sized for character audio, and a concurrency asset appropriate for crowd density. Keep physical-surface assignments consistent across Landscape, static meshes and building materials so the same character profile behaves automatically across the whole world.
