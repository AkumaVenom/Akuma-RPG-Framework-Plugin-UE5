from pathlib import Path
import hashlib, re

root=Path(__file__).resolve().parents[1]
src=root/'Source'/'AkumasRPGFramework'
def read(rel): return (src/rel).read_text(errors='ignore')
def require(text,*tokens):
    for t in tokens: assert t in text, f'missing token: {t}'

piece=read('Public/Data/ARPGBuildPieceDefinition.h')
defn=read('Public/Data/ARPGSettlementDefinition.h')
types=read('Public/ARPGTypes.h')
build=read('Private/Building/ARPGBuildingComponent.cpp')
bed_h=read('Public/Settlement/ARPGBuildBedActor.h'); bed_c=read('Private/Settlement/ARPGBuildBedActor.cpp')
hub_h=read('Public/Settlement/ARPGSettlementHubActor.h'); hub_c=read('Private/Settlement/ARPGSettlementHubActor.cpp')
res_h=read('Public/Settlement/ARPGSettlementResidentComponent.h'); res_c=read('Private/Settlement/ARPGSettlementResidentComponent.cpp')
vill_h=read('Public/Settlement/ARPGSettlementVillagerCharacter.h'); vill_c=read('Private/Settlement/ARPGSettlementVillagerCharacter.cpp')
ui_h=read('Public/Components/ARPGSettlementUIComponent.h'); ui_c=read('Private/Components/ARPGSettlementUIComponent.cpp')
widgets_h=read('Public/UI/ARPGSettlementWidgets.h'); widgets_c=read('Private/UI/ARPGSettlementWidgets.cpp')
inter_h=read('Public/Components/ARPGInteractionComponent.h'); inter_c=read('Private/Components/ARPGInteractionComponent.cpp')
bui=read('Private/Components/ARPGBuildingUIComponent.cpp')
char_h=read('Public/Actors/ARPGCharacter.h'); char_c=read('Private/Actors/ARPGCharacter.cpp')
tree_h=read('Public/Gathering/ARPGTree.h'); tree_c=read('Private/Gathering/ARPGTree.cpp')
save_h=read('Public/Save/ARPGSaveGame.h'); save_c=read('Private/Subsystems/ARPGSaveSubsystem.cpp')

# Enum stability: settlement kinds append after the confirmed v2.15.54 Light value.
assert re.search(r'Custom,\s*/\*\* Interactive buildable lighting/decor.*?\*/\s*Light,\s*/\*\* Assignable settlement/player bed.*?\*/\s*Bed,\s*/\*\* Palbox-style settlement control core.*?\*/\s*SettlementHub', piece, re.S)
require(piece,'DefaultBedRole','BedSurfaceOffset','BedInteractionRadius','SettlementDefinition','SettlementHubSurfaceOffset','SettlementHubInteractionRadius')
require(build,'ARPGIsSettlementSurfacePiece','ARPGResolveSettlementPlacementFromHit','ARPGFindSettlementSurfaceFromDesired',
        'case EARPGBuildPieceKind::Bed: return AARPGBuildBedActor::StaticClass();',
        'case EARPGBuildPieceKind::SettlementHub: return AARPGSettlementHubActor::StaticClass();')
# Beds are surface furniture, not ground pieces; hubs are the only optional terrain settlement piece.
require(build,'Piece->PieceKind == EARPGBuildPieceKind::Bed || !Piece->bAllowGroundPlacement','ARPGIsSettlementHorizontalHostKind')

# Explicit opt-in settlement definition and safe boundary policy.
for token in ('SettlementRadius = 5000.f','SettlementHUDRadius = 1800.f','bPreventOverlappingSettlementAreas = true',
              'SettlementSeparationPadding','MinimumFoundationWidth = 2','MinimumFoundationDepth = 2',
              'HomeStoryHeight = 300.f','MaximumVillagers = 20','VillagerClass','VillagerWanderRadius',
              'bEnableVillagerWoodcutting = true','WoodcuttingRadius','VillagerChopPower',
              'MaximumConcurrentWoodcutters','bDepositWoodcuttingRewardsToHub = true','SettlementStockpileSlots = 96'):
    assert token in defn, f'settlement definition missing: {token}'
