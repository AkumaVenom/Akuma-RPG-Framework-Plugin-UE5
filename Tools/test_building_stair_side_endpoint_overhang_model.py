"""Focused model for the project-reported v2.15.50 plain-Wall Stair regression."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CPP = (ROOT / "Source/AkumasRPGFramework/Private/Building/ARPGBuildingComponent.cpp").read_text(encoding="utf-8")

start = CPP.index("static bool ARPGIsStairWallFamilyBoundarySeam(")
end = CPP.index("/**\n * Structural neighbours around an incoming snapped Stair", start)
body = CPP[start:end]

for needle in (
    "intentional endpoint art/validation overhang",
    "Wood = 167 cm",
    "StairPiece->PlacementBounds.X",
    "WallHalfRun",
    "WallRunMin",
    "WallRunMax",
    "LongitudinalOverlap",
):
    assert needle in body, f"missing endpoint-overhang fix token: {needle}"

# Current Wood dimensions / structural lattice.
stair_half_art = 334.0 / 2.0
half_grid = 300.0 / 2.0
wall_half = 300.0 / 2.0
intentional_overhang = stair_half_art - half_grid
assert intentional_overhang == 17.0

# Adjacent side module centred at +300 spans [150, 450]. It touches only the Stair's intentional
# [150, 167] endpoint overhang, but that is enough for the broad placement volumes to report overlap.
wall_min, wall_max = 300.0 - wall_half, 300.0 + wall_half
art_overlap = min(wall_max, stair_half_art) - max(wall_min, -stair_half_art)
structural_overlap = min(wall_max, half_grid) - max(wall_min, -half_grid)
assert art_overlap == 17.0
assert structural_overlap == 0.0
assert art_overlap > 2.5

# A wall beyond the authored art does not receive the seam exception.
far_min, far_max = 320.0 - wall_half, 320.0 + wall_half
far_overlap = min(far_max, stair_half_art) - max(far_min, -stair_half_art)
assert far_overlap < 0.0

print("building Stair side endpoint-overhang regression model: PASS")
