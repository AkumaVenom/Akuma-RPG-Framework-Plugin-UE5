#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ARPGTypes.h"
#include "Data/ARPGRecipeDefinition.h"
#include "ARPGCraftingComponent.generated.h"

class UARPGInventoryComponent;
class UARPGItemDefinition;
class UARPGRecipeDefinition;

UENUM(BlueprintType)
enum class EARPGCraftingResult : uint8
{
    Success,
    RequestAccepted,
    InvalidRecipe,
    RecipeNotAvailable,
    RequiresStation,
    MissingSkill,
    MissingIngredients,
    InventoryFull,
    AlreadyCrafting,
    InvalidQuantity,
    Cancelled,
    InvalidRepairItem,
    AlreadyFullyRepaired,
    RepairNotConfigured,
    MissingRepairMaterials,
    Failed
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FARPGCraftingStateChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FARPGCraftingResultEvent, EARPGCraftingResult, Result, UARPGRecipeDefinition*, Recipe, int32, RemainingCount, FText, Message);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FARPGRepairResultEvent, EARPGCraftingResult, Result, FGuid, ItemInstanceId, FText, Message);

/**
 * Server-authoritative personal crafting + equipment repair component inherited by every ARPGCharacter.
 * Player recipes reuse ARPGRecipeDefinition. RequiredStationTag recipes remain station-only.
 * No permanent tick: crafting completion is timer driven; clients derive progress from synchronized server time.
 */
UCLASS(ClassGroup=(ARPG), BlueprintType, Blueprintable, meta=(BlueprintSpawnableComponent))
class AKUMASRPGFRAMEWORK_API UARPGCraftingComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UARPGCraftingComponent();

    /** Recipes shown/accepted by the player crafting UI. Data Assets remain the source of recipe truth. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Crafting|Recipes", meta=(DisplayName="Player Recipes"))
    TArray<TObjectPtr<UARPGRecipeDefinition>> PlayerRecipes;

    /** Off by default so a client cannot request an arbitrary project recipe that was not exposed to this character. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Crafting|Security", meta=(DisplayName="Allow Unlisted Recipe Requests"))
    bool bAllowUnlistedRecipeRequests = false;

    /** Caps one request even if a Recipe allows a larger authored batch. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Crafting|Queue", meta=(ClampMin="1", ClampMax="999"))
    int32 MaxCraftRequestCount = 99;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, ReplicatedUsing=OnRep_CraftingState, Category="Crafting|Runtime")
    TObjectPtr<UARPGRecipeDefinition> ActiveRecipe = nullptr;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, ReplicatedUsing=OnRep_CraftingState, Category="Crafting|Runtime")
    int32 ActiveRemainingCount = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, ReplicatedUsing=OnRep_CraftingState, Category="Crafting|Runtime")
    float ActiveCraftStartServerTime = 0.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, ReplicatedUsing=OnRep_CraftingState, Category="Crafting|Runtime")
    float ActiveCraftDuration = 0.f;

    UPROPERTY(BlueprintAssignable, Category="Crafting|Events") FARPGCraftingStateChanged OnCraftingStateChanged;
    UPROPERTY(BlueprintAssignable, Category="Crafting|Events") FARPGCraftingResultEvent OnCraftingResult;
    UPROPERTY(BlueprintAssignable, Category="Crafting|Events") FARPGRepairResultEvent OnRepairResult;

    UFUNCTION(BlueprintCallable, Category="ARPG|Crafting") bool CraftRecipe(UARPGRecipeDefinition* Recipe, int32 Count = 1);
    UFUNCTION(BlueprintCallable, Category="ARPG|Crafting") bool CancelCrafting();
    UFUNCTION(BlueprintPure, Category="ARPG|Crafting") bool IsCrafting() const { return ActiveRecipe != nullptr && ActiveRemainingCount > 0; }
    UFUNCTION(BlueprintPure, Category="ARPG|Crafting") float GetCraftProgress01() const;
    UFUNCTION(BlueprintPure, Category="ARPG|Crafting") float GetCraftSecondsRemaining() const;
    UFUNCTION(BlueprintPure, Category="ARPG|Crafting") int32 GetMaxCraftableCount(const UARPGRecipeDefinition* Recipe) const;
    UFUNCTION(BlueprintPure, Category="ARPG|Crafting") bool CanCraftRecipe(const UARPGRecipeDefinition* Recipe, int32 Count, FText& OutFailureReason) const;
    UFUNCTION(BlueprintPure, Category="ARPG|Crafting") bool IsRecipeAvailableToPlayer(const UARPGRecipeDefinition* Recipe) const;

    UFUNCTION(BlueprintCallable, Category="ARPG|Crafting|Repair") bool RepairItem(FGuid ItemInstanceId);
    UFUNCTION(BlueprintPure, Category="ARPG|Crafting|Repair") bool CanRepairItem(FGuid ItemInstanceId, FText& OutFailureReason) const;
    UFUNCTION(BlueprintPure, Category="ARPG|Crafting|Repair") bool GetRepairCost(FGuid ItemInstanceId, TArray<FARPGItemAmount>& OutRepairCost) const;

    /** Persistence bridge used by ARPGSaveSubsystem. Active craft inputs are already committed in Inventory. */
    FARPGCraftQueueEntry MakeCraftingSaveState() const;
    /** Clears any live timer/state and resumes the saved committed craft without consuming its inputs again. */
    void RestoreCraftingSaveState(const FARPGCraftQueueEntry& SavedState);

    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
    UFUNCTION(Server, Reliable) void ServerCraftRecipe(UARPGRecipeDefinition* Recipe, int32 Count);
    UFUNCTION(Server, Reliable) void ServerCancelCrafting();
    UFUNCTION(Server, Reliable) void ServerRepairItem(FGuid ItemInstanceId);
    UFUNCTION(Client, Reliable) void ClientCraftingResult(EARPGCraftingResult Result, UARPGRecipeDefinition* Recipe, int32 RemainingCount, const FText& Message);
    UFUNCTION(Client, Reliable) void ClientRepairResult(EARPGCraftingResult Result, FGuid ItemInstanceId, const FText& Message);
    UFUNCTION() void OnRep_CraftingState();

