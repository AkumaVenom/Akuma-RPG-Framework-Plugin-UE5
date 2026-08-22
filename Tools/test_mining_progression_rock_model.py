"""First-class Mining progression / mineable-rock regression model — v2.17.0."""
from pathlib import Path
import math

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / 'Source/AkumasRPGFramework'
ROCK_H = (SRC / 'Public/Gathering/ARPGMineableRock.h').read_text(encoding='utf-8', errors='ignore')
ROCK_CPP = (SRC / 'Private/Gathering/ARPGMineableRock.cpp').read_text(encoding='utf-8', errors='ignore')
MINING_H = (SRC / 'Public/Components/ARPGMiningComponent.h').read_text(encoding='utf-8', errors='ignore')
MINING_CPP = (SRC / 'Private/Components/ARPGMiningComponent.cpp').read_text(encoding='utf-8', errors='ignore')
SKILL_H = (SRC / 'Public/Components/ARPGSkillComponent.h').read_text(encoding='utf-8', errors='ignore')
SKILL_CPP = (SRC / 'Private/Components/ARPGSkillComponent.cpp').read_text(encoding='utf-8', errors='ignore')
SKILL_DEF = (SRC / 'Public/Data/ARPGSkillDefinition.h').read_text(encoding='utf-8', errors='ignore')
CHAR_H = (SRC / 'Public/Actors/ARPGCharacter.h').read_text(encoding='utf-8', errors='ignore')
CHAR_CPP = (SRC / 'Private/Actors/ARPGCharacter.cpp').read_text(encoding='utf-8', errors='ignore')
COMBAT_CPP = (SRC / 'Private/Components/ARPGCombatComponent.cpp').read_text(encoding='utf-8', errors='ignore')
BUILD_CPP = (SRC / 'Private/Building/ARPGBuildPieceActor.cpp').read_text(encoding='utf-8', errors='ignore')
PLACEMENT_CPP = (SRC / 'Private/Building/ARPGBuildingComponent.cpp').read_text(encoding='utf-8', errors='ignore')
SAVE_H = (SRC / 'Public/Save/ARPGSaveGame.h').read_text(encoding='utf-8', errors='ignore')
TAGS = (ROOT / 'Config/Tags/ARPGGameplayTags.ini').read_text(encoding='utf-8', errors='ignore')
README = (ROOT / 'README.md').read_text(encoding='utf-8', errors='ignore')
MINING_DOC = (ROOT / 'Docs/MINING.md').read_text(encoding='utf-8', errors='ignore') if (ROOT / 'Docs/MINING.md').exists() else ''

# Native Blueprint-first surfaces and ready character ownership.
for token in (
    'UCLASS(BlueprintType, Blueprintable)',
    'class AKUMASRPGFRAMEWORK_API AARPGMineableRock',
    'Rock Mesh Variations',
    'Randomize Rock Mesh Scale',
    'Randomize Rock Yaw',
    'Successful Strike Drops',
    'Depletion Drops',
    'Bonus Chance Drops',
    'RequiredMiningLevel',
    'MinimumToolTier',
    'MiningResistance',
    'XPPerSuccessfulStrike',
    'XPOnDepletion',
):
    assert token in ROCK_H, token
for token in (
    'class AKUMASRPGFRAMEWORK_API UARPGMiningComponent',
    'StartMiningFromView', 'MineRockOnce', 'StopMining',
    'GetMiningLevel()', 'GetMiningXP()', 'GetMiningLevelProgress()',
    'GetBestEquippedMiningTool', 'GetBestEquippedMiningToolInstanceId',
    'TryHandleBasicAttackAsMining',
):
    assert token in MINING_H, token
assert 'TObjectPtr<UARPGMiningComponent> Mining;' in CHAR_H
assert 'CreateDefaultSubobject<UARPGMiningComponent>(TEXT("Mining"))' in CHAR_CPP

# Resource actors do not permanently Tick. Authority owns replicated variation, health and state.
assert 'PrimaryActorTick.bCanEverTick = false;' in ROCK_CPP
assert 'PrimaryComponentTick.bCanEverTick = false;' in MINING_CPP
for token in (
    'DOREPLIFETIME(AARPGMineableRock, SelectedRockMeshIndex)',
    'DOREPLIFETIME(AARPGMineableRock, SelectedRockMeshScale)',
    'DOREPLIFETIME(AARPGMineableRock, SelectedRockYawDegrees)',
    'DOREPLIFETIME(AARPGMineableRock, CurrentMiningHealth)',
    'DOREPLIFETIME(AARPGMineableRock, RockState)',
    'SelectRandomRockMesh()', 'SelectRandomRockMeshScale()', 'SelectRandomRockYaw()',
):
    assert token in ROCK_CPP, token

# RuneScape-style 1-99 is an opt-in generic skill model and Mining uses it natively without breaking
# pre-existing Skill assets/custom curves.
for token in ('EARPGSkillXPModel', 'RuneScapeStyle99', 'XPRequiredPerLevel', 'FrameworkPower'):
    assert token in SKILL_DEF, token
for token in ('GetRuneScapeStyleTotalXPForLevel', 'GetXPForNextLevelForModel', 'GetXPForNextLevelFromDefinition', 'AddSkillXPWithModel'):
    assert token in SKILL_H and token in SKILL_CPP, token
