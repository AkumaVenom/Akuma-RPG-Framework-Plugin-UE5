#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Engine/EngineTypes.h"
#include "Stats/ARPGStatTypes.h"
#include "ARPGTypes.generated.h"

class UAnimMontage;
class UARPGItemDefinition;
class AActor;

UENUM(BlueprintType)
enum class EARPGLifeState : uint8 { Alive, Downed, Dead, Respawning };

UENUM(BlueprintType)
enum class EARPGRarity : uint8 { Poor, Common, Uncommon, Rare, Epic, Legendary, Mythic };

// Defines what pressing a quick-access slot does. Auto keeps existing item assets backward-compatible:
// equippable items equip/switch, usable items use immediately, and other items become selection-only.
UENUM(BlueprintType)
enum class EARPGQuickAccessAction : uint8
{
    Auto,
    Equip,
    Use,
    SelectOnly
};

UENUM(BlueprintType)
enum class EARPGQuickAccessResult : uint8
{
    Success,
    InvalidSlot,
    EmptySlot,
    ItemUnavailable,
    ItemNotAllowed,
    EquipFailed,
    ItemNotUsable,
    OnCooldown,
    InsufficientQuantity,
    NoUsefulEffect,
    UseFailed
};

UENUM(BlueprintType)
enum class EARPGQuestState : uint8 { Inactive, Active, ObjectivesComplete, Completed, Failed };

UENUM(BlueprintType)
enum class EARPGQuestObjectiveType : uint8
{
    Kill, Collect, Interact, Explore, CapturePet, WinPetBattle, Dungeon, RaidBoss,
    Craft, Reputation, Skill, Build, Mount, Custom
};

UENUM(BlueprintType)
enum class EARPGEncounterState : uint8 { NotStarted, InProgress, Failed, Completed };

UENUM(BlueprintType)
enum class EARPGGroupRole : uint8 { None, Tank, Healer, Damage };

UENUM(BlueprintType)
enum class EARPGGroupType : uint8 { Solo, Party, Raid };

UENUM(BlueprintType)
enum class EARPGChatChannel : uint8
{
    Say, Yell, Whisper, World, Zone, Party, Raid, Guild, Faction,
    System, Quest, Loot, Combat, NPC, Boss, Event, Custom
};

UENUM(BlueprintType)
enum class EARPGFactionDisposition : uint8
{
    Hated, Hostile, Unfriendly, Neutral, Friendly, Honored, Revered, Exalted
};

UENUM(BlueprintType)
enum class EARPGSpawnerShape : uint8 { Point, Circle, Box };

UENUM(BlueprintType)
enum class EARPGRespawnMode : uint8 { Individual, WholeGroup, Never };

UENUM(BlueprintType)
enum class EARPGPetBattleState : uint8
{
    None, Intro, ChoosingAction, ResolvingTurn, Victory, Defeat, Captured, Fled
};

UENUM(BlueprintType)
enum class EARPGCraftingStationState : uint8 { Idle, Crafting, Paused, Blocked };

UENUM(BlueprintType)
enum class EARPGPlacementResult : uint8
{
    Valid, NoPiece, TooFar, Blocked, Unsupported, MissingResources, Restricted, InvalidSurface
};

UENUM(BlueprintType)
enum class EARPGMountMovementType : uint8 { Ground, Flying, Aquatic, Amphibious };

UENUM(BlueprintType)
enum class EARPGBossType : uint8 { Rare, Elite, World, Dungeon, Raid, Custom };

UENUM(BlueprintType)
enum class EARPGPetAbilityTarget : uint8 { Enemy, Self, Ally };

UENUM(BlueprintType)
enum class EARPGWandererActivity : uint8 { Idle, Roam, Quest, Hunt, Vendor, Rest, Social, Dungeon, Custom };

USTRUCT(BlueprintType)
struct AKUMASRPGFRAMEWORK_API FARPGCombatMontageSet
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Animation") TArray<TSoftObjectPtr<UAnimMontage>> MeleeAttacks;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Animation") TArray<TSoftObjectPtr<UAnimMontage>> RangedAttacks;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Animation") TArray<TSoftObjectPtr<UAnimMontage>> MagicCasts;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Animation") TSoftObjectPtr<UAnimMontage> HitReact;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Animation") TSoftObjectPtr<UAnimMontage> Death;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Animation") TSoftObjectPtr<UAnimMontage> Revive;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Animation") TSoftObjectPtr<UAnimMontage> Interact;
};

USTRUCT(BlueprintType)
struct AKUMASRPGFRAMEWORK_API FARPGQuickAccessSlot
{
    GENERATED_BODY()

    // Stable assignment identity. This is only a bookmark; activation still requires a real owned runtime entry.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) FName ItemId = NAME_None;

