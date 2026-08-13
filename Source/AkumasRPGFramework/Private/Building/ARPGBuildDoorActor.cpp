#include "Building/ARPGBuildDoorActor.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

AARPGBuildDoorActor::AARPGBuildDoorActor()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;
    DoorPivot = CreateDefaultSubobject<USceneComponent>(TEXT("DoorPivot"));
    DoorPivot->SetupAttachment(RootComponent);
    if (BuildMesh) BuildMesh->SetupAttachment(DoorPivot);
}

void AARPGBuildDoorActor::BeginPlay()
{
    Super::BeginPlay();
    DoorAlpha = bDoorOpen ? 1.f : 0.f;
    DoorTargetAlpha = DoorAlpha;
    ApplyDoorPose();
}

bool AARPGBuildDoorActor::SetDoorOpen(bool bOpen, AActor* Requester)
{
    if (!HasAuthority() || !IsConstructionComplete() || (Requester && !CanActorUse(Requester))) return false;
    if (bDoorOpen == bOpen) return true;
    bDoorOpen = bOpen;
    BeginDoorTransition();
    OnDoorStateChanged.Broadcast(bDoorOpen);
    if (bDoorOpen && bAutoClose)
        GetWorldTimerManager().SetTimer(AutoCloseTimer, this, &AARPGBuildDoorActor::HandleAutoClose, FMath::Max(0.1f, AutoCloseDelay), false);
    else
        GetWorldTimerManager().ClearTimer(AutoCloseTimer);
    ForceNetUpdate();
    return true;
}

bool AARPGBuildDoorActor::ToggleDoor(AActor* Requester) { return SetDoorOpen(!bDoorOpen, Requester); }

void AARPGBuildDoorActor::RestoreDoorOpenState(bool bOpen)
{
    if (!HasAuthority()) return;
    bDoorOpen = bOpen; DoorAlpha = bOpen ? 1.f : 0.f; DoorTargetAlpha = DoorAlpha; ApplyDoorPose(); ForceNetUpdate();
}

void AARPGBuildDoorActor::OnRep_DoorOpen()
{
    BeginDoorTransition();
    OnDoorStateChanged.Broadcast(bDoorOpen);
}

void AARPGBuildDoorActor::BeginDoorTransition()
{
    DoorTargetAlpha = bDoorOpen ? 1.f : 0.f;
    SetActorTickEnabled(true);
}

void AARPGBuildDoorActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    const float Speed = 1.f / FMath::Max(0.01f, TransitionSeconds);
    DoorAlpha = FMath::FInterpConstantTo(DoorAlpha, DoorTargetAlpha, DeltaSeconds, Speed);
    ApplyDoorPose();
    if (FMath::IsNearlyEqual(DoorAlpha, DoorTargetAlpha, 0.001f))
    {
        DoorAlpha = DoorTargetAlpha;
        ApplyDoorPose();
        SetActorTickEnabled(false);
    }
}

void AARPGBuildDoorActor::ApplyDoorPose()
{
    if (DoorPivot) DoorPivot->SetRelativeRotation(FRotator(0.f, OpenYaw * DoorAlpha, 0.f));
}

void AARPGBuildDoorActor::HandleAutoClose() { SetDoorOpen(false, nullptr); }

void AARPGBuildDoorActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AARPGBuildDoorActor, bDoorOpen);
}
