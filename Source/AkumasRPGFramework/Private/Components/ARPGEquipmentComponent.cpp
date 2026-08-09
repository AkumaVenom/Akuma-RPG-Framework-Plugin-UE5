#include "Components/ARPGEquipmentComponent.h"
#include "Components/ARPGInventoryComponent.h"
#include "Components/ARPGProgressionComponent.h"
#include "Components/ARPGClassComponent.h"
#include "Data/ARPGItemDefinition.h"
#include "Utilities/ARPGAssetLibrary.h"
#include "AbilitySystemInterface.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"

UARPGEquipmentComponent::UARPGEquipmentComponent(){SetIsReplicatedByDefault(true);}
void UARPGEquipmentComponent::BeginPlay()
{
    Super::BeginPlay();
    if(UARPGInventoryComponent* I=GetOwner()?GetOwner()->FindComponentByClass<UARPGInventoryComponent>():nullptr)I->OnInventoryChanged.AddDynamic(this,&UARPGEquipmentComponent::HandleInventoryChanged);
    if(GetOwner()&&GetOwner()->HasAuthority())RefreshEquipmentEffects();
}
void UARPGEquipmentComponent::HandleInventoryChanged(){if(GetOwner()&&GetOwner()->HasAuthority())RefreshEquipmentEffects();}
FGuid UARPGEquipmentComponent::GetEquippedItemInSlot(FGameplayTag Slot)const
{
    if(const UARPGInventoryComponent* I=GetOwner()?GetOwner()->FindComponentByClass<UARPGInventoryComponent>():nullptr)
        for(const FARPGInventoryEntry&E:I->Items)if(E.bEquipped&&E.EquipmentSlot==Slot)return E.InstanceId;
    return FGuid();
}
bool UARPGEquipmentComponent::EquipItem(FGuid Id){if(!GetOwner()||!Id.IsValid())return false;if(!GetOwner()->HasAuthority()){ServerEquipItem(Id);return true;}return EquipAuthority(Id);}
bool UARPGEquipmentComponent::UnequipItem(FGuid Id){if(!GetOwner()||!Id.IsValid())return false;if(!GetOwner()->HasAuthority()){ServerUnequipItem(Id);return true;}return UnequipAuthority(Id);}
bool UARPGEquipmentComponent::EquipAuthority(FGuid Id)
{
    UARPGInventoryComponent* I=GetOwner()?GetOwner()->FindComponentByClass<UARPGInventoryComponent>():nullptr;if(!I||!GetOwner()->HasAuthority())return false;
    FARPGInventoryEntry* Entry=I->Items.FindByPredicate([&](const FARPGInventoryEntry&E){return E.InstanceId==Id;});if(!Entry)return false;
    const UARPGItemDefinition* Def=Cast<UARPGItemDefinition>(UARPGAssetLibrary::ResolveDefinitionById(UARPGItemDefinition::StaticClass(),Entry->ItemId));
    if(!Def||!Def->bEquippable||!Def->EquipmentSlot.IsValid())return false;
    if(const UARPGProgressionComponent* P=GetOwner()->FindComponentByClass<UARPGProgressionComponent>())if(P->Level<Def->RequiredLevel)return false;
    if(!Def->RequiredClassId.IsNone()){const UARPGClassComponent*C=GetOwner()->FindComponentByClass<UARPGClassComponent>();if(!C||C->GetClassId()!=Def->RequiredClassId)return false;}
    const bool Ok=I->SetEquipped(Id,true,Def->EquipmentSlot);if(Ok){RefreshEquipmentEffects();OnEquipmentChanged.Broadcast(Def->EquipmentSlot,Id);}OnEquipmentRequestResult.Broadcast(Ok,Id);return Ok;
}
bool UARPGEquipmentComponent::UnequipAuthority(FGuid Id)
{
    UARPGInventoryComponent* I=GetOwner()?GetOwner()->FindComponentByClass<UARPGInventoryComponent>():nullptr;if(!I||!GetOwner()->HasAuthority())return false;
    FARPGInventoryEntry* Entry=I->Items.FindByPredicate([&](const FARPGInventoryEntry&E){return E.InstanceId==Id;});if(!Entry||!Entry->bEquipped)return false;const FGameplayTag Slot=Entry->EquipmentSlot;
    const bool Ok=I->SetEquipped(Id,false,FGameplayTag());if(Ok){RefreshEquipmentEffects();OnEquipmentChanged.Broadcast(Slot,FGuid());}OnEquipmentRequestResult.Broadcast(Ok,Id);return Ok;
}
void UARPGEquipmentComponent::RefreshEquipmentEffects()
{
    if(!GetOwner()||!GetOwner()->HasAuthority())return;
    IAbilitySystemInterface* ASI=Cast<IAbilitySystemInterface>(GetOwner());UAbilitySystemComponent* ASC=ASI?ASI->GetAbilitySystemComponent():nullptr;
    if(ASC)for(const TPair<FGuid,FActiveGameplayEffectHandle>&P:ActiveEquipmentEffects)if(P.Value.IsValid())ASC->RemoveActiveGameplayEffect(P.Value);
    ActiveEquipmentEffects.Reset();
    UARPGInventoryComponent* I=GetOwner()->FindComponentByClass<UARPGInventoryComponent>();if(!I||!ASC)return;
    for(const FARPGInventoryEntry&E:I->Items)
    {
        if(!E.bEquipped)continue;const UARPGItemDefinition* Def=Cast<UARPGItemDefinition>(UARPGAssetLibrary::ResolveDefinitionById(UARPGItemDefinition::StaticClass(),E.ItemId));if(!Def||!Def->EquippedGameplayEffect)continue;
        FGameplayEffectContextHandle Context=ASC->MakeEffectContext();Context.AddSourceObject(GetOwner());FGameplayEffectSpecHandle Spec=ASC->MakeOutgoingSpec(Def->EquippedGameplayEffect,1.f,Context);
        if(Spec.IsValid())ActiveEquipmentEffects.Add(E.InstanceId,ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get()));
    }
}
void UARPGEquipmentComponent::ServerEquipItem_Implementation(FGuid Id){EquipAuthority(Id);}
void UARPGEquipmentComponent::ServerUnequipItem_Implementation(FGuid Id){UnequipAuthority(Id);}
