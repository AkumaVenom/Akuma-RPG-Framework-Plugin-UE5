#pragma once
#include "CoreMinimal.h"
#include "Building/ARPGBuildPieceActor.h"
#include "ARPGBuildWindowActor.generated.h"

class UBoxComponent;
class UAnimSequenceBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FARPGWindowStateChanged, bool, bOpen);

/**
 * Native replicated buildable Window. The active Skeletal build visual can play Data-Asset-authored
 * open/close sequences while authoritative state, access, collision and persistence remain native.
 */
UCLASS(BlueprintType, Blueprintable)
class AKUMASRPGFRAMEWORK_API AARPGBuildWindowActor : public AARPGBuildPieceActor
{
    GENERATED_BODY()
public:
    AARPGBuildWindowActor();

    /** Bounds-driven gameplay blocker. Disabled while an interactive Window is open when configured. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Window|Collision") TObjectPtr<UBoxComponent> WindowCollision;
    /** Lightweight view-trace target that remains usable while the gameplay blocker is open/disabled. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Window|Interaction") TObjectPtr<UBoxComponent> WindowInteractionCollision;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, ReplicatedUsing=OnRep_WindowOpen, SaveGame, Category="Window") bool bWindowOpen = false;
    UPROPERTY(BlueprintAssignable, Category="Window") FARPGWindowStateChanged OnWindowStateChanged;

    UFUNCTION(BlueprintCallable, Category="ARPG|Building|Window", meta=(BlueprintAuthorityOnly)) bool SetWindowOpen(bool bOpen, AActor* Requester = nullptr);
    UFUNCTION(BlueprintCallable, Category="ARPG|Building|Window", meta=(BlueprintAuthorityOnly)) bool ToggleWindow(AActor* Requester = nullptr);
    UFUNCTION(BlueprintCallable, Category="ARPG|Building|Window", meta=(BlueprintAuthorityOnly)) void RestoreWindowOpenState(bool bOpen);
    UFUNCTION(BlueprintPure, Category="ARPG|Building|Window") bool IsWindowOpen() const { return bWindowOpen; }
    UFUNCTION(BlueprintPure, Category="ARPG|Building|Window") bool IsWindowTransitioning() const { return bWindowTransitioning; }

    virtual void BeginPlay() override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
    UFUNCTION() void OnRep_WindowOpen();
    virtual void RefreshDefinitionPresentation() override;
    virtual void RefreshConstructionPresentation(bool bForce = false) override;

private:
    void RefreshWindowGeometry();
    void RefreshWindowCollisionState();
    void RefreshWindowInteractionCollisionState();
    void BeginWindowTransition(bool bPlaySound);
    void ApplyWindowRestPose();
    void FinishWindowTransition();
    void DisableWindowSkeletalTick();
    UAnimSequenceBase* ResolveTransitionAnimation(bool bOpening, bool& bOutReverse) const;

    FTimerHandle WindowAnimationTimer;
    bool bWindowTransitioning = false;
};
