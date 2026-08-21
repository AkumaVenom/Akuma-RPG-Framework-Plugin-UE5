#include "Actors/ARPGCharacter.h"
#include "AbilitySystemComponent.h"
#include "ARPGDeveloperSettings.h"
#include "Components/ARPGStatsComponent.h"
#include "Components/ARPGCombatComponent.h"
#include "Components/ARPGProgressionComponent.h"
#include "Components/ARPGInventoryComponent.h"
#include "Components/ARPGItemUseComponent.h"
#include "Components/ARPGCraftingComponent.h"
#include "Items/ARPGItemUseBehavior.h"
#include "Components/ARPGEquipmentComponent.h"
#include "Components/ARPGQuickAccessComponent.h"
#include "Components/ARPGCurrencyComponent.h"
#include "Components/ARPGQuestComponent.h"
#include "Components/ARPGSkillComponent.h"
#include "Components/ARPGSlayerComponent.h"
#include "Components/ARPGFactionComponent.h"
#include "Components/ARPGBattlePetComponent.h"
#include "Components/ARPGBattlePetBattleComponent.h"
#include "Components/ARPGClassComponent.h"
#include "Components/ARPGAbilityBridgeComponent.h"
#include "Components/ARPGEventRouterComponent.h"
#include "Components/ARPGPersistenceComponent.h"
#include "Components/ARPGInteractionComponent.h"
#include "Building/ARPGBuildingComponent.h"
#include "Components/ARPGBuildingUIComponent.h"
#include "Components/ARPGSettlementUIComponent.h"
#include "Data/ARPGBuildPieceDefinition.h"
#include "Mounts/ARPGMountComponent.h"
#include "Social/ARPGGroupComponent.h"
#include "Components/ARPGThreatComponent.h"
#include "Components/ARPGAICombatComponent.h"
#include "Components/ARPGTargetingComponent.h"
#include "Components/ARPGWoodcuttingComponent.h"
#include "Components/ARPGFootstepComponent.h"
#include "Components/ARPGCharacterInfoComponent.h"
#include "Components/ARPGStatsUIComponent.h"
#include "Components/ARPGInventoryUIComponent.h"
#include "Gathering/ARPGTree.h"
#include "Net/UnrealNetwork.h"

