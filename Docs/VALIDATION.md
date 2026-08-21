## v2.17.0 Mining / Mineable Rock acceptance

Source/model validation protects the new Mining contract: ready-character component ownership, RuneScape-style 1–99 model with authored-curve priority, replicated random rock mesh/scale/yaw, tick-free node runtime, exact equipped pickaxe instance validation, per-strike/depletion/bonus rewards, free context-sensitive Basic Attack interception, repeated Interact mining, renewable-resource building suppression and unchanged save schemas.

Project-side UE5.8 PIE acceptance should additionally verify:

1. Equip a real pickaxe runtime item tagged `Item.Tool.Pickaxe`; a Data Asset that merely exists must not satisfy Mining.
2. Basic Attack a Mineable Rock and confirm exactly one rate-limited Mining strike occurs without ordinary attack Stamina/Mana cost; a real non-rock combat target must retain normal combat behavior.
3. Bind Interact to `InteractWorld` or call `Start Mining From View`; confirm repeated swings continue until depletion/cancel/range/tool failure.
4. Confirm Required Mining Level and Minimum Tool Tier reject the wrong player/tool with failure feedback.
5. Confirm Successful Strike Drops can arrive on each hit, Depletion Drops arrive only on final depletion, and Bonus Chance Drops respect trigger/level/tier/chance settings.
6. Confirm XP advances on successful strikes/depletion and the default model requires 83 XP for Level 2; verify optional custom Skill Definition curves still override XP Model.
7. Confirm the exact equipped pickaxe loses configured Gathering durability only after successful authoritative strikes and broken/unequipped tools fail validation.
8. Place multiple nodes with Rock Mesh Variations/random scale/yaw and verify server/client see the same selection; verify optional rubble, FX/audio and respawn.
9. Build a Foundation through an `ARPGMineableRock`; the renewable node must suppress without XP/loot. Ordinary static rocks must remain placement blockers. Demolish the final blocker and confirm respawn can recover.
10. Save/reload and verify Mining level/XP, acquired resources and buildings persist normally; world save remains v9 and character save remains v5.
11. Test an Actor Foliage-derived Mineable Rock population at project target density and profile server/replication cost; non-harvestable background rocks should remain ISM/HISM foliage.

See `Docs/MINING.md` for full authoring details.

# Source Validation — Akuma's RPG Framework v2.16.12-alpha

Package: **Akuma's RPG Framework — v2.16.12-alpha**  
Target: **Unreal Engine 5.8 / 5.8.1**

## Important limitation

The automated checks below are source/model validation. They are not a substitute for UnrealHeaderTool/MSVC/PIE. v2.15.43 remains the project-confirmed Stair/story geometry baseline, the Window placement/interaction path through v2.15.48 is project-confirmed, and v2.15.53 is the completed four-cardinal Stair/Wall-family boundary baseline. v2.15.54 buildable Lights remain confirmed as an additive non-blocking layer. v2.16.0 adds settlement Beds/Hubs through a separate horizontal-surface contract and leaves the protected Stair/Wall-family boundary functions unchanged. v2.16.11 adds Settlement Paths through a separate non-structural continuous spline-placement contract. v2.16.12 fixes the project-observed turn tangent foldover while retaining that architecture. Runtime PIE confirmation is still required for this release.









## v2.16.12 Settlement Path turn-stability acceptance

Project-side PIE must specifically verify the root-fix case that failed under v2.16.11:

1. Use the same known-good Settlement Path Data Asset and road mesh; do not compensate by changing Forward Axis or terrain sampling.
2. Place a straight segment and confirm it remains visually unchanged.
3. Add 30°, 45°, 60° and 90° turns at ordinary 300-1200 cm point distances. No section may spike upward, fold under itself, invert into dark triangles or overshoot past the confirmed joint.
4. While aiming the next point, confirm the pooled preview bends from the previous travel direction without the old corner explosion.
5. Test a very sharp near-reversal. The road may form a visually tight/hard join, but geometry must remain bounded and must not fold back catastrophically.
6. Load a world saved by v2.16.11 containing path tangent overrides; verify the restored road is bounded and a subsequent save/load remains stable. World schema must remain v9.
7. Repeat in listen-server plus remote client PIE to confirm the replicated tangent-direction state produces the same final geometry.

## v2.16.11 player-built Settlement spline-path acceptance

- Create an `ARPGBuildPieceDefinition` with `Piece Kind = SettlementPath`, `Actor Class = None`, a subdivided Static Build Mesh and `Allow Ground Placement = true`.
- Confirm the first point: it must establish the path anchor only, consume no Build Cost and spawn no segment actor.
- Move the cursor and verify a connected valid/invalid Spline Mesh preview updates from the last confirmed point and follows terrain between endpoints.
- Confirm a second point: exactly one `AARPGBuildPathActor` must spawn, exactly one Build Cost must be consumed, and the preview origin must advance to the authoritative endpoint.
- Continue several points around a bend and verify adjacent segments share a smooth visual tangent rather than appearing as disconnected hard-angle strips.
- Confirm `PathSegmentTooShort` / `PathSegmentTooLong` feedback leaves the last endpoint and resources unchanged.
- Press Cancel / End Build Mode and verify the continuous session ends immediately; selecting the Path again starts from a fresh first point.
- Enable optional Path collision and verify subsequent path surface sampling pierces existing Path actors rather than stacking the new road on top of an old one.
- Build Foundations/Walls/Stairs over/beside a Path and verify the Path does not become a structural blocker. Cover an `ARPGTree` with a Path and verify the Path alone does not suppress Tree respawn.
- Save/reload a curved multi-segment road and verify segment endpoints plus turn smoothing are restored under world SaveVersion v9.
- In multiplayer PIE, spam confirm while a path request is pending and verify client state advances only on the server result. A direct generic one-shot path placement request must be rejected.
- `Tools/test_settlement_player_path_spline_model.py` and every previous regression must pass.

## v2.16.10 contextual villager woodcutting-tool acceptance

- Assign an Axe `ARPGItemDefinition` to `Villager Woodcutting Tool Item`; ensure that Item has an equipped mesh and a valid hand socket/relative transform.
- With `Show Tool While Going To Work = true`, verify the axe appears when the resident transitions from Roaming to Going To Work and remains visible during Woodcutting.
- Verify the axe disappears when the resident returns to Roaming, At Home, Returning Home or Homeless.
- Set `Show Tool While Going To Work = false`; verify travel is empty-handed and the axe appears only after Woodcutting begins.
- Confirm resident Inventory quantities/equipped flags and durability are unchanged by contextual axe show/hide transitions.
- In a network PIE test, verify remote clients see the same held-tool state after `ResidentState` / `CurrentWorkTree` replication and late visual refresh.

## v2.16.9 build-aware Tree suppression acceptance

- A Foundation preview/authority trace can pass through one or more `AARPGTree` actors to the real terrain or structural snap target behind them.
- Tree actors are ignored only for Foundation placement; ordinary world blockers and build-vs-build collision remain unchanged.
- Placing/restoring a build piece over a standing/felled Tree suppresses both Tree/Stump visibility and collision without granting Wood/XP/fell rewards.
- A suppressed Tree cannot naturally or forcibly respawn while any logical build occupancy remains. Multiple blockers are independent.
- Removing the final blocker resumes the original remaining respawn delay, or permits prompt respawn when the natural eligibility time already elapsed.
- Settlement villagers abandon build-suppressed work targets without generating a false stockpile deposit.
- Suppression rebuilds from persisted structural building occupancy; world SaveVersion remains v9 in v2.16.12 and Settlement Paths are explicitly excluded from suppression.
- `Tools/test_tree_build_suppression_model.py` passes alongside every previous framework regression.

## v2.16.8 native Dynamic Recast Stair navigation acceptance

Real PIE testing showed that after the project Supported Agent was configured correctly the Wood Stair itself rasterized as a continuous walkable surface, while the framework-authored off-mesh shortcut could compete with normal path following and make a resident oscillate near the Stair/doorway. Acceptance now requires:

- structural build initialization/completion/restore/removal continues to update NavigationSystem's component/actor octree entries and dirty a bounded local Recast region;
- no per-piece full-world synchronous `Build()` is used;
- completed Stairs create **no automatic `ANavLinkProxy` or Simple Nav Link**;
- obsolete Stair NavLink authoring fields are no longer exposed on build definitions;
- deprecated compatibility nodes create no links (`Refresh Stair Navigation Bridge` delegates to runtime nav refresh; `Has Active Stair Navigation Bridge` is false);
- the project owns Supported Agent radius/height and the framework does not overwrite those settings;
- the confirmed settlement/home/worker systems remain independent of this generic navigation cleanup.

PIE acceptance: with the Supported Agent configured so the Stair is visibly green under **P**, `Custom NavLinks count` should remain zero for framework Stair traversal and generic AI/residents must cross the Stair naturally in both directions while roaming or working.

## v2.16.6 runtime-built Stair navigation acceptance

Real PIE testing showed the house interior and terrain as separate green NavMesh islands with no connection across the player-built Stair. Acceptance now requires: completed Stairs create one transient non-replicated bidirectional Simple Nav Link; endpoints use the logical +X-uphill / `SnapSize` structural anchors plus the exposed landing inset; low/high points independently project to nearby NavMesh with a vertical cap below half a story; unfinished/non-Stair pieces own no link; demolition/EndPlay removes the proxy; restore/completion refreshes it; and the implementation remains generic with no settlement-villager class dependency.

PIE acceptance: with Navigation visualization enabled, build a Foundation/Floor interior reachable only by a Stair. Spawn/recruit an AI on the interior island and give it an exterior goal (and vice versa). Path following must cross the Stair in both directions without manual level-authored `NavLinkProxy` actors.

## v2.16.5 settlement interior-story spawn/home-anchor acceptance

Real PIE testing of v2.16.4 confirmed resident deduplication and state changes but exposed villagers spawning on the roof above otherwise valid homes. v2.16.5 must therefore ensure:

- resident home anchoring derives the walkable plane from the validated home, not the Bed actor pivot;
- NavMesh projection uses a tight vertical tolerance and rejects roof/ceiling islands one story above;
- projected home points remain inside the validated home footprint;
- recruitment never uses `AlwaysSpawn`, retries collision-safe same-story locations and rejects any collision adjustment that changes story;
- if no safe interior NavMesh point exists, recruitment is deferred instead of creating a stuck resident;
- roaming, Return Home and combat leash reuse the same semantic interior anchor;
- world-load restore can fall back to that anchor and repairs previous roof-position residents; that v2.16.5 fix added no schema field (the current v2.16.12 world schema remains v9 for Settlement Paths);
- v2.16.3 housing geometry, v2.16.4 registry/work routing and protected building topology remain unchanged.

