#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ARPGTypes.h"
#include "ARPGGroupComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FARPGGroupChanged);

UCLASS(ClassGroup=(ARPG), meta=(BlueprintSpawnableComponent))
class AKUMASRPGFRAMEWORK_API UARPGGroupComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UARPGGroupComponent();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, ReplicatedUsing=OnRep_Group, SaveGame, Category="Group") FARPGGroupMembership Membership;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, ReplicatedUsing=OnRep_Group, SaveGame, Category="Social") FName GuildId = NAME_None;
    UPROPERTY(BlueprintAssignable) FARPGGroupChanged OnGroupChanged;

    UFUNCTION(BlueprintCallable, Category="ARPG|Group", meta=(BlueprintAuthorityOnly)) void SetMembership(const FARPGGroupMembership& NewMembership);
    UFUNCTION(BlueprintCallable, Category="ARPG|Group", meta=(BlueprintAuthorityOnly)) void ClearGroup();
    UFUNCTION(BlueprintCallable, Category="ARPG|Group", meta=(BlueprintAuthorityOnly)) void SetGuild(FName NewGuildId);
    UFUNCTION(BlueprintPure, Category="ARPG|Group") bool IsInSameGroup(const UARPGGroupComponent* Other, bool bRequireRaidMatch=false) const;

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
protected:
    UFUNCTION() void OnRep_Group();
};
