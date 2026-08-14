#include "Building/ARPGBuildDoorActor.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

namespace
{
    static bool ARPGGetDoorActorLocalBounds(const AARPGBuildDoorActor* Door, FVector& OutMin, FVector& OutMax)
    {
        if (!Door || !Door->BuildMesh || !Door->BuildMesh->GetStaticMesh()) return false;

        const FBoxSphereBounds Bounds = Door->BuildMesh->GetStaticMesh()->GetBounds();
        const FBox RawBox(Bounds.Origin - Bounds.BoxExtent, Bounds.Origin + Bounds.BoxExtent);
        const FBox ActorLocalBox = RawBox.TransformBy(Door->BuildMesh->GetRelativeTransform());
        if (!ActorLocalBox.IsValid) return false;

        OutMin = ActorLocalBox.Min;
        OutMax = ActorLocalBox.Max;
        return true;
    }
}

AARPGBuildDoorActor::AARPGBuildDoorActor()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;

    DoorPivot = CreateDefaultSubobject<USceneComponent>(TEXT("DoorPivot"));
    DoorPivot->SetupAttachment(RootComponent);
    DoorPivot->SetMobility(EComponentMobility::Movable);

    if (BuildMesh)
    {
        BuildMesh->SetupAttachment(DoorPivot);
        BuildMesh->SetMobility(EComponentMobility::Movable);
    }

    // Do not trust marketplace/imported mesh simple collision for a gameplay-critical door.
    // This native slab collider follows the exact same moving pivot as the visible door, so a
    // closed door is always solid and an open door's collision swings out of the doorway with it.
    DoorCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("DoorCollision"));
    DoorCollision->SetupAttachment(DoorPivot);
    DoorCollision->SetMobility(EComponentMobility::Movable);
    DoorCollision->SetCollisionProfileName(TEXT("BlockAll"));
    DoorCollision->SetGenerateOverlapEvents(false);
    DoorCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    DoorCollision->SetCanEverAffectNavigation(false);
}

void AARPGBuildDoorActor::BeginPlay()
{
    Super::BeginPlay();
    DoorAlpha = bDoorOpen ? 1.f : 0.f;
    DoorTargetAlpha = DoorAlpha;
    RefreshDoorGeometry();
    ApplyDoorPose();
    RefreshDoorCollisionState();
}

void AARPGBuildDoorActor::RefreshDefinitionPresentation()
{
    Super::RefreshDefinitionPresentation();
    RefreshDoorGeometry();
    ApplyDoorPose();
    RefreshDoorCollisionState();
}

void AARPGBuildDoorActor::RefreshConstructionPresentation(bool bForce)
{
    Super::RefreshConstructionPresentation(bForce);
    RefreshDoorCollisionState();
}

bool AARPGBuildDoorActor::SetDoorOpen(bool bOpen, AActor* Requester)
{
    if (!HasAuthority() || !IsConstructionComplete() || (Requester && !CanActorUse(Requester))) return false;
    if (bDoorOpen == bOpen) return true;

    bDoorOpen = bOpen;
    RefreshDoorGeometry();
    BeginDoorTransition();
    OnDoorStateChanged.Broadcast(bDoorOpen);

    if (bDoorOpen && bAutoClose)
        GetWorldTimerManager().SetTimer(AutoCloseTimer, this, &AARPGBuildDoorActor::HandleAutoClose, FMath::Max(0.1f, AutoCloseDelay), false);
    else
        GetWorldTimerManager().ClearTimer(AutoCloseTimer);

    ForceNetUpdate();
    return true;
}

bool AARPGBuildDoorActor::ToggleDoor(AActor* Requester)
{
    return SetDoorOpen(!bDoorOpen, Requester);
}

void AARPGBuildDoorActor::RestoreDoorOpenState(bool bOpen)
{
    if (!HasAuthority()) return;

    bDoorOpen = bOpen;
    DoorAlpha = bOpen ? 1.f : 0.f;
    DoorTargetAlpha = DoorAlpha;
    RefreshDoorGeometry();
    ApplyDoorPose();
    RefreshDoorCollisionState();
    SetActorTickEnabled(false);
    ForceNetUpdate();
}

