#include "Components/ARPGEquipmentComponent.h"
#include "Components/ARPGInventoryComponent.h"
#include "Components/ARPGProgressionComponent.h"
#include "Components/ARPGClassComponent.h"
#include "Data/ARPGItemDefinition.h"
#include "Equipment/ARPGEquipmentVisualActor.h"
#include "Utilities/ARPGAssetLibrary.h"
#include "AkumasRPGFramework.h"
#include "AbilitySystemInterface.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Animation/AnimMontage.h"
#include "Sound/SoundBase.h"
#include "Engine/World.h"

UARPGEquipmentComponent::UARPGEquipmentComponent()
{
    SetIsReplicatedByDefault(true);
    PrimaryComponentTick.bCanEverTick = false;

    // Common UE character/marketplace socket conventions. Designers can override the exact item socket,
    // but a missing socket should not silently place the weapon at the character origin.
    FallbackHandSockets = {
        FName(TEXT("weapon_r")),
        FName(TEXT("WeaponSocket")),
        FName(TEXT("RightHandSocket")),
        FName(TEXT("hand_r"))
    };
}

void UARPGEquipmentComponent::BeginPlay()
{
    Super::BeginPlay();
    if (UARPGInventoryComponent* Inventory = GetOwner() ? GetOwner()->FindComponentByClass<UARPGInventoryComponent>() : nullptr)
        Inventory->OnInventoryChanged.AddDynamic(this, &UARPGEquipmentComponent::HandleInventoryChanged);
    if (GetOwner() && GetOwner()->HasAuthority()) RefreshEquipmentEffects();
    RefreshEquipmentVisuals();
}

void UARPGEquipmentComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (UARPGInventoryComponent* Inventory = GetOwner() ? GetOwner()->FindComponentByClass<UARPGInventoryComponent>() : nullptr)
        Inventory->OnInventoryChanged.RemoveDynamic(this, &UARPGEquipmentComponent::HandleInventoryChanged);

    TArray<FGuid> Keys;
    EquipmentVisualActors.GetKeys(Keys);
    for (const FGuid& Id : Keys) DestroyEquipmentVisual(Id);
    EquipmentVisualActors.Reset();
    Super::EndPlay(EndPlayReason);
}

UARPGItemDefinition* UARPGEquipmentComponent::ResolveItemDefinition(FName ItemId) const
{
    return ItemId.IsNone() ? nullptr : Cast<UARPGItemDefinition>(UARPGAssetLibrary::ResolveDefinitionById(UARPGItemDefinition::StaticClass(), ItemId));
}

UARPGItemDefinition* UARPGEquipmentComponent::ResolveItemDefinition(const FARPGInventoryEntry& Entry) const
{
    if (const UARPGInventoryComponent* Inventory = GetOwner() ? GetOwner()->FindComponentByClass<UARPGInventoryComponent>() : nullptr)
        if (UARPGItemDefinition* Exact = Inventory->ResolveItemDefinition(Entry)) return Exact;
    return ResolveItemDefinition(Entry.ItemId);
}

bool UARPGEquipmentComponent::IsValidEquippedEntry(const FARPGInventoryEntry& Entry, const UARPGItemDefinition* Definition) const
{
    if (!Entry.InstanceId.IsValid() || Entry.Quantity <= 0 || !Entry.bEquipped || !Entry.EquipmentSlot.IsValid()) return false;
    if (!Definition || !Definition->bEquippable || !Definition->EquipmentSlot.IsValid()) return false;
    return Entry.EquipmentSlot == Definition->EquipmentSlot;
}

void UARPGEquipmentComponent::HandleInventoryChanged()
{
    if (GetOwner() && GetOwner()->HasAuthority()) RefreshEquipmentEffects();
    RefreshEquipmentVisuals();
}

