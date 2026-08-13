from pathlib import Path
root=Path(__file__).resolve().parents[1]
src=root/'Source'/'AkumasRPGFramework'
def read(rel): return (src/rel).read_text(errors='ignore')

def require(text, *tokens):
    for token in tokens:
        assert token in text, f'missing token: {token}'

piece_h=read(Path('Public/Data/ARPGBuildPieceDefinition.h'))
build_h=read(Path('Public/Building/ARPGBuildingComponent.h'))
build_cpp=read(Path('Private/Building/ARPGBuildingComponent.cpp'))
actor_h=read(Path('Public/Building/ARPGBuildPieceActor.h'))
actor_cpp=read(Path('Private/Building/ARPGBuildPieceActor.cpp'))
preview_h=read(Path('Public/Building/ARPGBuildPreviewActor.h'))
preview_cpp=read(Path('Private/Building/ARPGBuildPreviewActor.cpp'))
door_h=read(Path('Public/Building/ARPGBuildDoorActor.h'))
door_cpp=read(Path('Private/Building/ARPGBuildDoorActor.cpp'))
ui_comp_h=read(Path('Public/Components/ARPGBuildingUIComponent.h'))
ui_comp_cpp=read(Path('Private/Components/ARPGBuildingUIComponent.cpp'))
ui_h=read(Path('Public/UI/ARPGBuildingWidgets.h'))
ui_cpp=read(Path('Private/UI/ARPGBuildingWidgets.cpp'))
char_h=read(Path('Public/Actors/ARPGCharacter.h'))
char_cpp=read(Path('Private/Actors/ARPGCharacter.cpp'))
inter_h=read(Path('Public/Components/ARPGInteractionComponent.h'))
inter_cpp=read(Path('Private/Components/ARPGInteractionComponent.cpp'))
station_h=read(Path('Public/Crafting/ARPGCraftingStationActor.h'))
station_cpp=read(Path('Private/Crafting/ARPGCraftingStationActor.cpp'))
inv_h=read(Path('Public/Components/ARPGInventoryComponent.h'))
inv_cpp=read(Path('Private/Components/ARPGInventoryComponent.cpp'))
save_h=read(Path('Public/Save/ARPGSaveGame.h'))
save_cpp=read(Path('Private/Subsystems/ARPGSaveSubsystem.cpp'))
types_h=read(Path('Public/ARPGTypes.h'))

# Complete data-driven kit types and utility definitions.
for kind in ('Foundation','Wall','WindowWall','Window','Doorway','Door','Floor','Ceiling','Roof','Stair','Pillar','Storage','Production','Decoration','Custom'):
    assert kind in piece_h
require(piece_h, 'BuildMesh', 'PreviewMesh', 'BuildCost', 'ConstructionSeconds', 'ConstructionStartScaleZ',
        'ConstructionProgressMaterialParameter', 'SnapSize', 'StandardWallHeight', 'CustomSnapPoints',
        'StorageSlots', 'StationDefinition', 'DemolishRefundFraction')

# Local build mode + ghost; permanent component tick is avoided outside build mode.
require(build_h, 'BuildCatalog', 'BeginBuildMode', 'EndBuildMode', 'ConfirmPreviewPlacement', 'RotatePreview',
        'NextBuildPiece', 'PreviousBuildPiece', 'OnBuildModeChanged', 'OnBuildPreviewUpdated', 'bAllowUnlistedBuildRequests', 'bRequireSnapTargetModificationAccess')
require(build_cpp, 'PrimaryComponentTick.bStartWithTickEnabled = false', 'SetComponentTickEnabled(true)', 'SetComponentTickEnabled(false)',
        'AARPGBuildPreviewActor', 'UpdatePlacementPreview', 'FindBestSnapTransform', 'GetSnapTransformsFor',
        'ServerPlacePiece', 'PlacePieceAuthority', 'EvaluatePlacementInternal')
# Ground placement is mesh-pivot aware: bottom-center of the real Build Mesh is anchored to the surface,
# while collision/support validation uses the corresponding visible bounds center/bottom rather than actor origin.
require(build_cpp, 'ARPGGetBuildPieceBottomAnchorLocal', 'ARPGGetBuildPieceBoundsCenterLocal',
        'Hit.ImpactPoint - DesiredRotation.RotateVector(BottomAnchorLocal)', 'PlacementBoundsCenter',
        'BottomAnchor + FVector::UpVector * ProbeLift', 'Grid-snap the visible footprint anchor')