AARPGCharacter::AARPGCharacter()
{
    bReplicates = true;
    AbilitySystem = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystem"));
    AbilitySystem->SetIsReplicated(true);
    AbilitySystem->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);
    Stats = CreateDefaultSubobject<UARPGStatsComponent>(TEXT("Stats"));
    Combat = CreateDefaultSubobject<UARPGCombatComponent>(TEXT("Combat"));
    Progression = CreateDefaultSubobject<UARPGProgressionComponent>(TEXT("Progression"));
    Inventory = CreateDefaultSubobject<UARPGInventoryComponent>(TEXT("Inventory"));
    ItemUse = CreateDefaultSubobject<UARPGItemUseComponent>(TEXT("ItemUse"));
    Crafting = CreateDefaultSubobject<UARPGCraftingComponent>(TEXT("Crafting"));
    Equipment = CreateDefaultSubobject<UARPGEquipmentComponent>(TEXT("Equipment"));
    QuickAccess = CreateDefaultSubobject<UARPGQuickAccessComponent>(TEXT("QuickAccess"));
    Currencies = CreateDefaultSubobject<UARPGCurrencyComponent>(TEXT("Currencies"));
    Quests = CreateDefaultSubobject<UARPGQuestComponent>(TEXT("Quests"));
    Skills = CreateDefaultSubobject<UARPGSkillComponent>(TEXT("Skills"));
    Slayer = CreateDefaultSubobject<UARPGSlayerComponent>(TEXT("Slayer"));
    Faction = CreateDefaultSubobject<UARPGFactionComponent>(TEXT("Faction"));
    BattlePets = CreateDefaultSubobject<UARPGBattlePetComponent>(TEXT("BattlePets"));
    BattlePetBattle = CreateDefaultSubobject<UARPGBattlePetBattleComponent>(TEXT("BattlePetBattle"));
    ClassComponent = CreateDefaultSubobject<UARPGClassComponent>(TEXT("Class"));
    AbilityBridge = CreateDefaultSubobject<UARPGAbilityBridgeComponent>(TEXT("AbilityBridge"));
    EventRouter = CreateDefaultSubobject<UARPGEventRouterComponent>(TEXT("EventRouter"));
    Persistence = CreateDefaultSubobject<UARPGPersistenceComponent>(TEXT("Persistence"));
    Interaction = CreateDefaultSubobject<UARPGInteractionComponent>(TEXT("Interaction"));
    Building = CreateDefaultSubobject<UARPGBuildingComponent>(TEXT("Building"));
    BuildingUI = CreateDefaultSubobject<UARPGBuildingUIComponent>(TEXT("BuildingUI"));
    SettlementUI = CreateDefaultSubobject<UARPGSettlementUIComponent>(TEXT("SettlementUI"));
    Mounts = CreateDefaultSubobject<UARPGMountComponent>(TEXT("Mounts"));
    Group = CreateDefaultSubobject<UARPGGroupComponent>(TEXT("Group"));
    Threat = CreateDefaultSubobject<UARPGThreatComponent>(TEXT("Threat"));
    AICombat = CreateDefaultSubobject<UARPGAICombatComponent>(TEXT("AICombat"));
    Targeting = CreateDefaultSubobject<UARPGTargetingComponent>(TEXT("Targeting"));
    Woodcutting = CreateDefaultSubobject<UARPGWoodcuttingComponent>(TEXT("Woodcutting"));
    Footsteps = CreateDefaultSubobject<UARPGFootstepComponent>(TEXT("Footsteps"));
    CharacterInfo = CreateDefaultSubobject<UARPGCharacterInfoComponent>(TEXT("CharacterInfo"));
    CharacterInfo->SetupAttachment(GetRootComponent());
    StatsUI = CreateDefaultSubobject<UARPGStatsUIComponent>(TEXT("StatsUI"));
    InventoryUI = CreateDefaultSubobject<UARPGInventoryUIComponent>(TEXT("InventoryUI"));
}

void AARPGCharacter::BeginPlay()
{
    Super::BeginPlay();
    if (HasAuthority())
    {
        EnsureCharacterId();
        const UARPGDeveloperSettings* Settings = GetDefault<UARPGDeveloperSettings>();
        if (IsPlayerControlled() && Faction && Faction->GetPrimaryFactionId().IsNone() && Settings && !Settings->DefaultPlayerFactionId.IsNone())
            Faction->SetPrimaryFactionId(Settings->DefaultPlayerFactionId);
        if (Inventory && Settings) Inventory->MaxSlots = FMath::Max(1, Settings->DefaultInventorySlots);
        if (Quests && Settings) Quests->MaxActiveQuests = FMath::Max(1, Settings->MaxActiveQuests);
        if (Skills && Settings) Skills->DefaultMaxLevel = FMath::Max(1, Settings->DefaultMaxSkillLevel);
        if (Combat && Settings) Combat->RespawnDelay = FMath::Max(0.f, Settings->DefaultRespawnDelay);
    }
    if (AbilitySystem) AbilitySystem->InitAbilityActorInfo(this, this);
}

void AARPGCharacter::PossessedBy(AController* NewController)
{
    Super::PossessedBy(NewController);
    if (AbilitySystem) AbilitySystem->InitAbilityActorInfo(this, this);
    if (Footsteps) Footsteps->RefreshFootstepRuntime();
    if (InventoryUI) InventoryUI->HandleOwnerControlChanged();
    if (HasAuthority() && NewController && NewController->IsPlayerController())
    {
        const UARPGDeveloperSettings* Settings = GetDefault<UARPGDeveloperSettings>();
        if (Faction && Faction->GetPrimaryFactionId().IsNone() && Settings && !Settings->DefaultPlayerFactionId.IsNone())
            Faction->SetPrimaryFactionId(Settings->DefaultPlayerFactionId);
        if (Combat) Combat->bAutoRespawn = bEnablePlayerAutoRespawn;
    }
}