require(build,'bPreventOverlappingSettlementAreas','ExistingHub->GetSettlementRadius()','SettlementSeparationPadding')

# Bed roles are replicated, persistent, interactable, and deterministically owned by one Hub.
require(types,'enum class EARPGBedRole','Unassigned','Player','Villager','EARPGSettlementHomeState','FARPGSettlementHomeValidation','FARPGSettlementSummary')
require(bed_h,'ReplicatedUsing=OnRep_BedRole','SaveGame','FindManagingSettlementHub','GetCurrentHomeValidation','SetBedRole','AssignResident','RestoreBedState')
require(bed_c,'DOREPLIFETIME(AARPGBuildBedActor, BedRole)','DOREPLIFETIME(AARPGBuildBedActor, AssignedResidentId)','CanActorModify(Requester)',
        'EGuidFormats::Digits','BestDistSq')
require(inter_h,'ServerSetBuiltBedRole')
require(inter_c,'ServerSetBuiltBedRole_Implementation','Bed->SetBedRole(NewRole, GetOwner())')

# Hub contract: completed Hub + SettlementDefinition gates all simulation; home validation is semantic and complete.
require(hub_h,'class AKUMASRPGFRAMEWORK_API AARPGSettlementHubActor : public AARPGStorageActor','ValidateHomeForBed','GetManagedBeds','GetSettlementResidents',
        'CanResidentStartWoodcutting','RefreshSettlementNow','RegisterLoadedResident','SettlementSummary')
for token in ('StartSettlementRuntime','IsConstructionComplete()','GetSettlementDefinition()','Bed->FindManagingSettlementHub() != this',
              'MinimumFoundationWidth','MinimumFoundationDepth','FoundationMap','SettlementCoverKind','SettlementWallLike',
              'SettlementGetDefinitionLocalBounds','SettlementBuildSpatial','SettlementProjectedContains',
              'FoundationCell->WorldMaxZ + Story','UsedPerimeterPieces','MissingScore',
              'EARPGBuildPieceKind::Doorway','DoorOccupiesDoorway','Candidate.bHasDoorway && Candidate.bHasDoor',
              'Re-home existing homeless residents before recruiting anyone new','at most one new resident per recruitment interval'):
    assert token in hub_c, f'Hub/home/recruitment contract missing: {token}'
# Managed beds must resolve to one primary hub even for legacy/overlap edge cases.
require(hub_c,'Bed->FindManagingSettlementHub() == this')

# Villagers are real framework AI characters and inherit settlement ownership/faction.
require(vill_h,'AARPGSettlementVillagerCharacter : public AARPGAICharacter','UARPGSettlementResidentComponent','UARPGFactionOwnershipComponent','InitializeAsSettlementVillager')
for token in ('OwnerAccountId','OwnerCharacterId','OwnerFactionId','PrimaryFactionId','InitializeSettlementResident'):
    assert token in vill_c, f'villager ownership/faction integration missing: {token}'

# Residents use timers + existing AI movement and existing ARPGTree chopping, with reservation + Hub deposit.
require(res_h,'ResidentId','SettlementHub','AssignedBed','ResidentState','CurrentWorkTree','ForceChooseNewActivity','ReturnHome','CanBypassTreeRequirements')
for token in ('SetTimer(ActivityTimer','AIWanderer->SetHomeLocation','AIWanderer->SetWandererEnabled(true)','CurrentWorkTree == Tree',
              'AI->MoveToLocation','ProjectPointToNavigation','StartWorkMovementProof','Tree->ApplyChop(GetOwner(), Power)','DepositTreeRewardsToHub','TransferItemTo(SettlementHub->Inventory'):
    assert token in res_c, f'resident autonomy/woodcutting missing: {token}'
require(tree_h,'bAllowSettlementVillagerHarvest')
require(tree_c,'UARPGSettlementResidentComponent','CanBypassTreeRequirements')

