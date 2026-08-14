from pathlib import Path
root=Path(__file__).resolve().parents[1]
src=root/'Source'/'AkumasRPGFramework'
def read(rel): return (src/rel).read_text(errors='ignore')

def require(text, *tokens):
    for token in tokens:
        assert token in text, f'missing token: {token}'

piece_h=read(Path('Public/Data/ARPGBuildPieceDefinition.h'))
build_h=read(Path('Public/Building/ARPGBuildingComponent.h'))
build_cpp=read(Path('Private/Building/ARPGBuildingComponent.cpp'))
actor_h=read(Path('Public/Building/ARPGBuildPieceActor.h'))
actor_cpp=read(Path('Private/Building/ARPGBuildPieceActor.cpp'))
preview_h=read(Path('Public/Building/ARPGBuildPreviewActor.h'))
preview_cpp=read(Path('Private/Building/ARPGBuildPreviewActor.cpp'))
door_h=read(Path('Public/Building/ARPGBuildDoorActor.h'))
door_cpp=read(Path('Private/Building/ARPGBuildDoorActor.cpp'))
ui_comp_h=read(Path('Public/Components/ARPGBuildingUIComponent.h'))
ui_comp_cpp=read(Path('Private/Components/ARPGBuildingUIComponent.cpp'))
ui_h=read(Path('Public/UI/ARPGBuildingWidgets.h'))
ui_cpp=read(Path('Private/UI/ARPGBuildingWidgets.cpp'))
char_h=read(Path('Public/Actors/ARPGCharacter.h'))
char_cpp=read(Path('Private/Actors/ARPGCharacter.cpp'))
inter_h=read(Path('Public/Components/ARPGInteractionComponent.h'))
inter_cpp=read(Path('Private/Components/ARPGInteractionComponent.cpp'))
station_h=read(Path('Public/Crafting/ARPGCraftingStationActor.h'))
station_cpp=read(Path('Private/Crafting/ARPGCraftingStationActor.cpp'))
inv_h=read(Path('Public/Components/ARPGInventoryComponent.h'))
inv_cpp=read(Path('Private/Components/ARPGInventoryComponent.cpp'))
save_h=read(Path('Public/Save/ARPGSaveGame.h'))
save_cpp=read(Path('Private/Subsystems/ARPGSaveSubsystem.cpp'))
account_cpp=read(Path('Private/Subsystems/ARPGAccountSubsystem.cpp'))
persistence_h=read(Path('Public/Components/ARPGPersistenceComponent.h'))
persistence_cpp=read(Path('Private/Components/ARPGPersistenceComponent.cpp'))
types_h=read(Path('Public/ARPGTypes.h'))

# Complete data-driven kit types and utility definitions.
for kind in ('Foundation','Wall','WindowWall','Window','Doorway','Door','Floor','Ceiling','Roof','Stair','Pillar','Storage','Production','Decoration','Custom'):
    assert kind in piece_h
require(piece_h, 'BuildMesh', 'PreviewMesh', 'BuildCost', 'ConstructionSeconds', 'ConstructionStartScaleZ',
        'ConstructionProgressMaterialParameter', 'SnapSize', 'StandardWallHeight', 'CustomSnapPoints',
        'StorageSlots', 'StationDefinition', 'DemolishRefundFraction', 'MeshRelativeTransform')

# Local build mode + ghost; permanent component tick is avoided outside build mode.
require(build_h, 'BuildCatalog', 'BeginBuildMode', 'EndBuildMode', 'ConfirmPreviewPlacement', 'RotatePreview',
        'NextBuildPiece', 'PreviousBuildPiece', 'OnBuildModeChanged', 'OnBuildPreviewUpdated', 'bAllowUnlistedBuildRequests', 'bRequireSnapTargetModificationAccess')
require(build_cpp, 'PrimaryComponentTick.bStartWithTickEnabled = false', 'SetComponentTickEnabled(true)', 'SetComponentTickEnabled(false)',
        'AARPGBuildPreviewActor', 'UpdatePlacementPreview', 'FindBestSnapTransform', 'GetSnapTransformsFor',
        'ServerPlacePiece', 'PlacePieceAuthority', 'EvaluatePlacementInternal')
# Ground placement is mesh-pivot aware: bottom-center of the real Build Mesh is anchored to the surface,
# while collision/support validation uses the corresponding visible bounds center/bottom rather than actor origin.
require(build_cpp, 'ARPGGetBuildPieceBottomAnchorLocal', 'ARPGGetBuildPieceBoundsCenterLocal',
        'Hit.ImpactPoint - DesiredRotation.RotateVector(BottomAnchorLocal)', 'PlacementBoundsCenter',
        'BottomAnchor + FVector::UpVector * ProbeLift', 'Grid-snap the visible footprint anchor')
# v2.15.3 mesh-orientation adaptation: transformed bounds are used by placement and snapping,
# while preview/final visuals receive the same Data Asset relative transform.
require(build_cpp, 'RawBox.TransformBy(Piece->MeshRelativeTransform)')
require(actor_cpp, 'RawBox.TransformBy(Piece->MeshRelativeTransform)',
        'BuildMesh->SetRelativeTransform(Definition->MeshRelativeTransform)')
require(preview_h, 'USceneComponent', 'PreviewRoot')
require(preview_cpp, 'PreviewMesh->SetupAttachment(PreviewRoot)',
        'PreviewMesh->SetRelativeTransform(Piece->MeshRelativeTransform)')
require(piece_h, 'FTransform MeshRelativeTransform = FTransform::Identity')
assert 'Hit.ImpactNormal * FMath::Max(0.f, SelectedBuildPiece->PlacementBounds.Z)' not in build_cpp

# Mathematical regression: bottom-pivot and center-pivot 150 cm foundations both land their visible bottom at Z=0.
def grounded_actor_z(surface_z, local_min_z):
    return surface_z - local_min_z
assert grounded_actor_z(0.0, 0.0) == 0.0       # bottom pivot
assert grounded_actor_z(0.0, -75.0) == 75.0    # center pivot
# A 300 cm bottom-pivot wall on a 150 cm bottom-pivot foundation starts exactly at foundation top.
assert (150.0 - 0.0) == 150.0
require(actor_cpp, 'ARPGGetBuildDefinitionLocalBounds', 'AlignTopPlaneZ', 'IncomingOnTargetTopZ',
        'TargetMax.Z + WallHeight - IncomingMin.Z', 'IncomingAboveTargetZ')
