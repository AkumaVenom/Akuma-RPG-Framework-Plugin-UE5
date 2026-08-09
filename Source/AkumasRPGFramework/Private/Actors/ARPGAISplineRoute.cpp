#include "Actors/ARPGAISplineRoute.h"

#include "Components/SceneComponent.h"
#include "Components/SplineComponent.h"

AARPGAISplineRoute::AARPGAISplineRoute()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = false;

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(SceneRoot);

    Spline = CreateDefaultSubobject<USplineComponent>(TEXT("RouteSpline"));
    Spline->SetupAttachment(SceneRoot);
    Spline->SetClosedLoop(false, false);
}

void AARPGAISplineRoute::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    if (!Spline) return;
    Spline->SetClosedLoop(bClosedLoop, true);
    PointSettings.SetNum(FMath::Max(0, Spline->GetNumberOfSplinePoints()));
}

float AARPGAISplineRoute::GetRouteLength() const
{
    return Spline ? Spline->GetSplineLength() : 0.f;
}

int32 AARPGAISplineRoute::GetRoutePointCount() const
{
    return Spline ? Spline->GetNumberOfSplinePoints() : 0;
}

float AARPGAISplineRoute::GetDistanceAtRoutePoint(int32 PointIndex) const
{
    if (!Spline || PointIndex < 0 || PointIndex >= Spline->GetNumberOfSplinePoints()) return 0.f;
    return Spline->GetDistanceAlongSplineAtSplinePoint(PointIndex);
}

FVector AARPGAISplineRoute::GetLocationAtRouteDistance(float Distance) const
{
    if (!Spline) return GetActorLocation();
    const float Length = Spline->GetSplineLength();
    return Spline->GetLocationAtDistanceAlongSpline(FMath::Clamp(Distance, 0.f, Length), ESplineCoordinateSpace::World);
}

FVector AARPGAISplineRoute::GetDirectionAtRouteDistance(float Distance) const
{
    if (!Spline) return GetActorForwardVector();
    const float Length = Spline->GetSplineLength();
    return Spline->GetDirectionAtDistanceAlongSpline(FMath::Clamp(Distance, 0.f, Length), ESplineCoordinateSpace::World);
}

float AARPGAISplineRoute::FindDistanceClosestToWorldLocation(const FVector& WorldLocation) const
{
    return Spline ? Spline->GetDistanceAlongSplineAtLocation(WorldLocation, ESplineCoordinateSpace::World) : 0.f;
}

FARPGAISplinePointSettings AARPGAISplineRoute::GetRoutePointSettings(int32 PointIndex) const
{
    return PointSettings.IsValidIndex(PointIndex) ? PointSettings[PointIndex] : FARPGAISplinePointSettings();
}
