## v2.16.10-alpha — Settlement Villager Contextual Woodcutting Tool Presentation

- Added state-driven settlement worker tool presentation. A resident with a configured `Villager Woodcutting Tool Item` now holds that Item Definition's equipped visual while travelling to a reserved tree (`Going To Work`) and while chopping (`Woodcutting`), then automatically removes it when returning to `Roaming`, `At Home`, `Returning Home` or `Homeless`.
- Reuses the existing Item Definition equipment presentation contract: `Equipped Static Mesh` / `Equipped Skeletal Mesh`, `Attach Socket`, `Equipped Relative Transform`, optional custom Equipment Visual Actor, and optional Equip/Unequip montage + sounds. Designers can therefore point settlements at the same Axe Item Definition already used by players.
- The worker tool is presentation-only and never grants/removes an Inventory item, changes authoritative equipped slots, applies equipment Gameplay Effects or consumes durability. Existing settlement policy may still bypass player axe/skill requirements; the held axe is visual state driven by the real resident work assignment.
- Added reusable `Create Transient Equipment Visual` / `Destroy Transient Equipment Visual` Equipment-component APIs for contextual held props without Inventory mutation. Resident tool visuals are local/non-replicated actors reconstructed from replicated `ResidentState`, `CurrentWorkTree`, Hub and Settlement Definition, keeping bandwidth and save state unchanged.
- Added Blueprint exposure through `Is Woodcutting Tool Visual Active`, `Refresh Woodcutting Tool Visual` and `On Woodcutting Tool Visual Changed`. `CurrentWorkTree` now has an OnRep cosmetic refresh so replication order cannot leave a late client with a stale axe.
- World save remains v8. The confirmed v2.16.9 build-aware Tree suppression, v2.16.8 native Dynamic Recast navigation, 2x2+ home validation, villager recruitment/roaming/woodcutting/stockpile loop and protected v2.15.53 Stair/Wall-family topology remain unchanged.

## v2.16.9-alpha — Build-Aware Tree Replacement & Respawn Suppression

- Fixed Foundation placement being blocked or mis-aimed by `AARPGTree` trunk/stump collision. Foundation placement traces now retry past only encountered ARPG Tree actors, and Foundation logical occupancy ignores Tree actors while every non-tree world/build blocker keeps the existing strict validation. This applies to both preview targeting and authoritative support validation, so trees cannot steal a terrain/grid snap.
- Added authoritative build-aware tree suppression. Every `AARPGTree` can expose `Suppress Respawn While Built Over`, `Building Respawn Block Radius` and `Building Respawn Recheck Seconds`; defaults are enabled and require no Foundation Data Asset migration. Build occupancy is tested against each piece's logical `PlacementBounds` with rotation/scale respected rather than decorative collision or canopy overlap.
- When a build piece occupies a tree regeneration location, the Tree and Stump are both hidden and non-colliding. A standing tree replaced directly by construction is removed as environment replacement—not harvested—so it grants no wood, XP, fell VFX or fake settlement stockpile transfer. Existing settlement workers abort that work target cleanly.
- Tree regeneration now preserves its normal eligibility time while blocked. A felled/stump tree cannot `ForceRespawn` through a building; multiple overlapping buildings are tracked independently, and removing only one blocker does not release the tree. Once the last blocker clears, the normal remaining timer resumes, or the resource returns promptly if its natural respawn time already elapsed. Suppressed trees self-heal blocker state on a bounded timer only while suppressed.
- Runtime build placement/load/removal notifies nearby ARPG Trees immediately, while the Tree can also `Refresh Building Respawn Suppression` for custom runtime world changes. The derived suppression flag is replicated and exposed through `Is Respawn Suppressed By Building` plus a Blueprint event.
- Save schema remains world v8: tree suppression is intentionally reconstructed from the persisted build pieces on load, preventing stale duplicated save state. Native Dynamic Recast stairs, the confirmed 2x2+ housing validator, resident recruitment/roaming/woodcutting/stockpile loop and protected v2.15.53 Stair/Wall-family placement topology remain unchanged.

## v2.16.8-alpha — Native Dynamic Recast Stair Navigation Cleanup

- Removed the automatic runtime `NavLinkProxy` layer introduced in v2.16.6/v2.16.7. Real PIE testing with the project's Supported Agent configured correctly showed the Wood Stair rasterizing as a continuous walkable Recast surface; an overlapping off-mesh shortcut could then compete with the native path and make residents repeatedly oscillate around the Stair/door threshold instead of roaming or working.
- Completed Stairs now rely exclusively on their real Dynamic Recast polygons, exactly like normal walkable world geometry. No `ANavLinkProxy`, Simple Nav Link, endpoint projection, retry timer, custom-link actor or Stair-link Data Asset tuning is created at runtime.
- Preserved the useful v2.16.7 runtime navigation lifecycle: structural build pieces still re-register their active mesh with NavigationSystem, update octree bounds and dirty only a bounded local Dynamic Recast region on initialization, construction completion, restore and removal. Stairs receive slightly enlarged local dirty padding so treads/landings/door thresholds regenerate together without a full-world `Build()`.
- Kept `Refresh Stair Navigation Bridge` and `Has Active Stair Navigation Bridge` only as deprecated Blueprint compatibility nodes so existing Blueprint assets do not hard-break. The refresh node now delegates to `Refresh Runtime Navigation`; the query always returns `false`.
- Removed the obsolete Stair NavLink authoring fields from `ARPGBuildPieceDefinition`. Correct navigation is now controlled by the project's Recast **Supported Agent** settings and the actual Stair collision/rasterization, not framework-authored off-mesh links.
- Added regression coverage that explicitly forbids automatic Stair NavLink machinery while protecting local Dynamic Recast invalidation. The confirmed v2.16.3 2x2+ housing validator, v2.16.4 resident registry/work routing, v2.16.5 interior-story spawn protection, world save v8 and protected v2.15.53 Stair/Wall-family placement topology remain unchanged.

## v2.16.7-alpha — Dynamic Runtime Build Navigation & Stair Landing Link Root Fix

- Fixed the remaining real PIE navigation lifecycle defect where `RecastNavMesh-Default (Dynamic)` still displayed `NAVMESH NEEDS TO BE REBUILT` after player construction and required a manual editor **Build Navigation** before runtime-built Foundation/Floor/Stair geometry became current. Completed/removed structural build pieces now explicitly update their active mesh and actor bounds in the Navigation Octree and dirty only the affected local Recast region with `ENavigationDirtyFlag::All`; no full-world synchronous navigation build is forced per piece.
- Construction initialization, completion, save restoration and removal now notify NavigationSystem at the state transition where collision/geometry actually changes. Nearby completed Stairs are rescheduled after structural topology changes so a Stair placed before its landing Floor/Foundation can attach to the final generated polygons.
- Rebuilt Stair landing endpoint resolution around the canonical structural story contract rather than raw rendered height. LOW/HIGH navigation queries use the Stair's logical `+X = uphill`, `±SnapSize/2` anchors and `StandardWallHeight`; visual rails/stringers or a 278 cm Stair art rise can no longer move the HIGH navigation landing off the 300 cm story plane.
- Endpoint placement now computes actual transformed Stair art overhang and enforces at least 45 cm of clear landing beyond that visible art even when an older Data Asset still carries the original 35 cm authored inset. Each projection attempt is rejected if Recast snaps backward onto the Stair island or to the wrong vertical story, and raw/unprojected endpoints are never published.
- Added bounded asynchronous link retries while Dynamic Recast finishes dirty-tile generation, explicit Simple Nav Link snap radius/height, local dirtying when links are created/removed, and a distinct-story sanity check. A `Custom NavLinks` count can no longer be treated as success unless both landing polygons actually resolved.
- Added Blueprint-callable `Refresh Runtime Navigation` for diagnostics/custom runtime structural changes plus regression coverage for dirty-area generation and the strengthened Stair landing solver. The v2.16.3 2x2+ home validator, v2.16.4 resident registry/work routing, v2.16.5 interior-story spawn protection, world save v8 and protected v2.15.53 Stair/Wall-family topology remain unchanged.

## v2.16.6-alpha — Runtime-Built Stair Navigation Bridge Root Fix

- Fixed the real PIE navigation topology defect where player-built Foundation/Floor interiors and surrounding terrain could each generate valid green Recast NavMesh but remain disconnected because the authored Stair mesh itself did not rasterize as a continuous walkable corridor. Settlement residents could therefore spawn/roam on the correct interior story yet still be unable to leave the house.
- Added a generic completed-Stair navigation bridge to `AARPGBuildPieceActor`. Every eligible `Piece Kind = Stair` publishes a transient **bidirectional Simple Nav Link** between its logical LOW/HIGH landings, so the solution applies to all framework AI/path-following rather than special-casing settlement villagers.
- Navigation endpoints use the same protected Stair convention as structural snapping: actor-local `+X = uphill`, structural endpoints are `±SnapSize/2` around the transformed visual-bounds centre, and an exposed landing inset pushes each endpoint beyond intentional Stair art overhang onto the adjacent walkable terrain/Foundation/Floor. The current Wood Stair default uses 35 cm, placing the endpoint 18 cm beyond its 17 cm visual overhang.
- Each endpoint is projected independently to existing NavMesh with exposed XY/Z tolerances. Vertical projection is capped below half a `StandardWallHeight`, preventing a roof one full story above from being substituted for the intended landing.
- Added Stair Data Asset controls: `Enable Automatic Stair Navigation Bridge`, `Stair Navigation Landing Inset`, `Stair Navigation Projection Radius`, and `Stair Navigation Projection Half Height`, plus Blueprint calls `Refresh Stair Navigation Bridge` and `Has Active Stair Navigation Bridge`.
- The bridge is authority-owned, transient, non-replicated, created only after construction completes, refreshed after initialization/load restoration, updates the navigation octree, and is destroyed with/demolished alongside the Stair. It adds no save-schema data.
- Added dedicated runtime Stair navigation-bridge regression coverage. The confirmed v2.16.3 2x2+ home validator, v2.16.4 resident identity/work routing, v2.16.5 interior-story spawn protection and protected v2.15.53 Stair/Wall-family placement topology remain unchanged.

## v2.16.5-alpha — Settlement Interior Story Spawn & Home Anchor Root Fix

- Fixed the real PIE defect where a recruited villager could spawn on top of the house roof. The old recruitment query started near the Bed with a full-story vertical NavMesh extent and used `AdjustIfPossibleButAlwaysSpawn`, allowing UE collision resolution to choose/push the pawn onto the overhead roof/ceiling surface.
- Added Blueprint-callable `Resolve Resident Home Anchor` on `AARPGSettlementHubActor`. It derives the walkable floor plane from the already validated home (`HomeCenter.Z - HomeExtent.Z`), searches only a tight same-story NavMesh band, and requires the projected point to remain inside the validated home footprint. Roof/ceiling NavMesh islands are explicitly rejected.
- Recruitment now uses `AdjustIfPossibleButDontSpawnIfColliding`, retries up to ten navigable same-story points, validates any collision adjustment remains on the interior floor story, and defers recruitment rather than spawning a resident on another story.
- Resident roaming, Return Home and AI combat-leash home locations now share the same semantic interior anchor instead of the Bed actor pivot, preventing ambient wander queries from starting on the wrong vertical NavMesh island.
- Added runtime recovery for legacy/previous-session residents saved horizontally over their assigned home but at roof/upper-story height; those invalid positions are returned to the validated interior anchor without moving legitimate residents elsewhere in the settlement.
- Save restoration no longer force-spawns settlement residents through blocking geometry. If the saved transform is blocked, it retries from the Bed's validated interior anchor. World save schema remains v8.
- Added dedicated interior-story spawn/home-anchor regression coverage. The confirmed v2.16.3 2x2+ home validator, v2.16.4 resident identity/work routing, v2.16.2 UE5.8 compile fixes and protected building topology remain unchanged.

## v2.16.4-alpha — Settlement Resident Registry & Autonomous Locomotion Root Fix

- Fixed the real PIE duplicate-resident defect where one recruited villager could appear twice in the Settlement panel and inflate `Residents` / `Woodcutters` counts. Recruitment now pre-registers the pawn before initialization callbacks can fire, and the Hub registry is idempotent by both live actor identity and persistent `ResidentId`.
- Added registry self-healing for already-duplicated runtime arrays plus unique public resident views, summary counts, worker-cap accounting and teardown. One actor can no longer create duplicate rows or count as multiple workers.
- Hardened settlement villager locomotion. Resident work now recovers a missing AI controller with `SpawnDefaultController`, requires a live Navigation System, projects tree targets to a nearby navigable approach point, and only publishes `Going To Work` after the navigation request is accepted.
- Added work-movement proof: an accepted path must produce real actor translation. Accepted-but-motionless/unreachable routes are stopped, the tree reservation is released, and the resident returns to normal roaming instead of remaining permanently stuck in `Going To Work`.
- Tree chopping still uses the existing authoritative `ARPGTree::ApplyChop` durability/reward path and Hub inventory deposit; no fake resource timers or parallel gathering implementation were introduced.
- Added dedicated duplicate-registry + autonomous-locomotion regression coverage. Existing v2.16.3 structural-plane housing, v2.16.2 UE5.8 compile fixes, world save v8, buildable Lights, Windows/Doors and protected v2.15.53 Stair/Wall-family topology remain unchanged.

## v2.16.3-alpha — Settlement Structural-Plane Home Validation Root Fix

- Fixed the real PIE housing failure where completed modular homes could report `0/N` overhead cover and `Valid Homes 0` even though Floors/Ceilings and perimeter Walls were visibly complete. The original validator compared semantic house slots against raw actor origins; modular art in this framework may use center, bottom, corner or transformed pivots.
- Rebuilt home classification around the building system's actual transformed Static/Skeletal build bounds and canonical structural planes. Foundation cells now use transformed visual centers + finished top surfaces; cover pieces are credited by projected footprint coverage at `FoundationTop + HomeStoryHeight`; Wall/WindowWall/Doorway segments are credited at the real half-grid Foundation boundary with their finished bottom aligned to the Foundation top.
- Added one-piece-per-perimeter-slot accounting so a single Wall-family actor cannot accidentally satisfy multiple required boundary positions. Door validation continues to require a completed Door occupying the Doorway's native hosted socket.
- Improved incomplete-home diagnostics: connected larger Foundation builds now report the closest-to-valid useful rectangle instead of arbitrarily preferring a larger footprint (for example keeping a missing starter-home ceiling diagnostic at `0/4` rather than drifting to `0/8`).
- Changed the framework default minimum valid home from **4x4 Foundations** to **2x2 Foundations** (`Minimum Foundation Width = 2`, `Minimum Foundation Depth = 2`). Existing Settlement Definition assets remain explicitly configurable and keep their serialized values. Blueprint validation count defaults now match 4 Foundations / 4 cover cells / 8 perimeter segments.
- Updated native Settlement/Bed help text and current documentation for the configurable **2x2+ default** housing contract.
- Added dedicated pivot/structural-plane home-validation regression coverage while retaining all existing Settlement, save, UI compile-compatibility, Light, Window, Door and protected v2.15.53 Stair/Wall-family regression locks.
- World save schema remains **v8** and Character save remains **v5**; no save migration is required.

## v2.16.2-alpha — UE5.8 Settlement UI Incomplete-Type Compile Root Fix

- Fixed the remaining UE5.8/MSVC `C2664` errors in the Settlement UI state accessors. `UARPGSettlementPanelWidget` and `UARPGBedPanelWidget` are intentionally forward-declared in the public component header, so even `TObjectPtr::Get()` cannot be converted to `const UObject*` by `IsValid()` while those widget types are incomplete.
- Moved `IsSettlementPanelOpen()` and `IsBedPanelOpen()` out of `ARPGSettlementUIComponent.h` and implemented them in `ARPGSettlementUIComponent.cpp`, after `UI/ARPGSettlementWidgets.h` supplies the complete widget class definitions. This keeps the public Blueprint API unchanged while resolving the type relationship correctly instead of casting around it.
- Retained the v2.16.1 `UWidget::Slot` name-hiding fix. No settlement gameplay, housing, AI, faction, save, build placement, Stair/Wall-family, Window, Door or Light behavior is changed by this compile-only hotfix.
- Strengthened compile regression/validator coverage so the panel-state accessors must remain out-of-line and the `IsValid()` calls must stay in the complete-type `.cpp` context.
- Updated README, Quick Start, Settlement, Validation, CHANGELOG, source validator and source manifest for v2.16.2-alpha.

## v2.16.1-alpha — UE5.8 Settlement UI Compile Compatibility Fix

- Fixed the UE5.8/MSVC `C2664` errors in `UARPGSettlementUIComponent` caused by calling `IsValid()` directly on forward-declared `TObjectPtr` widget members. Blueprint-pure panel state accessors now pass the explicit raw pointer via `.Get()`, and the component's internal panel close-state checks use the same unambiguous path.
- Fixed the UE5.8 warnings-as-errors `C4458` failure in the native Settlement HUD layout where a local `Slot` variable hid `UWidget::Slot`. Native settlement canvas-slot locals now use explicit descriptive names (`CenterPanelSlot` / `HUDPanelSlot`).
- Added compile-compat regression checks for both exact failures reported by the real UE5.8 UnrealBuildTool/MSVC build log so these source patterns cannot silently return.
- This is a compile-compatibility hotfix only: settlement Hub/home/Bed/villager behavior, world save schema v8, Blueprint APIs, build placement, v2.15.53 Stair/Wall-family topology, v2.15.54 buildable Lights, Window interaction and all existing serialized enums remain unchanged.
- Updated README, Quick Start, Settlement, Validation, CHANGELOG, source validator and source manifest for v2.16.1-alpha.

## v2.16.0-alpha — Settlement Hubs, Validated Homes & Autonomous Villagers

- Added `Bed` and `SettlementHub` as **appended** build-piece kinds after v2.15.54 `Light`, preserving every pre-settlement serialized enum value. Beds/Hubs use a separate native horizontal-surface placement path; the protected v2.15.53 Stair/Wall-family boundary functions remain byte-identical.
- Added `UARPGSettlementDefinition`, a Blueprint-readable Data Asset for Hub radius/HUD radius, deterministic settlement-area overlap prevention, recruitment cadence/cap, resident class/name pool/wander cadence, configurable 4x4+ home rules, woodcutting policy and Hub stockpile capacity.
- Added native `AARPGSettlementHubActor` (derived from `AARPGStorageActor`) as the explicit Palbox-style opt-in boundary. Without a completed Hub + Settlement Definition, Beds/buildings remain ordinary and no settlement recruitment/work simulation runs. Hub placement may use Foundation/Floor and may optionally allow terrain; server authority re-resolves terrain seating. New Hubs can reject overlapping settlement areas, while legacy overlaps resolve Beds deterministically to one primary Hub.
- Added native `AARPGBuildBedActor` with replicated/persistent `Unassigned`, `Player`, and `Villager` roles, persistent Player Bed owner and Villager resident assignment. Bed role mutation is routed through the existing player-owned interaction RPC and normal modification/range rules.
- Added semantic home validation. Default resident housing requires a complete rectangular **4x4+ Foundation** footprint, complete overhead Floor/Ceiling/(optional Roof) cover, complete Wall/WindowWall/Doorway perimeter, at least one Doorway and a completed Door verified in that Doorway's native hosted socket. Validation results/counts/status are Blueprint-exposed.
- Added native `AARPGSettlementVillagerCharacter` + `UARPGSettlementResidentComponent`. Residents are real framework AI characters, inherit Hub account/character/faction ownership, keep persistent resident IDs/Bed links, reuse the existing AI wander/combat stack, and choose settlement activities on timers rather than permanent Tick.
- Added autonomous woodcutting integration with existing `AARPGTree`: configurable worker exemption from player axe/skill requirements, tree reservation between residents, AI pathing, optional replicated chop montage, normal authoritative `ApplyChop` durability/reward path, and optional transfer of tree Wood/bonus rewards into the Hub stockpile.
- Added native/reskinnable `UARPGSettlementUIComponent`, proximity `UARPGSettlementHUDWidget`, full `UARPGSettlementPanelWidget`, `UARPGSettlementResidentRowWidget`, and `UARPGBedPanelWidget`. The HUD auto-opens/closes by Hub distance; Hub/Bed interaction uses the existing Interact path; Widget Classes, refresh events and core settlement calls are Blueprint-exposed. The Settlement panel can hand off to the Hub's existing Storage UI through a Stockpile button.
- Advanced world save schema to **v8** while character save remains v5. Bed roles/owners/assignments and persistent resident Hub/Bed/class/transform/name/health/ownership state now round-trip; Hub inventory uses existing container persistence. v6 Window and v7 Light migration paths remain intact.
- Added dedicated settlement regression coverage plus source-validator checks for enum stability, explicit Hub gating, 4x4 housing, Door occupancy, deterministic Hub ownership, faction inheritance, autonomous tree work, UI/reskin APIs, v8 persistence and protected Stair/Wall-function hashes.
- Added `Docs/SETTLEMENTS.md` and updated README, Building/Crafting, Quick Start, Feature Matrix, Network/Authority, Woodcutting and Validation documentation.

## v2.15.54-alpha — Interactive Buildable Lighting & Surface Placement

- Added a new additive `Piece Kind = Light`, appended **after `Custom`** so every pre-v2.15.54 serialized build-piece enum value remains unchanged. Existing Foundation/Wall/Window/Door/Floor/Stair definitions require no migration.
- Added native `ARPGBuildLightActor` with replicated/persistent **On/Off** state, the existing player-owned `Interact Built Structure` button path, faction/use validation, optional On/Off sounds and smooth intensity/emissive fading. Toggling consumes **no fuel or wood**; only the normal one-time Build Cost applies when the fixture is constructed.
- Added two data-driven placement contracts without modifying the structural snap graph:
  - **Ground / Foundation / Floor** — world-upright stick torches and freestanding lamps seat their transformed visible bottom on terrain or the finished top surface of completed Foundations/Floors.
  - **Built Wall Surface** — wall torches/lanterns seat their transformed visible back plane on the actually aimed face of a completed `Wall`, `WindowWall` or `Doorway`; actor-local `+Y` is the fixture's outward direction and either wall face is supported.