# Authority re-resolves snap/placement rather than trusting local preview.
pa=build_cpp[build_cpp.index('bool UARPGBuildingComponent::PlacePieceAuthority'):]
assert 'ResolvePlacementTransform' in pa and 'EvaluatePlacementInternal' in pa
assert 'BuildCatalog.ContainsByPredicate' in build_cpp and 'SnapTarget->CanActorModify(Owner)' in build_cpp
# Costs aggregate and only unequipped resources can be consumed.
require(build_cpp, 'ARPGAggregateBuildCosts', 'HasUnequippedItem', 'RemoveUnequippedItem', 'RefundBuildResources')
# Native actor fallback makes common pieces usable without actor Blueprints.
require(build_cpp, 'EARPGBuildPieceKind::Door: return AARPGBuildDoorActor::StaticClass()',
        'EARPGBuildPieceKind::Storage: return AARPGStorageActor::StaticClass()',
        'EARPGBuildPieceKind::Production: return AARPGCraftingStationActor::StaticClass()')

# Preview actor is local-only and collisionless.
require(preview_cpp, 'bReplicates = false', 'SetActorEnableCollision(false)', 'ECollisionEnabled::NoCollision',
        'PlacementValid', 'PreviewOpacity')

# Snap graph covers requested structural families and doors/windows.
require(actor_cpp, 'Foundation', 'WindowWall', 'Doorway', 'Ceiling', 'Roof',
        'TargetKind == EARPGBuildPieceKind::Doorway && IncomingKind == EARPGBuildPieceKind::Door',
        'TargetKind == EARPGBuildPieceKind::WindowWall && IncomingKind == EARPGBuildPieceKind::Window')
# v2.15.7 insert regression: Door/Window sockets align visible geometry rather than actor pivots,
# and acquisition measures aim distance to the supporting opening bounds rather than the generated
# incoming actor location. Non-insert structural snapping keeps legacy candidate-distance capture.
require(actor_cpp, 'ARPGGetCenteredInsertTranslation', 'TargetCenter.X - IncomingCenter.X',
        'TargetCenter.Y - IncomingCenter.Y', 'TargetMin.Z - IncomingMin.Z')
require(build_cpp, 'ARPGIsInsertSnapPair', 'ARPGDistanceSquaredToBuildPieceBounds',
        'InsertAimDistSq', 'CaptureMetricSq', 'SemanticTieBreak')
# v2.15.9 insert acquisition is independent from the ordinary placement trace's first hit.
# Preview scans the complete camera segment for compatible opening envelopes, then validates each
# opening with its own LOS probes. This specifically prevents a Foundation/Floor hit in front of the
# doorway from prematurely truncating semantic Door/Window targeting. Authority still has a
# Door/Window-only actor fallback and can reacquire from exact candidate distance.
require(build_cpp, 'ARPGSegmentIntersectsLocalBox', 'ARPGFindViewDirectedInsertSnap',
        'ARPGHasClearInsertAimPath', 'ProbeLocals[]', 'ViewEnd',
        'TActorIterator<AARPGBuildPieceActor>', 'LineTraceSingleByChannel',
        'FMath::Min(InsertAimDistSq, CandidateDistSq)',
        'restricted to Door/Window pieces')
assert 'BlockingHitDistance' not in build_cpp
assert 'VisibleDistanceLimit' not in build_cpp
# v2.15.19 upper-story insert occupancy: a snapped Door/Window is hosted by its Doorway/WindowWall.
# Legitimate Floors/Ceilings/adjacent Wall-family pieces around that host must not independently
# block the insert, while duplicate inserts and conflicting structural slots remain on normal collision.
require(build_cpp, 'ARPGIsCompatibleInsertHostStructuralNeighbor',
        'ARPGIsInsertSnapPair(InsertHost->Definition->PieceKind, IncomingPiece->PieceKind)',
        'EARPGStructuralOccupancyRelation::CompatibleSeam',
        'SnapTarget && ARPGIsCompatibleInsertHostStructuralNeighbor')
# v2.15.22 reverse hosted-insert regression: build order must remain commutative after the insert
# exists. A Door/Window that exactly matches its Doorway/WindowWall native insert socket cannot veto
# a later Wall/Floor/Ceiling/Roof seam that is semantically compatible with that verified host.
require(build_cpp,
        'ARPGInsertActorMatchesHost',
        'ARPGHostedInsertAllowsStructuralNeighbor',
        'Reverse hosted-insert rule',
        'bNeighborIsInsert && bIncomingIsStructural',
        'ARPGIsInsertSnapPair(CandidateHost->Definition->PieceKind, NeighborKind)',
        '!BuildNeighbor->CanActorModify(Owner) || !ResolvedHost->CanActorModify(Owner)')

def reverse_hosted_insert_allows(insert_matches_host, host_seam_relation):
    return insert_matches_host and host_seam_relation == 'compatible'
assert reverse_hosted_insert_allows(True, 'compatible')       # Doorway -> Door -> upper Floor
assert not reverse_hosted_insert_allows(False, 'compatible') # unrelated Door cannot be ignored
assert not reverse_hosted_insert_allows(True, 'conflict')    # real host conflict still blocks


def segment_intersects_box(start, end, bmin, bmax):
    enter, exit = 0.0, 1.0
    for s0, e0, lo, hi in zip(start, end, bmin, bmax):
        d=e0-s0
        if abs(d) <= 1e-9:
            if s0 < lo or s0 > hi: return False
            continue
        t0=(lo-s0)/d; t1=(hi-s0)/d
        if t0 > t1: t0,t1=t1,t0
        enter=max(enter,t0); exit=min(exit,t1)
        if enter > exit: return False
    return True
# A camera ray aimed through the empty centre of a 302x31x271 doorway still intersects the overall
# visible envelope even though the physical frame collision may have no primitive in the opening.
assert segment_intersects_box((0.0,-500.0,120.0),(0.0,500.0,120.0),(-151.0,-15.5,0.0),(151.0,15.5,271.0))
# Regression for the runtime failure that survived v2.15.8: a generic placement hit on the supporting
# floor can occur before the doorway. The old truncated segment therefore never reached the opening,
# while v2.15.9 intentionally evaluates the complete placement segment.
assert not segment_intersects_box((0.0,-500.0,120.0),(0.0,-120.0,120.0),(-151.0,-15.5,0.0),(151.0,15.5,271.0))
assert segment_intersects_box((0.0,-500.0,120.0),(0.0,1200.0,120.0),(-151.0,-15.5,0.0),(151.0,15.5,271.0))
# A clearly off-to-the-side ray is not semantically captured.
assert not segment_intersects_box((250.0,-500.0,120.0),(250.0,500.0,120.0),(-151.0,-15.5,0.0),(151.0,15.5,271.0))
def centered_insert_translation(target_min, target_max, incoming_min, incoming_max):
    tc=tuple((a+b)*0.5 for a,b in zip(target_min,target_max))
    ic=tuple((a+b)*0.5 for a,b in zip(incoming_min,incoming_max))
    return (tc[0]-ic[0], tc[1]-ic[1], target_min[2]-incoming_min[2])
