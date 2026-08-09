#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Combat/ARPGCombatTypes.h"
#include "ARPGCombatProjectile.generated.h"

class USphereComponent;
class UProjectileMovementComponent;
class UPrimitiveComponent;

UCLASS(BlueprintType, Blueprintable)
class AKUMASRPGFRAMEWORK_API AARPGCombatProjectile : public AActor
{
    GENERATED_BODY()
public:
    AARPGCombatProjectile();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Projectile") TObjectPtr<USphereComponent> Collision;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Projectile") TObjectPtr<UProjectileMovementComponent> ProjectileMovement;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Projectile", meta=(ClampMin="0.1")) float LifeSeconds = 8.f;

    UFUNCTION(BlueprintCallable, Category="ARPG|Combat|Projectile", meta=(BlueprintAuthorityOnly))
    void InitializeCombatProjectile(AActor* InSourceActor, AActor* InTargetActor, float InDamage, EARPGBasicAttackType InAttackType, bool bInCanBeBlocked, bool bInCanBeParried, bool bInCritical, float InSpeed, bool bInHoming);

protected:
    UPROPERTY() TObjectPtr<AActor> SourceActor;
    UPROPERTY() TObjectPtr<AActor> TargetActor;
    float Damage = 0.f;
    EARPGBasicAttackType AttackType = EARPGBasicAttackType::Ranged;
    bool bCanBeBlocked = true;
    bool bCanBeParried = false;
    bool bCritical = false;

    UFUNCTION() void HandleProjectileHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent, FVector NormalImpulse, const FHitResult& Hit);
};
