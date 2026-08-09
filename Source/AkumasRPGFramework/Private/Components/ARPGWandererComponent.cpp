#include "Components/ARPGWandererComponent.h"
#include "AIController.h"
#include "NavigationSystem.h"
#include "GameFramework/Pawn.h"
#include "TimerManager.h"

UARPGWandererComponent::UARPGWandererComponent() { PrimaryComponentTick.bCanEverTick = false; }

void UARPGWandererComponent::BeginPlay()
{
    Super::BeginPlay();
    HomeLocation = GetOwner() ? GetOwner()->GetActorLocation() : FVector::ZeroVector;
    if (GetWorld()) GetWorld()->GetTimerManager().SetTimer(ThinkTimer, this, &UARPGWandererComponent::Think, ThinkInterval, true, FMath::FRandRange(0.1f, ThinkInterval));
}

void UARPGWandererComponent::SetWandererEnabled(bool bNewEnabled) { bEnabled = bNewEnabled; }
void UARPGWandererComponent::ForceChooseNewDestination() { Think(); }

void UARPGWandererComponent::Think()
{
    if (!bEnabled || !GetOwner() || !GetOwner()->HasAuthority()) return;
    APawn* Pawn = Cast<APawn>(GetOwner()); AAIController* AI = Pawn ? Cast<AAIController>(Pawn->GetController()) : nullptr;
    UNavigationSystemV1* Nav = GetWorld() ? FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()) : nullptr;
    if (!AI || !Nav) return;
    const FVector Origin = bStayNearHome ? HomeLocation : Pawn->GetActorLocation();
    FNavLocation Dest;
    if (Nav->GetRandomReachablePointInRadius(Origin, WanderRadius, Dest)) AI->MoveToLocation(Dest.Location, 75.f, true, true, true, false, nullptr, true);
}