## v2.16.4 settlement resident identity / locomotion acceptance

Real PIE testing after v2.16.3 confirmed home validation but exposed one resident appearing twice in the Hub UI and a worker remaining motionless in `Going To Work`. v2.16.4 must therefore ensure:

- recruitment pre-registers the pawn before initialization callbacks can register it again;
- runtime registry cleanup deduplicates both actor pointer and persistent `ResidentId`;
- public resident lists, summary counts and worker-cap accounting use the unique resident view;
- worker movement can recover a missing AI controller, requires Navigation System availability and projects tree targets to navigable approach points;
- `Going To Work` is published only after a non-failed move request;
- accepted work routes must prove real translation, otherwise movement is stopped, the tree reservation is released and roaming resumes;
- v2.16.3 housing geometry, v8 persistence and protected building topology remain unchanged.

## v2.16.3 settlement structural-plane home acceptance

Real PIE testing of v2.16.2 showed valid modular houses reporting `0/N` overhead cover and `Valid Homes 0`. v2.16.3 must therefore ensure:

- default minimum housing is 2x2 Foundations (4 cells / 8 perimeter segments), still Data Asset configurable;
- Foundation semantic cells use transformed build bounds / finished top planes, never raw actor origins;
- Floor/Ceiling/Roof cover is matched by transformed footprint coverage at `FoundationTop + HomeStoryHeight`;
- Wall/WindowWall/Doorway perimeter segments are matched at the real Foundation half-grid boundary and require their finished bottom on the Foundation top;
- one Wall-family actor may not satisfy multiple perimeter slots in the same candidate;
- incomplete diagnostics prefer the closest-to-valid/smallest useful candidate instead of drifting to an arbitrary larger connected rectangle;
- native Doorway -> Door socket validation remains unchanged;
- v2.15.53 Stair/Wall-family protected structural functions remain hash-identical.

## v2.16.2 UE5.8 incomplete-type compile acceptance

A real UE5.8/MSVC rebuild of v2.16.1 showed that `.Get()` alone is insufficient when `UARPGSettlementPanelWidget` / `UARPGBedPanelWidget` are still only forward-declared at the inline call site. v2.16.2 must therefore ensure:

- `IsSettlementPanelOpen()` and `IsBedPanelOpen()` remain declared in `ARPGSettlementUIComponent.h` but are **not implemented inline** there;
- their implementations live in `ARPGSettlementUIComponent.cpp`, after `UI/ARPGSettlementWidgets.h` provides complete widget types, and perform `IsValid(Active...Panel.Get())` there;
- the v2.16.1 descriptive UMG canvas-slot names remain intact under warnings-as-errors;
- world save remains v8 and settlement/building gameplay code remains unchanged by this compile-only hotfix.

## v2.16.1 UE5.8 compile-compatibility acceptance

A real UE5.8 UnrealBuildTool/MSVC compile of v2.16.0 exposed two settlement-UI C++ compatibility defects that source/model validation could not emulate. v2.16.1 must retain all settlement behavior while ensuring:

- forward-declared `TObjectPtr<UARPGSettlementPanelWidget>` / `TObjectPtr<UARPGBedPanelWidget>` state queries call `IsValid(...Get())`, not `IsValid(TObjectPtr)` directly;
- native settlement widget methods do not declare a local `Slot` identifier that hides inherited `UWidget::Slot` under warnings-as-errors;
- world save remains v8 and no structural placement/topology code is changed by the compile hotfix.

## v2.16.0 Settlement Hub / validated-home / villager acceptance

- `Bed` and `SettlementHub` must remain appended after `Light`; all pre-v2.16 build-piece enum values must remain stable.
- A completed building with no Hub must remain ordinary: no resident recruitment, no settlement worker scheduling and no settlement proximity HUD.
- New Hub placement must respect normal build authority and, when enabled, reject overlap with completed settlement areas using the sum of both Hub radii plus configured padding.
- Beds must require a completed Foundation/Floor support. Hubs may use Foundation/Floor and only use terrain when their Build Piece explicitly enables ground placement; authority must re-trace/re-seat terrain.
- Build a complete default 2x2 Foundation home. A Villager Bed must remain **Incomplete** until all 4 overhead cells are covered, all 8 perimeter Wall-family segments exist, at least one Doorway exists, and a completed Door occupies that Doorway's native socket.
- Remove one Foundation, cover piece, perimeter wall, Doorway or Door and verify the Bed immediately stops contributing valid resident capacity on the next Hub refresh.
- Bed role changes must use the existing player-owned interaction RPC and enforce range/modification access. Test Unassigned, Player and Villager roles.
- Recruitment must be server-only, at most one new resident per recruitment interval, must re-home existing homeless residents first, and must never exceed valid Villager Beds or Maximum Villagers.
- Resident class must derive from `AARPGSettlementVillagerCharacter`; owner account/character/faction must match the Hub. Verify normal faction hostility/ally behavior remains intact.
- Resident activity must be timer-driven. Test roaming around Bed/Hub and confirm active combat is not overwritten by settlement work.
- With settlement woodcutting enabled, two residents must not reserve the same `ARPGTree`. Workers must use the tree's normal `ApplyChop` durability/reward path and configured rewards should transfer into the Hub stockpile.
- The local Settlement HUD must appear inside `Settlement HUD Radius` and disappear outside it. Hub interaction must open the Settlement panel; Bed interaction must open Bed assignment; Stockpile must hand off to existing Storage UI. Blueprint Widget subclasses must be assignable on SettlementUI.
- World save **v8** must round-trip mixed Bed roles, Player Bed owner, resident IDs/classes/transforms/names/health, Hub/Bed links, faction ownership and Hub stockpile. v6 Window and v7 Light migration must remain intact.
- Re-run all confirmed Stair/Wall/Window/Light placement combinations around Hub/Bed content. Settlement additions must not change v2.15.53 structural classifier hashes.
- `Tools/test_settlement_hub_villager_model.py` checks the native settlement architecture and byte-hash locks the four protected v2.15.53 Stair/Wall-family functions.

Recommended UE 5.8 PIE acceptance: create one Hub definition, one Hub build piece, one Bed piece and one Blueprint villager subclass. Build a complete 2x2 house, assign a Villager Bed, verify recruitment/roam/woodcut/deposit, save/reload, then demolish or invalidate one home segment and confirm housing state updates without affecting unrelated building placement.

## v2.15.54 interactive buildable-light acceptance

- `Piece Kind = Light` must remain appended after `Custom`; no earlier build-piece enum value may shift.
- **Ground / Foundation / Floor** preview must seat the fixture's transformed visible bottom on terrain or a completed Foundation/Floor. Ground placement must still obey `Allow Ground Placement`, slope and support checks.
- **Built Wall Surface** preview must attach to the actually aimed face of a completed Wall/WindowWall/Doorway and orient local `+Y` outward. Test both inside and outside wall faces and several arbitrary points on the same 300 cm module.
- Authority must independently re-resolve the submitted Light transform to the compatible completed support and retain catalogue/range/resources/faction/territory/modification checks.
- Completed native Light fixture Static/Skeletal collision must remain disabled so placing a torch cannot change previously-valid Stair/Wall/Floor/Window/Door combinations. Two Lights closer than `Light Minimum Spacing` must still be rejected semantically.
- The same existing Interact Built Structure input must toggle Light state through `ServerToggleBuiltLight`. Test owner/allowed faction access and interaction range. No fuel or inventory item may be consumed by toggling.
- Point and Spot modes must respect configured intensity, attenuation, color/temperature, shadows and relative transform. Fade in/out must use short-lived actor Tick only while transitioning, and the runtime light must be hidden at zero alpha.
- Test Niagara-only, Cascade-only, Niagara-preferred/Cascade-fallback and Niagara+Cascade. FX should activate on ignition, remain during fade-out, and deactivate at zero illumination.
- Optional emissive scalar should interpolate with the same fade when the configured material parameter exists.
- Demolition must still find the intentionally non-colliding fixture through the same bounded view resolver and enforce modification access.
- World save v7 must round-trip one Light On and one Light Off. A v6 fixture load must use `Light Starts On`, while the existing v6 Window migration remains intact.
- `Tools/test_building_interactive_light_model.py` additionally hash-locks the four protected v2.15.53 Stair/Wall-family functions so this additive release cannot silently modify the confirmed structural seam implementation.

Recommended UE 5.8 PIE acceptance: create one stick torch (`Ground / Foundation / Floor`) and one wall lantern (`Built Wall Surface`). Place the stick torch on terrain, a Foundation and a Floor; place the wall lantern on both faces of Wall, WindowWall and Doorway pieces. Toggle each several times, verify smooth light/FX fade and replication on listen server + client, save/reload mixed On/Off states, then repeat the project's confirmed Stair/Wall/Window placement combinations beside the fixtures. Those structural placements must behave exactly as v2.15.53 with the lights absent.

## v2.15.53 four-cardinal Stair / Wall-family boundary acceptance

- Reproduce the final reported red Stair rotation **without changing Data Asset values**. The Wall-family module on that exact authored perimeter boundary must now be **VALID PLACEMENT**.
- Test Stair yaws `0`, `90`, `180`, and `270` against the same rotated layout. All four must classify identically.
- Verify side boundaries: Wall actor snap origin on Stair-local `Y = ±150 cm`, Wall run axis parallel/180-equivalent to Stair run, and longitudinal overlap against the Wood Stair `PlacementBounds.X = 167`.
- Verify LOW/HIGH endpoint boundaries: Wall actor snap origin on Stair-local `X = ±150 cm`, Wall run axis perpendicular to Stair run, and lateral overlap against `PlacementBounds.Y = 150`.
- Repeat for `Wall`, bare `WindowWall`, `Doorway`, and Window/Door-hosted variants, in both build orders and with a Stair-chain active host.
- Keep negative cases red: parallel centreline wall (`Y = 0`), perpendicular wall through the flight interior (`X = 0`), distant modules outside both boundary spans, unrelated inserts/world geometry and duplicate occupancy.
- No `Placement Bounds`, `Snap Size`, `Placement Offset`, `Window Insert Offset` or collision-channel changes are part of acceptance.

