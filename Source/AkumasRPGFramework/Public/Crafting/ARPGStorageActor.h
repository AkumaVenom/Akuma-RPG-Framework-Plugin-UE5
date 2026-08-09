#pragma once
#include "CoreMinimal.h"
#include "Building/ARPGBuildPieceActor.h"
#include "ARPGStorageActor.generated.h"
class UARPGInventoryComponent;

UCLASS(BlueprintType, Blueprintable)
class AKUMASRPGFRAMEWORK_API AARPGStorageActor : public AARPGBuildPieceActor
{
    GENERATED_BODY()
public:
    AARPGStorageActor();
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, SaveGame, Category="Storage") FGuid ContainerId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Storage") TObjectPtr<UARPGInventoryComponent> Inventory;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Storage") bool bPersistent = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Storage") bool bAllowMultipleViewers = true;
    UFUNCTION(BlueprintCallable, Category="ARPG|Storage", meta=(BlueprintAuthorityOnly)) void EnsureContainerId();
    UFUNCTION(BlueprintCallable, Category="ARPG|Storage", meta=(BlueprintAuthorityOnly)) void InitializeStorageOwnership(AActor* OwnerActor);
    UFUNCTION(BlueprintPure, Category="ARPG|Storage") bool CanActorAccess(AActor* Actor) const;
    virtual void BeginPlay() override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
