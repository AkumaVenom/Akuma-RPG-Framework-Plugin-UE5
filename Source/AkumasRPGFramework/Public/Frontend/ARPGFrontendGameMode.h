#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "ARPGFrontendGameMode.generated.h"

/** Drop this GameMode onto a blank Main Menu level; its controller creates the native/reskinnable frontend UI. */
UCLASS(BlueprintType, Blueprintable)
class AKUMASRPGFRAMEWORK_API AARPGFrontendGameMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    AARPGFrontendGameMode();

protected:
    virtual void BeginPlay() override;

private:
    void RecoverDestinationAuthoredGameModeIfNeeded();
};
