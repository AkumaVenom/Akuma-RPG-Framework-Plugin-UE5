#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ARPGAISplineRoute.generated.h"

class USceneComponent;
class USplineComponent;

USTRUCT(BlueprintType)
struct AKUMASRPGFRAMEWORK_API FARPGAISplinePointSettings
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Route Point") FName PointId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Route Point", meta=(ClampMin="0.0")) float WaitTime = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Route Point", meta=(ClampMin="0.0")) float RandomWaitTime = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Route Point") bool bFaceSplineDirectionWhileWaiting = true;
};

UCLASS(BlueprintType, Blueprintable)
class AKUMASRPGFRAMEWORK_API AARPGAISplineRoute : public AActor
{
    GENERATED_BODY()
public:
    AARPGAISplineRoute();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="ARPG|AI Spline") TObjectPtr<USceneComponent> SceneRoot;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="ARPG|AI Spline") TObjectPtr<USplineComponent> Spline;

    /** Optional stable route name used by AI that auto-resolve a route instead of holding a level reference. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ARPG|AI Spline|Core") FName RouteId = NAME_None;

    /** Master route traversal switch. Enabled by default so patrol NPCs do not stop permanently at an endpoint. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ARPG|AI Spline|Traversal", meta=(DisplayName="Loop Route")) bool bLoopRoute = true;
    /** Makes the spline geometry itself connect the final point back to the first point. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ARPG|AI Spline|Traversal", meta=(DisplayName="Closed Loop Geometry")) bool bClosedLoop = false;
    /** For an open looping route, reverse the whole route direction at the ends. Disable to return to the opposite endpoint through NavMesh. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ARPG|AI Spline|Traversal", meta=(DisplayName="Reverse At Open Ends", EditCondition="bLoopRoute && !bClosedLoop")) bool bReverseAtOpenEnds = true;

    /** Per-control-point wait/event settings. The array is automatically sized to the spline point count. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ARPG|AI Spline|Points") TArray<FARPGAISplinePointSettings> PointSettings;

    UFUNCTION(BlueprintPure, Category="ARPG|AI Spline") float GetRouteLength() const;
    UFUNCTION(BlueprintPure, Category="ARPG|AI Spline") int32 GetRoutePointCount() const;
    UFUNCTION(BlueprintPure, Category="ARPG|AI Spline") float GetDistanceAtRoutePoint(int32 PointIndex) const;
    UFUNCTION(BlueprintPure, Category="ARPG|AI Spline") FVector GetLocationAtRouteDistance(float Distance) const;
    UFUNCTION(BlueprintPure, Category="ARPG|AI Spline") FVector GetDirectionAtRouteDistance(float Distance) const;
    UFUNCTION(BlueprintPure, Category="ARPG|AI Spline") float FindDistanceClosestToWorldLocation(const FVector& WorldLocation) const;
    UFUNCTION(BlueprintPure, Category="ARPG|AI Spline") FARPGAISplinePointSettings GetRoutePointSettings(int32 PointIndex) const;

    virtual void OnConstruction(const FTransform& Transform) override;
};
