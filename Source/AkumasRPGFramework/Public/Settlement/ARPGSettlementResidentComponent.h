#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ARPGTypes.h"
#include "ARPGSettlementResidentComponent.generated.h"

class AARPGBuildBedActor;
class AARPGSettlementHubActor;
class AARPGSettlementVillagerCharacter;
class AARPGTree;
class UAnimMontage;
class UARPGItemDefinition;
class AARPGEquipmentVisualActor;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FARPGSettlementResidentStateChanged, EARPGSettlementResidentState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FARPGSettlementResidentBedChanged, AARPGBuildBedActor*, NewBed);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FARPGSettlementResidentWorkToolChanged, bool, bEquipped, AARPGEquipmentVisualActor*, VisualActor);

/** Native autonomous resident logic shared by settlement villager subclasses. */
UCLASS(ClassGroup=(ARPG), meta=(BlueprintSpawnableComponent))
class AKUMASRPGFRAMEWORK_API UARPGSettlementResidentComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UARPGSettlementResidentComponent();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, SaveGame, Category="Settlement|Resident") FGuid ResidentId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, ReplicatedUsing=OnRep_Hub, Category="Settlement|Resident") TObjectPtr<AARPGSettlementHubActor> SettlementHub = nullptr;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, ReplicatedUsing=OnRep_Bed, Category="Settlement|Resident") TObjectPtr<AARPGBuildBedActor> AssignedBed = nullptr;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, ReplicatedUsing=OnRep_State, Category="Settlement|Resident") EARPGSettlementResidentState ResidentState = EARPGSettlementResidentState::Homeless;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, ReplicatedUsing=OnRep_CurrentWorkTree, Category="Settlement|Resident") TObjectPtr<AARPGTree> CurrentWorkTree = nullptr;
    /** Local presentation-only tool actor. The authoritative Inventory is never mutated for this contextual visual. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="Settlement|Woodcutting|Tool Presentation") TObjectPtr<AARPGEquipmentVisualActor> ActiveWoodcuttingToolVisual = nullptr;

    UPROPERTY(BlueprintAssignable, Category="Settlement|Resident") FARPGSettlementResidentStateChanged OnResidentStateChanged;
    UPROPERTY(BlueprintAssignable, Category="Settlement|Resident") FARPGSettlementResidentBedChanged OnResidentBedChanged;
    UPROPERTY(BlueprintAssignable, Category="Settlement|Woodcutting|Tool Presentation") FARPGSettlementResidentWorkToolChanged OnWoodcuttingToolVisualChanged;

    UFUNCTION(BlueprintCallable, Category="ARPG|Settlement|Resident", meta=(BlueprintAuthorityOnly)) bool InitializeSettlementResident(AARPGSettlementHubActor* Hub, AARPGBuildBedActor* Bed, FGuid InResidentId = FGuid());
    UFUNCTION(BlueprintCallable, Category="ARPG|Settlement|Resident", meta=(BlueprintAuthorityOnly)) bool AssignBed(AARPGBuildBedActor* NewBed);
    UFUNCTION(BlueprintCallable, Category="ARPG|Settlement|Resident", meta=(BlueprintAuthorityOnly)) void ClearBed();
    UFUNCTION(BlueprintCallable, Category="ARPG|Settlement|Resident", meta=(BlueprintAuthorityOnly)) void ForceChooseNewActivity();
    UFUNCTION(BlueprintCallable, Category="ARPG|Settlement|Resident", meta=(BlueprintAuthorityOnly)) void ReturnHome();
    UFUNCTION(BlueprintPure, Category="ARPG|Settlement|Resident") bool HasValidHome() const;
    UFUNCTION(BlueprintPure, Category="ARPG|Settlement|Resident") bool IsWorking() const { return ResidentState == EARPGSettlementResidentState::GoingToWork || ResidentState == EARPGSettlementResidentState::Woodcutting; }
    UFUNCTION(BlueprintPure, Category="ARPG|Settlement|Woodcutting") bool CanBypassTreeRequirements(const AARPGTree* Tree) const;
    UFUNCTION(BlueprintPure, Category="ARPG|Settlement|Woodcutting|Tool Presentation") bool IsWoodcuttingToolVisualActive() const;
    /** Re-evaluate the local contextual work-tool visual from replicated resident state. Safe for custom cosmetic refreshes. */
    UFUNCTION(BlueprintCallable, Category="ARPG|Settlement|Woodcutting|Tool Presentation") void RefreshWoodcuttingToolVisual();
    UFUNCTION(BlueprintCallable, Category="ARPG|Settlement|Resident", meta=(BlueprintAuthorityOnly)) void RestoreResidentLinks(AARPGSettlementHubActor* Hub, AARPGBuildBedActor* Bed, FGuid InResidentId);

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
protected:
    UFUNCTION() void OnRep_Hub();
    UFUNCTION() void OnRep_Bed();
    UFUNCTION() void OnRep_State();
    UFUNCTION() void OnRep_CurrentWorkTree();
    UFUNCTION(NetMulticast, Unreliable) void MulticastPlayWoodcuttingMontage(UAnimMontage* Montage);
private:
    FTimerHandle ActivityTimer;
    FTimerHandle ChopTimer;
    FTimerHandle WorkMoveProofTimer;
    float NextTreeSearchTime = 0.f;
    int32 WorkMoveProofChecks = 0;
    FVector WorkMoveProofStartLocation = FVector::ZeroVector;
    UPROPERTY(Transient) TObjectPtr<UARPGItemDefinition> ActiveWoodcuttingToolDefinition = nullptr;

    void StartRuntime();
    void StopRuntime();
    void ThinkAuthority();
    void SetResidentState(EARPGSettlementResidentState NewState);
    bool TryBeginWoodcutting();
    AARPGTree* FindBestWorkTree() const;
    bool MoveToLocation(const FVector& Location, float AcceptanceRadius);
    bool MoveToActor(AActor* Actor, float AcceptanceRadius);
    void StartWorkMovementProof();
    void VerifyWorkMovement();
    void CancelWorkMovementProof();
    void BeginChoppingTree(AARPGTree* Tree);
    void PerformChopAuthority();
    void StopWoodcutting(bool bReturnToRoam=true);
    void DepositTreeRewardsToHub(AARPGTree* Tree);
    bool ShouldDisplayWoodcuttingTool() const;
    UARPGItemDefinition* ResolveWoodcuttingToolDefinition() const;
    void SetWoodcuttingToolVisualActive(bool bActive, bool bAllowTransitionPresentation);
    FVector ResolveHomeLocation() const;
    void RepairInvalidHomeStoryPosition();
};
