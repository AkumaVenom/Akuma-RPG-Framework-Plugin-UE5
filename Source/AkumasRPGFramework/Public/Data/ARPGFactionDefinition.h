#pragma once

#include "CoreMinimal.h"
#include "Data/ARPGDefinitionBase.h"
#include "ARPGFactionDefinition.generated.h"

USTRUCT(BlueprintType)
struct AKUMASRPGFRAMEWORK_API FARPGFactionRelationship
{
    GENERATED_BODY()
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) FName OtherFactionId = NAME_None;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly) int32 BaseRelationship = 0;
};

UCLASS(BlueprintType)
class AKUMASRPGFRAMEWORK_API UARPGFactionDefinition : public UARPGDefinitionBase
{
    GENERATED_BODY()
public:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Faction") TArray<FARPGFactionRelationship> Relationships;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Faction") bool bAttackHostileOnSight = true;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Faction") int32 HatedThreshold = -42000;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Faction") int32 HostileThreshold = -6000;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Faction") int32 UnfriendlyThreshold = -3000;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Faction") int32 FriendlyThreshold = 3000;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Faction") int32 HonoredThreshold = 9000;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Faction") int32 ReveredThreshold = 21000;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Faction") int32 ExaltedThreshold = 42000;
};