- Client preview uses the actual traced surface point/normal. Authority independently re-resolves the submitted transform against completed compatible hosts before placement, preserving range/resources/faction/ownership/territory validation. `Light Surface Offset`, `Placement Offset` and `Mesh Relative Transform` remain data-driven authoring controls.
- Added native selectable **Point Light / Spot Light** components with exposed relative transform, intensity, attenuation radius, color/temperature, shadow toggle, source/soft-source radius and Spot cone angles. Runtime light components become render-inactive at zero fade alpha.
- Added particle presentation modes: **None**, **Niagara Preferred / Cascade Fallback**, **Niagara Only**, **Cascade Only**, and **Niagara + Cascade**. Niagara/Cascade transforms are independently authorable and effects stay active through a fade-out until illumination reaches zero.
- Added optional material-emissive fading through a configurable scalar parameter so fixture glass/flame emissive can fade with the native light intensity.
- Buildable fixture Static/Skeletal mesh collision is intentionally disabled by the native Light actor. Lights are decorative surface occupants and therefore cannot re-open the project-confirmed Stair/Wall/Window blocking regressions or become hidden Pawn/placement blockers. Exact duplicate fixture stacking is still rejected through configurable semantic `Light Minimum Spacing`.
- Because fixture meshes are intentionally non-blocking, interaction and demolition use a bounded view-corridor resolver with line-of-sight validation. Wall-mounted lights aim LOS toward the nearest point of their visible world bounds so a pivot near the host plane does not make the fixture unusable. The server still validates normal interaction distance and access.
- World-save schema advances from **v6 to v7** with `bLightOn`. v6/older worlds remain compatible: Window state continues to use the v6 migration path, while older saved Lights restore their Data Asset `Light Starts On` default.
- Protected the v2.15.53 Stair/Wall-family baseline explicitly. The four core Stair/Wall/hosted-insert boundary functions are byte-identical to v2.15.53 and a new regression test locks those function hashes while testing the additive Light subsystem.
- Added `Tools/test_building_interactive_light_model.py` plus v2.15.54 source-validator coverage for enum stability, horizontal/wall placement, non-blocking fixture policy, Point/Spot lights, Niagara/Cascade modes, fade Tick ownership, authoritative interaction, demolition and save migration.

## v2.15.53-alpha — Four-Cardinal Stair / Wall-Family Boundary Seam Completion

- Fixed the final project-reported Stair adjacency failure after v2.15.52: three Stair rotations were accepted, but one rotation still returned **Blocked by another object** against the same authored Wall-family perimeter relationship. The remaining case was not another generic collision regression; it exposed that the unified classifier still modeled only **parallel side seams**.
- Root cause: in the remaining cardinal orientation, the Wall-family module occupies the Stair cell's exact LOW/HIGH boundary (`local X = +/- SnapSize/2`) with its run axis perpendicular to the Stair. v2.15.52 rejected that geometry before checking boundary ownership, so the overlap fell through to generic placement collision.
- Replaced the side-only contract with one private `ARPGIsStairWallFamilyBoundarySeam()` classifier. Parallel Wall/WindowWall/Doorway modules are accepted only on exact `local Y = +/-150 cm` side planes with longitudinal overlap; perpendicular modules are accepted only on exact `local X = +/-150 cm` LOW/HIGH endpoint planes with lateral overlap across the authored Stair width.
- The boundary test is transform-relative and therefore invariant across Stair yaws `0/90/180/270`, 180-degree wall facing reversals and both build orders. Hosted `Window`/`Door` inserts inherit only their verified host's accepted boundary relationship.
- Kept safety narrow: parallel centreline walls, perpendicular walls through the Stair interior, distant modules, unrelated inserts/world geometry and true non-boundary placement conflicts remain blockers. No global build-collision ignore was added.
- No reflected API or Data Asset migration. Keep the confirmed Wood Stair (`Placement Bounds = 167,150,139`, `Snap Size = 300`) and Wood WindowWall (`Window Insert Offset Z = +20`) authoring unchanged. v2.15.43 story geometry and v2.15.48 Window interaction remain protected.
- Updated README, Quick Start, Settlement Building guide, Feature Matrix, Validation, source validator and source manifest; added a dedicated four-cardinal world-space boundary regression covering the exact previously missing rotation.

## v2.15.52-alpha — Unified Bidirectional Stair / Wall-Family Seam Root Fix

- Fixed the systemic Stair adjacency regression demonstrated in fresh PIE after v2.15.51: **Stair -> plain Wall**, **Stair -> bare WindowWall**, **Wall-family -> existing Stair**, and **Window -> WindowWall beside an existing Stair** could all return `Blocked by another object`. The project screenshots prove this was no longer a Window-collider-only issue.
- Root cause: Stair-side coexistence had diverged into separate forward/reverse classifiers. The incoming-Stair helper was additionally gated to flat `Foundation` / `Floor` / `Ceiling` snap targets, so a Stair chained from another Stair could not inherit the same legal Wall-family side seam and fell through to generic collision. Reverse Wall placement used a different geometry path, and incoming hosted inserts did not inherit an already-legal Stair/host seam.
- Added one authoritative `ARPGIsStairWallFamilySideSeam()` predicate used by both build orders. It validates the Wall-family actor snap origin on the Stair's exact `+/- SnapSize/2` side plane, requires parallel run axes, and uses the Stair's authored `PlacementBounds.X` longitudinal half-run so the current Wood Stair's intentional `167 cm` half-run / `17 cm` endpoint overhang remains legal on the 300 cm structural lattice.
- Incoming Stair placement now evaluates Wall/WindowWall/Doorway side seams **before** the flat-support-only horizontal-neighbour gate and supports both flat landing hosts and authoritative Stair-to-Stair chain hosts. The final Stair transform must still match a native host candidate.
- Replaced the reverse Stair-first Wall-family approximation with a thin wrapper over the same shared predicate, eliminating build-order geometry drift.
- Extended hosted insert symmetry in both directions: a verified `WindowWall -> Window` or `Doorway -> Door` insert cannot independently veto a legal Stair-side host seam, including when the insert is installed after the Stair already exists. Duplicate inserts, unrelated inserts, perpendicular/end walls, centreline walls, distant parallel modules and real Stair travel-volume conflicts remain blocked.
- No reflected Data Asset fields or existing Editor setup changed. Keep the confirmed Wood Stair authoring (`Placement Bounds = 167,150,139`, `Snap Size = 300`, `Placement Offset = 0,0,0`) and Wood WindowWall `Window Insert Offset Z = +20`. v2.15.43 story geometry and v2.15.48 confirmed Window interaction remain protected.
- Updated README, Quick Start, Settlement Building guide, Feature Matrix, Validation, settlement/hosted-insert regressions, source validator and source manifest; added a dedicated four-direction bidirectional Stair/Wall-family regression model.

## v2.15.51-alpha — Stair Side Endpoint-Overhang Seam Symmetry Root Fix

- Fixed the broader Stair regression proven by fresh PIE after v2.15.50: a Wood Stair could still report **Blocked by another object** beside ordinary `Wall` pieces as well as bare `WindowWall` pieces. This confirms the hosted Window layer was not the root blocker and that v2.15.50's forward Wall-family longitudinal test still modeled the wrong collision seam.
- Root cause: the current Wood Stair is `334 cm` long inside a `300 cm` structural flight cell, so its authored placement/collision envelope intentionally extends `17 cm` beyond each structural `+/-150 cm` endpoint. A legitimate side-wall continuation centred one grid cell away begins at that same `+/-150 cm` endpoint. v2.15.50 compared the Wall-family segment only against the structural `[-150,+150]` cell, producing **zero** longitudinal overlap even though the actual Stair placement envelope overlaps the adjacent side wall by `17 cm`; the overlap then fell through to the generic blocker.
- `ARPGIsCompatibleStairHostStructuralNeighbor()` now separates topology from collision-seam tolerance: Wall/WindowWall/Doorway must still use the exact structural side plane and a run axis parallel to the Stair, but longitudinal overlap is measured between the Wall-family `SnapSize` segment and the Stair's **transformed authored placement bounds** in the same Stair-cell frame. Mesh-relative/pivot offsets are handled by transforming the Stair bound endpoints through the final snap transform before comparison.
- The corrected forward rule now matches the already-proven reverse `ARPGIsValidExistingStairWallSideSeamNeighbor()` contract, which has long used the Stair's authored longitudinal bounds to tolerate intentional endpoint art overhang. Direct side modules and the immediately adjacent continuation touched by the 17 cm overhang are accepted; perpendicular end walls, centreline walls, distant parallel modules and real travel-volume blockers remain rejected.
- Preserved the confirmed v2.15.43 Stair/story geometry, v2.15.45 Wood Window `Window Insert Offset Z = +20`, v2.15.47 replicated/persistent Window open-close behavior, v2.15.48 WindowWall interaction recovery and v2.15.49 exact hosted-insert seam inheritance. No reflected API, Data Asset migration, placement-bounds change, snap-size change, save-schema change or collision weakening.
- Added focused regression coverage for the exact `334 / 300 / 17 cm` endpoint-overhang case, strengthened the settlement Stair/Wall-family model, and updated README, Quick Start, Settlement Building guide, Feature Matrix, Validation, source validator and source manifest.

## v2.15.50-alpha — WindowWall Stair-Side Structural Span Root Fix

- Fixed Stairs still returning **Blocked by another object** beside a bare `WindowWall` even with no hosted Window present. v2.15.49 correctly handled the hosted insert layer, but project-side PIE proved the underlying forward Stair-versus-Wall-family classifier itself was still too strict.
- Root cause: `ARPGIsCompatibleStairHostStructuralNeighbor()` required an existing Wall-family actor origin to sit almost exactly at the Stair cell's longitudinal centre (`NeighborInCell.X ~= 0`). That treats a 300 cm structural wall segment like a point and is asymmetric with the already-proven reverse Stair-side seam, which correctly accepts a longitudinally offset Wall/WindowWall/Doorway when its structural span overlaps the flight.
- Replaced the exact-X-origin requirement with structural-span classification: the Wall-family run axis must remain parallel to the Stair, its actor snap origin must remain on the exact `+/- SnapSize/2` side plane, and its `SnapSize` segment must overlap the Stair's one-cell structural travel corridor longitudinally.
- Kept safety strict: perpendicular end walls, centreline walls, unrelated parallel segments outside the Stair cell, real travel-cell occupancy, unsnapped Stairs and non-Wall-family blockers remain rejected. The fix applies uniformly to `Wall`, `WindowWall` and `Doorway`; hosted Window/Door inheritance from v2.15.49 then builds on this corrected host classification.
- Preserved the confirmed v2.15.43 Stair geometry/story lattice, v2.15.45 Wood Window host offset, v2.15.47 replicated Window animation/collision/save behavior, v2.15.48 interaction trace fix and v2.15.49 hosted-insert transparency. No reflected API, Data Asset migration, placement-bounds change, save-schema change or collision weakening.
- Added dedicated forward Stair/Wall-family structural-span symmetry regression coverage and updated the settlement model, source validator, README, Quick Start, Settlement Building guide, Feature Matrix, Validation and source manifest.

## v2.15.49-alpha — Hosted Insert / Stair-Side Seam Collision Polish

- Fixed Stairs returning **Blocked by another object** beside an otherwise-valid `WindowWall` after a hosted skeletal Window was added. Root cause: the Stair-side Wall-family classifier correctly accepted the structural `WindowWall`, but the completed `Window` inside that host was still evaluated as an independent build blocker because reverse hosted-insert transparency did not include incoming Stairs.
- Added `ARPGHostedInsertAllowsStairSideNeighbor()`. A hosted `Window` or `Door` is ignored during Stair collision validation only after its exact `WindowWall -> Window` / `Doorway -> Door` native socket is verified and the resolved host independently passes the existing `ARPGIsCompatibleStairHostStructuralNeighbor()` parallel-side topology check.
- Kept the exception deliberately narrow: it requires an authoritative Stair snap to a flat `Foundation` / `Floor` / `Ceiling` host. Perpendicular end walls, centreline hosts, unrelated/misplaced inserts, real travel-cell occupancy and unsnapped/free Stairs remain blockers.
- Applied the fix symmetrically to both Window and Door hosted inserts, preserved insert/host modification-access checks, and retained the semantic world fallback for hollow/complex host collision without blanket-ignoring hosted actors.
- Preserved v2.15.43 Stair geometry/story math, v2.15.45 Wood Window host offset authoring, v2.15.47 replicated Window animation/collision/persistence and v2.15.48 WindowWall-occluded interaction. No reflected API, Data Asset migration, save-schema change, Stair bounds change or Window collision weakening.
- Added dedicated hosted-insert/Stair-side regression coverage and updated the settlement model, source validator, README, Quick Start, Settlement Building guide, Feature Matrix, Validation and source manifest.

## v2.15.48-alpha — WindowWall-Occluded Interaction Trace Root Fix

- Fixed placed Skeletal Windows refusing to open/close from the normal **Interact Built Structure** button even though Doors worked on the same input. Root cause: many WindowWall kits use conservative/simple Visibility collision across the visual aperture, so the player's first view-trace hit was the structural `WindowWall` host rather than the hosted `ARPGBuildWindowActor`; the interaction dispatcher correctly ignored that non-interactable host and never reached the Window.
- Added host-aware Window interaction resolution. When the first structure/Visibility trace hit is specifically a completed `Piece Kind = WindowWall`, the framework now resolves only the completed `Window` occupying that host's exact native `WindowWall -> Window` snap transform and routes the existing player-owned `ToggleBuiltWindow` RPC to it.
- The fix deliberately does **not** multi-trace through arbitrary blockers or unrelated walls. Host identity is verified from the same native snap transforms, placement-clearance tolerance and yaw contract used by semantic hosted-insert occupancy, preserving interaction occlusion/security while supporting conservative WindowWall collision.
- Direct Window hits, Door interaction, storage/crafting interaction, server range/access validation, replicated Window state, open/close animation, collision transitions and world-save v6 persistence are unchanged.
- Preserved the confirmed Wood Window placement baseline (`Placement Bounds 71.5,47.5,47.5`, WindowWall `Window Insert Offset Z = +20` for the current kit) and all v2.15.43-v2.15.47 structural/skeletal behavior. No Data Asset migration or new Blueprint wiring is required.
- Updated README, Quick Start, Settlement Building guide, Feature Matrix, Validation, Window interaction regression coverage, source validator and source manifest.

## v2.15.47-alpha — Replicated Skeletal Window Interaction & Animation

- Added the dedicated Window interaction layer on top of the confirmed v2.15.46 placement baseline. Native `ARPGBuildWindowActor` now owns authoritative replicated `bWindowOpen` state with Blueprint-observable state-change events and authority-only `SetWindowOpen` / `ToggleWindow` APIs.
- Added Data-Asset-driven Window animation authoring for `Piece Kind = Window`: **Window Open Animation**, optional **Window Close Animation**, **Window Animation Play Rate**, optional Open/Close sounds, and **Disable Window Collision When Open**. If Close Animation is empty, the framework reverses the Open Animation; if only Close is supplied, opening reverses that sequence.
- Skeletal animation Tick is enabled only during an active Window transition and disabled again at the final open/closed pose. Interaction spam during an active transition is rejected authoritatively so replicated peers cannot diverge through overlapping single-node playback.
- Separated Window gameplay collision from imported Static/Skeletal collision. The active imported visual is non-colliding on `ARPGBuildWindowActor`; the native bounds-derived `WindowCollision` is authoritative. Opening removes the gameplay blocker immediately, while closing keeps the opening non-blocking until the closed pose is reached, then restores collision.
- Added a lightweight Visibility-only `WindowInteractionCollision` target that remains queryable while the Window is open without blocking Pawn movement. Hosted Door/Window duplicate protection is now explicitly semantic to the occupied Doorway/WindowWall socket, so opening/moving/disabling an insert collider can never make the host accept a second insert. The existing `AARPGCharacter::InteractBuiltStructure()` / `ARPGBuildingUIComponent` view interaction now recognizes Windows and routes through the player-owned `ARPGInteractionComponent` RPC, exactly like Doors. Projects using a non-Visibility structure trace retain a Window-specific Visibility fallback.
- Added world-save schema **v6** with `bWindowOpen` persistence. v5 and older world saves remain compatible and restore Windows closed; Door/construction/container persistence is unchanged.
- Preserved the v2.15.46 skeletal ghost/host-acquisition path, v2.15.45 X/Y/Z hosted Window alignment and `Window Insert Offset`, and the confirmed v2.15.43 Foundation/Wall/WindowWall/Doorway/Floor/Stair structural baseline. No placed-transform migration.
- Updated README, Quick Start, Settlement Building guide, Feature Matrix, Validation, dedicated Window-interaction regression coverage, source validator and source manifest.

## v2.15.46-alpha — Skeletal Window Ghost + Hosted Snap Acquisition Hardening

- Fixed the first Skeletal Window placement ghost rendering with Unreal's grey/default fallback when the existing global Valid/Invalid Preview Materials had only been compiled for Static Mesh use. The native preview actor now checks the assigned preview materials for `MATUSAGE_SkeletalMesh` and, in editor/PIE, requests the required material usage permutation and completes shader preparation before applying the override. Static Mesh preview behavior and the existing global `ARPGBuildingComponent` material fields are unchanged.
- Added an explicit skeletal-preview render-state refresh after material override assignment so a first-use material permutation compile cannot leave the placement ghost visually stale for the rest of the build session. No per-piece preview-material property was added; all pieces continue to use the component-level Valid/Invalid Preview Materials.
- Hardened local `WindowWall -> Window` host acquisition for third-person building. A directly traced compatible `WindowWall` is now treated as the strongest intent signal, while hollow/open frames can also be acquired through a bounded view corridor around the host module when the exact camera-centre ray misses its OBB. The final placement transform still comes exclusively from the host's authoritative native Window socket.
- Made the semantic `WindowWall -> Window` socket intrinsic even when a WindowWall definition has `Generate Standard Snap Points` disabled. This prevents a WindowWall from becoming an invalid Window host merely because its generic structural neighbours are custom-authored. Existing standard-generated Window sockets are not duplicated, and Doorway/Door behavior is untouched.
- Preserved authority safety: the server still reacquires the compatible host from the submitted snapped transform, validates resources/access/range/support/collision, and rejects duplicate/conflicting inserts. The new corridor is local acquisition only and cannot authorize arbitrary placement.
- Preserved the v2.15.45 suspended X/Y/Z Window centering and `Window Insert Offset`, the v2.15.44 Static/Skeletal visual architecture, and the confirmed v2.15.43 Foundation/Wall/WindowWall/Doorway/Floor/Stair story baseline. No save-schema or existing placed-transform migration.
- Updated README, Quick Start, Settlement Building guide, Feature Matrix, Validation, settlement regression model, source validator and source manifest.

## v2.15.45-alpha — Suspended Window Insert Alignment Root Fix

- Fixed `WindowWall -> Window` vertical placement inheriting the floor-standing Door rule. Door inserts intentionally remain bottom-aligned; Window inserts are now treated as suspended openings and center the incoming transformed visible bounds against the WindowWall transformed visible bounds on **X/Y/Z**.
- Preserved the proven `Doorway -> Door` socket exactly: Door X/Y still use pivot-aware visible-bounds centering and Door Z still aligns visible bottom-to-bottom. No Door placement, hinge, collision, replication or persistence behavior was changed.
- Added **Window Insert Offset** to `ARPGBuildPieceDefinition` for `Piece Kind = WindowWall`. The default is `0,0,0`; it is applied in the WindowWall host's logical local axes after automatic 3D centering, allowing intentionally high/low/off-centre openings without editing the reusable Window asset or abusing its generic `Placement Offset`.
- The current Wood dimensions demonstrate the corrected default: a `95 cm`-high Window centered in a `271 cm`-high WindowWall resolves `88 cm` above a shared visible bottom plane (`(271 - 95) / 2 = 88`) instead of incorrectly sitting on the wall bottom.
- Static and Skeletal Window visuals use the same transformed-bounds rule, including `Mesh Relative Transform`; the v2.15.44 native skeletal Window actor/collision path remains unchanged.
- Preserves the **confirmed v2.15.43 Foundation/Wall/WindowWall/Doorway/Floor/Stair multi-storey baseline**, all Window/Door full-view host acquisition, structural occupancy, build-order transparency, save schema and already-placed transforms. Existing placed Windows are not automatically relocated; validate the new socket with freshly placed Windows.
- Updated README, Quick Start, Settlement Building guide, Feature Matrix, Validation, settlement regression model, source validator and source manifest for the separated Door/Window insert contracts.

## v2.15.44-alpha — Native Skeletal Build Visual Support

- Added optional `Build Skeletal Mesh` and `Preview Skeletal Mesh` fields to `ARPGBuildPieceDefinition` while retaining the existing `Build Mesh` / `Preview Mesh` Static Mesh fields and reflected names unchanged. Existing Static Mesh Data Assets require no migration.
- Added a native `USkeletalMeshComponent` path to `ARPGBuildPieceActor`. A valid skeletal build asset is an explicit opt-in and takes final visual/bounds precedence; otherwise the original Static Mesh path is used exactly as before. The skeletal component is Tick-dormant by default so static-pose skeletal building art does not add permanent per-frame animation work.
- Placement ghosts now support Skeletal Mesh assets with their skeletal component Tick disabled. Explicit preview assets are selected first, which also allows a lightweight Static `Preview Mesh` to represent a Skeletal final piece. If no preview asset is authored, the preview falls back to the active final visual.
- Extended pivot-aware visible-bounds resolution to Skeletal Mesh assets in both placement/validation and structural snap generation. Ground anchoring, `Mesh Relative Transform`, hosted `WindowWall -> Window` centering, support checks and collision-envelope math therefore stay in parity across Static and Skeletal pieces.
- Timed construction now operates through the active `UMeshComponent`, preserving construction scale/reveal, collision policy and material progress parameters for either visual type.
- Added native `ARPGBuildWindowActor` selection for `Piece Kind = Window`. Its bounds-driven `WindowCollision` gives skeletal Windows authoritative occupancy/duplicate-insert collision even when the imported Skeletal Mesh has no Physics Asset. Replicated Window open/close animation is deliberately deferred to the dedicated interaction layer instead of guessing project animation assets.
- Updated `ARPGBuildDoorActor` so an optional Skeletal build visual attaches beneath the same existing `DoorPivot` and uses the same transformed visible-bounds hinge/collision geometry; existing Static Door behavior remains unchanged.
- Preserves the **confirmed v2.15.43 Foundation/Wall/WindowWall/Doorway/Floor/Stair multi-storey baseline** without changing structural snap formulas, story planes, Stair anchors, save schema or already-placed transforms.
- Updated README, Quick Start, Settlement Building guide, Feature Matrix, Validation, regression model and source validator for the new skeletal authoring path.

