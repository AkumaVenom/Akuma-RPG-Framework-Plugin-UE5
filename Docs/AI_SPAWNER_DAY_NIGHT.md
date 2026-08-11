# AI Spawner — Day/Night Population Swapping

## Purpose

`ARPGAISpawner` can optionally own two time-of-day populations without Blueprint timers or level-script cleanup. The existing `Spawn Table` remains the normal/daylight population. `Midnight Spawn Table` is an additional weighted table used after true midnight.

This feature is **opt-in per spawner**. If `Enable Midnight Population Swap` is disabled, the spawner behaves exactly like the preserved pre-2.4 system: its existing spawn table, respawn rules and distance-population preservation remain in control.

## Runtime schedule

With the option enabled:

- **00:00 (midnight):** the currently loaded daylight population is cleanly removed and the `Midnight Spawn Table` becomes active.
- **00:00 → Day Start Hour:** respawns/replenishment use the midnight table.
- **Day Start Hour:** the midnight population is cleanly removed and the normal `Spawn Table` becomes active again.
- **Day Start Hour → next midnight:** normal/daylight entries remain active, including evening/night before 00:00.

The morning boundary intentionally comes from the authoritative `ARPGDayNightCycle::DayStartHour` rather than a second duplicated time setting. For the default Day/Night actor this is 06:00.

## Editor setup

On an `ARPGAISpawner` instance or Blueprint defaults:

1. Configure the normal `Spawn Table` exactly as before. This is the daylight/default population.
2. Enable **Spawn → Day Night Population → Enable Midnight Population Swap**.
3. Add one or more weighted entries to **Midnight Spawn Table**.
4. Optional: enable **Use Separate Midnight Group Size** and set `Midnight Min Group Size` / `Midnight Max Group Size`.
5. Usually leave `Day Night Cycle Override` empty. The spawner automatically discovers the first `ARPGDayNightCycle` in the world. Assign the override only when a map contains multiple clocks or for explicit testing.

Example:

```text
Spawn Table (daylight)
  Chicken       Weight 5
  Deer          Weight 2

Midnight Spawn Table
  Skeleton      Weight 4
  Night Wolf    Weight 2
```

At 23:59 the daylight group remains. At 00:00 those actors are removed and a new weighted midnight group is created. At `DayStartHour`, midnight leftovers are removed and a fresh daylight group is created.

## Distance population integration

The day/night feature is integrated with the existing distance-streamed population state instead of bypassing it.

If a spawner is unloaded because no player is near when midnight/morning occurs, it **does not spawn actors just because the clock changed**. It updates the selected population phase, clears obsolete preservation/respawn state, and waits. When a player later enters `Spawn Activation Radius`, the spawner loads only the population appropriate to the current time.

This avoids background AI creation in unloaded areas and prevents daylight NPCs preserved before midnight from reappearing after the world has already entered the midnight phase.

## Cleanup semantics

A phase swap is not combat death. Before old-phase pawns are destroyed, the spawner removes its `OnDestroyed` callbacks. This prevents time-based cleanup from:

- firing `On Spawn Group Defeated`;
- scheduling Individual/Whole Group respawns for the population that was intentionally removed;
- carrying an obsolete respawn deadline into the new phase.

The new population receives the same movement, spline/free-roam, leash, group-cohesion and AI-combat setup as ordinary spawns.

## Empty midnight table

An enabled spawner with an empty/zero-weight `Midnight Spawn Table` will remove its daylight population at midnight and have no valid midnight NPC class to spawn. This is useful for locations that intentionally become empty overnight, but normally you should add at least one positive-weight Pawn Class.

## Blueprint/testing API

- `Refresh Day Night Population Now` — re-resolves the clock and rebuilds the currently loaded group from the correct phase. Useful after changing test time/settings at runtime.
- `Is Midnight Population Active` — reports whether the spawner is currently selecting `Midnight Spawn Table`.
- `Runtime → Day Night Population → Midnight Population Active` — editor/runtime diagnostic state.

For deterministic testing, set the Day/Night actor to `Fixed Time` or `Simulated Clock`, then call `Refresh Day Night Population Now` if you want an immediate manual resync.

## Clock robustness

The spawner listens to the Day/Night actor's hour/day-start events and also validates the current time from its existing runtime/relevance checks. The actual state is derived from the current authoritative world hour, so a simulated clock jump from 23:00 directly to 02:00 still resolves to the midnight population even if an exact 00:00 tick was skipped.
