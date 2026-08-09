#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ARPGTypes.h"
#include "ARPGBuildingComponent.generated.h"

class UARPGBuildPieceDefinition;
class AARPGBuildPieceActor;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FARPGBuildPlacementResult, EARPGPlacementResult, Result, AARPGBuildPieceActor*, PlacedActor);

UCLASS(ClassGroup=(ARPG), meta=(BlueprintSpawnableComponent))
class AKUMASRPGFRAMEWORK_API UARPGBuildingComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UARPGBuildingComponent();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Building", meta=(ClampMin="100.0")) float MaxPlacementDistance = 1200.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Building") TEnumAsByte<ECollisionChannel> PlacementCollisionChannel = ECC_WorldStatic;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Building") bool bConsumeResources = true;
    UPROPERTY(BlueprintAssignable) FARPGBuildPlacementResult OnPlacementResult;

    UFUNCTION(BlueprintPure, Category="ARPG|Building") FTransform SnapTransform(const UARPGBuildPieceDefinition* Piece, const FTransform& DesiredTransform) const;
    UFUNCTION(BlueprintCallable, Category="ARPG|Building") EARPGPlacementResult EvaluatePlacement(const UARPGBuildPieceDefinition* Piece, const FTransform& DesiredTransform) const;
    UFUNCTION(BlueprintCallable, Category="ARPG|Building") bool RequestPlacePiece(UARPGBuildPieceDefinition* Piece, const FTransform& DesiredTransform);

protected:
    UFUNCTION(Server, Reliable) void ServerPlacePiece(UARPGBuildPieceDefinition* Piece, FTransform DesiredTransform);
    bool PlacePieceAuthority(UARPGBuildPieceDefinition* Piece, const FTransform& DesiredTransform);
    bool HasBuildResources(const UARPGBuildPieceDefinition* Piece) const;
    bool ConsumeBuildResources(const UARPGBuildPieceDefinition* Piece);
};
