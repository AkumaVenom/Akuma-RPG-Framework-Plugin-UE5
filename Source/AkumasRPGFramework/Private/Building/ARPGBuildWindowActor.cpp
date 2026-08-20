#include "Building/ARPGBuildWindowActor.h"
#include "Animation/AnimSequenceBase.h"
#include "Components/BoxComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

AARPGBuildWindowActor::AARPGBuildWindowActor()
{
    WindowCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("WindowCollision"));
    WindowCollision->SetupAttachment(RootComponent);
    WindowCollision->SetCollisionProfileName(TEXT("BlockAll"));
    WindowCollision->SetGenerateOverlapEvents(false);
    WindowCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    WindowCollision->SetCanEverAffectNavigation(false);

    // Kept separate from the gameplay blocker so an open Window remains targetable by the same
    // player view-interaction button. It blocks only Visibility and therefore never blocks Pawn movement.
    WindowInteractionCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("WindowInteractionCollision"));
    WindowInteractionCollision->SetupAttachment(RootComponent);
    WindowInteractionCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    WindowInteractionCollision->SetCollisionObjectType(ECC_WorldDynamic);
    WindowInteractionCollision->SetCollisionResponseToAllChannels(ECR_Ignore);
    WindowInteractionCollision->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
    WindowInteractionCollision->SetGenerateOverlapEvents(false);
    WindowInteractionCollision->SetCanEverAffectNavigation(false);
}

void AARPGBuildWindowActor::BeginPlay()
{
    Super::BeginPlay();
    RefreshWindowGeometry();
    ApplyWindowRestPose();
    RefreshWindowCollisionState();
    RefreshWindowInteractionCollisionState();
}

void AARPGBuildWindowActor::RefreshDefinitionPresentation()
{
    Super::RefreshDefinitionPresentation();
    RefreshWindowGeometry();
    ApplyWindowRestPose();
    RefreshWindowCollisionState();
    RefreshWindowInteractionCollisionState();
}

void AARPGBuildWindowActor::RefreshConstructionPresentation(bool bForce)
{
    Super::RefreshConstructionPresentation(bForce);
    RefreshWindowCollisionState();
    RefreshWindowInteractionCollisionState();
}

bool AARPGBuildWindowActor::SetWindowOpen(bool bOpen, AActor* Requester)
{
    if (!HasAuthority() || !IsConstructionComplete() || bWindowTransitioning || (Requester && !CanActorUse(Requester))) return false;
    if (bWindowOpen == bOpen) return true;

    bWindowOpen = bOpen;
    BeginWindowTransition(true);
    OnWindowStateChanged.Broadcast(bWindowOpen);
    ForceNetUpdate();
    return true;
}

bool AARPGBuildWindowActor::ToggleWindow(AActor* Requester)
{
    return SetWindowOpen(!bWindowOpen, Requester);
}

void AARPGBuildWindowActor::RestoreWindowOpenState(bool bOpen)
{
    if (!HasAuthority()) return;

    GetWorldTimerManager().ClearTimer(WindowAnimationTimer);
    bWindowTransitioning = false;
    bWindowOpen = bOpen;
    ApplyWindowRestPose();
    RefreshWindowCollisionState();
    RefreshWindowInteractionCollisionState();
    ForceNetUpdate();
}

void AARPGBuildWindowActor::OnRep_WindowOpen()
{
    BeginWindowTransition(true);
    OnWindowStateChanged.Broadcast(bWindowOpen);
}

UAnimSequenceBase* AARPGBuildWindowActor::ResolveTransitionAnimation(bool bOpening, bool& bOutReverse) const
{
    bOutReverse = false;
    if (!Definition) return nullptr;

    if (bOpening)
    {
        if (UAnimSequenceBase* Open = Definition->WindowOpenAnimation.LoadSynchronous()) return Open;
        if (UAnimSequenceBase* Close = Definition->WindowCloseAnimation.LoadSynchronous())
        {
            bOutReverse = true;
            return Close;
        }
    }
    else
    {
        if (UAnimSequenceBase* Close = Definition->WindowCloseAnimation.LoadSynchronous()) return Close;
        if (UAnimSequenceBase* Open = Definition->WindowOpenAnimation.LoadSynchronous())
        {
            bOutReverse = true;
            return Open;
        }
    }
    return nullptr;
}

void AARPGBuildWindowActor::BeginWindowTransition(bool bPlaySound)
{
    GetWorldTimerManager().ClearTimer(WindowAnimationTimer);
    bWindowTransitioning = false;
    RefreshWindowCollisionState();

    bool bReverse = false;
    UAnimSequenceBase* Animation = ResolveTransitionAnimation(bWindowOpen, bReverse);
    const float RateMagnitude = Definition ? FMath::Max(0.01f, Definition->WindowAnimationPlayRate) : 1.f;

    if (BuildSkeletalMesh && BuildSkeletalMesh->GetSkeletalMeshAsset() && Animation)
    {
        const float Length = FMath::Max(0.f, Animation->GetPlayLength());
        BuildSkeletalMesh->SetComponentTickEnabled(true);
        BuildSkeletalMesh->PlayAnimation(Animation, false);
        BuildSkeletalMesh->SetPlayRate(bReverse ? -RateMagnitude : RateMagnitude);
        BuildSkeletalMesh->SetPosition(bReverse ? Length : 0.f, false);
        bWindowTransitioning = Length > KINDA_SMALL_NUMBER;

        if (bWindowTransitioning)
        {
            const float Duration = Length / RateMagnitude;
            GetWorldTimerManager().SetTimer(WindowAnimationTimer, this, &AARPGBuildWindowActor::FinishWindowTransition, FMath::Max(0.01f, Duration), false);
        }
        else
        {
            FinishWindowTransition();
        }
    }
    else
    {
        // A Window remains fully functional as replicated state/collision even before animation art is assigned.
        ApplyWindowRestPose();
        RefreshWindowCollisionState();
    }

    // Re-evaluate after bWindowTransitioning is known. Closing transitions deliberately stay non-blocking
    // until the closed pose is reached; opening becomes non-blocking immediately.
    RefreshWindowCollisionState();
    RefreshWindowInteractionCollisionState();

    if (bPlaySound && Definition)
    {
        USoundBase* Sound = bWindowOpen ? Definition->WindowOpenSound.LoadSynchronous() : Definition->WindowCloseSound.LoadSynchronous();
        if (Sound) UGameplayStatics::PlaySoundAtLocation(this, Sound, GetActorLocation());
    }
}

