# Settlement Building, Storage & Production — v2.15.6

v2.15 promotes the framework's original low-level build actors into a complete player-facing settlement workflow. The gameplay state is authoritative; the placement ghost and menus are local presentation.

## What is ready

Every `AARPGCharacter` inherits:

- `Building` — replicated authority/placement component and exposed Build Catalog.
- `BuildingUI` — local ready build menu, placement HUD, storage UI and production-station UI.
- `Interaction` — player-owned RPC route for storage transfer, production queues, doors and demolition.

The standard build-piece kinds are:

`Foundation`, `Wall`, `WindowWall`, `Window`, `Doorway`, `Door`, `Floor`, `Ceiling`, `Roof`, `Stair`, `Pillar`, `Storage`, `Production`, `Decoration`, `Custom`.

Common pieces need **no build-actor Blueprint**. Assign a mesh on an `ARPGBuildPieceDefinition` and leave `Actor Class` empty; the framework chooses the native build actor. `Door`, `Storage` and `Production` automatically choose their specialised native actors.

---

## 1. Create resource Item Definitions

Create normal `ARPGItemDefinition` Data Assets for settlement resources, for example:

- `DA_Item_Wood`
- `DA_Item_Stone`
- `DA_Item_MetalOre`
- `DA_Item_MetalIngot`

Give them stable Definition IDs such as `Wood`, `Stone`, `MetalOre`, `MetalIngot`.

For furnace fuel, give Wood a Gameplay Tag such as:

`Item.Fuel.Wood`

The fuel system is tag based, so later you can permit Coal, Charcoal, Magic Fuel, etc. by using the recipe's desired fuel tag/design.

---

## 2. Create a Build Piece Data Asset

Content Browser:

**Right-click → Miscellaneous → Data Asset → `ARPGBuildPieceDefinition`**

Example:

`DA_Build_WoodFoundation`

Recommended values:

### Identity

- Definition Id: `Build_WoodFoundation`
- Display Name: `Wood Foundation`
- Build Category: `Wood`
- Piece Kind: `Foundation`
- optional Material Tier gameplay tag: e.g. `Building.Material.Wood`

### Actor

- Build Mesh: your foundation Static Mesh
- Preview Mesh: optional; leave empty to reuse Build Mesh
- Actor Class: **leave empty** unless you intentionally need a custom actor subclass

### Cost

Add Build Cost entries using the **Item asset picker**:

- Wood × 4

The old ItemId field still resolves older content, but new content should select the Item Definition asset directly.

### Placement

Typical modular starter values:

- Snap Placement: true
- Requires Snap Target: false for foundations
- Requires Support: true
- Allow Ground Placement: true
- Snap Size: match the real module width/depth used by the kit (for example `300` for a 300×300 foundation)
- Standard Wall Height: match the real wall-module height (for example `300`)
- Placement Bounds: set this to the actual half-extents used for collision validation
- Rotation Step Degrees: `90`
- Generate Standard Snap Points: true

`Placement Bounds` is important. It drives authoritative overlap/support validation, so tune it to the real mesh rather than leaving a huge generic box around a narrow object.

### Ground placement and mesh pivots (v2.15.2+)

Do **not** use `Placement Offset Z` just to compensate for a bottom-pivot foundation. Ground placement automatically reads the selected **Build Mesh** bounds and anchors its visible bottom-center to the traced landscape/support surface. A bottom-pivot mesh receives zero artificial lift; a centered mesh receives only its real half-height; corner pivots are also handled through the visible footprint anchor. `Placement Bounds` remains the collision-validation half-extents and is no longer used as an unconditional visual ground lift. Use `Placement Offset` only when you intentionally want an authored local offset.

### Imported mesh orientation (v2.15.3+)

You do **not** need to reimport a modular mesh just because its authored horizontal axis differs from the framework's logical snapping axis. Use **Actor → Mesh Relative Transform** on the Build Piece Data Asset. The transform is applied identically to the local placement ghost and the final native build actor, and transformed Build Mesh bounds are used by pivot-aware placement/snapping.

The standard wall graph intentionally keeps its logical wall run on actor-local **X** and defines actor-local **+Y as the logical front/exterior side**. For a wall mesh whose raw dimensions are approximately `31 × 302 × 271` (long axis on mesh Y), set **Mesh Relative Transform → Rotation Z** to `90` or `-90` so the visible wall becomes approximately `302 × 31 × 271` in actor space. No Static Mesh reimport or custom build Actor Blueprint is required. Choose the sign that places the wall face you consider the exterior/front toward actor-local +Y. Existing assets leave Mesh Relative Transform at Identity.

