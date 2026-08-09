# AI Spawner Movement, Groups and Free Roam — v1.5.0-alpha

`ARPGAISpawner` now treats **spawn membership**, **physical cohesion**, and **movement mode** as separate systems. This is intentional: a spawn group can remain one respawn/defeat cohort without forcing every NPC to stand together.

## Recommended setup

Place `ARPG_AI_Spawner`, fill `Spawn Table`, choose `Min Group Size` / `Max Group Size`, then choose `Movement Mode`:

- **Automatic (Spline If Assigned)** — preserves the earlier workflow. If `Assigned Spline Route` is set, spawned AI follow it; otherwise no automatic travel behavior is enabled.
- **Spline Route** — explicitly use `Assigned Spline Route`.
- **Free Roam** — independently or cooperatively wander through NavMesh around the spawner.
- **No Automatic Travel** — spawn and combat work, but the spawner does not own idle travel.

Normal `ARPGAICharacter` already contains both AI Spline Movement and AI Wanderer. The Wanderer stays disabled until Free Roam requests it.

## Spawn group versus Stay Together

Group size still controls the spawner's desired live population and Whole Group / Individual respawn behavior. `Stay Together` only controls physical cohesion.

### Independent free-roam group

Example:

- Min Group Size = 5
- Max Group Size = 5
- Movement Mode = Free Roam
- Stay Together = false
- Free Roam Radius = 2500
- Free Roam Leashed To Spawner = true

The five NPCs still belong to one spawner group, but each chooses its own reachable NavMesh destinations inside the spawner-centered roam area. They can fight separately and are not dragged back toward one another.

### Cohesive free-roam group

Set `Stay Together = true`. The spawner selects a living Group Leader. The leader free-roams around the spawner; followers normally roam near the leader. If a follower exceeds `Group Cohesion Radius`, its random wandering pauses and Navigation moves it back inside `Group Recovery Fraction` of the cohesion radius. Random free roam then resumes. Combat takes priority and cohesion does not pull an NPC away from an active fight.

## Spline groups that do not split direction

`Synchronize Spline Group Direction` is enabled by default and works independently from `Stay Together`.

The first living member is the route-direction leader. Followers mirror that member's forward/reverse direction. On an open route that reverses at endpoints, a follower that reaches the endpoint early waits rather than independently flipping direction. When the leader reverses, all followers reverse their NavMesh route travel too.

When `Stay Together` is also enabled, the same leader supplies physical route cohesion. A follower that exceeds `Group Cohesion Radius` asks its own spline component to rejoin the leader's current route progress through Navigation; no actor attachment or spline teleport is used. With Stay Together disabled, this recovery is removed but shared direction can remain enabled.

`Group Spline Direction` chooses the group's starting direction:

- Forward
- Reverse
- Random Per Spawn Group

A newly respawned member inherits the active group's route direction rather than choosing an unrelated direction.

## Route looping

The loop setting now belongs primarily to `ARPG AI Spline Route`:

- `Loop Route = true` — default; the route repeats.
- `Closed Loop Geometry = true` — final spline point connects to the first and travel wraps continuously.
- `Reverse At Open Ends = true` — default for open looping routes; the route reverses at endpoints.
- disable `Reverse At Open Ends` on an open looping route to make AI return to the opposite endpoint through normal NavMesh pathfinding instead.

`AI Spline Movement -> Use Route Traversal Settings` is enabled by default. Disable it only when one individual NPC needs to override the route actor with its own Once / Loop / Ping-Pong policy.

## Free-roam leash behavior

`Free Roam Leashed To Spawner` makes the Wanderer use the spawner location as its home and select random reachable points inside `Free Roam Radius`. The spawner's existing hard `Stay In Range / Max Leash Distance` remains a second safety boundary.

For a solo roaming NPC, set Min/Max Group Size to 1 and Movement Mode to Free Roam. The exact same system works without group-specific behavior.

## Combat priority

Movement ownership order is:

1. death/ragdoll
2. active combat
3. group cohesion recovery (when not fighting)
4. spline route or free roam
5. idle

The Wanderer checks the shared ARPG combat state before choosing a random destination. Spline movement already suspends/rejoins around combat. This prevents patrol/free-roam systems from repeatedly overriding combat chase movement.

## Blueprint runtime controls

The normal editor workflow needs no Blueprint logic. Advanced runtime control is available through:

- `Set Movement Mode`
- `Set Stay Together`
- `Set Assigned Spline Route`
- `Spawn Group` / `Spawn One`
- `Despawn All`

The inherited AI Wanderer also exposes `Set Wanderer Enabled`, `Set Home Location`, `Force Choose New Destination` and `Force Return Home`.
