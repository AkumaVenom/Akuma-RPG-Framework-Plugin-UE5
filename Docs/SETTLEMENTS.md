# Settlement Hubs, Player-Built Paths, Validated Homes & Autonomous Villagers — v2.16.12-alpha

**v2.16.3 structural-plane housing fix:** real UE5.8 PIE testing showed that the original home validator could see the Foundation rectangle yet report completed overhead Floors/Ceilings and perimeter Walls as missing because it compared raw actor origins. v2.16.3 classifies the real transformed build envelopes and canonical finished top/bottom planes used by the building system. The default minimum is now **2x2 Foundations**; Width/Depth remain Data Asset configurable. v2.16.2/v2.16.1 compile fixes remain intact.
**v2.16.4 resident runtime fix:** a recruited villager is now registered exactly once even though initialization state changes call back into the Hub. The registry deduplicates both pawn identity and persistent `ResidentId`, and HUD/worker counts consume the unique view. Woodcutting movement now requires a valid AI controller + Navigation System, projects a navigable approach point around the tree, only enters `Going To Work` after an accepted move request, and abandons accepted-but-motionless routes back to roaming.



**v2.16.12 turn-stability fix:** real PIE testing with a wide cobblestone road exposed a v2.16.11 tangent-magnitude bug at bends. Shared turn tangents now preserve only direction and derive their usable magnitude from the adjacent terrain-sampled spline span; sharp reversals fall back to local travel direction instead of folding the mesh. Existing world-v9 path tangent saves are normalized on restore and require no migration.

**v2.16.11 player-built Settlement Paths:** `Piece Kind = SettlementPath` adds continuous point placement for roads, tracks and walkways. The first confirmation establishes a server-approved start anchor; every following confirmation creates one connected terrain-conforming spline-mesh segment and advances the chain until **Cancel** is selected. Segments are independently owned/replicated/persistent normal build actors, while shared tangent smoothing keeps turns visually continuous.

**v2.16.10 work-tool presentation:** assign `Villager Woodcutting Tool Item` in the Settlement Definition to an `ARPGItemDefinition` that already contains the Axe's equipped mesh/socket/relative transform. Residents hold that contextual tool while travelling to a reserved tree and while chopping, then remove it automatically when roaming/home. This is presentation-only: it does not add/equip an Inventory item or consume durability.

**v2.16.9 environment integration:** settlement workers and player construction now share the same tree/build occupancy contract. Foundations can replace ARPG Tree locations, build-suppressed Trees cannot respawn through homes/settlement structures, and villagers abort a suppressed work target without treating construction as a harvested-tree reward. Hub stockpiles therefore receive only real woodcutting rewards.

**v2.16.8 runtime navigation cleanup:** structural construction still updates/dirties local Dynamic Recast tiles automatically, but completed Stairs now use only their real rasterized Recast surface. Automatic off-mesh Stair links were removed after PIE testing showed they can compete with a correctly configured Supported Agent and cause residents to oscillate at the Stair/door threshold. This remains generic building navigation for every AI, not a villager teleport or settlement-only path.


v2.16.0 adds an opt-in settlement simulation on top of the existing building, faction, AI, gathering, storage, interaction and persistence systems. It does **not** turn every structure into a settlement. A completed `SettlementHub` with an assigned `ARPGSettlementDefinition` is the explicit boundary and authority root for the system.

## Design contract

```text
ordinary building pieces
        +
no Settlement Hub
        =
ordinary building only
```

```text
completed Settlement Hub
        +
valid owned/same-faction configured 2x2+ enclosed home
        +
completed Villager Bed
        =
resident capacity / recruitment / settlement simulation
```

The implementation is deliberately additive. Beds and Hubs use a dedicated horizontal-surface placement path; Settlement Paths use their own continuous terrain-projected authoring session; and the v2.15.53 Stair/Wall-family structural boundary functions remain unchanged. Buildable Lights remain non-blocking fixtures from v2.15.54. Settlement Paths are also non-structural visual dressing: they do not claim structural `PlacementBounds`, suppress Tree respawn or dirty structural Dynamic Recast tiles.

## Native runtime classes