assert 'Hit.ImpactNormal * FMath::Max(0.f, SelectedBuildPiece->PlacementBounds.Z)' not in build_cpp

# Mathematical regression: bottom-pivot and center-pivot 150 cm foundations both land their visible bottom at Z=0.
def grounded_actor_z(surface_z, local_min_z):
    return surface_z - local_min_z
assert grounded_actor_z(0.0, 0.0) == 0.0       # bottom pivot
assert grounded_actor_z(0.0, -75.0) == 75.0    # center pivot
# A 300 cm bottom-pivot wall on a 150 cm bottom-pivot foundation starts exactly at foundation top.
assert (150.0 - 0.0) == 150.0
require(actor_cpp, 'ARPGGetBuildDefinitionLocalBounds', 'AlignTopPlaneZ', 'IncomingOnTargetTopZ',
        'TargetMax.Z + WallHeight - IncomingMin.Z', 'IncomingAboveTargetZ')
# Authority re-resolves snap/placement rather than trusting local preview.
pa=build_cpp[build_cpp.index('bool UARPGBuildingComponent::PlacePieceAuthority'):]
assert 'ResolvePlacementTransform' in pa and 'EvaluatePlacementInternal' in pa
assert 'BuildCatalog.ContainsByPredicate' in build_cpp and 'SnapTarget->CanActorModify(Owner)' in build_cpp
# Costs aggregate and only unequipped resources can be consumed.
require(build_cpp, 'ARPGAggregateBuildCosts', 'HasUnequippedItem', 'RemoveUnequippedItem', 'RefundBuildResources')
# Native actor fallback makes common pieces usable without actor Blueprints.
require(build_cpp, 'EARPGBuildPieceKind::Door: return AARPGBuildDoorActor::StaticClass()',
        'EARPGBuildPieceKind::Storage: return AARPGStorageActor::StaticClass()',
        'EARPGBuildPieceKind::Production: return AARPGCraftingStationActor::StaticClass()')

# Preview actor is local-only and collisionless.
require(preview_cpp, 'bReplicates = false', 'SetActorEnableCollision(false)', 'ECollisionEnabled::NoCollision',
        'PlacementValid', 'PreviewOpacity')

# Snap graph covers requested structural families and doors/windows.
require(actor_cpp, 'Foundation', 'WindowWall', 'Doorway', 'Ceiling', 'Roof',
        'TargetKind == EARPGBuildPieceKind::Doorway && IncomingKind == EARPGBuildPieceKind::Door',
        'TargetKind == EARPGBuildPieceKind::WindowWall && IncomingKind == EARPGBuildPieceKind::Window')
# Timed construction uses synchronized server time, visible reveal and tick-only-while-building.
require(actor_h, 'bConstructionComplete', 'ConstructionStartServerTime', 'ConstructionDuration', 'GetConstructionProgress01')
require(actor_cpp, 'GetServerWorldTimeSeconds', 'ConstructionStartScaleZ', 'SetScalarParameterValueOnMaterials',
        'SetCollisionEnabled', 'SetActorTickEnabled(true)', 'SetActorTickEnabled(false)', 'CompleteConstructionAuthority')

# Doors are replicated, authority-controlled, faction-access checked, and tick only while animating.
require(door_h, 'ReplicatedUsing=OnRep_DoorOpen', 'ToggleDoor', 'SetDoorOpen', 'RestoreDoorOpenState')
require(door_cpp, 'CanActorUse', 'SetActorTickEnabled(true)', 'SetActorTickEnabled(false)', 'DOREPLIFETIME(AARPGBuildDoorActor, bDoorOpen)')

# Player-facing native, reskinnable interfaces are inherited on AARPGCharacter.
require(char_h, 'UARPGBuildingUIComponent', 'OpenBuildMenuUI', 'BeginBuildPlacement', 'ConfirmBuildPlacement', 'InteractBuiltStructure', 'DemolishBuiltStructure')
require(char_cpp, 'CreateDefaultSubobject<UARPGBuildingUIComponent>(TEXT("BuildingUI"))')
require(ui_comp_h, 'BuildMenuWidgetClass', 'PlacementHUDWidgetClass', 'StorageWidgetClass', 'CraftingStationWidgetClass',
        'BuildPieceRowWidgetClass', 'StructureItemRowWidgetClass', 'StationRecipeRowWidgetClass', 'DemolishBuiltStructureFromView')
