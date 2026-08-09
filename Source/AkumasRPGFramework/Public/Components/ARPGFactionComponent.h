#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ARPGTypes.h"
#include "ARPGFactionComponent.generated.h"

class UARPGFactionDefinition;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FARPGOnReputationChanged, FName, FactionId, int32, NewReputation);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FARPGOnPrimaryFactionChanged, FName, FactionId);

UCLASS(ClassGroup=(ARPG), meta=(BlueprintSpawnableComponent))
class AKUMASRPGFRAMEWORK_API UARPGFactionComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UARPGFactionComponent();

    UPROPERTY(EditAnywhere, BlueprintReadOnly, ReplicatedUsing=OnRep_PrimaryFaction, Category="Faction") TObjectPtr<UARPGFactionDefinition> PrimaryFaction;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, ReplicatedUsing=OnRep_PrimaryFaction, Category="Faction") FName ExplicitFactionId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, Category="Faction") TArray<TObjectPtr<UARPGFactionDefinition>> SecondaryFactions;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, SaveGame, Category="Reputation") TArray<FARPGFactionStanding> Reputation;
    UPROPERTY(BlueprintAssignable) FARPGOnReputationChanged OnReputationChanged;
    UPROPERTY(BlueprintAssignable) FARPGOnPrimaryFactionChanged OnPrimaryFactionChanged;

    UFUNCTION(BlueprintPure, Category="ARPG|Faction") FName GetPrimaryFactionId() const;
    UFUNCTION(BlueprintCallable, Category="ARPG|Faction", meta=(BlueprintAuthorityOnly)) void SetPrimaryFaction(UARPGFactionDefinition* NewFaction);
    UFUNCTION(BlueprintCallable, Category="ARPG|Faction", meta=(BlueprintAuthorityOnly)) void SetPrimaryFactionId(FName NewFactionId);
    UFUNCTION(BlueprintPure, Category="ARPG|Faction") int32 GetReputation(FName FactionId) const;
    UFUNCTION(BlueprintCallable, Category="ARPG|Faction", meta=(BlueprintAuthorityOnly)) void AddReputation(FName FactionId, int32 Delta);
    UFUNCTION(BlueprintCallable, Category="ARPG|Faction", meta=(BlueprintAuthorityOnly)) void SetReputation(FName FactionId, int32 Value);
    UFUNCTION(BlueprintPure, Category="ARPG|Faction") EARPGFactionDisposition GetDisposition(const UARPGFactionDefinition* Faction) const;
    UFUNCTION(BlueprintPure, Category="ARPG|Faction") int32 GetBaseRelationshipTo(const UARPGFactionComponent* Other) const;
    UFUNCTION(BlueprintPure, Category="ARPG|Faction") int32 GetBaseRelationshipToFactionId(FName OtherFactionId) const;
    UFUNCTION(BlueprintPure, Category="ARPG|Faction") bool IsHostileTo(const UARPGFactionComponent* Other) const;
    UFUNCTION(BlueprintPure, Category="ARPG|Faction") bool IsFriendlyTo(const UARPGFactionComponent* Other) const;
    UFUNCTION(BlueprintCallable, Category="ARPG|Faction", meta=(BlueprintAuthorityOnly)) void ReplaceReputation(const TArray<FARPGFactionStanding>& NewReputation);

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
protected:
    UFUNCTION() void OnRep_PrimaryFaction();
};