Recommended UE 5.8 PIE acceptance: reproduce the exact screenshot rotation first, then rotate the Stair through all four cardinal orientations around the same perimeter.

## v2.15.52 unified bidirectional Stair / Wall-family side-seam acceptance

v2.15.52 is retained as historical regression coverage for the build-order/Stair-chain unification. Its side-only rule is superseded by the v2.15.53 boundary classifier above.

## v2.15.51 plain Wall / WindowWall -> Stair endpoint-overhang seam acceptance

- Reproduce the latest reported case with a fresh tiled `Floor`, an ordinary parallel `Wall` beside the intended flight, and a fresh Wood `Stair`. Repeat with a bare `WindowWall`. Both must remain **VALID PLACEMENT** at the same Stair socket.
- The current Wood dimensions are the key regression: Stair run `334 cm`, structural grid `300 cm`, therefore `17 cm` authored overhang at each endpoint. A side-wall continuation centred one full grid cell away spans `[150,450]` while the Stair art reaches to `167`; the `17 cm` overlap is a legal modular seam even though the structural cells only touch at `150`.
- Validate the same result on both `+Y` and `-Y` Stair side planes and with 180-degree wall facing reversal. `Wall`, `WindowWall` and `Doorway` must share one classifier.
- Keep negative cases blocked: 90-degree end walls across travel, centreline walls, a parallel side module moved beyond the authored Stair run (for the current kit, centre around `320 cm` or farther), unrelated world geometry and real travel-volume occupancy.
- Repeat with a hosted closed/open Window after the bare WindowWall passes. v2.15.49 exact host-socket inheritance must not make the result depend on Window animation/collision state.
- No `Placement Bounds`, `Snap Size`, Stair offset, Window offset or collision-channel authoring change is permitted as part of acceptance.

Recommended UE 5.8 PIE acceptance: use the exact room configuration from the reported screenshots, first with ordinary side Walls, then bare WindowWalls, then hosted Windows. Test both Stair directions. The Stair should be green whenever the surrounding Wall-family pieces occupy only the legal parallel side corridor.

## v2.15.50 bare Wall-family -> Stair-side structural-span acceptance

- Reproduce the reported case with a fresh `Floor`, a **bare** parallel `WindowWall` (no Window inserted), and a `Stair` snapped beside it. The Stair must remain **VALID PLACEMENT** rather than **Blocked by another object** when the WindowWall module occupies the legal side corridor.
- Repeat with ordinary `Wall` and `Doorway`; all Wall-family kinds must use the same structural rule.
- Validate longitudinal offset explicitly: a 300 cm Wall-family module whose actor origin is offset along the flight must still be accepted when its `SnapSize` segment overlaps the Stair structural cell. The old `NeighborInCell.X ~= 0` point test must not return.
- Keep negative cases blocked: rotate the wall 90 degrees across Stair travel, put it on the Stair centreline, or move the parallel wall segment completely outside the Stair cell so its structural spans do not overlap.
- Repeat with a hosted Window/Door after the bare-host test passes; v2.15.49 host-socket inheritance must remain valid on top of the corrected host classification.
- No Stair/Window/WindowWall Data Asset offsets or Placement Bounds should be changed to pass this test.

Recommended UE 5.8 PIE acceptance: use the same Wood WindowWall row that previously reproduced the failure, leave at least one WindowWall opening empty, and snap the Wood Stair beside each bare WindowWall from both side orientations. Then add a Window and repeat open/closed. The result must not depend on insert presence or animation state.

## v2.15.49 hosted Door/Window -> Stair-side seam acceptance

- Reproduce the reported case with a fresh `Floor`, a parallel side `WindowWall`, a completed hosted `Window`, and a `Stair` snapped to the Floor edge beside that wall. The Stair must remain **VALID PLACEMENT** instead of returning **Blocked by another object** solely because the Window collider overlaps the Stair profile.
- Repeat with `Doorway -> Door`; the behavior must be identical because the rule is hosted-insert generic rather than Window-special-cased.
- The insert may be ignored only after `ARPGInsertActorMatchesHost()` proves it occupies the candidate host's exact native insert socket and `ARPGIsCompatibleStairHostStructuralNeighbor()` proves that host lies on the incoming Stair's legal parallel side edge.
- The active Stair must still resolve to a valid flat `Foundation` / `Floor` / `Ceiling` snap. Unsnapped/free Stair placement cannot use this exception.
- Negative regressions must keep a misplaced/unrelated Window or Door, a perpendicular end-wall host, a centreline host, and a real occupied travel cell blocked.
- Modification-access checks still apply to both the hosted insert and its resolved host. Window open/close collision behavior, duplicate-host occupancy, Stair segmented occupancy and the v2.15.43 300 cm structural lattice remain unchanged.

Recommended UE 5.8 PIE acceptance: use the exact current Wood setup, close a Window in a WindowWall beside the intended Stair flight, and confirm the Stair ghost stays valid at the same snap transform that was valid before Windows were added. Open/close the Window and retest; placement validity must not depend on the Window animation state. Then rotate the Wall/WindowWall 90 degrees across the travel opening and confirm the conflicting Stair placement remains blocked.

## v2.15.48 replicated Skeletal Window interaction acceptance

- Test a WindowWall whose simple/Visibility collision covers the visual opening. Aim at the closed Window through the opening/frame and press the same Interact button used by Doors. The hosted Window must toggle even when the first trace hit reports the WindowWall host; an unrelated wall in front must still block interaction.

- A completed native Window must toggle only through server authority and normal `CanActorUse` / interaction-distance checks. A second toggle during an active transition must be rejected.
- `Window Open Animation` must play forward for opening. An explicit `Window Close Animation` must play forward for closing; if it is empty, the Open sequence must play in reverse.
- Skeletal component Tick must be enabled only during transition playback and disabled at the final open/closed pose.
- Imported Window mesh/Physics Asset collision must not remain as a second gameplay blocker. Native `WindowCollision` owns gameplay blocking; opening disables it immediately and closing restores it only at the fully closed pose when configured.
- A separate Visibility-only interaction target must remain queryable while open without blocking Pawn movement. `Interact Built Structure` must recognize `ARPGBuildWindowActor` and route through the player-owned Interaction RPC.
- `bWindowOpen` must replicate and persist in world-save schema v6. v5/older worlds restore Windows closed. Loading a state applies the final pose directly rather than replaying a sound/transition.
- v2.15.46 placement ghost/host acquisition, v2.15.45 Window socket alignment/host offset, Door behavior and v2.15.43 structural regressions remain enabled.

Recommended UE 5.8 PIE acceptance: assign the actual Wood Window Open animation (and Close if available), wire the player's interaction input action to `Interact Built Structure`, place a fresh Window, press the interaction button to open and close it repeatedly, verify collision opens/closes with the visual, then test listen-server + client replication and a world save/reload with one Window left open.

## v2.15.46 Skeletal Window ghost + host-acquisition acceptance

- A Skeletal Window preview must continue to use the character `ARPGBuildingComponent`'s existing global Valid/Invalid Preview Materials; no per-piece ghost material field is introduced.
- Before overriding a Skeletal Mesh ghost, the preview path must call `CheckMaterialUsage(MATUSAGE_SkeletalMesh)` on the assigned material and complete shader preparation. Static Mesh ghosts must remain on their existing path.
- A directly traced completed `WindowWall` must be accepted immediately as the semantic host for an incoming `Window` when the pair is compatible.
- If the exact camera-centre ray misses the host OBB, a bounded third-person view corridor may acquire a compatible Doorway/WindowWall, but it must only select the host; the final transform must still be the host's native insert candidate.
- `WindowWall -> Window` must still advertise its centered native insert socket when `Generate Standard Snap Points = false`. Existing standard-generated WindowWall sockets must not be duplicated, and `Doorway -> Door` behavior must remain unchanged.
- Authority must continue to re-resolve the host from the submitted snapped transform and perform normal access/range/resources/support/collision/duplicate validation. The local acquisition corridor must not authorize free placement.
- v2.15.45 3D Window centering / `Window Insert Offset`, v2.15.44 native skeletal actor/collision, and v2.15.43 structural story/Stair regressions remain enabled.

Recommended PIE acceptance after compiling in UE 5.8: with the same global preview materials already used by Static pieces, select the skeletal Wood Window and verify the ghost renders with the normal invalid/valid colors instead of grey. Aim at the WindowWall frame and then through/near the hollow opening from several third-person camera angles; the ghost should acquire the WindowWall and jump to its centered native socket. Place it, verify a second Window in the same host is rejected, and verify a normal Door still behaves unchanged.

## v2.15.45 suspended Window insert acceptance

- `Doorway -> Door` must continue to center transformed visible bounds on X/Y while aligning Door visible bottom to Doorway visible bottom on Z. This is a protected regression contract.
- `WindowWall -> Window` must center transformed visible bounds on X/Y/Z. It must not reuse the Door bottom-plane Z formula.
- `Window Insert Offset` must be exposed only for `Piece Kind = WindowWall`, default to `0,0,0`, and be applied after automatic Window 3D centering in host-local logical axes.
- Static and Skeletal Windows must resolve through the same transformed asset-bounds path; `Mesh Relative Transform` must be respected before insert centering.
- Current Wood dimensional model check: a 95 cm-high Window in a 271 cm-high WindowWall resolves an 88 cm visible-bottom difference at zero host offset. A +12 cm host Z offset resolves 100 cm.
- Existing full-view insert acquisition, duplicate-insert blocking, hosted-insert structural transparency, Door hinge/collision/replication behavior, v2.15.44 skeletal Window collision and v2.15.43 story/Stair formulas remain unchanged.

Recommended PIE acceptance after compiling in UE 5.8: place a **fresh** skeletal Wood Window into lower- and upper-storey WindowWalls with `Window Insert Offset = 0,0,0`; verify it appears centered in the aperture and remains stable from different camera angles. If the authored aperture is intentionally not centered, adjust only the WindowWall host's `Window Insert Offset`, then verify the same Window asset can be reused against another WindowWall host without changing the Window Data Asset. Re-test one normal Door to confirm its floor alignment is unchanged. Existing placed Window transforms are intentionally not relocated automatically.

## v2.15.44 skeletal build-visual acceptance

