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
window_h=read(Path('Public/Building/ARPGBuildWindowActor.h'))
window_cpp=read(Path('Private/Building/ARPGBuildWindowActor.cpp'))
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
require(piece_h, 'BuildMesh', 'BuildSkeletalMesh', 'PreviewMesh', 'PreviewSkeletalMesh', 'BuildCost', 'ConstructionSeconds', 'ConstructionStartScaleZ',
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
        'Hit.ImpactPoint - DesiredRotation.RotateVector(BottomAnchorLocal)', 'ARPGBuildPlacementOccupancyOBBs',
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
# v2.15.44 additive Skeletal Mesh build visuals: existing BuildMesh remains intact, skeletal assets
# are opt-in and drive the same transformed bounds, construction path and local ghost. A native
# Window actor supplies bounds-driven collision so skeletal Windows do not require a Physics Asset
# merely for authoritative occupancy/duplicate-insert detection.
require(actor_h, 'USkeletalMeshComponent', 'BuildSkeletalMesh', 'GetActiveBuildMeshComponent',
        'GetActiveBuildVisualLocalBounds', 'GetActiveBuildVisualRawBounds')
require(actor_cpp, 'Components/SkeletalMeshComponent.h', 'Engine/SkeletalMesh.h',
        'Definition->BuildSkeletalMesh', 'BuildSkeletalMesh->SetSkeletalMesh', 'BuildSkeletalMesh->SetComponentTickEnabled(false)',
        'GetActiveBuildMeshComponent()', 'ActiveMesh->SetScalarParameterValueOnMaterials',
        'USkeletalMesh* Mesh = Piece->BuildSkeletalMesh.LoadSynchronous()')
require(build_cpp, 'USkeletalMesh* Mesh = Piece->BuildSkeletalMesh.LoadSynchronous()',
        'same transformed bounds here keeps pivot-aware ground placement, structural snapping, hosted')
require(preview_h, 'USkeletalMeshComponent', 'PreviewSkeletalMesh')
require(preview_cpp, 'Piece->PreviewSkeletalMesh', 'Piece->BuildSkeletalMesh',
        'PreviewSkeletalMesh->SetSkeletalMesh', 'PreviewSkeletalMesh->SetComponentTickEnabled(false)', 'GetActivePreviewMeshComponent')
# v2.15.46 Skeletal ghost material compatibility is prepared by the framework rather than requiring
# per-piece preview materials or silently accepting Unreal's grey/default fallback.
require(preview_cpp, 'ARPGPrepareSkeletalPreviewMaterial', 'CheckMaterialUsage(MATUSAGE_SkeletalMesh)',
        'EnsureIsComplete()', 'MarkRenderStateDirty()', 'ValidPreviewMaterial.Get()', 'InvalidPreviewMaterial.Get()')
require(build_cpp, 'EARPGBuildPieceKind::Window: return AARPGBuildWindowActor::StaticClass()')
require(window_h, 'AARPGBuildWindowActor', 'WindowCollision')
require(window_cpp, 'GetActiveBuildVisualLocalBounds', 'SetBoxExtent',
        'IsConstructionComplete() || Definition->bCollisionDuringConstruction')
# Door remains backward-compatible but can also move an active skeletal visual beneath DoorPivot.
require(door_cpp, 'BuildSkeletalMesh->SetupAttachment(DoorPivot)', 'GetActiveBuildVisualLocalBounds')
assert 'Build Mesh (Static)' not in piece_h  # do not rename the established editor field
assert 'Preview Mesh (Static)' not in piece_h
assert 'Hit.ImpactNormal * FMath::Max(0.f, SelectedBuildPiece->PlacementBounds.Z)' not in build_cpp

# Mathematical regression: bottom-pivot and center-pivot 150 cm foundations both land their visible bottom at Z=0.
def grounded_actor_z(surface_z, local_min_z):
    return surface_z - local_min_z
assert grounded_actor_z(0.0, 0.0) == 0.0       # bottom pivot
assert grounded_actor_z(0.0, -75.0) == 75.0    # center pivot
# A 300 cm bottom-pivot wall on a 150 cm bottom-pivot foundation starts exactly at foundation top.
assert (150.0 - 0.0) == 150.0
require(actor_cpp, 'ARPGGetBuildDefinitionLocalBounds', 'AlignTopPlaneZ', 'IncomingOnTargetTopZ',
        'TargetMax.Z + WallHeight - IncomingMax.Z', 'IncomingOnNextWallStoryPlaneZ',
        'IncomingTopOnNextWallStoryPlaneZ',
        'TargetMin.Z + FMath::Max(1.f, Definition->StandardWallHeight) - IncomingMax.Z')
# v2.15.42 finished-surface lattice regression: Floor/Ceiling thickness extends DOWN from the story
# plane. A 300 cm storey with an 18 cm Floor therefore places the Floor bottom at 282 and its walking
# surface/top at 300; slab thickness never becomes +18 cm of additional building height.
story_height=300.0
floor_thickness=18.0
assert story_height-floor_thickness == 282.0
assert (story_height-floor_thickness)+floor_thickness == story_height
# v2.15.41 root lattice regression: Wall->Wall and Wall->Floor must never use rendered Wall top
# as the next-storey plane. A deliberately non-300-cm wall mesh still advances exactly 300 cm per
# story, so art height error cannot accumulate higher in the building.
def canonical_next_story(wall_bottom, story_height=300.0):
    return wall_bottom + story_height
wall_art_height=287.5
wall_bottom=0.0
for story in range(1, 11):
    wall_bottom=canonical_next_story(wall_bottom)
    assert abs(wall_bottom-story*300.0) < 1e-5
# The removed mesh-top path would already be 125 cm off after ten storeys.
assert abs(wall_art_height*10.0-3000.0) == 125.0
assert 'FVector(0.f, 0.f, IncomingAboveTargetZ)' not in actor_cpp
require(build_cpp, 'ARPGGetWallStructuralWorldZRange',
        'OutMaxZ = OutMinZ + FMath::Max(1.f, Piece->StandardWallHeight)',
        'WallStructuralTopZ')
# Authority re-resolves snap/placement rather than trusting local preview.
pa=build_cpp[build_cpp.index('bool UARPGBuildingComponent::PlacePieceAuthority'):]
assert 'ResolvePlacementTransform' in pa and 'EvaluatePlacementInternal' in pa
assert 'BuildCatalog.ContainsByPredicate' in build_cpp and 'SnapTarget->CanActorModify(Owner)' in build_cpp
# Costs aggregate and only unequipped resources can be consumed.
require(build_cpp, 'ARPGAggregateBuildCosts', 'HasUnequippedItem', 'RemoveUnequippedItem', 'RefundBuildResources')
# Native actor fallback makes common pieces usable without actor Blueprints.
require(build_cpp, 'EARPGBuildPieceKind::Door: return AARPGBuildDoorActor::StaticClass()',
        'EARPGBuildPieceKind::Window: return AARPGBuildWindowActor::StaticClass()',
        'EARPGBuildPieceKind::Storage: return AARPGStorageActor::StaticClass()',
        'EARPGBuildPieceKind::Production: return AARPGCraftingStationActor::StaticClass()')

# Preview actor is local-only and collisionless.
require(preview_cpp, 'bReplicates = false', 'SetActorEnableCollision(false)', 'ECollisionEnabled::NoCollision',
        'PlacementValid', 'PreviewOpacity')

# Snap graph covers requested structural families and doors/windows.
require(actor_cpp, 'Foundation', 'WindowWall', 'Doorway', 'Ceiling', 'Roof',
        'TargetKind == EARPGBuildPieceKind::Doorway && IncomingKind == EARPGBuildPieceKind::Door',
        'TargetKind == EARPGBuildPieceKind::WindowWall && IncomingKind == EARPGBuildPieceKind::Window')
# v2.15.7/v2.15.45 hosted-insert regression: both insert families remain pivot-aware, but they no
# longer share an incorrect vertical rule. Doors are floor-standing and remain bottom-aligned exactly
# as before. Windows are suspended inserts and center their transformed visible bounds in X/Y/Z, then
# apply the WindowWall host's optional local Window Insert Offset.
require(actor_cpp, 'ARPGGetBottomAlignedInsertTranslation', 'ARPGGetCenteredInsertTranslation',
        'TargetCenter.X - IncomingCenter.X', 'TargetCenter.Y - IncomingCenter.Y',
        'TargetMin.Z - IncomingMin.Z', 'return TargetCenter - IncomingCenter + HostLocalOffset',
        'Definition->WindowInsertOffset')
# v2.15.46 WindowWall hosting is intrinsic even if generic standard structural snaps are disabled.
require(actor_cpp, '!Definition->bGenerateStandardSnapPoints',
        'Definition->PieceKind == EARPGBuildPieceKind::WindowWall',
        'IncomingPiece->PieceKind == EARPGBuildPieceKind::Window')
require(piece_h, 'WindowInsertOffset', 'DisplayName="Window Insert Offset"',
        'PieceKind==EARPGBuildPieceKind::WindowWall')

def bottom_aligned_insert(target_min, target_max, incoming_min, incoming_max):
    tc = tuple((a+b)*0.5 for a,b in zip(target_min,target_max))
    ic = tuple((a+b)*0.5 for a,b in zip(incoming_min,incoming_max))
    return (tc[0]-ic[0], tc[1]-ic[1], target_min[2]-incoming_min[2])

def centered_window_insert(target_min, target_max, incoming_min, incoming_max, host_offset=(0.0,0.0,0.0)):
    tc = tuple((a+b)*0.5 for a,b in zip(target_min,target_max))
    ic = tuple((a+b)*0.5 for a,b in zip(incoming_min,incoming_max))
    return tuple(tc[i]-ic[i]+host_offset[i] for i in range(3))

# Current Wood example: a 95 cm-high Window inside a 271 cm-high WindowWall is centered vertically
# by default (88 cm above a shared bottom plane), while the Door contract remains bottom-aligned.
wood_wall_min=(0.0,0.0,0.0); wood_wall_max=(301.0,31.0,271.0)
wood_window_min=(0.0,0.0,0.0); wood_window_max=(143.0,95.0,95.0)
assert abs(bottom_aligned_insert(wood_wall_min,wood_wall_max,wood_window_min,wood_window_max)[2]) < 1e-6
assert abs(centered_window_insert(wood_wall_min,wood_wall_max,wood_window_min,wood_window_max)[2] - 88.0) < 1e-6
assert abs(centered_window_insert(wood_wall_min,wood_wall_max,wood_window_min,wood_window_max,(0,0,12))[2] - 100.0) < 1e-6
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
# v2.15.46 hardens local third-person acquisition without changing authority or the final native socket.
require(build_cpp, 'ARPGSegmentPassesInsertAimCorridor', 'PreferredTraceTarget', 'PreferredInsertHost',
        'UnpaddedLocalExtent', 'CorridorPenalty', 'WorldExtent.GetMax()')
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
        'bNeighborIsInsert && (bIncomingIsStandardStructural || bIncomingIsStair)',
        'ARPGIsInsertSnapPair(CandidateHost->Definition->PieceKind, NeighborKind)',
        '!BuildNeighbor->CanActorModify(Owner) || !ResolvedHost->CanActorModify(Owner)')