FGuid UARPGEquipmentComponent::GetEquippedItemInSlot(FGameplayTag Slot) const
{
    if (!Slot.IsValid()) return FGuid();
    if (const UARPGInventoryComponent* Inventory = GetOwner() ? GetOwner()->FindComponentByClass<UARPGInventoryComponent>() : nullptr)
    {
        for (const FARPGInventoryEntry& Entry : Inventory->Items)
        {
            UARPGItemDefinition* Definition = ResolveItemDefinition(Entry);
            if (Entry.EquipmentSlot == Slot && IsValidEquippedEntry(Entry, Definition)) return Entry.InstanceId;
        }
    }
    return FGuid();
}

UARPGItemDefinition* UARPGEquipmentComponent::GetEquippedItemDefinitionInSlot(FGameplayTag Slot) const
{
    if (!Slot.IsValid()) return nullptr;
    if (const UARPGInventoryComponent* Inventory = GetOwner() ? GetOwner()->FindComponentByClass<UARPGInventoryComponent>() : nullptr)
    {
        for (const FARPGInventoryEntry& Entry : Inventory->Items)
        {
            UARPGItemDefinition* Definition = ResolveItemDefinition(Entry);
            if (Entry.EquipmentSlot == Slot && IsValidEquippedEntry(Entry, Definition)) return Definition;
        }
    }
    return nullptr;
}

AARPGEquipmentVisualActor* UARPGEquipmentComponent::GetEquipmentVisual(FGuid ItemInstanceId) const
{
    if (const TWeakObjectPtr<AARPGEquipmentVisualActor>* Found = EquipmentVisualActors.Find(ItemInstanceId)) return Found->Get();
    return nullptr;
}

bool UARPGEquipmentComponent::EquipItem(FGuid Id)
{
    if (!GetOwner() || !Id.IsValid()) return false;
    if (!GetOwner()->HasAuthority()) { ServerEquipItem(Id); return true; }
    return EquipAuthority(Id);
}

bool UARPGEquipmentComponent::UnequipItem(FGuid Id)
{
    if (!GetOwner() || !Id.IsValid()) return false;
    if (!GetOwner()->HasAuthority()) { ServerUnequipItem(Id); return true; }
    return UnequipAuthority(Id);
}

bool UARPGEquipmentComponent::EquipAuthority(FGuid Id)
{
    UARPGInventoryComponent* Inventory = GetOwner() ? GetOwner()->FindComponentByClass<UARPGInventoryComponent>() : nullptr;
    if (!Inventory || !GetOwner()->HasAuthority()) return false;
    FARPGInventoryEntry* Entry = Inventory->Items.FindByPredicate([&](const FARPGInventoryEntry& E){ return E.InstanceId == Id; });
    if (!Entry || Entry->Quantity <= 0) return false;

    UARPGItemDefinition* Definition = Inventory->ResolveItemDefinition(*Entry);
    if (!Definition || !Definition->bEquippable || !Definition->EquipmentSlot.IsValid())
    {
        UE_LOG(LogARPG, Warning, TEXT("EquipItem rejected for %s: runtime inventory entry %s could not resolve a valid equippable Item Definition."),
            *GetNameSafe(GetOwner()), *Id.ToString());
        OnEquipmentRequestResult.Broadcast(false, Id);
        return false;
    }

    if (const UARPGProgressionComponent* Progression = GetOwner()->FindComponentByClass<UARPGProgressionComponent>())
        if (Progression->Level < Definition->RequiredLevel) return false;
    if (!Definition->RequiredClassId.IsNone())
    {
        const UARPGClassComponent* Class = GetOwner()->FindComponentByClass<UARPGClassComponent>();
        if (!Class || Class->GetClassId() != Definition->RequiredClassId) return false;
    }

    UARPGItemDefinition* ReplacedDefinition = nullptr;
    for (const FARPGInventoryEntry& Other : Inventory->Items)
    {
        if (Other.InstanceId == Id || !Other.bEquipped || Other.EquipmentSlot != Definition->EquipmentSlot) continue;
        ReplacedDefinition = Inventory->ResolveItemDefinition(Other);
        break;
    }

    const bool bOk = Inventory->SetEquipped(Id, true, Definition->EquipmentSlot);
    if (bOk)
    {
        RefreshEquipmentEffects();
        if (ReplacedDefinition) MulticastPlayEquipmentPresentation(ReplacedDefinition, false);
        MulticastPlayEquipmentPresentation(Definition, true);
        OnEquipmentChanged.Broadcast(Definition->EquipmentSlot, Id);
    }
    OnEquipmentRequestResult.Broadcast(bOk, Id);
    return bOk;
}

