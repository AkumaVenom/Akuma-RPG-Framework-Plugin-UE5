#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ARPGBuildPieceActor.generated.h"

class UARPGBuildPieceDefinition;
class UARPGFactionOwnershipComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FARPGBuildingHealthChanged, float, NewHealth, float, Delta);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FARPGBuildingDestroyed);

UCLASS(BlueprintType, Blueprintable)
class AKUMASRPGFRAMEWORK_API AARPGBuildPieceActor : public AActor
{
    GENERATED_BODY()
public:
    AARPGBuildPieceActor();

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, SaveGame, Category="Building") FGuid BuildingId;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, ReplicatedUsing=OnRep_Definition, Category="Building") TObjectPtr<UARPGBuildPieceDefinition> Definition;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Building") TObjectPtr<UARPGFactionOwnershipComponent> Ownership;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, ReplicatedUsing=OnRep_Health, SaveGame, Category="Building") float Health = 100.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, SaveGame, Category="Building") int32 UpgradeLevel = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, SaveGame, Category="Building") bool bRuntimePlaced = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Building") bool bAllowDemolish = true;

    UPROPERTY(BlueprintAssignable) FARPGBuildingHealthChanged OnBuildingHealthChanged;
    UPROPERTY(BlueprintAssignable) FARPGBuildingDestroyed OnBuildingDestroyed;

    UFUNCTION(BlueprintCallable, Category="ARPG|Building", meta=(BlueprintAuthorityOnly)) void InitializeBuilding(UARPGBuildPieceDefinition* InDefinition, AActor* Builder);
    UFUNCTION(BlueprintCallable, Category="ARPG|Building", meta=(BlueprintAuthorityOnly)) bool ApplyBuildingDamage(float Amount, AActor* DamageCauser=nullptr);
    UFUNCTION(BlueprintCallable, Category="ARPG|Building", meta=(BlueprintAuthorityOnly)) bool RepairBuilding(float Amount, AActor* Repairer=nullptr);
    UFUNCTION(BlueprintCallable, Category="ARPG|Building", meta=(BlueprintAuthorityOnly)) bool Demolish(AActor* Requester=nullptr);
    UFUNCTION(BlueprintPure, Category="ARPG|Building") float GetMaxBuildingHealth() const;
    UFUNCTION(BlueprintPure, Category="ARPG|Building") bool CanActorUse(AActor* Actor) const;
    UFUNCTION(BlueprintPure, Category="ARPG|Building") bool CanActorModify(AActor* Actor) const;

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
protected:
    UFUNCTION() void OnRep_Definition();
    UFUNCTION() void OnRep_Health(float OldHealth);
};
