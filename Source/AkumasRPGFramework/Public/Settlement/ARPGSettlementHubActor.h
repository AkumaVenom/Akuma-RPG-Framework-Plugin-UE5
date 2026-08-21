#pragma once

#include "CoreMinimal.h"
#include "Crafting/ARPGStorageActor.h"
#include "ARPGTypes.h"
#include "ARPGSettlementHubActor.generated.h"

class UARPGSettlementDefinition;
class AARPGBuildBedActor;
class AARPGSettlementVillagerCharacter;
class UARPGSettlementResidentComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FARPGSettlementSummaryChanged, FARPGSettlementSummary, NewSummary);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FARPGSettlementResidentEvent, AARPGSettlementVillagerCharacter*, Resident, bool, bAdded);

/**
 * Palbox-style buildable settlement core. This actor is the explicit opt-in boundary for settlement
 * simulation: without a completed Hub, Beds remain ordinary build pieces and no residents are spawned.
 */
UCLASS(BlueprintType, Blueprintable)
class AKUMASRPGFRAMEWORK_API AARPGSettlementHubActor : public AARPGStorageActor
{
    GENERATED_BODY()
public:
    AARPGSettlementHubActor();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, ReplicatedUsing=OnRep_SettlementSummary, Category="Settlement")
    FARPGSettlementSummary SettlementSummary;
    UPROPERTY(BlueprintAssignable, Category="Settlement|Events") FARPGSettlementSummaryChanged OnSettlementSummaryChanged;
    UPROPERTY(BlueprintAssignable, Category="Settlement|Events") FARPGSettlementResidentEvent OnSettlementResidentChanged;

    UFUNCTION(BlueprintPure, Category="ARPG|Settlement") UARPGSettlementDefinition* GetSettlementDefinition() const;
    UFUNCTION(BlueprintPure, Category="ARPG|Settlement") FGuid GetSettlementId() const { return BuildingId; }
    UFUNCTION(BlueprintPure, Category="ARPG|Settlement") float GetSettlementRadius() const;
    UFUNCTION(BlueprintPure, Category="ARPG|Settlement") float GetSettlementHUDRadius() const;
    UFUNCTION(BlueprintPure, Category="ARPG|Settlement") FARPGSettlementSummary GetSettlementSummary() const { return SettlementSummary; }
    UFUNCTION(BlueprintPure, Category="ARPG|Settlement") bool IsSettlementOperational() const { return SettlementSummary.bSettlementOperational; }
    UFUNCTION(BlueprintPure, Category="ARPG|Settlement") bool IsLocationInsideSettlement(const FVector& WorldLocation) const;
    UFUNCTION(BlueprintPure, Category="ARPG|Settlement") bool CanManageBuilding(const AARPGBuildPieceActor* Building) const;
    UFUNCTION(BlueprintCallable, Category="ARPG|Settlement|Housing") bool ValidateHomeForBed(AARPGBuildBedActor* Bed, FARPGSettlementHomeValidation& OutValidation) const;
    /** Resolve a NavMesh-backed point on the validated home's interior walkable story for resident spawn/return-home logic. */
    UFUNCTION(BlueprintCallable, Category="ARPG|Settlement|Housing") bool ResolveResidentHomeAnchor(AARPGBuildBedActor* Bed, FVector& OutWorldLocation) const;
    UFUNCTION(BlueprintCallable, Category="ARPG|Settlement|Housing") void GetManagedBeds(TArray<AARPGBuildBedActor*>& OutBeds) const;
    UFUNCTION(BlueprintCallable, Category="ARPG|Settlement|Residents") void GetSettlementResidents(TArray<AARPGSettlementVillagerCharacter*>& OutResidents) const;
    UFUNCTION(BlueprintPure, Category="ARPG|Settlement|Residents") AARPGBuildBedActor* FindBedByBuildingId(FGuid BedBuildingId) const;
    UFUNCTION(BlueprintPure, Category="ARPG|Settlement|Residents") AARPGSettlementVillagerCharacter* FindResidentById(FGuid ResidentId) const;
    UFUNCTION(BlueprintPure, Category="ARPG|Settlement|Woodcutting") bool CanResidentStartWoodcutting(const UARPGSettlementResidentComponent* Resident) const;

    UFUNCTION(BlueprintCallable, Category="ARPG|Settlement", meta=(BlueprintAuthorityOnly)) void RefreshSettlementNow();
    UFUNCTION(BlueprintCallable, Category="ARPG|Settlement|Residents", meta=(BlueprintAuthorityOnly)) bool RegisterLoadedResident(AARPGSettlementVillagerCharacter* Resident);
    UFUNCTION(BlueprintCallable, Category="ARPG|Settlement|Residents", meta=(BlueprintAuthorityOnly)) void NotifyResidentStateChanged(AARPGSettlementVillagerCharacter* Resident);

    virtual void InitializeBuilding(UARPGBuildPieceDefinition* InDefinition, AActor* Builder) override;
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
protected:
    UFUNCTION() void OnRep_SettlementSummary();
    UFUNCTION() void HandleConstructionCompleted();
private:
    TArray<TWeakObjectPtr<AARPGSettlementVillagerCharacter>> Residents;
    FTimerHandle SettlementRefreshTimer;
    float RecruitmentUnlockServerTime = 0.f;
    float NextRecruitmentServerTime = 0.f;

    void StartSettlementRuntime();
    void StopSettlementRuntime();
    void DismissResidentsAuthority();
    void RefreshSettlementAuthority();
    void CleanupResidentRegistry();
    void ReconcileBedsAndResidents(const TArray<AARPGBuildBedActor*>& ManagedBeds);
    bool RecruitResidentForBed(AARPGBuildBedActor* Bed);
    void UpdateReplicatedSummary(const TArray<AARPGBuildBedActor*>& ManagedBeds);
    FString MakeResidentName(int32 Ordinal) const;
};
