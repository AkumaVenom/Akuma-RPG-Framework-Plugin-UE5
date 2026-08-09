#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ARPGAssetLibrary.generated.h"

class UARPGDefinitionBase;

UCLASS()
class AKUMASRPGFRAMEWORK_API UARPGAssetLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="ARPG|Data", meta=(DeterminesOutputType="DefinitionClass"))
    static UARPGDefinitionBase* ResolveDefinitionById(TSubclassOf<UARPGDefinitionBase> DefinitionClass, FName DefinitionId);
};
