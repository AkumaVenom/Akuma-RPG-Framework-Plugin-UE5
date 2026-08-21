#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "Combat/ARPGCombatTypes.h"
#include "ARPGCharacter.generated.h"

class UAbilitySystemComponent;
class UARPGStatsComponent;
class UARPGCombatComponent;
class UARPGProgressionComponent;
class UARPGInventoryComponent;
class UARPGItemUseComponent;
class UARPGCraftingComponent;
class UARPGRecipeDefinition;
class UARPGEquipmentComponent;
class UARPGQuickAccessComponent;
class UARPGCurrencyComponent;
class UARPGQuestComponent;
class UARPGSkillComponent;
class UARPGSlayerComponent;
class UARPGFactionComponent;
class UARPGBattlePetComponent;
class UARPGBattlePetBattleComponent;
class UARPGClassComponent;
class UARPGAbilityBridgeComponent;
class UARPGEventRouterComponent;
class UARPGPersistenceComponent;
class UARPGInteractionComponent;
class UARPGBuildingComponent;
class UARPGBuildingUIComponent;
class UARPGSettlementUIComponent;
class UARPGBuildPieceDefinition;
class UARPGMountComponent;
class UARPGGroupComponent;
class UARPGThreatComponent;
class UARPGAICombatComponent;
class UARPGTargetingComponent;
class UARPGWoodcuttingComponent;
class UARPGFootstepComponent;
class UARPGCharacterInfoComponent;
class UARPGStatsUIComponent;
class UARPGInventoryUIComponent;
class AARPGTree;