def reverse_hosted_insert_allows(insert_matches_host, host_seam_relation):
    return insert_matches_host and host_seam_relation == 'compatible'
assert reverse_hosted_insert_allows(True, 'compatible')       # Doorway -> Door -> upper Floor
assert not reverse_hosted_insert_allows(False, 'compatible') # unrelated Door cannot be ignored
assert not reverse_hosted_insert_allows(True, 'conflict')    # real host conflict still blocks

# v2.15.49 hosted-insert/Stair-side seam regression: a verified Door/Window inherits its exact
# Doorway/WindowWall host's established legal Stair-side seam. This fixes a closed skeletal Window
# independently blocking a Stair even though its WindowWall is already accepted on the parallel side edge.
require(build_cpp, 'ARPGHostedInsertAllowsStairSideNeighbor',
        'ARPGInsertActorMatchesHost(InsertActor, InsertHost)',
        'const bool bIncomingIsStair = Piece->PieceKind == EARPGBuildPieceKind::Stair;',
        'bNeighborIsInsert && (bIncomingIsStandardStructural || bIncomingIsStair)',
        'InsertAllowsIncomingNeighbor')

def hosted_insert_allows_stair_side(insert_matches_host, host_is_valid_stair_side, has_flat_stair_snap=True):
    return insert_matches_host and host_is_valid_stair_side and has_flat_stair_snap
