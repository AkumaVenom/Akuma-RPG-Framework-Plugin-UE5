#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ARPGTypes.h"
#include "ARPGBattlePetComponent.generated.h"

class UARPGBattlePetDefinition;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FARPGOnBattlePetCollectionChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FARPGOnPetCaptured, FName, SpeciesId, FGuid, InstanceId);

UCLASS(ClassGroup=(ARPG), meta=(BlueprintSpawnableComponent))
class AKUMASRPGFRAMEWORK_API UARPGBattlePetComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UARPGBattlePetComponent();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, ReplicatedUsing=OnRep_Pets, SaveGame) TArray<FARPGPetInstance> Pets;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, SaveGame) TArray<FGuid> ActiveTeam;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Battle Pets") int32 MaxTeamSize = 3;
    UPROPERTY(BlueprintAssignable) FARPGOnBattlePetCollectionChanged OnCollectionChanged;
    UPROPERTY(BlueprintAssignable) FARPGOnPetCaptured OnPetCaptured;

    UFUNCTION(BlueprintCallable, Category="ARPG|Battle Pets", meta=(BlueprintAuthorityOnly)) bool CapturePet(const UARPGBattlePetDefinition* Definition, int32 Level=1, EARPGRarity Quality=EARPGRarity::Common);
    UFUNCTION(BlueprintCallable, Category="ARPG|Battle Pets", meta=(BlueprintAuthorityOnly)) bool SetActiveTeam(const TArray<FGuid>& PetIds);
    UFUNCTION(BlueprintPure, Category="ARPG|Battle Pets") bool OwnsSpecies(FName SpeciesId) const;
    UFUNCTION(BlueprintCallable, Category="ARPG|Battle Pets", meta=(BlueprintAuthorityOnly)) void AddPetXP(FGuid PetId, int64 XPAmount);
    UFUNCTION(BlueprintCallable, Category="ARPG|Battle Pets", meta=(BlueprintAuthorityOnly)) void ReplacePetState(const TArray<FARPGPetInstance>& NewPets, const TArray<FGuid>& NewTeam);

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
protected:
    UFUNCTION() void OnRep_Pets();
};
