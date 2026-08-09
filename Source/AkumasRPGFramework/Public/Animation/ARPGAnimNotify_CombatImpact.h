#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "ARPGAnimNotify_CombatImpact.generated.h"

UCLASS(meta=(DisplayName="ARPG Combat Impact"))
class AKUMASRPGFRAMEWORK_API UARPGAnimNotify_CombatImpact : public UAnimNotify
{
    GENERATED_BODY()
public:
    virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
};
