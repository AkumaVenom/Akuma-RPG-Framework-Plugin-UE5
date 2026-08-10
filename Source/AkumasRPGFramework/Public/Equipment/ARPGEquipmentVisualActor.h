#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ARPGEquipmentVisualActor.generated.h"

class USceneComponent;
class UStaticMeshComponent;
class USkeletalMeshComponent;
class UARPGItemDefinition;

UCLASS(BlueprintType, Blueprintable)
class AKUMASRPGFRAMEWORK_API AARPGEquipmentVisualActor : public AActor
{
    GENERATED_BODY()
public:
    AARPGEquipmentVisualActor();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Equipment Visual") TObjectPtr<USceneComponent> Root;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Equipment Visual") TObjectPtr<UStaticMeshComponent> StaticMesh;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Equipment Visual") TObjectPtr<USkeletalMeshComponent> SkeletalMesh;

    UFUNCTION(BlueprintCallable, Category="ARPG|Equipment|Visual") virtual void ConfigureFromItem(const UARPGItemDefinition* ItemDefinition);
    UFUNCTION(BlueprintImplementableEvent, Category="ARPG|Equipment|Visual") void OnEquipmentVisualConfigured(const UARPGItemDefinition* ItemDefinition);
};
