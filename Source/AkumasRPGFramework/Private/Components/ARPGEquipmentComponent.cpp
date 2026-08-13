#include "Components/ARPGEquipmentComponent.h"
#include "Components/ARPGInventoryComponent.h"
#include "Components/ARPGQuickAccessComponent.h"
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
    if (GetOwner() && GetOwner()->HasAuthority())
    {
        RepairExclusiveVisualAttachmentStateAuthority();
        RefreshEquipmentEffects();
    }
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
    if (Definition->bUsesDurability && Entry.Durability <= KINDA_SMALL_NUMBER) return false;
    return Entry.EquipmentSlot == Definition->EquipmentSlot;
}

bool UARPGEquipmentComponent::HasEquipmentVisualIntent(const UARPGItemDefinition* Definition) const
{
    return Definition && (Definition->EquippedVisualActorClass || !Definition->EquippedStaticMesh.IsNull() || !Definition->EquippedSkeletalMesh.IsNull());
}

bool UARPGEquipmentComponent::SharesExclusiveVisualAttachment(const UARPGItemDefinition* A, const UARPGItemDefinition* B) const
{
    // Logical equipment slots remain independent (armor/offhand/etc.), but two visible pieces cannot both own
    // the exact same physical character socket. This closes the Inventory <-> Quick Access handoff gap where
    // a Tool and Weapon used different gameplay EquipmentSlot tags while both rendered on the same hand socket.
    if (!A || !B || !bAttachToCharacterMesh || !HasEquipmentVisualIntent(A) || !HasEquipmentVisualIntent(B)) return false;

    const ACharacter* Character = Cast<ACharacter>(GetOwner());
    const USkeletalMeshComponent* CharacterMesh = Character ? Character->GetMesh() : nullptr;
    if (!CharacterMesh) return false;

    const FName SocketA = ResolveAttachSocket(A, CharacterMesh);
    const FName SocketB = ResolveAttachSocket(B, CharacterMesh);
    return !SocketA.IsNone() && SocketA == SocketB;
}

bool UARPGEquipmentComponent::RepairExclusiveVisualAttachmentStateAuthority()
{
    if (bRepairingExclusiveVisualState || !GetOwner() || !GetOwner()->HasAuthority() || !bAttachToCharacterMesh) return false;

    UARPGInventoryComponent* Inventory = GetOwner()->FindComponentByClass<UARPGInventoryComponent>();
    const ACharacter* Character = Cast<ACharacter>(GetOwner());
    const USkeletalMeshComponent* CharacterMesh = Character ? Character->GetMesh() : nullptr;
    if (!Inventory || !CharacterMesh) return false;

    FGuid PreferredActiveQuickAccessInstance;
    if (const UARPGQuickAccessComponent* QuickAccess = GetOwner()->FindComponentByClass<UARPGQuickAccessComponent>())
    {
        const int32 ActiveIndex = QuickAccess->ActiveSlotNumber - 1;
        if (QuickAccess->QuickAccessSlots.IsValidIndex(ActiveIndex))
            PreferredActiveQuickAccessInstance = QuickAccess->QuickAccessSlots[ActiveIndex].ItemInstanceId;
    }

    TMap<FName, int32> OwnerIndexBySocket;
    TArray<TPair<FGameplayTag, FGuid>> ClearedEntries;

    for (int32 Index = 0; Index < Inventory->Items.Num(); ++Index)
    {
        FARPGInventoryEntry& Entry = Inventory->Items[Index];
        UARPGItemDefinition* Definition = Inventory->ResolveItemDefinition(Entry);
        if (!IsValidEquippedEntry(Entry, Definition) || !HasEquipmentVisualIntent(Definition)) continue;

        const FName Socket = ResolveAttachSocket(Definition, CharacterMesh);
        if (Socket.IsNone()) continue;

        if (int32* ExistingIndex = OwnerIndexBySocket.Find(Socket))
        {
            if (!Inventory->Items.IsValidIndex(*ExistingIndex))
            {
                *ExistingIndex = Index;
                continue;
            }

            FARPGInventoryEntry& Existing = Inventory->Items[*ExistingIndex];
            const bool bCurrentPreferred = Entry.InstanceId == PreferredActiveQuickAccessInstance;
            const bool bExistingPreferred = Existing.InstanceId == PreferredActiveQuickAccessInstance;
            const int32 LoserIndex = (bCurrentPreferred && !bExistingPreferred) ? *ExistingIndex : Index;
            FARPGInventoryEntry& Loser = Inventory->Items[LoserIndex];
            ClearedEntries.Emplace(Loser.EquipmentSlot, Loser.InstanceId);
            Loser.bEquipped = false;
            Loser.EquipmentSlot = FGameplayTag();

            if (LoserIndex == *ExistingIndex) *ExistingIndex = Index;
        }
        else
        {
            OwnerIndexBySocket.Add(Socket, Index);
        }
    }

    if (ClearedEntries.Num() == 0) return false;

    bRepairingExclusiveVisualState = true;
    Inventory->OnInventoryChanged.Broadcast();
    bRepairingExclusiveVisualState = false;

    for (const TPair<FGameplayTag, FGuid>& Cleared : ClearedEntries)
    {
        UE_LOG(LogARPG, Warning, TEXT("Repaired conflicting equipped item %s on %s because another equipped visual owns the same physical attachment socket."),
            *Cleared.Value.ToString(), *GetNameSafe(GetOwner()));
        if (Cleared.Key.IsValid()) OnEquipmentChanged.Broadcast(Cleared.Key, FGuid());
    }
    return true;
}

