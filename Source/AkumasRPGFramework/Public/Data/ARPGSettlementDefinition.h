#pragma once

#include "CoreMinimal.h"
#include "Data/ARPGDefinitionBase.h"
#include "ARPGSettlementDefinition.generated.h"

class AARPGSettlementVillagerCharacter;
class UARPGItemDefinition;

/**
 * Data-driven settlement profile consumed by a buildable Settlement Hub.
 * A Hub is the explicit opt-in boundary: without one, ordinary building pieces stay ordinary and no
 * settlement scan, villager recruitment or settlement work runs.
 */
UCLASS(BlueprintType)
class AKUMASRPGFRAMEWORK_API UARPGSettlementDefinition : public UARPGDefinitionBase
{
    GENERATED_BODY()
public:
    /** Maximum world-space radius owned/managed by this Hub. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Settlement|Boundary", meta=(ClampMin="300.0", Units="cm"))
    float SettlementRadius = 5000.f;
    /** Local HUD appears while a player is this close to the Hub. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Settlement|Boundary", meta=(ClampMin="100.0", Units="cm"))
    float SettlementHUDRadius = 1800.f;
    /** Palbox-style safety: completed settlement areas may not overlap when placing a new Hub. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Settlement|Boundary")
    bool bPreventOverlappingSettlementAreas = true;
    /** Additional separation beyond the sum of both Hub radii when overlap prevention is enabled. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Settlement|Boundary", meta=(ClampMin="0.0", Units="cm", EditCondition="bPreventOverlappingSettlementAreas"))
    float SettlementSeparationPadding = 0.f;
    /** Server refresh cadence for homes/beds/resident assignment. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Settlement|Runtime", meta=(ClampMin="0.5", Units="s"))
    float SettlementRefreshInterval = 5.f;
    /** Grace period after Hub construction/load before automatic recruitment begins. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Settlement|Runtime", meta=(ClampMin="0.0", Units="s"))
    float InitialRecruitmentDelay = 5.f;
    /** Minimum delay between newly recruited villagers. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Settlement|Runtime", meta=(ClampMin="0.1", Units="s"))
    float RecruitmentInterval = 12.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Settlement|Residents", meta=(ClampMin="1"))
    int32 MaximumVillagers = 20;
    /** Native class or Blueprint subclass spawned for new residents. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Settlement|Residents")
    TSoftClassPtr<AARPGSettlementVillagerCharacter> VillagerClass;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Settlement|Residents", meta=(ClampMin="100.0", Units="cm"))
    float VillagerWanderRadius = 1400.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Settlement|Residents", meta=(ClampMin="0.2", Units="s"))
    float VillagerThinkInterval = 3.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Settlement|Residents")
    TArray<FString> VillagerNamePool;
    /** If true, same-faction completed structures can contribute to a Hub even when another faction member built them. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Settlement|Ownership")
    bool bAcceptSameFactionBuildings = true;

    /** Canonical grid cell used by the native home validator. Match the modular building kit. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Settlement|Housing", meta=(ClampMin="50.0", Units="cm"))
    float HomeGridSize = 300.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Settlement|Housing", meta=(ClampMin="2"))
    int32 MinimumFoundationWidth = 2;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Settlement|Housing", meta=(ClampMin="2"))
    int32 MinimumFoundationDepth = 2;
    /** Safety/performance cap for the native rectangle search. Larger connected builds may contain multiple homes. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Settlement|Housing", meta=(ClampMin="4", ClampMax="32"))
    int32 MaximumHomeDimensionCells = 12;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Settlement|Housing", meta=(ClampMin="50.0", Units="cm"))
    float HomeStoryHeight = 300.f;
    /** Positional tolerance used only by semantic home-grid matching; it never changes build collision. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Settlement|Housing", meta=(ClampMin="1.0", Units="cm"))
    float HomeGridTolerance = 24.f;
    /** Roof pieces may satisfy the complete overhead cover requirement in addition to Floor/Ceiling. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Settlement|Housing")
    bool bAllowRoofAsHomeCover = true;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Settlement|Woodcutting")
    bool bEnableVillagerWoodcutting = true;
    /** Chance an idle resident chooses a woodcutting job instead of another roam/home activity. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Settlement|Woodcutting", meta=(ClampMin="0.0", ClampMax="1.0"))
    float WoodcuttingDutyChance = 0.45f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Settlement|Woodcutting", meta=(ClampMin="100.0", Units="cm"))
    float WoodcuttingRadius = 2600.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Settlement|Woodcutting", meta=(ClampMin="0.5", Units="s"))
    float WoodcuttingSearchInterval = 8.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Settlement|Woodcutting", meta=(ClampMin="1.0"))
    float VillagerChopPower = 25.f;
    /** Settlement workers may harvest configured ARPGTree actors without requiring a player equipment loadout. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Settlement|Woodcutting")
    bool bVillagersIgnoreTreeSkillAndToolRequirements = true;
    /** Optional multicast chopping montage for settlement workers. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Settlement|Woodcutting")
    TSoftObjectPtr<class UAnimMontage> VillagerChopMontage;

    /**
     * Item Definition used only for the resident's contextual woodcutting-tool presentation. The Item's
     * Equipped Static/Skeletal Mesh, Attach Socket, Equipped Relative Transform and optional equip/unequip
     * montage/sounds are reused; no inventory item is granted, consumed or durability-worn by this visual.
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Settlement|Woodcutting|Tool Presentation", meta=(DisplayName="Villager Woodcutting Tool Item"))
    TSoftObjectPtr<UARPGItemDefinition> VillagerWoodcuttingToolItem;

    /** If true, the axe/tool is held while the resident travels to the reserved tree as well as while chopping. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Settlement|Woodcutting|Tool Presentation", meta=(DisplayName="Show Tool While Going To Work"))
    bool bShowWoodcuttingToolWhileGoingToWork = true;

    /** Reuse the Item Definition's optional Equip/Unequip montage and sounds on contextual tool transitions. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Settlement|Woodcutting|Tool Presentation", meta=(DisplayName="Play Tool Equip / Unequip Presentation"))
    bool bPlayWoodcuttingToolEquipPresentation = true;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Settlement|Woodcutting", meta=(ClampMin="0.2", Units="s"))
    float VillagerChopInterval = 1.4f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Settlement|Woodcutting", meta=(ClampMin="25.0", Units="cm"))
    float WoodcuttingAcceptanceRadius = 140.f;
    /** Maximum workers simultaneously reserved for woodcutting. 0 = all eligible residents. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Settlement|Woodcutting", meta=(ClampMin="0"))
    int32 MaximumConcurrentWoodcutters = 0;
    /** Residents deposit rewards into the Hub stockpile after a tree falls. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Settlement|Woodcutting")
    bool bDepositWoodcuttingRewardsToHub = true;

    /** Inventory slots on the Hub's native settlement stockpile. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Settlement|Stockpile", meta=(ClampMin="1"))
    int32 SettlementStockpileSlots = 96;
};