### Modular wall seams and 90-degree corners (v2.15.4+)

Walls often intentionally extend a little beyond the logical grid size (for example a `302` cm wall on a `300` cm snap grid), and corner posts/trim can overlap where two valid pieces meet. v2.15.4 treats that as a **structural seam**, not a generic collision exception.

The standard wall-family graph now includes side continuation, vertical stacking **and four geometric 90-degree L-corner positions (both facing variants)**. During placement, overlap with an already-built piece is tolerated only when the final incoming transform exactly matches one of that neighbour's native/custom snap transforms. This means valid straight seams and corners can overlap authored trim, while duplicate pieces, arbitrary clipping, unrelated world geometry and non-snap overlaps remain blocked. Multiplayer modification access is still enforced on every accepted seam neighbour.

For a `300` cm kit, keep `Snap Size = 300` even if the visible wall art is slightly longer (such as `302`). Use the real transformed half-extents for `Placement Bounds` and keep a small `Placement Collision Clearance` (the default `2` cm is a good starting point). You should not shrink the wall art, reimport the mesh, or inflate clearance simply to make corners place.

### Directional wall facing on support edges (v2.15.5+)

Wall art is often asymmetric: the exterior can use finished horizontal boards while the interior/back uses framing, posts or a different material. v2.15.5 formalizes the standard wall-facing convention instead of treating the two sides as interchangeable. After `Mesh Relative Transform`, actor-local **X** is the wall run and actor-local **+Y** is the logical front/exterior side.

Horizontal structural supports (`Foundation`, `Floor`, `Ceiling`, `Roof`) orient that logical +Y side toward the selected edge's **outward normal**. The four standard edge mappings are therefore `+Y edge → yaw 0`, `-Y edge → yaw 180`, `+X edge → yaw -90`, and `-X edge → yaw +90`. This fixes the previous +X/-X sign inversion that could make exactly two opposite walls display their back face.

At a built corner, a Foundation edge and a neighbouring Wall can both advertise the same geometric incoming-wall slot. v2.15.5 resolves that ambiguity semantically: when the transforms occupy the same physical slot, the horizontal support edge has priority because it uniquely defines exterior direction. Direct Wall → Wall corner construction still keeps both ±90° turn variants, so unsupported/custom wall-only shapes remain flexible. This priority is only a same-slot tie-breaker; normal distance/yaw snap selection elsewhere is unchanged.

### Deterministic upper-story wall facing (v2.15.6+)

Upper-story construction adds another same-slot case: the Wall-family piece directly below can advertise a vertical stack transform at the exact location where a neighbouring upper wall advertises a lateral or corner transform. Because asymmetric wall art distinguishes front from back, treating those candidates as equal can allow camera/view yaw to select the neighbouring candidate and reverse the new wall by 180 degrees.

v2.15.6 makes semantic ownership **candidate-aware**. For an incoming `Wall`, `WindowWall` or `Doorway`, a Wall-family candidate at target-local XY zero, above the target, with zero relative yaw is recognized as the direct vertical stack relationship and owns that physical slot. Its transform already inherits the supporting actor's rotation, so the new piece preserves the lower wall's logical +Y front/exterior direction. Lateral continuation and both ±90-degree corner variants remain unchanged when they describe different slots.

This is automatic framework behavior. Do not compensate upper-story pieces with `Mesh Relative Transform`, manual 180-degree rotation, custom snap points or alternate meshes; configure each wall-family asset's mesh transform once for its authored orientation and let the structural stack inherit actor facing.

### Construction

- `Construction Seconds = 0` → instant building
- `Construction Seconds > 0` → timed construction

For example:

- Wood Foundation: `0` or `0.25`
- Stone Foundation: `1.5`
- Metal Foundation: `2.5`

Timed construction grows/reveals the mesh from `Construction Start Scale Z` to full size. Materials can optionally expose a scalar parameter named by `Construction Progress Material Parameter` (default `ConstructionProgress`) or `BuildProgress` to create a custom hologram/dissolve/assembly material effect.

The framework also exposes Construction Start/Complete sounds.

---

## 3. Recommended starter structural kit

Use consistent module dimensions across a kit. Set `Snap Size` to the real horizontal module size used by the meshes (for example `300` for a 300×300 foundation or `400` for a 400×400 kit), and set `Standard Wall Height` to the real wall-module height.