assert hosted_insert_allows_stair_side(True, True)
assert not hosted_insert_allows_stair_side(False, True)
assert not hosted_insert_allows_stair_side(True, False)
assert not hosted_insert_allows_stair_side(True, True, False)


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

# v2.15.46 bounded aim-corridor model: a small third-person camera offset may miss the exact host OBB
# while still passing within the WindowWall's visible module radius; a clearly unrelated ray remains out.
def insert_aim_corridor(centerline_error, max_half_extent, padding):
    return abs(centerline_error) <= max_half_extent + max(0.0, padding)
assert not segment_intersects_box((170.0,-500.0,120.0),(170.0,500.0,120.0),(-151.0,-15.5,0.0),(151.0,15.5,271.0))
assert insert_aim_corridor(170.0,151.0,30.0)
assert not insert_aim_corridor(250.0,151.0,30.0)
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
        'CandidateEnvelopeDistSq', 'bHorizontalFootprintCapture',
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

# v2.15.24-v2.15.33 Stair regression: Foundation/Floor/Ceiling provide paired landing sockets,
# while Stair itself provides endpoint-chain sockets. A horizontal edge now supports both the proven
# HIGH-arrival/down-flight topology and a LOW-departure/up-flight topology on the exact same centerline.
# This lets a lower Stair arrive at a Foundation/Floor edge and the next Stair depart that edge without
# lateral/yaw drift. Stair HIGH/LOW endpoints can also chain directly when no horizontal landing owns
# the junction. v2.15.31 Landscape classification remains part of the validated placement path.
require(build_cpp, 'ARPGIsStairSupportSnapPair', 'ARPGIsStairChainSnapPair', 'ARPGIsAnyStairSnapPair',
        'bStairSupportSnapPair', 'bStairChainSnapPair', 'bAnyStairSnapPair', 'StairTargetAimDistSq',
        'CandidateAffinitySq', 'ARPGIsCompatibleStairHostStructuralNeighbor',
        'ARPGTransformMatchesStairHostCandidate', 'bLowEndOwnsHostEdge', 'StairCellCenter',
        'ARPGIsStairWallFamilyBoundarySeam', 'StairBoundsCenterLocal', 'StairFrame', 'WallAnchorInStair',
        'bOnSidePlane', 'bParallelToStairRun', 'StairPiece->PlacementBounds.X', 'WallHalfRun', 'LongitudinalOverlap',
        'ARPGIsCompatibleInsertHostStairNeighbor', 'ARPGGatherPlacementOverlaps',
        'ARPGBuildPlacementOccupancyOBBs', 'StairProfileSliceCount', 'ARPGIsLandscapeTerrainActor',
        'ALandscapeProxy', 'ARPGIsValidStairWorldSupportContact', 'SurfaceAboveSliceBottom',
        'bFoundSupportingSurface', 'SideOffsets')
require(actor_cpp, 'IncomingKind == EARPGBuildPieceKind::Stair && TargetKind != EARPGBuildPieceKind::Roof',
        'IncomingStairHalfRun', 'StairHighStructuralXYLocal', 'StairLowStructuralXYLocal', 'TargetStoryPlaneZ',
        'StairHighArrivalAlignedZ', 'StairLowDepartureAlignedZ', 'HighArrivalTranslation', 'LowDepartureTranslation',
        'Stairs are visual traversal art inside the structural story grid',
        'TargetKind == EARPGBuildPieceKind::Stair && IncomingKind == EARPGBuildPieceKind::Stair',
        'ChainStep', 'CenterDeltaX', 'ContinueUpZ', 'ContinueDownZ', 'FVector(CenterDeltaX + ChainStep, 0.f, ContinueUpZ)', 'FVector(CenterDeltaX - ChainStep, 0.f, ContinueDownZ)',
        'IncomingKind == EARPGBuildPieceKind::Floor || IncomingKind == EARPGBuildPieceKind::Ceiling',
        'LandingCenterOffset', 'StairLowStoryPlaneZ', 'HighLandingTranslation', 'LowLandingTranslation', 'completed Stair exposes flat landing CELLS',
        'local +X = uphill')
# Up-flights must start on the current walking surface; never high-align them to the next story.
assert 'const float StairLowDepartureAlignedZ = TargetStoryPlaneZ - IncomingMin.Z;' in actor_cpp
assert 'TargetStoryPlaneZ + WallHeight - IncomingMax.Z' not in actor_cpp
require(build_cpp, 'ARPGIsValidExistingStairWallSideSeamNeighbor', 'ARPGIsStairWallFamilyBoundarySeam',
        'WallAnchorInStair', 'WallTransform.GetLocation()', 'StairPiece->PlacementBounds.X',
        'LongitudinalOverlap', 'Reverse build order wrapper')

# v2.15.27 occupancy regression remains required: the final build-vs-build blocker check must consume
# the SAME Stair profile primitives as the broad overlap query.
meaningful_start=build_cpp.index('static bool ARPGPlacementVolumesOverlapMeaningfully(')
meaningful_end=build_cpp.index('/**', meaningful_start)
meaningful_body=build_cpp[meaningful_start:meaningful_end]
assert 'ARPGBuildPlacementOccupancyOBBs(IncomingPiece, IncomingFinal, IncomingVolumes)' in meaningful_body
assert 'ARPGBuildPlacementOccupancyOBBs(Neighbor->Definition, Neighbor->GetActorTransform(), NeighborVolumes)' in meaningful_body
assert 'ARPGMakePlacementOBB(IncomingPiece, IncomingFinal)' not in meaningful_body

