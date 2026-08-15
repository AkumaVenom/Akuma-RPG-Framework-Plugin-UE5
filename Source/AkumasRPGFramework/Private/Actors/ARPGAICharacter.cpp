#include "Actors/ARPGAICharacter.h"
#include "AIController.h"
#include "Components/ARPGAICombatComponent.h"
#include "Components/ARPGAISplineComponent.h"
#include "Components/ARPGAISocialComponent.h"
#include "Components/ARPGCombatComponent.h"
#include "Components/ARPGPersistenceComponent.h"
#include "Components/ARPGWandererComponent.h"
#include "Components/ARPGSpawnEntranceComponent.h"

AARPGAICharacter::AARPGAICharacter()
{
    AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
    AIControllerClass = AAIController::StaticClass();
    AISplineMovement = CreateDefaultSubobject<UARPGAISplineComponent>(TEXT("AISplineMovement"));
    AIWanderer = CreateDefaultSubobject<UARPGWandererComponent>(TEXT("AIWanderer"));
    AISocial = CreateDefaultSubobject<UARPGAISocialComponent>(TEXT("AISocial"));
    SpawnEntrance = CreateDefaultSubobject<UARPGSpawnEntranceComponent>(TEXT("SpawnEntrance"));
    AIWanderer->bEnabled = false; // Spawner/free-roam mode enables it only when requested.
    AIWanderer->bStayNearHome = true;
    AISocial->bEnableSocialInteractions = false; // Explicit opt-in preserves every existing NPC archetype.
    if (AICombat) AICombat->bEnabled = true;
    if (Persistence)
    {
        // NPC state is owned by its spawner/world systems, not the player's account character slot.
        // Keep these defaults off even though the base playable character enables persistence. Runtime
        // guards in UARPGPersistenceComponent/UARPGSaveSubsystem provide a second safety layer.
        Persistence->bAutoLoadOnBeginPlay = false;
        Persistence->bAutoSave = false;
        Persistence->bSaveOnEndPlay = false;
    }
    if (Combat)
    {
        // NPC corpses use physical ragdoll automatically. A configured death montage is only
        // used when the mesh cannot ragdoll (for example, no Physics Asset is assigned).
        Combat->DeathPresentation.bUseRagdollOnDeath = true;
        Combat->DeathPresentation.bFallbackToDeathMontage = true;
    }
}
