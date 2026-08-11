# ARPG Day / Night Cycle — v1.9

`ARPGDayNightCycle` is the framework's drop-in authoritative world-time and dynamic outdoor-lighting actor. The default is intentionally WoW-like in one important respect: **the world clock follows the host/server computer's local real-world time** rather than running an unrelated game-only 24-hour timer.

## Fastest level setup

1. Place **ARPG Day Night Cycle** in the persistent level.
2. Leave **Time Source = Host System Clock**.
3. Leave **Use Built-In Lighting Rig = true**.
4. Remove or disable duplicate project Sun / Moon / Sky Light / Sky Atmosphere / Height Fog actors if they would double-light the scene.
5. Press Play.

The actor already contains a movable Sun directional light, Moon directional light, Sky Atmosphere, movable Sky Light with Real Time Capture, and Exponential Height Fog. No Level Blueprint Tick graph is required.

## Host system time and multiplayer

On authority, Host System Clock reads `FDateTime::Now()`. In standalone or listen-server games this is the host PC's local date/time. A dedicated server uses the dedicated-server machine's local clock. The authority periodically replicates a time snapshot and clock rate; clients advance smoothly from that sample using real elapsed time until the next correction.

This means a client with a different local time zone still sees the **host's** world time.

## Blueprint-pure green nodes

The `ARPG World Time` Blueprint library can be used from any Blueprint with world context:

- **Is Day**
- **Is Night**
- **Get World Hour** — fractional 0..24 value
- **Get World Date Time**
- **Get Day Night Phase** — Dawn / Day / Dusk / Night
- **Get Daylight Amount** — smooth 0..1 visual daylight weight
- **Get Day Night Cycle** — returns the placed actor for advanced settings/events

The placed actor exposes matching pure functions plus `Get World Time String` and `Get World Date String`.

## Gameplay phases versus lighting

Gameplay day/night boundaries are explicit. Defaults are:

```text
Dawn Start = 05:00
Day Start  = 06:00
Dusk Start = 17:00
Night Start = 18:00
```

`Is Day` is true from Day Start until Night Start. `Is Night` is the inverse. These rules do not depend on sun intensity, fog or exposure, so gameplay stays predictable when an artist changes the look.

The actor broadcasts:

- On Dawn Started
- On Day Started
- On Dusk Started
- On Night Started
- On Phase Changed
- On Hour Changed

Use pure checks for branch-style logic and delegates for event-driven systems.

## NPC / quest examples

```text
Spawner Wants Nocturnal Creature
    -> Is Night
    -> Branch

Vendor Interaction
    -> Is Day
    -> Open Shop / Closed Dialogue

Quest Condition
    -> Get Day Night Phase
    -> Night
```

## Lighting controls

The built-in rig exposes:

- sunrise / sunset hour
- Sun yaw and pitch offset
- maximum Sun lux
- noon and horizon Sun colors
- Moon lux and color
- day/night Sky Light intensity
- Real Time Sky Capture toggle
- day/night fog density and color

Sun rotation is continuous over the 24-hour clock. The Moon is driven opposite the Sun. Lighting weights smoothly fade around sunrise/sunset instead of hard-switching with the gameplay day/night booleans.

## Existing project lighting

Set **Use Built-In Lighting Rig = false** and assign External Sun Light, External Moon Light, External Sky Light, External Sky Atmosphere and External Height Fog as needed. The clock and pure nodes do not change; the framework simply drives the assigned lighting components.

## Testing without changing Windows time

Three time sources are available:

- **Host System Clock** — production default
- **Simulated Clock** — starts from a chosen DateTime and advances by configurable game minutes per real second
- **Fixed Time** — freezes at a selected DateTime

There is also a `Clock Offset Hours` field for quick testing. Runtime authority calls include `Use Host System Clock`, `Set Fixed World Time`, `Start Simulated Clock` and `Force Clock Sync`.

## Sky Light performance

The built-in Sky Light uses **Real Time Capture** for dynamic time-of-day. The framework intentionally does not call `RecaptureSky()` every update; manual recapture is an expensive operation and can hitch.

## Multiplayer authority rule

World time is presentation + gameplay state owned by the authority. Clients may query it freely, but runtime changes to the selected clock source should be made on the server/host.


## AI spawner midnight populations (v2.4)

`ARPGAISpawner` can now consume this authoritative clock directly. Enable `Enable Midnight Population Swap` on a spawner to use its existing `Spawn Table` as the daylight/default population and `Midnight Spawn Table` from 00:00 until this actor reaches `DayStartHour`. Loaded old-phase pawns are cleanly replaced; distance-unloaded spawners only update phase and wait until player relevance before spawning. See `AI_SPAWNER_DAY_NIGHT.md` for full setup and lifecycle details.