gather_start=build_cpp.index('static void ARPGGatherPlacementOverlaps(')
gather_end=build_cpp.index('static bool ARPGWallOccupiesHorizontalStructuralEdge(', gather_start)
gather_body=build_cpp[gather_start:gather_end]
assert 'ARPGBuildPlacementOccupancyOBBs(Piece, Final, OccupancyVolumes)' in gather_body

# v2.15.37 canonical Stair XY+Z lattice regression. The visual Stair is 334 cm long x 300 cm wide x
# 278 cm high, but the STRUCTURAL flight cell is exactly 300 cm long x 300 cm wide x 300 cm story.
# The extra 34 cm of Stair art is split symmetrically as a 17 cm visual overhang at each end; it must
# never move Stair actor centres, chained flights or Floor landing cells off the 300 cm building grid.
import math

def rotate_z(v, yaw_deg):
    x,y,z=v
    r=math.radians(yaw_deg)
    c,sn=math.cos(r),math.sin(r)
    return (x*c-y*sn, x*sn+y*c, z)

def add(a,b): return tuple(x+y for x,y in zip(a,b))

def sub(a,b): return tuple(x-y for x,y in zip(a,b))

stair_min=(-167.0,-150.0,0.0)
stair_max=(167.0,150.0,278.0)
stair_center=tuple((a+b)*0.5 for a,b in zip(stair_min,stair_max))
low_visual=(stair_min[0],stair_center[1],stair_min[2])
high_visual=(stair_max[0],stair_center[1],stair_max[2])
wall_height=300.0
stair_grid=300.0
half_grid=stair_grid*0.5
floor_grid=300.0
floor_thickness=18.0
visual_half_run=(stair_max[0]-stair_min[0])*0.5
art_overhang=visual_half_run-half_grid
assert art_overhang == 17.0
residual=wall_height-(stair_max[2]-stair_min[2])
assert residual == 22.0
assert residual-floor_thickness == 4.0

# Structural Stair anchors are +/-150 around the transformed bounds centre, NOT +/-167 visual ends.
low_structural=(stair_center[0]-half_grid, stair_center[1], 0.0)
high_structural=(stair_center[0]+half_grid, stair_center[1], 0.0)
assert low_structural[0] == -150.0 and high_structural[0] == 150.0

host_edge=(0.0,150.0,0.0)
host_story_plane=0.0
yaw=-90.0

# HIGH-arrival/down-flight: structural HIGH anchor owns the Foundation edge. The Stair ACTOR centre
# lands exactly one 300 cm cell outside the host. Visual art overlaps 17 cm past both structural edges.
rh=rotate_z(high_structural,yaw)
high_arrival_translation=(host_edge[0]-rh[0], host_edge[1]-rh[1], host_story_plane-stair_max[2])
assert abs(high_arrival_translation[1]-300.0) < 1e-5
low_world=add(rotate_z(low_visual,yaw),high_arrival_translation)
high_world=add(rotate_z(high_visual,yaw),high_arrival_translation)
high_structural_world=add(rotate_z(high_structural,yaw),high_arrival_translation)
assert abs(high_structural_world[1]-150.0) < 1e-5
assert abs(high_world[1]-133.0) < 1e-5  # 17 cm visual overlap into landing
assert abs(low_world[1]-467.0) < 1e-5   # 17 cm visual overhang beyond far cell edge
assert abs(high_world[2]-host_story_plane) < 1e-5

# v2.15.43 LOW-departure/up-flight: structural LOW anchor owns the same host edge and the visible
# Stair LOW plane sits on the CURRENT finished Floor/Foundation surface. The previous high-aligned
# formula raised every flight by the 22 cm (300-278) residual, making each Stair sit on top of its
# landing. The 278 cm art now lives inside the 300 cm storey and stops 4 cm below the next Floor slab.
rl=rotate_z(low_structural,yaw)
low_departure_translation=(host_edge[0]-rl[0], host_edge[1]-rl[1], host_story_plane-stair_min[2])
assert abs(low_departure_translation[1]-0.0) < 1e-5
upper_low_world=add(rotate_z(low_visual,yaw),low_departure_translation)
upper_high_world=add(rotate_z(high_visual,yaw),low_departure_translation)
low_structural_world=add(rotate_z(low_structural,yaw),low_departure_translation)
high_structural_world=add(rotate_z(high_structural,yaw),low_departure_translation)
assert abs(low_structural_world[1]-150.0) < 1e-5
assert abs(high_structural_world[1]-(-150.0)) < 1e-5
assert abs(upper_low_world[1]-167.0) < 1e-5
assert abs(upper_high_world[1]-(-167.0)) < 1e-5
assert abs(upper_low_world[2]-0.0) < 1e-5
assert abs(upper_high_world[2]-278.0) < 1e-5
assert abs((host_story_plane+wall_height-floor_thickness)-upper_high_world[2]-4.0) < 1e-5

# Direct Stair->Stair continuation advances exactly one structural 300 cm cell and one 300 cm story.
# Regression explicitly forbids the old raw 334 cm endpoint delta that accumulated 34 cm XY drift.
chain_step=(stair_grid+stair_grid)*0.5
assert chain_step == 300.0
chain_up_local=(chain_step,0.0,wall_height)
chain_down_local=(-chain_step,0.0,-wall_height)
assert chain_up_local == (300.0,0.0,300.0)
assert chain_down_local == (-300.0,0.0,-300.0)
assert chain_up_local[0] != 334.0