bool UARPGEquipmentComponent::UnequipAuthority(FGuid Id)
{
    UARPGInventoryComponent* Inventory = GetOwner() ? GetOwner()->FindComponentByClass<UARPGInventoryComponent>() : nullptr;
    if (!Inventory || !GetOwner()->HasAuthority()) return false;
    FARPGInventoryEntry* Entry = Inventory->Items.FindByPredicate([&](const FARPGInventoryEntry& E){ return E.InstanceId == Id; });
    if (!Entry || !Entry->bEquipped) return false;

    UARPGItemDefinition* Definition = Inventory->ResolveItemDefinition(*Entry);
    const FGameplayTag Slot = Entry->EquipmentSlot;
    const bool bOk = Inventory->SetEquipped(Id, false, FGameplayTag());
    if (bOk)
    {
        RefreshEquipmentEffects();
        if (Definition) MulticastPlayEquipmentPresentation(Definition, false);
        OnEquipmentChanged.Broadcast(Slot, FGuid());
    }
    OnEquipmentRequestResult.Broadcast(bOk, Id);
    return bOk;
}

void UARPGEquipmentComponent::RefreshEquipmentEffects()
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;
    IAbilitySystemInterface* AbilityInterface = Cast<IAbilitySystemInterface>(GetOwner());
    UAbilitySystemComponent* ASC = AbilityInterface ? AbilityInterface->GetAbilitySystemComponent() : nullptr;
    if (ASC)
        for (const TPair<FGuid, FActiveGameplayEffectHandle>& Pair : ActiveEquipmentEffects)
            if (Pair.Value.IsValid()) ASC->RemoveActiveGameplayEffect(Pair.Value);
    ActiveEquipmentEffects.Reset();

    UARPGInventoryComponent* Inventory = GetOwner()->FindComponentByClass<UARPGInventoryComponent>();
    if (!Inventory || !ASC) return;
    for (const FARPGInventoryEntry& Entry : Inventory->Items)
    {
        UARPGItemDefinition* Definition = Inventory->ResolveItemDefinition(Entry);
        if (!IsValidEquippedEntry(Entry, Definition) || !Definition->EquippedGameplayEffect) continue;
        FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
        Context.AddSourceObject(GetOwner());
        FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(Definition->EquippedGameplayEffect, 1.f, Context);
        if (Spec.IsValid()) ActiveEquipmentEffects.Add(Entry.InstanceId, ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get()));
    }
}

FName UARPGEquipmentComponent::ResolveAttachSocket(const UARPGItemDefinition* Definition, const USkeletalMeshComponent* CharacterMesh) const
{
    if (!CharacterMesh) return NAME_None;

    auto IsUsable = [&](FName Candidate)
    {
        return !Candidate.IsNone() && (CharacterMesh->DoesSocketExist(Candidate) || CharacterMesh->GetBoneIndex(Candidate) != INDEX_NONE);
    };

    if (Definition && IsUsable(Definition->AttachSocket)) return Definition->AttachSocket;
    if (IsUsable(DefaultAttachSocket)) return DefaultAttachSocket;
    if (bAutoFindFallbackHandSocket)
        for (const FName Candidate : FallbackHandSockets)
            if (IsUsable(Candidate)) return Candidate;
    return NAME_None;
}