- Existing `Build Mesh` / `Preview Mesh` reflected names remain present and existing Static Mesh definitions stay on the original path when no skeletal asset is assigned.
- `Build Skeletal Mesh` and `Preview Skeletal Mesh` are additive soft asset references. A valid final skeletal asset takes presentation/bounds precedence; explicit preview assets may use either type.
- Final `ARPGBuildPieceActor` owns a `USkeletalMeshComponent`; local `ARPGBuildPreviewActor` owns a collisionless skeletal ghost component. Both apply the same `Mesh Relative Transform`, and their skeletal component Tick stays disabled by default so static-pose skeletal assets do not add permanent per-frame work.
- Pivot-aware ground placement, structural snap generation, `WindowWall -> Window` insert centering and validation must read transformed Skeletal Mesh asset bounds when the skeletal path is active.
- Timed construction must drive the active Static/Skeletal `UMeshComponent` for scale, collision and material progress.
- `Window` must resolve to native `ARPGBuildWindowActor`; its bounds-driven collision must remain available even when the imported Skeletal Mesh has no Physics Asset.
- Native Door keeps its existing Static behavior and reparents the optional skeletal visual under the same `DoorPivot`.
- v2.15.43 story/Stair regressions remain enabled unchanged.

Recommended PIE acceptance after compiling in UE 5.8: create a fresh `Window` Build Piece Definition with only **Build Skeletal Mesh** assigned, verify the local placement ghost renders, snap it repeatedly into lower- and upper-storey `WindowWall` openings, verify duplicate Window placement is blocked, then reload the map/save and confirm the visual remains aligned. Then configure the v2.15.48 Window interaction fields and run the dedicated interaction acceptance above.

## v2.15.43 confirmed Stair/story acceptance

- Finished story surfaces remain exactly `0, 300, 600, 900...` for the current kit.
- A LOW-departure `334 × 300 × 278` Stair from finished surface `S=300` must place its rendered LOW plane at `Z=300`, never `Z=322`.
- That Stair's rendered HIGH plane is `Z=578`; the next `18 cm` Floor slab is `Z=582..600`, and the next finished walking surface / next up-flight LOW is `Z=600`.
- Repeated flights must therefore start at `300, 600, 900...`; the old cumulative `+22 cm` Stair actor offset is a hard regression failure.
- Stair-to-Stair structural continuation and Stair-owned landing cells advance exactly `300 cm` in XY/story space for the current kit; raw 334 cm visual run must never become the structural chain step.
- Floor thickness must extend downward from the finished story surface; the old `+18 cm per storey` slab accumulation is a hard regression failure.
- Wall structural story progression must use `bottom + StandardWallHeight`, never rendered Wall mesh top.
- Verified Landscape Stair support, tiled-deck 17 cm art-overhang seams, Stair-owned Floor/Ceiling landings, and parallel Wall/Doorway side seams remain covered by the settlement regression model.

Recommended PIE acceptance: build fresh `Foundation -> Stair -> Floor -> Wall -> Stair -> Floor -> Wall` through several storeys. Floor seams, Wall baselines and Stair LOW anchors must stay on one 300 cm lattice with no progressive vertical or horizontal drift.

## v2.15.42 finished-surface story-plane acceptance

- For a 300 cm story and 18 cm Floor, the first upper Floor must occupy `Z=282..300`; its finished top, upper Wall bottom and lower Wall structural top all equal `Z=300`.
- `Foundation -> Floor`, `Wall -> Floor`, and `Stair -> Floor` must align the incoming Floor **top**, not bottom, to the story/landing plane.
- `Floor -> Wall` and `Floor -> Stair` must use the Floor **top/walking surface** as their story reference.
- Historical v2.15.42 established the finished-top story surface but still left the LOW-departure Stair art `+22 cm` above the current surface. v2.15.43 supersedes that actor-anchor formula; current validation requires the Stair LOW plane to equal the current story surface.
- Storey walking surfaces must remain `0,300,600,900...` with no +18 cm accumulation.

## v2.15.41 structural story-lattice acceptance

- Wall→Wall vertical stacking and Wall→Floor/Ceiling/Roof must resolve the same next-story plane from Wall bottom + `StandardWallHeight`.
- A deliberately short/tall Wall mesh must not change 300 cm story progression; ten-storey model coverage verifies `3000 cm` after ten levels.
- Wall semantic occupancy and inverse Wall/Floor seam validation must use the structural Wall top, not rendered mesh top.
- v2.15.40 relog combat/persistence regression coverage remains enabled.

## v2.15.40 relog persistence/combat validation

The melee/combat regression now statically verifies the relog root fix at all three persistence boundaries:

- `UARPGPersistenceComponent` must explicitly reject `AARPGAICharacter` as an account-character persistence owner before auto-load, manual load, or save.
- `AARPGAICharacter` must disable inherited account auto-load, auto-save, and save-on-EndPlay defaults.
- `UARPGSaveSubsystem::SaveCharacter()` and `LoadCharacter()` must reject AI characters even if invoked outside the persistence component.
- The model asserts player-character persistence remains allowed while AI account-character persistence remains rejected.

This specifically protects the relog sequence where `GetLastCharacterId()` is already valid before NPC BeginPlay and would otherwise alias enemies onto the player's save slot.

## v2.15.39 combat/targeting faction-integrity validation

- Verifies character load does not call `SetPrimaryFactionId(NAME_None)` over an already valid runtime/default faction when the saved faction id is empty.
- Verifies a non-empty saved faction still restores normally and reputation restoration is preserved.
- Verifies `CanDamageActor()` checks reciprocal target-AI hostility, closing the player-vs-hostile-AI neutral/unknown asymmetry.
- Verifies lock-on distinguishes valid faction identities from merely-present Faction components and recognizes hostile target-AI intent.
- Regression model covers empty-save/default-faction recovery and neutral base relationship with explicit hostile target AI.

## v2.15.38 Stair tiled-deck overhang seam validation

- Verifies a LOW-departure Stair remains centred on its active 300 cm host cell while its 334 cm art overhangs each structural endpoint by 17 cm.
- Verifies immediate same-story Foundation/Floor/Ceiling neighbours one grid cell from the Stair flight cell are compatible when returned only by the Stair profile/art overhang.
- Verifies a horizontal module centred in the actual Stair structural flight cell remains blocked, including the HIGH-arrival outside/down travel cell.
- Verifies diagonal/non-grid and different-story horizontal neighbours are not legalized by the overhang rule.
- Source validation requires `bNeighborFlatLanding`, `HostStoryPlane`, `NeighborStoryPlane`, `bImmediateGridNeighbor` and the explicit `actual Stair flight cell` guard in `ARPGIsCompatibleStairHostStructuralNeighbor()`.

## v2.15.37 Stair canonical 300 cm XY + Z lattice validation

- Verifies `Standard Wall Height = 300`, Stair visual run `334`, Stair visual rise `278`, Floor tile `300`, and Floor thickness `18` are separate art/structure concerns.
- Horizontal-host Stair sockets use structural LOW/HIGH anchors at `±150 cm` (`Snap Size / 2`) around the Stair bounds centre, not raw visual `±167 cm` endpoints.
- Regression proves a HIGH-arrival Stair actor centre is exactly one 300 cm cell outside the host and a LOW-departure Stair actor centre is exactly on the host cell, with the 334 cm art overhanging each structural end by 17 cm.
- Stair-to-Stair continuation advances exactly `300 cm` horizontally and `300 cm` vertically for the current kit; raw `334 cm` endpoint delta is explicitly rejected by regression.
- Stair-owned Floor/Ceiling landing centres are exactly `±300 cm` from the Stair actor centre while their top/walking surface remains on the canonical landing Z.
- Reproduces the old v2.15.36 drift numerically: `-17 cm` Stair actor shift plus `317 cm` raw endpoint-to-Floor-edge translation placed the next Floor centre at `-334 cm` instead of `-300 cm`, a 34 cm error per storey.
- Preserves Landscape terrain classification, segmented Stair occupancy, Stair-side Wall/Doorway seams and 300 cm canonical story Z.

Recommended PIE acceptance: build a fresh LOW-departure/up-flight Stair from a 300 cm Foundation/Floor cell, snap a fresh Floor to its HIGH landing, then extend several Stair/Floor storeys. Floor cell seams and Wall columns must stay on one straight 300 cm lattice with no 34 cm stair-step drift.

## v2.15.35 Stair Wall structural-anchor side-seam validation

- Fixes the v2.15.34 regression where the reverse Stair-side classifier tested the transformed Wall logical-bounds centre instead of the Wall actor's structural snap origin. Mesh-relative/pivot offsets could therefore move a correctly snapped +/-150 cm Wall off the perceived Stair side plane.
- Requires `ARPGIsValidExistingStairWallSideSeamNeighbor()` to use `IncomingFinal.GetLocation()` transformed into Stair space as `WallAnchorInStair`.
- Uses the incoming Wall-family `SnapSize / 2` as its structural longitudinal half-span instead of decorative mesh/bounds width.
- Regression proves a Wall actor origin at local Y=+150 remains a valid Stair side seam even when a simulated visual/logical bounds centre is offset to Y=176, which would fail the old v2.15.34 centre-based test.
- Shared-landing X=0, legacy +/-17 cm endpoint ownership, longitudinally offset valid side segments and 180-degree facing reversal remain accepted. Centreline, perpendicular end-wall and unrelated parallel-wall cases remain rejected.

Recommended PIE acceptance: reproduce the user's upper-flight case with a fresh Wall and Doorway snapped to the Floor edge beneath either side of the already-built upper Stair. Both should remain **VALID PLACEMENT**. Rotate the same piece 90 degrees across the Stair travel direction and it must remain blocked.

## v2.15.34 Upper-Stair Wall-family side-corridor validation

- Replaces the brittle v2.15.33 reverse Stair-side requirement that a Wall-family actor origin equal one of two exact +/-17 cm overhang-compensation coordinates.
- Validates the actual side relationship instead: Wall/WindowWall/Doorway axis parallel to Stair travel, logical bounds centre on either +/-150 cm side plane, and positive longitudinal overlap with the Stair flight.
- Regression explicitly accepts shared-landing/upper-flight side walls centred at local X=0 as well as the legacy +/-17 cm positions and a longitudinally offset Doorway/Wall-family segment that still overlaps the flight.
- Negative regressions keep perpendicular end walls, centreline walls and unrelated parallel walls outside the Stair run blocked.
- Existing Stair landing sockets, Stair-to-Stair chains, Stair-to-Floor/Ceiling landings, Landscape terrain classification and segmented occupancy remain unchanged.

