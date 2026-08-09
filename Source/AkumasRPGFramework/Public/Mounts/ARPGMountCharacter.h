#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "ARPGTypes.h"
#include "ARPGMountCharacter.generated.h"

class UARPGMountDefinition;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FARPGMountRiderChanged, ACharacter*, Rider);

UCLASS(BlueprintType, Blueprintable)
class AKUMASRPGFRAMEWORK_API AARPGMountCharacter : public ACharacter
{
    GENERATED_BODY()
public:
    AARPGMountCharacter();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mount") FName RiderSocketName = TEXT("RiderSocket");
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mount") FVector DismountOffset = FVector(120.f, 0.f, 0.f);
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, ReplicatedUsing=OnRep_Rider, Category="Mount") TObjectPtr<ACharacter> Rider;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category="Mount") TObjectPtr<UARPGMountDefinition> MountDefinition;
    UPROPERTY(BlueprintAssignable) FARPGMountRiderChanged OnRiderChanged;

    UFUNCTION(BlueprintCallable, Category="ARPG|Mount", meta=(BlueprintAuthorityOnly)) bool MountRider(ACharacter* NewRider, UARPGMountDefinition* Definition);
    UFUNCTION(BlueprintCallable, Category="ARPG|Mount") void RequestDismount(bool bDestroyMount=true);
    UFUNCTION(BlueprintImplementableEvent, Category="ARPG|Mount") void ConfigureMountMovement(EARPGMountMovementType MovementType);

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
protected:
    UFUNCTION(Server, Reliable) void ServerRequestDismount(bool bDestroyMount);
    UFUNCTION() void OnRep_Rider();
    void DismountAuthority(bool bDestroyMount);
};
