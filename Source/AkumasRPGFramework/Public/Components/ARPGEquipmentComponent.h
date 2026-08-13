#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "GameplayEffectTypes.h"
#include "Equipment/ARPGEquipmentVisualActor.h"
#include "ARPGEquipmentComponent.generated.h"

class UARPGItemDefinition;
struct FARPGInventoryEntry;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FARPGEquipmentChanged, FGameplayTag, Slot, FGuid, ItemInstanceId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FARPGEquipmentRequestResult, bool, bSuccess, FGuid, ItemInstanceId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FARPGEquipmentVisualChanged, FGuid, ItemInstanceId, FGameplayTag, Slot, AARPGEquipmentVisualActor*, VisualActor);

UCLASS(ClassGroup=(ARPG), meta=(BlueprintSpawnableComponent))
class AKUMASRPGFRAMEWORK_API UARPGEquipmentComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UARPGEquipmentComponent();

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Equipment|Visuals", meta=(DisplayName="Automatically Create Equipment Visuals")) bool bAutoCreateEquipmentVisuals = true;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Equipment|Visuals", meta=(DisplayName="Attach To Character Skeletal Mesh")) bool bAttachToCharacterMesh = true;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Equipment|Visuals") FName DefaultAttachSocket = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Equipment|Visuals", meta=(DisplayName="Auto Find Fallback Hand Socket")) bool bAutoFindFallbackHandSocket = true;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Equipment|Visuals", meta=(EditCondition="bAutoFindFallbackHandSocket")) TArray<FName> FallbackHandSockets;

    UPROPERTY(BlueprintAssignable) FARPGEquipmentChanged OnEquipmentChanged;
    UPROPERTY(BlueprintAssignable) FARPGEquipmentRequestResult OnEquipmentRequestResult;
    UPROPERTY(BlueprintAssignable, Category="Equipment|Visuals") FARPGEquipmentVisualChanged OnEquipmentVisualChanged;

    UFUNCTION(BlueprintCallable, Category="ARPG|Equipment") bool EquipItem(FGuid ItemInstanceId);
    UFUNCTION(BlueprintCallable, Category="ARPG|Equipment") bool UnequipItem(FGuid ItemInstanceId);
    UFUNCTION(BlueprintCallable, Category="ARPG|Equipment", meta=(BlueprintAuthorityOnly)) void RefreshEquipmentEffects();
    UFUNCTION(BlueprintCallable, Category="ARPG|Equipment|Visuals") void RefreshEquipmentVisuals();
    UFUNCTION(BlueprintPure, Category="ARPG|Equipment") FGuid GetEquippedItemInSlot(FGameplayTag Slot) const;
    UFUNCTION(BlueprintPure, Category="ARPG|Equipment") UARPGItemDefinition* GetEquippedItemDefinitionInSlot(FGameplayTag Slot) const;
    UFUNCTION(BlueprintPure, Category="ARPG|Equipment|Visuals") AARPGEquipmentVisualActor* GetEquipmentVisual(FGuid ItemInstanceId) const;

    // Returns true when an equipped item supplied a sound, allowing combat to fall back to its class/profile audio otherwise.
    bool PlayEquippedCombatSwingSoundLocal();

    /** Authority-only wear hook used after a successful combat hit. Prefers the active equipped Quick Access item. */
    UFUNCTION(BlueprintCallable, Category="ARPG|Equipment|Durability", meta=(BlueprintAuthorityOnly))
    bool ApplyCombatDurabilityWear(float WearMultiplier = 1.f);

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
protected:
    UFUNCTION(Server, Reliable) void ServerEquipItem(FGuid ItemInstanceId);
    UFUNCTION(Server, Reliable) void ServerUnequipItem(FGuid ItemInstanceId);
    UFUNCTION(NetMulticast, Unreliable) void MulticastPlayEquipmentPresentation(UARPGItemDefinition* Definition, bool bEquipping);
    UFUNCTION() void HandleInventoryChanged();

    bool EquipAuthority(FGuid ItemInstanceId);
    bool UnequipAuthority(FGuid ItemInstanceId);
    AARPGEquipmentVisualActor* CreateEquipmentVisual(FGuid ItemInstanceId, const UARPGItemDefinition* Definition, FGameplayTag Slot);
    void DestroyEquipmentVisual(FGuid ItemInstanceId, FGameplayTag Slot=FGameplayTag());
    UARPGItemDefinition* ResolveItemDefinition(FName ItemId) const;
    UARPGItemDefinition* ResolveItemDefinition(const FARPGInventoryEntry& Entry) const;
    bool IsValidEquippedEntry(const FARPGInventoryEntry& Entry, const UARPGItemDefinition* Definition) const;
    bool HasEquipmentVisualIntent(const UARPGItemDefinition* Definition) const;
    bool SharesExclusiveVisualAttachment(const UARPGItemDefinition* A, const UARPGItemDefinition* B) const;
    bool RepairExclusiveVisualAttachmentStateAuthority();
    FName ResolveAttachSocket(const UARPGItemDefinition* Definition, const class USkeletalMeshComponent* CharacterMesh) const;
    void PlayEquipmentPresentationLocal(const UARPGItemDefinition* Definition, bool bEquipping) const;

    TMap<FGuid, FActiveGameplayEffectHandle> ActiveEquipmentEffects;
    TMap<FGuid, TWeakObjectPtr<AARPGEquipmentVisualActor>> EquipmentVisualActors;
    bool bRepairingExclusiveVisualState = false;
};