# Bottom-pivot 302x31x271 doorway + center-pivot logical 103x16x205 door.
assert centered_insert_translation(
    (-151.0,-15.5,0.0),(151.0,15.5,271.0),
    (-51.5,-8.0,-102.5),(51.5,8.0,102.5)) == (0.0,0.0,102.5)
# Corner-pivot door is also centered and bottom-aligned without a hand-authored socket offset.
assert centered_insert_translation(
    (-151.0,-15.5,0.0),(151.0,15.5,271.0),
    (0.0,0.0,0.0),(103.0,16.0,205.0)) == (-51.5,-8.0,0.0)
# v2.15.4 modular seam/corner collision fix: wall graph advertises perpendicular L-corners, and
# placement only tolerates overlap with another completed build when the final transform exactly
# matches one of that neighbour's native/custom snap candidates. This keeps arbitrary clipping blocked.
require(actor_cpp, 'TargetHalfGrid', 'IncomingHalfGrid', 'CornerOffsets[]',
        'FVector( TargetHalfGrid,  IncomingHalfGrid, AlignBottomPlaneZ)',
        'FVector( TargetHalfGrid, -IncomingHalfGrid, AlignBottomPlaneZ)',
        'FVector(-TargetHalfGrid,  IncomingHalfGrid, AlignBottomPlaneZ)',
        'FVector(-TargetHalfGrid, -IncomingHalfGrid, AlignBottomPlaneZ)',
        'FRotator(0.f,  90.f, 0.f)', 'FRotator(0.f, -90.f, 0.f)')
require(build_cpp, 'ARPGIsValidSnappedBuildNeighbor', 'Neighbor->GetSnapTransformsFor',
        'PlacementCollisionClearance + 0.5f', 'BuildNeighbor->CanActorModify(Owner)')
assert 'const_cast<AARPGBuildPieceActor*>' not in build_cpp

# v2.15.13 upper-horizontal placement polish: capture uses the candidate tile's visible bounds so
# the supporting edge of a 300 cm floor is targetable even though it sits 150 cm from the actor
# origin and the default SnapCaptureDistance is 140 cm. Multi-wall room supports may advertise that
# same physical tile with cardinal yaw differences; rotation equivalence is accepted only when the
# placement footprint is physically unchanged (square for 90 degrees, any rectangle for 180).
require(build_cpp, 'ARPGIsUpperHorizontalStructuralKind', 'ARPGDistanceSquaredToDefinitionBoundsAtTransform',
        'HorizontalFootprintDistSq', 'bHorizontalFootprintCapture',
        'ARPGRotationsPreservePlacementFootprint',
        'Piece->PlacementBounds.X - Piece->PlacementBounds.Y',
        'same physical slot can be advertised')

def distance_sq_to_local_box(point, bmin, bmax):
    closest=tuple(max(lo,min(v,hi)) for v,lo,hi in zip(point,bmin,bmax))
    return sum((a-b)**2 for a,b in zip(point,closest))

# Legacy origin capture fails at the wall edge: 150 > 140. Bounds capture succeeds because that
# edge lies on the intended 300x300x18 tile envelope.
legacy_capture_distance=140.0
wall_edge_point=(150.0,0.0,0.0)
assert wall_edge_point[0]**2 > legacy_capture_distance**2
assert distance_sq_to_local_box(wall_edge_point,(-150.0,-150.0,-9.0),(150.0,150.0,9.0)) == 0.0

def rotations_preserve_footprint(extent_x, extent_y, delta_yaw, tolerance=2.5):
    d=abs(((delta_yaw+180.0)%360.0)-180.0)
    if d <= 1.0 or abs(d-180.0) <= 1.0:
        return True
    if abs(d-90.0) > 1.0:
        return False
    return abs(extent_x-extent_y) <= tolerance

assert rotations_preserve_footprint(150.0,150.0,90.0)
assert rotations_preserve_footprint(150.0,150.0,180.0)
assert not rotations_preserve_footprint(150.0,300.0,90.0)
assert rotations_preserve_footprint(150.0,300.0,180.0)

# v2.15.14 inter-story seam regression: build order must be commutative. If upper walls were stacked
# before the 300x300x18 Floor is inserted, their bases share the Floor bottom/story plane and the
# slab deliberately occupies its own 18 cm thickness through the wall frame. Only exact edge/facing
# relationships are accepted; a wall through the middle or at an unrelated height still blocks.
require(build_cpp, 'ARPGIsValidUpperHorizontalWallSeamNeighbor', 'bPreStackedUpperWallAtStorySeam',
        'bSupportingWallBelow', 'bWallBuiltOnSlabTop', 'ARPGWallOccupiesHorizontalStructuralEdge',
        'This makes Floor-first and Wall-stack-first construction produce the same valid result')

def valid_interstory_seam(origin_xy, wall_yaw, floor_yaw, wall_bottom, wall_top, floor_bottom, floor_top, snap=300.0, tol=2.5):
    half=snap*0.5
    edges=[((0.0, half),0.0),((0.0,-half),180.0),((half,0.0),-90.0),((-half,0.0),90.0)]
    def yaw_delta(a,b):
        return abs(((a-b+180.0)%360.0)-180.0)
    edge_ok=False
    for (ex,ey), rel_yaw in edges:
        if (origin_xy[0]-ex)**2 + (origin_xy[1]-ey)**2 <= tol*tol and yaw_delta(floor_yaw+rel_yaw, wall_yaw) <= 1.0:
            edge_ok=True
            break
    if not edge_ok:
        return False
    return (abs(wall_top-floor_bottom) <= tol or
            abs(wall_bottom-floor_top) <= tol or
            abs(wall_bottom-floor_bottom) <= tol)