void AARPGCharacter::OnRep_PlayerState()
{
    Super::OnRep_PlayerState();
    if (AbilitySystem) AbilitySystem->InitAbilityActorInfo(this, this);
    if (Footsteps) Footsteps->RefreshFootstepRuntime();
}

void AARPGCharacter::PawnClientRestart()
{
    Super::PawnClientRestart();
    if (Footsteps) Footsteps->RefreshFootstepRuntime();
    if (InventoryUI) InventoryUI->HandleOwnerControlChanged();
}

void AARPGCharacter::EnsureCharacterId()
{
    if (HasAuthority() && !CharacterId.IsValid()) CharacterId = FGuid::NewGuid();
}

void AARPGCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AARPGCharacter, CharacterId);
    DOREPLIFETIME(AARPGCharacter, RPGCharacterName);
}


bool AARPGCharacter::BasicAttack(AActor* OptionalTarget)
{
    AActor* ResolvedTarget = OptionalTarget;
    if (!ResolvedTarget && Targeting) ResolvedTarget = Targeting->GetCurrentTarget();
    if (Targeting) Targeting->RequestAttackFacing();
    return Combat ? Combat->PerformBasicAttack(ResolvedTarget) : false;
}

bool AARPGCharacter::Dodge(EARPGDodgeDirection Direction)
{
    return Combat ? Combat->PerformDodge(Direction) : false;
}

bool AARPGCharacter::BlockPressed()
{
    return Combat ? Combat->StartBlocking() : false;
}

void AARPGCharacter::BlockReleased()
{
    if (Combat) Combat->StopBlocking();
}


bool AARPGCharacter::ToggleLockOn()
{
    return Targeting ? Targeting->ToggleLockOn() : false;
}

bool AARPGCharacter::TargetLeft()
{
    return Targeting ? Targeting->SwitchTargetLeft() : false;
}

bool AARPGCharacter::TargetRight()
{
    return Targeting ? Targeting->SwitchTargetRight() : false;
}

void AARPGCharacter::ClearLockOn()
{
    if (Targeting) Targeting->UnlockTarget();
}

bool AARPGCharacter::QuickAccessPressed(int32 SlotNumber)
{
    return QuickAccess ? QuickAccess->ActivateSlot(SlotNumber) : false;
}

bool AARPGCharacter::QuickAccessNext()
{
    return QuickAccess ? QuickAccess->ActivateNextSlot() : false;
}

bool AARPGCharacter::QuickAccessPrevious()
{
    return QuickAccess ? QuickAccess->ActivatePreviousSlot() : false;
}

bool AARPGCharacter::UseActiveQuickAccessItem()
{
    return QuickAccess ? QuickAccess->UseActiveSlot() : false;
}

bool AARPGCharacter::UseInventoryItem(FGuid ItemInstanceId)
{
    return ItemUse ? ItemUse->UseItem(ItemInstanceId, EARPGItemUseSource::Blueprint, 0) : false;
}

bool AARPGCharacter::UseFirstInventoryItemById(FName ItemId)
{
    return ItemUse ? ItemUse->UseFirstItemById(ItemId, EARPGItemUseSource::Blueprint) : false;
}


bool AARPGCharacter::CraftRecipe(UARPGRecipeDefinition* Recipe, int32 Count)
{
    return Crafting ? Crafting->CraftRecipe(Recipe, Count) : false;
}

bool AARPGCharacter::CancelCrafting()
{
    return Crafting ? Crafting->CancelCrafting() : false;
}

bool AARPGCharacter::RepairInventoryItem(FGuid ItemInstanceId)
{
    return Crafting ? Crafting->RepairItem(ItemInstanceId) : false;
}

bool AARPGCharacter::StartWoodcuttingFromView()
{
    return Woodcutting ? Woodcutting->StartWoodcuttingFromView() : false;
}