- `UARPGSettlementDefinition` — Data Asset containing settlement radius, housing rules, resident population/timing, woodcutting policy and Hub stockpile capacity.
- `AARPGSettlementHubActor` — buildable Palbox-style settlement core. It derives from `AARPGStorageActor`, so its stockpile uses the existing authoritative inventory/storage system.
- `AARPGBuildBedActor` — buildable assignable Bed with `Unassigned`, `Player`, and `Villager` roles.
- `AARPGBuildPathActor` — one authoritative/persistent Settlement Path segment. It owns a native Spline Component and generates terrain-conforming Spline Mesh sections from the authored Build Mesh.
- `AARPGSettlementVillagerCharacter` — ready resident pawn derived from the framework's existing `AARPGAICharacter`.
- `UARPGSettlementResidentComponent` — resident home/activity/woodcutting state and server-side autonomous behavior.
- `UARPGSettlementUIComponent` — local proximity HUD, Hub panel, Bed panel and reskin class ownership.
- `UARPGSettlementHUDWidget`, `UARPGSettlementPanelWidget`, `UARPGSettlementResidentRowWidget`, `UARPGBedPanelWidget` — ready native widgets with Blueprint replacement hooks.

## 1. Create the Settlement Definition

Create **Content Browser > Miscellaneous > Data Asset > ARPGSettlementDefinition**.

Recommended first-pass values for the current 300 cm Wood kit:

```text
Settlement Radius                  = 5000 cm
Settlement HUD Radius              = 1800 cm
Prevent Overlapping Settlement Areas = true
Settlement Separation Padding      = 0 cm

Settlement Refresh Interval        = 5 s
Initial Recruitment Delay          = 5 s
Recruitment Interval               = 12 s
Maximum Villagers                  = 20
Villager Class                     = BP_YourSettlementVillager
Villager Wander Radius             = 1400 cm
Villager Think Interval            = 3 s

Home Grid Size                     = 300 cm
Minimum Foundation Width           = 2
Minimum Foundation Depth           = 2
Maximum Home Dimension Cells       = 12
Home Story Height                  = 300 cm
Home Grid Tolerance                = 24 cm
Allow Roof As Home Cover           = true

Enable Villager Woodcutting        = true
Woodcutting Duty Chance            = 0.45
Woodcutting Radius                 = 2600 cm
Woodcutting Search Interval        = 8 s
Villager Chop Power                = 25
Villagers Ignore Tree Skill/Tool Requirements = true
Villager Woodcutting Tool Item     = DA_Item_Axe (optional)
Show Tool While Going To Work      = true
Play Tool Equip / Unequip Presentation = true
Villager Chop Interval             = 1.4 s
Woodcutting Acceptance Radius      = 140 cm
Maximum Concurrent Woodcutters     = 0
Deposit Woodcutting Rewards To Hub = true

Settlement Stockpile Slots         = 96
```

`Maximum Concurrent Woodcutters = 0` means every otherwise-eligible resident may work. Use a positive value to cap simultaneous woodcutters.

## 2. Create the Settlement Hub build piece

Create an `ARPGBuildPieceDefinition` and set:

```text
Piece Kind = SettlementHub
Actor Class = None
Settlement Definition = DA_YourSettlement
```

`Actor Class = None` selects the native `AARPGSettlementHubActor`.

The Hub may be authored to sit on a completed Foundation/Floor. If `Allow Ground Placement` is enabled, it may also seat on terrain; authority re-traces the terrain and re-seats the visible bottom instead of trusting a client-supplied Z position.

The Hub is the settlement's ownership and simulation boundary. By default a new Hub cannot overlap an existing completed Hub's settlement radius. This prevents one Bed/home from being recruited by two settlements. Legacy overlapping Hubs still resolve every Bed deterministically to one primary managing Hub: nearest eligible Hub first, then stable Building ID tie-break.

## 3. Create the Bed build piece

Create another `ARPGBuildPieceDefinition`:

```text
Piece Kind = Bed
Actor Class = None
Default Bed Role = Unassigned
Allow Ground Placement = false
```

Beds require a completed Foundation or Floor host. They do not use the Wall/Stair structural snap graph.

Interact with a completed Bed using the same **Interact Built Structure** input used for Doors, Windows and Lights. The native Bed panel provides:

```text
UNASSIGNED BED
PLAYER BED
VILLAGER BED
```

A Player Bed records the assigning character identity. A Villager Bed becomes resident capacity only when it belongs to a valid home inside an operational Hub.

