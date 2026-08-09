#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ARPGTypes.h"
#include "ARPGCurrencyComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FARPGOnCurrencyChanged, FName, CurrencyId, int64, NewAmount);

UCLASS(ClassGroup=(ARPG), meta=(BlueprintSpawnableComponent))
class AKUMASRPGFRAMEWORK_API UARPGCurrencyComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UARPGCurrencyComponent();
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, SaveGame) TArray<FARPGCurrencyBalance> Balances;
    UPROPERTY(BlueprintAssignable) FARPGOnCurrencyChanged OnCurrencyChanged;

    UFUNCTION(BlueprintPure, Category="ARPG|Currency") int64 GetCurrency(FName CurrencyId) const;
    UFUNCTION(BlueprintCallable, Category="ARPG|Currency", meta=(BlueprintAuthorityOnly)) void AddCurrency(FName CurrencyId, int64 Amount);
    UFUNCTION(BlueprintCallable, Category="ARPG|Currency", meta=(BlueprintAuthorityOnly)) bool SpendCurrency(FName CurrencyId, int64 Amount);
    UFUNCTION(BlueprintCallable, Category="ARPG|Currency", meta=(BlueprintAuthorityOnly)) void ReplaceBalances(const TArray<FARPGCurrencyBalance>& NewBalances);
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
