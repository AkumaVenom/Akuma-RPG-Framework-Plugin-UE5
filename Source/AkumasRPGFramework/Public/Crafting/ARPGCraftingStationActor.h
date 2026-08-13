#pragma once
#include "CoreMinimal.h"
#include "Crafting/ARPGStorageActor.h"
#include "ARPGTypes.h"
#include "ARPGCraftingStationActor.generated.h"

class UARPGCraftingStationDefinition;
class UARPGRecipeDefinition;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FARPGCraftQueueChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FARPGCraftCompleted, FName, RecipeId, int32, RemainingCount);

UCLASS(BlueprintType, Blueprintable)
class AKUMASRPGFRAMEWORK_API AARPGCraftingStationActor : public AARPGStorageActor
{
    GENERATED_BODY()
public:
    AARPGCraftingStationActor();

    UPROPERTY(EditAnywhere, BlueprintReadOnly, ReplicatedUsing=OnRep_StationDefinition, Category="Crafting") TObjectPtr<UARPGCraftingStationDefinition> StationDefinition;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Crafting") TObjectPtr<UARPGInventoryComponent> OutputInventory;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, ReplicatedUsing=OnRep_CraftQueue, SaveGame, Category="Crafting") TArray<FARPGCraftQueueEntry> CraftQueue;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category="Crafting") EARPGCraftingStationState StationState = EARPGCraftingStationState::Idle;
    UPROPERTY(BlueprintAssignable) FARPGCraftQueueChanged OnCraftQueueChanged;
    UPROPERTY(BlueprintAssignable) FARPGCraftCompleted OnCraftCompleted;

    UFUNCTION(BlueprintCallable, Category="ARPG|Crafting", meta=(BlueprintAuthorityOnly)) bool QueueRecipe(AActor* Crafter, UARPGRecipeDefinition* Recipe, int32 Count=1);
    /** Applies station data at runtime; used by data-driven build pieces so a furnace needs no actor Blueprint. */
    UFUNCTION(BlueprintCallable, Category="ARPG|Crafting", meta=(BlueprintAuthorityOnly)) void ApplyStationDefinition(UARPGCraftingStationDefinition* InDefinition);
    UFUNCTION(BlueprintPure, Category="ARPG|Crafting") bool CanQueueRecipe(AActor* Crafter, const UARPGRecipeDefinition* Recipe) const { return CanUseRecipe(Crafter, Recipe); }
    UFUNCTION(BlueprintCallable, Category="ARPG|Crafting", meta=(BlueprintAuthorityOnly)) bool CancelQueueEntry(FGuid QueueId, bool bRefundRemaining=true);
    UFUNCTION(BlueprintPure, Category="ARPG|Crafting") float GetCurrentCraftProgress01() const;
    UFUNCTION(BlueprintCallable, Category="ARPG|Crafting", meta=(BlueprintAuthorityOnly)) void ProcessOfflineElapsed();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
protected:
    UFUNCTION() void OnRep_CraftQueue();
    UFUNCTION() void OnRep_StationDefinition();
    void ApplyStationConfiguration();

    bool QueueRecipeAuthority(AActor* Crafter, UARPGRecipeDefinition* Recipe, int32 Count);
    bool CanUseRecipe(AActor* Crafter, const UARPGRecipeDefinition* Recipe) const;
    bool ConsumeRecipeInputs(AActor* Crafter, const UARPGRecipeDefinition* Recipe, int32 Count);
    void RefundRecipeInputs(AActor* Crafter, const UARPGRecipeDefinition* Recipe, int32 Count);
    UARPGInventoryComponent* ResolveInputInventory(AActor* Crafter) const;
    UARPGInventoryComponent* ResolveFuelInventory(AActor* Crafter) const;
    bool HasFuelForCraft(AActor* Crafter, const UARPGRecipeDefinition* Recipe) const;
    bool ConsumeFuelForCraft(AActor* Crafter, const UARPGRecipeDefinition* Recipe);
    AActor* FindCrafter(FGuid CharacterId) const;
    UARPGRecipeDefinition* ResolveRecipe(FName RecipeId) const;
    bool CompleteOneCraft(FARPGCraftQueueEntry& Entry);
    float GetRecipeSeconds(FName RecipeId) const;
};
