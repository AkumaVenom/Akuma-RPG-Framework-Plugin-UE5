#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "ARPGDefinitionBase.generated.h"

UCLASS(Abstract, BlueprintType)
class AKUMASRPGFRAMEWORK_API UARPGDefinitionBase : public UPrimaryDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Identity") FName DefinitionId = NAME_None;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Identity") FText DisplayName;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Identity", meta=(MultiLine=true)) FText Description;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Identity") TSoftObjectPtr<UTexture2D> Icon;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Tags") FGameplayTagContainer Tags;

    virtual FPrimaryAssetId GetPrimaryAssetId() const override
    {
        const FName StableName = DefinitionId.IsNone() ? GetFName() : DefinitionId;
        return FPrimaryAssetId(GetClass()->GetFName(), StableName);
    }
};
