#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ARPGTypes.h"
#include "ARPGVendorComponent.generated.h"

class UARPGVendorDefinition;


USTRUCT(BlueprintType)
struct AKUMASRPGFRAMEWORK_API FARPGVendorBuybackEntry
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) FGuid BuybackId;
    UPROPERTY(BlueprintReadOnly) FGuid CharacterId;
    UPROPERTY(BlueprintReadOnly) FName ItemId = NAME_None;
    UPROPERTY(BlueprintReadOnly) int32 Quantity = 0;
    UPROPERTY(BlueprintReadOnly) int64 UnitPrice = 0;
    UPROPERTY(BlueprintReadOnly) FName CurrencyId = TEXT("Gold");
    UPROPERTY(BlueprintReadOnly) FDateTime SoldAtUtc;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FARPGOnVendorStockChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FARPGOnVendorTransaction, AActor*, Customer, FName, ItemId, int32, Quantity);

UCLASS(ClassGroup=(ARPG), meta=(BlueprintSpawnableComponent))
class AKUMASRPGFRAMEWORK_API UARPGVendorComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UARPGVendorComponent();

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Vendor") TObjectPtr<UARPGVendorDefinition> VendorDefinition;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, ReplicatedUsing=OnRep_Stock, SaveGame, Category="Vendor") TArray<FARPGVendorStockEntry> RuntimeStock;
    UPROPERTY(BlueprintAssignable) FARPGOnVendorStockChanged OnStockChanged;
    UPROPERTY(BlueprintAssignable) FARPGOnVendorTransaction OnTransaction;

    UFUNCTION(BlueprintCallable, Category="ARPG|Vendor", meta=(BlueprintAuthorityOnly)) void InitializeFromDefinition();
    UFUNCTION(BlueprintCallable, Category="ARPG|Vendor", meta=(BlueprintAuthorityOnly)) bool Purchase(AActor* Customer, FName ItemId, int32 Quantity=1);
    UFUNCTION(BlueprintCallable, Category="ARPG|Vendor", meta=(BlueprintAuthorityOnly)) bool SellToVendor(AActor* Customer, FName ItemId, int32 Quantity=1);
    UFUNCTION(BlueprintCallable, Category="ARPG|Vendor", meta=(BlueprintAuthorityOnly)) bool BuybackFromVendor(AActor* Customer, FGuid BuybackId);
    UFUNCTION(BlueprintPure, Category="ARPG|Vendor") TArray<FARPGVendorBuybackEntry> GetBuybackItems(AActor* Customer) const;
    UFUNCTION(BlueprintPure, Category="ARPG|Vendor") int32 GetAvailableStock(FName ItemId) const;
    UFUNCTION(BlueprintPure, Category="ARPG|Vendor") int64 GetPurchasePriceForCustomer(AActor* Customer, FName ItemId, int32 Quantity=1) const;
    UFUNCTION(BlueprintPure, Category="ARPG|Vendor") bool MeetsPurchaseRequirements(AActor* Customer, FName ItemId) const;
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="ARPG|Vendor", meta=(BlueprintAuthorityOnly)) bool PerformVendorService(AActor* Customer, FGameplayTag ServiceTag);

    virtual void BeginPlay() override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
protected:
    UFUNCTION() void OnRep_Stock();
    void RefreshRestock(FARPGVendorStockEntry& Entry);
    const struct FARPGVendorItemDefinition* FindStockDefinition(FName ItemId) const;
    float GetReputationPriceMultiplier(AActor* Customer, const struct FARPGVendorItemDefinition* StockDef) const;
    FGuid ResolveCustomerId(AActor* Customer) const;
    TArray<FARPGVendorBuybackEntry> BuybackHistory;
};