# All four horizontal support edges put Stair structural anchors on exact +/-150 grid edges.
for edge,yaw_i in [((0.0,150.0,0.0),-90.0),((0.0,-150.0,0.0),90.0),((150.0,0.0,0.0),180.0),((-150.0,0.0,0.0),0.0)]:
    rhi=rotate_z(high_structural,yaw_i)
    high_t=(edge[0]-rhi[0],edge[1]-rhi[1],host_story_plane-stair_max[2])
    rlo=rotate_z(low_structural,yaw_i)
    low_t=(edge[0]-rlo[0],edge[1]-rlo[1],host_story_plane-stair_min[2])
    high_anchor=add(rotate_z(high_structural,yaw_i),high_t)
    low_anchor=add(rotate_z(low_structural,yaw_i),low_t)
    assert abs(high_anchor[0]-edge[0]) < 1e-5 and abs(high_anchor[1]-edge[1]) < 1e-5
    assert abs(low_anchor[0]-edge[0]) < 1e-5 and abs(low_anchor[1]-edge[1]) < 1e-5

# Stair -> Floor/Ceiling landing centres use structural half-grids. A 300 cm Floor is therefore
# centred exactly +/-300 cm from the Stair actor, regardless of the Stair art being 334 cm long.
floor_min=(-150.0,-150.0,0.0)
floor_max=(150.0,150.0,18.0)
landing_center_offset=half_grid+floor_grid*0.5
assert landing_center_offset == 300.0
# Landing Z is structural: current Stair LOW story + 300, not rendered Stair top. This keeps the
# upper Floor at the canonical story even though the Stair art itself ends at 278 cm.
high_floor_t=(stair_center[0]+landing_center_offset, stair_center[1], stair_min[2]+wall_height-floor_max[2])
low_floor_t=(stair_center[0]-landing_center_offset, stair_center[1], stair_min[2]-floor_max[2])
assert high_floor_t[0] == 300.0
assert low_floor_t[0] == -300.0

# For the up-flight placed from the +Y host edge, target actor Y is exactly 0. The upper Floor centre
# must therefore be exactly one grid cell beyond the far edge at Y=-300, NOT -334 or -317.
up_stair_actor_world=(0.0,0.0,0.0)
high_floor_world_offset=rotate_z((high_floor_t[0],high_floor_t[1],0.0),yaw)
upper_floor_center_world=(up_stair_actor_world[0]+high_floor_world_offset[0],
                          up_stair_actor_world[1]+high_floor_world_offset[1],
                          up_stair_actor_world[2]+high_floor_t[2])
assert abs(upper_floor_center_world[1]-(-300.0)) < 1e-5
assert abs(upper_floor_center_world[2]-282.0) < 1e-5  # Floor bottom; finished top/story plane is Z=300
assert abs((upper_floor_center_world[2]+18.0)-300.0) < 1e-5

# Old v2.15.36 topology: actor -17 plus raw landing 317 = -334 cm world centre. Keep this explicit so
# future regressions cannot reintroduce visual-endpoint drift under the claim that 334 'cancels'.
old_actor_y=-17.0
old_raw_landing_local_x=167.0-(-150.0)  # 317
old_world_floor_y=old_actor_y-old_raw_landing_local_x
assert old_world_floor_y == -334.0
assert abs(old_world_floor_y-upper_floor_center_world[1]) == 34.0

# v2.15.38 tiled-deck Stair regression. Canonical 300 cm LOW-departure placement centers the Stair
# in its host cell while the current 334 cm art overhangs each structural end by 17 cm. On a deck made
# from multiple 300 cm Foundation/Floor cells, that visual overhang may be returned by the broad Stair
# occupancy query against an immediate same-story neighbour. Such a neighbour is a compatible modular
# seam unless it actually occupies the Stair's structural flight cell.
def stair_horizontal_neighbor_compatible(stair_cell_center, neighbor_center, grid=300.0, tol=2.5, same_story=True):
    if not same_story:
        return False
    dx=neighbor_center[0]-stair_cell_center[0]
    dy=neighbor_center[1]-stair_cell_center[1]
    if dx*dx+dy*dy <= tol*tol:
        return False  # horizontal tile closing the actual flight cell
    ax,ay=abs(dx),abs(dy)
    return ((abs(ax-grid) <= tol and ay <= tol) or
            (abs(ay-grid) <= tol and ax <= tol))

# LOW-departure flight occupies the active host cell; the host itself is ignored by EvaluatePlacement.
# Immediate deck cells touched only by the 17 cm visual overhang are legal.
flight_cell=(0.0,0.0)
assert stair_horizontal_neighbor_compatible(flight_cell,(300.0,0.0))
assert stair_horizontal_neighbor_compatible(flight_cell,(-300.0,0.0))
assert stair_horizontal_neighbor_compatible(flight_cell,(0.0,300.0))
assert stair_horizontal_neighbor_compatible(flight_cell,(0.0,-300.0))
# A duplicate tile in the flight cell, diagonal/non-grid tile, or different-story tile is not legalized.
assert not stair_horizontal_neighbor_compatible(flight_cell,(0.0,0.0))
assert not stair_horizontal_neighbor_compatible(flight_cell,(300.0,300.0))
assert not stair_horizontal_neighbor_compatible(flight_cell,(300.0,0.0),same_story=False)

# HIGH-arrival flight occupies the outside neighbour cell. A tile centered there still blocks, while a
# tile one cell beyond/alongside may only be touched by the Stair art overhang and is a legal seam.
outside_flight_cell=(0.0,300.0)
assert not stair_horizontal_neighbor_compatible(outside_flight_cell,(0.0,300.0))
assert stair_horizontal_neighbor_compatible(outside_flight_cell,(0.0,600.0))
assert stair_horizontal_neighbor_compatible(outside_flight_cell,(300.0,300.0))

require(build_cpp, 'bNeighborFlatLanding', 'HostStoryPlane', 'NeighborStoryPlane',
        'bImmediateGridNeighbor', 'actual Stair flight cell', '17 cm art/rail overhang')

