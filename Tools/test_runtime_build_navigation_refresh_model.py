"""Runtime build navigation invalidation regression — v2.16.8."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CPP = (ROOT / 'Source/AkumasRPGFramework/Private/Building/ARPGBuildPieceActor.cpp').read_text(encoding='utf-8', errors='ignore')
H = (ROOT / 'Source/AkumasRPGFramework/Public/Building/ARPGBuildPieceActor.h').read_text(encoding='utf-8', errors='ignore')

assert 'void RefreshRuntimeNavigation();' in H

for token in (
    'RefreshConstructionPresentation(true);\n    RefreshRuntimeNavigation();',
    'ENavigationDirtyFlag::All',
    'UpdateComponentInNavOctree',
    'UpdateNavOctreeBounds',
    'ARPG Runtime Build Piece Changed',
    'Grid * 0.35f',
    'Story * 0.35f',
):
    assert token in CPP, token

# Never solve runtime construction by forcing a full-world synchronous Build() per piece.
refresh_start = CPP.index('void AARPGBuildPieceActor::RefreshRuntimeNavigation()')
refresh_end = CPP.index('bool AARPGBuildPieceActor::HasActiveStairNavigationBridge()', refresh_start)
refresh_body = CPP[refresh_start:refresh_end]
assert 'Nav->Build()' not in refresh_body
assert 'Build()' not in refresh_body

# v2.16.8 intentionally removed automatic off-mesh Stair links.
for forbidden in ('ANavLinkProxy', 'PointLinks', 'RefreshNearbyStairNavigationBridges'):
    assert forbidden not in CPP, forbidden

print('runtime build local Dynamic Recast refresh regression model: PASS')