void UARPGEquipmentComponent::HandleInventoryChanged()
{
    if (GetOwner() && GetOwner()->HasAuthority())
    {
        if (!bRepairingExclusiveVisualState) RepairExclusiveVisualAttachmentStateAuthority();
        RefreshEquipmentEffects();
    }
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

    if (Definition->bUsesDurability && Entry->Durability <= KINDA_SMALL_NUMBER)
    {
        UE_LOG(LogARPG, Verbose, TEXT("EquipItem rejected for %s: item %s is broken."), *GetNameSafe(GetOwner()), *Id.ToString());
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

    struct FReplacedEquipment
    {
        int32 InventoryIndex = INDEX_NONE;
        FGuid InstanceId;
        FGameplayTag Slot;
        UARPGItemDefinition* Definition = nullptr;
        bool bNeedsExplicitClear = false;
    };

    TArray<FReplacedEquipment> ReplacedItems;
    for (int32 Index = 0; Index < Inventory->Items.Num(); ++Index)
    {
        FARPGInventoryEntry& Other = Inventory->Items[Index];
        if (Other.InstanceId == Id || !Other.bEquipped || !Other.EquipmentSlot.IsValid()) continue;

        UARPGItemDefinition* OtherDefinition = Inventory->ResolveItemDefinition(Other);
        if (!OtherDefinition) continue;

        const bool bSameLogicalSlot = Other.EquipmentSlot == Definition->EquipmentSlot;
        const bool bSamePhysicalAttachment = SharesExclusiveVisualAttachment(Definition, OtherDefinition);
        if (!bSameLogicalSlot && !bSamePhysicalAttachment) continue;

        FReplacedEquipment& Replaced = ReplacedItems.AddDefaulted_GetRef();
        Replaced.InventoryIndex = Index;
        Replaced.InstanceId = Other.InstanceId;
        Replaced.Slot = Other.EquipmentSlot;
        Replaced.Definition = OtherDefinition;
        Replaced.bNeedsExplicitClear = !bSameLogicalSlot && bSamePhysicalAttachment;

        // SetEquipped already atomically clears items in the same logical slot. For a cross-slot physical
        // collision (for example Equipment.Tool.MainHand vs Equipment.Weapon.MainHand), clear the old
        // state inside this same authority transaction before the single InventoryChanged broadcast.
        if (Replaced.bNeedsExplicitClear)
        {
            Other.bEquipped = false;
            Other.EquipmentSlot = FGameplayTag();
        }
    }

    const bool bOk = Inventory->SetEquipped(Id, true, Definition->EquipmentSlot);
    if (!bOk)
    {
        // The new entry was fully validated above, but restore any cross-slot state defensively if an
        // unexpected SetEquipped failure occurs so an equip request can never orphan the previous item.
        for (const FReplacedEquipment& Replaced : ReplacedItems)
        {
            if (!Replaced.bNeedsExplicitClear || !Inventory->Items.IsValidIndex(Replaced.InventoryIndex)) continue;
            FARPGInventoryEntry& Other = Inventory->Items[Replaced.InventoryIndex];
            if (Other.InstanceId != Replaced.InstanceId) continue;
            Other.bEquipped = true;
            Other.EquipmentSlot = Replaced.Slot;
        }
        OnEquipmentRequestResult.Broadcast(false, Id);
        return false;
    }

    RefreshEquipmentEffects();
    for (const FReplacedEquipment& Replaced : ReplacedItems)
    {
        if (Replaced.Definition) MulticastPlayEquipmentPresentation(Replaced.Definition, false);
        if (Replaced.bNeedsExplicitClear && Replaced.Slot.IsValid())
            OnEquipmentChanged.Broadcast(Replaced.Slot, FGuid());
    }
    MulticastPlayEquipmentPresentation(Definition, true);
    OnEquipmentChanged.Broadcast(Definition->EquipmentSlot, Id);
    OnEquipmentRequestResult.Broadcast(true, Id);
    return true;
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

    // Visual safety net: even legacy/corrupt replicated state must never render two equipment actors on the
    // same physical character socket. Normal authority equips are already repaired in EquipAuthority; this
    // projection guard prevents a one-frame or old-save double-mesh state from ever reaching the player.
    FGuid PreferredActiveQuickAccessInstance;
    if (const UARPGQuickAccessComponent* QuickAccess = GetOwner()->FindComponentByClass<UARPGQuickAccessComponent>())
    {
        const int32 ActiveIndex = QuickAccess->ActiveSlotNumber - 1;
        if (QuickAccess->QuickAccessSlots.IsValidIndex(ActiveIndex))
            PreferredActiveQuickAccessInstance = QuickAccess->QuickAccessSlots[ActiveIndex].ItemInstanceId;
    }

    const ACharacter* Character = Cast<ACharacter>(GetOwner());
    const USkeletalMeshComponent* CharacterMesh = Character ? Character->GetMesh() : nullptr;

    TSet<FGuid> Desired;
    TMap<FGuid, FGameplayTag> Slots;
    TMap<FGuid, UARPGItemDefinition*> Definitions;
    TMap<FName, FGuid> DesiredOwnerBySocket;

    for (const FARPGInventoryEntry& Entry : Inventory->Items)
    {
        UARPGItemDefinition* Definition = Inventory->ResolveItemDefinition(Entry);
        if (!IsValidEquippedEntry(Entry, Definition)) continue;

        FName PhysicalSocket = NAME_None;
        if (bAttachToCharacterMesh && CharacterMesh && HasEquipmentVisualIntent(Definition))
            PhysicalSocket = ResolveAttachSocket(Definition, CharacterMesh);

        if (!PhysicalSocket.IsNone())
        {
            if (FGuid* ExistingOwner = DesiredOwnerBySocket.Find(PhysicalSocket))
            {
                const bool bCurrentIsPreferred = Entry.InstanceId == PreferredActiveQuickAccessInstance;
                const bool bExistingIsPreferred = *ExistingOwner == PreferredActiveQuickAccessInstance;
                if (!bCurrentIsPreferred || bExistingIsPreferred)
                {
                    UE_LOG(LogARPG, Warning, TEXT("Suppressed duplicate equipment visual for %s on socket '%s': %s and %s were both marked equipped."),
                        *GetNameSafe(GetOwner()), *PhysicalSocket.ToString(), *ExistingOwner->ToString(), *Entry.InstanceId.ToString());
                    continue;
                }

                Desired.Remove(*ExistingOwner);
                Slots.Remove(*ExistingOwner);
                Definitions.Remove(*ExistingOwner);
                *ExistingOwner = Entry.InstanceId;
            }
            else
            {
                DesiredOwnerBySocket.Add(PhysicalSocket, Entry.InstanceId);
            }
        }

        Desired.Add(Entry.InstanceId);
        Slots.Add(Entry.InstanceId, Entry.EquipmentSlot);
        Definitions.Add(Entry.InstanceId, Definition);
    }

    // Destroy stale/conflicting visuals before creating the winner so a physical socket never contains both.
    TArray<FGuid> Existing;
    EquipmentVisualActors.GetKeys(Existing);
    for (const FGuid& ExistingId : Existing)
        if (!Desired.Contains(ExistingId)) DestroyEquipmentVisual(ExistingId);

    for (const FGuid& DesiredId : Desired)
    {
        if (IsValid(GetEquipmentVisual(DesiredId))) continue;
        EquipmentVisualActors.Remove(DesiredId);
        CreateEquipmentVisual(DesiredId, Definitions.FindRef(DesiredId), Slots.FindRef(DesiredId));
    }
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

bool UARPGEquipmentComponent::ApplyCombatDurabilityWear(float WearMultiplier)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || WearMultiplier <= 0.f) return false;
    UARPGInventoryComponent* Inventory = GetOwner()->FindComponentByClass<UARPGInventoryComponent>();
    if (!Inventory) return false;

    auto TryWear = [&](const FGuid& InstanceId) -> bool
    {
        if (!InstanceId.IsValid()) return false;
        FARPGInventoryEntry* Entry = Inventory->Items.FindByPredicate([&](const FARPGInventoryEntry& Candidate){ return Candidate.InstanceId == InstanceId; });
        if (!Entry) return false;
        UARPGItemDefinition* Definition = Inventory->ResolveItemDefinition(*Entry);
        if (!IsValidEquippedEntry(*Entry, Definition) || !Definition->bUsesDurability || !Definition->bLoseDurabilityOnCombatHit) return false;
        const float Wear = FMath::Max(0.f, Definition->CombatDurabilityLossPerSuccessfulHit) * WearMultiplier;
        return Wear > KINDA_SMALL_NUMBER && Inventory->DamageItemDurability(InstanceId, Wear);
    };

    // The held/active Quick Access item is the deterministic combat durability owner when available.
    if (const UARPGQuickAccessComponent* QuickAccess = GetOwner()->FindComponentByClass<UARPGQuickAccessComponent>())
    {
        const int32 ActiveIndex = QuickAccess->ActiveSlotNumber - 1;
        if (QuickAccess->QuickAccessSlots.IsValidIndex(ActiveIndex))
            if (TryWear(QuickAccess->QuickAccessSlots[ActiveIndex].ItemInstanceId)) return true;
    }

    // Direct Inventory equip (or a project without Quick Access) still receives wear.
    for (const FARPGInventoryEntry& Entry : Inventory->Items)
        if (TryWear(Entry.InstanceId)) return true;
    return false;
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