UCLASS(BlueprintType, Blueprintable)
class AKUMASRPGFRAMEWORK_API AARPGCharacter : public ACharacter, public IAbilitySystemInterface
{
    GENERATED_BODY()
public:
    AARPGCharacter();

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, SaveGame, Category="Identity") FGuid CharacterId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, SaveGame, Category="Identity") FString RPGCharacterName = TEXT("Hero");
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ARPG|Combat") bool bEnablePlayerAutoRespawn = true;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="ARPG") TObjectPtr<UAbilitySystemComponent> AbilitySystem;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="ARPG") TObjectPtr<UARPGStatsComponent> Stats;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="ARPG") TObjectPtr<UARPGCombatComponent> Combat;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="ARPG") TObjectPtr<UARPGProgressionComponent> Progression;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="ARPG") TObjectPtr<UARPGInventoryComponent> Inventory;
    /** Server-authoritative generic consumable/custom item-use pipeline shared by Inventory UI and Quick Access. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="ARPG") TObjectPtr<UARPGItemUseComponent> ItemUse;
    /** Player crafting/repair authority component. Recipes are data-driven and reuse the framework recipe assets. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="ARPG") TObjectPtr<UARPGCraftingComponent> Crafting;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="ARPG") TObjectPtr<UARPGEquipmentComponent> Equipment;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="ARPG") TObjectPtr<UARPGQuickAccessComponent> QuickAccess;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="ARPG") TObjectPtr<UARPGCurrencyComponent> Currencies;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="ARPG") TObjectPtr<UARPGQuestComponent> Quests;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="ARPG") TObjectPtr<UARPGSkillComponent> Skills;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="ARPG") TObjectPtr<UARPGSlayerComponent> Slayer;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="ARPG") TObjectPtr<UARPGFactionComponent> Faction;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="ARPG") TObjectPtr<UARPGBattlePetComponent> BattlePets;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="ARPG") TObjectPtr<UARPGBattlePetBattleComponent> BattlePetBattle;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="ARPG") TObjectPtr<UARPGClassComponent> ClassComponent;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="ARPG") TObjectPtr<UARPGAbilityBridgeComponent> AbilityBridge;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="ARPG") TObjectPtr<UARPGEventRouterComponent> EventRouter;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="ARPG") TObjectPtr<UARPGPersistenceComponent> Persistence;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="ARPG") TObjectPtr<UARPGInteractionComponent> Interaction;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="ARPG") TObjectPtr<UARPGBuildingComponent> Building;
    /** Local ready build catalogue / placement HUD / storage / production station UI owner. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="ARPG") TObjectPtr<UARPGBuildingUIComponent> BuildingUI;
    /** Local proximity Settlement HUD + Bed/Hub panels. Native fallback widgets are ready and subclassable. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="ARPG") TObjectPtr<UARPGSettlementUIComponent> SettlementUI;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="ARPG") TObjectPtr<UARPGMountComponent> Mounts;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="ARPG") TObjectPtr<UARPGGroupComponent> Group;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="ARPG") TObjectPtr<UARPGThreatComponent> Threat;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="ARPG") TObjectPtr<UARPGAICombatComponent> AICombat;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="ARPG") TObjectPtr<UARPGTargetingComponent> Targeting;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="ARPG") TObjectPtr<UARPGWoodcuttingComponent> Woodcutting;
    /** Automatic physical-surface footsteps for players and NPCs. Select this inherited component to author audio/stride settings. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="ARPG") TObjectPtr<UARPGFootstepComponent> Footsteps;
    /** Automatic local overhead name/level/health popup. Select this inherited component and assign a Widget Class. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="ARPG") TObjectPtr<UARPGCharacterInfoComponent> CharacterInfo;
    /** Local player JRPG stats panel owner. Select this inherited component to assign a custom Stats Widget Class. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="ARPG") TObjectPtr<UARPGStatsUIComponent> StatsUI;
    /** Ready-to-use local player Inventory + Quick Access UI. Select this inherited component to assign custom widget classes. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="ARPG") TObjectPtr<UARPGInventoryUIComponent> InventoryUI;

    UFUNCTION(BlueprintCallable, Category="ARPG|Combat|Input") bool BasicAttack(AActor* OptionalTarget = nullptr);
    UFUNCTION(BlueprintCallable, Category="ARPG|Combat|Input") bool Dodge(EARPGDodgeDirection Direction = EARPGDodgeDirection::Auto);
    UFUNCTION(BlueprintCallable, Category="ARPG|Combat|Input") bool BlockPressed();
    UFUNCTION(BlueprintCallable, Category="ARPG|Combat|Input") void BlockReleased();
    UFUNCTION(BlueprintCallable, Category="ARPG|Targeting|Input") bool ToggleLockOn();
    UFUNCTION(BlueprintCallable, Category="ARPG|Targeting|Input") bool TargetLeft();
    UFUNCTION(BlueprintCallable, Category="ARPG|Targeting|Input") bool TargetRight();
    UFUNCTION(BlueprintCallable, Category="ARPG|Targeting|Input") void ClearLockOn();

    // 1-based quick-access wrappers keep player Blueprints clean: key 1 -> slot 1, key 2 -> slot 2, etc.
    UFUNCTION(BlueprintCallable, Category="ARPG|Quick Access|Input") bool QuickAccessPressed(int32 SlotNumber);
    UFUNCTION(BlueprintCallable, Category="ARPG|Quick Access|Input") bool QuickAccessNext();
    UFUNCTION(BlueprintCallable, Category="ARPG|Quick Access|Input") bool QuickAccessPrevious();
    UFUNCTION(BlueprintCallable, Category="ARPG|Quick Access|Input") bool UseActiveQuickAccessItem();

    // Direct item-use helpers. These work even when the item is not assigned to Quick Access.
    UFUNCTION(BlueprintCallable, Category="ARPG|Item Use|Input") bool UseInventoryItem(FGuid ItemInstanceId);
    UFUNCTION(BlueprintCallable, Category="ARPG|Item Use|Input") bool UseFirstInventoryItemById(FName ItemId);

    UFUNCTION(BlueprintCallable, Category="ARPG|Crafting|Input") bool CraftRecipe(UARPGRecipeDefinition* Recipe, int32 Count = 1);
    UFUNCTION(BlueprintCallable, Category="ARPG|Crafting|Input") bool CancelCrafting();
    UFUNCTION(BlueprintCallable, Category="ARPG|Crafting|Repair") bool RepairInventoryItem(FGuid ItemInstanceId);

    UFUNCTION(BlueprintCallable, Category="ARPG|Woodcutting|Input") bool StartWoodcuttingFromView();
    UFUNCTION(BlueprintCallable, Category="ARPG|Woodcutting|Input") bool StartWoodcutting(AARPGTree* Tree);
    UFUNCTION(BlueprintCallable, Category="ARPG|Woodcutting|Input") bool ChopTreeOnce(AARPGTree* Tree);
    UFUNCTION(BlueprintCallable, Category="ARPG|Woodcutting|Input") void StopWoodcutting();

    // One-call local player stats panel wrappers. The inherited StatsUI component owns presentation/input state.
    UFUNCTION(BlueprintCallable, Category="ARPG|Stats UI|Input") bool OpenStatsUI();
    UFUNCTION(BlueprintCallable, Category="ARPG|Stats UI|Input") bool CloseStatsUI();
    UFUNCTION(BlueprintCallable, Category="ARPG|Stats UI|Input") bool ToggleStatsUI();
    UFUNCTION(BlueprintPure, Category="ARPG|Stats UI") bool IsStatsUIOpen() const;

    // One-call local Inventory panel wrappers. The Quick Access HUD is auto-created by the inherited InventoryUI component.
    UFUNCTION(BlueprintCallable, Category="ARPG|Inventory UI|Input") bool OpenInventoryUI();
    UFUNCTION(BlueprintCallable, Category="ARPG|Inventory UI|Input") bool CloseInventoryUI();
    UFUNCTION(BlueprintCallable, Category="ARPG|Inventory UI|Input") bool ToggleInventoryUI();
    UFUNCTION(BlueprintPure, Category="ARPG|Inventory UI") bool IsInventoryUIOpen() const;
    UFUNCTION(BlueprintCallable, Category="ARPG|Inventory UI|Input") bool OpenCraftingUI();

    // Ready world-building wrappers. Wire these to your input actions; authority remains inside Building/Interaction.
    UFUNCTION(BlueprintCallable, Category="ARPG|Building UI|Input") bool OpenBuildMenuUI();
    UFUNCTION(BlueprintCallable, Category="ARPG|Building UI|Input") bool CloseBuildMenuUI();
    UFUNCTION(BlueprintCallable, Category="ARPG|Building UI|Input") bool ToggleBuildMenuUI();
    UFUNCTION(BlueprintCallable, Category="ARPG|Building|Input") bool BeginBuildPlacement(UARPGBuildPieceDefinition* Piece);
    UFUNCTION(BlueprintCallable, Category="ARPG|Building|Input") bool ConfirmBuildPlacement();
    UFUNCTION(BlueprintCallable, Category="ARPG|Building|Input") void RotateBuildPlacement(float Direction = 1.f);
    UFUNCTION(BlueprintCallable, Category="ARPG|Building|Input") bool NextBuildPiece();
    UFUNCTION(BlueprintCallable, Category="ARPG|Building|Input") bool PreviousBuildPiece();
    UFUNCTION(BlueprintCallable, Category="ARPG|Building|Input") void CancelBuildPlacement();
    UFUNCTION(BlueprintCallable, Category="ARPG|Building UI|Input") bool InteractBuiltStructure();
    UFUNCTION(BlueprintCallable, Category="ARPG|Building UI|Input") bool DemolishBuiltStructure();
    UFUNCTION(BlueprintCallable, Category="ARPG|Building UI|Input") bool CloseBuiltStructureUI();
    UFUNCTION(BlueprintCallable, Category="ARPG|Settlement UI|Input") bool OpenNearbySettlementUI();
    UFUNCTION(BlueprintCallable, Category="ARPG|Settlement UI|Input") bool CloseSettlementUI();

    virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override { return AbilitySystem; }
    virtual void BeginPlay() override;
    virtual void PossessedBy(AController* NewController) override;
    virtual void OnRep_PlayerState() override;
    virtual void PawnClientRestart() override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UFUNCTION(BlueprintCallable, Category="ARPG|Identity", meta=(BlueprintAuthorityOnly)) void EnsureCharacterId();
};