    // Exact currently-bound runtime inventory instance. If a stack is depleted/rebuilt, the component can rebind
    // ItemId to another owned runtime instance without ever treating a project Data Asset as ownership.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) FGuid ItemInstanceId;

    // Monotonic assignment revision. This lets duplicate repair deterministically keep the most recently assigned
    // slot, including across replication/save migration where legacy duplicate state may already exist.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame) int32 AssignmentRevision = 0;

    // Runtime-only server time for UI cooldown display. Intentionally not SaveGame.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float CooldownEndServerTime = 0.f;
};

USTRUCT(BlueprintType)
struct AKUMASRPGFRAMEWORK_API FARPGInventoryEntry
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) FGuid InstanceId;
    // Exact asset reference for the runtime item instance. Keeping this alongside the stable ItemId avoids
    // re-resolving newly-authored project items through Asset Manager just to equip/use them.
    // The soft reference is save/replication friendly and remains backward-compatible with older ID-only saves.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) TSoftObjectPtr<UARPGItemDefinition> ItemDefinition;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) FName ItemId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) int32 Quantity = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) float Durability = 100.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) bool bBound = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) bool bEquipped = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) FGameplayTag EquipmentSlot;
};

USTRUCT(BlueprintType)
struct AKUMASRPGFRAMEWORK_API FARPGQuestObjectiveProgress
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) FName ObjectiveId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) EARPGQuestObjectiveType Type = EARPGQuestObjectiveType::Custom;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) FGameplayTag TargetTag;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) FName TargetId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) int32 Current = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) int32 Required = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) bool bComplete = false;
};

USTRUCT(BlueprintType)
struct AKUMASRPGFRAMEWORK_API FARPGQuestRuntime
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) FName QuestId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) EARPGQuestState State = EARPGQuestState::Inactive;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) TArray<FARPGQuestObjectiveProgress> Objectives;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) bool bTracked = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) int32 CompletionCount = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) FDateTime AcceptedAtUtc;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) FDateTime CompletedAtUtc;
};

USTRUCT(BlueprintType)
struct AKUMASRPGFRAMEWORK_API FARPGSkillState
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) FName SkillId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) int32 Level = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) int64 XP = 0;
};

USTRUCT(BlueprintType)
struct AKUMASRPGFRAMEWORK_API FARPGSlayerTask
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) FGuid TaskInstanceId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) FName MasterId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) FName TaskId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) FGameplayTag TargetCategory;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) int32 RequiredKills = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) int32 CurrentKills = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) int32 SlayerXPPerKill = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) int32 CompletionPoints = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) bool bComplete = false;
};

USTRUCT(BlueprintType)
struct AKUMASRPGFRAMEWORK_API FARPGFactionStanding
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) FName FactionId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) int32 Reputation = 0;
};

USTRUCT(BlueprintType)
struct AKUMASRPGFRAMEWORK_API FARPGCurrencyBalance
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) FName CurrencyId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) int64 Amount = 0;
};

USTRUCT(BlueprintType)
struct AKUMASRPGFRAMEWORK_API FARPGVendorStockEntry
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName ItemId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Quantity = -1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int64 UnitPrice = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName CurrencyId = TEXT("Gold");
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float RestockSeconds = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 InitialQuantity = -1;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FDateTime NextRestockUtc;
};

USTRUCT(BlueprintType)
struct AKUMASRPGFRAMEWORK_API FARPGChatMessage
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) FGuid MessageId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EARPGChatChannel Channel = EARPGChatChannel::Say;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString SenderName;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FGuid SenderAccountId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString TargetName;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FText Message;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FGameplayTag MessageTag;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FDateTime TimestampUtc;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bFromNPC = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bSystemGenerated = false;
};

USTRUCT(BlueprintType)
struct AKUMASRPGFRAMEWORK_API FARPGThreatEntry
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) TObjectPtr<AActor> Actor = nullptr;
    UPROPERTY(BlueprintReadOnly) float Threat = 0.f;
};

USTRUCT(BlueprintType)
struct AKUMASRPGFRAMEWORK_API FARPGPetAbilityCooldown
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName AbilityId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 RemainingTurns = 0;
};

USTRUCT(BlueprintType)
struct AKUMASRPGFRAMEWORK_API FARPGPetBattleUnit
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) FGuid PetInstanceId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName SpeciesId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Level = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EARPGRarity Quality = EARPGRarity::Common;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float MaxHealth = 100.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float CurrentHealth = 100.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Power = 10.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Speed = 10.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FName> AbilityIds;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FARPGPetAbilityCooldown> Cooldowns;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FGameplayTagContainer StatusTags;
};

