#include "Actors/ARPGAICharacter.h"
#include "AIController.h"
#include "Components/ARPGAICombatComponent.h"
#include "Components/ARPGAISplineComponent.h"
#include "Components/ARPGCombatComponent.h"
#include "Components/ARPGWandererComponent.h"

AARPGAICharacter::AARPGAICharacter()
{
    AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
    AIControllerClass = AAIController::StaticClass();
    AISplineMovement = CreateDefaultSubobject<UARPGAISplineComponent>(TEXT("AISplineMovement"));
    AIWanderer = CreateDefaultSubobject<UARPGWandererComponent>(TEXT("AIWanderer"));
    AIWanderer->bEnabled = false; // Spawner/free-roam mode enables it only when requested.
    AIWanderer->bStayNearHome = true;
    if (AICombat) AICombat->bEnabled = true;
    if (Combat)
    {
        // NPC corpses use physical ragdoll automatically. A configured death montage is only
        // used when the mesh cannot ragdoll (for example, no Physics Asset is assigned).
        Combat->DeathPresentation.bUseRagdollOnDeath = true;
        Combat->DeathPresentation.bFallbackToDeathMontage = true;
    }
}
