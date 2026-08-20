# Dynamic Street Lights / Lamps

> **Player-built lights (v2.15.54):** `AARPGDynamicStreetLight` remains the designer-placed, Day/Night-driven world-light actor. For settlement torches/lanterns that players build on terrain/Floors/Walls and manually toggle with the normal Interact button, use `ARPGBuildPieceDefinition` with `Piece Kind = Light`; the framework automatically spawns `ARPGBuildLightActor`. See `BUILDING_CRAFTING.md`.

`AARPGDynamicStreetLight` is a Blueprint-derivable, event-driven world-light actor that follows `AARPGDayNightCycle` without maintaining a second clock. It is intended for street lamps, torches, braziers, shop signs, window lights, magical lanterns and similar world props.

## Quick setup

1. Create a Blueprint derived from **ARPGDynamicStreetLight**.
2. Add/attach the lamp mesh under the inherited `Root` component.
3. Position/tune the inherited `LampLight` Point Light component at the bulb/flame.
4. Optionally assign a Niagara System to inherited `NiagaraEffect`.
5. Optionally assign a Cascade Particle System to inherited `CascadeEffect`.
6. Keep `Enable Automatic Day/Night Control` enabled.
7. Place one `ARPGDayNightCycle` in the world. The lamp auto-discovers it, or an instance can explicitly set `Day Night Cycle Override`.

Default schedule:

- **Night:** on
- **Dawn:** on
- **Day:** off
- **Dusk:** off

This mirrors the existing Day/Night semantic window where lights stay on until `DayStartHour`. Enable `Active During Dusk` when a project wants lamps to ignite at dusk instead of waiting for the cycle's `NightStartHour`.

## Components exposed to derived Blueprints

- `Root` — attachment root for the lamp mesh and project-specific components.
- `LampLight` — movable built-in Point Light with warm starter values; all normal Point Light properties remain editable in a derived Blueprint.
- `NiagaraEffect` — auto-activation disabled; the dynamic lamp owns activation/deactivation.
- `CascadeEffect` — optional legacy particle fallback, also owned by the lamp state.

When `Control All Owned Light Components` is enabled (default), Blueprint-added Point, Spot or Rect light components on the same actor are included in the state change. Disable it to control only the inherited `LampLight`.

## FX modes

`FX Mode` supports:

- **None**
- **Niagara Preferred / Cascade Fallback** (default)
- **Niagara Only**
- **Cascade Only**
- **Niagara + Cascade**

The default mode checks whether the inherited Niagara component has an assigned FX asset. If it does, Niagara runs and Cascade stays disabled. If it does not, an assigned Cascade system becomes the fallback. This allows one Blueprint family to support modern Niagara content while preserving older Cascade assets.

## Day/Night integration

The lamp does not poll time every frame. At BeginPlay it resolves the cycle, applies the current phase immediately, and binds to:

- `ARPGDayNightCycle.OnPhaseChanged`
- `ARPGDayNightCycle.OnHourChanged` as a low-frequency safety refresh for simulated/fixed time changes and live phase-boundary authoring

If a lamp begins before the Day/Night actor is available, it can fail-safe off and retries cycle discovery on a configurable low-frequency timer. Once a cycle resolves, that retry timer is removed. If the cycle actor is destroyed, the lamp safely releases the binding and resumes discovery.

`Day Night Cycle Override` should be assigned when a level intentionally contains more than one cycle. Otherwise the first discovered `ARPGDayNightCycle` is used, matching the framework's existing day/night-aware spawner pattern.

## Networking/performance

Individual lamps do **not** replicate their own clock or permanent Tick. The existing `ARPGDayNightCycle` is already the replicated clock source; each lamp derives the same cosmetic phase locally. This avoids turning a town containing hundreds of decorative lights into hundreds of replicated clock actors.

Automatic lamp state therefore follows the server-synchronized Day/Night cycle. Project-specific runtime manual overrides that must be authoritative across the network should be driven from replicated project gameplay state and then call the lamp state API on each relevant peer.

## Blueprint extension hooks

Available functions:

- `Refresh From Day Night Cycle Now`
- `Set Automatic Day Night Control Enabled`
- `Set Manual Light State`
- `Force Street Light State`
- `Is Street Light On`
- `Get Resolved Day Night Cycle`
- `Should Be On For Phase`

Events:

- `On Street Light State Changed` multicast delegate
- overridable Blueprint event `On Street Light State Changed`

Use the Blueprint event for emissive material changes, ignition/extinguish sounds, shutters, animation, extra project-specific FX, light buzz or other presentation without modifying framework C++.

## Manual/editor testing

Set `Enable Automatic Day/Night Control = false` and use `Manual Light On`, or call `Set Manual Light State`, to validate the light/FX presentation independently of world time.

For full automatic testing, put the Day/Night cycle in **Fixed Time** or accelerated **Simulated Clock** mode and move between:

- 17:xx — Dusk, default off
- 18:xx — Night, default on
- 05:xx — Dawn, default on
- 06:xx — Day, default off

`Editor Preview On` keeps inherited components visible while authoring the Blueprint/level actor; it does not replace the runtime day/night state.
