#pragma once

#include "CoreMinimal.h"
#include "Actors/ARPGAICharacter.h"
#include "ARPGSettlementVillagerCharacter.generated.h"

class UARPGFactionOwnershipComponent;
class UARPGSettlementResidentComponent;
class AARPGSettlementHubActor;
class AARPGBuildBedActor;

/** Ready native resident pawn. Projects may subclass this in Blueprint for custom meshes/AnimBP/stats. */
UCLASS(BlueprintType, Blueprintable)
class AKUMASRPGFRAMEWORK_API AARPGSettlementVillagerCharacter : public AARPGAICharacter
{
    GENERATED_BODY()
public:
    AARPGSettlementVillagerCharacter();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="ARPG|Settlement") TObjectPtr<UARPGSettlementResidentComponent> SettlementResident;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="ARPG|Settlement") TObjectPtr<UARPGFactionOwnershipComponent> SettlementOwnership;

    UFUNCTION(BlueprintCallable, Category="ARPG|Settlement", meta=(BlueprintAuthorityOnly)) bool InitializeAsSettlementVillager(AARPGSettlementHubActor* Hub, AARPGBuildBedActor* Bed, FGuid ResidentId = FGuid());
};