Recommended PIE acceptance: build a lower Stair and an upper Stair from the same landing, then place fresh Wall and Doorway pieces on both side edges beneath the upper flight. They should remain **VALID PLACEMENT** when parallel to the Stair. Rotate either piece 90 degrees across the Stair travel path and it must remain blocked.

## v2.15.33 Stair landing + side-seam integration validation

- Completed Stair actors expose pivot-aware `Floor` / `Ceiling` sockets at both HIGH and LOW endpoints. The landing tile extends away from the flight and its visible top is coplanar with the Stair endpoint.
- The `334 × 300 × 278` Wood Stair test proves the high landing near edge exactly equals `StairMax.X` and the low landing near edge exactly equals `StairMin.X`; the 17 cm run overhang does not drift the 300 cm Floor grid.
- Stair-first `Wall` / `WindowWall` / `Doorway` placement accepts only parallel side edges of either logical 300 cm Stair cell (`-17 cm` / `+17 cm` centres for the current asset). Perpendicular end-wall and wrong-axis cases remain negative tests.
- `ARPGIsValidSnappedBuildNeighbor()` consumes the reverse Stair-side seam, keeping active-target choice/build order irrelevant while retaining normal modification-access checks.

## v2.15.32 Stair landing-chain + multi-storey alignment validation

- Verifies every Foundation/Floor/Ceiling edge advertises **two** Stair transforms with one shared edge center/yaw: the retained HIGH-arrival/down-flight candidate and the new LOW-departure/up-flight candidate.
- Regression models the current `334 × 300 × 278` Stair on the +Y edge of a 300-unit host. The HIGH-arrival candidate places the Stair high end at `Y=150` and low end at approximately `Y=484`; the LOW-departure candidate places the low end at the same `Y=150` landing and the high end at approximately `Y=-184`.
- Verifies a completed Stair advertises both endpoint-chain transforms: `LOW(incoming) -> HIGH(target)` for upward continuation and `HIGH(incoming) -> LOW(target)` for downward continuation, with zero relative yaw.
- Proves the horizontal host's LOW-departure transform is numerically identical to the lower Stair's HIGH-end continuation transform when that lower Stair already lands on the same edge. This is the core no-drift guarantee for multi-storey stair chains.
- Verifies capture recognizes both horizontal landing hosts and Stair chain targets. Candidate-envelope affinity participates in ranking so paired up/down sockets are not selected by overlap iteration order alone.
- Verifies side-wall structural classification detects which Stair endpoint owns the horizontal host edge: HIGH-arrival uses the neighbouring outside stairwell cell, while LOW-departure uses the host cell. End-wall and genuine horizontal travel-path conflicts remain blocked.
- Verifies v2.15.31 Landscape terrain classification, v2.15.26 segmented occupancy and v2.15.27 broad/final occupancy consistency remain active. No reflected Blueprint API or save-schema migration is introduced.

Recommended PIE acceptance: build a lower Stair that arrives at a Foundation edge. Without changing the Foundation, place a second Stair from that same edge and confirm its LOW end snaps to the exact lower-Stair HIGH landing/centerline and rises upward. Then aim directly at the top of the lower Stair and verify Stair-to-Stair continuation resolves to the same transform. Repeat the inverse at the lower endpoint and on an upper Floor/Ceiling landing. Rotating/selecting another edge should move the entire chain to that edge without lateral drift.

## v2.15.31 Stair edge-landing + Landscape terrain classification validation

- Verifies standard Foundation/Floor/Ceiling Stair sockets still anchor `StairHighEndLocal` to the selected host perimeter edge and align the Stair top/landing plane with the host top surface; v2.15.31 does **not** move the accepted Stair transform.
- Regression models the current `334 × 300 × 278` Wood Stair on a 300-unit host: the HIGH end lands at the host edge/top while the LOW end extends outward by 334 units and descends by exactly 278 units.
- Verifies `ALandscapeProxy`/`ARPGIsLandscapeTerrainActor()` is classified before generic non-build blocking, and only after `ARPGTransformMatchesStairHostCandidate()` proves the incoming Stair still occupies a native Foundation/Floor/Ceiling socket.
- Verifies the `Landscape` engine module is a **private** dependency and the bypass is Landscape-specific. Static-mesh rocks, cliffs, props and other non-Landscape WorldStatic actors continue through `ARPGIsValidStairWorldSupportContact()` and the normal blocker path.
- Verifies the v2.15.26 segmented Stair profile and v2.15.27 shared broad/final occupancy contract remain active, together with side-wall/end-wall structural rules and authority re-acquisition.

Recommended PIE acceptance: use the exact isolated Foundation repro on Landscape. A fresh Stair should keep its HIGH landing on the Foundation edge, extend outward/down, and remain **VALID PLACEMENT** even where the lower flight embeds into Landscape. Move a discrete Static Mesh rock or Wall into the middle/upper Stair path and placement must still turn blocked.

## v2.15.27 Stair query/final-occupancy consistency validation

- Verifies `ARPGGatherPlacementOverlaps()` and `ARPGPlacementVolumesOverlapMeaningfully()` both consume `ARPGBuildPlacementOccupancyOBBs()` rather than maintaining separate Stair geometry rules.
- Verifies a Stair is never reconstructed as one full PlacementBounds OBB during final build-vs-build validation; the same eight diagonal slices used to discover overlaps are used to decide whether a neighbouring build actor meaningfully occupies the Stair path.
- Regression specifically covers the v2.15.26 failure where the snapped Stair query found an adjacent Foundation/Floor through decorative collision, then the final neighbour pass used the old full Stair box and returned `Blocked by another object` anyway.
- Verifies standard non-Stair build pieces still use one authored logical OBB and that authority shares the same `EvaluatePlacementInternal` path. No reflected API or save migration is introduced.

Historical v2.15.27 acceptance note: occupancy consistency remains regression-covered. v2.15.30 retains the edge-landing transform and corrects the WorldStatic support classifier so one continuous terrain actor may support multiple lower Stair slices without becoming an automatic blocker.

## v2.15.26 Stair profile collision root validation

- Verifies snapped Stair placement no longer uses one full enclosing `PlacementBounds` box as the collision query. Standard Stairs are decomposed into eight small oriented slices following local `+X` uphill and local `+Z` rise.
- Verifies the low-foot and high-landing ends are clearance-shrunk, so flush terrain/host seam contact is not reported as a blocker merely because it touches the old box boundary.
- Verifies empty space underneath the high half of the Stair is not queried as solid occupancy. v2.15.30 keeps this profile logic and classifies WorldStatic support per touched slice, accepting only surfaces at/below the slice underside.
- Verifies the existing Stair host, side-wall and travel-opening rules remain active; the profile query changes collision shape, not structural permissions. Preview and authority both use the same `EvaluatePlacementInternal` path.

Historical v2.15.27 acceptance note: this test targeted occupancy consistency only. Current PIE acceptance is defined by the v2.15.30 edge-landing + per-slice WorldStatic support section above.

## v2.15.25 Stair edge-landing snap-transform validation

- Verifies standard Stair sockets are generated on the **four edges** of Foundation/Floor/Ceiling hosts rather than at the host center.
- Verifies the logical Stair convention is local `+X = uphill/toward landing` after `Mesh Relative Transform`. The transformed Stair high-end center is aligned to the selected host edge, and the Stair visible top is aligned to the host visible top.
- For the current `334 × 300 × 278` example on a 300 grid, verifies the +Y host-edge candidate places the high end exactly at `Y=150` while the low end extends outward to approximately `Y=484`; the old centered/bottom-on-host transform is not the native socket.
- Verifies Stair capture still considers both the compatible support bounds and the corrected edge-landing candidate envelope, so aiming down the run can acquire the Stair even when the host is farther than the normal capture distance.
- Verifies a same-plane horizontal tile in the Stair travel cell is **not** blanket-whitelisted. Only exact side Wall-family seams parallel to the Stair run are accepted; end walls remain blockers.
- Verifies authority re-resolves the same edge-landing candidate and no reflected Blueprint API/save-schema migration is introduced.

Historical v2.15.25 acceptance note: the edge-landing transform remains the intended topology. v2.15.30 fixes the overly strict v2.15.29 assumption that the same terrain actor could support only the first slice.

## v2.15.24 Wood Stair structural-snap validation

- Verifies the standard Stair host family is exactly `Foundation`, `Floor` and `Ceiling`; `Roof` remains outside the flat generic Stair contract.
- Verifies Stair capture uses the compatible support's transformed visible bounds plus the incoming candidate envelope and exact candidate origin. For the current 334×300 Stair on a 300 grid, aiming at the 150 cm host edge therefore succeeds without increasing the default 140 cm global capture distance.
- Verifies authority can still re-resolve the exact native Stair candidate from the snapped transform; the support-aware local capture is presentation/acquisition polish, not an authority bypass.
- Historical v2.15.24 check: Stair-host seam handling was introduced here; v2.15.25 supersedes the centered-host transform and tightens horizontal-cell blocking.
- Verifies Walls across the Stair's +/-X travel opening and perpendicular side-wall axes are **not** auto-accepted and remain subject to normal blocking validation.
- Verifies the current editor example `334 × 300 × 278 -> Placement Bounds 167,150,139` without changing the established 300-unit Snap Size / Standard Wall Height contract.

Recommended PIE acceptance: start with an isolated Foundation, select Wood Stair and aim at the Foundation top/edge from several camera angles. Confirm the ghost locks to the Foundation and rotates through four cardinal directions. Repeat on an upper Floor, then add side Walls and adjacent Floor tiles; the Stair should remain valid. A Wall directly across the Stair's lower or upper travel opening should still make that conflicting placement invalid.

## v2.15.23 multi-cell native Wall-facing validation

- Regression models 1×2 and 2×2 Foundation/Floor footprints and verifies outer-perimeter `Wall` / `WindowWall` / `Doorway` slots receive one unambiguous occupied-cell claim.
- Verifies the final facade yaw comes from the claiming horizontal support's **native standard Wall socket**, not from camera yaw or a hard-coded assumption about the imported mesh's front axis.
- Verifies a shared interior edge receives opposite occupancy claims and is treated as intentionally ambiguous; direct vertical-stack continuity is preferred, otherwise the selected/native yaw is retained.
- Verifies first-storey Foundation-top and upper-storey slab-bottom/story-plane ownership remain distinct, preserving the v2.15.20 no-gap rule.

