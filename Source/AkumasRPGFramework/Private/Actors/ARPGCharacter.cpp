#include "Actors/ARPGCharacter.h"
#include "AbilitySystemComponent.h"
#include "ARPGDeveloperSettings.h"
#include "Components/ARPGStatsComponent.h"
#include "Components/ARPGCombatComponent.h"
#include "Components/ARPGProgressionComponent.h"
#include "Components/ARPGInventoryComponent.h"
#include "Components/ARPGEquipmentComponent.h"
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
}

void AARPGCharacter::BeginPlay()
{
    Super::BeginPlay();
    if (HasAuthority())
    {
        EnsureCharacterId();
        const UARPGDeveloperSettings* Settings = GetDefault<UARPGDeveloperSettings>();
        if (Faction && Faction->GetPrimaryFactionId().IsNone() && Settings && !Settings->DefaultPlayerFactionId.IsNone())
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
}

void AARPGCharacter::OnRep_PlayerState()
{
    Super::OnRep_PlayerState();
    if (AbilitySystem) AbilitySystem->InitAbilityActorInfo(this, this);
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