## v2.15.43-alpha — Stair Low-End Story-Surface Anchor Root Fix

- Fixed the remaining cumulative Stair landing defect after v2.15.42. LOW-departure/up-flight Stairs were still high-aligning their `278 cm` rendered top to the next `300 cm` story surface, which placed the Stair LOW end at `+22 cm` above the current Floor (`300 - 278 = 22`). Every new flight therefore visibly sat on top of its landing even though Floor story planes themselves were canonical.
- `Floor/Foundation/Ceiling -> Stair` LOW-departure sockets now align the incoming Stair **LOW visible plane** to the current finished walking surface. HIGH-arrival/down-flight sockets remain unchanged.
- For the current Wood kit, an up-flight from story `Z=300` now spans rendered `Z=300..578`; the next 18 cm Floor slab occupies `Z=582..600`, leaving a 4 cm underside tolerance and the intended final 22 cm rise to the finished upper landing surface instead of turning that 22 cm into an actor offset.
- `Stair -> Floor/Ceiling` landing Z is now derived from the Stair LOW structural story plane plus `StandardWallHeight`, never from the rendered Stair top. Upper Floor finished surfaces therefore stay exactly on `300/600/900...` while 278 cm Stair art remains inside each 300 cm story.
- Direct Stair-to-Stair chain transforms keep their canonical 300 cm XY/Z structural step. No Stair scaling, Placement Offset, custom socket, reflected Blueprint API change, or save-schema migration is required.
- Preserves v2.15.40 player-only relog persistence/combat protection and all v2.15.38+ Landscape, tiled-deck, Wall/Doorway side-seam, and canonical Stair XY behavior. Existing already-placed Stair actors retain their saved transforms and should be rebuilt fresh for acceptance testing.
- **Runtime acceptance:** fresh multi-storey project testing confirmed the final Stair/Floor/Wall stack remains aligned through repeated storeys with the Stair LOW end on each finished Floor surface. v2.15.43 is therefore the confirmed-good Wood Stair / multi-storey building baseline for subsequent building-piece work.
- **Documentation refresh:** README, Settlement Building guide, Quick Start, Feature Matrix and Validation now present one authoritative v2.15.43 story/Stair contract. Superseded v2.15.24-v2.15.42 transform experiments remain in this Changelog as history but are no longer presented as current authoring guidance.

## v2.15.42-alpha — Finished-Surface Story Plane & Stair/Floor Recess Root Fix

- Replaced the temporary upper-slab **bottom/story-plane** convention with one canonical finished-surface lattice: `Foundation/Floor/Ceiling/Roof` **TOP** is the structural story plane. With `Standard Wall Height = 300`, finished walking surfaces stay at `0, 300, 600, 900...` regardless of slab thickness.
- A `300 × 300 × 18` Floor at the first upper story now occupies `Z=282..300`, not `Z=300..318`. The 18 cm slab extends downward into the lower storey instead of adding 18 cm of extra height to every level.
- `Foundation -> Floor/Ceiling/Roof` and `Wall -> Floor/Ceiling/Roof` now align the incoming tile **top** to the canonical story surface. Direct Wall stacking still advances by `StandardWallHeight`, so Floor-first and Wall-stack-first construction share the same `Z=300/600/900...` wall baselines.
- `Floor/Ceiling/Roof -> Wall/WindowWall/Doorway` now starts the incoming Wall-family piece on the horizontal **top** surface. The slab therefore overlaps the final 18 cm of the lower wall instead of the first 18 cm of the upper wall.
- `Stair -> Floor/Ceiling` landing sockets now align the tile **top/walking surface** to the Stair landing instead of aligning the slab bottom. `Floor/Ceiling -> Stair` also uses the host top surface for both HIGH-arrival and LOW-departure story math. The current `334 × 300 × 278` Stair can therefore enter the slab at the upper landing and meet the walkable surface directly.
- For the current kit, the resulting vertical geometry is intentional: `300 - 278 = 22 cm` residual lower riser; the 18 cm Floor no longer becomes a separate top landing step.
- Updated Wall/Floor semantic snap-slot ownership, inverse upper-slab seams, Stair tiled-deck same-story classification and regression models to use the same finished-top story plane.
- Preserves v2.15.40 player-only persistence/relog combat protection and all v2.15.37-v2.15.38 canonical Stair XY/Landscape/tiled-deck topology. No reflected Blueprint API or save-schema migration. Existing placed structures retain their saved transforms and should be rebuilt fresh to test the new story convention.

## v2.15.41-alpha — Canonical Wall Story Lattice Root Fix

- Fixed the remaining architectural source of **new vertical gaps appearing on higher storeys**. Two standard snap paths still derived the next storey from rendered Wall geometry: Wall→Wall vertical stacking used the target Wall mesh top, and Wall→Floor/Ceiling/Roof also aligned to that mesh top. Any art/pivot/trim height difference could therefore become the next structural baseline and accumulate again on every level.
- Wall-family vertical stacking now resolves the next story from `target structural Wall bottom + StandardWallHeight`. With the current 300 cm kit, storey baselines remain exactly `0 / 300 / 600 / 900 ...` regardless of Wall mesh height or build order.
- Wall→Floor/Ceiling/Roof uses the same canonical next-story plane as Wall→Wall, so those two ownership paths cannot advertise different Z transforms for the same structural boundary.
- Added `ARPGGetWallStructuralWorldZRange()` so semantic Wall occupancy uses the authored story bay rather than rendered Wall height. Wall/Wall and Wall/horizontal seam classification therefore agree with native snap generation.
- Updated inverse upper-slab and inserted-horizontal seam validation to compare against the Wall's structural top (`bottom + StandardWallHeight`) instead of the visible mesh top. This prevents collision validation from reintroducing the same art-height dependency after the snap transform has already been corrected.
- Added a ten-storey regression using deliberately non-300-cm Wall art to prove the structural baseline remains exactly 300 cm per level and does not accumulate mesh-height error.
- Preserves the v2.15.40 player-only persistence/relog combat fix and the v2.15.38+ Stair/Landscape/side-seam runtime. No reflected Blueprint API or save-schema migration. Existing placed structures are not silently moved.

## v2.15.40-alpha — Player-Only Character Persistence Scope / Relog Combat Recovery Fix

- Fixed the proven relog root cause behind **enemies becoming untargetable and combat hits being discarded after loading back into the game**. `AARPGAICharacter` inherits `AARPGCharacter`, and therefore inherited the same `UARPGPersistenceComponent` used by the playable character. The component's auto-load path unconditionally read `UARPGAccountSubsystem::GetLastCharacterId()` for any ARPG Character owner, allowing spawned/reloaded AI to adopt the active player's CharacterId and call `LoadCharacter()` on the player's account save slot.
- Because `LoadCharacter()` restores faction, stats, inventory, location, progression and other player state, an enemy that consumed that save could lose its authored enemy faction and become player-aligned/unresolved. Lock-on and `CanDamageActor()` then correctly rejected it from the corrupted runtime state, producing the paired symptom of **no target acquisition + no registered damage**.
- Added a hard player-only persistence boundary in `UARPGPersistenceComponent`: AI owners no longer schedule account auto-load/auto-save timers and `SaveNow()` / `LoadNow()` reject AI owners even if called directly through the inherited component.
- Added a second hard boundary in `UARPGSaveSubsystem::SaveCharacter()` / `LoadCharacter()` so an `AARPGAICharacter` cannot read or write an account character slot even if another system invokes the save subsystem directly.
- `AARPGAICharacter` now disables inherited `bAutoLoadOnBeginPlay`, `bAutoSave`, and `bSaveOnEndPlay` defaults. NPC persistence remains the responsibility of spawner/world systems rather than the player's account-character save path.
- Added regression guards proving account persistence remains enabled for player characters and is rejected for AI characters, including protection against `LastCharacterId` propagation into enemies on relog.
- Preserves v2.15.39 reciprocal hostility/faction restoration hardening as defense-in-depth and leaves the v2.15.38 Stair/building code untouched. No reflected Blueprint API changes or Data Asset migration.

## v2.15.39-alpha — Combat Targeting & Faction Integrity Recovery Fix

- Fixed a shared combat/targeting failure where a character load containing an empty `PrimaryFactionId` could clear an already valid Blueprint-authored or `DefaultPlayerFactionId` runtime faction. Because both lock-on and hit permission depend on faction hostility, the same empty identity could make enemies impossible to target and cause `CanDamageActor()` to discard hits.
- Character loading now applies a saved faction only when the save actually contains one. Empty/legacy faction ids preserve the already-authored runtime faction, or recover the configured `DefaultPlayerFactionId` when no runtime identity exists. Reputation restoration remains unchanged.
- Fixed player-vs-AI hostility asymmetry: `CanDamageActor()` now recognizes hostility owned by the target AI as well as the attacker's AI. An NPC that explicitly considers the player hostile through faction rules, proactive fallback aggression, or retaliation is therefore a legitimate combat target from the player's side too.
- Lock-on hostile filtering now distinguishes **Faction component exists** from **valid faction identity exists**. Missing/empty identities use the combat permission fallback instead of being treated as a resolved neutral relationship, and hostile target-AI intent is recognized.
- Combat, targeting and character source were byte-identical between the pre-Stair baseline and v2.15.38; this release intentionally leaves the v2.15.38 building/Stair code unchanged and hardens the shared faction/hostility contract that can disable both systems together.
- No reflected Blueprint API or build-piece Data Asset migration. Existing valid faction setups keep their authored behavior; friendly fire and neutral-damage profile settings remain authoritative unless the target AI itself explicitly considers the attacker hostile.

## v2.15.38-alpha — Stair Tiled-Deck Overhang Seam Root Fix

- Fixed the regression where a correctly snapped LOW-departure Wood Stair on a multi-tile Foundation/Floor deck could immediately report **Blocked by another object** after v2.15.37 moved Stair actor centres onto the canonical 300 cm structural grid.
- Root cause: the Stair host itself was already ignored correctly, but `ARPGIsCompatibleStairHostStructuralNeighbor()` still returned `false` for **every** horizontal neighbour. The current 334 cm Stair intentionally overhangs its 300 cm structural flight by 17 cm at each end, so an immediately adjacent same-story Foundation/Floor/Ceiling cell can be returned by the Stair profile overlap even though it does not occupy the Stair travel cell.
- Stair horizontal-neighbour validation now classifies the **structural flight cell** first. LOW-departure flights own the active host cell; HIGH-arrival flights own the neighbouring outside cell. A second horizontal module centred in that actual flight cell remains blocked.
- Immediate same-story grid neighbours one 300 cm cell away are accepted only when the broad overlap is the intentional art/rail overhang seam. Different-story tiles, diagonal/non-grid tiles and true flight-cell occupancy are not legalized.
- Preserves v2.15.37 canonical 300 cm Stair XY/Z snapping, v2.15.36 story planes, v2.15.31 Landscape terrain handling, Stair side Wall/Doorway seams, Stair-to-Floor/Ceiling landings, Doors, persistence and reflected APIs unchanged.

## v2.15.37-alpha — Stair Canonical 300 cm XY Landing Grid Root Fix

- Fixed the severe horizontal Stair/Floor/Wall drift visible in multi-storey Stair towers even after v2.15.36 corrected the Z story plane.
- Root cause: Stair XY topology was still derived from the current Wood Stair's raw `334 cm` visual run. LOW/HIGH horizontal-host sockets aligned raw `±167 cm` mesh ends to `±150 cm` grid edges, shifting a LOW-departure Stair actor by 17 cm. Stair-owned Floor landing math then added another raw 317 cm endpoint-to-tile-edge offset, placing the next 300 cm Floor centre at 334 cm instead of 300 cm — a **34 cm XY error per storey**. Direct Stair-to-Stair chaining also advanced by 334 cm.
- Horizontal Foundation/Floor/Ceiling → Stair sockets now use structural Stair anchors at `bounds centre ± SnapSize/2` (`±150 cm` for the current kit). The unchanged 334 cm Stair art overhangs those structural anchors by 17 cm at each end rather than moving the build grid.
- Stair-to-Stair continuation now advances by the average structural SnapSize (`300 cm` for the current kit) and one `StandardWallHeight`, never raw mesh endpoint delta.
- Stair → Floor/Ceiling landing centres now use structural half-grid sums (`150 + 150 = 300 cm`) around the Stair bounds centre. The tile bottom/story plane still uses the v2.15.36 canonical Z contract.
- Preserves the v2.15.31 Landscape fix, segmented Stair collision, paired HIGH/LOW topology, Stair-side Wall/Doorway seams, Doors, Wall/Floor symmetry, persistence and reflected APIs. Existing pieces placed under old transforms are not silently moved; test with fresh Stair/Floor/Wall actors.

## v2.15.36-alpha — Stair Canonical Story-Grid Landing & Wall Gap Root Fix

- Fixed the severe visible gap/misalignment between Wall columns built through normal Foundation/Wall/Floor stacking and Wall columns built from Stair-created upper landings.
- Root cause: v2.15.33-v2.15.35 aligned a Stair-owned Floor's **visible top** directly to the Stair's raw visual endpoint. With the current 278 cm Stair and 18 cm Floor this could put the Floor bottom/story plane at 260 cm instead of the kit's canonical 300 cm wall story, so individually valid Walls were separated by up to 40 cm.
- Horizontal Stair sockets now use one canonical story plane: Foundation **top**, or Floor/Ceiling **bottom**. HIGH-arrival flights land their visual HIGH endpoint on that plane; LOW-departure flights place their visual HIGH endpoint one `StandardWallHeight` above it. The Stair mesh rise no longer defines storey height.
- Direct Stair-to-Stair continuation advances by `StandardWallHeight` (300 cm in the current kit) instead of raw 278 cm endpoint delta, preventing 22 cm drift per chained flight.
- Stair-owned Floor/Ceiling landing sockets now align the incoming tile **bottom/story plane** to the Stair structural landing plane. Historical limitation: v2.15.36 still used the raw 334 cm Stair endpoints for XY and therefore left a 34 cm horizontal drift in chained landing cells; v2.15.37 supersedes that XY contract with structural 300 cm anchors.
- The current dimensions intentionally resolve as `300 - 278 = 22 cm` residual landing riser; with an 18 cm Floor the lower visual seam is only `4 cm`. No Stair scaling, Placement Offset or custom sockets are required.
- Existing placed Stair/Floor actors retain their saved transforms; rebuild pieces created under the old raw-rise landing contract to test the corrected lattice. No reflected Blueprint API or save-schema migration was introduced.

## v2.15.35-alpha — Stair Wall Structural-Anchor Side-Seam Root Fix

- Fixed the remaining `Wall` / `WindowWall` / `Doorway` blocker beneath or beside an already-built upper Stair.
- Root cause: v2.15.34 changed the reverse Stair-side classifier from the Wall actor snap origin to the transformed Wall logical-bounds centre. Wall-family snapping elsewhere in the framework is authored around the actor origin; meshes with `Mesh Relative Transform` or offset pivots can therefore have a visual/logical centre that is not on the +/-150 cm structural edge even though the snapped actor origin is correct.
- `ARPGIsValidExistingStairWallSideSeamNeighbor()` now uses `IncomingFinal.GetLocation()` as the Wall structural anchor and transforms that point into Stair space. This matches Foundation/Floor wall sockets, vertical stacks and standard structural occupancy.
- Longitudinal overlap now uses the incoming piece's `SnapSize / 2` structural span rather than decorative Wall bounds, preserving the useful v2.15.34 shared-landing/offset tolerance without letting mesh art redefine topology.
- Keeps centreline and perpendicular end-wall conflicts blocked and preserves Stair chaining, Stair-to-Floor/Ceiling landings, Landscape terrain handling, Doors, multi-storey Wall/Floor seams and reflected APIs unchanged.

## v2.15.34-alpha — Upper-Stair Wall & Doorway Side-Corridor Seam Fix

- Fixed the remaining Stair-first reverse seam where a valid `Wall`, `WindowWall` or `Doorway` could still report **Blocked by another object** beneath/alongside an already-built upper Stair.
- Root cause: v2.15.33 required the incoming Wall-family actor centre to match one of two exact `-17 cm / +17 cm` logical cell-centre offsets derived from the `334 cm` Stair overhang. Shared landings and Stair-chain ownership can legitimately centre the same grid-side wall between those offsets, so the Stair vetoed an otherwise correct Floor/Foundation snap.
- Replaced exact-centre ownership with semantic side-corridor classification: parallel run axis, logical bounds centre on the Stair's `+/- HalfGrid` side plane, and genuine longitudinal overlap with the flight.
- Full-story Walls/Doorways below an upper flight and at shared landings are now accepted in either build order. Perpendicular end walls across the travel path, centreline walls and unrelated parallel structures remain blocked.
- Preserves v2.15.33 Stair-to-Floor/Ceiling landing sockets, v2.15.32 Stair chaining, v2.15.31 Landscape handling, segmented Stair occupancy, Doors, Walls/Floors, persistence and all reflected APIs unchanged.

## v2.15.33-alpha — Stair Landing Floor & Wall Side-Seam Integration Polish

- Completed the reverse Stair structural graph so an already-built Stair is now a native support for `Floor` / `Ceiling` landing tiles at both HIGH and LOW endpoints. The incoming tile extends away from the flight and aligns its visible top to the Stair endpoint walk plane, leaving the travel cell open instead of laying a full tile across the Stair.
- Landing transforms use transformed visible bounds rather than a hard-coded 300 cm assumption. For the current `334 × 300 × 278` Wood Stair, the tile's near edge lands exactly on the Stair endpoint while its centre remains one half 300 cm tile beyond it, eliminating the 17 cm art-overhang drift.
- Added reverse Stair-first Wall-family seam validation. An existing Stair may intentionally overlap a `Wall`, `WindowWall` or `Doorway` on either exact **parallel side edge** of the logical Stair cell, even when the Wall snapped from the underlying Foundation/Floor instead of the Stair.
- The reverse side classifier derives both possible logical 300 cm cell centres from the Stair LOW/HIGH endpoints (`StairMin.X + HalfGrid` and `StairMax.X - HalfGrid`). This keeps both HIGH-arrival and LOW-departure flight topologies build-order symmetric on the current 334 cm art run.
- Perpendicular end walls across LOW/HIGH travel openings, wrong-axis walls, duplicate flights and unrelated occupancy remain blocked. No global Stair collision bypass was added.
- Retained v2.15.31 Landscape terrain classification and v2.15.32 paired landing/Stair-chain alignment unchanged.
- Private source/docs update only: no reflected Blueprint API, Data Asset migration or save-schema change. Existing placed structures retain their transforms.

## v2.15.32-alpha — Stair Landing Chain & Multi-Storey Alignment Polish

- Added paired Stair landing sockets to every standard Foundation/Floor/Ceiling edge. The existing HIGH-end arrival/down-flight socket is preserved, and a new LOW-end departure/up-flight socket uses the **same edge center and yaw** so consecutive flights share one deterministic structural centerline.
- Added native Stair-to-Stair endpoint chaining. `LOW(incoming) -> HIGH(target)` continues a flight upward and `HIGH(incoming) -> LOW(target)` continues downward with zero relative yaw, allowing Stair chains to be extended directly from either endpoint.
- Made horizontal-host and Stair-host continuation mathematically identical at a shared landing. If a lower Stair already arrives at a Foundation/Floor edge, the host's LOW-departure candidate and the lower Stair's HIGH-end continuation candidate resolve to the same transform; build order cannot introduce XY/yaw drift between storeys.
- Extended Stair capture to compatible Stair targets and added candidate-envelope affinity for the paired up/down horizontal sockets. This lets aiming inside/on the landing favor the rising departure flight while aiming outside/down favors the descending arrival flight without increasing global snap capture distance.
- Updated Stair side-wall topology to detect whether the LOW or HIGH endpoint owns the horizontal host edge. HIGH-arrival flights use the neighbouring outside stairwell cell; LOW-departure flights use the host cell itself. Exact side seams remain valid while end walls, duplicate flights and genuine travel-path conflicts remain blocked.
- Preserved v2.15.31 Landscape terrain classification, v2.15.26 segmented Stair occupancy, v2.15.27 shared broad/final occupancy, authority snap re-resolution, save compatibility and existing Stair Data Asset settings. No reflected Blueprint API or save-schema migration.

## v2.15.31-alpha — Stair Landscape Terrain Classification Root Fix

- Fixed the isolated-Foundation Wood Stair remaining **Blocked by another object** when the only non-build collision around the correct edge-landing socket is the Unreal Landscape. The placement channel is `WorldStatic`, the active Foundation/Floor/Ceiling snap target is already ignored, and Landscape collision components can conservatively report OBB overlap independently of the local height-field surface. Treating those component overlaps as ordinary discrete obstacles made a verified Stair socket stay red on normal terrain.
- Added an explicit `ALandscapeProxy` terrain classification for **verified snapped Stairs only**. Once the Stair transform exactly matches a native Foundation/Floor/Ceiling candidate, Landscape contact cannot independently veto placement. This intentionally allows terrain embedding/blending for Stair approaches and avoids trying to infer precise terrain obstruction from Landscape component broad-phase overlap.
- The exception is deliberately narrow: static-mesh rocks, cliffs, props, buildings and every other non-Landscape `WorldStatic` actor still pass through the strict sampled Stair support/obstruction test and normal blocker path. Build-piece structural occupancy, side-wall/end-wall rules, duplicate structures, ownership checks and authority re-acquisition are unchanged.
- Added the engine `Landscape` module as a private dependency only; no reflected Blueprint API, Data Asset migration or save-schema change. The proven v2.15.25/v2.15.29 edge-landing transform is unchanged.

## v2.15.30-alpha — Stair Continuous World Support Classification Root Fix

- Fixed the persistent case where a correctly snapped edge-landing Wood Stair still reported **Blocked by another object** on terrain. The v2.15.29 support helper rejected a continuous WorldStatic/Landscape actor as soon as that same actor touched any Stair profile slice above slice 0, conflating terrain underneath the descending flight with an obstruction.
- Replaced the blanket slice-index rejection with per-slice support-surface validation. For each Stair profile slice touched by a non-build WorldStatic actor, the framework samples the blocking surface vertically at the slice centre and both lateral positions. The contact is accepted only when the surface stays at or below that slice underside within the authored placement-clearance tolerance.
- World geometry that rises materially into a Stair slice, overlaps only from the side/overhead, or cannot be proven as support beneath the touched slice remains **Blocked**. This preserves rock/cliff/world obstruction checks without moving the Stair or globally ignoring terrain.
- Preserved the v2.15.25 edge-landing socket transform, v2.15.26 segmented Stair occupancy, v2.15.27 shared broad/final occupancy contract, Foundation/Floor/Ceiling host set, four cardinal sockets, side-wall structural seams, authority re-acquisition and existing editor data. No reflected Blueprint API or save-schema change.