Recommended PIE acceptance: place two adjacent Foundations, stand inside the footprint, build fresh perimeter Walls/Doorways on every outside edge, then repeat one storey above. Confirm outside facades remain consistent. A shared middle partition may preserve the chosen/native facing because both sides are occupied.

## v2.15.22 hosted-insert structural-transparency validation

- Verifies `Door` / `Window` actors are resolved back to their exact compatible Doorway/WindowWall host through the host's native insert transform.
- Verifies `Doorway -> Door -> upper Floor` and `Doorway -> upper Floor -> Door` are both accepted when the structural seam is valid.
- Verifies a hosted insert cannot independently return `Blocked by another object` for a later valid Wall/Floor/Ceiling/Roof seam around its host.
- Verifies duplicate inserts, unrelated hosts, ownership/access failures and genuine logical occupancy conflicts remain blocked.
- Verifies a directly stacked lower Wall-family piece can preserve upper-storey visible yaw while the horizontal slab still supplies canonical story-plane Z.

## v2.15.20–v2.15.21 story-plane/facing validation

- Regression models a 300×300×18 Floor and verifies both `Wall stack -> insert Floor` and `Floor -> upper Wall` place the upper Wall on the same canonical finished-top/story plane.
- Verifies Floor thickness does not accumulate into storey height and does not create an 18 cm facade gap.
- Verifies upper Wall-family orientation is normalized after slot selection without changing the corrected story-plane transform.
- Verifies first-storey Walls still use the Foundation's visible top rather than the upper-slab rule.

## v2.15.19 insert-host and upper-story ownership validation

- Verifies Door/Window inserts accept legitimate structural neighbours of their designated opening host without disabling duplicate/conflict blocking.
- Verifies competing horizontal/vertical Wall-family candidates do not alter the logical structural slot or permission checks.

## v2.15.18 semantic structural-slot collision validation

- Verifies build-vs-build placement collision compares authored `PlacementBounds` for both incoming and existing structures through oriented-box SAT instead of treating rendered Static Mesh collision as structural occupancy.
- Verifies decorative post/brace/lip collision can overlap the broad physics query without blocking when the two logical placement volumes do not penetrate.
- Verifies true duplicate/logical-volume penetration remains blocked unless a declared snap or strict inter-story seam explicitly owns that relationship.
- Verifies Wall/WindowWall/Doorway edge occupancy compares the wall run axis modulo 180 degrees: front/back reversal is accepted on the same edge, while a perpendicular wall axis or center-crossing wall remains rejected.
- Verifies v2.15.16 native-snap-first -> strict-seam-fallback ordering remains intact and non-building world blockers are unchanged.
- Static/model validation is not an Unreal Engine compile; UE5.8/5.8.1 PIE remains the runtime acceptance test.

## v2.15.16 symmetric inter-story seam fallback validation

- Verifies `ARPGIsValidSnappedBuildNeighbor()` no longer returns `false` merely because `Neighbor->GetSnapTransformsFor()` produced zero candidates.
- Verifies native snap matching runs first, followed by the strict `ARPGIsValidUpperHorizontalWallSeamNeighbor()` and `ARPGIsValidWallUnderUpperHorizontalSeamNeighbor()` fallbacks.
- Regression-models the reported build orders where a 300×300×18 Floor is inserted after pre-stacked upper Walls and where a Wall is inserted between existing lower/upper horizontal slabs.
- Negative cases remain blocked through the existing exact edge/facing/story-plane checks; no global building-overlap tolerance is introduced.

## v2.15.15 wall-between-upper-slabs seam validation

- Regression models a Wall/WindowWall/Doorway snapped to the top of a lower horizontal support while an upper Floor/Ceiling/Roof already exists one storey above.
- The upper slab is accepted only when the incoming wall is on one of its exact structural edges, has the canonical edge-facing yaw, and the wall visible top meets the slab visible bottom.
- A wall through the slab center, wrong-facing wall, wall extending through the slab, and unrelated-height slab remain rejected.
- Existing modification-access enforcement, v2.15.14 horizontal-after-prestacked-wall seams, and full settlement/source/package validation remain required.

## v2.15.14 inter-story Floor/Wall seam validation

- Regression covers a 300×300×18 upper Floor inserted after the next-storey walls are already vertically stacked.
- Exact +Y/+X tile-edge upper walls at the shared story plane are accepted, including perpendicular cardinal facing.
- Lower supporting walls and walls placed on top of an existing slab remain accepted.
- A wall through the tile center, a wrong-facing edge wall, and an unrelated-height wall remain blocked.
- Full settlement model and source validation are run together with package manifest verification.

## v2.15.13 multi-support upper-floor validation

- Verifies Floor/Ceiling/Roof snap capture uses the incoming candidate's transformed visible envelope instead of requiring aim within `SnapCaptureDistance` of the tile origin.
- Regression models a 300 cm floor: aiming at its supporting edge is 150 cm from center (outside the 140 cm legacy origin capture) but zero distance from the candidate bounds and must capture.
- Verifies additional completed build neighbours are still accepted only when they advertise the exact incoming physical slot.
- Verifies horizontal seam rotation equivalence: 300×300 footprints accept 90/180-degree supporting-wall yaw differences; 300×600 rectangular footprints reject 90 degrees but accept 180 degrees.
- Confirms the existing modification-access check remains applied to every tolerated support/seam neighbour.

## v2.15.12 persistent build ownership reload validation

Static/model validation now additionally checks that:

- the local account index persists a stable `GuestCharacterId`;
- Guest/no-login `RegisterCharacterId` and `GetLastCharacterId` use that identity instead of discarding it;
- Character persistence restores a valid Guest identity without requiring an account login;
- the first legacy Guest auto-load attempt can defer one frame so GameMode world loading can recover pre-v2.15.12 ownership regardless of BeginPlay/timer ordering;
- legacy world recovery runs only for exactly one player-controlled locally owned character and one unique no-account owner identity;
- ambiguous multiplayer/multi-owner worlds are not silently reassigned;
- the existing authoritative `SnapTarget->CanActorModify` security check remains present rather than being bypassed to hide the symptom.

Recommended acceptance test: in standalone PIE as Guest, build a Foundation + Wall/Doorway/Door, save/exit, reload, then snap another Wall to the loaded Foundation/Wall and confirm the placement HUD remains valid instead of reporting `Building is restricted here`. Also confirm a genuinely foreign/faction-protected structure is still rejected. Static/model checks are not a UE 5.8.1 compile or runtime result.

## v2.15.0 settlement building / storage / production validation

Repository validation now additionally checks that:

- the inherited player Building/BuildingUI path exposes the Build Catalog and ready build/placement/structure UI classes;
- placement ghosts are local-only/collisionless while the server independently re-resolves snap transforms and validates resources/range/collision/support/faction/territory;
- standard structural snap coverage includes foundations, wall/window/door families, floors/ceilings/roofs, roof continuation and stairs, with custom snap transforms remaining available;
- timed construction uses synchronized server time, material/reveal presentation and Tick only while construction is incomplete;
- native door state is replicated, access-controlled and persisted;
- storage/furnace transfer UI uses exact runtime InstanceIds so durable item identity/condition is preserved;
- station-required recipes reject missing/wrong stations, duplicate ingredients aggregate, and input/fuel/output transactions contain rollback/capacity guards;
- world save schema v5 persists remaining construction time and door state while retaining existing storage/station/durability behavior.

`Tools/test_building_settlement_system_model.py` covers this integration alongside the complete prior framework regression suite. These checks are static/model checks, **not** an UnrealHeaderTool/MSVC/PIE compile/runtime result. A real UE 5.8.1 Development Editor build and multiplayer/standalone placement test remains the acceptance step.

## v2.7.1 spawned Free-Roam navigation readiness

Static/model validation additionally checks that:

- Wanderer uses the concrete UE path-following request result and includes `Navigation/PathFollowingComponent.h`;
- initial/failed Free-Roam navigation requests schedule short retries rather than relying on the normal long ambient Think interval;
- only `RequestSuccessful` establishes the Free-Roam startup handshake;
- disabling or temporarily pausing Wanderer cancels pending startup retries;
- social AI rejects an enabled Free-Roam participant until locomotion has successfully established once;
- no reflected Blueprint API was added for the native-only readiness state.

These checks are not a UE 5.8.1 compile or runtime test. Repeated PIE starts with spawner-created NPCs remain the acceptance test for the intermittent startup symptom.


## v2.6.1 Free-Roam / social startup reliability

Static/model validation additionally checks that:

- Wanderer temporary movement ownership is tracked independently from the persistent `bEnabled` authoring/spawner state;
- social interaction uses its own movement-pause reason and releases only that reason;
- social candidate selection rejects NPCs already owned by another temporary Wanderer recovery state;
- spawner group cohesion uses its own pause reason and reissues leader recovery through the full hysteresis band until `RecoveryRadius` is reached;
- cohesion does not issue competing `MoveTo` requests while an NPC is socially engaged;
- enabling Free Roam requests an immediate first destination while the normal recurring Think timer remains timer-driven;
- Wanderer Think yields to an active spline route;
- the first automatic social opportunity is delayed through the existing Opportunity Retry range so spawned NPCs can initialize and disperse first.

Repository checks do not replace a real UE 5.8.1 Development Editor build/package and PIE observation of spawned NPC movement.

## v2.6 ambient NPC social interaction checks

- `AARPGAICharacter` owns a disabled-by-default `AISocial` component.
- Social scanning is timer/overlap based and capped; there is no permanent social Tick.
- Pair matching requires a common interaction id and symmetric tag/faction acceptance; hostility in either direction is rejected.
- Pair reservation, approach/interact/end state and combat interruption are server-authoritative.
- Wanderer/spline ambient movement is paused/restored around social encounters.
- `Tools/test_ai_social_interaction_model.py` covers same/friendly/neutral/hostile relationships, tag gates, common interaction ids, pair state progression and interruption cleanup.


## v2.5.4 packaged-build compatibility

The user-provided UE 5.8.1 Windows packaging log identified the external DirectionalLight `GetComponent()` call as a non-editor compile failure and two deprecated Inventory soft-pointer assignments as C4996 warnings. v2.5.4 replaces those paths and adds static regression checks. These checks are not a substitute for UnrealBuildTool/UHT/MSVC packaging; a fresh local Windows package remains required.

## Historical source-validation baseline — v2.2.1-alpha

Package: **Akuma's RPG Framework — 2.2.1-alpha Quick Access / Active Item Slots**  
Target: **Unreal Engine 5.8 / 5.8.1**