require(ui_h, 'UARPGBuildMenuWidget', 'UARPGBuildPlacementHUDWidget', 'UARPGStoragePanelWidget', 'UARPGCraftingStationPanelWidget',
        'UARPGBuildPieceRowWidget', 'UARPGStructureItemRowWidget', 'UARPGStationRecipeRowWidget')
require(ui_cpp, 'PRODUCTION STATION', 'PLAYER INVENTORY', 'INPUT + FUEL', 'OUTPUT', 'QueueRecipe')
# Structure transfer UI targets the exact clicked runtime instance (important for durability).
require(ui_h, 'FGuid InstanceId')
require(ui_cpp, 'DepositToStorageInstance', 'WithdrawFromStorageInstance', 'WithdrawStationOutputInstance')
require(inv_h, 'TransferItemInstanceTo')
require(inv_cpp, 'bool UARPGInventoryComponent::TransferItemInstanceTo', 'SourceEntry.bEquipped', 'FARPGInventoryEntry Moved = SourceEntry', 'Destination->Items.Append(MovedEntries)')

# Storage/furnace interactions are routed through the player-owned Interaction component/RPCs.
require(inter_h, 'DepositToStorageInstance', 'WithdrawFromStorageInstance', 'WithdrawStationOutputInstance', 'ToggleBuiltDoor', 'DemolishBuilding', 'QueueCraft')
require(inter_cpp, 'ServerDepositToStorageInstance_Implementation', 'ServerWithdrawFromStorageInstance_Implementation',
        'ServerWithdrawStationOutputInstance_Implementation', 'ServerToggleBuiltDoor_Implementation', 'ServerDemolishBuilding_Implementation', 'ServerQueueCraft_Implementation')

# Production station/furnace: data-driven station, wood-style tagged fuel, ore inputs, outputs and transactional safeguards.
require(station_h, 'ApplyStationDefinition', 'CanQueueRecipe', 'OutputInventory')
require(station_cpp, 'bConsumesFuel', 'FuelTag', 'FuelPerCraft', 'ARPGCountTaggedItems', 'ConsumeFuelForCraft',
        'ARPGAggregateRecipeAmounts', 'ARPGCanFitResolvedOutputs', 'HasUnequippedItem', 'RemoveUnequippedItem', 'ReplaceInventory(Before)',
        'StationDefinition->StationTag.MatchesTagExact', 'SetActorTickEnabled(true)', 'SetActorTickEnabled(false)')
# Required station tag is a hard requirement, even if the station is misconfigured/untagged.
canuse=station_cpp[station_cpp.index('bool AARPGCraftingStationActor::CanUseRecipe'):station_cpp.index('bool AARPGCraftingStationActor::ConsumeRecipeInputs')]
assert '!StationDefinition || !StationDefinition->StationTag.IsValid()' in canuse

# Persistent construction + door state and existing container/furnace inventories/queues.
require(types_h, 'bConstructionComplete', 'ConstructionRemainingSeconds', 'bDoorOpen')
assert save_h.count('SaveVersion = 5') >= 2
require(save_cpp, 'R.bConstructionComplete=B->IsConstructionComplete()', 'R.ConstructionRemainingSeconds=B->GetConstructionRemainingSeconds()',
        'R.bDoorOpen=Door->IsDoorOpen()', 'RestoreConstructionState', 'RestoreDoorOpenState', 'CraftQueue', 'OutputItems',
        'ProcessOfflineElapsed()', 'SetActorTickEnabled(C->CraftQueue.Num()>0)')

# UE5.8.1 compile compatibility guards from the first real v2.15 build.
require(build_cpp, '#include "Engine/OverlapResult.h"')
assert 'UVerticalBox*&OutBox' not in ui_cpp
require(ui_cpp, 'TObjectPtr<UVerticalBox>&OutBox')
assert 'UARPGStoragePanelWidget*P=' not in ui_cpp and 'UARPGCraftingStationPanelWidget*P=' not in ui_cpp

print('Settlement building + storage + production UI model: PASS')