bool AARPGCharacter::StartWoodcutting(AARPGTree* Tree)
{
    return Woodcutting ? Woodcutting->StartWoodcutting(Tree) : false;
}

bool AARPGCharacter::ChopTreeOnce(AARPGTree* Tree)
{
    return Woodcutting ? Woodcutting->ChopTreeOnce(Tree) : false;
}

void AARPGCharacter::StopWoodcutting()
{
    if (Woodcutting) Woodcutting->StopWoodcutting();
}


bool AARPGCharacter::OpenStatsUI()
{
    return StatsUI ? StatsUI->OpenStatsUI() : false;
}

bool AARPGCharacter::CloseStatsUI()
{
    return StatsUI ? StatsUI->CloseStatsUI() : false;
}

bool AARPGCharacter::ToggleStatsUI()
{
    return StatsUI ? StatsUI->ToggleStatsUI() : false;
}

bool AARPGCharacter::IsStatsUIOpen() const
{
    return StatsUI ? StatsUI->IsStatsUIOpen() : false;
}

bool AARPGCharacter::OpenInventoryUI()
{
    return InventoryUI ? InventoryUI->OpenInventoryUI() : false;
}

bool AARPGCharacter::CloseInventoryUI()
{
    return InventoryUI ? InventoryUI->CloseInventoryUI() : false;
}

bool AARPGCharacter::ToggleInventoryUI()
{
    return InventoryUI ? InventoryUI->ToggleInventoryUI() : false;
}

bool AARPGCharacter::IsInventoryUIOpen() const
{
    return InventoryUI ? InventoryUI->IsInventoryUIOpen() : false;
}

bool AARPGCharacter::OpenCraftingUI()
{
    return InventoryUI ? InventoryUI->OpenCraftingUI() : false;
}


bool AARPGCharacter::OpenBuildMenuUI() { return BuildingUI ? BuildingUI->OpenBuildMenu() : false; }
bool AARPGCharacter::CloseBuildMenuUI() { return BuildingUI ? BuildingUI->CloseBuildMenu() : false; }
bool AARPGCharacter::ToggleBuildMenuUI() { return BuildingUI ? BuildingUI->ToggleBuildMenu() : false; }
bool AARPGCharacter::BeginBuildPlacement(UARPGBuildPieceDefinition* Piece) { return Building ? Building->BeginBuildMode(Piece) : false; }
bool AARPGCharacter::ConfirmBuildPlacement() { return Building ? Building->ConfirmPreviewPlacement() : false; }
void AARPGCharacter::RotateBuildPlacement(float Direction) { if (Building) Building->RotatePreview(Direction); }
bool AARPGCharacter::NextBuildPiece() { return Building ? Building->SelectNextBuildPiece() : false; }
bool AARPGCharacter::PreviousBuildPiece() { return Building ? Building->SelectPreviousBuildPiece() : false; }
void AARPGCharacter::CancelBuildPlacement() { if (Building) Building->EndBuildMode(); }
bool AARPGCharacter::InteractBuiltStructure() { return BuildingUI ? BuildingUI->InteractWithBuiltStructureFromView() : false; }
bool AARPGCharacter::DemolishBuiltStructure() { return BuildingUI ? BuildingUI->DemolishBuiltStructureFromView() : false; }
bool AARPGCharacter::CloseBuiltStructureUI()
{
    const bool bBuildingClosed = BuildingUI ? BuildingUI->CloseStructureUI() : false;
    const bool bSettlementClosed = SettlementUI ? SettlementUI->CloseAllSettlementUI() : false;
    return bBuildingClosed || bSettlementClosed;
}
bool AARPGCharacter::OpenNearbySettlementUI() { return SettlementUI && SettlementUI->GetNearbySettlementHub() ? SettlementUI->OpenSettlementPanel(SettlementUI->GetNearbySettlementHub()) : false; }
bool AARPGCharacter::CloseSettlementUI() { return SettlementUI ? SettlementUI->CloseAllSettlementUI() : false; }