| Data Asset | Piece Kind | Typical Cost | Requires Snap Target | Ground Placement |
|---|---|---:|---|---|
| Wood Foundation | Foundation | Wood ×4 | No | Yes |
| Wood Wall | Wall | Wood ×2 | Yes | Usually No |
| Wood Window Wall | WindowWall | Wood ×2 | Yes | Usually No |
| Wood Window | Window | Wood ×1 | Yes | No |
| Wood Doorway | Doorway | Wood ×2 | Yes | Usually No |
| Wood Door | Door | Wood ×1 | Yes | No |
| Wood Floor | Floor | Wood ×2 | Yes | Usually No |
| Wood Ceiling | Ceiling | Wood ×2 | Yes | No |
| Wood Roof | Roof | Wood ×3 | Yes | No |
| Wood Stair | Stair | Wood ×3 | Yes | Usually No |
| Wood Pillar | Pillar | Wood ×2 | Yes or project choice | Optional |
| Wood Chest | Storage | Wood ×8 | No | Yes |
| Primitive Furnace | Production | Stone ×15 + Wood ×5 | No | Yes |

Repeat the same structural definitions for Stone/Metal while changing Build Mesh, costs, health, construction duration and material tier.

Example Stone Wall:

- Piece Kind: Wall
- Build Cost: Stone ×5
- Max Health: higher than Wood
- Requires Snap Target: true
- Construction Seconds: `1.25`

Example Metal Wall:

- Piece Kind: Wall
- Build Cost: Metal Ingot ×4
- Max Health: higher than Stone
- Construction Seconds: `2.0`

This produces the intended game loop:

`gather Wood/Stone → build starter settlement → smelt Ore → obtain Metal Ingots → unlock/author stronger Metal structures`.

---

## 4. Standard snapping

The native snap graph covers the ordinary modular relationships automatically:

- Foundation → adjacent Foundation
- Foundation/Floor/Ceiling → wall, window-wall and doorway edges
- Foundation → one-story Floor/Ceiling/Roof
- Wall family → neighbouring/stacked Wall family
- Wall family → top Floor/Ceiling/Roof
- WindowWall → Window insert
- Doorway → Door insert
- Roof → neighbouring Roof
- horizontal modules → Stair orientations
- Pillar → wall-family connections

The local preview searches nearby completed build actors and chooses the closest compatible snap transform inside `Snap Capture Distance`.

**The server performs the snap and placement validation again.** A locally green ghost is prediction/presentation, not authority.

### Custom kit shapes

For triangular foundations, half walls, curved walls, gates, special roof ridges or unusual meshes:

- keep or disable `Generate Standard Snap Points`
- add `Custom Snap Points`
- set each `Incoming Placement Transform`
- optionally restrict `Accepted Incoming Kinds`

The transform is authored relative to the already-built target piece.

---

## 5. Add pieces to the player's ready Build Menu

Open the Player Blueprint derived from `AARPGCharacter`.

Select inherited component:

**Building → Catalogue → Build Catalog**

Add the Build Piece Data Assets in the order you want them displayed.

The native Build Menu groups them using each piece's `Build Category`, so a useful catalog can be organised as:

- Wood
- Stone
- Metal
- Utility
- Production
- Decoration

The row automatically shows icon/name/cost/buildable count and disables placement naturally through the validation path when resources are missing.

---

## 6. Recommended player input wiring

No Create Widget graph is required. Bind your project input actions to the inherited `AARPGCharacter` Blueprint-callable wrappers.

```text
B Pressed / Build Action
    -> Toggle Build Menu UI

Left Mouse / Confirm Build
    -> Confirm Build Placement

R / Rotate Build
    -> Rotate Build Placement (Direction = 1)

Mouse Wheel Down / Next Build Piece
    -> Next Build Piece

Mouse Wheel Up / Previous Build Piece
    -> Previous Build Piece

Right Mouse or Escape / Cancel Build
    -> Cancel Build Placement

E / Interact
    -> Interact Built Structure

Delete / Demolish Build
    -> Demolish Built Structure
```

`Interact Built Structure` performs the ready view trace:

- Door → toggle door
- Storage → open storage transfer UI
- Production → open station/furnace UI

`Demolish Built Structure` performs the same ready view trace for a player-modifiable runtime build, routes the request through the authoritative Interaction component, destroys it only after access checks pass, and applies the Build Piece Definition's configured demolition refund.