AARPGEquipmentVisualActor* UARPGEquipmentComponent::CreateEquipmentVisual(FGuid ItemInstanceId, const UARPGItemDefinition* Definition, FGameplayTag Slot)
{
    if (!bAutoCreateEquipmentVisuals || !GetOwner() || !GetWorld() || !Definition || GetOwner()->GetNetMode() == NM_DedicatedServer) return nullptr;
    const bool bHasNativeMesh = !Definition->EquippedStaticMesh.IsNull() || !Definition->EquippedSkeletalMesh.IsNull();
    if (!Definition->EquippedVisualActorClass && !bHasNativeMesh) return nullptr;

    TSubclassOf<AARPGEquipmentVisualActor> VisualClass = Definition->EquippedVisualActorClass;
    if (!VisualClass) VisualClass = AARPGEquipmentVisualActor::StaticClass();

    FActorSpawnParameters Params;
    Params.Owner = GetOwner();
    Params.Instigator = Cast<APawn>(GetOwner());
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AARPGEquipmentVisualActor* Visual = GetWorld()->SpawnActor<AARPGEquipmentVisualActor>(VisualClass, GetOwner()->GetActorTransform(), Params);
    if (!Visual) return nullptr;
    Visual->SetReplicates(false);
    Visual->ConfigureFromItem(Definition);

    USceneComponent* AttachParent = GetOwner()->GetRootComponent();
    FName Socket = NAME_None;
    if (bAttachToCharacterMesh)
    {
        if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
        {
            if (USkeletalMeshComponent* CharacterMesh = Character->GetMesh())
            {
                AttachParent = CharacterMesh;
                Socket = ResolveAttachSocket(Definition, CharacterMesh);
                if (Socket.IsNone() && !Definition->AttachSocket.IsNone())
                {
                    UE_LOG(LogARPG, Warning, TEXT("Equipment visual for %s could not find socket/bone '%s' on %s. Attach Socket or fallback sockets need to match the character skeleton."),
                        *GetNameSafe(Definition), *Definition->AttachSocket.ToString(), *GetNameSafe(CharacterMesh));
                }
            }
        }
    }
    if (!AttachParent) { Visual->Destroy(); return nullptr; }

    Visual->AttachToComponent(AttachParent, FAttachmentTransformRules::SnapToTargetNotIncludingScale, Socket);
    Visual->SetActorRelativeTransform(Definition->EquippedRelativeTransform);
    EquipmentVisualActors.Add(ItemInstanceId, Visual);
    OnEquipmentVisualChanged.Broadcast(ItemInstanceId, Slot, Visual);
    return Visual;
}

void UARPGEquipmentComponent::DestroyEquipmentVisual(FGuid ItemInstanceId, FGameplayTag Slot)
{
    if (TWeakObjectPtr<AARPGEquipmentVisualActor>* Found = EquipmentVisualActors.Find(ItemInstanceId))
    {
        if (AARPGEquipmentVisualActor* Visual = Found->Get()) Visual->Destroy();
        EquipmentVisualActors.Remove(ItemInstanceId);
        OnEquipmentVisualChanged.Broadcast(ItemInstanceId, Slot, nullptr);
    }
}

void UARPGEquipmentComponent::RefreshEquipmentVisuals()
{
    if (!GetOwner()) return;
    const UARPGInventoryComponent* Inventory = GetOwner()->FindComponentByClass<UARPGInventoryComponent>();
    if (!Inventory) return;

    TSet<FGuid> Desired;
    TMap<FGuid, FGameplayTag> Slots;
    for (const FARPGInventoryEntry& Entry : Inventory->Items)
    {
        UARPGItemDefinition* Definition = Inventory->ResolveItemDefinition(Entry);
        if (!IsValidEquippedEntry(Entry, Definition)) continue;

        Desired.Add(Entry.InstanceId);
        Slots.Add(Entry.InstanceId, Entry.EquipmentSlot);
        if (!IsValid(GetEquipmentVisual(Entry.InstanceId)))
        {
            EquipmentVisualActors.Remove(Entry.InstanceId);
            CreateEquipmentVisual(Entry.InstanceId, Definition, Entry.EquipmentSlot);
        }
    }

    TArray<FGuid> Existing;
    EquipmentVisualActors.GetKeys(Existing);
    for (const FGuid& Id : Existing)
        if (!Desired.Contains(Id)) DestroyEquipmentVisual(Id, Slots.FindRef(Id));
}

