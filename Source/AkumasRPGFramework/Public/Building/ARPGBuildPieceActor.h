#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Data/ARPGBuildPieceDefinition.h"
#include "ARPGBuildPieceActor.generated.h"

class UARPGFactionOwnershipComponent;
class USceneComponent;
class UStaticMeshComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FARPGBuildingHealthChanged, float, NewHealth, float, Delta);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FARPGBuildingDestroyed);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FARPGConstructionProgressChanged, float, Progress01);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FARPGConstructionCompleted);

UCLASS(BlueprintType, Blueprintable)
class AKUMASRPGFRAMEWORK_API AARPGBuildPieceActor : public AActor
{
    GENERATED_BODY()
public:
    AARPGBuildPieceActor();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Building|Components") TObjectPtr<USceneComponent> BuildRoot;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Building|Components") TObjectPtr<UStaticMeshComponent> BuildMesh;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, SaveGame, Category="Building") FGuid BuildingId;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, ReplicatedUsing=OnRep_Definition, Category="Building") TObjectPtr<UARPGBuildPieceDefinition> Definition;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Building") TObjectPtr<UARPGFactionOwnershipComponent> Ownership;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, ReplicatedUsing=OnRep_Health, SaveGame, Category="Building") float Health = 100.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, SaveGame, Category="Building") int32 UpgradeLevel = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, SaveGame, Category="Building") bool bRuntimePlaced = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Building") bool bAllowDemolish = true;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, ReplicatedUsing=OnRep_ConstructionState, SaveGame, Category="Building|Construction") bool bConstructionComplete = true;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, ReplicatedUsing=OnRep_ConstructionState, SaveGame, Category="Building|Construction") float ConstructionStartServerTime = 0.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, ReplicatedUsing=OnRep_ConstructionState, SaveGame, Category="Building|Construction") float ConstructionDuration = 0.f;

    UPROPERTY(BlueprintAssignable) FARPGBuildingHealthChanged OnBuildingHealthChanged;
    UPROPERTY(BlueprintAssignable) FARPGBuildingDestroyed OnBuildingDestroyed;
    UPROPERTY(BlueprintAssignable, Category="Building|Construction") FARPGConstructionProgressChanged OnConstructionProgressChanged;
    UPROPERTY(BlueprintAssignable, Category="Building|Construction") FARPGConstructionCompleted OnConstructionCompleted;

    UFUNCTION(BlueprintCallable, Category="ARPG|Building", meta=(BlueprintAuthorityOnly)) virtual void InitializeBuilding(UARPGBuildPieceDefinition* InDefinition, AActor* Builder);
    UFUNCTION(BlueprintCallable, Category="ARPG|Building", meta=(BlueprintAuthorityOnly)) bool ApplyBuildingDamage(float Amount, AActor* DamageCauser=nullptr);
    UFUNCTION(BlueprintCallable, Category="ARPG|Building", meta=(BlueprintAuthorityOnly)) bool RepairBuilding(float Amount, AActor* Repairer=nullptr);
    UFUNCTION(BlueprintCallable, Category="ARPG|Building", meta=(BlueprintAuthorityOnly)) bool Demolish(AActor* Requester=nullptr);
    UFUNCTION(BlueprintPure, Category="ARPG|Building") float GetMaxBuildingHealth() const;
    UFUNCTION(BlueprintPure, Category="ARPG|Building") bool CanActorUse(AActor* Actor) const;
    UFUNCTION(BlueprintPure, Category="ARPG|Building") bool CanActorModify(AActor* Actor) const;

    UFUNCTION(BlueprintPure, Category="ARPG|Building|Construction") bool IsConstructionComplete() const { return bConstructionComplete; }
    UFUNCTION(BlueprintPure, Category="ARPG|Building|Construction") float GetConstructionProgress01() const;
    UFUNCTION(BlueprintPure, Category="ARPG|Building|Construction") float GetConstructionRemainingSeconds() const;
    UFUNCTION(BlueprintCallable, Category="ARPG|Building|Construction", meta=(BlueprintAuthorityOnly)) void RestoreConstructionState(bool bWasComplete, float RemainingSeconds);

    /** Produces ready standard snap transforms plus any custom snap transforms authored on the definition. */
    UFUNCTION(BlueprintCallable, Category="ARPG|Building|Snapping") void GetSnapTransformsFor(const UARPGBuildPieceDefinition* IncomingPiece, TArray<FTransform>& OutWorldTransforms) const;

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
protected:
    UFUNCTION() void OnRep_Definition();
    UFUNCTION() void OnRep_Health(float OldHealth);
    UFUNCTION() void OnRep_ConstructionState();
    virtual void RefreshDefinitionPresentation();
    virtual void RefreshConstructionPresentation(bool bForce = false);
    void CompleteConstructionAuthority();
    float GetAuthoritativeServerTime() const;
private:
    FVector BaseMeshRelativeLocation = FVector::ZeroVector;
    FVector BaseMeshRelativeScale = FVector::OneVector;
    float LastConstructionVisualProgress = -1.f;
};