Your project can use Enhanced Input, legacy input, controller mappings, or another routing layer; these functions are deliberately input-system agnostic.

If you drag directly from the inherited **Building component reference** instead of calling the Character convenience wrappers, the component-native node names are:

- `Confirm Preview Placement` — same underlying placement confirm action.
- `Rotate Preview` — same underlying rotation action.
- `Select Next Build Piece` / `Select Previous Build Piece` — catalogue cycling while in build mode.
- `End Build Mode` — cancel placement and return directly to gameplay without reopening the Build Menu.

A useful control split is **B = reopen/toggle Build Menu** and **C/Escape = Building → End Build Mode** for a clean return to gameplay.

---

### Safe multiplayer defaults

The inherited `Building` component ships with conservative authority defaults:

- **Allow Unlisted Build Requests = false** — the server only accepts Build Piece Definitions that are actually present in that character's `Build Catalog`. A client cannot submit an arbitrary Build Piece asset by reference.
- **Require Snap Target Modification Access = true** — snapping a new structure to an existing runtime build requires modification rights on that target. This prevents a player from extending another player's private structure just because they can reach one of its snap sockets.

Projects that intentionally use a dynamic/public building model can expose different policy, but keeping these defaults is recommended for normal multiplayer settlements.

---

## 7. Placement preview

When build mode starts the framework spawns a local-only `ARPGBuildPreviewActor`.

It:

- never replicates
- has no collision
- uses Preview Mesh or Build Mesh fallback
- follows the player view trace
- uses structural snapping when a compatible piece is close
- reflects resource/support/collision/territory validity
- exposes generic material parameters `PreviewOpacity`, `PreviewTint`, `PlacementValid`
- supports optional explicit Valid/Invalid Preview Material overrides on the `Building` component

The placement HUD shows selected piece, cost, current placement status and controls.

---

## 8. Timed construction presentation

Every placed `ARPGBuildPieceActor` replicates construction state using synchronized server time.

`Construction Seconds = 0` finishes immediately.

For timed structures:

1. resources are committed on successful authoritative placement
2. the replicated structure is spawned
3. its mesh begins at `Construction Start Scale Z`
4. the visible mesh grows upward over the configured duration
5. optional `ConstructionProgress` / `BuildProgress` material scalar advances 0→1
6. collision is disabled during construction by default
7. the piece becomes fully interactive/supporting when complete

The actor only ticks while construction is active.

Construction progress is included in the world save. Saving/reloading a structure that had 4 seconds remaining restores those remaining seconds rather than restarting from zero or instantly completing it.

---

## 9. Functional doors

For a Door definition:

- Piece Kind: `Door`
- Requires Snap Target: true
- Build Mesh: door mesh
- leave Actor Class empty

A Doorway automatically exposes the standard Door snap.

The native `ARPGBuildDoorActor` provides:

- replicated open/closed state
- faction/ownership access checks
- configurable open yaw
- smooth transition duration
- optional auto close
- save/load of open state
- Tick only during door movement

For unusual pivots/hinges, make a Blueprint child of `ARPGBuildDoorActor` and adjust the component/pivot arrangement or exposed settings.

---

## 10. Functional storage container

Create another `ARPGBuildPieceDefinition`:

- Piece Kind: `Storage`
- Build Mesh: chest/container mesh
- Storage Slots: e.g. `48`
- Build Cost: e.g. Wood ×8
- Actor Class: empty

The framework automatically spawns `ARPGStorageActor`.

After construction, look at the container and call `Interact Built Structure`.

The ready native Storage UI opens with:

**PLAYER INVENTORY** | **STORAGE**

Each runtime row has transfer-one and transfer-all controls. Transfers go through the player-owned authoritative Interaction RPC.

v2.15 transfers the **exact clicked runtime InstanceId**, so two damaged swords with different durability cannot be confused merely because they share an ItemId. Durable/bound runtime state remains intact through storage.

Storage contents and ownership are already part of world persistence.

---

## 11. Build a working Wood-fuel Furnace

The Furnace is simply a Production build piece + Crafting Station Definition + station-only recipe.

### A. Wood fuel

On `DA_Item_Wood` add Item Tag:

`Item.Fuel.Wood`

### B. Metal Ore and Metal Ingot

Create:

- `DA_Item_MetalOre`
- `DA_Item_MetalIngot`

### C. Furnace recipe

Create `ARPGRecipeDefinition`:

`DA_Recipe_SmeltMetalIngot`

Suggested setup:

**Inputs**
- Metal Ore ×2

**Outputs**
- Metal Ingot ×1

**Requirements**
- Required Station Tag: `Station.Furnace`

**Timing**
- Craft Seconds: e.g. `5`

**Station Fuel**
- Consumes Fuel: true
- Fuel Tag: `Item.Fuel.Wood`
- Fuel Per Craft: `1`

### D. Furnace station definition

Create `ARPGCraftingStationDefinition`:

`DA_Station_Furnace`

Set:

- Station Tag: `Station.Furnace`
- Recipes: add `DA_Recipe_SmeltMetalIngot`
- Input Slots: e.g. `16`
- Output Slots: e.g. `16`
- Use Station Inventory For Inputs: true
- Fuel Comes From Station Inventory: true
- Process While Offline: project preference

### E. Furnace build piece

Create `ARPGBuildPieceDefinition`:

`DA_Build_Furnace`

Set:

- Piece Kind: `Production`
- Build Category: `Production`
- Build Mesh: furnace mesh
- Build Cost: Stone ×15, Wood ×5
- Station Definition: `DA_Station_Furnace`
- Construction Seconds: e.g. `4`
- Actor Class: empty

Add it to the player's `Building -> Build Catalog`.

### F. Runtime flow

1. Build Furnace.
2. Wait for construction to complete if timed.
3. Look at Furnace and call `Interact Built Structure`.
4. Transfer Metal Ore from Player → `INPUT + FUEL`.
5. Transfer Wood from Player → `INPUT + FUEL`.
6. Press the Metal Ingot recipe row.
7. Station queues the recipe on authority.
8. One fuel unit is consumed per successful craft completion.
9. Metal Ingot appears under `OUTPUT`.
10. Transfer output back to Player.
11. Metal Ingot can now be consumed by Metal building-piece Build Costs or ordinary Crafting recipes.

Station recipe validation is strict: a recipe requiring `Station.Furnace` cannot run on an untagged/wrong station. Duplicate ingredient lines are aggregated and input/fuel/output mutations are rollback-safe against unexpected partial failures.

---

## 12. Ready UI classes / reskinning

Select inherited **BuildingUI** on the player Blueprint.

It exposes:

- Build Menu Widget Class
- Build Piece Row Widget Class
- Placement HUD Widget Class
- Storage Widget Class
- Crafting Station Widget Class
- Structure Item Row Widget Class
- Station Recipe Row Widget Class

Native ready classes:

- `UARPGBuildMenuWidget`
- `UARPGBuildPieceRowWidget`
- `UARPGBuildPlacementHUDWidget`
- `UARPGStoragePanelWidget`
- `UARPGCraftingStationPanelWidget`
- `UARPGStructureItemRowWidget`
- `UARPGStationRecipeRowWidget`

Create Widget Blueprint subclasses of those classes and select them on `BuildingUI`. Standard named child bindings and `BP_On...Updated/Refreshed` events let you replace the visual design while retaining the native logic.

The ready panels use no permanent UI Tick. The production UI uses a short timer only while open to animate live craft progress; inventory/queue events rebuild content.

---

## 13. Multiplayer authority

Local client:

- build menu
- ghost preview
- placement HUD
- storage/furnace presentation

Authority/server:

- re-resolves snap transform
- validates distance/collision/support/faction/territory
- validates and consumes Build Cost
- spawns replicated structure
- owns construction completion
- owns doors
- owns storage transfers
- owns furnace inputs/fuel/output/queue
- owns demolition/refund

The server does not trust the preview's green/red result or an arbitrary client transform.

---

## 14. Persistence

World save schema v5 stores:

- stable Building ID
- definition ID / actor class
- transform
- health / upgrade level
- owner account/character/faction
- construction complete state / remaining build seconds
- door open state
- storage contents
- furnace/station input contents
- output contents
- craft queue/progress timestamps

Offline station elapsed time can resume when enabled. Output capacity and fuel requirements can still block completion instead of inventing resources.

---

## 15. Performance rules

The ready implementation avoids permanent ticking:

- `BuildingComponent` ticks only while local build mode is active.
- placement preview actor has no Tick.
- build piece ticks only while timed construction is incomplete.
- door ticks only while opening/closing.
- production station ticks only while it has queue work.
- production UI progress timer exists only while that UI is open.

Completed static structures therefore do not each carry a permanent framework gameplay Tick.
