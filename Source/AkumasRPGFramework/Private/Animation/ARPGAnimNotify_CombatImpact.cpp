#include "Animation/ARPGAnimNotify_CombatImpact.h"

#include "Components/ARPGCombatComponent.h"
#include "Components/SkeletalMeshComponent.h"

void UARPGAnimNotify_CombatImpact::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
    Super::Notify(MeshComp, Animation, EventReference);
    if (!MeshComp) return;
    AActor* Owner = MeshComp->GetOwner();
    if (!Owner || !Owner->HasAuthority()) return;
    if (UARPGCombatComponent* Combat = Owner->FindComponentByClass<UARPGCombatComponent>())
        Combat->NotifyAttackImpact();
}