## v2.15.29-alpha — Stair Edge-Landing Foot Support Root Fix

- Restored the intended Wood Stair **edge-landing** topology confirmed by the user's accepted snap-position screenshot: the Stair HIGH end/landing terminates on a Foundation/Floor/Ceiling edge and the flight extends outward/down into the neighbouring stairwell space. The temporary v2.15.28 low-end/inward socket was removed as the **only/replacement** standard socket because it displaced the proven ground-access topology. v2.15.32 later adds a LOW-end departure back as a second paired multi-storey socket while preserving this HIGH-arrival socket unchanged.
- Identified the exact persistent blocker path in `EvaluatePlacementInternal()`: after the verified snap target was ignored correctly, any overlapping non-build actor skipped all structural seam logic and fell directly to the generic `Blocked` return. On the isolated Foundation reproduction, the remaining overlap is the terrain/WorldStatic surface touching the Stair LOW foot.
- Added `ARPGIsValidStairLowFootWorldSupport()`, a strict non-build support seam that runs before the generic blocker. It accepts only the LOWEST Stair profile slice touching the supporting world actor, requires a short vertical trace through the low-foot plane to hit that same actor within one profile-slice height, and requires an upward walkable normal.
- A world actor that reaches any higher Stair profile slice remains blocked, so rocks/cliffs/walls intersecting the travel path are not legalized. Build-piece occupancy, horizontal stairwell blockers, end walls and ownership/access checks are unchanged.
- Retained the v2.15.26 segmented Stair profile and v2.15.27 shared broad/final occupancy geometry so Stair collision remains slope-shaped and consistent.
- Added model regressions for the current `334 × 300 × 278` Wood Stair proving the high landing lands at the host edge, the low end extends outward/down, only first-slice world support is tolerated, and the final placement path calls the low-foot support classifier before generic blocking.
- Private runtime/source fix only: no reflected Blueprint API, Data Asset migration or save-schema change. Existing placed Stairs retain their saved transforms; place a fresh Stair for PIE acceptance.

## v2.15.28-alpha — Stair Low-End Edge-Start Socket Root Fix

> Historical note: v2.15.29 supersedes this socket interpretation. PIE confirmed that the desired Stair topology is the v2.15.25 edge-landing/high-end socket; the persistent blocker was a separate non-build WorldStatic low-foot support-classification gap.

- Fixed the actual Stair placement regression introduced in v2.15.25. The original Stair contract aligned the incoming Stair **bottom** to the supporting Foundation/Floor/Ceiling top, but v2.15.25-v2.15.27 changed the edge socket to align the Stair **high/top end** to that same support. A full-story Stair therefore extended downward from the host and could penetrate Landscape/world-static terrain, correctly causing the generic placement validator to return `Blocked by another object`.
- Retained deterministic four-edge Stair snapping while restoring the correct vertical contract: the Stair **low end** is anchored to the selected host perimeter edge, the Stair bottom is aligned to the host top surface, and local `+X` rises inward/upward across the host cell.
- Updated Stair side-wall seam classification to use the verified host cell rather than the obsolete fabricated outward/down stairwell cell from v2.15.25-v2.15.27. End-wall and genuine profile obstruction rules remain strict.
- Preserved the v2.15.26 segmented Stair occupancy profile and v2.15.27 shared broad/final occupancy contract. Those systems now validate the corrected upward Stair transform instead of trying to compensate for a downward socket.
- Added a regression for the current `334 × 300 × 278` Wood Stair proving that, even on a low Foundation top, the low end remains exactly on the host top and the high end is `+278` above it rather than being pushed below terrain.
- Private runtime/source fix only: no reflected Blueprint API, Data Asset migration or save-schema change. Existing placed Stairs retain saved transforms; place a fresh Stair to test the corrected socket.

## v2.15.27-alpha — Stair Query/Final Occupancy Consistency Root Fix

- Fixed the exact source inconsistency that made v2.15.26 appear unchanged in PIE. `ARPGGatherPlacementOverlaps()` correctly queried a Stair as eight low-to-high profile slices, but `ARPGPlacementVolumesOverlapMeaningfully()` later rebuilt the incoming Stair as one full `PlacementBounds` OBB whenever a neighbouring build actor was encountered. The final blocker decision could therefore return the same false `Blocked by another object` result even though the broad Stair query had been fixed.
- Added one shared `ARPGBuildPlacementOccupancyOBBs()` geometry contract. Standard pieces produce one authored OBB; Stairs produce the segmented diagonal profile. Both broad overlap gathering and final build-vs-build occupancy validation now consume those same primitives.
- Final neighbour validation is now profile-to-profile/profile-to-OBB when either side is a Stair, so decorative collision from an adjacent Foundation/Floor can be discovered by physics without the empty Stair AABB being treated as meaningful structural penetration afterward. Genuine geometry occupying the Stair profile remains blocked.
- Added a regression that explicitly inspects the final blocker function and fails if it recreates the incoming Stair with `ARPGMakePlacementOBB()` instead of the shared occupancy-profile helper.
- Private runtime/source change only: no reflected Blueprint API, Data Asset migration or save-schema change. Updated README, Building guide, Quick Start, Feature Matrix and Validation notes accordingly.

## v2.15.26-alpha — Stair Profile Collision Root Fix

- Fixed the remaining Wood Stair placement failure where the Stair snapped to the correct Foundation/Floor/Ceiling edge but still returned `Blocked by another object`. The root cause was the generic placement validator treating the Stair's full rectangular `PlacementBounds` envelope as solid occupancy even though a Stair is a sloped traversal shape. Empty triangular space under/above the Stair could therefore overlap terrain, foundation skirts or decorative modular trim and falsely reject an otherwise correct snap.
- Added a Stair-specific deterministic collision profile composed of eight small oriented box slices following the logical low-to-high run (`local +X = uphill`, `local +Z = up`). The authored Placement Bounds remain the broad envelope, but blocker queries now follow the actual Stair slope instead of filling the entire 334×300×278 box.
- Clearance is applied to each profile slice, including the low foot and high landing, so flush contact at terrain/landing seams does not become a false blocker. Real geometry intersecting the Stair path is still returned by the profile and continues through the existing structural/ownership/blocker rules.
- The change is private runtime logic only: no reflected Blueprint API, Data Asset migration or save-schema change. Preview and server authority share the same collision-profile path through `EvaluatePlacementInternal`.
- Updated Settlement Building docs, Quick Start, Feature Matrix, Validation and regression coverage for the Stair profile contract.

## v2.15.25-alpha — Stair Edge-Landing Snap Transform Root Fix

- Fixed the remaining Wood Stair failure exposed in PIE: v2.15.24 could acquire a Foundation/Floor/Ceiling and turn green, but the native transform still placed the Stair **across the middle/top of the host cell**. The problem was the socket itself, not capture. Standard Stair sockets are now generated on the host's four edges.
- Added a clear logical Stair convention after `Mesh Relative Transform`: local `+X` is uphill/toward the upper landing, local `Y` is width and local `+Z` is up. The transformed high-end center is aligned to the selected host edge, while the Stair visible top is aligned to the host visible top. The Stair therefore extends outward/down into the neighbouring stairwell cell instead of floating above the host.
- Kept Foundation/Floor/Ceiling as the flat native Stair host family and kept Roof excluded. Support/candidate-envelope capture now works with the corrected edge-landing candidates, so aiming at the landing edge or down the Stair run can acquire the intended socket without increasing global snap magnetism.
- Tightened Stair collision semantics for the corrected topology. Same-plane horizontal tiles are no longer auto-whitelisted because a tile in the stairwell cell closes the opening. Only exact Wall/WindowWall/Doorway side seams parallel to the run are accepted; low/high end walls remain blockers.
- Current Wood Stair editor dimensions remain `334 × 300 × 278` with `Placement Bounds = 167,150,139`, `Snap Size = 300`, zero Placement Offset and standard snap points. If imported art rises toward local `-X`, adapt it once with a 180° `Mesh Relative Transform` yaw rather than authoring fake offsets/custom sockets.
- Added regression coverage for edge-socket geometry, high-end/top alignment, support/run-envelope capture, side-wall classification, authority candidate matching and preservation of the v2.15.23/v2.15.24 building baseline. No reflected Blueprint API or save-schema migration is required.

## v2.15.24-alpha — Wood Stair Structural Snap & Host Seam Polish

- Fixed standard `Stair` pieces remaining at `Needs structural support / snap point` even while the player aimed naturally at a compatible Foundation/Floor/Ceiling. The native Stair socket already existed, but capture was still dominated by the generated Stair actor origin; on a 300-unit module that invisible centre can sit beyond the default 140 cm capture distance from the support edge. Stair acquisition now measures against the **support bounds**, the transformed incoming Stair candidate envelope, and the exact candidate origin, while leaving the authored socket transform and global capture radius unchanged.
- Centralized the native Stair support family as `Foundation`, `Floor` and `Ceiling`; generic `Roof -> Stair` remains intentionally excluded because pitched/kit-specific roofs need authored custom sockets rather than the flat standard Stair contract. Authority still re-resolves the same native candidate from the snapped transform, so local preview does not bypass server validation.
- Added verified Stair-host seam validation for real modular rooms. A Stair hosted by one horizontal cell may coexist with that host's immediate same-plane horizontal neighbours and with Wall/WindowWall/Doorway pieces on the two **side edges parallel to the Stair run**. This handles full-width/overhanging Stair art such as the current 334×300 Wood Stair on a 300 grid without shrinking collision globally. Walls across the Stair's +/-X bottom/top travel opening remain normal blockers, so the update does not create a generic clipping bypass.
- Added settlement regression coverage for support-aware Stair capture, the current 334×300×278 Wood Stair dimensions, side-wall compatibility, end-wall rejection, and preservation of all v2.15.23 Foundation/Wall/Doorway/Door/Floor/multi-storey behavior. No reflected Blueprint API or save-schema migration is required.

## v2.15.23-alpha — Multi-Cell Native Wall Facing Root Fix

- Fixed the remaining 180-degree Wall/WindowWall/Doorway inversion when building a real footprint from the inside across two or more adjacent Foundation/Floor cells. The previous post-snap facing pass reconstructed wall art orientation from an assumed actor-local front axis and could still preserve/propagate the wrong side when multiple structural candidates existed.
- Wall-family facing is now **occupied-cell aware**. Horizontal cells that actually own the incoming wall's story baseline claim the wall edge geometrically. A perimeter edge has exactly one occupied side, so it receives a deterministic exterior direction independent of camera position, overlap iteration order, or whether the player is standing inside or outside the structure.
- The framework no longer invents the final art yaw from a `+Y` assumption. Once the owning perimeter cell is known, that cell's own native standard Wall socket supplies the final yaw, restoring the proven v2.15.6 authored-facing contract for the actual modular kit. This keeps Mesh Relative Transform/custom mesh authoring safe.
- A shared edge between two occupied horizontal cells is recognized as an intentional interior partition. Because there is no unique exterior side, upper stories inherit the exact native facing of the direct Wall-family piece below; first-story interior partitions preserve the selected/native snap yaw so manual rotation remains meaningful.
- Preserves the v2.15.20 canonical no-gap story plane and the v2.15.22 hosted Door/Window structural transparency fix. No Data Asset, save-schema, Blueprint API, Placement Offset, collision-clearance, or custom-snap migration is required.
- Documentation refresh: README, Settlement Building guide, Feature Matrix, Quick Start and Validation now describe the complete v2.15.23 Foundation/Wall/Doorway/Door/upper-Floor baseline, hosted-insert build-order rules, canonical story planes, logical occupancy and multi-cell Wall-facing behavior.

## v2.15.22-alpha — Hosted Insert Structural Transparency + Wall Facing Continuity Fix

- Fixed upper `Floor/Ceiling/Roof` placement being rejected as `Blocked by another object` after a Door or Window had already been inserted into the supporting Doorway/WindowWall. Hosted inserts are now recognized in the reverse direction too: when placing a structural neighbour, the framework verifies the insert's exact native host socket and then validates the incoming structure against the host's semantic structural seam. The hosted Door/Window can no longer independently veto a legitimate host seam.
- Host resolution first checks the active snap target, then actors already returned by the placement overlap, and only uses a semantically restricted world fallback when hollow/complex host collision kept the Doorway/WindowWall out of that overlap. Duplicate inserts, unrelated structures and true host conflicts remain blocked. Ownership/modification access is still enforced on both the insert and its resolved host.
- Fixed remaining upper-storey Wall-family front/back instability by making the direct Wall/WindowWall/Doorway immediately below the strongest **visual-facing continuity** authority when it advertises the same native vertical-stack socket. The horizontal Floor/Ceiling/Roof still owns the canonical story-plane Z; only yaw is inherited from the established lower wall. This preserves authored mesh-facing conventions instead of assuming every kit's visible exterior is actor-local `+Y`.
- Horizontal edge-normal facing remains the fallback when no direct Wall-family support exists, so first-storey Foundation edges, cantilevered floor edges and new wall-only branches still receive deterministic facing. No Data Asset, Blueprint API, Placement Offset, collision-clearance or save-schema migration is required.

## v2.15.21-alpha — Canonical Horizontal Edge Wall Facing Fix

- Fixed the remaining 180-degree Wall/WindowWall/Doorway inversion that could appear on upper stories after the v2.15.20 story-plane gap fix. The snap slot and Z baseline were correct, but the final visible front/back could still depend on which competing Wall/Floor candidate supplied the snap or on the inherited yaw of a square horizontal tile.
- Added a final post-snap Wall-family facing normalization against the actual horizontal support edge. The framework reconstructs the support edge from the candidate **position** in the support's local grid, transforms that edge normal into world space, and rotates the Wall's logical local `+Y` front/exterior side to that world outward normal. Facing is therefore invariant under 0/90/180/270-degree Floor/Foundation actor yaw and independent of overlap iteration order.
- If two horizontal cells claim the same edge from opposite sides (an interior partition), facing is intentionally left unchanged instead of arbitrarily flipping to one cell's exterior. Custom/non-standard horizontal snap points are not reinterpreted as standard edge sockets.
- Preserves the v2.15.20 canonical bottom/story-plane Z baseline, v2.15.19 Door insert-host validation, semantic structural occupancy, Door motion/collision/hinges, persistence, and all existing Foundation/Wall/Doorway/Floor snapping behavior. No Data Asset, Blueprint API, save-schema, Placement Offset or custom snap-point migration is required.

## v2.15.20-alpha — Canonical Story Plane Wall Gap Fix

- Fixed the visible horizontal gap between stacked upper-storey Wall/WindowWall/Doorway pieces when a non-zero-thickness Floor/Ceiling/Roof exists at the story seam. The old horizontal-support socket placed the next wall with its visible bottom on the slab **top**, while direct wall stacking placed that same wall on the slab **bottom/story plane**. A 300×300×18 Floor therefore lifted Floor-first walls by 18 cm and could accumulate that thickness again on later storeys.
- Upper horizontal supports now generate Wall-family edge sockets on their **bottom/story plane**. Foundations remain the exception: first-storey walls still sit on the Foundation's visible top. The result is one canonical wall baseline for every upper story, independent of slab thickness or build order.
- This deliberately lets the horizontal slab overlap the lower portion of the upper wall frame by its own thickness. The existing semantic structural-occupancy rules already treat that exact grid-edge/story-plane relationship as a legal seam, so there is no collision bypass and no visible facade gap.
- Floor-first and Wall-stack-first construction now resolve to the same Wall transform and the same exterior-facing owner. Doorways inherit the same correction, so upper-storey Doors remain hosted/aligned without a raised threshold caused by Floor thickness.
- No reflected Blueprint API, Data Asset migration, save-schema change, custom snap point, placement offset, or collision-clearance workaround is required. Existing Foundation/Wall/Doorway/Door snapping, insert-host validation, Door motion/collision/hinges and persistent ownership are preserved.

## v2.15.19-alpha - Upper-Story Wall Facing + Insert Host Occupancy Polish
- Fixed upper-storey Wall front/back inversion when a Floor edge and the Wall directly below advertise the same logical wall column. Horizontal supports now own canonical exterior facing, direct vertical stacks remain second priority, and lateral/corner Wall candidates remain third. Same-slot ownership is structural rather than actor-origin based, so an 18 cm Floor slab no longer prevents the Floor-edge candidate from beating the coincident stack candidate merely because their wall bottoms sit on opposite slab faces.
- Fixed Door/Window inserts being reported as `Blocked by another object` on upper stories when legitimate Floors/Ceilings/Roofs or adjacent Wall framing touch the hosted opening. Insert collision now validates surrounding structural pieces against the designated Doorway/WindowWall host slot instead of treating those legal host seams as independent blockers.
- Preserves duplicate Door/Window blocking, conflicting wall-slot blocking, ownership/access checks, Door replication/collision/opening, and the v2.15.18 semantic structural-slot collision model.

## v2.15.18-alpha — Semantic Structural Slot Collision Root Fix

- Fixed the remaining `Blocked by another object` failures when placing Wall-family pieces on upper Floors beside existing Walls/corners, and when inserting Floors into already-framed multi-storey bays. The previous v2.15.17 OBB root fix correctly separated rendered collision from logical volumes, but standard structural pieces could still fall back to generic box penetration whenever an overlapping neighbour did not advertise the exact same active snap transform. That made a valid edge/corner depend on which actor happened to become `SnapTarget`.
- Added a semantic standard-structure occupancy classifier that runs before generic OBB blocking. Wall/WindowWall/Doorway pieces are represented as logical **grid-edge segments plus vertical story ranges**; horizontal structures are represented by their exact grid cell/edge relationship. Collinear endpoint continuation, perpendicular endpoint-to-endpoint corners, vertical story stacking, and Wall/Floor boundary seams are accepted independently of active snap-target ownership.
- Duplicate Wall slots in the same story, collinear penetration, perpendicular walls crossing through each other's interiors, walls through a horizontal tile interior, and true non-structural logical-volume overlap remain blocked. Front/back wall reversal remains occupancy-equivalent, while the wall run axis still has to match the structural edge.
- Build permission checks remain enforced for every accepted semantic seam neighbour. Native/custom snap transforms are still preferred when available, and non-standard pieces continue through the authored `PlacementBounds` OBB fallback. No reflected Blueprint API or save-schema change is introduced.
- Preserved the confirmed Foundation/Wall/Doorway/Door behavior, persistent ownership, upper-floor capture, inter-story build-order symmetry and Door motion/collision/hinge systems.

## v2.15.17-alpha — Logical Structural Occupancy Collision Root Fix

- Fixed the root cause behind recurring `Blocked by another object` failures in multi-storey Floor/Wall construction. Placement validation previously overlapped the incoming piece's logical `PlacementBounds` against the *neighbour actor's rendered physics collision*. Modular wall/floor art commonly contains posts, braces, lips and beams that intentionally extend across a seam, so harmless decorative contact could repeatedly masquerade as structural occupancy.
- Build-vs-build validation now uses authored logical placement volumes on **both** structures. A full oriented-box SAT check compares the incoming and existing `PlacementBounds`; rendered collision that extends beyond those logical volumes no longer blocks placement by itself. Duplicate/penetrating logical occupancy still blocks, while non-building world collision remains unchanged.
- Fixed the second root cause in inter-story seam classification: collision validation incorrectly required the wall's canonical exterior/front yaw. Front/back reversal is a presentation/facing distinction, not an occupancy distinction. Wall-family seam checks now compare the wall **run axis modulo 180 degrees**, so both equivalent facings of the same structural edge are accepted while a perpendicular wall axis remains invalid.
- Existing native snap transforms remain the first proof of a legitimate neighbour, followed by strict inter-story seam rules, followed by logical-volume conflict classification. Modification-access checks remain enforced for true accepted structural neighbours. No global collision bypass, reflected Blueprint API change or save-schema migration is introduced.
- Preserved v2.15.16 symmetric seam fallback order, v2.15.15 wall-under-upper-slab support, v2.15.14 Floor-after-prestacked-wall build order, v2.15.13 footprint-aware horizontal capture, persistent ownership, Door snapping/motion/hinges and the confirmed Foundation/Wall/Doorway baseline.

## v2.15.16-alpha — Symmetric Inter-Story Seam Fallback Fix

- Fixed a control-flow bug in `ARPGIsValidSnappedBuildNeighbor()` that could still report `Blocked by another object` for legitimate Floor/Wall inter-story seams even after the v2.15.14/v2.15.15 geometry validators were added. The function returned immediately whenever the overlapping neighbour advertised zero native snap candidates, so inverse structural seam validation was unreachable in exactly the cases it was designed to handle.
- Native/custom snap candidates remain the preferred proof of a valid neighbour, but zero candidates no longer end validation. The strict inter-story seam fallbacks now always get a chance to validate `Floor/Ceiling/Roof <-> Wall/WindowWall/Doorway` relationships before the overlap is classified as blocked.
- This makes the relationship genuinely build-order symmetric: insert a horizontal slab after pre-stacked upper walls, or insert a Wall-family bay between existing lower/upper horizontal slabs, without relying on which actor happens to advertise the socket.
- Collision security is unchanged: the seam fallbacks still require exact structural edge, canonical facing and visible story-plane alignment. Walls through tile centers, wrong-facing pieces, unrelated heights, duplicate slabs, arbitrary clipping, protected-build access violations and world blockers remain rejected. No reflected Blueprint API or save-schema migration is required.

## v2.15.15-alpha — Wall Between Upper Floors Structural Seam Fix

- Fixed Wall/WindowWall/Doorway placement reporting `Blocked by another object` when the piece was correctly snapped to an upper Floor/Ceiling but another horizontal slab already existed exactly one storey above. The wall was filling the intended story bay, but the upper slab's native sockets only advertise walls built on top of that slab and therefore could not identify a wall terminating at its underside.
- Added a narrow inverse inter-story seam validator for incoming Wall-family pieces. An already-built upper Floor/Ceiling/Roof is tolerated only when the wall actor lies on one of that slab's exact four structural edges, the wall's canonical facing matches the edge, and the wall's visible top meets the upper slab's visible bottom plane.
- The rule is deliberately not a generic building-collision bypass: a wall through the tile center, wrong-facing wall, wall that extends into/through the slab, unrelated-height slab, arbitrary actor overlap and world geometry remain blocked. Modification-access checks still apply to every accepted structural neighbour.
- Preserved v2.15.14 Floor-after-prestacked-wall build-order independence, v2.15.13 multi-support horizontal placement, v2.15.12 persistent ownership, Door snapping/motion/hinges and all established Foundation/Wall/Doorway behavior. No reflected Blueprint API or save-schema change is required.