USTRUCT(BlueprintType)
struct AKUMASRPGFRAMEWORK_API FARPGPetInstance
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) FGuid InstanceId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) FName SpeciesId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) FString CustomName;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) int32 Level = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) int64 XP = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) EARPGRarity Quality = EARPGRarity::Common;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) float CurrentHealth = 100.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) TArray<FName> EquippedAbilityIds;
};

USTRUCT(BlueprintType)
struct AKUMASRPGFRAMEWORK_API FARPGEncounterRuntime
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) FName EncounterId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) EARPGEncounterState State = EARPGEncounterState::NotStarted;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) int32 WipeCount = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) FDateTime LastCompletedUtc;
};

USTRUCT(BlueprintType)
struct AKUMASRPGFRAMEWORK_API FARPGGroupMembership
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) FGuid GroupId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) EARPGGroupType GroupType = EARPGGroupType::Solo;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) EARPGGroupRole Role = EARPGGroupRole::None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) bool bLeader = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) bool bAssistant = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) int32 Subgroup = 0;
};

USTRUCT(BlueprintType)
struct AKUMASRPGFRAMEWORK_API FARPGCraftQueueEntry
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) FGuid QueueId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) FGuid CrafterCharacterId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) FName RecipeId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) int32 RemainingCount = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) float ProgressSeconds = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) FDateTime LastUpdatedUtc;
};

USTRUCT(BlueprintType)
struct AKUMASRPGFRAMEWORK_API FARPGPlacedBuildingSave
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) FGuid BuildingId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) FName PieceId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) FSoftClassPath ActorClass;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) FTransform Transform;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) float Health = 100.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) int32 UpgradeLevel = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) FGuid OwnerAccountId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) FGuid OwnerCharacterId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) FName OwnerFactionId = NAME_None;
};

USTRUCT(BlueprintType)
struct AKUMASRPGFRAMEWORK_API FARPGContainerSave
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) FGuid ContainerId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) FGuid LinkedBuildingId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) FSoftClassPath ActorClass;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) FTransform Transform;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) TArray<FARPGInventoryEntry> Items;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) TArray<FARPGCraftQueueEntry> CraftQueue;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) TArray<FARPGInventoryEntry> OutputItems;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) FGuid OwnerAccountId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) FGuid OwnerCharacterId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) FName OwnerFactionId = NAME_None;
};

USTRUCT(BlueprintType)
struct AKUMASRPGFRAMEWORK_API FARPGMountSaveState
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) TArray<FName> UnlockedMountIds;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) FName ActiveMountId = NAME_None;
};

USTRUCT(BlueprintType)
struct AKUMASRPGFRAMEWORK_API FARPGCharacterSaveData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) FGuid CharacterId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) FString CharacterName;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) FName ClassId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) FName PrimaryFactionId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) FName GuildId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) int32 Level = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) int64 XP = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) FARPGStatProgressionSaveState StatProgression;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) FVector Location = FVector::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) FRotator Rotation = FRotator::ZeroRotator;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) float Health = 100.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) float Mana = 100.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) float Stamina = 100.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) TArray<FARPGInventoryEntry> Inventory;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) TArray<FARPGQuickAccessSlot> QuickAccessSlots;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) int32 ActiveQuickAccessSlotNumber = 0;
    /** Active personal craft. The current craft's ingredients are already committed in Inventory. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) FARPGCraftQueueEntry PersonalCraftingState;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) TArray<FARPGQuestRuntime> Quests;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) TArray<FARPGSkillState> Skills;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) FARPGSlayerTask SlayerTask;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) int32 SlayerPoints = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) int32 SlayerStreak = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) TArray<FARPGFactionStanding> Reputation;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) TArray<FARPGCurrencyBalance> Currencies;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) TArray<FARPGPetInstance> BattlePets;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) TArray<FGuid> BattlePetTeam;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) TArray<FARPGEncounterRuntime> EncounterProgress;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) FARPGGroupMembership GroupMembership;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) FARPGMountSaveState Mounts;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) TArray<FName> UnlockedRecipes;
};

USTRUCT(BlueprintType)
struct AKUMASRPGFRAMEWORK_API FARPGDungeonSaveState
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) FName DungeonId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) TArray<FARPGEncounterRuntime> Encounters;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) int32 Checkpoint = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) bool bComplete = false;
};

USTRUCT(BlueprintType)
struct AKUMASRPGFRAMEWORK_API FARPGWorldSaveData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) FString WorldId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) TArray<FARPGPlacedBuildingSave> Buildings;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) TArray<FARPGContainerSave> Containers;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) TArray<FARPGDungeonSaveState> Dungeons;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) FDateTime SavedAtUtc;
};