void AARPGBuildWindowActor::FinishWindowTransition()
{
    GetWorldTimerManager().ClearTimer(WindowAnimationTimer);
    bWindowTransitioning = false;
    ApplyWindowRestPose();
    RefreshWindowCollisionState();
    RefreshWindowInteractionCollisionState();
}

void AARPGBuildWindowActor::ApplyWindowRestPose()
{
    if (!BuildSkeletalMesh || !BuildSkeletalMesh->GetSkeletalMeshAsset()) return;

    UAnimSequenceBase* PoseAnimation = nullptr;
    float PosePosition = 0.f;

    if (Definition)
    {
        UAnimSequenceBase* Open = Definition->WindowOpenAnimation.LoadSynchronous();
        UAnimSequenceBase* Close = Definition->WindowCloseAnimation.LoadSynchronous();
        if (bWindowOpen)
        {
            if (Open)
            {
                PoseAnimation = Open;
                PosePosition = Open->GetPlayLength();
            }
            else if (Close)
            {
                PoseAnimation = Close;
                PosePosition = 0.f;
            }
        }
        else
        {
            if (Open)
            {
                PoseAnimation = Open;
                PosePosition = 0.f;
            }
            else if (Close)
            {
                PoseAnimation = Close;
                PosePosition = Close->GetPlayLength();
            }
        }
    }

    if (PoseAnimation)
    {
        // Give the single-node instance one render/update frame at the exact endpoint, then return the
        // component to the framework's dormant-at-rest policy. This avoids permanent skeletal Tick.
        BuildSkeletalMesh->SetComponentTickEnabled(true);
        BuildSkeletalMesh->PlayAnimation(PoseAnimation, false);
        BuildSkeletalMesh->SetPlayRate(0.f);
        BuildSkeletalMesh->SetPosition(PosePosition, false);
        GetWorldTimerManager().SetTimerForNextTick(this, &AARPGBuildWindowActor::DisableWindowSkeletalTick);
    }
    else
    {
        BuildSkeletalMesh->Stop();
        BuildSkeletalMesh->SetComponentTickEnabled(false);
    }
}

void AARPGBuildWindowActor::DisableWindowSkeletalTick()
{
    if (!bWindowTransitioning && BuildSkeletalMesh) BuildSkeletalMesh->SetComponentTickEnabled(false);
}

void AARPGBuildWindowActor::RefreshWindowGeometry()
{
    FVector LocalMin;
    FVector LocalMax;
    if (!GetActiveBuildVisualLocalBounds(LocalMin, LocalMax))
    {
        if (WindowCollision) WindowCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        if (WindowInteractionCollision) WindowInteractionCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        return;
    }

    const FVector Center = (LocalMin + LocalMax) * 0.5f;
    FVector Extent = (LocalMax - LocalMin) * 0.5f;
    Extent.X = FMath::Max(1.f, Extent.X);
    Extent.Y = FMath::Max(1.f, Extent.Y);
    Extent.Z = FMath::Max(1.f, Extent.Z);

    if (WindowCollision)
    {
        WindowCollision->SetRelativeLocation(Center);
        WindowCollision->SetRelativeRotation(FRotator::ZeroRotator);
        WindowCollision->SetBoxExtent(Extent, false);
    }
    if (WindowInteractionCollision)
    {
        WindowInteractionCollision->SetRelativeLocation(Center);
        WindowInteractionCollision->SetRelativeRotation(FRotator::ZeroRotator);
        WindowInteractionCollision->SetBoxExtent(Extent + FVector(4.f), false);
    }
}

void AARPGBuildWindowActor::RefreshWindowCollisionState()
{
    if (!WindowCollision) return;

    // WindowCollision is the authoritative gameplay shape. Imported Static/Skeletal collision is disabled
    // so Physics Asset quality cannot make an open Window remain invisibly blocked or double-collide.
    if (BuildMesh) BuildMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    if (BuildSkeletalMesh) BuildSkeletalMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    const bool bBuiltCollision = Definition && (IsConstructionComplete() || Definition->bCollisionDuringConstruction);
    bool bShouldCollide = bBuiltCollision;
    if (bShouldCollide && Definition->bDisableWindowCollisionWhenOpen)
    {
        // Opening removes the blocker immediately. Closing restores it only once the animation has
        // reached the closed pose, preventing a player from being trapped by an early collision snap.
        if (bWindowOpen || bWindowTransitioning) bShouldCollide = false;
    }
    WindowCollision->SetCollisionEnabled(bShouldCollide ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
}

void AARPGBuildWindowActor::RefreshWindowInteractionCollisionState()
{
    if (!WindowInteractionCollision) return;
    WindowInteractionCollision->SetCollisionEnabled(IsConstructionComplete() ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
}

void AARPGBuildWindowActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AARPGBuildWindowActor, bWindowOpen);
}