## v2.15.14-alpha — Inter-Story Floor/Wall Seam Build-Order Fix

- Fixed the remaining `Blocked by another object` failure when inserting a `Floor` / `Ceiling` / `Roof` after the player had already vertically stacked Wall/WindowWall/Doorway pieces for the next storey. The upper stacked wall begins on the same story plane as the horizontal slab bottom, so the slab deliberately occupies its own thickness through the wall frame; v2.15.13 still misclassified that upper-wall seam as arbitrary clipping.
- Added a dedicated, collision-safe inter-story seam validator for upper horizontal pieces. A Wall-family neighbour is accepted only when its actor origin lies on one of the tile's four exact structural grid edges, its facing matches that edge's canonical outward yaw, and its visible top/bottom aligns with a legitimate slab seam plane.
- Build order is now commutative: `lower wall -> Floor -> upper wall` and `lower wall -> vertically stacked upper wall -> insert Floor` resolve to the same valid structural result.
- The seam rule remains narrow: walls crossing the middle of a tile, wrong-facing walls, unrelated heights, arbitrary clipping, world geometry and non-building blockers remain rejected. Existing modification-access checks are still applied to every accepted neighbour.
- Preserved v2.15.13 footprint-aware capture/cardinal multi-support equivalence, v2.15.12 persistent build ownership, v2.15.9 Door snapping, Door motion/collision/hinges, Foundation/Wall/Doorway snapping and upper-wall facing. No save-schema or Blueprint migration is required.

## v2.15.13-alpha — Multi-Support Upper Floor Snap & Collision Polish

- Fixed `Floor` / `Ceiling` / `Roof` previews intermittently reporting `Needs structural support / snap point` when aiming naturally at the top edge of a supporting Wall/Doorway. Horizontal snap capture previously measured only to the generated tile actor origin; on a 300 cm tile the support edge is about 150 cm from that origin, which could exceed the default 140 cm capture distance.
- Upper-horizontal snap capture now measures against the **incoming candidate's transformed visible bounds**. The player can aim at the actual area the tile will occupy (including the supporting edge) without inflating the global `Snap Capture Distance` or changing Foundation/Wall snap behaviour. Candidate-origin distance remains an exact authority reacquisition path.
- Fixed enclosed/corner rooms reporting `Blocked by another object` when a valid upper Floor touched two or more supporting Wall-family pieces. Different walls can advertise the same physical horizontal slot with inherited yaws of 0/90/180/270 degrees; the old seam validator required quaternion-identical candidates and misclassified perpendicular legitimate supports as blockers.
- Added placement-envelope yaw equivalence for **upper horizontal pieces only**: 180-degree yaw is equivalent for rectangular footprints, while 90-degree yaw is accepted only when `PlacementBounds.X` and `PlacementBounds.Y` are square within the existing collision-clearance tolerance. Rectangular 90-degree clipping and arbitrary unsnapped overlap remain blocked.
- Modification-access checks remain enforced for every accepted additional support neighbour. Existing v2.15.9 Door snapping, v2.15.10 Door motion/collision, v2.15.11 hinge-side authoring, v2.15.12 persistent build ownership, Foundation/Wall/Doorway snapping and upper-wall facing remain unchanged.

## v2.15.12-alpha — Persistent Build Ownership Reload Fix

- Fixed loaded settlement pieces reporting `Building is restricted here` when trying to continue/snap construction after a save/reload in the local no-login/Guest profile. The structural actor itself was restored correctly, but pre-v2.15.12 Guest sessions generated a new `CharacterId` on restart, so the unchanged secure `SnapTarget->CanActorModify()` check correctly treated the loaded structure as somebody else's build.
- Added a persisted `GuestCharacterId` to the local account index. Guest/no-login play now has the same stable character identity continuity that logged-in local accounts already had, so building ownership, character saves and modification permissions survive restarts without disabling access control.
- `ARPGPersistenceComponent` now accepts a valid Guest last-character identity instead of requiring `IsLoggedIn()`, and registers a freshly generated Guest character identity when starting a genuinely new Guest profile.
- Added a one-frame Guest auto-load grace period so world persistence can recover legacy ownership before character auto-load when both systems start on the same PIE/game frame. This removes GameMode/Character BeginPlay ordering as a source of ownership mismatch.
- Added conservative migration for pre-v2.15.12 worlds: only when there is exactly one local player character and exactly one unique saved no-account owner identity does the world loader adopt that legacy `OwnerCharacterId` as the Guest identity. Multi-player or multi-owner worlds are intentionally never guessed or reassigned.
- Preserved the existing server-authoritative building security model: snapping to an existing build still requires modification access; faction/territory checks are unchanged; no global `Restricted` validation was weakened.
- Existing v2.15.9 Doorway insert snapping, v2.15.10 Door motion/collision, v2.15.11 hinge-side authoring, Foundation/Wall/Doorway snapping, wall facing and vertical stacking remain intact.

## v2.15.11-alpha — Data-Driven Door Hinge Side Fix

- Fixed native Doors rotating around the wrong jamb for art kits whose visible hinges are on the opposite side from v2.15.10's hard-coded logical `-X` edge.
- Added `Door Hinge Side` (`Left` / `Right`) directly to `ARPGBuildPieceDefinition` for `Door` pieces. The default is `Left`, matching the current Wood Door art and requiring no custom actor Blueprint.
- Hinge selection is evaluated from the Door's **transformed visible bounds**, after `Mesh Relative Transform`, so imported `16 × 103 × 205` doors re-oriented into logical `103 × 16 × 205` axes still use a stable gameplay hinge edge.
- In the framework wall convention, viewed from logical `+Y`/front: `Left` maps to local `+X`; `Right` maps to local `-X`. This definition is explicit and deterministic rather than inferred from unreliable imported mesh pivots.
- The compensated pivot translation remains unchanged: closed state preserves the exact snapped Door transform, while visible mesh and native `DoorCollision` rotate together around the selected stationary hinge edge.
- v2.15.9 Doorway insert acquisition, v2.15.10 motion Tick ownership, replicated open state, native collision, save/load, access checks, Foundation/Wall/Doorway snapping, wall facing and vertical stacking remain intact.

## v2.15.10-alpha — Replicated Door Motion & Native Slab Collision Fix

- Fixed placed Doors accepting interaction but visually remaining closed. The base build actor was disabling Actor Tick every frame after construction completed, which cancelled the specialised Door actor's short-lived animation Tick immediately after it began. Completed base structures still remain tickless, while an explicitly enabled specialised Door Tick is now allowed to run until its transition finishes.
- Added a native `DoorCollision` box component derived from the transformed visible Door mesh bounds. Closed doors therefore remain solid even when an imported/marketplace Static Mesh has no usable simple collision.
- `DoorCollision` is parented to the same `DoorPivot` as the visible mesh and remains enabled after construction, so its collision follows the actual moving door slab instead of leaving an invisible blocker in the doorway.
- Added automatic visible-bounds hinge correction. The native Door now rotates around the logical local `-X` door edge using a compensated pivot translation, so centered/corner/import pivots can swing like a hinged door without altering the Build Piece Data Asset's authored mesh transform.
- Door collision follows construction collision policy: it is disabled during construction unless `Collision During Construction` is enabled, then becomes `QueryAndPhysics` when complete.
- Replicated `bDoorOpen`, faction/ownership checks, optional auto-close, save/load state, v2.15.9 Doorway insert acquisition, and all confirmed Foundation/Wall/Doorway snapping behavior are preserved. No save-schema migration is required.

## v2.15.9-alpha — Deterministic Doorway Insert Acquisition Fix

- Fixed the actual remaining Doorway → Door `Needs structural support / snap point` failure visible in third-person placement: v2.15.8 truncated semantic opening search at the ordinary placement trace's first hit, so a supporting Foundation/Floor hit could stop the ray before it ever reached the Doorway.
- Door/Window insert acquisition now evaluates the **full camera placement segment** against compatible completed opening envelopes. The generic ground/structure placement hit no longer controls how far semantic insert targeting is allowed to search.
- Added per-opening occlusion validation using center/upper/lower visible-envelope probes. A valid hollow doorway can be acquired through its opening, while an unrelated wall/world blocker still prevents snapping through solid geometry.
- Kept the insert path restricted to Door/Window pieces. Foundation, Wall, Doorway, wall corners, directional facing and vertical wall-stack ownership remain on the confirmed v2.15.6 structural path.
- Retained pivot-aware visible-bounds insert alignment and authoritative exact-socket reacquisition. No reflected Blueprint API or save-schema migration was added.

## v2.15.8-alpha — Collision-Independent Door/Window Opening Targeting Fix

- Fixed the remaining `Needs structural support / snap point` failure where a valid Doorway/WindowWall was never discovered because snap acquisition still began from `OverlapMultiByObjectType`. Imported hollow frames, complex-only collision, or an aim ray passing through the physical opening could therefore bypass v2.15.7's improved bounds scoring entirely.
- Added a Door/Window-only view-directed semantic targeting path. The camera segment is tested against the completed supporting piece's transformed visible bounds, so looking at the frame **or through the actual opening** can lock the insert into place even when the ordinary placement trace hits terrain/world geometry behind it.
- Added first-hit occlusion limiting so semantic opening targeting does not snap through unrelated blocking world geometry.
- Added a collision-independent insert-support fallback to `FindBestSnapTransform`; this is restricted to Door/Window inserts and lets server authority reacquire the exact Doorway/WindowWall from an already-snapped preview transform even when the target mesh cannot participate in overlap queries.
- Insert authority capture now accepts the minimum of visible-envelope distance and exact candidate-transform distance. This preserves free-preview capture while making the client → server revalidation path deterministic.
- Foundation, Wall, Doorway placement, wall continuation/corners, vertical stack facing and every non-insert snap continue using the established fast overlap-based path. No reflected Blueprint API or save-schema migration was added.

## v2.15.7-alpha — Pivot-Aware Door/Window Insert Snap Fix

- Fixed Door previews reporting `Needs structural support / snap point` while aiming at a valid completed Doorway when the Doorway and Door meshes use different pivots or the camera aim point is not close to the insert actor origin.
- Replaced the native Doorway → Door and WindowWall → Window identity-pivot insert with visible-bounds alignment: incoming inserts are centered in the target XY envelope and their visible bottoms are aligned to the target visible bottom.
- Insert snap acquisition now measures the player's desired point against the supporting Doorway/WindowWall visible bounds rather than requiring it to fall within `Snap Capture Distance` of an arbitrary generated actor-pivot location. This makes aiming at the frame/opening reliable while preserving the existing capture rule for every non-insert structural piece.
- Neighbouring compatible insert targets retain deterministic nearest-socket scoring after the semantic target is captured.
- Existing custom snap points remain additive and can still override unusual/off-centre art kits without changing the native modular graph.
- Preserved v2.15.6 vertical stack facing, v2.15.5 directional wall facing, v2.15.4 seam/corner collision validation, v2.15.3 mesh-relative transforms, multiplayer authority and save compatibility. No reflected Blueprint API or save schema migration is required.
- Added settlement regression/model and source-validator coverage for pivot-aware insert alignment and target-bounds capture.

## v2.15.6-alpha — Deterministic Vertical Wall Stack Facing Fix

- Fixed an upper-story Wall-family rotation bug where a neighbouring lateral/corner snap candidate could occupy the same world slot as the direct vertical stack candidate and win through normal distance/view-yaw scoring, causing asymmetric wall art to flip 180 degrees.
- Replaced target-kind-only same-slot ownership with candidate-aware semantic ownership for incoming Wall/WindowWall/Doorway pieces.
- A Wall-family candidate is now recognized as the direct vertical stack owner only when it is above its target at target-local XY zero and preserves zero relative yaw; this makes the supporting wall below authoritative for the new piece's structural column and facing.
- Preserved first-story horizontal-support ownership from v2.15.5, so Foundation/Floor/Ceiling/Roof edges still define canonical outward-facing orientation when they share a slot with a wall corner.
- Preserved straight Wall-family continuation and both ±90° corner variants when they are not competing with a direct vertical support candidate.
- Added regression/model and source-validator coverage for vertical-stack candidate ownership, including guards that a 180°-flipped custom candidate is not misclassified as the native stack owner.
- No reflected Blueprint API, save format or Build Piece Data Asset migration is required; v2.15.5 directional facing, v2.15.4 seam collision, v2.15.3 mesh transforms and multiplayer authority are preserved.

## v2.15.5-alpha — Directional Wall Facing & Snap Ownership Fix

- Fixed the root support-edge yaw-sign error that made wall-family pieces on horizontal supports face correctly on +Y/-Y edges but inside-out on +X/-X edges.
- Formalized the standard wall coordinate convention: actor-local X is wall run and actor-local +Y is the logical front/exterior side after `Mesh Relative Transform`.
- Corrected horizontal support edge yaws to `+Y: 0`, `-Y: 180`, `+X: -90`, `-X: +90`, so the logical +Y side always points toward the selected edge's outward normal.
- Added same-physical-slot semantic snap ownership: for incoming wall-family pieces, Foundation/Floor/Ceiling/Roof edge candidates take precedence over overlapping Wall/WindowWall/Doorway corner candidates only when they resolve to the same location.
- Preserved both ±90° direct Wall → Wall corner variants for intentional wall-only left/right turns; normal distance/yaw snap scoring is unchanged for different slots.
- Preserved v2.15.4 selective seam-overlap validation, v2.15.3 mesh orientation, v2.15.2 pivot-aware placement, multiplayer authority/replication, saves and existing Build Piece Data Assets.
- Added regression coverage for all four support-edge outward normals and same-slot support priority.

## v2.15.4-alpha — Modular Wall Seam & Corner Collision Fix

- Fixed a snapped second wall being rejected as `Blocked by another object` when an already-built neighbouring wall intentionally overlaps a small amount at the modular seam/corner.
- Added four geometric wall-family L-corner positions based on the target and incoming `Snap Size`, with both ±90° facing variants, supporting Foundation-edge corners and direct Wall → Wall corner continuation.
- Hardened placement validation so build-piece overlap is **not** globally ignored: a completed neighbouring build actor is tolerated only when the exact final incoming transform matches one of that neighbour's native/custom snap transforms.
- Retained strict collision rejection for duplicate placement, arbitrary clipping, world geometry and unrelated actors.
- Applied snap-target-style modification-access checks to every tolerated seam neighbour so protected/faction-owned construction cannot be intersected through the seam rule.
- Preserved v2.15.3 data-driven mesh orientation, v2.15.2 pivot-aware placement, authoritative server revalidation and existing Build Piece Data Assets.

## v2.15.3-alpha — Data-Driven Mesh Orientation for Modular Building Kits

- Added exposed `Mesh Relative Transform` to `ARPGBuildPieceDefinition`, defaulting to Identity for backward compatibility.
- Build Piece Data Assets can rotate, offset and scale imported Build/Preview meshes inside the native framework actor without requiring Static Mesh reimport or per-piece Actor Blueprints.
- Added a dedicated Preview scene root so the ghost actor's authoritative placement transform remains separate from art-orientation adaptation.
- Final replicated Build Mesh and local ghost now apply the same mesh-relative transform.
- Pivot-aware ground anchors, validation centers and structural vertical snapping now use the Build Mesh bounds after the data-driven relative transform.
- This directly supports Y-long wall meshes (for example `31 × 302 × 271`) by setting Mesh Relative Transform Z rotation to `+90` or `-90`, while the native wall snap graph continues to use a stable logical local-X run direction.
- Added settlement regression guards for transformed mesh bounds, preview/final transform parity and backward-compatible Identity defaults.

## v2.15.2-alpha — Pivot-Aware Ground Placement & Structural Height Fix

- Fixed bottom-pivot foundations floating above Landscape in both ghost preview and final placement because v2.15.0/2.15.1 blindly added `PlacementBounds.Z` to every ground hit.
- Ground placement now anchors the actual Build Mesh bottom-center to the traced surface from its real Static Mesh local bounds; bottom, center and corner pivots are supported automatically.
- Rotation/grid snapping preserve the visible footprint anchor instead of rotating/snapping around an arbitrary actor pivot.
- Placement collision validation now centers its authored half-extents on the Build Mesh bounds center, while support traces originate from the visible bottom anchor.
- Standard vertical structural snaps now resolve target/incoming mesh min/max bounds for foundation→wall, same-plane pieces, wall stacks, wall→floor/ceiling/roof and story-height attachments.
- Preserved `PlacementOffset` as an explicit local designer override after automatic ground anchoring.
- Added regression coverage for bottom-pivot and center-pivot 150 cm foundations and guards against reintroducing the old `ImpactNormal * PlacementBounds.Z` lift.
- No reflected Blueprint API names changed.
- Refreshed the root README into a concise project overview and moved detailed release history to this changelog; updated feature/documentation navigation for the current crafting/durability/building/UI systems.

## v2.15.1-alpha — UE5.8.1 Settlement Compile Compatibility Fix

- Fixed `ARPGBuildingComponent.cpp` using `FOverlapResult::GetActor()` with only the forward declaration visible by explicitly including UE5.8's `Engine/OverlapResult.h`.
- Fixed native Storage and Production widget layout helpers passing reflected `TObjectPtr<UVerticalBox>` bindings to incompatible `UVerticalBox*&` lambda parameters.
- Fixed C4456 warnings-as-errors in structure item transfer handlers by using distinct Storage/Station panel local names.
- Added settlement compile-regression guards for all three failures while preserving building, snapping, storage, production, persistence and UI behavior from v2.15.0-alpha.

## v2.15.0-alpha — Settlement Building, Structural Snapping, Storage & Production UI

- Promoted the existing low-level Building backend into a complete player build-mode workflow with local ghost preview, live placement result, ready Build Menu and Placement HUD.
- Expanded `ARPGBuildPieceDefinition` with Foundation/Wall/WindowWall/Window/Doorway/Door/Floor/Ceiling/Roof/Stair/Pillar/Storage/Production/Decoration/Custom kinds, direct mesh authoring, categories, bounds, snapping, construction and utility settings.
- Added native actor fallback so ordinary structural pieces require only a Build Piece Data Asset + mesh; Door, Storage and Production automatically select specialised framework actors when Actor Class is empty.
- Added structural snap generation for foundation continuation, wall-family edges/stacks, window/door inserts, floors/ceilings/roofs, roof continuation, stairs and pillars, plus custom accepted-incoming snap transforms for unusual kits.
- Build placement is server-authoritative: server re-resolves snapping and revalidates distance, collision, support/slope, resources, faction and territory before committing materials/spawning. Duplicate Build Cost lines aggregate by stable ItemId.
- Hardened multiplayer placement defaults: authority rejects definitions outside the character Build Catalog unless explicitly opted in, and snapping to an existing runtime build requires modification access by default.
- Added local-only `ARPGBuildPreviewActor` with collisionless Build/Preview mesh, valid/invalid materials and generic PreviewOpacity/PreviewTint/PlacementValid material parameters.
- Added replicated timed construction with synchronized server time, upward mesh reveal, optional ConstructionProgress/BuildProgress material scalar, configurable construction collision and start/complete audio. Build actors tick only while constructing.
- Added replicated `ARPGBuildDoorActor` with authority/faction access, smooth open/close, optional auto-close and persisted open state; doors tick only while moving.
- Added ready `Demolish Built Structure` view interaction with authoritative modification checks and configurable Data Asset demolition refund.
- Added inherited local `BuildingUI` component and reskinnable native Build Menu, Build Piece Row, Placement HUD, Storage Panel, Structure Item Row, Production Panel and Station Recipe Row widget classes.
- Added ready persistent Storage UI and exact-instance transfer RPCs; clicked durable/runtime items preserve the intended InstanceId/condition instead of ambiguously transferring another copy by ItemId. Legacy ItemId transfer APIs remain compatible.
- Added data-driven Production build pieces: assign a Crafting Station Definition directly on the Build Piece asset to create a furnace/workbench with no actor Blueprint.
- Hardened station crafting with strict Required Station Tag enforcement, duplicate ingredient aggregation, rollback-safe input/fuel transactions, whole-output capacity simulation and rollback-safe fuel+output completion.
- Added ready Production UI with Player, Input+Fuel, Recipes and Output columns, queue controls and live progress. Wood-tagged fuel + Ore -> Ingot furnace flow is documented and functional through existing recipe/Inventory systems.
- World save schema advanced to v5 for incomplete-construction remaining time and door state while preserving storage contents, station queues/output and previous durability migration behavior.
- Added `Tools/test_building_settlement_system_model.py` regression coverage and expanded `Docs/BUILDING_CRAFTING.md`.
- Preserved the README splash at the top and all v2.14 Item Management, crafting, durability, repair, Inventory/Quick Access and Item Use behavior.

## v2.14.0-alpha — Player Crafting, Functional Durability, Repair & Tabbed Item Management UI

- Added inherited replicated `UARPGCraftingComponent` to every `AARPGCharacter`; no permanent Tick.
- Reused `UARPGRecipeDefinition` for secure personal crafting and existing station crafting rather than creating a second recipe format.
- Added direct Item Definition asset references to recipe inputs/outputs while preserving legacy ItemId authoring and station compatibility.
- Added server-authoritative timed/batch crafting, owner-only synchronized runtime state, skill checks/XP, output-capacity simulation, per-craft ingredient commits, cancellation refund and EventRouter craft reporting.
- Added functional per-instance durability with unique durable stacks, current/max/percent/broken APIs, broken equip rejection and optional automatic unequip on break.
- Added explicit Combat and Gathering wear contexts so durable armor or unrelated equipment cannot accidentally lose durability from weapon/tool actions.
- Combat durability is charged only after positive applied damage; woodcutting charges the exact axe instance only after `ApplyChop()` succeeds.
- Added generic `Damage Item Durability`, `Repair Item Durability`, `Repair Item To Full` Blueprint APIs for future mining/pickaxe and custom gameplay systems.
- Added Data Asset repair configuration with proportional missing-durability material cost and authoritative repair validation/consumption.
- Rebuilt the ready Inventory panel into an extensible Item Management shell with Inventory + Crafting & Repair tabs.
- Added native `ARPGCraftingPanelWidget`, recipe-row and repair-row widgets, exposed reskin classes, quantity controls, craft/cancel/progress, damaged-item list and repair action.
- Added durability bars/BROKEN state to ready Inventory/Quick Access slot presentation and disabled equipping broken equipment.
- Durable storage/container transfers now preserve exact runtime InstanceId state/condition instead of recreating items at full durability.
- Bumped character/world save schemas for backward-compatible durability migration: legacy durable items initialize at authored max once; new saves persist exact damaged/broken condition.
- Active personal crafting now persists recipe, remaining batch and progress so reload resumes the already-committed current craft without double-consuming ingredients.
- Preserved the README splash, Inventory/Quick Access drag/drop, Item Use, equipment socket exclusivity and previous framework behavior.