private:
    FTimerHandle CraftCompletionTimer;
    bool bCurrentInputsCommitted = false;

    UARPGInventoryComponent* GetInventory() const;
    FName ResolveAmountItemId(const FARPGItemAmount& Amount) const;
    UARPGItemDefinition* ResolveAmountItemDefinition(const FARPGItemAmount& Amount) const;
    bool AggregateAmounts(const TArray<FARPGItemAmount>& Amounts, int32 Multiplier, TArray<FARPGItemAmount>& OutAggregated) const;
    int32 GetAvailableIngredientCount(FName ItemId) const;
    bool HasInputs(const UARPGRecipeDefinition* Recipe, int32 Count) const;
    bool ConsumeInputs(const TArray<FARPGItemAmount>& Inputs, int32 Multiplier, TArray<FARPGItemAmount>* OutConsumed = nullptr);
    void RefundInputs(const TArray<FARPGItemAmount>& Inputs, int32 Multiplier);
    bool CanFitOutputs(const UARPGRecipeDefinition* Recipe, int32 Count) const;
    bool CanFitCommittedOutputs(const UARPGRecipeDefinition* Recipe) const;
    bool GrantOutputs(const UARPGRecipeDefinition* Recipe);
    bool MeetsSkillRequirement(const UARPGRecipeDefinition* Recipe) const;
    float GetServerTimeSeconds() const;

    EARPGCraftingResult ValidateCraftRequest(const UARPGRecipeDefinition* Recipe, int32 Count, FText& OutFailureReason) const;
    bool StartCraftingAuthority(UARPGRecipeDefinition* Recipe, int32 Count);
    void BeginNextCraftAuthority();
    void CompleteCurrentCraftAuthority();
    void FinishCraftingAuthority(EARPGCraftingResult Result, const FText& Message, bool bRefundCommittedInputs);
    void AwardRecipeRewards(UARPGRecipeDefinition* Recipe);
    void BroadcastCraftingState();

    bool RepairItemAuthority(FGuid ItemInstanceId);
    EARPGCraftingResult ValidateRepair(FGuid ItemInstanceId, TArray<FARPGItemAmount>& OutCost, FText& OutFailureReason) const;
};