assert 'Curve && Curve->GetNumKeys() > 0' in SKILL_CPP
assert 'bUseRuneScapeStyleXPWithoutSkillDefinition = true' in MINING_H
assert 'NativeMiningMaxLevel = 99' in MINING_H
assert 'EARPGSkillXPModel::RuneScapeStyle99' in MINING_CPP

def rs_total(level: int) -> int:
    points = sum(int(l + 300 * math.pow(2.0, l / 7.0)) for l in range(1, level))
    return points // 4

assert rs_total(1) == 0
assert rs_total(2) == 83
assert rs_total(3) == 174
assert rs_total(99) == 13_034_431
assert rs_total(2) - rs_total(1) == 83
assert rs_total(3) - rs_total(2) == 91

# Exact equipped runtime pickaxe instance contract; no Data-Asset-only equipment inference.
for token in (
    'Entry.InstanceId.IsValid()', 'Entry.Quantity <= 0', '!Entry.bEquipped', '!Entry.EquipmentSlot.IsValid()',
    'Entry.EquipmentSlot != Definition->EquipmentSlot', 'Entry.Durability <= KINDA_SMALL_NUMBER',
    'GatheringToolTags.HasTag(DesiredTag)', 'GatheringToolTier', 'GatheringPower',
    'DamageItemDurability(ToolInstanceId',
):
    assert token in MINING_CPP, token
assert 'Item.Tool.Pickaxe' in MINING_CPP and 'Item.Tool.Pickaxe' in ROCK_CPP

# Palworld/Dragonwilds-style repeated strike health plus normal per-hit/final payload and independent rare finds.
for token in (
    'MiningPower / FMath::Max(0.05f, MiningResistance)',
    'GrantNormalDropArray(Harvester, StrikeDrops, EARPGMiningBonusDropTrigger::SuccessfulStrike)',
    'GrantNormalDropArray(Harvester, DepletionDrops, EARPGMiningBonusDropTrigger::Depletion)',
    'GrantBonusDrops(Harvester, EARPGMiningBonusDropTrigger::SuccessfulStrike)',
    'GrantBonusDrops(Harvester, EARPGMiningBonusDropTrigger::Depletion)',
    'BonusChancePerMiningLevel', 'BonusChancePerToolTier', 'MaximumEffectiveBonusChance',
    'Drop.RequiredMiningLevel', 'Drop.MinimumToolTier',
):
    assert token in ROCK_CPP or token in ROCK_H, token

# Basic Attack mining is context-sensitive and returns before ordinary combat resource spending.
wood_pos = COMBAT_CPP.index('TryHandleBasicAttackAsWoodcutting')
mining_pos = COMBAT_CPP.index('TryHandleBasicAttackAsMining')
stamina_pos = COMBAT_CPP.index('Stats->SpendStamina')
assert wood_pos < mining_pos < stamina_pos
assert 'bHandledAsMining' in COMBAT_CPP
assert 'return bMiningStarted;' in COMBAT_CPP

# Interact-world convenience preserves dedicated legacy wrappers and routes Mining -> Tree -> built structure.
interact_start = CHAR_CPP.index('bool AARPGCharacter::InteractWorld()')
interact = CHAR_CPP[interact_start:interact_start + 1800]
assert interact.index('FindMineableRockInView') < interact.index('FindWoodcuttingTreeInView') < interact.index('InteractWithBuiltStructureFromView')
assert 'InteractBuiltStructure()' in CHAR_CPP

# Player-built Foundations may replace only framework-managed renewable resources; both Trees and Mineable Rocks
# receive occupancy suppression while ordinary world props remain normal blockers.
assert '#include "Gathering/ARPGMineableRock.h"' in PLACEMENT_CPP
assert 'Params.AddIgnoredActor(Rock);' in PLACEMENT_CPP
assert '(Other->IsA<AARPGTree>() || Other->IsA<AARPGMineableRock>())' in PLACEMENT_CPP
assert 'Rock->NotifyBuildPieceOccupancyChanged(this, bPresent);' in BUILD_CPP
suppress_start = ROCK_CPP.index('void AARPGMineableRock::UpdateBuildingSuppressionStateAuthority')
suppress_end = ROCK_CPP.index('void AARPGMineableRock::SelectRandomRockMesh', suppress_start)
suppress = ROCK_CPP[suppress_start:suppress_end]
for forbidden in ('GrantOneReward(', 'GrantNormalDropArray(', 'GrantBonusDrops(', 'AwardMiningXP(', 'OnRockDepleted.Broadcast'):
    assert forbidden not in suppress, forbidden

# Existing generic Skill persistence carries Mining without a character schema migration; renewable rock depletion
# deliberately remains runtime/resource-spawn state like ARPGTree, while world build persistence remains v9.
assert 'SaveVersion = 10' in SAVE_H
assert 'Skills' in SKILL_H and 'SaveGame' in SKILL_H

# Standard tag conventions and documentation must ship with the feature.
for tag in ('Skill.Mining', 'Gathering.MineableRock', 'Item.Tool.Pickaxe', 'Item.Resource.Stone', 'Item.Resource.Ore', 'Item.Resource.Gem'):
    assert tag in TAGS, tag
assert 'v2.17.0-alpha' in README
for phrase in ('AARPGMineableRock', 'RuneScape-Style 1-99', 'Basic Attack', 'InteractWorld', 'Actor Foliage', 'Bonus Chance Drops'):
    assert phrase in MINING_DOC, phrase

print('mining progression / mineable-rock regression model: PASS')