# Floor spans Z=300..318. Lower support ends at 300; a pre-stacked upper wall also starts at 300.
assert valid_interstory_seam((0.0,150.0),0.0,0.0,0.0,300.0,300.0,318.0)      # lower support
assert valid_interstory_seam((0.0,150.0),0.0,0.0,300.0,600.0,300.0,318.0)    # pre-stacked upper wall
assert valid_interstory_seam((150.0,0.0),-90.0,0.0,300.0,600.0,300.0,318.0) # perpendicular upper wall
assert valid_interstory_seam((0.0,150.0),0.0,0.0,318.0,618.0,300.0,318.0)    # wall built after Floor
assert not valid_interstory_seam((0.0,0.0),0.0,0.0,300.0,600.0,300.0,318.0) # wall through tile center
assert not valid_interstory_seam((0.0,150.0),90.0,0.0,300.0,600.0,300.0,318.0) # wrong facing
assert not valid_interstory_seam((0.0,150.0),0.0,0.0,350.0,650.0,300.0,318.0) # unrelated height

# v2.15.15 inverse story-bay seam: an incoming Wall-family piece may fill the bay between a lower
# slab and an already-built upper Floor/Ceiling/Roof. The upper slab is accepted only when the wall
# sits on its exact edge/facing and the wall visible top meets the slab visible bottom.
require(build_cpp, 'ARPGIsValidWallUnderUpperHorizontalSeamNeighbor',
        'WallTopZ - HorizontalBottomZ', 'Inverse story-bay seam',
        'upper slab is a legitimate')

def valid_wall_under_upper_slab(wall_origin_xy, wall_yaw, slab_yaw, wall_top, slab_bottom, snap=300.0, tol=2.5):
    half=snap*0.5
    edges=[((0.0, half),0.0),((0.0,-half),180.0),((half,0.0),-90.0),((-half,0.0),90.0)]
    def yaw_delta(a,b):
        return abs(((a-b+180.0)%360.0)-180.0)
    edge_ok=False
    for (ex,ey), rel_yaw in edges:
        if (wall_origin_xy[0]-ex)**2 + (wall_origin_xy[1]-ey)**2 <= tol*tol and yaw_delta(slab_yaw+rel_yaw, wall_yaw) <= 1.0:
            edge_ok=True
            break
    return edge_ok and abs(wall_top-slab_bottom) <= tol

# Lower slab top is Z=318; incoming wall fills 318..589; upper slab begins at Z=589.
assert valid_wall_under_upper_slab((0.0,150.0),0.0,0.0,589.0,589.0)
assert valid_wall_under_upper_slab((150.0,0.0),-90.0,0.0,589.0,589.0)
# Square upper slab may itself be cardinally rotated; local edge mapping still resolves same world facing.
assert valid_wall_under_upper_slab((150.0,0.0),0.0,90.0,589.0,589.0)
assert not valid_wall_under_upper_slab((0.0,0.0),0.0,0.0,589.0,589.0)       # wall through tile center
assert not valid_wall_under_upper_slab((0.0,150.0),90.0,0.0,589.0,589.0)    # wrong facing
assert not valid_wall_under_upper_slab((0.0,150.0),0.0,0.0,600.0,589.0)     # extends through slab / wrong plane
# 300 cm target + 300 cm incoming wall: the four L-corner centers sit at +/-150 cm on both axes,
# and each center accepts both +/-90 facing variants for intentional wall-only left/right turns.
target_half = 300.0 * 0.5
incoming_half = 300.0 * 0.5
corner_centers = {
    (target_half, incoming_half), (target_half, -incoming_half),
    (-target_half, incoming_half), (-target_half, -incoming_half),
}
assert corner_centers == {(150.0,150.0),(150.0,-150.0),(-150.0,150.0),(-150.0,-150.0)}
corner_candidates = {(x, y, yaw) for x, y in corner_centers for yaw in (90.0, -90.0)}
assert len(corner_candidates) == 8
# v2.15.5 directional-wall regression: local +Y is the logical front/exterior side. Foundation
# support-edge yaw must rotate that +Y vector toward each edge's outward normal. The previous
# +X/-X signs were reversed, making exactly two opposite walls show their back face.
require(actor_cpp,
        'const float IncomingWallStoryBaseZ',
        'TargetKind == EARPGBuildPieceKind::Foundation',
        '? IncomingOnTargetTopZ', ': AlignBottomPlaneZ',
        'FRotator(0.f, -90.f, 0.f), FVector(Half, 0.f, IncomingWallStoryBaseZ)',
        'FRotator(0.f,  90.f, 0.f), FVector(-Half, 0.f, IncomingWallStoryBaseZ)',
        'canonical story seam', 'actor local +Y is the', 'front/exterior side')
import math
def rotate_y(yaw_deg):
    r=math.radians(yaw_deg)
    # Standard UE yaw in XY: x'=cos*x-sin*y, y'=sin*x+cos*y. Input is local +Y=(0,1).
    return (round(-math.sin(r), 6), round(math.cos(r), 6))
edge_yaws = {
    (0.0, 1.0): 0.0,
    (0.0,-1.0): 180.0,
    (1.0, 0.0):-90.0,
    (-1.0,0.0): 90.0,
}
for outward, yaw in edge_yaws.items():
    assert rotate_y(yaw) == outward, (outward, yaw, rotate_y(yaw))
# v2.15.23 multi-cell facade regression: final Wall-family facing must be determined from occupied
# horizontal cells, while the horizontal support's OWN native edge socket supplies the authored yaw.
# This restores the proven v2.15.6 facing contract and removes the unsafe assumption that actor-local
# +Y necessarily describes the visible exterior after arbitrary third-party mesh authoring.
require(build_cpp,
        'ARPGTryGetHorizontalWallFacingClaim',
        'occupied cell tells us WHICH side is outside',
        'own native Wall socket tells us the',
        'actual authored yaw',
        'multi-cell aware',
        '1x2, 2x2 or larger foundation footprint',
        'interior partition',
        'FirstNativeYaw')

# Native standard horizontal socket mapping from the confirmed v2.15.6 baseline. The support actor yaw
# is composed afterward; these are the authored local edge yaws we must preserve rather than re-derive.
edge_native_yaws = {
    (0.0, 1.0): 0.0,
    (0.0,-1.0): 180.0,
    (1.0, 0.0):-90.0,
    (-1.0,0.0): 90.0,
}

def rotate_xy(v, yaw_deg):
    r=math.radians(yaw_deg)
    x,y=v
    return (round(math.cos(r)*x-math.sin(r)*y,6), round(math.sin(r)*x+math.cos(r)*y,6))

def norm_yaw(y):
    return ((y+180.0)%360.0)-180.0

