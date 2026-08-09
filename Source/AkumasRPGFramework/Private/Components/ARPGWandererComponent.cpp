#include "Components/ARPGWandererComponent.h"

#include "AIController.h"
#include "Components/ARPGAICombatComponent.h"
#include "Components/ARPGCombatComponent.h"
#include "GameFramework/Pawn.h"
#include "NavigationSystem.h"
#include "TimerManager.h"

UARPGWandererComponent::UARPGWandererComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UARPGWandererComponent::BeginPlay()
{
    Super::BeginPlay();
    HomeLocation = GetOwner() ? GetOwner()->GetActorLocation() : FVector::ZeroVector;
    if (GetOwner() && GetOwner()->HasAuthority() && bEnabled)
        EnsureThinkTimer();
}

void UARPGWandererComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(ThinkTimer);
    Super::EndPlay(EndPlayReason);
}

void UARPGWandererComponent::EnsureThinkTimer()
{
    if (!GetWorld() || !bEnabled || GetWorld()->GetTimerManager().IsTimerActive(ThinkTimer)) return;
    const float Interval = FMath::Max(0.1f, ThinkInterval);
    GetWorld()->GetTimerManager().SetTimer(
        ThinkTimer,
        this,
        &UARPGWandererComponent::Think,
        Interval,
        true,
        FMath::FRandRange(0.05f, Interval));
}

void UARPGWandererComponent::SetWandererEnabled(bool bNewEnabled)
{
    bEnabled = bNewEnabled;
    if (!GetWorld()) return;

    if (bEnabled)
    {
        GetWorld()->GetTimerManager().ClearTimer(ThinkTimer);
        EnsureThinkTimer();
    }
    else
    {
        GetWorld()->GetTimerManager().ClearTimer(ThinkTimer);
        if (bStopMovementWhenDisabled)
        {
            if (APawn* Pawn = Cast<APawn>(GetOwner()))
                if (AAIController* AI = Cast<AAIController>(Pawn->GetController())) AI->StopMovement();
        }
    }
}

void UARPGWandererComponent::SetHomeLocation(const FVector& NewHomeLocation)
{
    HomeLocation = NewHomeLocation;
}

void UARPGWandererComponent::ForceChooseNewDestination()
{
    Think();
}

void UARPGWandererComponent::ForceReturnHome()
{
    if (!bEnabled || !GetOwner() || !GetOwner()->HasAuthority()) return;
    APawn* Pawn = Cast<APawn>(GetOwner());
    if (AAIController* AI = Pawn ? Cast<AAIController>(Pawn->GetController()) : nullptr)
        AI->MoveToLocation(HomeLocation, FMath::Max(5.f, AcceptanceRadius), true, true, true, false, nullptr, true);
}

void UARPGWandererComponent::Think()
{
    if (!bEnabled || !GetOwner() || !GetOwner()->HasAuthority()) return;

    if (const UARPGCombatComponent* Combat = GetOwner()->FindComponentByClass<UARPGCombatComponent>())
        if (!Combat->IsAlive()) return;

    if (bPauseDuringCombat)
    {
        if (const UARPGAICombatComponent* AICombat = GetOwner()->FindComponentByClass<UARPGAICombatComponent>())
            if (IsValid(AICombat->CurrentTarget)) return;
    }

    APawn* Pawn = Cast<APawn>(GetOwner());
    AAIController* AI = Pawn ? Cast<AAIController>(Pawn->GetController()) : nullptr;
    UNavigationSystemV1* Nav = GetWorld() ? FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()) : nullptr;
    if (!Pawn || !AI || !Nav) return;

    const float Radius = FMath::Max(100.f, WanderRadius);
    if (bStayNearHome && FVector::Dist2D(Pawn->GetActorLocation(), HomeLocation) > Radius)
    {
        AI->MoveToLocation(HomeLocation, FMath::Max(5.f, AcceptanceRadius), true, true, true, false, nullptr, true);
        return;
    }

    const FVector Origin = bStayNearHome ? HomeLocation : Pawn->GetActorLocation();
    FNavLocation Destination;
    if (Nav->GetRandomReachablePointInRadius(Origin, Radius, Destination))
    {
        AI->MoveToLocation(
            Destination.Location,
            FMath::Max(5.f, AcceptanceRadius),
            true,
            true,
            true,
            false,
            nullptr,
            true);
    }
}
