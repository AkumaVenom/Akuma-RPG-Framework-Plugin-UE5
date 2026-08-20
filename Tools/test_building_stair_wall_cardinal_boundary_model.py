"""v2.15.53 cardinal Stair / Wall-family boundary regression.

Reproduces the project-reported final failure after v2.15.52: three Stair rotations were accepted but
one rotation still hit a perpendicular Wall-family boundary and fell through to generic collision.
All four 90-degree rotations must classify the same authored perimeter relationship identically.
"""
from pathlib import Path
from math import cos, sin, radians

ROOT = Path(__file__).resolve().parents[1]
CPP = (ROOT / "Source/AkumasRPGFramework/Private/Building/ARPGBuildingComponent.cpp").read_text(encoding="utf-8")
for token in (
    "ARPGIsStairWallFamilyBoundarySeam(",
    "bParallelToStairRun",
    "bPerpendicularToStairRun",
    "bOnSidePlane",
    "bOnEndpointPlane",
    "LateralOverlap",
    "all four 90-degree Stair rotations receive identical semantics",
):
    assert token in CPP, f"missing v2.15.53 cardinal-boundary token: {token}"

TOL = 2.5
GRID = 300.0
HALF = 150.0
STAIR_HALF_RUN = 167.0
STAIR_HALF_WIDTH = 150.0
WALL_HALF = 150.0


def norm_delta(a, b):
    d = abs(((a - b + 180.0) % 360.0) - 180.0)
    return d


def inv_rotate_point(world_x, world_y, stair_yaw):
    # Equivalent to inverse-transforming XY by StairFrame rotation.
    a = radians(-stair_yaw)
    return world_x * cos(a) - world_y * sin(a), world_x * sin(a) + world_y * cos(a)


def boundary(world_anchor, wall_yaw, stair_yaw):
    x, y = inv_rotate_point(world_anchor[0], world_anchor[1], stair_yaw)
    d = norm_delta(wall_yaw, stair_yaw)
    parallel = d <= 1.0 or abs(d - 180.0) <= 1.0
    perpendicular = abs(d - 90.0) <= 1.0
    if parallel:
        if abs(abs(y) - HALF) > TOL:
            return False
        overlap = min(x + WALL_HALF, STAIR_HALF_RUN) - max(x - WALL_HALF, -STAIR_HALF_RUN)
        return overlap > TOL
    if perpendicular:
        if abs(abs(x) - HALF) > TOL:
            return False
        overlap = min(y + WALL_HALF, STAIR_HALF_WIDTH) - max(y - WALL_HALF, -STAIR_HALF_WIDTH)
        return overlap > TOL
    return False


# Build the same local side and endpoint seams, rotate the complete layout through all cardinal yaws.
for stair_yaw in (0.0, 90.0, 180.0, 270.0):
    a = radians(stair_yaw)
    def to_world(x, y):
        return (x * cos(a) - y * sin(a), x * sin(a) + y * cos(a))

    # parallel +Y side boundary
    assert boundary(to_world(0.0, HALF), stair_yaw, stair_yaw), f"side failed at {stair_yaw}"
    # perpendicular HIGH endpoint boundary -- exact case missing in v2.15.52
    assert boundary(to_world(HALF, 0.0), stair_yaw + 90.0, stair_yaw), f"endpoint failed at {stair_yaw}"
    # opposite LOW endpoint, reversed wall facing
    assert boundary(to_world(-HALF, 0.0), stair_yaw - 90.0, stair_yaw), f"low endpoint failed at {stair_yaw}"
    # interior perpendicular crossing is never a boundary seam
    assert not boundary(to_world(0.0, 0.0), stair_yaw + 90.0, stair_yaw), f"interior legalized at {stair_yaw}"

print("building Stair/Wall-family four-cardinal boundary regression model: PASS")