# Native/reskinnable UI: proximity HUD, Hub panel, Bed role panel, resident rows, stockpile handoff.
require(ui_h,'SettlementHUDWidgetClass','SettlementPanelWidgetClass','ResidentRowWidgetClass','BedPanelWidgetClass','bAutoShowNearbySettlementHUD',
        'OpenSettlementPanel','OpenBedPanel','SetBedRole','RequestSettlementRefresh','OpenSettlementStockpile','GetNearbySettlementHub')
for token in ('PollNearbySettlement','FindNearestUsableSettlement','GetSettlementHUDRadius','ShowSettlementHUD','HideSettlementHUD',
              'Character->BuildingUI->OpenStorageUI(Hub)'):
    assert token in ui_c, f'settlement UI runtime missing: {token}'
for token in ('UARPGSettlementHUDWidget','UARPGSettlementPanelWidget','UARPGSettlementResidentRowWidget','UARPGBedPanelWidget',
              'BlueprintImplementableEvent','OpenStockpileButton'):
    assert token in widgets_h, f'settlement widget exposure missing: {token}'
require(widgets_c,'HandleOpenStockpile','PLAYER BED','VILLAGER BED','UNASSIGNED BED')
require(char_h,'UARPGSettlementUIComponent','SettlementUI')
require(char_c,'CreateDefaultSubobject<UARPGSettlementUIComponent>')
# Hub check must precede generic Storage because Hub derives from Storage.
assert bui.find('Cast<AARPGSettlementHubActor>') >= 0 and bui.find('Cast<AARPGStorageActor>') >= 0
assert bui.find('Cast<AARPGSettlementHubActor>') < bui.find('Cast<AARPGStorageActor>'), 'Settlement Hub interaction must precede generic Storage interaction'

# v8 persistence keeps older Window/Light migrations and serializes Beds + residents + Hub stockpile via normal container path.
assert save_h.count('SaveVersion = 5') >= 1 and 'SaveVersion = 8' in save_h
require(types,'FARPGSettlementResidentSave','SettlementHubBuildingId','AssignedBedBuildingId','SettlementResidents','BedAssignedResidentId','PlayerBedOwnerCharacterId')
for token in ('R.BedRole=Bed->BedRole','D.SettlementResidents.Add(R)','Save->SaveVersion>=8','RestoreBedState',
              'Save->SaveVersion>=7 ? R.bLightOn','Save->SaveVersion>=6 ? R.bWindowOpen : false',
              'for(const FARPGSettlementResidentSave& R:Save->World.SettlementResidents)','RegisterLoadedResident'):
    assert token in save_c, f'v8 settlement persistence/migration missing: {token}'

# Structural regression lock: settlement support is additive and may not modify the hard-won v2.15.53 Stair/Wall-family classifier.
def extract_function(text: str, name: str) -> str:
    start=text.find('static bool '+name+'('); assert start>=0, f'protected function missing: {name}'
    brace=text.find('{',start); depth=0
    for i in range(brace,len(text)):
        if text[i]=='{': depth+=1
        elif text[i]=='}':
            depth-=1
            if depth==0: return text[start:i+1]
    raise AssertionError('unterminated '+name)
protected={
 'ARPGIsStairWallFamilyBoundarySeam':'9c373187bbb929c99267d395876e55ebda588b1740abb05e741fb2430cda9b02',
 'ARPGIsCompatibleStairHostStructuralNeighbor':'3b21473d366e1f47685e06eda00a8f9c4849d9c9255dfae1f38141e1ba1e9639',
 'ARPGHostedInsertAllowsStairSideNeighbor':'ddb193165d1dcac4bd352b555744ee61fafcaa284e56f3bed8ac7d7891b3c6f4',
 'ARPGIsCompatibleInsertHostStairNeighbor':'b43be840f382ed45dfa482d5139a883a6b669db19a3a3a023f736b35ad154e14',
}
for name,expected in protected.items():
    actual=hashlib.sha256(extract_function(build,name).encode()).hexdigest()
    assert actual==expected, f'protected Stair/Wall function changed: {name} {actual}'

print('Settlement Hub + validated homes + autonomous villager model: PASS')
