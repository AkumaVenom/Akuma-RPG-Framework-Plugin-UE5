#pragma once

#include "CoreMinimal.h"
#include "Building/ARPGBuildPieceActor.h"
#include "ARPGTypes.h"
#include "ARPGBuildBedActor.generated.h"

class AARPGSettlementHubActor;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FARPGBedRoleChanged, EARPGBedRole, NewRole);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FARPGBedResidentAssignmentChanged, FGuid, ResidentId);

/** Native buildable Bed used by the settlement housing system. */
UCLASS(BlueprintType, Blueprintable)
class AKUMASRPGFRAMEWORK_API AARPGBuildBedActor : public AARPGBuildPieceActor
{
    GENERATED_BODY()
public:
    AARPGBuildBedActor();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, ReplicatedUsing=OnRep_BedRole, SaveGame, Category="Settlement|Bed")
    EARPGBedRole BedRole = EARPGBedRole::Unassigned;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, ReplicatedUsing=OnRep_Assignment, SaveGame, Category="Settlement|Bed")
    FGuid AssignedResidentId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, SaveGame, Category="Settlement|Bed")
    FGuid PlayerBedOwnerCharacterId;

    UPROPERTY(BlueprintAssignable, Category="Settlement|Bed") FARPGBedRoleChanged OnBedRoleChanged;
    UPROPERTY(BlueprintAssignable, Category="Settlement|Bed") FARPGBedResidentAssignmentChanged OnResidentAssignmentChanged;

    UFUNCTION(BlueprintPure, Category="ARPG|Settlement|Bed") bool IsVillagerBed() const { return BedRole == EARPGBedRole::Villager; }
    UFUNCTION(BlueprintPure, Category="ARPG|Settlement|Bed") bool IsPlayerBed() const { return BedRole == EARPGBedRole::Player; }
    UFUNCTION(BlueprintPure, Category="ARPG|Settlement|Bed") bool HasAssignedResident() const { return AssignedResidentId.IsValid(); }
    UFUNCTION(BlueprintPure, Category="ARPG|Settlement|Bed") AARPGSettlementHubActor* FindManagingSettlementHub() const;
    UFUNCTION(BlueprintCallable, Category="ARPG|Settlement|Bed") bool GetCurrentHomeValidation(FARPGSettlementHomeValidation& OutValidation) const;

    UFUNCTION(BlueprintCallable, Category="ARPG|Settlement|Bed", meta=(BlueprintAuthorityOnly)) bool SetBedRole(EARPGBedRole NewRole, AActor* Requester=nullptr);
    UFUNCTION(BlueprintCallable, Category="ARPG|Settlement|Bed", meta=(BlueprintAuthorityOnly)) void AssignResident(FGuid ResidentId);
    UFUNCTION(BlueprintCallable, Category="ARPG|Settlement|Bed", meta=(BlueprintAuthorityOnly)) void ClearResidentAssignment(FGuid ExpectedResidentId = FGuid());
    UFUNCTION(BlueprintCallable, Category="ARPG|Settlement|Bed", meta=(BlueprintAuthorityOnly)) void RestoreBedState(EARPGBedRole SavedRole, FGuid SavedResidentId, FGuid SavedPlayerOwnerCharacterId);

    virtual void InitializeBuilding(UARPGBuildPieceDefinition* InDefinition, AActor* Builder) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
protected:
    UFUNCTION() void OnRep_BedRole();
    UFUNCTION() void OnRep_Assignment();
};