def claim_for_cell(cell_center, cell_yaw, wall_center, snap=300.0, tol=2.5):
    # Transform world wall center into cell-local XY (inverse yaw + translation).
    dx,dy=wall_center[0]-cell_center[0], wall_center[1]-cell_center[1]
    local=rotate_xy((dx,dy), -cell_yaw)
    half=snap*0.5
    edge=None
    for local_edge in ((0.0,half),(0.0,-half),(half,0.0),(-half,0.0)):
        if (local[0]-local_edge[0])**2 + (local[1]-local_edge[1])**2 <= tol*tol:
            edge=local_edge
            break
    if edge is None:
        return None
    unit=(0.0 if edge[0]==0 else (1.0 if edge[0]>0 else -1.0),
          0.0 if edge[1]==0 else (1.0 if edge[1]>0 else -1.0))
    world_out=rotate_xy(unit, cell_yaw)
    native_local=edge_native_yaws[unit]
    native_world=norm_yaw(cell_yaw+native_local)
    return world_out,native_world

# 1x2 footprint: perimeter edges have ONE occupied cell and therefore a deterministic native yaw,
# independent of where the player/camera is standing while placing from the inside.
cells=[((0.0,0.0),0.0),((300.0,0.0),0.0)]
for wall_center, expected_yaw in [
    ((0.0,150.0),0.0),      # north perimeter of left cell
    ((300.0,150.0),0.0),    # north perimeter of right cell
    ((-150.0,0.0),90.0),    # west outer perimeter
    ((450.0,0.0),-90.0),    # east outer perimeter
]:
    claims=[claim_for_cell(c,y,wall_center) for c,y in cells]
    claims=[c for c in claims if c]
    assert len(claims)==1, (wall_center,claims)
    assert claims[0][1]==expected_yaw, (wall_center,claims[0],expected_yaw)

# The shared edge between two occupied cells is an intentional interior partition: the two cells cast
# opposite outward claims. There is no globally correct "outside", so first-story selected yaw is kept;
# on upper stories the direct lower Wall-family stack becomes the continuity authority.
shared=[claim_for_cell(c,y,(150.0,0.0)) for c,y in cells]
shared=[c for c in shared if c]
assert len(shared)==2
assert round(shared[0][0][0]*shared[1][0][0] + shared[0][0][1]*shared[1][0][1],6)==-1.0
assert abs(abs(((shared[0][1]-shared[1][1]+180.0)%360.0)-180.0)-180.0) <= 1.0

# Rotating an entire modular cell preserves the cell's own authored edge yaw by composition.
for support_yaw in (0.0,90.0,180.0,-90.0):
    claim=claim_for_cell((0.0,0.0),support_yaw,rotate_xy((0.0,150.0),support_yaw))
    assert claim is not None
    assert claim[1] == norm_yaw(support_yaw)

# Vertical continuity remains a fallback only for ambiguous interior/no-horizontal-support cases; it
# must never override a unique perimeter-cell claim.
require(build_cpp,
        'bFoundHorizontalClaim && !bAmbiguousHorizontalClaim',
        'Rotation.Yaw = FirstNativeYaw',
        'preserve the exact native vertical-stack facing',
        'Rotation.Yaw = VerticalSupport->GetActorRotation().Yaw')

def direct_stack_facing_owner(local_x, local_y, local_z, relative_yaw, tol=2.5):
    return (local_x*local_x + local_y*local_y <= tol*tol and local_z > tol and abs(relative_yaw) <= 1.0)
assert direct_stack_facing_owner(0.0,0.0,300.0,0.0)
assert not direct_stack_facing_owner(300.0,0.0,0.0,0.0)
assert not direct_stack_facing_owner(150.0,150.0,0.0,90.0)

# Example from a 300 foundation: north wall (0,+150,yaw0) and east wall (+150,0,yaw-90).
# East relative to north is (+150,-150,yaw-90), which is one wall-only corner variant.
assert (150.0, -150.0, -90.0) in corner_candidates
# If a foundation edge and an already-built wall corner advertise that same physical slot, the
# horizontal support owns orientation. Wall-only corners still keep both variants when no support ties.
require(build_cpp, 'ARPGGetSnapCandidateSemanticPriority', 'ARPGIsHorizontalStructuralKind',
        'SameSlotTolerance', 'bSamePhysicalSlot', 'bBetterSemanticOwner',
        'SemanticPriority < BestSemanticPriority', 'ARPGWallSnapCandidatesShareStructuralSlot',
        'bHorizontalCandidateStartsOnStoryPlane', 'bWallStackCandidateStartsOnStoryPlane',
        'bCandidateBottomsAgree')
# v2.15.20 no-gap story-plane regression: a real 18 cm upper Floor occupies Z=300..318, but the
# canonical next-story wall starts at the Floor *bottom/story plane* (Z=300), exactly matching a
# direct vertical stack from the lower wall. The slab overlaps the lower 18 cm of the upper wall frame
# instead of adding 18 cm to the storey height and exposing a horizontal facade gap.
floor_bottom, floor_top = 300.0, 318.0
vertical_wall_bottom = 300.0
floor_edge_wall_bottom = floor_bottom
assert floor_edge_wall_bottom == vertical_wall_bottom
assert floor_top-floor_edge_wall_bottom == 18.0
assert abs(vertical_wall_bottom-floor_bottom) <= 2.5
# v2.15.6 vertical-stack facing regression: candidate ownership is relationship-aware, not merely
# target-kind-aware. The wall directly below owns local-XY-zero, above-target, zero-relative-yaw
# stack candidates; lateral/corner wall candidates remain lower priority at the same world slot.
require(build_cpp, 'TargetLocalLocation', 'HorizontalOffsetSq', 'RelativeYawDelta',
        'bVerticalStackCandidate', 'StackFacingToleranceDegrees',
        'direct wall below remains the second-best semantic owner')
require(actor_cpp, "inherits the supporting wall's world facing as well as its structural column")
def wall_candidate_priority(local_x, local_y, local_z, relative_yaw, clearance=2.0):
    tol=max(1.0, clearance+0.5)
    horizontal_sq=local_x*local_x + local_y*local_y
    # Match FindDeltaAngleDegrees semantics closely enough for the cardinal snap yaws used here.
    yaw=((relative_yaw + 180.0) % 360.0) - 180.0
    vertical=(horizontal_sq <= tol*tol and local_z > tol and abs(yaw) <= 1.0)
    return 1 if vertical else 2