## Player-built Settlement Paths — v2.16.12

### v2.16.12 turn tangent stability

The first project-side spline test confirmed straight sections but exposed severe folding at bends. The source mesh was not the cause: v2.16.11 used the correct shared bisector **direction**, but multiplied it by the full confirmed segment length before assigning it to a spline endpoint. Because terrain sampling subdivides a segment (125 cm spacing by default), that full-length vector could be many times larger than the endpoint's actual neighboring spline span. Unreal's Hermite interpolation then overshot the local interval, producing raised spikes, inverted/folded triangles and dark geometry around turns.

v2.16.12 keeps the same independent-segment architecture and v9 persistence, but endpoint tangent state is treated as directional intent. `AARPGBuildPathActor` normalizes the stored direction and derives the runtime magnitude from only the adjacent sampled span. A forward-dot guard prevents near-U-turn shared tangents from pointing backward through a segment. Older v2.16.11 v9 saves are normalized when restored, so their oversized vectors self-heal. The live pooled preview uses the same bounded start-turn rule.

**Editor impact:** none. Do not compensate with Forward Axis, Terrain Sample Spacing, mesh rotation or extreme Tangent Scale changes. Existing correctly-authored Settlement Path Data Assets should be retested unchanged under v2.16.12.

Settlement Paths are intended for the visual road/path network that makes a player-built settlement read as one coherent place. They use the normal build catalogue and resource system, but their placement interaction is deliberately continuous instead of one-shot.

### Create a Path build piece

Create an `ARPGBuildPieceDefinition` and use the following baseline:

```text
Piece Kind                         = SettlementPath
Actor Class                        = None
Build Mesh                         = SM_YourPathSegment
Preview Mesh                       = optional first-point marker
Allow Ground Placement             = true
Requires Support                   = false
Requires Snap Target               = false
Snap Placement                     = false
Construction Seconds               = 0 (recommended for ordinary roads)

Building | Settlement | Path | Mesh
Settlement Path Forward Axis       = X
Path Mesh Scale                    = (1.0, 1.0)
Settlement Path Tangent Scale      = 1.0
Settlement Path Collision Enabled  = false
Settlement Path Cast Shadow        = true

Building | Settlement | Path | Terrain
Settlement Path Ground Offset      = 2 cm
Terrain Sample Spacing             = 125 cm
Terrain Trace Height               = 250 cm
Terrain Trace Depth                = 500 cm

Building | Settlement | Path | Placement
Minimum Segment Length             = 75 cm
Maximum Segment Length             = 1200 cm
```

`Actor Class = None` selects native `AARPGBuildPathActor`. Settlement Paths require a **Static Build Mesh** because each visual section is a `USplineMeshComponent`; `Build Skeletal Mesh` is not a spline deformation source. Author the source mesh so its length runs along the selected `Settlement Path Forward Axis`. Give the mesh enough vertices/subdivisions along that axis to bend cleanly over terrain, and use `Path Mesh Scale` for width/thickness tuning rather than trying to repair spline orientation with structural `Placement Offset`.

`Preview Mesh` is optional and is used only as the normal first-point ghost before a start anchor exists. Once the first point is confirmed, the framework switches to the actual Build Mesh as a live spline preview from the confirmed point to the cursor.

### Player placement flow

The runtime contract is intentionally simple:

```text
Select SettlementPath
        ->
Place first point
        ->  server confirms start anchor; no segment and no resource charge yet
Move cursor
        ->  live terrain-conforming connecting mesh preview
Place next point
        ->  one authoritative segment is built + one Build Cost is consumed
Move / place next point
        ->  chain continues from the last server-confirmed endpoint
Cancel / End Build Mode
        ->  path session ends
```

The generic `Keep Build Mode After Placement` toggle is intentionally not used for `SettlementPath`: a path session always remains active after a valid point until **Cancel** or `End Build Mode`. `Cancel Settlement Path Placement` is Blueprint-callable and routes through the same Build Mode shutdown.

The placement HUD exposes exact feedback for `PathSegmentTooShort` and `PathSegmentTooLong`. A rejected point does **not** move the confirmed start/end and does not consume resources. Remote clients also use a pending-request guard so repeated confirm input cannot advance locally before the authoritative result arrives.

### Terrain conformity and visual continuity

