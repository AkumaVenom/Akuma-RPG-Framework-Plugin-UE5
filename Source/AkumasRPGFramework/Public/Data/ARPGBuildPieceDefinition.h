#pragma once
#include "CoreMinimal.h"
#include "Data/ARPGDefinitionBase.h"
#include "Data/ARPGRecipeDefinition.h"
#include "ARPGBuildPieceDefinition.generated.h"

class AARPGBuildPieceActor;

UCLASS(BlueprintType)
class AKUMASRPGFRAMEWORK_API UARPGBuildPieceDefinition : public UARPGDefinitionBase
{
    GENERATED_BODY()
public:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Building") TSoftClassPtr<AARPGBuildPieceActor> ActorClass;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Building") TArray<FARPGItemAmount> BuildCost;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Building") FGameplayTag PieceType;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Building") FGameplayTag MaterialTier;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Building") bool bSnapPlacement = true;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Building") bool bRequiresSupport = true;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Building", meta=(ClampMin="1.0")) float MaxHealth = 500.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Building", meta=(ClampMin="1.0")) float SnapSize = 100.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Building") FVector PlacementBounds = FVector(50.f, 50.f, 50.f);
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Building", meta=(ClampMin="0.0")) float SupportTraceDepth = 150.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Faction") FName RequiredBuilderFactionId = NAME_None;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Faction") int32 MinimumBuilderReputation = TNumericLimits<int32>::Lowest();
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Faction") bool bInheritBuilderFaction = true;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Faction") bool bSameFactionCanUse = true;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Faction") bool bAlliesCanUse = true;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Faction") bool bNeutralCanUse = false;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Faction") bool bHostilesCanUse = false;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Faction") bool bFactionMembersCanModify = false;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Faction") bool bHostilesCanDamage = true;
};
