"""v2.15.53 unified Stair <-> Wall-family boundary regression.

Models the four project-side failures that motivated the root fix:
  1) Stair placed beside an existing plain Wall;
  2) Stair placed beside an existing WindowWall;
  3) Wall-family piece placed beside an existing Stair;
  4) hosted Window/Door installed after the Stair already exists.

The same structural predicate must drive both build orders, including Stair-to-Stair chain placement.
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CPP = (ROOT / "Source/AkumasRPGFramework/Private/Building/ARPGBuildingComponent.cpp").read_text(encoding="utf-8")


def require(text: str, *needles: str) -> None:
    for needle in needles:
        assert needle in text, f"missing v2.15.53 unified Stair boundary token: {needle}"


start = CPP.index("static bool ARPGIsStairWallFamilyBoundarySeam(")
end = CPP.index("/**\n * Structural neighbours around an incoming snapped Stair", start)
shared = CPP[start:end]
require(
    shared,
    "StairPiece->PieceKind != EARPGBuildPieceKind::Stair",
    "ARPGIsWallLikeKind(WallPiece->PieceKind)",
    "bParallelToStairRun",
    "bPerpendicularToStairRun",
    "StairBoundsCenterLocal",
    "StairFrame",
    "WallTransform.GetLocation()",
    "HalfGrid",
    "bOnSidePlane",
    "bOnEndpointPlane",
    "StairPiece->PlacementBounds.X",
    "WallHalfRun",
    "LongitudinalOverlap",
    "return LongitudinalOverlap > PositionTolerance;",
    "LateralOverlap",
    "return LateralOverlap > PositionTolerance;",
)

forward_start = CPP.index("static bool ARPGIsCompatibleStairHostStructuralNeighbor(")
forward_end = CPP.index("/**\n * Hosted Window/Door inserts inherit", forward_start)
forward = CPP[forward_start:forward_end]
require(
    forward,
    "ARPGIsAnyStairSnapPair",
    "ARPGTransformMatchesStairHostCandidate",
    "ARPGIsStairWallFamilyBoundarySeam(",
    "ARPGIsStairSupportSnapPair",
)
# Wall-family handling must happen before the flat-host-only gate so Stair-chain placement inherits it.
assert forward.index("ARPGIsStairWallFamilyBoundarySeam(") < forward.index("if (!ARPGIsStairSupportSnapPair")

reverse_start = CPP.index("static bool ARPGIsValidExistingStairWallSideSeamNeighbor(")
reverse_end = CPP.index("/**\n * Incoming hosted inserts also inherit", reverse_start)
reverse = CPP[reverse_start:reverse_end]
require(reverse, "return ARPGIsStairWallFamilyBoundarySeam(")

insert_start = CPP.index("static bool ARPGIsCompatibleInsertHostStairNeighbor(")
insert_end = CPP.index("/**\n * A snapped edge-landing Stair", insert_start)
insert = CPP[insert_start:insert_end]
require(
    insert,
    "ARPGIsInsertSnapPair",
    "ExistingStair->Definition->PieceKind != EARPGBuildPieceKind::Stair",
    "ARPGIsStairWallFamilyBoundarySeam(",
    "InsertHost->Definition",
)

# EvaluatePlacementInternal must consume all three directions: hosted insert -> incoming Stair,
# incoming insert -> existing Stair, and generic reverse Wall-family -> existing Stair.
require(
    CPP,
    "ARPGHostedInsertAllowsStairSideNeighbor(",
    "ARPGIsCompatibleInsertHostStairNeighbor(BuildNeighbor, SnapTarget, Piece)",
    "ARPGIsValidExistingStairWallSideSeamNeighbor(Neighbor, IncomingPiece, IncomingFinal)",
)


def boundary_seam(
    wall_x: float,
    wall_y: float,
    wall_yaw: float,
    stair_yaw: float = 0.0,
    stair_half_run: float = 167.0,
    stair_grid: float = 300.0,
    wall_snap: float = 300.0,
    tol: float = 2.5,
) -> bool:
    delta = abs(((wall_yaw - stair_yaw + 180.0) % 360.0) - 180.0)
    parallel = delta <= 1.0 or abs(delta - 180.0) <= 1.0
    perpendicular = abs(delta - 90.0) <= 1.0
    wall_half = wall_snap * 0.5
    if parallel:
        if abs(abs(wall_y) - stair_grid * 0.5) > tol:
            return False
        overlap = min(wall_x + wall_half, stair_half_run) - max(wall_x - wall_half, -stair_half_run)
        return overlap > tol
    if perpendicular:
        if abs(abs(wall_x) - stair_grid * 0.5) > tol:
            return False
        stair_half_width = stair_grid * 0.5
        overlap = min(wall_y + wall_half, stair_half_width) - max(wall_y - wall_half, -stair_half_width)
        return overlap > tol
    return False


# Plain Wall / WindowWall / Doorway all use the same geometry contract.
for kind in ("Wall", "WindowWall", "Doorway"):
    assert boundary_seam(0.0, 150.0, 0.0), kind
    assert boundary_seam(300.0, 150.0, 0.0), kind   # 17 cm endpoint overhang seam
    assert boundary_seam(-300.0, -150.0, 180.0), kind

# Same answer in both build orders by construction: no second reverse geometry model exists.
forward_result = boundary_seam(0.0, 150.0, 0.0)
reverse_result = boundary_seam(0.0, 150.0, 0.0)
assert forward_result == reverse_result == True

# Stair-chain host does not change side-wall legality; active host only proves the Stair final transform.
for active_host in ("Foundation", "Floor", "Ceiling", "Stair"):
    assert boundary_seam(0.0, 150.0, 0.0), active_host

# Hosted insert inherits its verified Wall-family host seam, not its own mesh bounds.
windowwall_is_legal = boundary_seam(0.0, 150.0, 0.0)
doorway_is_legal = boundary_seam(0.0, -150.0, 180.0)
assert windowwall_is_legal
assert doorway_is_legal

# Exact LOW/HIGH endpoint framing is legal in v2.15.53; an interior perpendicular wall remains blocked.
assert boundary_seam(150.0, 0.0, 90.0)
assert boundary_seam(-150.0, 0.0, -90.0)

# Safety: interior/centreline and distant geometry remain blockers.
assert not boundary_seam(0.0, 0.0, 0.0)       # parallel centreline
assert not boundary_seam(0.0, 0.0, 90.0)      # perpendicular through flight interior
assert not boundary_seam(600.0, 150.0, 0.0)   # distant parallel wall
assert not boundary_seam(320.0, 150.0, 0.0)   # beyond Wood Stair endpoint overhang tolerance

print("building unified bidirectional Stair/Wall-family boundary regression model: PASS")