## v2.13.3-alpha — Full-Vitals Hard-Gate Fix

- Fixed the actual v2.13.2 loophole where any configured `UseGameplayEffect` or `Item Use Behavior Class` bypassed the full Health/Mana/Stamina protection on an otherwise vital-restoring consumable.
- Built-in `Restore Health` / `Restore Mana` / `Restore Stamina` are now a hard usefulness gate by default on both client preflight and server authority. At least one configured restored vital must be missing before use can proceed.
- Added opt-in Item Definition setting **Allow Other Effects When Restored Vitals Are Full** for intentionally mixed heal+buff/custom items that should remain usable after their restored vitals are full. Default is disabled to prevent accidental potion waste.
- Gameplay Effects now count as applied only when `FActiveGameplayEffectHandle::WasSuccessfullyApplied()` confirms application; merely creating a valid spec no longer consumes an item when GAS rejects the effect.
- Preserved consume-after-success, cooldown, presentation, Inventory UI, Quick Access, custom-only item behavior and all previous equipment/UI systems.

## v2.13.2-alpha — Full-Vitals Consumable Guard Fix
- Pure Health/Mana/Stamina restoration items are locally preflighted before Inventory/Quick Access sends a use RPC.
- Authority rejects `NoUsefulEffect` when all configured vital-restoration targets are already full.
- Consumption, cooldown and presentation happen only after a real vital delta or another independent effect succeeds.
- `Heal()` now returns true only when Health actually increased.
- Inventory Use button disables when the selected pure vital consumable has no useful target.

## v2.13.1-alpha — Equipment Physical-Socket Exclusivity Fix

- Fixed Inventory and Quick Access equipment being able to leave two visible held meshes equipped simultaneously when the Item Definitions used different logical `EquipmentSlot` tags but resolved to the same physical character attachment socket.
- Moved the protection into the central `UARPGEquipmentComponent`, so Inventory equip, Quick Access activation, starting equipment and direct Blueprint `Equip Item` calls all use the same exclusivity rule.
- Cross-slot same-socket replacements are cleared inside the same authority transaction before the Inventory changed broadcast; the replaced slot receives an empty equipment-change notification and the new item becomes the sole socket owner.
- Added authority self-repair for legacy/inconsistent saved equipment state that already contains same-socket duplicates; the active Quick Access item wins the repair tie when available.
- Added a client/local visual projection safety net: legacy or inconsistent state can never render two `AARPGEquipmentVisualActor` instances on the same resolved socket. The active Quick Access item wins if a legacy conflict must be projected.
- Different physical sockets remain independent, preserving weapon + offhand/armor combinations.
- Added `Tools/test_equipment_visual_exclusivity_model.py` regression coverage.
- No new reflected Blueprint properties/functions were added for this fix.

## v2.13.0-alpha — Generic Item Use + Blueprint Item Behaviors

- Added inherited replicated `ARPGItemUseComponent` to every `AARPGCharacter` as the single authority path for usable inventory items.
- Added direct `Use Inventory Item` and `Use First Inventory Item By Id` Character Blueprint helpers; items no longer need a Quick Access assignment to be used.
- Added Blueprintable `ARPGItemUseBehavior` with `Can Use Item`, authoritative `Execute Item Use`, and cosmetic `Play Item Use Presentation` hooks assignable per Item Definition.
- Preserved zero-Blueprint consumables: Health/Mana/Stamina restoration, Gameplay Effect, consume quantity, cooldown, montage and sound are now executed by ItemUse.
- Consumption occurs only after at least one configured/custom effect succeeds; full-vitals, cooldown, insufficient quantity and custom rejection do not consume the item.
- Added owner-only replicated item-type cooldown state and kept Quick Access cooldown projection compatible.
- Quick Access Use now delegates to ItemUse rather than maintaining a second consumable implementation.
- Ready Inventory UI now has context-sensitive `PrimaryActionButton` / `PrimaryActionText` bindings for Use, Equip and Unequip, with right-click using the same action.
- Usable Inventory slots expose cooldown remaining and the ready Use button disables/counts down while the item is cooling down.
- Added `Docs/ITEM_USE.md`, Item Use model coverage and source-validator guards.

# Akuma RPG Framework Changelog

## v2.12.2-alpha — Inventory Viewport Hit-Test Ownership Fix
- Fixed the actual full-panel input blocker: the higher-Z Quick Access `UUserWidget` spans the viewport and was still `Visible`/hit-testable at its top level even though its inner Canvas was pass-through.
- Quick Access now uses `SelfHitTestInvisible` at the top-level widget whenever shown, so its actual hotbar children remain interactive while unused full-screen space no longer blocks the lower-Z Inventory panel.
- Kept Quick Access above Inventory so Inventory -> Quick Access drag/drop remains reachable.
- Preserved the v2.12.1 Inventory preview mouse handling as a secondary ScrollBox-safe interaction guard.
- No gameplay ownership, Inventory, Equipment, Quick Access authority, replication, Stats UI, NPC popup, spawn, movement or footstep logic changed.

## v2.12.1-alpha — Inventory Interaction / Drag-and-Drop Input Fix
- Fixed Inventory item tiles being visible but unable to receive selection/drag interaction when hosted beneath the native Inventory `ScrollBox`.
- Inventory item presses now use `NativeOnPreviewMouseButtonDown` for the Inventory source only, so selection/right-click equip and `DetectDragIfPressed` are armed before the scrolling container can consume the pointer gesture.
- Marked the full-screen dim decoration hit-test invisible and made decorative Inventory panel/grid/slot-size wrappers pass input through to the actual slot widgets.
- Preserved the already-working Quick Access interaction route and all existing authoritative drag/drop, equipment, active-slot clear/unequip and replication behavior.

## v2.12.0-alpha — Complete Ready-to-Use Inventory + Quick Access UI
- Compile-fix repackage: removed unsupported `Units="px"` UPROPERTY metadata from `InventorySlotSize` and `QuickAccessSlotSize` for UE 5.8.1 UnrealHeaderTool compatibility; clamps and runtime sizing are unchanged.
- Added inherited local-only `InventoryUI` component to every `AARPGCharacter`, with automatic local-player Quick Access HUD creation after possession/client restart.
- Added native `ARPGInventoryPanelWidget`, `ARPGQuickAccessBarWidget`, reusable `ARPGInventoryItemSlotWidget`, typed `ARPGInventoryDragDropOperation` and Blueprint-friendly `FARPGInventoryUISlotView`.
- Added ready native Inventory presentation for Item Definition icon/name/description/rarity, exact runtime quantity/GUID, durability, bound/equipped state and Inventory capacity.
- Added ready native Quick Access presentation for slot number, Item Definition/icon, owned quantity, active/equipped state and authoritative consumable cooldown progress.
- Added functional drag/drop: Inventory -> Quick Access assignment, Quick Access -> Quick Access swapping, Quick Access -> Inventory clearing, and drag-away clearing.
- Added `Clear Slot And Unequip Active` to Quick Access as an atomic server-authoritative UI-safe path so clearing the active held weapon/tool cannot leave it equipped accidentally.
- Added right-click Inventory equip/unequip using the existing `ARPGEquipmentComponent` request path; usable items already assigned to Quick Access can be activated through the same context action.
- Added one-call Character Blueprint wrappers: `Open Inventory UI`, `Close Inventory UI`, `Toggle Inventory UI`, `Is Inventory UI Open`.
- Added exposed Inventory/Quick Access/slot Widget Classes, grid columns, slot sizes, Z-order, input/cursor handling, automatic hotbar creation and drag-out unequip policy.
- Kept UI state local/non-replicated and event-driven. A cooldown refresh timer exists only while at least one visible Quick Access slot is cooling down; no permanent Tick was added.
- Preserved all existing Inventory ownership, Equipment authority, Quick Access duplicate repair/save state, JRPG Stats UI, NPC info popup, ground-rise, footsteps and spawn/locomotion behavior.

## v2.11.0-alpha — Complete Ready-to-Use JRPG Stats UI
- Added inherited local-only `StatsUI` component to every `AARPGCharacter`, with no NPC/dedicated-server runtime work unless a locally controlled player explicitly opens the panel.
- Added one-call Character Blueprint wrappers: `Open Stats UI`, `Close Stats UI`, `Toggle Stats UI` and `Is Stats UI Open`.
- Added native `ARPGStatsPanelWidget` default that requires no Widget Blueprint and presents Character Name, effective Level, XP, Health/Mana/Stamina, all six primary stats, per-stat allocations, Attribute Points and all derived JRPG combat stats.
- Added native `+` buttons for Strength/Vitality/Magic/Spirit/Dexterity/Luck using the existing validated `SpendAttributePoints` client/server path.
- Added built-in Close button and optional automatic Game-and-UI input/cursor management for immediate mouse interaction.
- Added exposed `Stats Widget Class` so projects can select a Blueprint subclass while keeping a working native fallback.
- Added `ARPGStatsUISnapshot`, `On ARPG Stats UI Updated`, helper accessors and standard child-name auto binding for custom zero-graph UMG layouts.
- Added a local 0.15 s refresh timer only while the panel is open; no permanent Component/Widget Tick and no new replicated UI state.
- Preserved all existing JRPG Stats authority/replication, NPC info popups, ground-rise spawns, locomotion proof and footsteps.

## v2.10.1-alpha — NPC Info Popup Runtime Visibility Fix
- Fixed a runtime blocker where `UARPGCharacterInfoComponent` permanently set `PrimaryComponentTick.bCanEverTick = false`. Because the component derives from `UWidgetComponent`, this also disabled Unreal's own screen-space projection/rendering TickComponent path, allowing proximity state to become visible internally while nothing could appear on screen.
- CharacterInfo now keeps WidgetComponent ticking available but starts disabled, enables native component ticking only while an overhead popup is visible, and disables it again after hiding. Proximity decisions remain on the existing staggered timer and do not move into Tick.
- Preserves the Widget Class selected on the inherited CharacterInfo component instead of clearing it during lazy/far release. Runtime widget instances can still be retired and recreated without destroying the authored class or screen-space lifecycle.
- Explicitly requests a render update when showing and removes the screen widget immediately when hiding. Added a non-zero fallback draw size while retaining Draw At Desired Size for custom widgets.
- Added static regression guards preventing permanent WidgetComponent tick disablement and far-release WidgetClass destruction.
- No spawn, locomotion, footstep, combat or replication behavior changed.

## v2.10.0-alpha — Automatic Proximity NPC Info Popup

- Compile-fix repackage: renamed the private CharacterInfo visibility parameter from `bVisible` to `bShouldBeVisible` so it does not shadow `USceneComponent::bVisible` under UE5.8/MSVC C4458 warning-as-error builds.

- Added inherited `CharacterInfo` screen-space WidgetComponent to every `AARPGCharacter`.
- Exposed Widget Class selection directly on the inherited component for player/NPC Blueprint archetypes.
- Added automatic replicated-data presentation for RPG character name, effective level, current/max health and health percent.
- Added `ARPGCharacterInfoWidget` native fallback/base widget with standard Name/Level/Health fields plus `On ARPG Character Info Updated` for custom Blueprint presentation.
- Added zero-graph binding for ordinary UserWidgets through exposed child-widget name mapping.
- Added NPC-oriented defaults: AI shown, player-controlled characters hidden, local self hidden.
- Added local proximity show/hide with 1100/1350 cm hysteresis, optional line-of-sight requirement and no replicated UI/proximity state.
- Added capsule-size-aware automatic overhead placement and manual height override.
- Added v2.9 ground-rise integration so popups stay hidden until the NPC has finished emerging.
- Added dead-character hiding and correct effective-level presentation for NPC player-level scaling.
- Added staggered timer sampling, dedicated-server UI suppression, lazy widget creation and optional delayed far-widget release.
- Preserved v2.9 ground-rise movement ownership/collision-safe spawn behavior and v2.8 automatic replicated footsteps.

## v2.9.0-alpha — Polished Replicated Spawner Ground-Rise Entrances

- Added an automatic `Ground Rise Entrance` section to `ARPGAISpawner`, enabled by default for framework NPCs. Every `SpawnOne()` path now applies the same entrance, covering BeginPlay populations, individual/whole-group respawns, distance-stream reloads and Day/Night population swaps.
- Preserved the v2.7.2 collision-safe spawn proof: the actual pawn/capsule remains at the final accepted NavMesh/collision-safe position. Only the skeletal mesh is offset below ground and eased back to its authored relative position.
- Added `UARPGSpawnEntranceComponent` to `AARPGAICharacter`. Its compact replicated state uses synchronized server world time so clients observe the same rise timing without replicating a per-frame transform stream.
- Added capsule-size-aware automatic rise depth, optional extra depth, start delay, duration and ease-out exponent controls. Manual depth remains available for unusual creatures.
- During the entrance, the component stops AIController pathing, disables CharacterMovement, acquires its own Wanderer pause token, pauses active Spline travel, and optionally suspends AI Combat plus ambient Social behaviour. Restoration releases only the state owned by the entrance.
- Optional actor-location locking rejects custom/external translation during the short reveal while leaving facing/rotation available. The location lock never drives the visual rise itself.
- Death during the reveal ends the presentation without restoring walking/AI locomotion into a dead pawn, and ragdolling meshes are not forced back through a relative-transform write.
- Added `Tools/test_spawner_ground_rise_model.py` plus structural validation guards for replication, movement ownership, capsule-safe visual-only presentation and preserved v2.7.2 spawn collision handling.

## v2.8.0-alpha — Automatic Replicated Physical-Surface Footsteps
- Added inherited `UARPGFootstepComponent` to `AARPGCharacter`, automatically covering player characters and all `AARPGAICharacter` NPCs without Blueprint component setup or animation notifies.
- Automatic cadence is driven by real grounded horizontal travel distance with walk/run stride blending, left/right alternation, stop resets and teleport/network-correction rejection.
- Each due step traces from the matching foot socket/bone (`foot_l` / `foot_r` by default) with capsule-bottom left/right fallback when those names do not exist.
- Ground hits request Physical Material data and resolve Unreal Physical Surface types into exposed per-surface randomized sound pools, with Default Sounds fallback, immediate-repeat avoidance, per-surface volume/pitch variation and movement-speed volume scaling.
- Added optional Sound Attenuation and Sound Concurrency authoring directly on the inherited Footsteps component for project-quality 3D/crowd audio control.
- Multiplayer uses server-authoritative automatic cadence plus `NetMulticast, Unreliable` transient cues. The owning player can locally predict its own automatic footstep audio while the authoritative multicast skips that owner, avoiding duplicate playback and reducing audible network latency; NPCs remain server-driven.
- Manual `Trigger Footstep` is also exposed for projects that later choose animation-authored contacts; client requests are locally predicted, server re-traced/rate-limited, and never trusted for surface/sound/location selection.
- Dedicated servers never play local audio. Dead/ragdolled or non-grounded ARPG characters do not generate footsteps.
- Performance remains timer/event based: no permanent Actor/Component Tick was added, characters with no configured footstep audio create no automatic sampling timer, active timers are staggered, and ground traces only occur when accumulated movement says a step is due.
- Added `Docs/FOOTSTEPS.md` and deterministic `Tools/test_footstep_replication_model.py` coverage.

## v2.7.2-alpha — Spawn Collision & Locomotion Proof Fix
- Fixed the deeper rotate-but-will-not-walk failure path that v2.7.1 did not cover. The spawner no longer uses `AdjustIfPossibleButAlwaysSpawn`, which could legally create possessed AI capsules encroaching another pawn/prop and leave them able to rotate/focus while CharacterMovement could not translate.
- Spawned AI now uses `AdjustIfPossibleButDontSpawnIfColliding` with up to 10 collision-safe candidate attempts. Point spawners receive a small NavMesh-projected fallback spread after the preferred origin fails, preventing multi-member groups from stacking capsules at one exact point.
- If no safe position exists, the spawner now refuses that individual spawn and logs a clear warning instead of deliberately creating a stuck pawn.
- `MoveToLocation == RequestSuccessful` is no longer considered proof that Free Roam works. Wanderer now performs a short server-only locomotion proof and requires real 2D actor displacement before `HasEstablishedFreeRoam` becomes true.
- An accepted request that remains rotate-only/stationary or drops out of path following is aborted and retried at a fresh reachable destination. Social AI remains ineligible until real locomotion has been proven, not merely until path following accepts a request.
- The locomotion proof uses one-shot timers only; no permanent Actor/Component Tick was added.
- No reflected Blueprint API changes.

## v2.7.1-alpha — Spawned AI Navigation Readiness Fix
- Fixed an intermittent startup failure where Free-Roam NPCs could rotate/focus but never translate after being spawned.
- Spawner-owned Free Roam/Spline movement now explicitly ensures a default AI controller exists before movement configuration.
- Wanderer no longer assumes a synchronous startup `MoveToLocation` succeeded; it checks UE's `EPathFollowingRequestResult` and only marks Free Roam established after a real `RequestSuccessful`.
- Added a short, server-only navigation readiness retry for temporary missing AIController/NavSystem, failed random NavMesh sampling, failed MoveTo requests, and `AlreadyAtGoal` startup results; retries back off if navigation remains unavailable so broken NavMesh authoring cannot create a high-frequency loop.
- Startup retries are cancelled when Wanderer is disabled/paused and restored safely when temporary movement ownership is released.
- Social AI will not reserve a Free-Roam NPC until that NPC has successfully established locomotion at least once, preventing social focus/rotation from masking a startup navigation failure.
- No reflected Blueprint API changes; the new readiness state/hooks are native-only.

## 2.7.0-alpha — 2026-08-12

- Added `AARPGDynamicStreetLight`, a new Blueprint-derivable world-light actor that consumes the existing `AARPGDayNightCycle` rather than maintaining a second clock.
- Added inherited editable `LampLight` Point Light, `NiagaraEffect` and `CascadeEffect` components. Derived Blueprints can attach meshes and reposition/tune all inherited components normally.
- Automatic control applies the current Day/Night phase immediately at BeginPlay and then reacts to phase/hour events without a permanent Tick. Default schedule is Night + Dawn ON and Day + Dusk OFF, with all four phases independently authorable.
- Added optional explicit Day/Night Cycle override plus first-cycle auto-discovery. Missing cycles fail safe (off by default), retry on a low-frequency timer only while unresolved, and re-resolve if the cycle actor is destroyed.
- Added FX modes for Niagara preferred with Cascade fallback, Niagara-only, Cascade-only, both, or none. Auto activation is disabled on inherited FX components so the lamp state has deterministic ownership.
- `Control All Owned Light Components` optionally toggles every LightComponent on the actor, allowing Blueprint-added Point/Spot/Rect lights to follow the same schedule while retaining an inherited ready-to-use Point Light.
- Added Blueprint manual/force/refresh/query APIs plus a multicast and overridable state-change event for emissive materials, ignition audio, extra FX or project presentation.
- Lamps intentionally do not replicate their own clock or run a permanent Tick; automatic cosmetic state derives from the framework's already-replicated Day/Night cycle, avoiding per-lamp network clock overhead in large towns.
- Existing public headers/reflection schemas are unchanged; this release adds only the new dynamic-street-light public header/class.
- Added `Docs/DYNAMIC_STREET_LIGHTS.md` and deterministic `Tools/test_dynamic_street_light_model.py` coverage.

## 2.6.1-alpha — 2026-08-12

- Fixed intermittent spawned-NPC Free Roam stalls exposed after v2.6 social AI by separating **persistent Wanderer enablement** from **temporary movement ownership**. Social interaction and group-cohesion recovery now use independent native pause reasons instead of toggling `Wanderer.bEnabled`.
- A social reservation can no longer permanently lose the spawner's Free-Roam state when startup/configuration order differs. If social starts before the spawner enables Free Roam, the spawner's enable request is remembered and movement resumes when social releases its own pause. If the spawner disables Free Roam during social, social completion does not resurrect it.
- Free-Roam Wanderers now choose an initial destination immediately when newly enabled while retaining the staggered recurring Think timer for performance.
- Social AI now gives freshly spawned NPCs an initial opportunity grace window based on the existing Opportunity Retry range, allowing spawner movement configuration and a first roam destination before ambient conversations begin.
- Hardened `Stay Together` group recovery: cohesion now owns its own Wanderer pause token, social encounters cannot steal a pawn already recovering cohesion, social encounters are never overwritten by cohesion MoveTo requests, and recovery MoveTo is reissued through the hysteresis band until the true recovery radius is reached.
- Wanderer now ignores its own roam Think while a Spline route is active, preventing a restored timer from competing with route traversal.
- Changing spawner Stay Together or Movement Mode releases stale cohesion ownership before clearing recovery bookkeeping, preventing disabled/stalled Wanderers from being stranded by runtime configuration changes.
- No reflected Blueprint property/function/schema changes were required for this reliability patch; the new Wanderer pause API is native-only coordination state.
- Added deterministic Free-Roam/social/cohesion handoff regression coverage.

## 2.6.0-alpha — 2026-08-12

