# AI Spawner Distance Population Streaming — v1.10.0-alpha

`ARPGAISpawner` can now stream its NPC population by player distance. The normal/default workflow is automatic: place the spawner, configure its spawn table, and leave **Enable Distance Based Population** enabled. Far-away spawners remain unloaded and do not keep AI actors, combat components, movement logic, skeletal meshes, animation or replication alive unnecessarily.

## Recommended setup

On the placed `ARPG AI Spawner`:

- **Enable Distance Based Population** = true
- **Auto Spawn When Player Is Near** = true
- **Spawn Activation Radius** = 6000 cm
- **Despawn Radius** = 8000 cm
- **Population Check Interval** = 1.25 s
- **Distance Despawn Delay** = 3 s
- **Use 2D Player Distance** = true
- **Keep Loaded Near Spawned Pawns** = true
- **Prevent Distance Despawn While In Combat** = false
- **Reroll Group Size On Distance Reload** = false

With distance population enabled, it intentionally supersedes unconditional `Spawn On Begin Play`: the population waits until a player is relevant instead of spawning every world NPC at map start. Disable distance population on a specific spawner if it must always exist.

## Activation and hysteresis

An unloaded spawner measures player-controlled pawns from the **spawner origin**. When the nearest player enters **Spawn Activation Radius**, the spawner creates its group normally.

A loaded population uses the larger **Despawn Radius**. The framework automatically enforces a minimum hysteresis gap even if a designer accidentally enters a despawn radius smaller than the activation radius. A player must also remain outside the despawn radius for **Distance Despawn Delay** before unload occurs. This avoids rapid edge flicker/pop when a player moves around the boundary.

## Long spline/free-roam routes

`Keep Loaded Near Spawned Pawns` is enabled by default. While a population is active, a player near either the spawner **or any spawned NPC** keeps that population relevant. This matters for long spline routes: an NPC can travel far from the spawner and remain loaded while the player is actually beside it.

Inactive spawners still activate from their own origin because no spawned NPC anchors exist yet.

## Clean unload: not a death

Distance unload removes the spawner's destruction delegate before destroying its temporary runtime pawns. Therefore distance streaming does **not**:

- broadcast Spawn Group Defeated;
- grant kill/Slayer/quest credit;
- produce loot/XP as a death;
- schedule a fake death respawn;
- treat unload as ragdoll/death combat presentation.

When the player returns, the spawner rebuilds the population from the spawner through its normal spawn configuration.

## Respawn correctness

The spawner remembers its desired group size by default instead of randomly changing population every time the area streams in. `Reroll Group Size On Distance Reload` can opt back into a fresh Min/Max roll.

Real death respawn cooldowns survive distance unload. Example: a five-creature Individual-respawn group has three living creatures and two dead creatures still waiting 12 seconds. If the player leaves and immediately returns, the three living members can be reconstructed immediately while the other two keep the remaining respawn delay.

`Respawn Mode = Never` is also protected: defeated Never-respawn members are never resurrected merely because the player unloaded/reloaded the area.

## Performance behavior

The spawner never enables Actor Tick. Distance relevance uses a server-only timer. The timer's initial delay is randomized so many spawners are distributed across frames rather than checking simultaneously.

While inactive, the normal leash and group-cohesion timers are stopped. Only the low-frequency relevance timer remains. Once the population activates, normal corpse cleanup, leash, group cohesion, spline/free-roam and combat systems resume.

For very large worlds, increase **Population Check Interval** to 2–5 seconds on distant/low-priority wildlife spawners. The separate despawn delay keeps this responsive without requiring per-frame checks.

## Combat retention

`Prevent Distance Despawn While In Combat` is OFF by default because performance unloading should win after all players have left the area. Enable it for special encounter spawners that must finish an NPC-vs-NPC battle before they are allowed to unload.

## Blueprint API

Runtime status and control nodes are exposed under `ARPG | AI Spawner | Performance`:

- `Is Population Active` (pure)
- `Get Nearest Relevant Player Distance` (pure; `-1` means no player pawn found)
- `Is Player Inside Spawn Activation Radius` (pure)
- `Evaluate Population Relevance Now`
- `Set Population Active`
- `On Population Activated`
- `On Population Deactivated`

Normal games should rarely need to call these manually; the distance timer owns the default lifecycle.

## Day/night population integration (v2.4)

Distance streaming and midnight population swapping cooperate rather than competing. When `Enable Midnight Population Swap` is enabled and a spawner is distance-unloaded, midnight/morning changes update the selected population phase and discard obsolete preserved-count/respawn state **without spawning actors in the unloaded area**. The next distance activation spawns only the currently correct table.

This means a group preserved before midnight cannot be resurrected as daylight NPCs at 02:00 simply because the player left and returned. Conversely, when the feature is disabled, the original distance-population preservation path is unchanged.