Each confirmed segment stores only its authoritative endpoints and optional endpoint tangents. The visual spline derives intermediate samples from the underlying Visibility surface at `Terrain Sample Spacing`, capped to 32 visual intervals per segment. Endpoints are preserved exactly; only intermediate points conform to local terrain undulation. Existing `AARPGBuildPathActor` collision is pierced during both preview and finished terrain sampling, so a new path cannot accidentally climb over an older path when optional path collision is enabled.

When a new segment turns away from the previous segment, authority computes a normalized direction bisector at the shared endpoint and applies matching endpoint tangents to both actors. This keeps adjoining sections C1-aligned through ordinary corners while preserving independent build actors for saving and demolition. The tangent override is saved so the turn does not become sharp again after reload.

For very tight corners, prefer adding an extra point rather than forcing an extreme bend through one long segment. `Settlement Path Tangent Scale` is a presentation tuning control, not a structural placement value.

### Collision, navigation and building semantics

Default `Settlement Path Collision Enabled = false` is recommended for decorative dirt, gravel, stone and painted road surfaces. This prevents a visual path from becoming an invisible structural blocker or competing navigation surface. If a project intentionally enables collision, it is applied only to the generated completed path meshes; preview meshes never collide.

Settlement Paths are intentionally excluded from:

- normal structural overlap rejection between build pieces;
- semantic Foundation/Floor/Wall/Stair slots;
- Tree replacement/respawn-suppression occupancy;
- structural Dynamic Recast refresh/dirtying.

This means roads may pass through or immediately beside normal settlement structures without altering the protected v2.15.53 topology. If a game needs paths to affect AI cost/navigation, implement that as a separate project-owned navigation layer rather than relying on decorative path mesh collision.

### Multiplayer, persistence and Blueprint hooks

The server owns the active path session. It reprojects every requested point to a valid surface, rechecks catalogue membership, build range, resources and min/max segment length, and only then creates the next segment. Direct generic `Request Place Piece` / one-shot placement is rejected for `SettlementPath`, preventing a client or Blueprint from bypassing the continuous-session validation.

Useful `UARPGBuildingComponent` Blueprint API/events:

```text
Cancel Settlement Path Placement
Is Settlement Path Placement Active
Has Settlement Path Start Point
Get Settlement Path Last Confirmed Point
Is Settlement Path Request Pending

On Settlement Path Session Changed
On Settlement Path Segment Placed
```

Useful `AARPGBuildPathActor` Blueprint API:

```text
Get Path Start World
Get Path End World
Get Path Segment Length
Get Generated Spline Mesh Count
Refresh Path Presentation
On Path Geometry Changed
```

World save **v9** adds local start/end/tangent data for each path actor. All existing normal build-piece state—Building ID, definition, transform, owner/faction, health, construction state and demolition/refund behavior—continues through the base build actor.

## 4. Native home validation

A Bed is not considered a valid villager home merely because it is indoors visually. The server validates the modular structure semantically.

For the default definition, a valid home requires:

- a complete rectangular Foundation footprint of at least **2 x 2 cells** by default;
- the Bed positioned on/within that Foundation-based home;
- complete overhead coverage for every cell using Floor, Ceiling, or optionally Roof pieces;
- a complete perimeter made from `Wall`, `WindowWall`, or `Doorway` pieces;
- at least one perimeter `Doorway`;
- a **completed Door actually occupying that Doorway's native hosted socket**;
- all contributing pieces completed and manageable by the Hub under owner/account/faction rules;
- the Bed resolving to that Hub as its primary manager.

The validator returns `FARPGSettlementHomeValidation`, exposed to Blueprint, including foundation/cover/perimeter counts, required counts, doorway/door flags, home center/extents and a ready status message.

**v2.16.3 pivot-aware classification:** Foundation cell centers and finished top planes come from each piece's transformed Static/Skeletal build bounds (falling back to `Placement Bounds` only for custom-actor-only pieces). A cover cell is satisfied when a completed Floor/Ceiling/allowed Roof spatially covers that real Foundation cell center and its finished top is exactly one `Home Story Height` above the Foundation top. Perimeter targets are derived from the real Foundation center plus/minus half `Home Grid Size`, then matched against a Wall/WindowWall/Doorway transformed envelope whose finished bottom sits on the Foundation top. This mirrors the native snap contract and does not depend on mesh pivot placement.

