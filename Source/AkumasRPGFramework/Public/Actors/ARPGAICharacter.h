#pragma once

#include "CoreMinimal.h"
#include "Actors/ARPGCharacter.h"
#include "ARPGAICharacter.generated.h"

class UARPGAISplineComponent;
class UARPGWandererComponent;
class UARPGAISocialComponent;

UCLASS(BlueprintType, Blueprintable)
class AKUMASRPGFRAMEWORK_API AARPGAICharacter : public AARPGCharacter
{
    GENERATED_BODY()
public:
    AARPGAICharacter();

    /** Automatic NavMesh spline route follower. Assign a route and it works without Blueprint movement logic. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="ARPG") TObjectPtr<UARPGAISplineComponent> AISplineMovement;
    /** Lightweight NavMesh free-roam component used automatically by ARPG AI Spawner when Free Roam is selected. Disabled until requested. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="ARPG") TObjectPtr<UARPGWandererComponent> AIWanderer;
    /** Optional ambient NPC-to-NPC social behaviour. Disabled by default until enabled per NPC archetype. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="ARPG") TObjectPtr<UARPGAISocialComponent> AISocial;
};
