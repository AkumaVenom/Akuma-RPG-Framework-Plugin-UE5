#include "Components/ARPGCombatComponent.h"
#include "Components/ARPGStatsComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Animation/AnimMontage.h"
#include "Net/UnrealNetwork.h"

UARPGCombatComponent::UARPGCombatComponent() { SetIsReplicatedByDefault(true); }

void UARPGCombatComponent::BeginPlay()
{
    Super::BeginPlay();
    if (UARPGStatsComponent* Stats = GetOwner() ? GetOwner()->FindComponentByClass<UARPGStatsComponent>() : nullptr)
        Stats->OnDeath.AddDynamic(this, &UARPGCombatComponent::HandleStatsDeath);
}

void UARPGCombatComponent::HandleStatsDeath() { if (GetOwner() && GetOwner()->HasAuthority()) Kill(); }

void UARPGCombatComponent::Kill()
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || LifeState == EARPGLifeState::Dead) return;
    LifeState = EARPGLifeState::Dead; OnLifeStateChanged.Broadcast(LifeState);
    if (ACharacter* Character = Cast<ACharacter>(GetOwner())) Character->GetCharacterMovement()->DisableMovement();
}

void UARPGCombatComponent::RespawnAtTransform(const FTransform& Transform)
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;
    LifeState = EARPGLifeState::Respawning; OnLifeStateChanged.Broadcast(LifeState);
    GetOwner()->SetActorTransform(Transform, false, nullptr, ETeleportType::TeleportPhysics);
    if (UARPGStatsComponent* Stats = GetOwner()->FindComponentByClass<UARPGStatsComponent>()) Stats->RestoreAllVitals();
    if (ACharacter* Character = Cast<ACharacter>(GetOwner())) Character->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    LifeState = EARPGLifeState::Alive; OnLifeStateChanged.Broadcast(LifeState);
}

UAnimMontage* UARPGCombatComponent::PickRandomAttackMontage(bool bMagic, bool bRanged) const
{
    const TArray<TSoftObjectPtr<UAnimMontage>>* Source = bMagic ? &Montages.MagicCasts : (bRanged ? &Montages.RangedAttacks : &Montages.MeleeAttacks);
    if (!Source || Source->Num() == 0) return nullptr;
    return (*Source)[FMath::RandRange(0, Source->Num()-1)].LoadSynchronous();
}

void UARPGCombatComponent::OnRep_LifeState(EARPGLifeState OldState) { OnLifeStateChanged.Broadcast(LifeState); }
void UARPGCombatComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps); DOREPLIFETIME(UARPGCombatComponent, LifeState);
}
