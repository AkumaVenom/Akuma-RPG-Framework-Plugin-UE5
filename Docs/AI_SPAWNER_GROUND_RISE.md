# AI Spawner Ground-Rise Entrance — v2.9.0-alpha

`ARPGAISpawner` now gives spawned framework NPCs a polished emergence instead of an instant visual pop. The feature is enabled by default and requires no Blueprint graph wiring for `AARPGAICharacter` subclasses.

## Safety model

The entrance deliberately does **not** lower the actual pawn/capsule below the terrain. `SpawnOne()` first uses the existing v2.7.2 collision-safe `AdjustIfPossibleButDontSpawnIfColliding` path and accepts a real final spawn position. The new `SpawnEntrance` component then applies a temporary negative **relative Z offset only to the Character skeletal mesh**. The capsule, NavMesh anchor, leash origin and replicated actor location remain valid at all times.

This prevents the presentation layer from reintroducing the stuck/encroaching spawned-AI bug that the v2.7.2 collision proof fixed.

## Automatic coverage

The entrance is invoked from the common `SpawnOne()` path, so it applies consistently to:

- initial BeginPlay groups;
- individual respawns;
- whole-group respawns;
- distance-population reloads;
- Day/Night population swaps;
- explicit Blueprint/native calls that ultimately use `SpawnOne()` / `SpawnGroup()`.

`AARPGAICharacter` creates a replicated `SpawnEntrance` component automatically. A custom Pawn class that does not contain `UARPGSpawnEntranceComponent` still spawns normally; the spawner logs a Verbose diagnostic and skips only the presentation.

## Spawner settings

Select the spawner and use **Spawn -> Ground Rise Entrance**:

- **Enable Ground Rise Entrance** — master switch, enabled by default.
- **Auto Calculate Ground Rise Depth** — uses approximately one full scaled capsule height so differently sized NPCs begin fully below the floor.
- **Ground Rise Depth** — manual visual sink depth when automatic depth is disabled.
- **Extra Ground Rise Depth** — additional burial depth added to automatic capsule-height calculation.
- **Ground Rise Duration** — rise time; default `1.15 s`.
- **Ground Rise Start Delay** — optional below-ground hold before movement begins; default `0.05 s`.
- **Ground Rise Ease Exponent** — ease-out weight; default `2.25` for a quick emergence with a soft finish. `1.0` is effectively linear.
- **Suspend AI Behaviour During Ground Rise** — temporarily disables framework AI Combat and ambient Social decision making in addition to locomotion ownership.
- **Lock Actor Location During Ground Rise** — advanced safeguard that rejects custom/external translation attempts during the reveal while still allowing rotation/facing.

## Movement ownership

When the entrance starts on authority it:

1. stops the current `AAIController` path request;
2. saves and disables `CharacterMovement`;
3. acquires its own `SpawnerGroundRise` Wanderer pause reason;
4. pauses an active spline route without destroying route progress;
5. optionally suspends AI Combat and Social behaviour;
6. records the accepted actor location as the temporary translation lock.

When the reveal completes it stops any stale path request one final time, restores CharacterMovement only if the entrance still owns `MOVE_None`, releases only its Wanderer pause token, resumes only a spline route that is still active, and restores only AI systems that were enabled before the entrance.

This ownership model is intentionally compatible with the framework's existing Combat, Social and Group Cohesion pause reasons. One system cannot accidentally release another system's pause.

## Replication and performance

The spawner remains server-only. The `SpawnEntrance` component on each replicated AI carries a small replicated state containing sequence, start server time, delay, duration, depth and easing. Clients use `GameState` synchronized server time to evaluate the same local mesh offset.

The rise therefore does **not** multicast or replicate a transform every frame. Component Tick is disabled normally and enabled only for the short active entrance; once the rise reaches completion, Tick is disabled again on both server and clients.

## Recommended PIE checks

Test with at least one Free Roam spawner and one Spline spawner:

1. Watch initial spawn: body should begin below terrain and rise smoothly while the capsule stays in its valid standing location.
2. Confirm no NPC translates during the reveal.
3. Confirm Free Roam begins after the reveal and still passes the v2.7.2 real-translation proof.
4. Confirm Spline NPCs resume route travel after the reveal without jumping or restarting at an incorrect route point.
5. Kill an NPC and verify its respawn uses the same entrance.
6. Leave/re-enter a distance-streamed population and verify every newly created NPC uses the entrance.
7. If Day/Night population swapping is enabled, cross midnight/day-start and verify replacement NPCs also rise correctly.
8. Test a large and small capsule with automatic depth enabled.
9. Test multiplayer PIE and confirm clients see the same rise timing and no duplicate/root-motion translation.
10. Attack an NPC during the entrance and confirm death does not restore walking into the dead/ragdolled pawn.

Repository model/static validation is not a substitute for an actual UE 5.8/5.8.1 compile and PIE/package test.