## Important limitation

This generation environment does not contain Epic's UE 5.8.1 build toolchain, so the package is **not** claimed to have completed a real Unreal Development Editor compile here. The user's local UE 5.8.1 build remains authoritative. Static/source validation is used to catch structural and known source regressions before that build.

All previous real-build fixes through v1.4.1 are retained, including the explicit `Navigation/PathFollowingComponent.h` dependency used by the AI spline component.




## v2.2.1 Quick Access duplicate-instance regression

1. Start with one owned Stone Axe runtime entry (quantity 1) assigned to slot 1. Drag/assign that exact `InstanceId` to slots 4, 7 and 2 in succession. After every assignment, exactly one slot may contain that runtime GUID and the newest target slot must win.
2. Repeat with `Allow Same Item Type In Multiple Slots` enabled. The same exact runtime GUID must still move rather than duplicate.
3. Add two separately owned runtime entries with the same ItemId and different valid `InstanceId` values. With same-item-type duplicates enabled, each distinct instance may occupy its own slot; with the option disabled, assigning either item type moves/removes the other ItemId assignment.
4. Load/replace a deliberately duplicated legacy Quick Access state containing the same runtime GUID in multiple slots. Authority repair must normalize it to unique runtime bindings and must never resolve two slots to the same owned instance.
5. Deplete/remove a bound stack and add a replacement stack of the same ItemId. Rebinding may choose only an unclaimed positive-quantity runtime instance.
6. Verify `Find Slot For Item Instance` returns the exact 1-based slot for a valid bound GUID and 0 for an invalid/unassigned GUID.

## v2.2 Quick Access / consumable checks

The repository validator now asserts that:

- `ARPGCharacter` creates `ARPGQuickAccessComponent` by default and exposes the 1-based input wrappers;
- Quick Access activation resolves real owned runtime Inventory entries and never calls global Item Definition lookup as an ownership substitute;
- equippable slot activation routes through Equipment, while usable items route through the authoritative consumable path;
- consumables support Health/Mana/Stamina restoration, optional Gameplay Effect, exact-stack consumption, cooldown and multicast presentation;
- slot/active state persists in character save data and is owner-only replicated;
- Starting Items can assign a Quick Access slot after their real runtime entry exists.

Recommended in-editor smoke test:

1. Put a sword/axe in Starting Items with `Equip On Spawn = true`, `Quick Access Slot = 1`; put a stack of health potions in slot 2.
2. Bind keyboard `1` to `Quick Access Pressed(1)` and `2` to `Quick Access Pressed(2)`.
3. Press 1 and confirm the exact runtime weapon/tool equips and its normal held visual/audio path remains correct.
4. Damage the character, press 2, and confirm Health restores, quantity decrements, use montage/sound plays and cooldown rejects spam.
5. At full Health, press a health-only potion and confirm it is rejected without consuming quantity.
6. Exhaust a potion stack while another stack exists and confirm the hotbar assignment rebinds to the remaining owned runtime stack.
7. Save/reload and confirm slot assignments/active slot restore against the loaded Inventory GUIDs.
8. In multiplayer PIE, confirm clients cannot create/use an item by assigning a Data Asset alone and remote players still see switched equipment normally.

## v2.1 inventory/equipment usability checks

The static validator now also checks that:

- `Runtime Items` remains read-only editor state while `Starting Items` exposes Item Definition, quantity and Equip On Spawn authoring;
- starting items are converted through the normal authority `Add Item Definition` path and optional auto-equip routes through the Equipment component;
- `ARPGEquipmentVisualActor` exists with static/skeletal mesh support, no collision and no replication by default;
- Item Definitions expose held mesh/custom visual class, socket/relative transform and equipment/tool audio fields;
- Equipment reacts to replicated Inventory changes, creates/attaches/destroys local visuals, and multicasts equip/unequip presentation;
- ordinary melee can prefer equipped-item swing audio while retaining combat-profile fallback;
- Woodcutting uses item gathering swing audio and tree impact can use the equipped tool's gathering-hit audio.

Recommended PIE test: put `DA_StoneAxe` in the player's Starting Items with Equip On Spawn, assign `SM_Stoneaxe`, a valid hand socket and gathering sounds, then confirm host/client both see the axe, Basic Attack chops a tree, Interact still auto-chops, and unequip removes the visual.

## v2.0 Woodcutting / harvestable-tree checks

The static validator additionally checks that:

- `ARPGCharacter` creates the Woodcutting component automatically;
- Item Definitions expose reusable Gathering Tool Tags, Gathering Power and Gathering Tool Tier;
- the Woodcutting component has automatic view targeting, server start/stop RPCs, repeated swing timing, level/XP/progress pure queries and equipped-tool resolution;
- `ARPGTree` exposes Tree Mesh Variations, direct Wood Item rewards, Required Woodcutting Level, optional tool tag/tier gates, fall/stump/respawn settings and Blueprint tree calls/events;
- tree fall is transform-driven and does not teleport the actor; Actor Tick starts disabled and is used only while a trunk is actively falling;
- server chop/fell paths award Woodcutting XP, preflight definition-aware Inventory capacity before adding the selected Item Definition, and route successful item acquisition into normal Collect quest progress;
- tree mesh/state/health/fall direction are replicated and chop/fell cosmetic cues use Niagara with Cascade fallback;
- the explicit UE5.8 Niagara/Cascade pooling enum headers remain present in the new tree feedback translation unit.

### v2.0 recommended runtime cases

1. Create `DA_AshLogs`, create `BP_AshTree : ARPGTree`, add 2-3 tree meshes to Tree Mesh Variations, assign Wood Item and place several trees. Confirm different instances can select different meshes while all clients agree on each selection.
2. Bind player input to `Start Woodcutting From View`; confirm one press begins repeated chopping and leaving range automatically stops it.
3. Fell a tree; confirm per-chop/fell Woodcutting XP is awarded, the selected log item enters Inventory, and the trunk visibly falls away from the player before the stump-only state.
4. Put the log Item Definition into a Build Piece Build Cost and confirm chopped logs can immediately pay that cost.
5. Add a Collect quest objective for the log DefinitionId and confirm successful tree rewards progress it.
6. Create/equip two axe Item Definitions with `Item.Tool.Axe`, different Gathering Tool Tier/Power values, require the tag on the tree, and confirm the best valid equipped tool is used and low-tier tools are rejected when Minimum Tool Tier is raised.
7. Raise Required Woodcutting Level above the player level and confirm the server rejects chopping with the exposed failure result; add XP until the level gate is met and confirm it becomes harvestable.
8. Observe a felled tree through its respawn timer; confirm the stump remains, health resets, and the tree can optionally reroll its visual variation on respawn.
9. Fill the player Inventory so the complete configured wood reward cannot fit; confirm no partial reward is silently added and `On Tree Reward Granted` reports failure.
10. In listen-server + client PIE, chop from the client and confirm damage/rewards are server authoritative while fall/FX/audio are visible on both peers.


## v1.10 distance-population checks

The static validator additionally checks that:

- `ARPGAISpawner` exposes default-on distance population streaming, separate activation/despawn radii, unload delay, player-distance mode and route-aware spawned-pawn relevance;
- the runtime iterates server player controllers using UE world player-controller iteration rather than per-frame Actor Tick;
- activation/deactivation paths exist and clean unload removes the pawn destruction delegate before destroying streamed actors;
- inactive population runtime timers are stopped;
- desired/alive-count preservation and pending respawn timing state exist;
- `Respawn Mode = Never` is explicitly handled so streaming cannot resurrect permanent kills;
- Blueprint status, evaluation/control helpers and activation/deactivation delegates remain exposed.

## v1.9.1 UE 5.8.1 compile fix

The user's UE 5.8.1 Development Editor build reached the new day/night translation unit and failed on `#include "Engine/SkyAtmosphere.h"`. UE 5.8 declares both `ASkyAtmosphere` and `USkyAtmosphereComponent` in `Components/SkyAtmosphereComponent.h`. The invalid Engine include has been removed, and the source validator now checks this exact regression.

## v1.9 day/night checks

The static validator additionally checks that:

- the framework contains the placeable replicated `ARPGDayNightCycle` actor and global `ARPGWorldTimeLibrary` Blueprint-pure query library;
- Host System Clock remains the default source and the runtime uses `FDateTime::Now()` on authority;
- the authority clock is replicated and clients extrapolate from replicated time using real elapsed seconds;
- the built-in rig contains Sun, Moon, Sky Light, Sky Atmosphere and Exponential Height Fog components;
- Sky Light Real Time Capture is used for dynamic time-of-day rather than repeated `RecaptureSky()` calls;
- pure Is Day / Is Night / world-hour / date-time / phase / daylight nodes and phase events remain present.

### v1.9 runtime cases

1. Place one `ARPG Day Night Cycle`, leave Host System Clock selected, start PIE and verify the displayed/getter time matches the host PC local clock.
2. Test around configured Day Start / Night Start using Fixed Time; confirm Is Day / Is Night and Dawn/Day/Dusk/Night phase nodes/events change at the authored boundaries.
3. Use Simulated Clock at a high rate; verify sun/moon rotation, Sky Light fill and fog transition smoothly through sunrise/sunset.
4. Listen-server + client: verify both machines report the host's world hour even if the client PC clock differs.
5. Disconnect/reconnect a client and verify the replicated time snaps to the host once, then advances smoothly rather than stepping every replication interval.
6. Disable Built-In Lighting Rig, assign external Sun/Moon/Sky/Fog actors and confirm the same clock drives the project lighting rig.


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

## v2.0.2 Woodcutting polish checks

The validator now also verifies that:

- normal `ARPGCombatComponent` Basic Attack calls the Woodcutting context redirect before starting an ordinary attack;
- auto-chop from Basic Attack is enabled by default and requires an equipped axe/Woodcutting tool by default;
- real combat/lock-on targets retain priority because explicit non-tree targets are not redirected;
- Basic Attack chops are single-swing and rate-limited by the authored Woodcutting swing interval;
- chop animation can fall back to the normal melee combat montage when no dedicated chop montage is assigned;
- `ARPGTree` exposes minimum/maximum mesh scale, replicated selected scale and Blueprint scale APIs;
- the authority randomizes the tree scale and both trunk/fall hierarchy and stump receive the same multiplier;
- selected tree scale is included in lifetime replication.

## v2.0.1 UE5.8.1 compile-regression checks

