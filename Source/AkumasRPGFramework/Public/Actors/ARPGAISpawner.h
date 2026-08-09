#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ARPGTypes.h"
#include "ARPGAISpawner.generated.h"

class APawn;

USTRUCT(BlueprintType)
struct AKUMASRPGFRAMEWORK_API FARPGSpawnEntry
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TSubclassOf<APawn> PawnClass;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0.0")) float Weight = 1.f;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FARPGOnSpawnedAI, APawn*, Pawn);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FARPGOnSpawnGroupDefeated);

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

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Leash") bool bStayInRange = true;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Leash", meta=(ClampMin="100")) float MaxLeashDistance = 2500.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Leash", meta=(ClampMin="0.1")) float LeashCheckInterval = 1.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Leash") bool bTeleportHomeIfOverDoubleRange = true;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Runtime") TArray<TObjectPtr<APawn>> SpawnedPawns;
    UPROPERTY(BlueprintAssignable) FARPGOnSpawnedAI OnSpawnedAI;
    UPROPERTY(BlueprintAssignable) FARPGOnSpawnGroupDefeated OnSpawnGroupDefeated;

    UFUNCTION(BlueprintCallable, Category="ARPG|AI Spawner", meta=(BlueprintAuthorityOnly)) void SpawnGroup();
    UFUNCTION(BlueprintCallable, Category="ARPG|AI Spawner", meta=(BlueprintAuthorityOnly)) APawn* SpawnOne();
    UFUNCTION(BlueprintCallable, Category="ARPG|AI Spawner", meta=(BlueprintAuthorityOnly)) void DespawnAll(bool bDestroyActors=true);
    UFUNCTION(BlueprintPure, Category="ARPG|AI Spawner") int32 GetAliveCount() const;

    virtual void BeginPlay() override;
protected:
    UPROPERTY(VisibleAnywhere) int32 DesiredGroupSize = 0;
    FTimerHandle LeashTimer;
    FTimerHandle RespawnTimer;
    FVector ChooseSpawnLocation() const;
    TSubclassOf<APawn> ChoosePawnClass() const;
    void CheckLeashes();
    void ReplenishGroup();
    UFUNCTION() void HandlePawnDestroyed(AActor* DestroyedActor);
};