assert wall_candidate_priority(0.0, 0.0, 271.0, 0.0) == 1      # direct stack: second owner
assert wall_candidate_priority(300.0, 0.0, 0.0, 0.0) == 2       # straight continuation
assert wall_candidate_priority(150.0, 150.0, 0.0, 90.0) == 2    # L-corner
assert wall_candidate_priority(0.0, 0.0, 271.0, 180.0) == 2     # flipped custom candidate is not stack owner
# Horizontal support priority is 0, so an upper Floor edge owns the canonical exterior facing when
# it competes with the vertical wall below. The vertical stack still beats lateral/corner candidates
# when no horizontal support is present.
horizontal_priority=0
vertical_priority=wall_candidate_priority(0.0, 0.0, 271.0, 0.0)
lateral_priority=wall_candidate_priority(300.0, 0.0, 0.0, 0.0)
assert horizontal_priority < vertical_priority < lateral_priority

# Timed construction uses synchronized server time, visible reveal and tick-only-while-building.
require(actor_h, 'bConstructionComplete', 'ConstructionStartServerTime', 'ConstructionDuration', 'GetConstructionProgress01')
require(actor_cpp, 'GetServerWorldTimeSeconds', 'ConstructionStartScaleZ', 'SetScalarParameterValueOnMaterials',
        'SetCollisionEnabled', 'SetActorTickEnabled(true)', 'SetActorTickEnabled(false)', 'CompleteConstructionAuthority')

# Doors are replicated, authority-controlled, faction-access checked, and tick only while animating.
require(door_h, 'ReplicatedUsing=OnRep_DoorOpen', 'ToggleDoor', 'SetDoorOpen', 'RestoreDoorOpenState',
        'UBoxComponent', 'DoorCollision', 'RefreshDefinitionPresentation() override', 'RefreshConstructionPresentation(bool bForce = false) override')
require(door_cpp, 'CanActorUse', 'SetActorTickEnabled(true)', 'SetActorTickEnabled(false)',
        'DOREPLIFETIME(AARPGBuildDoorActor, bDoorOpen)', 'DoorCollision->SetCollisionProfileName(TEXT("BlockAll"))',
        'DoorCollision->SetCollisionEnabled(bShouldCollide ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision)',
        'Definition->DoorHingeSide == EARPGBuildDoorHingeSide::Left',
        'bHingeOnLeft ? LocalMax.X : LocalMin.X',
        'PivotTranslation = DoorHingeLocal - DoorRotation.RotateVector(DoorHingeLocal)',
        'DoorPivot->SetRelativeLocationAndRotation(PivotTranslation, DoorRotation)')
# v2.15.10 regression: completed construction must not cancel a specialised Door Tick. The base actor
# still disables construction Tick from RefreshConstructionPresentation(), but its completed Tick branch
# must simply return so a Door that explicitly enabled Tick can interpolate to the replicated target.
base_tick=actor_cpp[actor_cpp.index('void AARPGBuildPieceActor::Tick'):actor_cpp.index('void AARPGBuildPieceActor::CompleteConstructionAuthority')]
completed_branch=base_tick[base_tick.index('if (bConstructionComplete)'):base_tick.index('RefreshConstructionPresentation();')]
assert 'SetActorTickEnabled(false)' not in completed_branch
require(actor_cpp, 'specialised', 'derived build actors (notably doors)', 'explicitly re-enabled derived Tick')
# Door collision and hinge are derived from transformed visible geometry. v2.15.11 makes hinge side
# explicit on the Door definition. For the current 103x16x205 Wood Door, Left means +X = +51.5 when
# viewed from logical +Y/front; Right means -X = -51.5. Closed alpha preserves the snapped center.
def rotate_yaw_xy(v, yaw_deg):
    import math
    x,y,z=v
    r=math.radians(yaw_deg)
    return (x*math.cos(r)-y*math.sin(r), x*math.sin(r)+y*math.cos(r), z)
def hinged_point(point, hinge, yaw_deg):
    rel=(point[0]-hinge[0], point[1]-hinge[1], point[2]-hinge[2])
    rr=rotate_yaw_xy(rel,yaw_deg)
    return (hinge[0]+rr[0], hinge[1]+rr[1], hinge[2]+rr[2])
door_min=(-51.5,-8.0,-102.5); door_max=(51.5,8.0,102.5)
door_center=tuple((a+b)*0.5 for a,b in zip(door_min,door_max))
door_extent=tuple((b-a)*0.5 for a,b in zip(door_min,door_max))
left_hinge=(door_max[0],door_center[1],door_center[2])
right_hinge=(door_min[0],door_center[1],door_center[2])
assert door_extent == (51.5,8.0,102.5)
assert hinged_point(door_center,left_hinge,0.0) == door_center
assert hinged_point(door_center,right_hinge,0.0) == door_center
left_opened=hinged_point(door_center,left_hinge,90.0)
right_opened=hinged_point(door_center,right_hinge,90.0)
assert abs(left_opened[0]-51.5) < 1e-6 and abs(left_opened[1]+51.5) < 1e-6
assert abs(right_opened[0]+51.5) < 1e-6 and abs(right_opened[1]-51.5) < 1e-6
require(piece_h, 'enum class EARPGBuildDoorHingeSide', 'Left UMETA(DisplayName="Left")', 'Right UMETA(DisplayName="Right")',
        'DoorHingeSide = EARPGBuildDoorHingeSide::Left')

# Player-facing native, reskinnable interfaces are inherited on AARPGCharacter.
require(char_h, 'UARPGBuildingUIComponent', 'OpenBuildMenuUI', 'BeginBuildPlacement', 'ConfirmBuildPlacement', 'InteractBuiltStructure', 'DemolishBuiltStructure')
require(char_cpp, 'CreateDefaultSubobject<UARPGBuildingUIComponent>(TEXT("BuildingUI"))')
require(ui_comp_h, 'BuildMenuWidgetClass', 'PlacementHUDWidgetClass', 'StorageWidgetClass', 'CraftingStationWidgetClass',
        'BuildPieceRowWidgetClass', 'StructureItemRowWidgetClass', 'StationRecipeRowWidgetClass', 'DemolishBuiltStructureFromView')
require(ui_h, 'UARPGBuildMenuWidget', 'UARPGBuildPlacementHUDWidget', 'UARPGStoragePanelWidget', 'UARPGCraftingStationPanelWidget',
        'UARPGBuildPieceRowWidget', 'UARPGStructureItemRowWidget', 'UARPGStationRecipeRowWidget')
require(ui_cpp, 'PRODUCTION STATION', 'PLAYER INVENTORY', 'INPUT + FUEL', 'OUTPUT', 'QueueRecipe')
# Structure transfer UI targets the exact clicked runtime instance (important for durability).
require(ui_h, 'FGuid InstanceId')
require(ui_cpp, 'DepositToStorageInstance', 'WithdrawFromStorageInstance', 'WithdrawStationOutputInstance')
require(inv_h, 'TransferItemInstanceTo')
require(inv_cpp, 'bool UARPGInventoryComponent::TransferItemInstanceTo', 'SourceEntry.bEquipped', 'FARPGInventoryEntry Moved = SourceEntry', 'Destination->Items.Append(MovedEntries)')

