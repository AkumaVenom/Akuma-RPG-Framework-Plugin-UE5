# Source Validation — Akuma's RPG Framework v1.8.0-alpha

Package: **Akuma's RPG Framework — 1.8.0-alpha Group Combat Polish**  
Target: **Unreal Engine 5.8 / 5.8.1**

## Important limitation

This generation environment does not contain Epic's UE 5.8.1 build toolchain, so the package is **not** claimed to have completed a real Unreal Development Editor compile here. The user's local UE 5.8.1 build remains authoritative. Static/source validation is used to catch structural and known source regressions before that build.

All previous real-build fixes through v1.4.1 are retained, including the explicit `Navigation/PathFollowingComponent.h` dependency used by the AI spline component.

## v1.8 target-reset + group-combat checks

The static validator now additionally checks that:

- temporary retaliation is configured to restore original disposition on target death by default;
- dead temporary targets can have stale threat removed by default;
- AI binds/unbinds the current target's Combat `OnLifeStateChanged` delegate and has an explicit target-life-state cleanup path;
- coordinated group combat is enabled by default with a finite simultaneous melee-attacker cap;
- coordinated melee grouping, engagement-slot evaluation, combat-ring positioning, Navigation projection and focused waiting/orbit behavior exist in the runtime component;
- runtime group-combat role/slot state is Blueprint-readable.

## v1.7 AI aggro / faction fallback checks

The validator verifies that:

- AI Combat binds automatically to `OnCombatHitReceived`;
- `Retaliate When Attacked` is enabled by default;
- neutral-faction retaliation override is enabled by default;
- missing-faction retaliation fallback is enabled by default;
- nearby ally assistance is enabled by default;
- same-spawner-group and same-class-when-faction-unknown assistance fallbacks exist;
- received aggression is remembered and can remain a valid combat target even when the faction relationship itself is neutral;
- Combat damage legality honors an active AI retaliation target so a class that disallows neutral damage can still fight back;
- ARPG AI Spawner registers itself as the spawned AI's runtime Spawn Group Owner;
- Faction Definitions expose `Default Relationship To Unlisted Factions`;
- `Attack Hostile On Sight` is consumed by runtime automatic target acquisition;
- v1.6 targeting/camera/combat-feedback/stagger requirements remain intact;
- v1.5 spline/group/free-roam requirements remain intact;
- v1.3 automatic NPC ragdoll + death montage fallback remains intact;
- v1.2 target marker compile regression coverage remains intact.

## Expected static result

Run:

```text
python Tools/validate_source.py
```

The machine-readable result is emitted separately as `AkumasRPGFramework_validation_v1.8.0.json` during packaging.

## Recommended UE 5.8.1 test pass

1. Replace the complete previous plugin folder with v1.8.0 and clear project/plugin `Intermediate` plus old plugin `Binaries`.
2. Build Development Editor / Win64.
3. Create/use a passive `ARPGAICharacter` chicken with no faction configured. Leave `Retaliate When Attacked`, `Retaliate When Faction Unknown`, `Call For Help When Attacked`, and `Assist Same Class When Faction Unknown` enabled. Attack one chicken and confirm it immediately stops free roam, targets the player, chases and attacks.
4. Place another chicken inside `Ally Assist Radius`; confirm it joins the fight even with no faction Data Asset.
5. Spawn several chickens from one `ARPGAISpawner`; confirm same-spawner-group assistance works even when `Stay Together` is disabled.
6. Assign a Chicken Faction Data Asset and keep Player relationship at `0`; confirm the chicken remains passive until hit, then retaliates because neutral retaliation override is enabled.
7. Set Chicken -> Player relationship negative with `Attack Hostile On Sight = true`; confirm proactive acquisition works.
8. Set `Attack Hostile On Sight = false` while keeping the negative relationship; confirm the chicken does not initiate combat but still retaliates when attacked.
9. Set `Fallback: Attack Players On Sight = true` with missing/neutral faction data; confirm proactive faction-free monster behavior works.
10. Re-test free roam/spline resumption after combat, ragdoll death, lock-on camera, hit FX/audio and stagger/knockback.

### v1.8 runtime cases

1. Passive neutral chicken attacks only after being hit, kills the player, then immediately clears that temporary hostility; after respawn it ignores the player until attacked again.
2. Truly hostile faction NPC kills the player and is allowed to reacquire after respawn because its authored faction/fallback remains hostile.
3. Spawn at least 8 allied melee NPCs against one player: no more than the configured simultaneous attacker cap should actively commit while the remainder distribute on the waiting ring.
4. Observe waiting NPCs: they should keep facing/focus on the player, orbit/reposition on NavMesh, and rotate into openings after attacks/yields instead of forming one stationary pile.
5. Put an obstacle in one attack slot: the blocked NPC must yield after the slot lease and another NPC should receive an opening.

## Retained compile fixes

The v1.8 tree retains prior fixes discovered through real UE 5.8.1 Development Editor builds: GameplayTags/GameplayTasks descriptor correction, AttributeSet replication fix, battle-pet cooldown fix, CharacterMovement/PlayerState includes, mount Controller shadow naming, targeting `TSubclassOf` ambiguity fix, and AI spline Path Following include fix.
