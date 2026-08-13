#pragma once
#include "CoreMinimal.h"
#include "Building/ARPGBuildPieceActor.h"
#include "ARPGBuildDoorActor.generated.h"

class USceneComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FARPGDoorStateChanged, bool, bOpen);

/** Ready replicated buildable door. The base BuildMesh is re-parented under DoorPivot and animated locally from replicated state. */
UCLASS(BlueprintType, Blueprintable)
class AKUMASRPGFRAMEWORK_API AARPGBuildDoorActor : public AARPGBuildPieceActor
{
    GENERATED_BODY()
public:
    AARPGBuildDoorActor();
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Door") TObjectPtr<USceneComponent> DoorPivot;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Door", meta=(ClampMin="-180.0", ClampMax="180.0", Units="deg")) float OpenYaw = 90.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Door", meta=(ClampMin="0.01", Units="s")) float TransitionSeconds = 0.25f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Door") bool bAutoClose = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Door", meta=(ClampMin="0.1", Units="s", EditCondition="bAutoClose")) float AutoCloseDelay = 3.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, ReplicatedUsing=OnRep_DoorOpen, SaveGame, Category="Door") bool bDoorOpen = false;
    UPROPERTY(BlueprintAssignable, Category="Door") FARPGDoorStateChanged OnDoorStateChanged;

    UFUNCTION(BlueprintCallable, Category="ARPG|Building|Door", meta=(BlueprintAuthorityOnly)) bool SetDoorOpen(bool bOpen, AActor* Requester = nullptr);
    UFUNCTION(BlueprintCallable, Category="ARPG|Building|Door", meta=(BlueprintAuthorityOnly)) bool ToggleDoor(AActor* Requester = nullptr);
    UFUNCTION(BlueprintCallable, Category="ARPG|Building|Door", meta=(BlueprintAuthorityOnly)) void RestoreDoorOpenState(bool bOpen);
    UFUNCTION(BlueprintPure, Category="ARPG|Building|Door") bool IsDoorOpen() const { return bDoorOpen; }

    virtual void Tick(float DeltaSeconds) override;
    virtual void BeginPlay() override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
protected:
    UFUNCTION() void OnRep_DoorOpen();
private:
    float DoorAlpha = 0.f;
    float DoorTargetAlpha = 0.f;
    FTimerHandle AutoCloseTimer;
    void BeginDoorTransition();
    void ApplyDoorPose();
    void HandleAutoClose();
};