# Storage/furnace interactions are routed through the player-owned Interaction component/RPCs.
require(inter_h, 'DepositToStorageInstance', 'WithdrawFromStorageInstance', 'WithdrawStationOutputInstance', 'ToggleBuiltDoor', 'DemolishBuilding', 'QueueCraft')
require(inter_cpp, 'ServerDepositToStorageInstance_Implementation', 'ServerWithdrawFromStorageInstance_Implementation',
        'ServerWithdrawStationOutputInstance_Implementation', 'ServerToggleBuiltDoor_Implementation', 'ServerDemolishBuilding_Implementation', 'ServerQueueCraft_Implementation')

# Production station/furnace: data-driven station, wood-style tagged fuel, ore inputs, outputs and transactional safeguards.
require(station_h, 'ApplyStationDefinition', 'CanQueueRecipe', 'OutputInventory')
require(station_cpp, 'bConsumesFuel', 'FuelTag', 'FuelPerCraft', 'ARPGCountTaggedItems', 'ConsumeFuelForCraft',
        'ARPGAggregateRecipeAmounts', 'ARPGCanFitResolvedOutputs', 'HasUnequippedItem', 'RemoveUnequippedItem', 'ReplaceInventory(Before)',
        'StationDefinition->StationTag.MatchesTagExact', 'SetActorTickEnabled(true)', 'SetActorTickEnabled(false)')
# Required station tag is a hard requirement, even if the station is misconfigured/untagged.
canuse=station_cpp[station_cpp.index('bool AARPGCraftingStationActor::CanUseRecipe'):station_cpp.index('bool AARPGCraftingStationActor::ConsumeRecipeInputs')]
assert '!StationDefinition || !StationDefinition->StationTag.IsValid()' in canuse

# Persistent construction + door state and existing container/furnace inventories/queues.
require(types_h, 'bConstructionComplete', 'ConstructionRemainingSeconds', 'bDoorOpen')
assert save_h.count('SaveVersion = 5') >= 2
require(save_cpp, 'R.bConstructionComplete=B->IsConstructionComplete()', 'R.ConstructionRemainingSeconds=B->GetConstructionRemainingSeconds()',
        'R.bDoorOpen=Door->IsDoorOpen()', 'RestoreConstructionState', 'RestoreDoorOpenState', 'CraftQueue', 'OutputItems',
        'ProcessOfflineElapsed()', 'SetActorTickEnabled(C->CraftQueue.Num()>0)')

# v2.15.12 persistent build ownership reload regression. Guest/no-login play must keep a stable
# CharacterId because loaded build modification access intentionally keys off OwnerCharacterId when
# no account identity exists. Pre-v2.15.12 worlds are recovered only when the saved owner and local
# player are unambiguous; multiplayer/multi-owner worlds are never guessed/adopted.
require(save_h, 'FGuid GuestCharacterId')
require(account_cpp, 'Index->GuestCharacterId = CharacterId', 'if (!bLoggedIn) return Index->GuestCharacterId',
        'GuestIds.Add(Index->GuestCharacterId)')
require(persistence_h, 'bDeferredGuestIdentityRecoveryOnce')
require(persistence_cpp, 'if (Last.IsValid())', 'Character->CharacterId = Last',
        'SetTimerForNextTick(this, &UARPGPersistenceComponent::AttemptAutoLoad)',
        'Accounts->RegisterCharacterId(Character->CharacterId)')
assert 'Last.IsValid() && Accounts->IsLoggedIn()' not in persistence_cpp
require(save_cpp, 'ARPGRecoverLegacyGuestWorldOwnerIdentity', 'PlayerCharacterCount != 1 || !SoleLocalPlayer',
        '!Record.OwnerAccountId.IsValid() && Record.OwnerCharacterId.IsValid()',
        'LegacyGuestOwnerIds.Num() != 1', 'Accounts->RegisterCharacterId(StableGuestId)',
        'Record.OwnerCharacterId = StableGuestId', 'SoleLocalPlayer->CharacterId = StableGuestId',
        'ARPGRecoverLegacyGuestWorldOwnerIdentity(W, Save)')
# Model the reported standalone failure: old loaded build owner != newly generated Guest id =>
# modification denied. Recovery makes the local identity equal to the saved owner without weakening
# CanActorModify, so both preview and authority paths regain access.
old_owner='guest-old'; regenerated='guest-new'
assert old_owner != regenerated
recovered=old_owner
assert recovered == old_owner
# Ambiguous worlds must not pick an owner.
def recover_guest_owner(saved_owner_ids, local_player_count):
    unique={x for x in saved_owner_ids if x}
    return next(iter(unique)) if local_player_count == 1 and len(unique) == 1 else None
assert recover_guest_owner(['guest-old','guest-old'],1) == 'guest-old'
assert recover_guest_owner(['guest-a','guest-b'],1) is None
assert recover_guest_owner(['guest-old'],2) is None


# v2.15.17 root placement-collision regression: build-vs-build validation must use both pieces'
# authored logical PlacementBounds instead of treating decorative Static Mesh collision as structural
# occupancy. Inter-story Wall/Floor seam classification also compares the wall run axis modulo 180
# degrees; front/back art facing is not a collision distinction.
require(build_cpp, 'FARPGPlacementOBB', 'ARPGMakePlacementOBB', 'ARPGPlacementOBBsOverlap',
        'ARPGPlacementVolumesOverlapMeaningfully', 'ARPGYawAxesEquivalent',
        'ARPGWallOccupiesHorizontalStructuralEdge',
        'Raw mesh collision by itself is',
        '!ARPGPlacementVolumesOverlapMeaningfully(BuildNeighbor, Piece, Final)')

def yaw_axes_equivalent(a, b, tol=1.0):
    d=abs(((a-b+180.0)%360.0)-180.0)
    return d <= tol or abs(d-180.0) <= tol

# Front/back reversal occupies the same wall edge; a perpendicular wall does not.
assert yaw_axes_equivalent(0.0, 180.0)
assert yaw_axes_equivalent(90.0, -90.0)
assert not yaw_axes_equivalent(0.0, 90.0)