void UARPGEquipmentComponent::PlayEquipmentPresentationLocal(const UARPGItemDefinition* Definition, bool bEquipping) const
{
    if (!Definition || !GetOwner() || GetOwner()->GetNetMode() == NM_DedicatedServer) return;
    const TSoftObjectPtr<USoundBase>& SoundAsset = bEquipping ? Definition->EquipSound : Definition->UnequipSound;
    if (!SoundAsset.IsNull())
    {
        if (USoundBase* Sound = SoundAsset.LoadSynchronous())
        {
            const float PitchLow = FMath::Min(Definition->EquipmentAudioPitchMin, Definition->EquipmentAudioPitchMax);
            const float PitchHigh = FMath::Max(Definition->EquipmentAudioPitchMin, Definition->EquipmentAudioPitchMax);
            UGameplayStatics::PlaySoundAtLocation(this, Sound, GetOwner()->GetActorLocation(), FMath::Max(0.f, Definition->EquipmentAudioVolume), FMath::FRandRange(PitchLow, PitchHigh), 0.f, nullptr, nullptr, nullptr);
        }
    }

    if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
    {
        const TSoftObjectPtr<UAnimMontage>& MontageAsset = bEquipping ? Definition->EquipMontage : Definition->UnequipMontage;
        if (!MontageAsset.IsNull())
            if (UAnimMontage* Montage = MontageAsset.LoadSynchronous()) Character->PlayAnimMontage(Montage);
    }
}

void UARPGEquipmentComponent::MulticastPlayEquipmentPresentation_Implementation(UARPGItemDefinition* Definition, bool bEquipping)
{
    PlayEquipmentPresentationLocal(Definition, bEquipping);
}

bool UARPGEquipmentComponent::PlayEquippedCombatSwingSoundLocal()
{
    if (!GetOwner() || GetOwner()->GetNetMode() == NM_DedicatedServer) return false;
    const UARPGInventoryComponent* Inventory = GetOwner()->FindComponentByClass<UARPGInventoryComponent>();
    if (!Inventory) return false;
    for (const FARPGInventoryEntry& Entry : Inventory->Items)
    {
        UARPGItemDefinition* Definition = Inventory->ResolveItemDefinition(Entry);
        if (!IsValidEquippedEntry(Entry, Definition) || Definition->CombatSwingSound.IsNull()) continue;
        if (USoundBase* Sound = Definition->CombatSwingSound.LoadSynchronous())
        {
            const float PitchLow = FMath::Min(Definition->EquipmentAudioPitchMin, Definition->EquipmentAudioPitchMax);
            const float PitchHigh = FMath::Max(Definition->EquipmentAudioPitchMin, Definition->EquipmentAudioPitchMax);
            UGameplayStatics::PlaySoundAtLocation(this, Sound, GetOwner()->GetActorLocation(), FMath::Max(0.f, Definition->EquipmentAudioVolume), FMath::FRandRange(PitchLow, PitchHigh), 0.f, nullptr, nullptr, nullptr);
            return true;
        }
    }
    return false;
}

void UARPGEquipmentComponent::ServerEquipItem_Implementation(FGuid Id) { EquipAuthority(Id); }
void UARPGEquipmentComponent::ServerUnequipItem_Implementation(FGuid Id) { UnequipAuthority(Id); }
