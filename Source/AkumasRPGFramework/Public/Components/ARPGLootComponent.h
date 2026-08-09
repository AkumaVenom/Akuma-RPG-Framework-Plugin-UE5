#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ARPGLootComponent.generated.h"

class UARPGLootTableDefinition;
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FARPGLootGranted, AActor*, Recipient, FName, LootTableId);

UCLASS(ClassGroup=(ARPG), meta=(BlueprintSpawnableComponent))
class AKUMASRPGFRAMEWORK_API UARPGLootComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Loot") TObjectPtr<UARPGLootTableDefinition> LootTable;
    UPROPERTY(BlueprintAssignable) FARPGLootGranted OnLootGranted;
    UFUNCTION(BlueprintCallable, Category="ARPG|Loot", meta=(BlueprintAuthorityOnly)) bool GrantLootTo(AActor* Recipient);
};
