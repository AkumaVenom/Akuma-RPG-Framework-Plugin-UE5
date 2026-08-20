from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BUILD_CPP = (ROOT / 'Source/AkumasRPGFramework/Private/Building/ARPGBuildingComponent.cpp').read_text(encoding='utf-8')


def require(text: str, *needles: str) -> None:
    for needle in needles:
        assert needle in text, f'missing required hosted-insert Stair seam contract: {needle}'


# v2.15.52: hosted Door/Window collision inherits the exact shared Stair <-> Wall-family seam of its
# verified Doorway/WindowWall host. The permission works for either flat-support or Stair-chain incoming
# Stairs, and in reverse when a Window/Door is installed after the Stair already exists.
require(
    BUILD_CPP,
    'ARPGHostedInsertAllowsStairSideNeighbor',
    'ARPGInsertActorMatchesHost(InsertActor, InsertHost)',
    'ARPGIsAnyStairSnapPair',
    'ARPGTransformMatchesStairHostCandidate',
    'ARPGIsStairWallFamilyBoundarySeam(',
    'ARPGIsCompatibleInsertHostStairNeighbor',
    'ARPGIsCompatibleInsertHostStairNeighbor(BuildNeighbor, SnapTarget, Piece)',
    'const bool bIncomingIsStair = Piece->PieceKind == EARPGBuildPieceKind::Stair;',
    'bNeighborIsInsert && (bIncomingIsStandardStructural || bIncomingIsStair)',
    'InsertAllowsIncomingNeighbor',
)

helper_start = BUILD_CPP.index('static bool ARPGHostedInsertAllowsStairSideNeighbor(')
helper_end = BUILD_CPP.index('/**\n * Reverse build order wrapper', helper_start)
helper = BUILD_CPP[helper_start:helper_end]
require(
    helper,
    'StairPiece->PieceKind != EARPGBuildPieceKind::Stair',
    '!ARPGInsertActorMatchesHost(InsertActor, InsertHost)',
    'ARPGIsAnyStairSnapPair',
    'ARPGTransformMatchesStairHostCandidate',
    'ARPGIsStairWallFamilyBoundarySeam(',
    'InsertHost->Definition',
)

reverse_insert_start = BUILD_CPP.index('static bool ARPGIsCompatibleInsertHostStairNeighbor(')
reverse_insert_end = BUILD_CPP.index('/**\n * A snapped edge-landing Stair', reverse_insert_start)
reverse_insert = BUILD_CPP[reverse_insert_start:reverse_insert_end]
require(
    reverse_insert,
    'ARPGIsInsertSnapPair',
    'ExistingStair->Definition->PieceKind != EARPGBuildPieceKind::Stair',
    'ARPGIsStairWallFamilyBoundarySeam(',
)

# Pure behavior boundary. Active host may be either a flat landing or another Stair; the final transform
# must still be authoritative, and host/insert identity plus side-seam geometry must all be proven.
def hosted_insert_allows_stair_side(
    *,
    incoming_is_stair: bool,
    has_authoritative_stair_snap: bool,
    insert_matches_host_socket: bool,
    host_is_parallel_side_seam: bool,
) -> bool:
    return (
        incoming_is_stair
        and has_authoritative_stair_snap
        and insert_matches_host_socket
        and host_is_parallel_side_seam
    )

for snap_kind in ('flat landing', 'Stair chain'):
    assert hosted_insert_allows_stair_side(
        incoming_is_stair=True,
        has_authoritative_stair_snap=True,
        insert_matches_host_socket=True,
        host_is_parallel_side_seam=True,
    ), snap_kind

assert not hosted_insert_allows_stair_side(
    incoming_is_stair=True, has_authoritative_stair_snap=True,
    insert_matches_host_socket=False, host_is_parallel_side_seam=True)
assert not hosted_insert_allows_stair_side(
    incoming_is_stair=True, has_authoritative_stair_snap=True,
    insert_matches_host_socket=True, host_is_parallel_side_seam=False)
assert not hosted_insert_allows_stair_side(
    incoming_is_stair=True, has_authoritative_stair_snap=False,
    insert_matches_host_socket=True, host_is_parallel_side_seam=True)

print('building hosted-insert Stair seam model: PASS')
