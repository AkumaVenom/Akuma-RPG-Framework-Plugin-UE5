from pathlib import Path

root = Path(__file__).resolve().parents[1]
src = root / 'Source' / 'AkumasRPGFramework'

def read(rel):
    return (src / rel).read_text(errors='ignore')

def require(text, *tokens):
    for token in tokens:
        assert token in text, f'missing token: {token}'

hub = read('Private/Settlement/ARPGSettlementHubActor.cpp')
resident = read('Private/Settlement/ARPGSettlementResidentComponent.cpp')
resident_h = read('Public/Settlement/ARPGSettlementResidentComponent.h')

# Regression from PIE: one recruited pawn was displayed/counting twice because initialization changed
# resident state -> NotifyResidentStateChanged -> RegisterLoadedResident, then RecruitResidentForBed
# appended the same pawn again. Recruitment must pre-register before Initialize and never append after it.
recruit_start = hub.index('bool AARPGSettlementHubActor::RecruitResidentForBed')
recruit_end = hub.index('bool AARPGSettlementHubActor::RegisterLoadedResident', recruit_start)
recruit = hub[recruit_start:recruit_end]
pre_add = recruit.index('Residents.Add(Resident);')
initialize = recruit.index('InitializeAsSettlementVillager')
assert pre_add < initialize, 'resident must be pre-registered before initialization callbacks can fire'
assert recruit.count('Residents.Add(Resident);') == 1, 'recruitment must append the pawn exactly once'
require(recruit, 'Residents.RemoveAll([Resident]', 'CleanupResidentRegistry();', 'OnSettlementResidentChanged.Broadcast(Resident, true)')

# Registry identity is protected both by live actor and persistent ResidentId, and all public/UI views
# are unique even if a legacy runtime array somehow contains duplicates.
require(hub,
        'TSet<const AARPGSettlementVillagerCharacter*> SeenActors',
        'TSet<FGuid> SeenResidentIds',
        'bDuplicateActor', 'bDuplicateId',
        'IncomingId.IsValid()',
        'ExistingResident->SettlementResident->ResidentId == IncomingId',
        'TArray<AARPGSettlementVillagerCharacter*> UniqueResidents',
        'NewSummary.ResidentCount = UniqueResidents.Num()')

# Minimal identity model: callback registration followed by recruitment completion must remain one entry.
registry = []
def register(actor, rid):
    if any(a == actor for a, _ in registry): return True
    if rid and any(existing_rid == rid for _, existing_rid in registry): return False
    registry.append((actor, rid)); return True
actor = object(); rid = 'resident-guid'
registry.append((actor, rid))       # pre-register before Initialize
assert register(actor, rid)         # state callback is idempotent
assert len(registry) == 1

# Work movement must be controller/nav-aware and must not claim GoingToWork before a navigation request
# succeeds. Tree origins can be carved out of NavMesh, so approach uses projected navigation geometry.
require(resident,
        'SpawnDefaultController()',
        'FNavigationSystem::GetCurrent<UNavigationSystemV1>',
        'ProjectPointToNavigation(Actor->GetActorLocation()',
        'Result != EPathFollowingRequestResult::Failed',
        'StartWorkMovementProof()',
        'VerifyWorkMovement()',
        'EPathFollowingStatus::Moving',
        'SettlementWorkMovementProofMaxChecks',
        'abandoned an unreachable/stalled woodcutting route',
        'StopWoodcutting(true)')
require(resident_h, 'WorkMoveProofTimer', 'WorkMoveProofChecks', 'WorkMoveProofStartLocation')

try_start = resident.index('bool UARPGSettlementResidentComponent::TryBeginWoodcutting')
try_end = resident.index('AARPGTree* UARPGSettlementResidentComponent::FindBestWorkTree', try_start)
try_body = resident[try_start:try_end]
assert try_body.index('if (!MoveToActor(Tree, Acceptance))') < try_body.index('SetResidentState(EARPGSettlementResidentState::GoingToWork)'), \
    'GoingToWork must only be published after navigation accepts the work route'

print('Settlement resident registry + autonomous locomotion regression model: PASS')