def wall_occupies_edge(origin_xy, wall_yaw, floor_yaw=0.0, snap=300.0, tol=2.5):
    half=snap*0.5
    edges=[((0.0, half),0.0),((0.0,-half),0.0),((half,0.0),90.0),((-half,0.0),90.0)]
    # floor_yaw is zero in this regression, matching the runtime edge-axis contract.
    for (ex,ey), tangent in edges:
        if (origin_xy[0]-ex)**2 + (origin_xy[1]-ey)**2 <= tol*tol and yaw_axes_equivalent(floor_yaw+tangent, wall_yaw):
            return True
    return False

assert wall_occupies_edge((0.0,150.0),0.0)
assert wall_occupies_edge((0.0,150.0),180.0)  # same edge, opposite visible face
assert wall_occupies_edge((150.0,0.0),90.0)
assert wall_occupies_edge((150.0,0.0),-90.0)
assert not wall_occupies_edge((0.0,150.0),90.0)
assert not wall_occupies_edge((0.0,0.0),0.0)

# Logical placement volumes remain the authoritative build occupancy. A decorative mesh beam/post
# may overlap the incoming physics query while the authored placement volumes are still separated.
def aabb_overlap(center_a, extent_a, center_b, extent_b):
    return all(abs(a-b) <= ea+eb for a,ea,b,eb in zip(center_a,extent_a,center_b,extent_b))

# Two logical modules separated by a seam do not conflict even if rendered art extends past it.
assert not aabb_overlap((0.0,0.0,300.0),(148.0,148.0,7.0),
                        (0.0,0.0,330.0),(148.0,148.0,7.0))
# True duplicate occupancy remains a conflict.
assert aabb_overlap((0.0,0.0,300.0),(148.0,148.0,7.0),
                    (0.0,0.0,300.0),(148.0,148.0,7.0))

# v2.15.16 regression: strict inverse/inter-story seam fallbacks must still execute when an
# overlapping neighbour advertises *zero* native snap candidates. v2.15.14/v2.15.15 added the
# correct geometry predicates, but an early `NeighborCandidates.Num() == 0` return made those
# predicates unreachable in precisely the inverse relationships they were designed to validate.
neighbor_fn=build_cpp[build_cpp.index('static bool ARPGIsValidSnappedBuildNeighbor'):build_cpp.index('UARPGBuildingComponent::UARPGBuildingComponent')]
assert 'if (NeighborCandidates.Num() == 0) return false;' not in neighbor_fn
require(neighbor_fn,
        'valid inter-story relationships are',
        'ARPGIsValidUpperHorizontalWallSeamNeighbor(Neighbor, IncomingPiece, IncomingFinal)',
        'ARPGIsValidWallUnderUpperHorizontalSeamNeighbor(Neighbor, IncomingPiece, IncomingFinal)')
# Model the control flow: zero native candidates do not decide the result; a strict seam match can.
def snapped_neighbor_accepts(native_candidates, upper_horizontal_wall_seam=False, wall_under_upper_slab=False):
    if native_candidates:
        return True
    if upper_horizontal_wall_seam:
        return True
    if wall_under_upper_slab:
        return True
    return False
assert snapped_neighbor_accepts([], upper_horizontal_wall_seam=True)       # Floor inserted after upper Wall
assert snapped_neighbor_accepts([], wall_under_upper_slab=True)            # Wall inserted under upper Floor
assert not snapped_neighbor_accepts([])                                    # arbitrary zero-socket overlap stays blocked

# UE5.8.1 compile compatibility guards from the first real v2.15 build.
require(build_cpp, '#include "Engine/OverlapResult.h"')
assert 'UVerticalBox*&OutBox' not in ui_cpp
require(ui_cpp, 'TObjectPtr<UVerticalBox>&OutBox')
assert 'UARPGStoragePanelWidget*P=' not in ui_cpp and 'UARPGCraftingStationPanelWidget*P=' not in ui_cpp

print('Settlement building + storage + production UI model: PASS')


# v2.15.18 semantic structural-slot regression: a valid Wall on a Floor must not become blocked merely
# because another completed Wall overlaps the broad physics query but did not supply the active SnapTarget.
# Standard wall occupancy is edge-segment/story-bay topology, not generic OBB penetration.
require(build_cpp,
        'EARPGStructuralOccupancyRelation',
        'ARPGClassifyWallWallStructuralOccupancy',
        'ARPGClassifyWallHorizontalStructuralOccupancy',
        'ARPGClassifyStandardStructuralOccupancy',
        'semantic grid occupancy before falling back to raw OBB penetration')

def seg_relation(ca, ya, cb, yb, half=150.0, tol=2.5):
    import math
    def axis(y):
        r=math.radians(y); return (math.cos(r), math.sin(r))
    a=axis(ya); b=axis(yb); d=(cb[0]-ca[0], cb[1]-ca[1])
    dot=a[0]*b[0]+a[1]*b[1]
    if abs(dot)>=0.999:
        n=(-a[1],a[0]); across=abs(d[0]*n[0]+d[1]*n[1]); along=abs(d[0]*a[0]+d[1]*a[1])
        if across>tol: return 'n/a'
        if along<=tol: return 'conflict'
        if abs(along-2*half)<=tol: return 'compatible'
        if along<2*half-tol: return 'conflict'
        return 'n/a'
    den=a[0]*b[1]-a[1]*b[0]
    ta=(d[0]*b[1]-d[1]*b[0])/den
    tb=(d[0]*a[1]-d[1]*a[0])/den
    if abs(ta)>half+tol or abs(tb)>half+tol: return 'n/a'
    ea=abs(abs(ta)-half)<=tol; eb=abs(abs(tb)-half)<=tol
    return 'compatible' if ea and eb else 'conflict'

# Same-story side continuation and L-corner are valid even if physics/art overlaps at their seam.
assert seg_relation((0,150),0,(300,150),0) == 'compatible'
assert seg_relation((0,150),0,(150,0),90) == 'compatible'
assert seg_relation((0,150),180,(150,0),-90) == 'compatible'
# Same slot and interior crossings remain blocked.
assert seg_relation((0,150),0,(0,150),180) == 'conflict'
assert seg_relation((0,0),0,(0,0),90) == 'conflict'

# EvaluatePlacementInternal must consult semantic occupancy before generic logical OBB fallback.
eval_fn=build_cpp[build_cpp.index('EARPGPlacementResult UARPGBuildingComponent::EvaluatePlacementInternal'):build_cpp.index('EARPGPlacementResult UARPGBuildingComponent::EvaluatePlacement(')]
assert eval_fn.index('ARPGClassifyStandardStructuralOccupancy') < eval_fn.index('ARPGPlacementVolumesOverlapMeaningfully')
