"""ARPG Tree/build occupancy suppression regression — v2.16.9."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
TREE_H = (ROOT / 'Source/AkumasRPGFramework/Public/Gathering/ARPGTree.h').read_text(encoding='utf-8', errors='ignore')
TREE_CPP = (ROOT / 'Source/AkumasRPGFramework/Private/Gathering/ARPGTree.cpp').read_text(encoding='utf-8', errors='ignore')
BUILD_H = (ROOT / 'Source/AkumasRPGFramework/Public/Building/ARPGBuildPieceActor.h').read_text(encoding='utf-8', errors='ignore')
BUILD_CPP = (ROOT / 'Source/AkumasRPGFramework/Private/Building/ARPGBuildPieceActor.cpp').read_text(encoding='utf-8', errors='ignore')
PLACEMENT_CPP = (ROOT / 'Source/AkumasRPGFramework/Private/Building/ARPGBuildingComponent.cpp').read_text(encoding='utf-8', errors='ignore')
RESIDENT_CPP = (ROOT / 'Source/AkumasRPGFramework/Private/Settlement/ARPGSettlementResidentComponent.cpp').read_text(encoding='utf-8', errors='ignore')
SAVE_H = (ROOT / 'Source/AkumasRPGFramework/Public/Save/ARPGSaveGame.h').read_text(encoding='utf-8', errors='ignore')

# Foundation placement may pierce framework-managed renewable Trees and Mineable Rocks without weakening normal world blockers.
for token in (
    '#include "Gathering/ARPGTree.h"',
    'ARPGTracePlacementSurfaceIgnoringFoundationTrees',
    'Piece->PieceKind != EARPGBuildPieceKind::Foundation',
    'Params.AddIgnoredActor(Tree);',
    '#include "Gathering/ARPGMineableRock.h"',
    '(Other->IsA<AARPGTree>() || Other->IsA<AARPGMineableRock>())',
    'Params.AddIgnoredActor(Rock);',
):
    assert token in PLACEMENT_CPP, token

# Client preview and authority support validation must share the tree-piercing trace contract.
assert PLACEMENT_CPP.count('ARPGTracePlacementSurfaceIgnoringFoundationTrees(') >= 3

# Suppression is a first-class, replicated, Blueprint-visible tree state derived from build occupancy.
for token in (
    'bSuppressRespawnWhileBuiltOver = true',
    'BuildingRespawnBlockRadius = 85.f',
    'BuildingRespawnRecheckSeconds = 1.f',
    'ReplicatedUsing=OnRep_BuildingRespawnSuppressed',
    'IsRespawnSuppressedByBuilding()',
    'RefreshBuildingRespawnSuppression()',
    'IsRespawnBlockedByBuildPiece',
    'OnTreeBuildingSuppressionChanged',
):
    assert token in TREE_H, token

for token in (
    'Building->DoesLogicalPlacementOverlapWorldCylinder',
    'BuildingRespawnBlockers.Add(Building)',
    'BuildingRespawnBlockers.Remove(Building)',
    'CacheStandingRespawnBounds()',
    'ScheduleSuppressionRecheck()',
    'TryRespawnAuthority()',
    'CompleteRespawnAuthority()',
    'DOREPLIFETIME(AARPGTree, bBuildingRespawnSuppressed)',
):
    assert token in TREE_CPP, token

# Suppression must hide both tree and stump and disable their collision so construction never leaves
# a resource trunk/stump embedded inside the new building.
visual_start = TREE_CPP.index('void AARPGTree::ApplyTreeStateVisuals')
visual_end = TREE_CPP.index('void AARPGTree::StartFallVisualLocal', visual_start)
visual = TREE_CPP[visual_start:visual_end]
assert 'if (bBuildingRespawnSuppressed)' in visual
assert visual.count('SetCollisionEnabled(ECollisionEnabled::NoCollision)') >= 2
assert visual.count('SetHiddenInGame(true, true)') >= 2

# Environment replacement is not harvesting. The suppression state transition must not grant XP,
# drops or fell feedback, and settlement workers must not treat suppression as a rewarded fell.
update_start = TREE_CPP.index('void AARPGTree::UpdateBuildingSuppressionStateAuthority')
update_end = TREE_CPP.index('void AARPGTree::SelectRandomTreeMesh', update_start)
update = TREE_CPP[update_start:update_end]
for forbidden in ('GrantRewards(', 'AwardWoodcuttingXP(', 'MulticastPlayFellFeedback(', 'OnTreeFelled.Broadcast'):
    assert forbidden not in update, forbidden
assert '!CurrentWorkTree->IsRespawnSuppressedByBuilding()' in RESIDENT_CPP

# Build lifecycle notifications are immediate for placement/load and removal; a suppressed tree also
# self-heals periodically, so custom movement/destruction cannot leave stale blockers indefinitely.
for token in (
    'RefreshNearbyTreeRespawnSuppression();',
    'NotifyNearbyTreesOfOccupancy(false);',
    'Tree->NotifyBuildPieceOccupancyChanged(this, bPresent);',
    'DoesLogicalPlacementOverlapWorldCylinder',
):
    assert token in BUILD_CPP or token in BUILD_H, token
assert 'SetTimer(BuildingSuppressionRecheckTimer' in TREE_CPP

# Respawn is condition + time based: the normal eligibility time is retained while occupied and the
# actual standing tree is restored only through the guarded Try/Complete path.
force_start = TREE_CPP.index('void AARPGTree::ForceRespawn()')
force_end = TREE_CPP.index('void AARPGTree::SelectRandomTreeMesh()', force_start)
respawn = TREE_CPP[force_start:force_end]
assert 'TryRespawnAuthority();' in respawn
assert 'if (bBuildingRespawnSuppressed)' in respawn
assert 'RespawnEligibleServerTime' in respawn
assert 'TreeState = EARPGTreeState::Standing;' in respawn

# Suppression is derived from buildings and therefore needs no world-save schema bump.
assert 'SaveVersion = 9' in SAVE_H

print('tree/build occupancy suppression regression model: PASS')
