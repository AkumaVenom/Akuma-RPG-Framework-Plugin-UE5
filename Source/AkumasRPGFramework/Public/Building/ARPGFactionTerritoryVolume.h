#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Volume.h"
#include "ARPGFactionTerritoryVolume.generated.h"

UCLASS(BlueprintType, Blueprintable)
class AKUMASRPGFRAMEWORK_API AARPGFactionTerritoryVolume : public AVolume
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Territory") FName OwnerFactionId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Territory") int32 MinimumReputationToBuild = TNumericLimits<int32>::Lowest();
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Territory") bool bAllowOwnerFaction = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Territory") bool bAllowAllies = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Territory") bool bAllowNeutral = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Territory") bool bAllowHostile = false;
    UFUNCTION(BlueprintPure, Category="ARPG|Territory") bool CanActorBuildHere(const AActor* Actor) const;
};