The validator also rejects the exact `ARPGTree::GetSelectedTreeMesh()` mixed `TObjectPtr<UStaticMesh>` / raw `UStaticMesh*` conditional expression that produced MSVC C2445 in the user's real UE5.8.1 build. It additionally rejects a direct `NetUpdateFrequency =` write in `ARPGDayNightCycle`, keeping the code on UE5.8's setter API.

## Expected static result

Run:

```text
python Tools/validate_source.py
```

The machine-readable result is emitted separately as `AkumasRPGFramework_validation_v2.1.1.json` during packaging.

## Recommended UE 5.8.1 test pass

1. Replace the complete previous plugin folder with v2.1.1 and clear project/plugin `Intermediate` plus old plugin `Binaries`.
2. Build Development Editor / Win64.
3. Place one `ARPG Day Night Cycle` and test Host System Clock, Fixed Time and accelerated Simulated Clock before the retained combat/AI regression pass.
4. In a listen-server + client PIE session, verify both peers report the host world time and the client lighting transitions smoothly between clock syncs.
5. Create/use a passive `ARPGAICharacter` chicken with no faction configured. Leave `Retaliate When Attacked`, `Retaliate When Faction Unknown`, `Call For Help When Attacked`, and `Assist Same Class When Faction Unknown` enabled. Attack one chicken and confirm it immediately stops free roam, targets the player, chases and attacks.
6. Place another chicken inside `Ally Assist Radius`; confirm it joins the fight even with no faction Data Asset.
7. Spawn several chickens from one `ARPGAISpawner`; confirm same-spawner-group assistance works even when `Stay Together` is disabled.
8. Assign a Chicken Faction Data Asset and keep Player relationship at `0`; confirm the chicken remains passive until hit, then retaliates because neutral retaliation override is enabled.
9. Set Chicken -> Player relationship negative with `Attack Hostile On Sight = true`; confirm proactive acquisition works.
10. Set `Attack Hostile On Sight = false` while keeping the negative relationship; confirm the chicken does not initiate combat but still retaliates when attacked.
11. Set `Fallback: Attack Players On Sight = true` with missing/neutral faction data; confirm proactive faction-free monster behavior works.
12. Re-test free roam/spline resumption after combat, ragdoll death, lock-on camera, hit FX/audio and stagger/knockback.

### v2.1.1 Equipment / inventory / Woodcutting regression cases

1. Leave `Inventory > Starting Items` empty and use a fresh character/save. `Runtime Items` must remain empty; aiming at a tree and pressing Basic Attack must **not** be treated as an axe chop.
2. Create `DA_StoneAxe` with `DefinitionId` left blank, `Equippable=true`, a valid Equipment Slot, `Item.Tool.Axe`, Static Mesh and gathering sounds. Add it to Starting Items with Equip On Spawn. The runtime ItemId should fall back to `DA_StoneAxe`, and the generated entry must have a valid instance GUID and exact Item Definition soft reference.
3. Confirm the runtime axe entry is actually equipped and `Get Best Equipped Woodcutting Tool Instance Id` returns that same GUID. Merely opening/creating another axe Data Asset must not change this result.
4. Confirm the held visual appears on the authored socket. Enter a nonexistent socket name and verify the component falls back to a valid configured/common hand socket when available and logs a warning instead of silently behaving as if visual setup succeeded.
5. With the valid equipped axe, Basic Attack at an `ARPGTree` must reduce replicated `Current Chop Health` once per allowed swing interval. Unequip/remove the axe and confirm Basic Attack no longer redirects into Woodcutting.
6. Assign Gathering Swing and Gathering Hit sounds on the axe. Confirm the swing cue plays on chop start and the hit cue plays at the successful tree impact; the exact authority-selected tool is used for presentation.
7. Save/load the equipped axe. Confirm the soft Item Definition path survives the new save. For an older ID-only save, configure the matching item in Starting Items and confirm the loaded runtime entry is backfilled without granting a duplicate.
8. Confirm a valid saved equipped axe remains equipped even when Starting Items is empty; saved runtime inventory is authoritative and should not be deleted merely because the default loadout changed.

### v2.0.2 Woodcutting runtime cases

1. Equip an Item Definition tagged `Item.Tool.Axe`, stand in front of an `ARPGTree`, clear lock-on/combat target, and press the existing `Basic Attack`: one chop should occur without wiring a new attack input.
2. Press Basic Attack repeatedly faster than `Swing Interval Seconds`; the tree must not harvest faster than the configured cadence.
3. Lock onto a hostile NPC while holding the axe with a tree also in view; Basic Attack must attack the NPC, not steal the input for Woodcutting.
4. Use `Start Woodcutting From View`; automatic repeated chopping must still work independently of Basic Attack integration.
5. Place several copies of one tree Blueprint with `Minimum Mesh Scale = 0.90` and `Maximum Mesh Scale = 1.10`; runtime instances should receive different sizes while all multiplayer peers agree on each tree's size.
6. Fell a scaled tree and confirm the trunk and stump retain matching scale through the fall/stump transition and optional respawn reroll.

### v1.8 runtime cases

1. Passive neutral chicken attacks only after being hit, kills the player, then immediately clears that temporary hostility; after respawn it ignores the player until attacked again.
2. Truly hostile faction NPC kills the player and is allowed to reacquire after respawn because its authored faction/fallback remains hostile.
3. Spawn at least 8 allied melee NPCs against one player: no more than the configured simultaneous attacker cap should actively commit while the remainder distribute on the waiting ring.
4. Observe waiting NPCs: they should keep facing/focus on the player, orbit/reposition on NavMesh, and rotate into openings after attacks/yields instead of forming one stationary pile.
5. Put an obstacle in one attack slot: the blocked NPC must yield after the slot lease and another NPC should receive an opening.

## Retained compile fixes

The v2.0 tree retains prior fixes discovered through real UE 5.8.1 Development Editor builds: GameplayTags/GameplayTasks descriptor correction, AttributeSet replication fix, battle-pet cooldown fix, CharacterMovement/PlayerState includes, mount Controller shadow naming, targeting `TSubclassOf` ambiguity fix, and AI spline Path Following include fix.

## v1.10 detailed distance-population runtime cases

1. Place an `ARPGAISpawner` with `Enable Distance Based Population` on, Activation Radius 3000 and Despawn Radius 4500. Start farther than 4500 units away and confirm no spawned pawns exist.
2. Walk inside 3000 units and confirm the configured group appears and `Is Population Active` becomes true.
3. Walk between 3000 and 4500 units and confirm the population stays loaded (hysteresis).
4. Walk beyond 4500 units and remain there longer than `Distance Despawn Delay`; confirm the actors are destroyed without `On Spawn Group Defeated` firing.
5. Return inside 3000 units and confirm the group is rebuilt from the spawner.
6. On a long spline, follow a spawned NPC far beyond the spawner's Despawn Radius with `Keep Loaded Near Spawned Pawns` on; confirm it remains loaded while the player is near that NPC.
7. Kill one member of an Individual-respawn group, leave before its respawn delay expires, then return immediately; confirm previously living members return while the killed member waits for the remaining cooldown.
8. Repeat with `Respawn Mode = Never`; confirm killed members do not return after distance unload/reload.
9. With the spawner inactive, profile that its leash/cohesion timers are stopped and only the staggered population relevance timer remains.

## v2.4 day/night spawner regression model

Run:

```bash
python Tools/test_day_night_spawner_model.py
```

The model verifies legacy-disabled preservation, midnight daylight→midnight replacement, morning restoration, distance-unloaded phase switching without background spawning, and clock-jump robustness.


## v2.7 dynamic street-light regression model

Run:

```bash
python Tools/test_dynamic_street_light_model.py
```

The model verifies the default Night/Dawn-on and Day/Dusk-off schedule, Niagara-preferred Cascade fallback semantics, required Blueprint-facing components/APIs, no permanent actor Tick, Day/Night event bindings, low-frequency missing-cycle retry and use of the existing Niagara module dependency.

Recommended UE 5.8.1 runtime check: derive a Blueprint from `ARPGDynamicStreetLight`, attach the street-lamp mesh, position `LampLight`/FX, place an `ARPGDayNightCycle`, then use Fixed Time or accelerated Simulated Clock to cross Dusk -> Night -> Dawn -> Day. Confirm the light and chosen FX activate/deactivate once per transition and that starting PIE directly at a night or day time initializes correctly without waiting for a phase edge.

## v2.7.2 spawned-AI collision / locomotion checks

Static/model validation additionally checks that:

- `ARPGAISpawner` no longer uses `AdjustIfPossibleButAlwaysSpawn` for AI population creation;
- spawned AI use `AdjustIfPossibleButDontSpawnIfColliding` with bounded retries and point-spawner fallback spread;
- a failed safe spawn is skipped rather than deliberately creating an encroaching pawn;
- Wanderer does not mark Free Roam established merely because `MoveToLocation` returned `RequestSuccessful`;
- real 2D actor translation is required before Free Roam becomes established/socially eligible;
- an accepted-but-stationary path is aborted and retried through the normal navigation recovery path;
- no reflected Blueprint API was added for the native-only locomotion proof state.

These checks do not replace repeated PIE/runtime observation or a UE 5.8.1 compile/package test.


## v2.9.0 AI spawner ground-rise entrance checks

Run:

```bash
python Tools/test_spawner_ground_rise_model.py
```

The model verifies that every framework AI uses the inherited replicated `SpawnEntrance` component, that `SpawnOne()` applies the entrance only after normal spawner movement configuration, and that the v2.7.2 `AdjustIfPossibleButDontSpawnIfColliding` safety path remains intact. It also guards the critical visual-only invariant: rise depth is applied to the Character mesh relative transform while the accepted actor/capsule location is retained as a lock target.

Movement-ownership checks require the entrance to stop AIController pathing, disable CharacterMovement, acquire an independent Wanderer pause reason, pause active Spline travel, optionally suspend AI Combat/Social behaviour, and restore/release those owners after the reveal. Replication checks require compact replicated state plus synchronized server world time and short-lived component Tick ownership.

Recommended UE 5.8/5.8.1 PIE acceptance: test initial spawn, Individual + Whole Group respawn, distance unload/reload and Day/Night swaps with both Free Roam and Spline movement. Observe that the mesh rises smoothly, the capsule never travels underground, no NPC translates during the entrance, and normal locomotion begins immediately after completion. Repeat in multiplayer PIE.

These checks are repository/model validation only and do not claim an Unreal Engine compile.
