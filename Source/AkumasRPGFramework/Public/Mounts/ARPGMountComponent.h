#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ARPGTypes.h"
#include "ARPGMountComponent.generated.h"

class UARPGMountDefinition;
class AARPGMountCharacter;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FARPGMountCollectionChanged, FName, MountId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FARPGMountSummonResult, bool, bSuccess, AARPGMountCharacter*, Mount);

UCLASS(ClassGroup=(ARPG), meta=(BlueprintSpawnableComponent))
class AKUMASRPGFRAMEWORK_API UARPGMountComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UARPGMountComponent();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, ReplicatedUsing=OnRep_MountState, SaveGame, Category="Mounts") TArray<FName> UnlockedMountIds;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, ReplicatedUsing=OnRep_MountState, SaveGame, Category="Mounts") FName ActiveMountId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mounts", meta=(ClampMin="0.0")) float SummonForwardOffset = 150.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mounts") bool bBlockSummonInCombat = true;
    UPROPERTY(BlueprintAssignable) FARPGMountCollectionChanged OnMountCollectionChanged;
    UPROPERTY(BlueprintAssignable) FARPGMountSummonResult OnMountSummonResult;

    UFUNCTION(BlueprintCallable, Category="ARPG|Mounts") bool UnlockMount(UARPGMountDefinition* Definition);
    UFUNCTION(BlueprintCallable, Category="ARPG|Mounts") bool SummonAndRide(UARPGMountDefinition* Definition);
    UFUNCTION(BlueprintPure, Category="ARPG|Mounts") bool IsMountUnlocked(FName MountId) const { return UnlockedMountIds.Contains(MountId); }
    UFUNCTION(BlueprintCallable, Category="ARPG|Mounts", meta=(BlueprintAuthorityOnly)) void ReplaceMountState(const FARPGMountSaveState& State);
    UFUNCTION(BlueprintPure, Category="ARPG|Mounts") FARPGMountSaveState MakeMountSaveState() const;

protected:
    UFUNCTION(Server, Reliable) void ServerUnlockMount(UARPGMountDefinition* Definition);
    UFUNCTION(Server, Reliable) void ServerSummonAndRide(UARPGMountDefinition* Definition);
    UFUNCTION() void OnRep_MountState();
    bool UnlockMountAuthority(UARPGMountDefinition* Definition);
    bool SummonAndRideAuthority(UARPGMountDefinition* Definition);
};