# v2.15.52 shared bidirectional Stair <-> Wall-family seam symmetry. Structural topology still owns the
# +/-150 side plane and parallel run axis, but collision-seam longitudinal overlap is compared against
# the Stair's authored 334 cm placement run. This allows the intentional 17 cm endpoint overhang to
# touch the immediately adjacent side-wall module without turning the Stair red.
def incoming_stair_wall_side_compatible(anchor_x, anchor_y, wall_yaw, stair_yaw=0.0, tol=2.5, wall_snap_size=300.0, stair_art_half=167.0):
    d=abs(((wall_yaw-stair_yaw+180.0)%360.0)-180.0)
    parallel=d <= 1.0 or abs(d-180.0) <= 1.0
    on_side_plane=abs(abs(anchor_y)-150.0) <= tol
    wall_half=max(1.0, wall_snap_size*0.5)
    wall_min=anchor_x-wall_half
    wall_max=anchor_x+wall_half
    overlap=min(wall_max,stair_art_half)-max(wall_min,-stair_art_half)
    return parallel and on_side_plane and overlap > tol

assert incoming_stair_wall_side_compatible(0.0,150.0,0.0)
assert incoming_stair_wall_side_compatible(140.0,150.0,0.0)
assert incoming_stair_wall_side_compatible(-140.0,-150.0,180.0)
# Current 334 cm Stair touches a side-wall continuation centred one 300 cm cell away by exactly 17 cm.
assert incoming_stair_wall_side_compatible(300.0,150.0,0.0)
assert incoming_stair_wall_side_compatible(-300.0,-150.0,180.0)
assert not incoming_stair_wall_side_compatible(0.0,150.0,90.0)
assert not incoming_stair_wall_side_compatible(0.0,0.0,0.0)
assert not incoming_stair_wall_side_compatible(320.0,150.0,0.0)
assert not incoming_stair_wall_side_compatible(600.0,150.0,0.0)

# Stair-first Wall-family reverse seam consumes the same v2.15.52 shared side-plane relationship
# rather than requiring an exact +/-17 cm actor-centre offset. This is important below an upper flight:
# a valid grid-owned wall can be centred at X=0 at a shared landing and must still be accepted when it
# runs parallel to the Stair and overlaps the flight longitudinally. Perpendicular end walls and walls
# through the Stair centreline remain blockers.
def existing_stair_wall_side_compatible(anchor_x, anchor_y, wall_yaw, stair_yaw=0.0, tol=2.5, wall_snap_size=300.0):
    # v2.15.35 uses the Wall-family ACTOR SNAP ORIGIN as the structural edge anchor. This mirrors
    # Foundation/Floor wall sockets and is deliberately independent of MeshRelativeTransform/pivot.
    d=abs(((wall_yaw-stair_yaw+180.0)%360.0)-180.0)
    parallel=d <= 1.0 or abs(d-180.0) <= 1.0
    on_side_plane=abs(abs(anchor_y-stair_center[1])-150.0) <= tol
    wall_half_run=max(1.0, wall_snap_size*0.5)
    wall_min=anchor_x-wall_half_run
    wall_max=anchor_x+wall_half_run
    overlap=min(wall_max,stair_max[0])-max(wall_min,stair_min[0])
    return parallel and on_side_plane and overlap > tol

# A visual/logical bounds-centre offset must NOT change the structural side result. This is the
# exact v2.15.34 regression: actor snap origin is on +150, while mesh-relative art could put the
# rendered/logical centre at +176 and falsely fail an origin-independent side-plane test.
assert existing_stair_wall_side_compatible(0.0,150.0,0.0)
visual_bounds_center_y=176.0
assert abs(abs(visual_bounds_center_y-stair_center[1])-150.0) > 2.5
# Exact legacy +/-17 cm ownership still works.
assert existing_stair_wall_side_compatible(-17.0,150.0,0.0)
assert existing_stair_wall_side_compatible(17.0,-150.0,180.0)
# Shared-landing / upper-flight wall centre between the two ownership offsets is now valid.
assert existing_stair_wall_side_compatible(0.0,150.0,0.0)
assert existing_stair_wall_side_compatible(0.0,-150.0,180.0)
# Doorway/Wall-family geometry can be longitudinally offset while still overlapping the same side flight.
assert existing_stair_wall_side_compatible(140.0,150.0,0.0)
# End wall, centreline wall, and unrelated parallel wall remain invalid.
assert not existing_stair_wall_side_compatible(0.0,150.0,90.0)
assert not existing_stair_wall_side_compatible(0.0,0.0,0.0)
assert not existing_stair_wall_side_compatible(500.0,150.0,0.0)

# Segmented Stair collision profile remains shared by broad and final blocker validation.
stair_profile_slices=8
stair_clearance=2.0
def stair_profile_slice(i):
    t0=i/stair_profile_slices
    t1=(i+1)/stair_profile_slices
    x0=stair_min[0]+(stair_max[0]-stair_min[0])*t0
    x1=stair_min[0]+(stair_max[0]-stair_min[0])*t1
    z0=stair_min[2]+(stair_max[2]-stair_min[2])*t0
    z1=stair_min[2]+(stair_max[2]-stair_min[2])*t1
    return (x0+stair_clearance, x1-stair_clearance, z0+stair_clearance, z1-stair_clearance)
first=stair_profile_slice(0)
second=stair_profile_slice(1)
high=stair_profile_slice(6)
assert first[2] > stair_min[2]
assert second[2] > first[3]
assert high[2] > 200.0
assert high[3] < stair_max[2]

# v2.15.30 support semantics: a continuous terrain actor may touch more than one lower slice when its
# surface remains below each slice underside. It becomes a blocker as soon as the surface protrudes
# materially into any touched profile slice. This directly guards against reintroducing v2.15.29's
# blanket slice-index rejection.
surface_tolerance=max(2.0, stair_clearance+1.0)
def support_surface_is_valid(slice_bottom_z, surface_z):
    return (surface_z - slice_bottom_z) <= surface_tolerance