The default minimum is 2x2, but `Minimum Foundation Width` and `Minimum Foundation Depth` are Data Asset settings. A larger connected building may contain multiple independently valid homes.

## 5. Resident recruitment and ownership

Recruitment is server-authoritative and runs only while the Hub is completed and has a valid Settlement Definition.

The Hub first re-homes existing homeless residents into valid free Villager Beds. Only then may it recruit a new resident, and it recruits at most one resident per `Recruitment Interval`.

A resident:

- receives a persistent `ResidentId`;
- is assigned to one Villager Bed;
- copies the Hub's owner account, owner character and faction identity;
- uses the framework faction system rather than a settlement-only allegiance hack;
- derives from `AARPGAICharacter`, keeping the existing NPC gameplay stack available;
- can use a native class or a Blueprint subclass selected by `Villager Class`.

For custom presentation, make a Blueprint child of `ARPGSettlementVillagerCharacter`, assign the mesh/AnimBP/stats you want, and select it in the Settlement Definition. Do not replace the resident component or Hub relationship unless you intentionally want custom behavior.

## 6. Resident movement and settlement life

`UARPGSettlementResidentComponent` runs on timers rather than permanent Tick. It integrates with the existing `AIWanderer` component and sets the resident's home around the assigned Bed/Hub.

Typical states are:

```text
Homeless
At Home
Roaming
Going To Work
Woodcutting
Returning Home
```

Residents may roam inside/outside the property within the configured settlement wander radius. Existing AI combat remains authoritative; settlement work does not override an active combat target.

**v2.16.4 locomotion safety:** settlement work does not claim `Going To Work` optimistically. The resident first resolves/recovers its `AAIController`, requires the Navigation System, projects the target to a navigable approach point (important because tree trunks may carve NavMesh), submits the move, and only then changes state. A short native movement-proof timer confirms actual 2D translation. If a request is accepted but the pawn remains motionless/unreachable, the request is stopped, the tree reservation is released and normal roaming is restored. This keeps a bad path from turning a resident into a permanently stuck worker.

**v2.16.5 home-story safety:** the resident's home is no longer the raw Bed actor location. The Hub resolves the validated home's finished floor plane and searches for NavMesh only within a tight vertical band on that story. Recruitment tries multiple collision-safe points on the connected interior surface; if UE would need to push the capsule onto the roof, the attempt is rejected. `Resolve Resident Home Anchor` is Blueprint-callable for custom resident/UI logic. This requires the house interior and doorway path to be covered by usable NavMesh; if they are not, recruitment is deliberately deferred rather than producing a stuck roof resident.

Blueprint-exposed resident calls include:

- `Initialize Settlement Resident`
- `Assign Bed`
- `Clear Bed`
- `Force Choose New Activity`
- `Return Home`
- `Has Valid Home`
- `Is Working`
- `Can Bypass Tree Requirements`
- `Is Woodcutting Tool Visual Active`
- `Refresh Woodcutting Tool Visual`

## 7. Woodcutting jobs

Settlement woodcutting reuses the existing `ARPGTree` actor and its durability/reward path. Villagers do not generate fake Wood from a timer.

When eligible, a worker:

1. searches within the configured Hub woodcutting radius;
2. ignores trees already reserved by another resident;
3. pathfinds to the selected `ARPGTree`;
4. optionally plays the configured replicated chop montage;
5. calls the tree's existing authoritative `ApplyChop` path;
6. receives the tree's normal Wood/bonus rewards;
7. transfers those configured tree reward item IDs into the Hub stockpile after the tree falls.

`ARPGTree` exposes `Allow Settlement Villager Harvest`. The Settlement Definition can allow workers to bypass player-specific axe/skill requirements while still using the tree's real chop/reward implementation.

### Contextual held axe / work tool — v2.16.10

Set `Settlement | Woodcutting | Tool Presentation -> Villager Woodcutting Tool Item` to an existing Axe `ARPGItemDefinition`. The resident reuses that Item Definition's normal equipment presentation fields:

```text
Equipped Static Mesh / Equipped Skeletal Mesh
Attach Socket
Equipped Relative Transform
optional Equipped Visual Actor Class
optional Equip / Unequip Montage
optional Equip / Unequip Sound
```

The native state contract is:

