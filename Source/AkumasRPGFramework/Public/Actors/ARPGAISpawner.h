#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ARPGTypes.h"
#include "ARPGAISpawner.generated.h"

class APawn;
class AARPGAISplineRoute;
class UARPGWandererComponent;

USTRUCT(BlueprintType)
struct AKUMASRPGFRAMEWORK_API FARPGSpawnEntry
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TSubclassOf<APawn> PawnClass;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0.0")) float Weight = 1.f;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FARPGOnSpawnedAI, APawn*, Pawn);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FARPGOnSpawnGroupDefeated);

UENUM(BlueprintType)
enum class EARPGSpawnerMovementMode : uint8
{
    Automatic UMETA(DisplayName="Automatic (Spline If Assigned)"),
    SplineRoute UMETA(DisplayName="Spline Route"),
    FreeRoam UMETA(DisplayName="Free Roam"),
    HoldPosition UMETA(DisplayName="No Automatic Travel")
};

UENUM(BlueprintType)
enum class EARPGGroupSplineDirection : uint8
{
    Forward UMETA(DisplayName="Forward"),
    Reverse UMETA(DisplayName="Reverse"),
    RandomPerGroup UMETA(DisplayName="Random Per Spawn Group")
};

UCLASS(BlueprintType, Blueprintable)
class AKUMASRPGFRAMEWORK_API AARPGAISpawner : public AActor
{
    GENERATED_BODY()
public:
    AARPGAISpawner();

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spawn") TArray<FARPGSpawnEntry> SpawnTable;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spawn", meta=(ClampMin="0")) int32 MinGroupSize = 1;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spawn", meta=(ClampMin="0")) int32 MaxGroupSize = 1;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spawn") EARPGSpawnerShape SpawnShape = EARPGSpawnerShape::Circle;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spawn", meta=(ClampMin="0")) float SpawnRadius = 800.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spawn") FVector BoxExtents = FVector(800.f,800.f,200.f);
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spawn") bool bSpawnOnBeginPlay = true;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spawn") EARPGRespawnMode RespawnMode = EARPGRespawnMode::Individual;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spawn", meta=(ClampMin="0")) float RespawnDelay = 20.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spawn", meta=(ClampMin="0")) float CorpseDespawnDelay = 8.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Leash") bool bStayInRange = true;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Leash", meta=(ClampMin="100")) float MaxLeashDistance = 2500.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Leash", meta=(ClampMin="0.1")) float LeashCheckInterval = 1.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Leash") bool bTeleportHomeIfOverDoubleRange = true;

    /** Spawn membership and physical cohesion are independent. Disable this to keep group respawn/defeat semantics while allowing members to travel independently. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Group Cohesion", meta=(DisplayName="Stay Together")) bool bStayTogether = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Group Cohesion", meta=(ClampMin="100.0")) float GroupCohesionRadius = 2500.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Group Cohesion", meta=(ClampMin="0.1")) float GroupCohesionCheckInterval = 0.75f;
    /** Keep spline members moving in the same direction even when physical Stay Together cohesion is disabled. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Group Cohesion") bool bSynchronizeSplineGroupDirection = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Group Cohesion") EARPGGroupSplineDirection GroupSplineDirection = EARPGGroupSplineDirection::Forward;

    /** Select how this spawner owns non-combat movement. Automatic preserves old behavior: use a spline when one is assigned, otherwise no travel. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Movement") EARPGSpawnerMovementMode MovementMode = EARPGSpawnerMovementMode::Automatic;

    UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Spline Route") TObjectPtr<AARPGAISplineRoute> AssignedSplineRoute;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spline Route") bool bAutoStartAssignedSplineRoute = true;
    /** When a spawned pawn is actively following the assigned route, let the route/combat-local leash own movement instead of dragging it back to the spawner. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spline Route") bool bRouteOverridesSpawnerLeash = true;

    /** Free-roam uses normal NavMesh random reachable points and never attaches/transforms a pawn. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Free Roam", meta=(ClampMin="100.0")) float FreeRoamRadius = 1800.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Free Roam", meta=(ClampMin="0.25")) float FreeRoamThinkInterval = 4.f;
    /** Recommended: each independent roamer remains centered on this spawner's location. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Free Roam") bool bFreeRoamLeashedToSpawner = true;
    /** When Stay Together is enabled, followers recover toward the current group leader before resuming random roaming. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Free Roam", meta=(ClampMin="0.1", ClampMax="0.95")) float GroupRecoveryFraction = 0.70f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Runtime") TArray<TObjectPtr<APawn>> SpawnedPawns;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Runtime") TObjectPtr<APawn> GroupLeader;
    UPROPERTY(BlueprintAssignable) FARPGOnSpawnedAI OnSpawnedAI;
    UPROPERTY(BlueprintAssignable) FARPGOnSpawnGroupDefeated OnSpawnGroupDefeated;

    UFUNCTION(BlueprintCallable, Category="ARPG|AI Spawner", meta=(BlueprintAuthorityOnly)) void SpawnGroup();
    UFUNCTION(BlueprintCallable, Category="ARPG|AI Spawner", meta=(BlueprintAuthorityOnly)) APawn* SpawnOne();
    UFUNCTION(BlueprintCallable, Category="ARPG|AI Spawner", meta=(BlueprintAuthorityOnly)) void DespawnAll(bool bDestroyActors=true);
    UFUNCTION(BlueprintCallable, Category="ARPG|AI Spawner", meta=(BlueprintAuthorityOnly)) void SetAssignedSplineRoute(AARPGAISplineRoute* NewRoute, bool bApplyToExisting = true);
    UFUNCTION(BlueprintCallable, Category="ARPG|AI Spawner", meta=(BlueprintAuthorityOnly)) void SetStayTogether(bool bNewStayTogether);
    UFUNCTION(BlueprintCallable, Category="ARPG|AI Spawner", meta=(BlueprintAuthorityOnly)) void SetMovementMode(EARPGSpawnerMovementMode NewMode, bool bApplyToExisting = true);
    UFUNCTION(BlueprintPure, Category="ARPG|AI Spawner") int32 GetAliveCount() const;

    virtual void BeginPlay() override;
protected:
    UPROPERTY(VisibleAnywhere) int32 DesiredGroupSize = 0;
    FTimerHandle LeashTimer;
    FTimerHandle GroupCohesionTimer;
    FTimerHandle RespawnTimer;
    TSet<TWeakObjectPtr<APawn>> CohesionRecoveringPawns;
    int32 CurrentGroupSplineDirectionSign = 1;
    FVector ChooseSpawnLocation() const;
    TSubclassOf<APawn> ChoosePawnClass() const;
    void CheckLeashes();
    void CheckGroupCohesion();
    void ReplenishGroup();
    void ConfigureSpawnedPawn(APawn* Pawn);
    void ConfigureAllSpawnedPawns();
    void RefreshGroupLeader();
    void RefreshSplineGroupDirectionLeaders();
    void ConfigureFreeRoamPawn(APawn* Pawn, bool bIsLeader);
    EARPGSpawnerMovementMode GetEffectiveMovementMode() const;
    bool IsPawnAlive(const APawn* Pawn) const;
    bool IsPawnInCombat(const APawn* Pawn) const;
    UARPGWandererComponent* GetOrCreateWanderer(APawn* Pawn) const;
    int32 ChooseGroupSplineDirectionSign() const;
    UFUNCTION() void HandlePawnDestroyed(AActor* DestroyedActor);
};
