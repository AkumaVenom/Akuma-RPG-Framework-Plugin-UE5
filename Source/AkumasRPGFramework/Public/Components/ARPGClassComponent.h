#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ARPGClassComponent.generated.h"

class UARPGClassDefinition;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FARPGOnClassChanged, FName, ClassId);

UCLASS(ClassGroup=(ARPG), meta=(BlueprintSpawnableComponent))
class AKUMASRPGFRAMEWORK_API UARPGClassComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UARPGClassComponent();

    UPROPERTY(EditAnywhere, BlueprintReadOnly, ReplicatedUsing=OnRep_ClassDefinition, Category="Class") TObjectPtr<UARPGClassDefinition> ClassDefinition;
    UPROPERTY(BlueprintAssignable) FARPGOnClassChanged OnClassChanged;

    UFUNCTION(BlueprintCallable, Category="ARPG|Class", meta=(BlueprintAuthorityOnly)) bool ApplyClassDefinition(UARPGClassDefinition* NewClass, bool bGrantAbilities=true);
    UFUNCTION(BlueprintPure, Category="ARPG|Class") FName GetClassId() const;

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
protected:
    UFUNCTION() void OnRep_ClassDefinition();
};
