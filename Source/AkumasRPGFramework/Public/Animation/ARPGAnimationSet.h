#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Animation/AnimMontage.h"
#include "ARPGAnimationSet.generated.h"
UCLASS(BlueprintType) class AKUMASRPGFRAMEWORK_API UARPGAnimationSet:public UPrimaryDataAsset { GENERATED_BODY() public:
UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Combat") TArray<TObjectPtr<UAnimMontage>> MeleeAttackMontages;
UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Combat") TArray<TObjectPtr<UAnimMontage>> RangedAttackMontages;
UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Combat") TArray<TObjectPtr<UAnimMontage>> MagicCastMontages;
UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Combat") TObjectPtr<UAnimMontage> HitReactMontage=nullptr;
UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Life") TObjectPtr<UAnimMontage> DeathMontage=nullptr;
UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Life") TObjectPtr<UAnimMontage> RespawnMontage=nullptr;
UPROPERTY(EditAnywhere,BlueprintReadOnly,Category="Interaction") TObjectPtr<UAnimMontage> InteractMontage=nullptr;
virtual FPrimaryAssetId GetPrimaryAssetId() const override { return FPrimaryAssetId(TEXT("ARPGAnimationSet"),GetFName()); }
};
