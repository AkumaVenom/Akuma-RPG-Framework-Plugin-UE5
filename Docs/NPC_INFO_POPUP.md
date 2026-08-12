# Automatic Character / NPC Info Popup — v2.10.1-alpha

Every `AARPGCharacter` now owns an inherited `CharacterInfo` component. It is a specialized screen-space `WidgetComponent` that can automatically present the replicated character name, effective level, current health and maximum health above the character.

## Fast setup

1. Open any Blueprint derived from `AARPGCharacter` or `AARPGAICharacter`.
2. Select the inherited **CharacterInfo** component.
3. Under **User Interface -> Widget Class**, choose your popup Widget Blueprint (for example `UI_NPCPopup`).
4. Leave **Enable Character Info Popup** enabled.

The component is already configured for screen-space presentation, desired-size drawing, no collision, no hardware focus, automatic capsule-height placement and local proximity visibility. v2.10.1 keeps Unreal's native `UWidgetComponent` TickComponent capability intact and enables it only while the popup is visible, which is required for reliable screen-space projection.

## Zero-Blueprint automatic field binding

An ordinary UserWidget does not have to contain framework graph logic. By default the component looks for these child widget names:

- `CharacterNameText` — Text Block
- `LevelText` — Text Block
- `HealthBar` — Progress Bar
- `HealthText` — Text Block

If your existing widget uses different names, change the four **Widget Binding** names on the CharacterInfo component to match your widget. The framework writes the replicated values directly.

## Framework widget base

For the cleanest custom implementation, reparent your Widget Blueprint to `ARPGCharacterInfoWidget`.

That base exposes:

- `Get Character Info`
- `Get Observed Character`
- `On ARPG Character Info Updated`

The update snapshot contains the character reference, character name, effective level, health, max health, health percent and alive state. A Blueprint can therefore drive any custom art without polling gameplay components itself.

If no custom Widget Class is selected, the component falls back to the native `ARPGCharacterInfoWidget`, which creates a functional name / level / health presentation automatically.

## Visibility defaults

The component defaults to NPC-oriented behavior:

- AI characters: shown
- player-controlled characters: hidden
- local player self: hidden
- show distance: 1100 cm
- hide distance: 1350 cm
- check interval: 0.20 s
- hide dead characters: enabled
- hide while the v2.9 ground-rise entrance is active: enabled
- line of sight requirement: disabled

The separate show/hide distances provide hysteresis so a character standing on the range boundary does not flicker in and out.

## Performance

The system uses a staggered local timer for **proximity decisions** rather than evaluating range every frame. Because CharacterInfo derives from Unreal's `UWidgetComponent`, its native component Tick is enabled only while a popup is actually visible so Screen-space projection can function; the Tick is disabled again while hidden. Dedicated servers do not update UI. With **Lazy Create Widget** enabled, the runtime widget instance is retired while initially hidden and recreated on first in-range presentation while preserving the authored Widget Class. With **Release Widget When Far** enabled, the runtime widget instance is released after the configured far delay.

All visibility decisions are local presentation only. Name, stats and progression continue to use their existing replicated gameplay state; the popup does not add replicated proximity/UI state.

## NPC level scaling

The displayed level uses `Stats -> Get Effective Level`, so an NPC using the framework's player-relative level scaling shows its actual runtime combat/stat level. Non-scaled characters naturally fall back to their authored progression level.

## Ground-rise integration

`Hide During Ground Rise Entrance` is enabled by default. A spawned NPC stays visually clean while emerging through the ground and the popup appears only after the replicated entrance state completes. No changes were made to the v2.9 spawn movement lock or collision-safe capsule placement.
