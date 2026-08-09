#include "Abilities/ARPGGameplayAbility.h"

#include "Components/ARPGTargetingComponent.h"

UARPGGameplayAbility::UARPGGameplayAbility()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

AActor* UARPGGameplayAbility::GetLockOnTarget() const
{
    const FGameplayAbilityActorInfo* Info = GetCurrentActorInfo();
    AActor* Avatar = Info ? Info->AvatarActor.Get() : nullptr;
    if (!Avatar) return nullptr;
    if (const UARPGTargetingComponent* Targeting = Avatar->FindComponentByClass<UARPGTargetingComponent>())
        return Targeting->GetCurrentTarget();
    return nullptr;
}

bool UARPGGameplayAbility::HasValidLockOnTarget() const
{
    AActor* Target = GetLockOnTarget();
    if (!Target) return false;
    const FGameplayAbilityActorInfo* Info = GetCurrentActorInfo();
    AActor* Avatar = Info ? Info->AvatarActor.Get() : nullptr;
    if (!Avatar) return false;
    if (MaximumLockOnTargetRange > 0.f && FVector::DistSquared(Avatar->GetActorLocation(), Target->GetActorLocation()) > FMath::Square(MaximumLockOnTargetRange))
        return false;
    if (const UARPGTargetingComponent* Targeting = Avatar->FindComponentByClass<UARPGTargetingComponent>())
        return Targeting->IsValidLockOnTarget(Target);
    return false;
}

FVector UARPGGameplayAbility::GetLockOnTargetLocation() const
{
    AActor* Target = GetLockOnTarget();
    if (!Target) return FVector::ZeroVector;
    const FGameplayAbilityActorInfo* Info = GetCurrentActorInfo();
    AActor* Avatar = Info ? Info->AvatarActor.Get() : nullptr;
    if (Avatar)
        if (const UARPGTargetingComponent* Targeting = Avatar->FindComponentByClass<UARPGTargetingComponent>())
            return Targeting->GetTargetAimLocation(Target);
    return Target->GetActorLocation();
}

FGameplayAbilityTargetDataHandle UARPGGameplayAbility::MakeLockOnTargetData() const
{
    FGameplayAbilityTargetDataHandle Handle;
    AActor* Target = GetLockOnTarget();
    if (!Target) return Handle;
    FGameplayAbilityTargetData_ActorArray* Data = new FGameplayAbilityTargetData_ActorArray();
    Data->TargetActorArray.Add(Target);
    Handle.Add(Data);
    return Handle;
}

bool UARPGGameplayAbility::CanActivateAbility(const FGameplayAbilitySpecHandle Handle,
                                               const FGameplayAbilityActorInfo* ActorInfo,
                                               const FGameplayTagContainer* SourceTags,
                                               const FGameplayTagContainer* TargetTags,
                                               FGameplayTagContainer* OptionalRelevantTags) const
{
    if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags)) return false;
    if (TargetingPolicy != EARPGAbilityTargetingPolicy::RequireLockOn) return true;
    if (!ActorInfo || !ActorInfo->AvatarActor.IsValid()) return false;
    AActor* Avatar = ActorInfo->AvatarActor.Get();
    const UARPGTargetingComponent* Targeting = Avatar->FindComponentByClass<UARPGTargetingComponent>();
    if (!Targeting) return false;
    AActor* Target = Targeting->GetCurrentTarget();
    if (!Target || !Targeting->IsValidLockOnTarget(Target)) return false;
    if (MaximumLockOnTargetRange > 0.f && FVector::DistSquared(Avatar->GetActorLocation(), Target->GetActorLocation()) > FMath::Square(MaximumLockOnTargetRange))
        return false;
    return true;
}
