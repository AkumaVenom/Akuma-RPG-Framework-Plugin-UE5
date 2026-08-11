#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ARPGTypes.h"
#include "ARPGAISpawner.generated.h"

class APawn;
class AARPGAISplineRoute;
class AARPGDayNightCycle;
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
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FARPGOnSpawnerPopulationStateChanged);

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

    /**
     * Optional runtime population swap driven by ARPGDayNightCycle. Disabled preserves the original spawner
     * behavior exactly: Spawn Table remains the only population and distance unload/reload preserves it as before.
     * When enabled, Spawn Table is the daylight population; at midnight (00:00) the current population is removed
     * and Midnight Spawn Table is spawned. At the Day/Night actor's Day Start Hour, midnight NPCs are removed and
     * the daylight Spawn Table returns. Distance-streamed spawners update their phase while unloaded and spawn only
     * the correct phase when a player makes them relevant again.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spawn|Day Night Population", meta=(DisplayName="Enable Midnight Population Swap"))
    bool bEnableMidnightPopulationSwap = false;

    /** Optional explicit world clock. Leave unset to automatically use the first ARPGDayNightCycle in the world. */
    UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Spawn|Day Night Population", meta=(EditCondition="bEnableMidnightPopulationSwap"))
    TObjectPtr<AARPGDayNightCycle> DayNightCycleOverride;

    /** Weighted NPC classes used from 00:00 until the Day/Night actor reaches Day Start Hour. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spawn|Day Night Population", meta=(EditCondition="bEnableMidnightPopulationSwap", DisplayName="Midnight Spawn Table"))
    TArray<FARPGSpawnEntry> MidnightSpawnTable;

    /** Use a different group-size range for the midnight population. Otherwise Min/Max Group Size are shared. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spawn|Day Night Population", meta=(EditCondition="bEnableMidnightPopulationSwap"))
    bool bUseSeparateMidnightGroupSize = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spawn|Day Night Population", meta=(ClampMin="0", EditCondition="bEnableMidnightPopulationSwap && bUseSeparateMidnightGroupSize"))
    int32 MidnightMinGroupSize = 1;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spawn|Day Night Population", meta=(ClampMin="0", EditCondition="bEnableMidnightPopulationSwap && bUseSeparateMidnightGroupSize"))
    int32 MidnightMaxGroupSize = 1;

    /**
     * Performance streaming for spawned AI. When enabled, this spawner stays unloaded until a player-controlled pawn
     * enters Spawn Activation Radius, then unloads again only after every relevant player is beyond Despawn Radius.
     * Separate radii provide hysteresis so standing near the boundary cannot rapidly spawn/despawn the population.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Performance|Distance Population")
    bool bEnableDistanceBasedPopulation = true;

    /** Automatically load/spawn this population when a player enters Spawn Activation Radius. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Performance|Distance Population")
    bool bAutoSpawnWhenPlayerIsNear = true;

    /** Distance from the spawner at which an unloaded population becomes relevant and spawns. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Performance|Distance Population", meta=(ClampMin="100.0", Units="cm"))
    float SpawnActivationRadius = 6000.f;

    /** Distance at which an already-loaded population may unload. Keep this larger than Spawn Activation Radius. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Performance|Distance Population", meta=(ClampMin="100.0", Units="cm"))
    float DespawnRadius = 8000.f;

    /** How often this server-only spawner evaluates player relevance. No Actor Tick is used. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Performance|Distance Population", meta=(ClampMin="0.25", ClampMax="30.0", Units="s"))
    float PopulationCheckInterval = 1.25f;

    /** Player must remain outside the despawn radius for this long before the population is unloaded. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Performance|Distance Population", meta=(ClampMin="0.0", ClampMax="60.0", Units="s"))
    float DistanceDespawnDelay = 3.f;

    /** Ignore vertical separation when measuring relevance. Recommended for normal outdoor/open-world spawners. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Performance|Distance Population")
    bool bUse2DPlayerDistance = true;

    /**
     * Keep a loaded group relevant while a player is near one of its spawned NPCs, even if that NPC has travelled far
     * from the spawner on a spline/free-roam route. Inactive spawners still activate from the spawner origin.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Performance|Distance Population")
    bool bKeepLoadedNearSpawnedPawns = true;

    /** Optional persistence override. Disabled by default so leaving an area always wins for performance. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Performance|Distance Population")
    bool bPreventDistanceDespawnWhileInCombat = false;

    /** Re-roll Min/Max Group Size each time distance streaming reloads the group. Disabled preserves the previous desired count. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Performance|Distance Population")
    bool bRerollGroupSizeOnDistanceReload = false;

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
    /** True while this spawner's NPC population is currently loaded/relevant. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Runtime|Performance") bool bPopulationActive = false;
    /** Nearest relevant player distance from the spawner/population. -1 means no player-controlled pawn is currently available. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Runtime|Performance") float NearestRelevantPlayerDistance = -1.f;
    /** True while the spawner is currently selecting Midnight Spawn Table instead of the normal daylight Spawn Table. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Runtime|Day Night Population") bool bMidnightPopulationActive = false;
    UPROPERTY(BlueprintAssignable) FARPGOnSpawnedAI OnSpawnedAI;
    UPROPERTY(BlueprintAssignable) FARPGOnSpawnGroupDefeated OnSpawnGroupDefeated;
    UPROPERTY(BlueprintAssignable, Category="ARPG|AI Spawner|Performance") FARPGOnSpawnerPopulationStateChanged OnPopulationActivated;
    UPROPERTY(BlueprintAssignable, Category="ARPG|AI Spawner|Performance") FARPGOnSpawnerPopulationStateChanged OnPopulationDeactivated;

    UFUNCTION(BlueprintCallable, Category="ARPG|AI Spawner", meta=(BlueprintAuthorityOnly)) void SpawnGroup();
    UFUNCTION(BlueprintCallable, Category="ARPG|AI Spawner", meta=(BlueprintAuthorityOnly)) APawn* SpawnOne();
    UFUNCTION(BlueprintCallable, Category="ARPG|AI Spawner", meta=(BlueprintAuthorityOnly)) void DespawnAll(bool bDestroyActors=true);
    UFUNCTION(BlueprintCallable, Category="ARPG|AI Spawner", meta=(BlueprintAuthorityOnly)) void SetAssignedSplineRoute(AARPGAISplineRoute* NewRoute, bool bApplyToExisting = true);
    UFUNCTION(BlueprintCallable, Category="ARPG|AI Spawner", meta=(BlueprintAuthorityOnly)) void SetStayTogether(bool bNewStayTogether);
    UFUNCTION(BlueprintCallable, Category="ARPG|AI Spawner", meta=(BlueprintAuthorityOnly)) void SetMovementMode(EARPGSpawnerMovementMode NewMode, bool bApplyToExisting = true);
    UFUNCTION(BlueprintPure, Category="ARPG|AI Spawner") int32 GetAliveCount() const;
    UFUNCTION(BlueprintPure, Category="ARPG|AI Spawner|Performance") bool IsPopulationActive() const { return bPopulationActive; }
    UFUNCTION(BlueprintPure, Category="ARPG|AI Spawner|Performance") float GetNearestRelevantPlayerDistance() const { return NearestRelevantPlayerDistance; }
    UFUNCTION(BlueprintPure, Category="ARPG|AI Spawner|Performance") bool IsPlayerInsideSpawnActivationRadius() const;
    /** Immediately re-evaluate player proximity instead of waiting for the next staggered performance timer. */
    UFUNCTION(BlueprintCallable, Category="ARPG|AI Spawner|Performance", meta=(BlueprintAuthorityOnly)) void EvaluatePopulationRelevanceNow();
    /** Manually load/unload the distance-streamed population. Automatic distance checks continue afterward. */
    UFUNCTION(BlueprintCallable, Category="ARPG|AI Spawner|Performance", meta=(BlueprintAuthorityOnly)) void SetPopulationActive(bool bNewActive);
    /** Re-resolve the world clock and immediately synchronize this spawner to the correct daylight/midnight population. */
    UFUNCTION(BlueprintCallable, Category="ARPG|AI Spawner|Day Night Population", meta=(BlueprintAuthorityOnly)) void RefreshDayNightPopulationNow();
    UFUNCTION(BlueprintPure, Category="ARPG|AI Spawner|Day Night Population") bool IsMidnightPopulationActive() const { return bMidnightPopulationActive; }

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
protected:
    UPROPERTY(VisibleAnywhere) int32 DesiredGroupSize = 0;
    FTimerHandle LeashTimer;
    FTimerHandle GroupCohesionTimer;
    FTimerHandle RespawnTimer;
    FTimerHandle PopulationRelevanceTimer;
    TSet<TWeakObjectPtr<APawn>> CohesionRecoveringPawns;
    int32 CurrentGroupSplineDirectionSign = 1;
    int32 PreservedImmediatePopulationCount = 0;
    double OutOfRangeSinceTime = -1.0;
    double RespawnNotBeforeTime = 0.0;
    bool bWholeGroupRespawnPending = false;
    TWeakObjectPtr<AARPGDayNightCycle> ResolvedDayNightCycle;
    bool bWarnedMissingDayNightCycle = false;
    FVector ChooseSpawnLocation() const;
    TSubclassOf<APawn> ChoosePawnClass() const;
    const TArray<FARPGSpawnEntry>& GetActiveSpawnTable() const;
    void GetActiveGroupSizeRange(int32& OutMinGroupSize, int32& OutMaxGroupSize) const;
    void InitializeDayNightPopulation();
    void ShutdownDayNightPopulation();
    void ResolveAndBindDayNightCycle();
    bool IsWorldTimeInMidnightPopulationWindow() const;
    void CheckDayNightPopulationPhase();
    void ApplyDayNightPopulationPhase(bool bUseMidnightPopulation, bool bForceRefresh = false);
    void CheckLeashes();
    void CheckPopulationRelevance();
    void ActivateDistancePopulation();
    void DeactivateDistancePopulation();
    void StartActiveRuntimeTimers();
    void StopActiveRuntimeTimers();
    void SpawnUntilAliveCount(int32 TargetAliveCount);
    float FindNearestRelevantPlayerDistance(bool bIncludeSpawnedPawnAnchors) const;
    float GetEffectiveDespawnRadius() const;
    bool HasAnySpawnedPawnInCombat() const;
    void ResumePopulationAfterDistanceLoad();
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
    UFUNCTION() void HandleWorldHourChanged(int32 NewHour);
    UFUNCTION() void HandleDayStarted();
};