void AARPGBuildDoorActor::OnRep_DoorOpen()
{
    RefreshDoorGeometry();
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
    // Base Tick owns construction progression only. Once construction is complete it deliberately
    // leaves a specialised actor's explicitly enabled Tick alone, allowing this door transition to run.
    Super::Tick(DeltaSeconds);

    RefreshDoorCollisionState();
    if (!IsConstructionComplete()) return;

    const float Speed = 1.f / FMath::Max(0.01f, TransitionSeconds);
    DoorAlpha = FMath::FInterpConstantTo(DoorAlpha, DoorTargetAlpha, DeltaSeconds, Speed);
    ApplyDoorPose();

    if (FMath::IsNearlyEqual(DoorAlpha, DoorTargetAlpha, 0.001f))
    {
        DoorAlpha = DoorTargetAlpha;
        ApplyDoorPose();
        RefreshDoorCollisionState();
        SetActorTickEnabled(false);
    }
}

void AARPGBuildDoorActor::RefreshDoorGeometry()
{
    if (!DoorCollision || !BuildMesh) return;

    FVector LocalMin;
    FVector LocalMax;
    if (!ARPGGetDoorActorLocalBounds(this, LocalMin, LocalMax))
    {
        DoorCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        DoorHingeLocal = FVector::ZeroVector;
        return;
    }

    const FVector Center = (LocalMin + LocalMax) * 0.5f;
    FVector Extent = (LocalMax - LocalMin) * 0.5f;

    // Avoid zero-thickness physics shapes from unusual art while preserving the authored visible size.
    Extent.X = FMath::Max(1.f, Extent.X);
    Extent.Y = FMath::Max(1.f, Extent.Y);
    Extent.Z = FMath::Max(1.f, Extent.Z);

    DoorCollision->SetRelativeLocation(Center);
    DoorCollision->SetRelativeRotation(FRotator::ZeroRotator);
    DoorCollision->SetBoxExtent(Extent, false);

    // Building logical axes define local X as doorway width/run and local +Y as the wall/front side.
    // Hinge side is authored on the Door definition AFTER MeshRelativeTransform has established those
    // logical axes. Looking from logical +Y toward the Door, Left maps to +X and Right maps to -X.
    // ApplyDoorPose supplies the compensating translation, so neither a centered import pivot nor a
    // corner pivot can move the closed snapped position away from the doorway opening.
    const bool bHingeOnLeft = !Definition || Definition->DoorHingeSide == EARPGBuildDoorHingeSide::Left;
    const float HingeX = bHingeOnLeft ? LocalMax.X : LocalMin.X;
    DoorHingeLocal = FVector(HingeX, Center.Y, Center.Z);
}

void AARPGBuildDoorActor::RefreshDoorCollisionState()
{
    if (!DoorCollision) return;

    const bool bShouldCollide = Definition && (IsConstructionComplete() || Definition->bCollisionDuringConstruction);
    DoorCollision->SetCollisionEnabled(bShouldCollide ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
}

void AARPGBuildDoorActor::ApplyDoorPose()
{
    if (!DoorPivot) return;

    const float DoorYaw = OpenYaw * DoorAlpha;
    const FQuat DoorRotation = FRotator(0.f, DoorYaw, 0.f).Quaternion();

    // Rotate around an arbitrary actor-local hinge without altering the authored BuildMesh relative
    // transform. Translation = H - R(H) keeps hinge point H stationary while the whole DoorPivot rotates.
    const FVector PivotTranslation = DoorHingeLocal - DoorRotation.RotateVector(DoorHingeLocal);
    DoorPivot->SetRelativeLocationAndRotation(PivotTranslation, DoorRotation);
}

void AARPGBuildDoorActor::HandleAutoClose()
{
    SetDoorOpen(false, nullptr);
}

void AARPGBuildDoorActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AARPGBuildDoorActor, bDoorOpen);
}
