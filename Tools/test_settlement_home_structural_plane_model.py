from pathlib import Path
import math

root = Path(__file__).resolve().parents[1]
src = root / 'Source' / 'AkumasRPGFramework'

def read(rel):
    return (src / rel).read_text(errors='ignore')

def require(text, *tokens):
    for token in tokens:
        assert token in text, f'missing token: {token}'

hub = read('Private/Settlement/ARPGSettlementHubActor.cpp')
defn = read('Public/Data/ARPGSettlementDefinition.h')
types = read('Public/ARPGTypes.h')
widgets = read('Private/UI/ARPGSettlementWidgets.cpp')

# Current design contract: 2x2 is the default minimum, still configurable per Settlement Definition.
require(defn, 'int32 MinimumFoundationWidth = 2;', 'int32 MinimumFoundationDepth = 2;')
require(types,
        'int32 RequiredFoundationCount = 4;',
        'int32 RequiredCoverCount = 4;',
        'int32 RequiredPerimeterSegmentCount = 8;')
require(widgets, '2x2+ by default')

# Root fix: settlement validation must use the same transformed asset bounds/planes as building placement.
require(hub,
        'SettlementGetDefinitionLocalBounds',
        'BuildSkeletalMesh.LoadSynchronous()',
        'BuildMesh.LoadSynchronous()',
        'Piece->MeshRelativeTransform',
        'SettlementBuildSpatial',
        'SettlementProjectedDistanceSquaredToBounds',
        'SettlementProjectedContains',
        'BedSpatial.WorldMinZ',
        'FoundationTopZ = Anchor->WorldMaxZ',
        'FoundationCell->WorldMaxZ + Story',
        'South->WorldCenter - YAxis * (Grid * 0.5f)',
        'North->WorldCenter + YAxis * (Grid * 0.5f)',
        'West->WorldCenter - XAxis * (Grid * 0.5f)',
        'East->WorldCenter + XAxis * (Grid * 0.5f)',
        'UsedPerimeterPieces',
        'MissingScore')

# The v2.16.0-v2.16.2 failure mode must not return: raw actor origins are not house cell centers.
for forbidden in (
    'B->GetActorLocation().Z - (Origin.Z + Story)',
    'FVector::DistSquared2D(B->GetActorLocation(), Target)',
):
    assert forbidden not in hub, f'raw actor-origin housing regression returned: {forbidden}'

# Independent geometry sanity model for the reported PIE failure:
# A modular cover can have an off-centre/corner pivot while its transformed visible footprint is exactly
# over the Foundation cell. Origin-distance matching rejects it; projected-bounds matching accepts it.
def projected_contains(actor_xy, local_min, local_max, target_xy, tolerance=0.0):
    lx = target_xy[0] - actor_xy[0]
    ly = target_xy[1] - actor_xy[1]
    return (local_min[0] - tolerance <= lx <= local_max[0] + tolerance and
            local_min[1] - tolerance <= ly <= local_max[1] + tolerance)

cell_center = (0.0, 0.0)
cover_actor_origin = (130.0, -90.0)
cover_local_min = (-280.0, -60.0)
cover_local_max = (20.0, 240.0)
old_origin_distance = math.dist(cover_actor_origin, cell_center)
assert old_origin_distance > 34.0, 'representative old origin matcher should fail this pivot layout'
assert projected_contains(cover_actor_origin, cover_local_min, cover_local_max, cell_center, 24.0), \
    'pivot-aware cover footprint should contain the Foundation cell center'

# Same principle for a Wall-family piece: the semantic Foundation boundary may lie inside the wall's
# transformed envelope even when the actor pivot is over 100 cm away from that boundary point.
wall_target = (150.0, 0.0)
wall_actor_origin = (150.0, 110.0)
wall_local_min = (-150.5, -125.5)
wall_local_max = (150.5, -94.5)
assert math.dist(wall_actor_origin, wall_target) > 42.0
assert projected_contains(wall_actor_origin, wall_local_min, wall_local_max, wall_target, 24.0), \
    'pivot-aware perimeter matching should accept the real wall envelope'

print('Settlement structural-plane / pivot-aware home validation model: PASS')
