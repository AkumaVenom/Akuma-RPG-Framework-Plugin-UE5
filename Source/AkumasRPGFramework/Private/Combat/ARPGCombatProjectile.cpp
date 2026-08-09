#include "Combat/ARPGCombatProjectile.h"
#include "Components/SphereComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Components/ARPGCombatComponent.h"

AARPGCombatProjectile::AARPGCombatProjectile()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;
    SetReplicateMovement(true);

    Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
    SetRootComponent(Collision);
    Collision->InitSphereRadius(12.f);
    Collision->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    Collision->SetCollisionObjectType(ECC_WorldDynamic);
    Collision->SetCollisionResponseToAllChannels(ECR_Ignore);
    Collision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
    Collision->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
    Collision->SetNotifyRigidBodyCollision(true);
    Collision->OnComponentHit.AddDynamic(this, &AARPGCombatProjectile::HandleProjectileHit);

    ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
    ProjectileMovement->UpdatedComponent = Collision;
    ProjectileMovement->InitialSpeed = 2200.f;
    ProjectileMovement->MaxSpeed = 2200.f;
    ProjectileMovement->bRotationFollowsVelocity = true;
    ProjectileMovement->bShouldBounce = false;
    ProjectileMovement->ProjectileGravityScale = 0.f;
}

void AARPGCombatProjectile::InitializeCombatProjectile(AActor* InSourceActor, AActor* InTargetActor, float InDamage, EARPGBasicAttackType InAttackType, bool bInCanBeBlocked, bool bInCanBeParried, bool bInCritical, float InSpeed, bool bInHoming)
{
    if (!HasAuthority()) return;
    SourceActor = InSourceActor;
    TargetActor = InTargetActor;
    Damage = FMath::Max(0.f, InDamage);
    AttackType = InAttackType;
    bCanBeBlocked = bInCanBeBlocked;
    bCanBeParried = bInCanBeParried;
    bCritical = bInCritical;
    SetOwner(InSourceActor);
    SetInstigator(Cast<APawn>(InSourceActor));
    SetLifeSpan(FMath::Max(0.1f, LifeSeconds));

    if (Collision && InSourceActor)
        Collision->IgnoreActorWhenMoving(InSourceActor, true);

    if (ProjectileMovement)
    {
        const float Speed = FMath::Max(1.f, InSpeed);
        ProjectileMovement->InitialSpeed = Speed;
        ProjectileMovement->MaxSpeed = Speed;
        FVector Direction = GetActorForwardVector();
        if (InTargetActor)
            Direction = (InTargetActor->GetActorLocation() - GetActorLocation()).GetSafeNormal();
        ProjectileMovement->Velocity = Direction * Speed;
        ProjectileMovement->bIsHomingProjectile = bInHoming && InTargetActor && InTargetActor->GetRootComponent();
        ProjectileMovement->HomingAccelerationMagnitude = Speed * 4.f;
        ProjectileMovement->HomingTargetComponent = ProjectileMovement->bIsHomingProjectile ? InTargetActor->GetRootComponent() : nullptr;
    }
}

void AARPGCombatProjectile::HandleProjectileHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent, FVector NormalImpulse, const FHitResult& Hit)
{
    if (!HasAuthority() || !OtherActor || OtherActor == SourceActor) return;
    if (UARPGCombatComponent* Combat = OtherActor->FindComponentByClass<UARPGCombatComponent>())
        Combat->ReceiveCombatHit(SourceActor, Damage, AttackType, bCanBeBlocked, bCanBeParried, Hit.ImpactPoint, bCritical);
    Destroy();
}
