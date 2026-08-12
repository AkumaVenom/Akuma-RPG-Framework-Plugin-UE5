#include "Actors/ARPGCharacter.h"
#include "AbilitySystemComponent.h"
#include "ARPGDeveloperSettings.h"
#include "Components/ARPGStatsComponent.h"
#include "Components/ARPGCombatComponent.h"
#include "Components/ARPGProgressionComponent.h"
#include "Components/ARPGInventoryComponent.h"
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
#include "Mounts/ARPGMountComponent.h"
#include "Social/ARPGGroupComponent.h"
#include "Components/ARPGThreatComponent.h"
#include "Components/ARPGAICombatComponent.h"
#include "Components/ARPGTargetingComponent.h"
#include "Components/ARPGWoodcuttingComponent.h"
#include "Components/ARPGFootstepComponent.h"
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
    Mounts = CreateDefaultSubobject<UARPGMountComponent>(TEXT("Mounts"));
    Group = CreateDefaultSubobject<UARPGGroupComponent>(TEXT("Group"));
    Threat = CreateDefaultSubobject<UARPGThreatComponent>(TEXT("Threat"));
    AICombat = CreateDefaultSubobject<UARPGAICombatComponent>(TEXT("AICombat"));
    Targeting = CreateDefaultSubobject<UARPGTargetingComponent>(TEXT("Targeting"));
    Woodcutting = CreateDefaultSubobject<UARPGWoodcuttingComponent>(TEXT("Woodcutting"));
    Footsteps = CreateDefaultSubobject<UARPGFootstepComponent>(TEXT("Footsteps"));
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