```text
Going To Work + reserved Tree -> axe shown (when Show Tool While Going To Work = true)
Woodcutting                   -> axe shown
Roaming / At Home / Returning Home / Homeless -> axe removed
```

`Show Tool While Going To Work = false` keeps the axe hidden during travel and equips it only once the resident reaches `Woodcutting`. `Play Tool Equip / Unequip Presentation` controls whether the Item Definition's optional montage/sounds are reused on these transitions.

This is deliberately **presentation-only**. The villager does not receive a fake Axe inventory instance, does not overwrite a gameplay equipment slot, does not gain equipped stats/effects and does not consume Axe durability. The local visual is reconstructed from replicated `ResidentState`, `CurrentWorkTree`, Hub and Settlement Definition, so no save-schema change or replicated visual actor is required.

Blueprint cosmetics can query/call `Is Woodcutting Tool Visual Active` and `Refresh Woodcutting Tool Visual`, or listen to `On Woodcutting Tool Visual Changed`.

## 8. Hub stockpile

`AARPGSettlementHubActor` derives from `AARPGStorageActor`. The Hub therefore gets a normal persistent framework Inventory with a configurable slot count.

The full Settlement panel includes **Stockpile**, which hands off to the existing Storage UI instead of creating a second inventory implementation.

Woodcutting deposits also use normal inventory transfer calls, keeping capacity/stack behavior in the existing inventory authority layer.

## 9. Automatic Settlement HUD and interaction UI

`ARPGCharacter` now owns `ARPGSettlementUIComponent` by default.

When `Auto Show Nearby Settlement HUD` is enabled, the local component periodically finds the nearest usable operational Hub. Entering its `Settlement HUD Radius` opens the compact HUD; leaving the radius closes it automatically.

Interacting with the Hub opens the full Settlement panel. Interacting with a Bed opens the Bed assignment panel.

The native Settlement UI exposes Widget Classes on the component:

```text
Settlement HUD Widget Class
Settlement Panel Widget Class
Resident Row Widget Class
Bed Panel Widget Class
```

You can use the native widgets immediately or replace them with Blueprint children. Important data and refresh hooks are Blueprint-exposed, including the `BP_On...Refreshed` events on native widgets.

Useful UI/API calls include:

```text
Open Settlement Panel
Open Bed Panel
Close All Settlement UI
Set Bed Role
Request Settlement Refresh
Open Settlement Stockpile
Refresh Open Settlement UI
Get Nearby Settlement Hub
```

## 10. Hub Blueprint API

Useful native calls on `AARPGSettlementHubActor` include:

```text
Get Settlement Definition
Get Settlement Id
Get Settlement Radius
Get Settlement HUD Radius
Get Settlement Summary
Is Settlement Operational
Is Location Inside Settlement
Can Manage Building
Validate Home For Bed
Get Managed Beds
Get Settlement Residents
Find Bed By Building Id
Find Resident By Id
Can Resident Start Woodcutting
Refresh Settlement Now          [authority]
Register Loaded Resident        [authority]
Notify Resident State Changed   [authority]
```

These calls are intended for custom settlement UIs, quests, settlement upgrades and later profession/job systems without duplicating core ownership/housing logic.

## 11. Faction and access behavior

A Hub manages completed structures in its radius only when normal ownership rules allow them. It accepts exact owner character/account matches, and may optionally accept same-faction structures through `Accept Same Faction Buildings`.

Bed role changes are routed through the existing player-owned `ARPGInteractionComponent` RPC and still require normal range/modification access. Hub panel/stockpile access also uses existing `CanActorUse` rules.

This means settlement villagers are player/faction-owned NPCs within the framework's existing authority model rather than a second independent allegiance system.

## 12. Save/load contract

World save schema remains **v9** in v2.16.12 (introduced by v2.16.11). Character save remains v5.

v8 persists:

- Bed role;
- Bed assigned resident ID;
- Player Bed owner character ID;
- resident ID;
- owning Hub Building ID;
- assigned Bed Building ID;
- resident class and transform;
- resident name and health;
- owner account/character/faction identity;
- Hub stockpile through the existing container persistence path;
- Settlement Path local start/end endpoints and saved tangent overrides.

