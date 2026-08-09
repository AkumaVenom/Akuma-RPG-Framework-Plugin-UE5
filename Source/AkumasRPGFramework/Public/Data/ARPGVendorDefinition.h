#pragma once

#include "CoreMinimal.h"
#include "ARPGTypes.h"
#include "Data/ARPGDefinitionBase.h"
#include "ARPGVendorDefinition.generated.h"

USTRUCT(BlueprintType)
struct AKUMASRPGFRAMEWORK_API FARPGVendorItemDefinition
{
    GENERATED_BODY()
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) FName ItemId = NAME_None;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) int32 InitialStock = -1;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) int64 UnitPrice = 0;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) FName CurrencyId = TEXT("Gold");
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) float RestockSeconds = 0.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) int32 RequiredLevel = 1;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) FName RequiredFactionId = NAME_None;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) int32 RequiredReputation = TNumericLimits<int32>::Lowest();
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) FName RequiredQuestId = NAME_None;
};

UCLASS(BlueprintType)
class AKUMASRPGFRAMEWORK_API UARPGVendorDefinition : public UARPGDefinitionBase
{
    GENERATED_BODY()
public:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Vendor") TArray<FARPGVendorItemDefinition> Stock;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Vendor", meta=(ClampMin="0.0")) float BuyPriceMultiplier = 1.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Vendor", meta=(ClampMin="0.0")) float SellPriceMultiplier = 0.25f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Vendor") bool bSupportsBuyback = true;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Vendor", meta=(ClampMin="1", ClampMax="50")) int32 BuybackLimit = 12;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Vendor") bool bCanRepair = false;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Vendor") bool bCanTrain = false;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Vendor", meta=(ClampMin="0.0", ClampMax="0.25")) float ReputationDiscountPerPositiveTier = 0.025f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Vendor", meta=(ClampMin="0.0", ClampMax="0.75")) float MaxReputationDiscount = 0.20f;
};