- Added the inherited `AISocial` component to `AARPGAICharacter`; social AI is **disabled by default** and therefore preserves all existing NPC archetypes unless explicitly enabled.
- Added server-authoritative ambient NPC-to-NPC social encounters: staggered local Pawn scans, throttled random opportunity rolls, atomic partner reservation, approach, natural facing, randomized interaction duration, alternating speech/audio beats, cooldowns and clean return to prior movement.
- Added symmetric faction safety. Same/friendly/neutral/factionless participation can be authored independently, but a hostile relationship in either direction always blocks ambient social interaction.
- Added optional GameplayTag-based archetype matching (`Social Identity`, required partner tags and blocked partner tags), allowing villagers/guards/merchants/wildlife/project-specific social types without hard-coded NPC classes.
- Added a shared-id/local-presentation interaction pool. Both NPCs must contain the same `InteractionId`, while each actor uses its own montage/sound/FText line assets for that id so different skeleton/voice archetypes can interact safely.
- Integrated social reservation with Free-Roam Wanderer and Spline movement. Ambient movement pauses for the encounter and resumes without discarding route/home state; combat suspension remains higher priority.
- Combat target acquisition, incoming combat hits, death and active combat/defensive states interrupt social encounters immediately, preventing ambient behaviour from fighting chase/attack/dodge/stagger/death systems.
- Added replicated runtime social state plus Blueprint force/test/cancel/query APIs and Started/Ended/Line/State events for speech bubbles, subtitles, quest scripting or custom presentation.
- Added dedicated `Docs/AI_SOCIAL_INTERACTIONS.md` and deterministic social interaction model coverage.

## 2.5.4-alpha — 2026-08-11

- Fixed a real UE 5.8.1 Windows packaging compile failure in `ARPGDayNightCycle.cpp`: packaged/non-editor targets do not expose the `ADirectionalLight::GetComponent()` path used by the external sun/moon resolver. External directional lights now resolve their `UDirectionalLightComponent` through the runtime-safe `AActor::FindComponentByClass<UDirectionalLightComponent>()` API.
- Preserved all Day/Night authoring and runtime semantics: built-in rig behavior is unchanged, external Sun/Moon actor assignments still work, and no reflected/public DayNight API changed.
- Removed the two UE 5.8 `C4996` inventory compile warnings seen in the same packaging log by constructing the saved `TSoftObjectPtr<UARPGItemDefinition>` through `FSoftObjectPath` instead of assigning from a const raw definition pointer. No Inventory public API or runtime ownership semantics changed.
- Added a packaging-compatibility regression model and source-validator checks for both issues.
- This patch is based on the user-provided UE 5.8.1 packaging log. Repository validation cannot replace a real local Unreal package/build; the next Windows package attempt is the final verification.

## 2.5.3-alpha — 2026-08-11

- Fixed `Stats -> JRPG Stats -> NPC Player Scaling -> Scale NPC To Player` being greyed out because the entire nested scaling settings struct inherited the outer `Enable JRPG Stat System` edit condition.
- The master `Scale NPC To Player` checkbox is now always editable in the Details panel; only its subordinate reference/matching/stability controls remain conditional on that checkbox.
- For non-player Pawns, explicitly enabling `Scale NPC To Player` now automatically opts the authority into the JRPG stat layer at runtime if it was left disabled, preventing a valid scaling setup from silently doing nothing. Player-controlled Pawns are never auto-converted by the NPC-only scaling switch.
- Designers can still explicitly enable `Enable JRPG Stat System` in the editor to author Growth/Derived settings before PIE. No existing setting was removed or renamed.
- Added regression coverage for the Details-panel metadata trap and runtime auto-enable safety path.

## 2.5.2-alpha — 2026-08-11

- Made the authoritative player/NPC level much easier to author and test: `Progression -> Level -> Base Character Level` is now explicitly named, clamped and documented as the single base-level source of truth.
- Added optional `Progression -> Testing -> Enable Manual Level Override` + `Manual Test Level` for fast PIE validation without grinding XP. The override applies a real authoritative progression level at BeginPlay, so JRPG natural growth, derived stats, max vitals, NPC scaling references, requirements and other level consumers all see the same value.
- Added Blueprint `Set Level` and `Apply Manual Test Level Now` (also exposed as a Details-panel Call In Editor button); both use the existing progression event path, so `OnLevelChanged` drives immediate JRPG stat recalculation instead of silently editing an unrelated debug value.
- Preserved player-scaled NPC semantics: `Base Character Level` remains the NPC's authored identity while `Get Effective Level` remains its player-relative runtime level. Manual test override changes Base Level first, then optional NPC scaling evaluates from that base.
- Manual override is disabled by default and is intentionally not a second saved level system. Normal XP/save progression remains the production source of truth when the override is off.

# 2.5.1-alpha — player-relative NPC level/stat scaling

- Added opt-in `Scale NPC To Player` under `Stats -> JRPG Stats -> NPC Player Scaling`; disabled NPCs retain their authored level/stat behavior exactly.
- Player scaling never overwrites `Progression.Level`. The NPC keeps a permanent authored **Base Level** and receives a replicated runtime **Effective Level** used for JRPG natural growth, all six primary stats, every derived combat stat, max vitals, crit, accuracy/evasion, attack speed and movement speed.
- Scaling preserves creature identity: each NPC continues to use its own Base Primary Stats, Primary Growth Per Level, derived formulas and equipped modifiers. A chicken and a demon scaled to the same player level therefore remain intentionally different creatures.
- Added `Level Offset`, `Level Match Strength`, minimum/maximum scaled level, allow-up/allow-down controls and no-player fallback, supporting soft scaling, elites, fixed difficulty floors and zone brackets.
- Added multiplayer reference policies: Combat Target then Nearest, Nearest, Highest Level, Lowest Level and Average Nearby Player Level. The server scans player controllers only rather than searching every actor.
- `Lock Level While In Combat` snapshots the encounter level so a different nearby player or mid-fight player level-up cannot suddenly inflate/deflate NPC health, damage or movement during an active fight.
- Scaling refresh is timer-driven, server-authoritative, staggered across NPCs, and replicated through a compact runtime state with `On NPC Level Scaling Changed`.
- Attribute Point ownership remains based on the NPC's real authored Progression Level. Temporarily scaling a level-1 creature to effective level 50 cannot generate level-50 Attribute Points.
- `Get Stat Snapshot().Level` now reports Effective Level when scaling is active; `Get Effective Level`, `Get Base Progression Level`, `Get NPC Level Scaling State` and `Refresh NPC Level Scaling Now` support nameplates/debugging/Blueprint systems.
- Runtime scaling state is intentionally not saved: saves retain the authored base progression and normal stat allocation, then resolve the appropriate nearby-player scaling after load.

See `Docs/NPC_PLAYER_LEVEL_SCALING.md`.

# 2.5.0-alpha — classic JRPG stats + attribute points

- Added an opt-in six-primary-stat system to `ARPGStatsComponent`: Strength, Vitality, Magic, Spirit, Dexterity and Luck.
- Added deterministic per-level natural stat growth, configurable 255-style primary caps and derived Max Health/Mana/Stamina growth.
- Added separate derived melee, ranged and magic attack power; physical and magic defense; Accuracy, Physical Evasion, Magic Evasion, Speed; critical chance/damage; attack-speed and movement-speed multipliers.
- Combat now consumes the attack-type-specific power/defense values, combines Luck-derived critical bonuses with the existing Combat Profile critical settings, and scales montage/impact/combo/recovery timing together from derived attack speed. AI attack-cadence estimation uses the same Speed clock.
- Speed-driven movement remains correct through blocking: stat/equipment changes preserve the active block movement penalty and restore the new derived full speed when blocking ends.
- Added server-authoritative Attribute Points with points-per-level, point spending, cap validation, refund, external point rewards, replicated UI events and Blueprint snapshot/getter APIs.
- Added `Equipment -> Stats -> Equipped Stat Modifier` to Item Definitions. Only genuinely equipped runtime inventory instances contribute bonuses, and inventory/equipment changes rebuild stats automatically.
- Character save format advanced to version 4 and persists stat allocations/unspent points; older saves are migrated from their saved Level without discarding existing inventory/XP/vitals. Equipped max-vital bonuses are restored before saved current vitals are clamped.
- Attribute Point level rewards use a saved high-water mark so level-down/re-level cycles cannot duplicate point rewards. Direct authoritative `SetProgression` changes now emit the normal XP/Level events.
- Added optional classic Accuracy/Evasion hit checks. They are disabled by default for action-combat presentation compatibility.
- `Enable JRPG Stat System` is disabled by default so all pre-v2.5 characters retain their existing legacy stat/combat behavior until explicitly migrated.

See `Docs/JRPG_STATS.md`.

# 2.4.2-alpha — AI dodge authoring + stagger escape

- Added `AI Dodge Chance` directly to `Combat -> Dodge`, so NPC dodge probability is visible alongside Direction Montages, Duration, Distance and invulnerability settings instead of being hidden on the separate AICombat component.
- Retained the existing AICombat `DodgeChance` property unchanged for serialized/Blueprint compatibility; customized legacy values remain a fallback while the new profile chance is left at its default.
- Added `Dodge Cancels Stagger` (enabled by default). A valid dodge may now start while staggered, atomically clears the stagger timer/tag/state, broadcasts stagger-end, clears residual stagger knockback velocity, then begins the dodge.
- Automatic AI defence can therefore dodge out of stagger when a new incoming attack creates a valid reaction opportunity.
- Improved reaction scheduling for fast attacks so `ThinkInterval` cannot make an otherwise valid dodge window disappear between AI defence ticks.
- Preserved v2.4.1 navigation-abort and code-driven lateral displacement behavior.
- Public reflected changes are intentionally limited to the Dodge settings struct; no Quick Access, Inventory, Equipment, Spawner, Character or Day/Night API was changed.

# 2.4.1-alpha — AI dodge movement reliability

- Fixed automatic NPC dodges being selected and animated while active AI path following could immediately fight the dodge displacement, making left/right evasion appear stationary.
- `PerformDodgeAuthority` now aborts an AI controller's active path before dodge presentation/movement begins.
- Code-driven dodges clear current CharacterMovement velocity and apply a clean lateral launch derived from the existing authored `Distance / Duration` settings.
- `ARPGAICombatComponent` resets cached combat MoveTo state when committing/holding a dodge, preventing stale path state from blocking pursuit after the dodge finishes.
- Root-motion-only dodge behavior remains opt-in and unchanged; navigation is still suspended so root motion can move the NPC without path-following interference.
- No Blueprint-facing public header/schema changes versus v2.4.0-alpha.
- Added `Tools/test_ai_dodge_movement_model.py` regression coverage.

# 2.4.0-alpha — Day/night AI population swapping

- Added opt-in `Enable Midnight Population Swap` to `ARPGAISpawner`; disabled spawners retain the pre-2.4 preserved population lifecycle unchanged.
- The existing weighted `Spawn Table` is the daylight/default population. Added a weighted `Midnight Spawn Table` used from 00:00 until `ARPGDayNightCycle::DayStartHour`.
- At midnight, loaded spawners cleanly remove daylight leftovers and immediately spawn midnight entries. At morning/day start, midnight leftovers are removed and daylight entries return.
- Phase cleanup removes `OnDestroyed` bindings before destroying phase NPCs, so a time-of-day swap cannot be mistaken for combat death, group defeat, or schedule an obsolete respawn.
- Respawn timers, pending whole-group state, preserved immediate counts and desired group size are reset atomically on a population-phase transition.
- Distance-streamed spawners update their phase while unloaded without spawning; the next relevance activation uses only the correct current phase table instead of restoring the old phase.
- Added optional `Day Night Cycle Override` and automatic first-cycle discovery, plus separate midnight Min/Max group-size authoring.
- Added event-driven hour/day bindings with recurring spawner checks as a safety net, so simulated/fixed clock jumps that skip directly across midnight still synchronize correctly.
- Added `Refresh Day Night Population Now` and `Is Midnight Population Active` Blueprint utilities for testing/diagnostics.
- Added `Tools/test_day_night_spawner_model.py` regression coverage and dedicated setup documentation.

# 2.3.0-alpha — Melee combat polish

- Fixed ordinary Hit React montages interrupting an already-running attack montage while the authoritative attack timer continued, which made player combos look cancelled even though damage still resolved. Light hits now preserve an active attack; true stagger/guard-break/parry/death remain legitimate interruption states.
- Critical stagger now stops AI path following immediately, zeroes character movement before launch, applies the full configured launch vector, and emits the Stagger cue from the successful stagger path itself.
- Automatic melee AI no longer submits Basic Attack again while already attacking, preventing the Think loop from auto-buffering endless combo attacks.
- `Attack Slot Cooldown After Attack` now gates solo melee AI as well as coordinated groups, and is measured after estimated attack recovery for readable spacing between attacks.
- No Blueprint-facing public header/schema changes in this release.

# Changelog

## 2.2.3-alpha.2 — 2026-08-11

- Blueprint-compatibility hotfix based directly on the known-compiling 2.2.3-alpha.1 public API/schema.
- Reverted the 2.2.4 reflected `UARPGQuickAccessComponent` property addition so existing Blueprint casts, component references, and serialized nodes do not see a changed native reflection schema.
- Active-slot replacement auto-equip is implemented privately in `ARPGQuickAccessComponent.cpp`: dropping/assigning a different Equip-action weapon/tool into the currently active Quick Access slot immediately performs the existing exclusive equipment handoff.
- Consumables are never auto-used by assignment.
- No public header, UFUNCTION, UPROPERTY, USTRUCT, or delegate signature changes versus 2.2.3-alpha.1.

## 2.2.3-alpha.1 — 2026-08-11

- Fixed UE 5.8 C++ compilation in `UARPGQuickAccessComponent::ClearSlotAuthority`: the non-canonical-slot guard now returns `false` from the `bool` function instead of `EARPGQuickAccessResult::EmptySlot`.
- No Quick Access gameplay or Blueprint API behavior was changed by this compile-only correction.

## 2.2.3-alpha — 2026-08-11

- Fixed Quick Access weapon/tool activation leaving the previously active held item equipped when the new item used a different logical `EquipmentSlot` tag (for example Tool vs Weapon), which could render both meshes in the same hand.
- Quick Access now tracks the last item it activated as equipment and explicitly unequips that runtime instance before equipping a different Quick Access weapon/tool.
- Replacing or clearing the currently active hotbar slot preserves the previous held-item runtime GUID long enough for the next activation to perform a clean handoff, so drag-replacing an active Axe with a Sword cannot orphan the Axe in-hand.
- Added `Exclusive Active Quick Access Equipment` (enabled by default) to keep Quick Access as one active held-item channel without changing the general Equipment system's support for independent armor/offhand slots.
- Consumable activation intentionally does not unequip the currently held weapon/tool.
- Added v2.2.3 source validation for the exclusive active-equipment handoff path.

## 2.2.2-alpha — 2026-08-11

- Reworked Quick Access duplicate handling around a deterministic per-slot `AssignmentRevision`. The most recently assigned target slot now wins duplicate repair instead of repair depending on array iteration order.
- Quick Access assignment is now treated as an atomic move: all prior claims for the runtime `InstanceId` (and same `ItemId` when duplicate item types are disabled) are cleared before the target is written.
- `Get Slot View`, item lookup helpers and availability queries now defensively suppress non-canonical duplicate slots even if legacy, replicated or corrupted state reaches the owner UI.
- Save/load repair is revision-aware and backward-compatible: older slots default to revision 0 and are normalized deterministically.
- Added v2.2.2 regression validation for assignment revisions, target-preferred repair, and defensive UI projection.

## 2.2.1-alpha — 2026-08-11

- Fixed Quick Access allowing the exact same runtime Inventory `InstanceId` to remain assigned to multiple hotbar slots in configurations where same-item-type duplicates were enabled/serialized.
- The exact runtime inventory GUID is now **always unique across Quick Access slots**. Reassigning an owned item instance clears every previous slot holding that same instance before the target slot is written, so inventory-to-hotbar drag/drop behaves as a true move.
- Clarified `Allow Same Item Type In Multiple Slots`: when enabled it permits only different owned runtime instances/stacks of the same `ItemId`; it never permits cloning one runtime instance across multiple slots.
- Hardened runtime/save repair so duplicate or stale bindings select only unclaimed positive-quantity owned instances. Existing duplicate v2.2.0 state is repaired automatically on BeginPlay/load/inventory refresh.
- Fallback `ItemId` rebinding no longer steals an instance already claimed by another Quick Access slot.
- Added Blueprint-pure `Find Slot For Item Instance` for exact runtime-GUID diagnostics and UI workflows.
- Active-slot notifications now refresh correctly when the contents of the already-active target slot change during reassignment.
- Added v2.2.1 source-regression validation for strict runtime-instance uniqueness and package version integrity.

## 2.2.0-alpha — 2026-08-10

- Added inherited `ARPGQuickAccessComponent` to the ready `ARPGCharacter`, providing persistent numbered active-item slots for weapons, gathering tools, food, potions and project-specific items.
- Added one-button `Activate Slot`: `Quick Access Action = Auto` equips equippable items through the existing Equipment component, uses `bUsable` items immediately, and falls back to selection-only for other items.
- Added 1-based Blueprint input wrappers on `ARPGCharacter`: `Quick Access Pressed`, `Quick Access Next`, `Quick Access Previous` and `Use Active Quick Access Item`, so keyboard number keys and gamepad D-pad/wheel controls do not need hand-written Equip/SetEquipped graphs.
- Quick-access assignments retain both stable ItemId and the exact owned runtime inventory instance GUID. Runtime activation never treats an Item Definition Data Asset as ownership.
- Added authoritative hotbar rebinding when a stack instance disappears: a slot may reconnect to another positive-quantity owned runtime stack of the same ItemId; if none exists the assignment remains as an unavailable bookmark and cannot be activated.
- Quick Access slot layout and active slot are SaveGame-backed and owner-only replicated; remote players continue to receive only the normal equipment/inventory presentation state they actually need.
- Added optional duplicate-assignment prevention, slot swapping/clearing, next/previous cycling, active-slot events, action-result events, item-used events, quantity/availability helpers and a UMG-ready `Quick Access Slot View`.
- Added generic Item Definition use metadata for food/potions: `Usable`, consume-on-use/quantity, cooldown, Health/Mana/Stamina restoration, optional GAS Gameplay Effect and use montage/sound.
- Consumables are server authoritative, reject unavailable/insufficient/full-vitals-no-effect uses without consuming an item, apply cooldown per item type, and consume the exact active runtime stack only after the use path succeeds.
- Added `Restore Mana` to the Stats component so generic consumables can restore all three framework vitals without custom Blueprint mutation.
- Starting Inventory entries now expose optional `Quick Access Slot (0 = None)`. An auto-equipped starter assigned to a quick slot also becomes the initial active hotbar slot.
- Character saves now persist Quick Access slots and active slot after Inventory, allowing load-time runtime-instance repair against the restored inventory. Save version advanced to 3.
- Added `Docs/QUICK_ACCESS.md` and v2.2 source-regression validation.

## 2.1.1-alpha — 2026-08-10

- Fixed v2.1 equipment/Woodcutting runtime-definition desynchronization by storing the exact Item Definition soft reference inside each runtime `FARPGInventoryEntry` alongside its stable ItemId.
- `Add Item Definition`, Starting Items, tree rewards and future saves now preserve the exact authored Data Asset instead of requiring Equipment/Woodcutting to rediscover it later through Asset Manager.
- Older ID-only inventory saves remain supported: runtime entries backfill from Starting Items and the existing stable-ID resolver when possible.
- Item `DefinitionId` is now optional for inventory authoring; when blank, the Data Asset name (for example `DA_StoneAxe`) becomes the stable runtime ItemId automatically.
- Tightened equipped-tool validation: an axe only counts when a real runtime entry has a valid instance GUID, positive quantity, `bEquipped=true`, a valid slot, and that slot matches the resolved equippable Item Definition. Merely having an axe Data Asset in the project can never satisfy Woodcutting.
- Equipment effects, held visuals, combat swing audio and Woodcutting now resolve from the exact runtime inventory entry first.
- Tagged crafting/fuel checks now resolve Item Definitions from the runtime entry as well, keeping newly-authored project items consistent outside Equipment/Woodcutting.
- Equip/unequip presentation RPCs now carry the exact Item Definition asset instead of only an ItemId, removing another late-resolution failure path.
- Woodcutting swing and tree-hit presentation now receive the exact equipped tool selected by authority, so gathering swing/hit audio no longer depends on client-side definition rediscovery timing.
- Equipment visuals now auto-fallback through common right-hand socket/bone names when the configured socket is missing, with a runtime warning instead of silently placing the weapon at an unusable location.
- Added Blueprint inventory diagnostics for resolving an item definition by instance and checking runtime equipped state.
- Added v2.1.1 regression validation covering exact runtime item references, strict equipped-tool gates, exact-tool audio presentation and descriptor/version integrity.

## 2.1.0-alpha — 2026-08-10

- Added editable `Starting Items` to `ARPGInventoryComponent`, using Item Definition asset pickers, quantity and optional Equip On Spawn; retained `Runtime Items` as protected replicated/save state.
- Added authority `Apply Starting Items` and default-on begin-play seeding with empty-inventory protection; automatic seeding is deferred past the persistence auto-load pass so existing saves take precedence cleanly.
- Added native Blueprintable `ARPGEquipmentVisualActor` with static/skeletal mesh support and no collision/tick/replication by default.
- Item Definitions now expose Equipped Visual Actor Class, Equipped Static Mesh, Equipped Skeletal Mesh and Equipped Relative Transform in addition to the existing attach socket.
- Equipment automatically creates/destroys held visuals from replicated equipped inventory state and exposes refresh/query/visual-change Blueprint hooks.
- Existing Equip/Unequip Montage fields are now consumed by the runtime Equipment component.
- Added per-item Equip, Unequip, Combat Swing, Gathering Swing and Gathering Hit sounds with volume/pitch tuning.
- Ordinary melee prefers an equipped item's Combat Swing Sound when configured; class/profile melee audio remains the fallback.
- Woodcutting automatically uses Gathering Swing Sound (Combat Swing fallback), and successful tree chops can play the equipped axe/tool Gathering Hit Sound alongside tree feedback.
- Equipment replacement in the same slot now presents the displaced item's unequip cue before the new item's equip cue.
- Added `Docs/EQUIPMENT_INVENTORY.md` and v2.1 source-regression validation.

## 2.0.2-alpha — 2026-08-10

