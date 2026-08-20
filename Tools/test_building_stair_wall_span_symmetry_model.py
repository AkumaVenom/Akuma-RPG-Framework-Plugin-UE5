from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BUILD_CPP = (ROOT / "Source/AkumasRPGFramework/Private/Building/ARPGBuildingComponent.cpp").read_text(encoding="utf-8")


def require(text: str, *needles: str) -> None:
    for needle in needles:
        assert needle in text, f"missing Stair/Wall-family seam contract: {needle}"


# v2.15.52 root fix: both incoming-Stair and Stair-first Wall-family validation consume one shared
# classifier. Structural side-plane/yaw ownership stays on the 300 cm lattice while longitudinal
# tolerance uses the Stair PlacementBounds half-run, preserving the Wood Stair's 17 cm endpoint seam.
start = BUILD_CPP.index("static bool ARPGIsStairWallFamilyBoundarySeam(")
end = BUILD_CPP.index("/**\n * Structural neighbours around an incoming snapped Stair", start)
body = BUILD_CPP[start:end]
require(
    body,
    "bOnSidePlane",
    "bParallelToStairRun",
    "StairBoundsCenterLocal",
    "StairFrame",
    "WallTransform.GetLocation()",
    "StairPiece->PlacementBounds.X",
    "WallHalfRun",
    "WallRunMin",
    "WallRunMax",
    "LongitudinalOverlap",
    "return LongitudinalOverlap > PositionTolerance;",
)
assert "FMath::Abs(NeighborInCell.X) <= PositionTolerance" not in body

reverse_start = BUILD_CPP.index("static bool ARPGIsValidExistingStairWallSideSeamNeighbor(")
reverse_end = BUILD_CPP.index("/**\n * Incoming hosted inserts also inherit", reverse_start)
reverse = BUILD_CPP[reverse_start:reverse_end]
require(reverse, "return ARPGIsStairWallFamilyBoundarySeam(")

def compatible(
    anchor_x: float,
    anchor_y: float,
    wall_yaw: float,
    stair_yaw: float = 0.0,
    grid: float = 300.0,
    wall_snap: float = 300.0,
    stair_art_min: float = -167.0,
    stair_art_max: float = 167.0,
    tol: float = 2.5,
) -> bool:
    delta = abs(((wall_yaw - stair_yaw + 180.0) % 360.0) - 180.0)
    parallel = delta <= 1.0 or abs(delta - 180.0) <= 1.0
    half_grid = grid * 0.5
    on_side = abs(abs(anchor_y) - half_grid) <= tol
    if not (parallel and on_side):
        return False

    half_wall = max(1.0, wall_snap * 0.5)
    wall_min, wall_max = anchor_x - half_wall, anchor_x + half_wall
    overlap = min(wall_max, stair_art_max) - max(wall_min, stair_art_min)
    return overlap > tol


# Direct side-wall module remains legal.
assert compatible(0.0, 150.0, 0.0)
assert compatible(140.0, 150.0, 0.0)
assert compatible(-140.0, -150.0, 180.0)

# Exact reported regression family: a 334 cm Stair overhangs the 300 cm structural endpoint by 17 cm.
# Side-wall modules centred one full grid cell away start at +/-150 and are therefore touched by that
# overhang. They are legitimate continuation modules and must not turn the Stair red.
assert compatible(300.0, 150.0, 0.0)
assert compatible(-300.0, -150.0, 180.0)

# The old v2.15.50 structural-cell-only comparison would see zero overlap for anchor_x=300 and reject it.
def old_v21550_structural_cell_only(anchor_x: float, wall_snap: float = 300.0, grid: float = 300.0) -> bool:
    half_wall = wall_snap * 0.5
    half_grid = grid * 0.5
    overlap = min(anchor_x + half_wall, half_grid) - max(anchor_x - half_wall, -half_grid)
    return overlap > 2.5


assert not old_v21550_structural_cell_only(300.0)
assert compatible(300.0, 150.0, 0.0)

# Safety boundaries remain strict.
assert not compatible(0.0, 150.0, 90.0)    # perpendicular end wall
assert not compatible(0.0, 0.0, 0.0)       # centreline wall
assert not compatible(320.0, 150.0, 0.0)   # beyond the 17 cm authored overhang + tolerance
assert not compatible(600.0, 150.0, 0.0)   # unrelated distant parallel segment

print("building Stair/Wall-family transformed-overhang seam symmetry model: PASS")