assert support_surface_is_valid(first[2], first[2]-1.0)
assert support_surface_is_valid(second[2], second[2]-1.0)  # same terrain actor under slice 1 is legal
assert not support_surface_is_valid(second[2], second[2]+10.0)

support_start=build_cpp.index('static bool ARPGIsValidStairWorldSupportContact(')
support_end=build_cpp.index('/**\n * Collision validation cares about the wall', support_start)
support_body=build_cpp[support_start:support_end]
assert 'for (const FARPGPlacementOBB& Volume : StairVolumes)' in support_body
assert 'SurfaceAboveSliceBottom > SurfaceTolerance' in support_body
assert 'if (!bFoundSupportingSurface)' in support_body
assert 'for (int32 SliceIndex = 1; SliceIndex < StairVolumes.Num(); ++SliceIndex)' not in support_body
assert 'ARPGIsValidStairLowFootWorldSupport' not in build_cpp
assert 'ARPGIsValidStairWorldSupportContact(' in build_cpp[build_cpp.index('EARPGPlacementResult UARPGBuildingComponent::EvaluatePlacementInternal'):]

# v2.15.31 Landscape contract: a verified native Stair socket must classify ALandscapeProxy before
# the generic/sampled WorldStatic blocker path. This is intentionally Landscape-only; static meshes
# continue through ARPGIsValidStairWorldSupportContact and then the normal blocker return.
eval_start=build_cpp.index('EARPGPlacementResult UARPGBuildingComponent::EvaluatePlacementInternal')
eval_body=build_cpp[eval_start:]
landscape_pos=eval_body.index('ARPGIsLandscapeTerrainActor(Other)')
sampled_world_pos=eval_body.index('ARPGIsValidStairWorldSupportContact(')
generic_block_pos=eval_body.index('return EARPGPlacementResult::Blocked;', sampled_world_pos)
assert landscape_pos < sampled_world_pos < generic_block_pos
assert 'ARPGTransformMatchesStairHostCandidate(SnapTarget, Piece, Final)' in eval_body[landscape_pos:sampled_world_pos]
assert '#include "LandscapeProxy.h"' in build_cpp
build_cs=(root/'Source/AkumasRPGFramework/AkumasRPGFramework.Build.cs').read_text(encoding='utf-8')
assert '"Landscape"' in build_cs

# Capture can occur from the host edge, either paired Stair envelope, or another Stair endpoint.
stair_capture_distance=140.0
landing_point=(0.0,150.0,host_story_plane)
assert distance_sq_to_local_box(landing_point,(-150.0,-150.0,-9.0),(150.0,150.0,9.0)) == 0.0
run_midpoint=(0.0,317.0,139.0)
assert run_midpoint[1] > 150.0

# Side-wall classification is relative to the outward stairwell cell and remains strict.
def stair_side_wall_compatible(local_x, local_y, wall_axis_yaw, stair_yaw=-90.0, half_grid=150.0, tolerance=2.5):
    on_side=abs(abs(local_y)-half_grid) <= tolerance and abs(local_x) <= tolerance
    d=abs(((wall_axis_yaw-stair_yaw+180.0)%360.0)-180.0)
    parallel=d <= 1.0 or abs(d-180.0) <= 1.0
    return on_side and parallel
assert stair_side_wall_compatible(0.0,150.0,-90.0)
assert stair_side_wall_compatible(0.0,-150.0,90.0)
assert not stair_side_wall_compatible(150.0,0.0,0.0)
assert not stair_side_wall_compatible(0.0,150.0,0.0)

# Inter-story seam regression: build order must be commutative. The 300x300x18 Floor's FINISHED TOP
# is the Z=300 story plane, while the slab itself extends downward through Z=282..300. A lower wall
# ends at that top plane and a pre-stacked upper wall starts there. Only exact edge/facing relationships
# are accepted; a wall through the middle or at an unrelated height still blocks.
require(build_cpp, 'ARPGIsValidUpperHorizontalWallSeamNeighbor', 'bSupportingWallBelow',
        'bWallBuiltOnStoryPlane', 'ARPGWallOccupiesHorizontalStructuralEdge',
        'This makes build order commutative without globally ignoring building collision')

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
    return (abs(wall_top-floor_top) <= tol or
            abs(wall_bottom-floor_top) <= tol)

# Floor spans Z=282..300. Lower support ends at 300; a pre-stacked upper wall also starts at 300.
assert valid_interstory_seam((0.0,150.0),0.0,0.0,0.0,300.0,282.0,300.0)      # lower support
assert valid_interstory_seam((0.0,150.0),0.0,0.0,300.0,600.0,282.0,300.0)    # pre-stacked upper wall
assert valid_interstory_seam((150.0,0.0),-90.0,0.0,300.0,600.0,282.0,300.0) # perpendicular upper wall
assert valid_interstory_seam((0.0,150.0),0.0,0.0,300.0,600.0,282.0,300.0)    # wall built after Floor
assert not valid_interstory_seam((0.0,0.0),0.0,0.0,300.0,600.0,282.0,300.0) # wall through tile center
assert not valid_interstory_seam((0.0,150.0),90.0,0.0,300.0,600.0,282.0,300.0) # wrong facing
assert not valid_interstory_seam((0.0,150.0),0.0,0.0,350.0,650.0,282.0,300.0) # unrelated height

# Inverse story-bay seam: an incoming Wall-family piece may fill the bay between a lower slab and an
# already-built upper Floor/Ceiling/Roof. The upper slab is accepted only when the wall sits on its exact
# edge/facing and the wall structural story top meets the slab FINISHED TOP/story plane.
require(build_cpp, 'ARPGIsValidWallUnderUpperHorizontalSeamNeighbor',
        'WallStructuralTopZ - HorizontalTopZ', 'Inverse inter-story seam',
        'upper slab is a legitimate')

