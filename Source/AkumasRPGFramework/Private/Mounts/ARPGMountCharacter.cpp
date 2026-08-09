#include "Mounts/ARPGMountCharacter.h"
#include "Data/ARPGMountDefinition.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Components/CapsuleComponent.h"
#include "Net/UnrealNetwork.h"

AARPGMountCharacter::AARPGMountCharacter()
{
    bReplicates = true;
    SetReplicateMovement(true);
}

bool AARPGMountCharacter::MountRider(ACharacter* NewRider, UARPGMountDefinition* Definition)
{
    if (!HasAuthority() || !NewRider || Rider) return false;
    AController* RiderController = NewRider->GetController();
    if (!RiderController) return false;
    Rider = NewRider;
    MountDefinition = Definition;
    SetOwner(RiderController);

    NewRider->GetCharacterMovement()->StopMovementImmediately();
    NewRider->GetCharacterMovement()->DisableMovement();
    NewRider->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    NewRider->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, RiderSocketName);
    RiderController->Possess(this);

    if (Definition)
    {
        GetCharacterMovement()->MaxWalkSpeed *= FMath::Max(0.1f, Definition->SpeedMultiplier);
        ConfigureMountMovement(Definition->MovementType);
    }
    OnRiderChanged.Broadcast(Rider);
    return true;
}

void AARPGMountCharacter::RequestDismount(bool bDestroyMount)
{
    if (HasAuthority()) DismountAuthority(bDestroyMount);
    else ServerRequestDismount(bDestroyMount);
}

void AARPGMountCharacter::ServerRequestDismount_Implementation(bool bDestroyMount)
{
    DismountAuthority(bDestroyMount);
}

void AARPGMountCharacter::DismountAuthority(bool bDestroyMount)
{
    if (!HasAuthority() || !Rider) return;
    ACharacter* OldRider = Rider;
    AController* RiderController = GetController();
    const FVector Target = GetActorLocation() + GetActorRotation().RotateVector(DismountOffset);

    OldRider->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
    OldRider->SetActorLocation(Target, false, nullptr, ETeleportType::TeleportPhysics);
    OldRider->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    OldRider->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    Rider = nullptr;
    MountDefinition = nullptr;
    if (RiderController) RiderController->Possess(OldRider);
    OnRiderChanged.Broadcast(nullptr);
    if (bDestroyMount) Destroy();
}

void AARPGMountCharacter::OnRep_Rider() { OnRiderChanged.Broadcast(Rider); }

void AARPGMountCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AARPGMountCharacter, Rider);
    DOREPLIFETIME(AARPGMountCharacter, MountDefinition);
}