Load order restores buildings first (including v9 Path geometry), then containers/Hub stockpile, then resident links after Hubs and Beds exist. Older saves remain compatible: v6 Window migration and v7 buildable-Light migration remain intact. Pre-v8 worlds simply have no persisted settlement residents and the completed Hub may recruit normally after its configured initial delay; pre-v9 worlds have no saved Settlement Path endpoint/tangent payload because the piece kind did not yet exist.

## 13. Performance notes

- Hub home/Bed reconciliation uses `Settlement Refresh Interval`, not frame Tick.
- Resident decision making uses `Villager Think Interval`.
- Woodcutting search uses a configurable search interval/radius and per-Hub concurrent-worker cap.
- Settlement HUD proximity polling defaults to 0.35 s locally and can be changed or disabled.
- Bed/Hub placement is a separate surface contract and does not add permanent collision complexity to the structural build graph.
- Settlement Path preview reuses pooled Spline Mesh components while Build Mode is active; finished path actors have no permanent framework Tick. Terrain sampling is bounded to at most 32 visual intervals per segment.
- Settlement Paths skip Tree-occupancy notification scans and structural Dynamic Recast refresh by design.

For very large worlds with many simultaneous settlements, increase refresh/search intervals and set a practical maximum resident/woodcutter count per Hub.

## 14. PIE acceptance checklist

For the first real UE5.8 test:

1. Place a Hub and verify the proximity HUD appears/disappears at the configured range.
2. Build a complete **2x2 Foundation** house (or larger configured rectangle) with full perimeter walls, full overhead Floor/Ceiling, one Doorway and one completed Door.
3. Place a Bed on a Foundation/Floor and set it to `Villager`.
4. Verify the Bed panel reports a valid home only after every required module exists.
5. Wait through the recruitment delay/interval and verify exactly one villager moves in.
6. Confirm the villager is player/same-faction aligned and roams around its Bed/Hub.
7. Place harvestable `ARPGTree` actors in work radius and confirm only one resident reserves a given tree at once.
8. Confirm chopping uses the real tree durability/reward path and rewards reach the Hub stockpile.
9. Open the Hub panel and Stockpile button; confirm the native UI works and your Blueprint subclasses can replace it.
10. Save with mixed Bed roles and at least one resident, reload, and verify Bed/resident/Hub links and stockpile survive.
11. Build a house with no Hub nearby and verify **no settlement recruitment or worker simulation starts**.
12. Author a `SettlementPath` with a subdivided Static Build Mesh. Confirm the first point creates only an anchor, then place several straight/curved points and verify a connected terrain-following road appears until Cancel.
13. Try a point below the Minimum Segment Length and beyond the Maximum Segment Length; verify neither consumes resources nor advances the last confirmed point.
14. Save/reload a curved multi-segment path and verify endpoints/turn smoothing persist. In multiplayer PIE, confirm remote-client point placement advances only after the server result.
15. Re-run the confirmed Stair/Wall/Window/Light construction combinations around/through the path. They must behave exactly as the v2.15.53/v2.15.54 baseline and Trees must not be suppressed merely by path coverage.

## Validation limitation

The repository's model regressions and source validator check architecture, source invariants and migration contracts. They are **not** UnrealHeaderTool/MSVC/PIE. The project's UE5.8 build and runtime tests remain authoritative for reflected-property compilation, navigation, animation assets and final content tuning.

## Runtime-built navigation and Stair traversal

A valid home can have green NavMesh on its interior Foundation/Floor and surrounding terrain only when the project's Recast agent can actually rasterize the doorway/Stair route. v2.16.8 keeps the v2.16.7 runtime build-navigation refresh: structural pieces explicitly update their Navigation Octree entry and dirty only local Dynamic Recast tiles when collision becomes final or is removed.

Stairs no longer create automatic `NavLinkProxy` actors. With the project's **Supported Agent** configured to match the villager/AI navigation profile, the Wood Stair should appear as a continuous green walkable surface and normal `MoveTo` path following should traverse it directly. This avoids competing off-mesh shortcuts and endpoint oscillation. The framework does not overwrite Agent Radius/Height; those remain project-owned navigation settings.

Press **P** in PIE and verify the interior, doorway, Stair and terrain form one usable Recast route. Acceptance is that residents can path interior -> Stair -> terrain and terrain -> Stair -> interior while roaming/working, with `Custom NavLinks count` remaining zero for framework-generated Stair navigation.