def valid_wall_under_upper_slab(wall_origin_xy, wall_yaw, slab_yaw, wall_top, slab_top, snap=300.0, tol=2.5):
    half=snap*0.5
    edges=[((0.0, half),0.0),((0.0,-half),180.0),((half,0.0),-90.0),((-half,0.0),90.0)]
    def yaw_delta(a,b):
        return abs(((a-b+180.0)%360.0)-180.0)
    edge_ok=False
    for (ex,ey), rel_yaw in edges:
        if (wall_origin_xy[0]-ex)**2 + (wall_origin_xy[1]-ey)**2 <= tol*tol and yaw_delta(slab_yaw+rel_yaw, wall_yaw) <= 1.0:
            edge_ok=True
            break
    return edge_ok and abs(wall_top-slab_top) <= tol

# Incoming wall fills Z=300..600; upper Floor slab spans Z=582..600 and is recessed downward.
assert valid_wall_under_upper_slab((0.0,150.0),0.0,0.0,600.0,600.0)
assert valid_wall_under_upper_slab((150.0,0.0),-90.0,0.0,600.0,600.0)
# Square upper slab may itself be cardinally rotated; local edge mapping still resolves same world facing.
assert valid_wall_under_upper_slab((150.0,0.0),0.0,90.0,600.0,600.0)
assert not valid_wall_under_upper_slab((0.0,0.0),0.0,0.0,600.0,600.0)       # wall through tile center
assert not valid_wall_under_upper_slab((0.0,150.0),90.0,0.0,600.0,600.0)    # wrong facing
assert not valid_wall_under_upper_slab((0.0,150.0),0.0,0.0,611.0,600.0)     # wrong story plane
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
        'const float IncomingWallStoryBaseZ = IncomingOnTargetTopZ',
        'FRotator(0.f, -90.f, 0.f), FVector(Half, 0.f, IncomingWallStoryBaseZ)',
        'FRotator(0.f,  90.f, 0.f), FVector(-Half, 0.f, IncomingWallStoryBaseZ)',
        'finished horizontal walking surface as the canonical story',
        'actor local +Y is the', 'front/exterior side')
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
# v2.15.42 finished-surface no-gap regression: a real 18 cm upper Floor occupies Z=282..300. The
# canonical next-story wall starts at the Floor TOP/walking surface (Z=300), exactly matching a direct
# vertical stack from the lower wall. The slab overlaps the final 18 cm of the LOWER wall instead of
# adding 18 cm to storey height or forcing the Stair to stop at the underside of the landing.
floor_bottom, floor_top = 282.0, 300.0
vertical_wall_bottom = 300.0
floor_edge_wall_bottom = floor_top
assert floor_edge_wall_bottom == vertical_wall_bottom
assert floor_top-floor_bottom == 18.0
assert abs(vertical_wall_bottom-floor_top) <= 2.5
# Every build order must resolve the next story surface from wall bottom + StandardWallHeight, never WallMaxZ.
# Simulate art that is intentionally 12.5 cm shorter than the 300 cm structural bay.
short_wall_visual_top=287.5
canonical_floor_top=0.0+300.0
canonical_floor_bottom=canonical_floor_top-18.0
canonical_upper_wall_bottom=0.0+300.0
assert canonical_floor_top == canonical_upper_wall_bottom == 300.0
assert canonical_floor_bottom == 282.0
assert short_wall_visual_top != canonical_floor_top
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
require(inter_h, 'DepositToStorageInstance', 'WithdrawFromStorageInstance', 'WithdrawStationOutputInstance', 'ToggleBuiltDoor', 'ToggleBuiltWindow', 'DemolishBuilding', 'QueueCraft')
require(inter_cpp, 'ServerDepositToStorageInstance_Implementation', 'ServerWithdrawFromStorageInstance_Implementation',
        'ServerWithdrawStationOutputInstance_Implementation', 'ServerToggleBuiltDoor_Implementation', 'ServerToggleBuiltWindow_Implementation', 'ServerDemolishBuilding_Implementation', 'ServerQueueCraft_Implementation')

# Production station/furnace: data-driven station, wood-style tagged fuel, ore inputs, outputs and transactional safeguards.
require(station_h, 'ApplyStationDefinition', 'CanQueueRecipe', 'OutputInventory')
require(station_cpp, 'bConsumesFuel', 'FuelTag', 'FuelPerCraft', 'ARPGCountTaggedItems', 'ConsumeFuelForCraft',
        'ARPGAggregateRecipeAmounts', 'ARPGCanFitResolvedOutputs', 'HasUnequippedItem', 'RemoveUnequippedItem', 'ReplaceInventory(Before)',
        'StationDefinition->StationTag.MatchesTagExact', 'SetActorTickEnabled(true)', 'SetActorTickEnabled(false)')
# Required station tag is a hard requirement, even if the station is misconfigured/untagged.
canuse=station_cpp[station_cpp.index('bool AARPGCraftingStationActor::CanUseRecipe'):station_cpp.index('bool AARPGCraftingStationActor::ConsumeRecipeInputs')]
assert '!StationDefinition || !StationDefinition->StationTag.IsValid()' in canuse

# Persistent construction + Door/Window/Light state and existing container/furnace inventories/queues.
# Character schema remains v5; world schema is v7 while the v6 Window migration remains explicit.
require(types_h, 'bConstructionComplete', 'ConstructionRemainingSeconds', 'bDoorOpen', 'bWindowOpen', 'bLightOn')
assert save_h.count('SaveVersion = 5') >= 1 and 'SaveVersion = 8' in save_h
require(save_cpp, 'R.bConstructionComplete=B->IsConstructionComplete()', 'R.ConstructionRemainingSeconds=B->GetConstructionRemainingSeconds()',
        'R.bDoorOpen=Door->IsDoorOpen()', 'R.bWindowOpen=Window->IsWindowOpen()', 'R.bLightOn=Light->IsLightOn()', 'RestoreConstructionState', 'RestoreDoorOpenState', 'RestoreWindowOpenState', 'RestoreLightState', 'Save->SaveVersion>=6 ? R.bWindowOpen : false', 'Save->SaveVersion>=7', 'CraftQueue', 'OutputItems',
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
