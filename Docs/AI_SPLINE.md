# Automatic AI Spline Routes — v1.5.0-alpha

`ARPGAICharacter` now includes an inherited **AI Spline Movement** component. It is a server-authoritative patrol/travel system that follows a designer spline through Unreal Navigation instead of attaching the pawn to the spline.

## Fast setup — placed NPC

1. Place **ARPG AI Spline Route** in the level.
2. Edit its `RouteSpline` control points in the viewport so the path sits over valid NavMesh.
3. Place an NPC whose parent class is `ARPGAICharacter`.
4. Select the NPC instance -> **AI Spline Movement** -> assign `Route` to the route actor.
5. Leave `Enabled` and `Auto Start` enabled.
6. Play.

No Blueprint movement graph, Behavior Tree, spline attachment, timeline, or manual `AI Move To` graph is required.

The component samples route locations at `Follow Step Distance`, projects each goal to Navigation, then uses `AAIController::MoveToLocation`. Character Movement performs normal movement and replication.

## Blueprint-class setup with Route Id

A Blueprint class cannot store a direct reference to a specific level actor in its class defaults. For reusable NPC classes:

1. Give the `ARPG AI Spline Route` a `Route Id`, for example `VillageGuardNorth`.
2. On the NPC Blueprint's inherited **AI Spline Movement** component set the same `Route Id`.
3. Keep `Auto Find Route By Id` enabled.

At runtime the component resolves the nearest route carrying that id and starts automatically.

## Spawner setup

`ARPG_AI_Spawner` can assign a route automatically to every compatible spawned pawn:

- `Movement Mode` = Automatic or Spline Route
- `Assigned Spline Route` = desired level route
- `Auto Start Assigned Spline Route` = true
- `Route Overrides Spawner Leash` = true (recommended)
- `Synchronize Spline Group Direction` = true (recommended for group spawns)

The spawned pawn still needs an `AI Spline Movement` component; `ARPGAICharacter` already includes one. At runtime, `Set Assigned Spline Route` can change the route and optionally apply it immediately to already-spawned pawns. Free-roam/group-cohesion behavior is documented in `Docs/AI_SPAWNER_MOVEMENT.md`.

## Route-level loop behavior (recommended)

Traversal is now controlled from the **ARPG AI Spline Route** by default:

- `Loop Route = true` — enabled by default; prevents ordinary patrols from permanently stopping at an endpoint.
- `Closed Loop Geometry = true` — physically closes the spline and wraps continuously.
- `Reverse At Open Ends = true` — for an open loop, reverse at each endpoint and follow the same spline back.
- disable `Reverse At Open Ends` to return to the opposite endpoint using NavMesh rather than attachment or teleportation.

The NPC's `AI Spline Movement -> Use Route Traversal Settings` is enabled by default. Advanced per-NPC overrides can disable it and choose the original Once / Loop / Ping-Pong enum.

### Group direction synchronization and cohesion

When an `ARPGAISpawner` creates multiple NPCs on one open reversing route, the group has one route-direction leader. Followers mirror that direction and wait at an endpoint if they arrive before the leader. This prevents one member from reversing early and making the group run both directions on the same spline. The synchronization can remain enabled even when `Stay Together` is disabled.

If `Stay Together` is enabled and a spline follower exceeds `Group Cohesion Radius`, its own spline component re-targets the leader's current route progress and rejoins that area through NavMesh. It is still never attached to the leader or spline. Turning Stay Together off disables this physical route-progress recovery while preserving the optional shared travel direction.

## Route start modes

- `Nearest Location On Route` — recommended default for placed/spawned NPCs.
- `First Route Point`
- `Last Route Point`
- `Explicit Route Point`
- `Random Route Point`

If the pawn begins away from the spline, it first navigates to the selected/nearest route anchor before advancing along the route.

## Following quality

`Follow Step Distance` controls how closely navigation goals trace the spline shape:

- smaller value = follows tight curves more accurately, more move requests
- larger value = fewer requests, suitable for broad roads/long routes

Default is `300` Unreal units.

Other useful settings:

- `Acceptance Radius`
- `Update Interval`
- `Lateral Offset`
- `Randomize Lateral Offset`
- `Random Lateral Offset Range`
- `Navigation Filter Class`
- `Allow Partial Paths`
- `Use Pathfinding`
- `Nav Projection Extent`
- stalled-move detection/retry

Random lateral offset is useful when multiple guards share the same road so they do not all occupy an identical centerline.

## Route point waits and events

`ARPG AI Spline Route` exposes one settings entry per spline control point:

- `Point Id`
- `Wait Time`
- `Random Wait Time`
- `Face Spline Direction While Waiting`

`On Route Point Reached(PointIndex, PointId)` is Blueprint-assignable. Use it for optional route-specific behavior such as an emote, guard inspection animation, dialogue bark, gate interaction, or scripted event. Basic route following does not require any event graph.

## Combat integration

Combat ownership has priority over route movement.

When `ARPGAICombatComponent` acquires a target:

1. spline movement suspends immediately;
2. the current route position becomes the combat leash anchor when `Use Route As Combat Leash Anchor` is enabled;
3. normal ARPG combat AI chases, attacks, uses abilities, blocks and dodges;
4. route movement does not issue competing navigation requests;
5. after combat, the NPC waits `Combat Resume Delay` and rejoins through NavMesh;
6. it then continues the route automatically.

`Combat Resume Mode` controls recovery:

- `Rejoin At Nearest Route Location` — recommended; finds the closest route position to where combat ended.
- `Return To Previous Route Progress` — returns to the progress position where the NPC left the patrol.

The combat leash now only constrains an active combat chase, so long route patrols are not incorrectly pulled back to the original spawn position.

## Wanderer integration

If an NPC also has `ARPGWandererComponent`, an active spline route temporarily owns non-combat movement and suppresses wandering. When the route stops or finishes, the previous Wanderer enabled state is restored. Combat does not accidentally re-enable Wanderer while the spline route is active.

## Death / ragdoll / respawn

Dead NPCs stop route movement automatically because the spline component checks the shared Combat life state. Existing v1.3 automatic NPC ragdoll and death-animation fallback remain unchanged. A newly spawned replacement receives its assigned spawner route and starts it again automatically.

## Multiplayer

Spline movement decisions run on authority. Unreal AI controllers exist on the server; normal Character Movement replicates the resulting pawn motion. The spline route actor itself is navigation/designer data and does not need to drive client movement directly.

## Blueprint API

The automatic path is the intended default, but advanced projects can call:

- `Set Route`
- `Find And Assign Route By Id`
- `Start Route`
- `Stop Route`
- `Pause Route`
- `Resume Route`
- `Rejoin Nearest Route Location`
- `Notify Combat Started`
- `Notify Combat Ended`
- `Is Route Active`
- `Get Current Route Anchor Location`
- `Get Current Spline Direction`

Events:

- `On Route Changed`
- `On Route Started`
- `On Route Paused For Combat`
- `On Route Resumed After Combat`
- `On Route Point Reached`
- `On Move Failed`
- `On Route Finished`

## NavMesh requirement

The route is guidance, not a replacement for Navigation. Keep a valid NavMesh over the route and over reasonable combat/rejoin space around it. `Project Goals To NavMesh` is enabled by default.

## UE 5.8.1 path-following include

`ARPGAISplineComponent.cpp` explicitly includes `Navigation/PathFollowingComponent.h`. In UE 5.8.1, `AIController.h` can expose only the forward declaration of `EPathFollowingRequestResult::Type`; the Path Following header supplies the concrete request-result enumerators used to detect immediate `MoveToLocation` failure.
