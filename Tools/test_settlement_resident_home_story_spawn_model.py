from pathlib import Path

root = Path(__file__).resolve().parents[1]
src = root / 'Source' / 'AkumasRPGFramework'

def read(rel):
    return (src / rel).read_text(errors='ignore')

def require(text, *tokens):
    for token in tokens:
        assert token in text, f'missing token: {token}'

hub_h = read('Public/Settlement/ARPGSettlementHubActor.h')
hub = read('Private/Settlement/ARPGSettlementHubActor.cpp')
resident = read('Private/Settlement/ARPGSettlementResidentComponent.cpp')
save = read('Private/Subsystems/ARPGSaveSubsystem.cpp')

# PIE regression: a resident could be projected/spawn-adjusted onto the ceiling/roof because the
# original Bed query allowed a full-story vertical NavMesh search and AlwaysSpawn collision recovery.
require(hub_h, 'ResolveResidentHomeAnchor(AARPGBuildBedActor* Bed, FVector& OutWorldLocation) const;')
require(hub,
        'const float WalkablePlaneZ = Validation.HomeCenter.Z - Validation.HomeExtent.Z;',
        'ProjectPointToNavigation(Candidate, Projected, FVector(QueryXY, QueryXY, VerticalTolerance))',
        'FMath::Abs(Projected.Location.Z - WalkablePlaneZ) > VerticalTolerance',
        'FVector::DotProduct(Delta, XAxis)',
        'FVector::DotProduct(Delta, YAxis)',
        'AdjustIfPossibleButDontSpawnIfColliding',
        'MaxResidentSpawnAttempts = 10',
        'SpawnStoryTolerance',
        'FMath::Abs(Spawned->GetActorLocation().Z - HomeAnchor.Z) > SpawnStoryTolerance',
        'deferred instead of placing a villager on the roof')

recruit_start = hub.index('bool AARPGSettlementHubActor::RecruitResidentForBed')
recruit_end = hub.index('bool AARPGSettlementHubActor::RegisterLoadedResident', recruit_start)
recruit = hub[recruit_start:recruit_end]
assert 'AdjustIfPossibleButAlwaysSpawn' not in recruit, 'resident recruitment must never force a blocked pawn onto another story'
assert recruit.index('ResolveResidentHomeAnchor(Bed, HomeAnchor)') < recruit.index('SpawnActor<AARPGSettlementVillagerCharacter>'), \
    'same-story interior anchor must be resolved before resident spawn'

# Roaming/return-home uses the same semantic interior anchor rather than the Bed actor pivot, so
# GetRandomReachablePointInRadius starts from the correct connected NavMesh island.
require(resident,
        'SettlementHub->ResolveResidentHomeAnchor(AssignedBed, InteriorAnchor)',
        'AI->AIWanderer->SetHomeLocation(ResolveHomeLocation())',
        'AI->AICombat->HomeLocation = ResolveHomeLocation();',
        'RepairInvalidHomeStoryPosition()',
        'bOnUpperStorySurface',
        'returned to the validated interior home story')

# World-load restoration must avoid AlwaysSpawn and has a Bed/home-anchor fallback. Old saves whose
# resident was already stored on the roof are repaired by RestoreResidentLinks.
settlement_load = save[save.index('// Residents are restored only after every building/Bed/Hub') : save.index('for(const FARPGDungeonSaveState& R:Save->World.Dungeons)')]
assert 'AdjustIfPossibleButAlwaysSpawn' not in settlement_load
require(settlement_load,
        'AdjustIfPossibleButDontSpawnIfColliding',
        'Hub->ResolveResidentHomeAnchor(Bed,InteriorAnchor)',
        'SafeTransform.SetLocation(InteriorAnchor)')

# Tiny model of the vertical safety contract: a roof 300 cm above a 0 cm walkable plane must be
# rejected by an 80 cm maximum story tolerance, while a normal nav surface offset is accepted.
def same_story(projected_z, floor_z, tolerance):
    return abs(projected_z - floor_z) <= tolerance
assert same_story(3.0, 0.0, 80.0)
assert not same_story(300.0, 0.0, 80.0)

print('Settlement resident interior-story spawn/home-anchor regression model: PASS')
