"""Native Dynamic Recast Stair navigation regression — v2.16.8.

Once the project's Supported Agent is configured so the authored Stair rasterizes as a real walkable
surface, Akuma RPG Framework must not publish competing off-mesh NavLinkProxy shortcuts. Runtime
building changes still dirty local Dynamic Recast tiles so AI uses the actual Stair polygons.
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "Source/AkumasRPGFramework"

def read(rel):
    return (SRC / rel).read_text(encoding="utf-8", errors="ignore")

def require(text, *tokens):
    for token in tokens:
        assert token in text, f"missing token: {token}"

actor_h = read("Public/Building/ARPGBuildPieceActor.h")
def_h = read("Public/Data/ARPGBuildPieceDefinition.h")
actor = read("Private/Building/ARPGBuildPieceActor.cpp")

# Dynamic Recast remains the only runtime Stair navigation mechanism.
require(actor,
        'SetCanEverAffectNavigation(true)',
        'UNavigationSystemV1::UpdateComponentInNavOctree(*ActiveMesh)',
        'UNavigationSystemV1::UpdateNavOctreeBounds(this)',
        'Nav->AddDirtyArea(DirtyBounds, ENavigationDirtyFlag::All',
        'ARPG Runtime Build Piece Changed',
        'Definition->PieceKind == EARPGBuildPieceKind::Stair',
        'Grid * 0.35f',
        'Story * 0.35f')

# The off-mesh bridge implementation and its authoring knobs are gone.
for forbidden in (
    'ANavLinkProxy',
    'Navigation/NavLinkProxy.h',
    'FNavigationLink StairLink',
    'PointLinks',
    'StairNavigationProxy',
    'ResolveStairNavigationBridgeEndpoints',
    'ScheduleStairNavigationBridgeRefresh',
    'RefreshNearbyStairNavigationBridges',
    'StairNavigationRefreshRetryCount',
    'bEnableAutomaticStairNavigationBridge',
    'StairNavigationLandingInset',
    'StairNavigationProjectionRadius',
    'StairNavigationProjectionHalfHeight',
):
    assert forbidden not in actor + def_h, f"obsolete Stair NavLink machinery remains: {forbidden}"

# Keep old Blueprint nodes source-compatible, but they must not create links.
require(actor_h,
        'void RefreshStairNavigationBridge();',
        'bool HasActiveStairNavigationBridge() const;',
        'DeprecatedFunction',
        'Automatic Stair NavLinks were removed in v2.16.8')
require(actor,
        'bool AARPGBuildPieceActor::HasActiveStairNavigationBridge() const',
        'void AARPGBuildPieceActor::RefreshStairNavigationBridge()',
        'return false;',
        'RefreshRuntimeNavigation();')

bridge_start = actor.index('void AARPGBuildPieceActor::RefreshStairNavigationBridge()')
bridge_end = actor.index('float AARPGBuildPieceActor::GetAuthoritativeServerTime()', bridge_start)
bridge = actor[bridge_start:bridge_end]
for forbidden in ('SpawnActor', 'NavLink', 'ProjectPointToNavigation', 'PointLinks'):
    assert forbidden not in bridge, f"compatibility stub must not recreate off-mesh links: {forbidden}"

print('native Dynamic Recast Stair navigation / no automatic NavLink regression model: PASS')
