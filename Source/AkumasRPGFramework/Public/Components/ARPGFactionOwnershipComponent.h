#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ARPGFactionOwnershipComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FARPGOwnershipChanged);

UCLASS(ClassGroup=(ARPG), meta=(BlueprintSpawnableComponent))
class AKUMASRPGFRAMEWORK_API UARPGFactionOwnershipComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UARPGFactionOwnershipComponent();

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, SaveGame, Category="Ownership") FGuid OwnerAccountId;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, SaveGame, Category="Ownership") FGuid OwnerCharacterId;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, SaveGame, Category="Ownership") FName OwnerFactionId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Permissions") bool bSameFactionCanUse = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Permissions") bool bAlliesCanUse = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Permissions") bool bNeutralCanUse = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Permissions") bool bHostilesCanUse = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Permissions") bool bFactionMembersCanModify = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Permissions") bool bHostilesCanDamage = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Permissions") bool bFriendlyFireCanDamage = false;
    UPROPERTY(BlueprintAssignable) FARPGOwnershipChanged OnOwnershipChanged;

    UFUNCTION(BlueprintCallable, Category="ARPG|Ownership", meta=(BlueprintAuthorityOnly)) void InitializeFromActor(AActor* OwnerActor, bool bInheritFaction=true);
    UFUNCTION(BlueprintCallable, Category="ARPG|Ownership", meta=(BlueprintAuthorityOnly)) void SetOwnership(FGuid AccountId, FGuid CharacterId, FName FactionId);
    UFUNCTION(BlueprintPure, Category="ARPG|Ownership") bool IsPersonalOwner(const AActor* Actor) const;
    UFUNCTION(BlueprintPure, Category="ARPG|Ownership") bool CanActorUse(const AActor* Actor) const;
    UFUNCTION(BlueprintPure, Category="ARPG|Ownership") bool CanActorModify(const AActor* Actor) const;
    UFUNCTION(BlueprintPure, Category="ARPG|Ownership") bool CanActorDamage(const AActor* Actor) const;

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
    FGuid ResolveAccountId(const AActor* Actor) const;
    FGuid ResolveCharacterId(const AActor* Actor) const;
    FName ResolveFactionId(const AActor* Actor) const;
    int32 ResolveRelationship(const AActor* Actor) const;
};