- Added automatic Basic Attack -> Woodcutting integration: when no real combat target is supplied, an equipped axe and an `ARPGTree` in the Woodcutting view trace redirect the normal Basic Attack input into one authoritative chop.
- Real combat/lock-on targets keep priority, so holding an axe does not steal attacks away from enemies.
- The separate `Start Woodcutting From View` interaction remains unchanged and continues to support automatic repeated chopping.
- Basic-attack chopping requires an equipped Woodcutting tool by default, respects each tree's level/tool-tier gates, and respects the normal Woodcutting swing interval so input spam cannot harvest faster than the authored cadence.
- Added `Has Equipped Woodcutting Tool` and `Try Chop Tree With Basic Attack` Blueprint helpers.
- If no dedicated chop montage is assigned, Woodcutting can now fall back to the character's normal melee combat montage automatically.
- Added replicated per-instance tree size variation with exposed `Minimum Mesh Scale` / `Maximum Mesh Scale`, randomize/reroll toggles and runtime selected scale.
- Tree scale multiplies the existing authored visual scale and applies consistently to both the falling trunk and stump.
- Added `Get Selected Tree Mesh Scale`, `Select Random Tree Mesh Scale` and `Set Tree Mesh Scale` Blueprint APIs.
- Retained the v2.0.1 UE5.8.1 `TObjectPtr` compile fix and Day/Night network-frequency deprecation fix.

## 2.0.1-alpha — 2026-08-10

- Fixed the real UE5.8.1 `ARPGTree.cpp` C2445 compile error in `GetSelectedTreeMesh()` caused by a conditional expression mixing `UStaticMesh*` with `TObjectPtr<UStaticMesh>`.
- Replaced the mixed ternary with an explicit validity branch returning `TreeMeshes[SelectedTreeMeshIndex].Get()` or the TreeMesh component's raw static-mesh pointer fallback.
- Added validator regression coverage that rejects the exact mixed `TObjectPtr`/raw-pointer tree-mesh ternary pattern.
- Replaced the deprecated direct `NetUpdateFrequency` member write in `ARPGDayNightCycle` with `SetNetUpdateFrequency(2.f)` as requested by the UE5.8.1 compiler warning.
- No Woodcutting, tree-fall, reward, skill, AI, day/night, combat, targeting, save, or networking behavior was removed.

## 2.0.0-alpha — 2026-08-10

- Added inherited `ARPGWoodcuttingComponent` to the ready player character.
- Added automatic `Start Woodcutting From View` targeting plus direct Start/Stop/Chop Once APIs.
- Added persistent Woodcutting progression through the existing Skill Component, with level/XP/progress/unlock pure nodes and optional Skill Definition curve/unlocks.
- Added gathering metadata to Item Definitions: tool tags, gathering power and gathering tool tier.
- Added native Blueprintable `ARPGTree` with tree-mesh variation arrays, replicated variation selection, stump support, chop health/resistance, required Woodcutting level and optional axe/tool-tier gates.
- Added server-authoritative repeated chop timing with exposed swing interval and impact delay plus optional chop montage.
- Added direct Wood Item picker, min/max wood rewards, bonus-drop array, inventory-capacity preflight and automatic item-looted quest routing.
- Added smooth replicated tree fall away from the harvester, fallen-trunk hold, stump state and timed tree respawn.
- Added Niagara/Cascade chop/fell feedback and exposed sounds.
- Added tree Blueprint calls/events for custom harvesting extensions.
- Added Woodcutting gameplay tags and source-regression validation.

## 1.10.0-alpha — 2026-08-10

- Added first-class distance-based AI population streaming directly to `ARPGAISpawner`, enabled by default for performance.
- Unloaded spawners automatically create their NPC population when a player-controlled pawn enters the exposed Spawn Activation Radius.
- Loaded populations automatically despawn after all players remain beyond the larger Despawn Radius for the exposed delay, providing hysteresis and preventing boundary pop/thrash.
- Active spline/free-roam populations can remain relevant while a player is near any spawned NPC, preventing long-route NPCs from disappearing beside the player merely because they travelled away from their spawner origin.
- Distance unload is a clean streaming despawn: it removes destruction callbacks first, so no kill credit, loot, quest kills, group-defeated events or false respawn scheduling are generated.
- Inactive spawners stop leash/group-cohesion timers; only a low-frequency server relevance timer remains, with randomized first-delay staggering across many spawners.
- Added exposed optional combat retention, 2D/3D relevance distance, activation/despawn radii, unload delay, check interval and group-size re-roll policy.
- Distance reload preserves desired group size by default and preserves real pending respawn cooldowns; members that were alive can return immediately while still-dead members wait out their remaining delay.
- `Respawn Mode = Never` remains permanent across unload/reload and cannot be bypassed by leaving/re-entering the area.
- Added Blueprint runtime status/controls: `Is Population Active`, `Get Nearest Relevant Player Distance`, `Is Player Inside Spawn Activation Radius`, `Evaluate Population Relevance Now`, `Set Population Active`, plus population activated/deactivated events.
- Added `Docs/AI_SPAWNER_PERFORMANCE.md` and validator coverage for distance population streaming invariants.

## 1.9.1-alpha — 2026-08-10

- Fixed the UE 5.8.1 Day/Night compile failure caused by the invalid `Engine/SkyAtmosphere.h` include.
- `ASkyAtmosphere` and `USkyAtmosphereComponent` are declared by `Components/SkyAtmosphereComponent.h` in UE 5.8; the day/night implementation now relies on that correct Engine header only.
- Added validator coverage that rejects the obsolete `Engine/SkyAtmosphere.h` include and requires `Components/SkyAtmosphereComponent.h` for the day/night source.
- No Day/Night behavior was removed: host-system-clock authority, replicated world time, built-in sun/moon/sky/fog rig, pure day/night queries, and test clock modes are retained.

## 1.9.0-alpha — 2026-08-10

- Added `ARPGDayNightCycle`, a drop-in replicated day/night actor that follows the authority/host computer's local system clock by default.
- Added a built-in dynamic Sun + Moon + Sky Atmosphere + real-time Sky Light + Exponential Height Fog rig for one-actor level setup.
- Added optional external Sun/Moon/Sky Light/Sky Atmosphere/Fog actor references for projects that already own a lighting rig.
- Added smooth client clock extrapolation between replicated authority snapshots so celestial movement does not visibly step with network updates.
- Added explicit Dawn/Day/Dusk/Night phases, exposed semantic boundaries, phase/hour events and lighting-independent day/night gameplay checks.
- Added global Blueprint-pure `ARPG World Time` nodes: Is Day, Is Night, Get World Hour, Get World Date Time, Get Day Night Phase and Get Daylight Amount.
- Added Fixed Time and accelerated Simulated Clock testing modes plus a designer clock offset.
- Added smooth sun/moon rotation and exposed day/night intensity, color, skylight and fog tuning.
- Uses Sky Light Real Time Capture for dynamic environment lighting rather than repeatedly calling costly manual sky recaptures.


## 1.8.0-alpha — 2026-08-10

- Fixed temporary retaliation persistence across player death/respawn: an AI now binds to its current target's Combat LifeState and immediately forgets temporary aggression when that target leaves Alive state.
- Added `Restore Original Disposition After Target Death` (default ON) and `Clear Threat Against Dead Targets` (default ON), so passive/neutral NPCs return to their original faction/fallback behavior after killing an aggressor.
- Added Blueprint authority helpers to forget one temporary aggressor or all temporary aggression without changing authored faction relationships.
- Added first-class coordinated melee group combat so large packs no longer all path into the exact target location.
- Added a default cap of three simultaneous melee attackers per shared target; excess allied melee NPCs enter a waiting/orbit role and automatically rotate into attack openings.
- Added distributed inner attack positions plus an outer waiting ring, optional continuous orbiting, stable group spacing, NavMesh projection, path-refresh throttling and target-facing AI focus.
- Attack-slot leasing/yield/cooldown prevents one blocked or unreachable NPC from monopolizing an opening and gives waiting actors fair attack opportunities.
- Group coordination uses the existing ally model (faction, spawn group, assist group and configured fallbacks), so independent wildlife groups can coordinate without being physically attached or forced into a formation.
- Added Blueprint-readable `Group Combat Role`, group size, slot index and melee-slot state plus `On Group Combat Role Changed` for animation/UI/debug extensions.
- Added `Docs/GROUP_COMBAT.md`, expanded aggro documentation, and validator coverage for target-death cleanup and coordinated group-combat runtime paths.

## 1.7.0-alpha — 2026-08-10

- Fixed automatic NPC retaliation: received combat hits now create temporary aggression so neutral or unresolved faction attackers remain valid combat targets instead of being discarded on the next AI think.
- Added exposed AICombat retaliation controls for neutral-faction override, missing-faction fallback, friendly-attacker policy, aggression memory duration and retaliation threat bonus.
- Added optional faction-free proactive fallbacks for Attack Players On Sight and Attack Unfactioned Pawns On Sight.
- Added automatic nearby ally assistance when an NPC is attacked, with exposed assist radius/threat and protection against overriding an ally already in combat by default.
- Ally recognition now supports same faction, allied factions, explicit Assist Group Id, same ARPG AI Spawner group, and same-class fallback when faction identity is missing.
- ARPG AI Spawner now registers itself as each spawned AI's runtime Spawn Group Owner, allowing group members to help each other even with Stay Together disabled and without faction data.
- Added temporary AI aggression override to Combat CanDamageActor so retaliation still deals damage when a class profile disallows neutral damage.
- Added `Default Relationship To Unlisted Factions` to Faction Definitions for easier authoring of factions that hate/love all unlisted factions.
- Wired the existing `Attack Hostile On Sight` Faction Data Asset flag into automatic AI acquisition.
- Added Blueprint aggro/assist queries, events and explicit help-call APIs.
- Added `Docs/AI_AGGRO_ASSIST.md` and validator coverage for retaliation/assist integration.

## 1.6.0-alpha — 2026-08-10

- Upgraded player lock-on to a constant Z-target style by default: the owning camera/control rotation smoothly tracks the selected target while the player character continuously faces it.
- Added automatic Spring Arm/direct Camera `Use Pawn Control Rotation` management during lock-on, with the previous camera-rig value restored when the target is released.
- Added lock-on movement-facing override for player characters so Orient Rotation To Movement cannot pull the character away from the target; settings are cached/restored automatically.
- Targeting tick now runs after the owning Actor tick to reduce conflicts with Blueprint movement/rotation helper logic.
- Added first-class combat impact feedback settings to Class Definitions with Niagara-first hit/critical/block/parry/stagger effects and automatic Cascade `UParticleSystem` fallback.
- Added exposed combat audio for melee swing, ranged attack, magic cast, hit, critical hit, block, parry, dodge, block start/end, guard break, stagger, death and revive, with global volume and randomized pitch range.
- Added authority-side critical-hit stagger chance with one-hit stagger duration, immunity window, optional attack interruption, dedicated montage (Hit React fallback), real character knockback and physics impulse support.
- Added replicated `bIsStaggered`, `OnStaggerStateChanged`, `Combat.State.Staggered`, and Blueprint `Is Staggered`.
- Automatic NPC combat now stops path movement while staggered so navigation does not immediately override knockback/reaction presentation.
- Added lightweight multicast combat feedback cues so cosmetic particles/audio spawn on relevant clients while damage/critical/stagger decisions remain server-authoritative.
- Added Niagara runtime dependency and validator coverage for camera lock, Niagara/Cascade fallback, audio, stagger and AI knockback integration.
- Added `Docs/COMBAT_FEEL.md`.

## 1.5.0-alpha — 2026-08-09

- Added route-level `Loop Route` on `ARPGAISplineRoute`, enabled by default so normal patrol routes no longer stop permanently at an endpoint.
- Added `Closed Loop Geometry` and `Reverse At Open Ends` as separate designer concepts: closed splines wrap continuously; open looping routes reverse by default, or can return to the opposite endpoint through NavMesh.
- `ARPGAISplineComponent` uses route traversal settings by default while retaining the per-NPC Once / Loop / Ping-Pong override for advanced cases.
- Added spawner synchronized spline direction: one spawned group member acts as direction leader so an open Ping-Pong group reverses together instead of splitting and running both ways.
- Group direction synchronization is independent of physical `Stay Together` cohesion, so a loose group can still share one sensible route travel direction.
- Implemented functional `Group Cohesion` settings on `ARPGAISpawner`: Stay Together, cohesion radius/check interval, shared spline direction and forward/reverse/random-per-group route direction.
- Added first-class spawner `Movement Mode`: Automatic, Spline Route, Free Roam, or No Automatic Travel. Automatic preserves prior behavior by selecting spline travel whenever a route is assigned.
- Added spawner Free Roam settings with NavMesh reachable-point wandering, roam radius, think interval and spawner-centered leash.
- Spawn grouping is now independent from movement cohesion: groups still share desired count/defeat/respawn behavior when Stay Together is disabled, while each member can roam independently.
- When Stay Together + Free Roam are both enabled, a group leader roams around the spawner and followers use cohesion recovery before resuming random travel. Combat always has higher movement priority.
- `ARPGAICharacter` now includes a lightweight `AIWanderer` component by default, disabled until requested by free-roam behavior. Custom APawn classes can receive a runtime wanderer component from the spawner when needed.
- Hardened `ARPGWandererComponent` with combat/death awareness, home-return enforcement, timer-on-demand behavior, configurable acceptance radius and explicit home/return Blueprint APIs.

## 1.4.1-alpha — 2026-08-09

- Fixed the UE 5.8.1 AI spline compile failure by explicitly including `Navigation/PathFollowingComponent.h` in `ARPGAISplineComponent.cpp`.
- `AIController.h` only forward-declares `EPathFollowingRequestResult::Type` for this translation unit, so the concrete request-result enumerators require the Path Following header before `EPathFollowingRequestResult::Failed` can be used.
- Kept the original symbolic request-result handling and retry behavior; no magic numeric enum workaround is used.
- Added validator coverage requiring the Path Following header whenever the spline component references `EPathFollowingRequestResult` enumerators.

## 1.4.0-alpha — 2026-08-09

- Added first-class `ARPGAISplineRoute` world actor with an editable native `USplineComponent` and per-control-point wait/event settings.
- Added `ARPGAISplineComponent` as an inherited default component on every `ARPGAICharacter`; assigning a route is enough to start NavMesh-following automatically.
- Spline-following never attaches or teleports the pawn along the path: it samples look-ahead route locations, projects goals to Navigation, and uses `AAIController::MoveToLocation`.
- Added Once, Loop and Ping-Pong patrol modes plus nearest/first/last/explicit/random start modes.
- Added automatic initial route joining, configurable follow-step distance, acceptance radius, lateral lane offsets, random per-NPC offsets, navigation filters, partial-path policy, and stalled-move recovery.
- Added Route Id auto-resolution for reusable NPC Blueprint classes; when multiple routes share an id the nearest matching spline is selected.
- Added automatic combat suspension/rejoin: NPCs leave the route to fight, use the route departure point as the chase-leash anchor, then rejoin the nearest/previous route progress through NavMesh.
- Corrected AI combat home-leash behavior so it constrains active combat chases instead of breaking long non-combat patrol routes.
- Integrated spline ownership with Wanderer AI so optional roaming cannot issue competing movement requests while a route is active.
- Integrated `ARPGAISpawner` with an exposed Assigned Spline Route, automatic route start for spawned NPCs, and route-aware spawner leash suppression.
- Added per-route-point `PointId`, fixed/random wait time, facing option, and Blueprint route-point events for optional guard/emote/script hooks.
- Added runtime Blueprint APIs/events for route assignment, start/stop/pause/resume, route rejoin, route state, failure and finish notifications.

## 1.3.0-alpha — 2026-08-09

- Added automatic physical ragdoll death presentation for `ARPGAICharacter`; NPCs use ragdoll by default without Blueprint death logic.
- Added `FARPGDeathPresentationSettings` on the Combat Component with exposed ragdoll collision profile, capsule handling, inherited movement velocity, death-hit impulse and dedicated-server simulation policy.
- Death montage behavior is now a true fallback: when an NPC mesh has no usable Physics Asset or ragdoll simulation cannot start, the existing Class Definition / Combat death montage plays instead.
- Player `ARPGCharacter` behavior remains animation-first by default; player ragdoll can be enabled from the same exposed Combat -> Death settings when desired.
- Added reliable multicast death-presentation/reset paths and late-join `LifeState` recovery so ragdoll/fallback presentation is reconstructed on clients.
- Added ragdoll reset for same-actor respawns, restoring capsule collision, mesh collision profile, mesh relative transform and animation control before revive.
- Death ragdolls inherit character movement velocity and can receive a configurable velocity-change impulse at the physics body nearest the final hit location.
- Added `OnRagdollStateChanged` and `IsRagdollActive` Blueprint hooks.
- Added validator coverage requiring the ready AI character to keep ragdoll + montage fallback enabled by default.

## 1.2.1-alpha — 2026-08-09

- Fixed UE 5.8.1/MSVC `C2445` in `ARPGTargetingComponent::CreateTargetMarker` by removing the ambiguous conditional expression between `TSubclassOf<UARPGTargetMarkerWidget>` and `UClass*`.
- Target marker widget selection now copies the configured `TSubclassOf` first, then explicitly falls back to `UARPGTargetMarkerWidget::StaticClass()` only when no override is assigned.
- Added validator coverage to reject this ambiguous `TSubclassOf ? TSubclassOf : StaticClass()` pattern in future releases.
- No targeting behavior or editor settings were removed; custom marker widget subclasses still override the native fallback.

## 1.2.0-alpha — 2026-08-09

- Added `ARPGTargetingComponent` as a ready component on every `ARPGCharacter`, with player-local camera-centered hostile target acquisition.
- Added one-call `ToggleLockOn`, `TargetLeft`, `TargetRight` and `ClearLockOn` Character Blueprint input helpers.
- Added configurable acquire/maintain/switch ranges, field-of-view scoring, distance weighting, LOS acquisition, LOS grace, auto-unlock and optional auto-reacquisition.
- Lock-on now synchronizes the selected actor into replicated `ARPGCombatComponent::CombatTarget`, with server-side target/distance validation and client reconciliation.
- Basic attacks automatically consume the locked target and use target-aware attack direction while preserving smooth facing.
- Gameplay Ability activation automatically requests target facing through the Ability System Component activation callback; `Combat.Targeting.IgnoreAutoFace` can opt an ability out.
- Added `ARPGGameplayAbility` with `Ignore Lock-On`, `Prefer Lock-On Target` and `Require Lock-On Target` policy, optional range validation, and Blueprint helpers for target actor/location/TargetData.
- Expanded `ARPGAbilityBridgeComponent` with preferred lock-on target access, target-data creation and tag activation with an explicit target.
- Added native `ARPGTargetMarkerWidget` and automatic screen-space marker creation on the selected target.
- Exposed target marker texture, UI material, tint/color, size, height/socket and acquire/pulse/release animation parameters directly on the Targeting Component.
- Added a built-in fallback reticle so lock-on UI works even before the game supplies custom artwork.
- Added Blueprint marker customization events while retaining the default native acquire/release animation.
- Added optional target aim socket and marker socket support for precise character/boss presentation.
- Added `Combat.State.LockedOn` and standard targeting-policy Gameplay Tags.
- Added UMG/Slate runtime dependencies required by the automatic native target marker.

## 1.1.0-alpha — 2026-08-09

- Rebuilt `ARPGCombatComponent` into a complete basic-combat runtime instead of death/respawn-only plumbing.
- Added Class Definition combat profiles with melee/ranged/magic basic attack type, damage scaling, criticals, ranges, projectile settings, detailed combo steps, dodge and block configuration.
- Ordered Class Definition melee/ranged/magic montage arrays now drive combos automatically when detailed combo steps are left empty.
- Added server-authoritative `PerformBasicAttack`, combo queue/reset state, automatic impact timing, melee sphere tracing, ranged hitscan and optional replicated `ARPGCombatProjectile`.
- Added direct character Blueprint input helpers: `BasicAttack`, `Dodge`, `BlockPressed`, `BlockReleased`.
- Added the built-in `ARPG Combat Impact` Anim Notify for montage-authored hit/fire timing without custom Blueprint trace logic.
- Added directional dodge with exposed montages, stamina cost, cooldown, launch/root-motion mode and invulnerability window.
- Added shield/guard blocking with facing arc, damage-type reduction, stamina pressure, perfect-block/parry timing and guard-break stagger.
- Added hit reactions, death/revive montage use, automatic player respawn/checkpoint transform support, kill-credit routing and loot handoff.
- Added standard combat Gameplay Tags plus `Equipment.Shield`.
- Added `ARPGAICombatComponent` and ready `ARPGAICharacter` for automatic faction/threat-driven NPC combat without requiring a Behavior Tree.
- Added AI reaction delay, dodge/block decisions, NavMesh chase, combat range selection and optional GAS auto-ability tags.
- Integrated AI spawner leash/home settings into AI combat and added dead combat pawn corpse cleanup/respawn handling.
- Corrected default Player faction assignment so it only applies to player-controlled characters.
- Marked the shared RPG definition base `NotBlueprintable` so designers create native Data Asset instances instead of accidentally creating incompatible definition Blueprint Classes.

## 1.0.2-alpha — 2026-08-09

- Fixed `UARPGAttributeSet::GetLifetimeReplicatedProps` to use Unreal's required `OutLifetimeProps` replication list and replaced the malformed macro/brace block with explicit `DOREPLIFETIME_CONDITION_NOTIFY` registrations.
- Fixed Gameplay Attribute RepNotify definitions so they compile cleanly with UE 5.8.1.
- Fixed the battle-pet cooldown local variable conflict in `SetCooldown`.
- Added the required `GameFramework/CharacterMovementComponent.h` include for combat death/respawn movement calls.
- Added the required `GameFramework/PlayerState.h` includes for chat player-name access.
- Renamed mount-local controller variables to avoid UE's shadow-variable warning-as-error.
- Updated setup documentation so only Gameplay Abilities is described as a plugin; GameplayTags and GameplayTasks remain Build.cs modules.

## 1.0.1-alpha — 2026-08-09

- Fixed UE 5.8 plugin dependency declaration: `GameplayTags` and `GameplayTasks` are runtime modules, not standalone plugins, so they are no longer listed in `AkumasRPGFramework.uplugin`.
- Kept `GameplayTags` and `GameplayTasks` in `AkumasRPGFramework.Build.cs`, where module dependencies belong.
- Added validator coverage for this descriptor mistake so it cannot silently return in later packages.

## 1.0.0-alpha — 2026-08-09

Initial unified source build of Akuma's RPG Framework for UE 5.8.

Highlights include the ready RPG character, GAS integration, stats/combat/progression, inventory/equipment, quests, generic skills + Slayer, factions/reputation, vendors/buyback, AI spawner/wanderers, boss/dungeon foundations, battle pets, unified chat, local accounts, direct-IP network helpers, character/world persistence, faction-aware modular building, storage/crafting/furnaces and mounts.
