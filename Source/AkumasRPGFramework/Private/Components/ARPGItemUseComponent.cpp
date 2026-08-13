#include "Components/ARPGItemUseComponent.h"
#include "Actors/ARPGCharacter.h"
#include "Components/ARPGInventoryComponent.h"
#include "Components/ARPGQuickAccessComponent.h"
#include "Components/ARPGStatsComponent.h"
#include "Data/ARPGItemDefinition.h"
#include "AbilitySystemInterface.h"
#include "AbilitySystemComponent.h"
#include "ActiveGameplayEffectHandle.h"
#include "GameplayEffect.h"
#include "GameFramework/Character.h"
#include "GameFramework/GameStateBase.h"
#include "Kismet/GameplayStatics.h"
#include "Animation/AnimMontage.h"
#include "Sound/SoundBase.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"

namespace
{
    // Avoid treating tiny floating-point residue as a meaningful consumable target.
    constexpr float ARPGItemUseVitalTolerance = 0.01f;

    bool ARPGCanRestoreVital(float CurrentValue, float MaxValue, float RestoreAmount)
    {
        return RestoreAmount > KINDA_SMALL_NUMBER
            && MaxValue > KINDA_SMALL_NUMBER
            && CurrentValue + ARPGItemUseVitalTolerance < MaxValue;
    }
}

UARPGItemUseComponent::UARPGItemUseComponent()
{
    SetIsReplicatedByDefault(true);
    PrimaryComponentTick.bCanEverTick = false;
}

UARPGInventoryComponent* UARPGItemUseComponent::GetInventory() const
{
    return GetOwner() ? GetOwner()->FindComponentByClass<UARPGInventoryComponent>() : nullptr;
}

AARPGCharacter* UARPGItemUseComponent::GetARPGCharacter() const
{
    return Cast<AARPGCharacter>(GetOwner());
}

float UARPGItemUseComponent::GetServerTimeSeconds() const
{
    if (const UWorld* World = GetWorld())
    {
        if (const AGameStateBase* GameState = World->GetGameState<AGameStateBase>()) return GameState->GetServerWorldTimeSeconds();
        return World->GetTimeSeconds();
    }
    return 0.f;
}

float UARPGItemUseComponent::GetCooldownEnd(FName ItemId) const
{
    if (ItemId.IsNone()) return 0.f;
    if (const FARPGItemUseCooldownState* State = Cooldowns.FindByPredicate([&](const FARPGItemUseCooldownState& Candidate)
    {
        return Candidate.ItemId == ItemId;
    })) return State->CooldownEndServerTime;
    return 0.f;
}

float UARPGItemUseComponent::GetCooldownRemaining(FName ItemId) const
{
    return FMath::Max(0.f, GetCooldownEnd(ItemId) - GetServerTimeSeconds());
}

float UARPGItemUseComponent::GetItemInstanceCooldownRemaining(FGuid ItemInstanceId) const
{
    const UARPGInventoryComponent* Inventory = GetInventory();
    if (!Inventory || !ItemInstanceId.IsValid()) return 0.f;
    const FARPGInventoryEntry* Entry = Inventory->Items.FindByPredicate([&](const FARPGInventoryEntry& Candidate)
    {
        return Candidate.InstanceId == ItemInstanceId && Candidate.Quantity > 0;
    });
    return Entry ? GetCooldownRemaining(Entry->ItemId) : 0.f;
}

void UARPGItemUseComponent::SetCooldownAuthority(FName ItemId, float CooldownEndServerTime)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || ItemId.IsNone()) return;

    const float Now = GetServerTimeSeconds();
    const int32 ExistingIndex = Cooldowns.IndexOfByPredicate([&](const FARPGItemUseCooldownState& Candidate)
    {
        return Candidate.ItemId == ItemId;
    });

    if (CooldownEndServerTime <= Now + KINDA_SMALL_NUMBER)
    {
        if (ExistingIndex != INDEX_NONE) Cooldowns.RemoveAt(ExistingIndex);
    }
    else if (ExistingIndex != INDEX_NONE)
    {
        Cooldowns[ExistingIndex].CooldownEndServerTime = CooldownEndServerTime;
    }
    else
    {
        FARPGItemUseCooldownState& Added = Cooldowns.AddDefaulted_GetRef();
        Added.ItemId = ItemId;
        Added.CooldownEndServerTime = CooldownEndServerTime;
    }

    OnItemUseCooldownsChanged.Broadcast();

    // Preserve the existing Quick Access cooldown projection/API so current hotbar widgets and Blueprint queries
    // continue to work even though the authoritative use logic now lives here.
    if (UARPGQuickAccessComponent* QuickAccess = GetOwner()->FindComponentByClass<UARPGQuickAccessComponent>())
        QuickAccess->NotifyItemUsedAuthority(ItemId, CooldownEndServerTime);
}

UARPGItemUseBehavior* UARPGItemUseComponent::CreateUseBehavior(const UARPGItemDefinition* Definition) const
{
    if (!Definition || !Definition->UseBehaviorClass) return nullptr;
    return NewObject<UARPGItemUseBehavior>(const_cast<UARPGItemUseComponent*>(this), Definition->UseBehaviorClass);
}

bool UARPGItemUseComponent::HasConfiguredBuiltInVitalRestore(const UARPGItemDefinition* Definition) const
{
    return Definition && (
        Definition->RestoreHealth > KINDA_SMALL_NUMBER
        || Definition->RestoreMana > KINDA_SMALL_NUMBER
        || Definition->RestoreStamina > KINDA_SMALL_NUMBER);
}

bool UARPGItemUseComponent::HasUsefulBuiltInVitalRestore(const UARPGItemDefinition* Definition, const UARPGStatsComponent* Stats) const
{
    if (!Definition || !Stats) return false;
    const bool bCanRestoreHealth = Stats->Health > 0.f && ARPGCanRestoreVital(Stats->Health, Stats->MaxHealth, Definition->RestoreHealth);
    const bool bCanRestoreMana = ARPGCanRestoreVital(Stats->Mana, Stats->MaxMana, Definition->RestoreMana);
    const bool bCanRestoreStamina = ARPGCanRestoreVital(Stats->Stamina, Stats->MaxStamina, Definition->RestoreStamina);
    return bCanRestoreHealth || bCanRestoreMana || bCanRestoreStamina;
}

bool UARPGItemUseComponent::ShouldBlockForFullConfiguredVitals(const UARPGItemDefinition* Definition, const UARPGStatsComponent* Stats) const
{
    if (!Definition || !HasConfiguredBuiltInVitalRestore(Definition)) return false;

    // A vital-restoration item is safe-by-default: secondary GAS/custom effects do not make it consumable
    // while every configured restored vital is already full. Designers can explicitly opt out for mixed
    // items such as "healing food + independent long-duration buff".
    if (HasUsefulBuiltInVitalRestore(Definition, Stats)) return false;
    if (!Definition->bAllowOtherEffectsWhenRestoredVitalsFull) return true;

    return Definition->UseGameplayEffect == nullptr && Definition->UseBehaviorClass == nullptr;
}

FText UARPGItemUseComponent::BuildFullVitalsFailureReason(const UARPGItemDefinition* Definition) const
{
    if (!Definition) return NSLOCTEXT("AkumasRPGFramework", "ItemUseVitalsAlreadyFull", "The item's restored vitals are already full.");
    const bool bHealthOnly = Definition->RestoreHealth > KINDA_SMALL_NUMBER && Definition->RestoreMana <= KINDA_SMALL_NUMBER && Definition->RestoreStamina <= KINDA_SMALL_NUMBER;
    const bool bManaOnly = Definition->RestoreMana > KINDA_SMALL_NUMBER && Definition->RestoreHealth <= KINDA_SMALL_NUMBER && Definition->RestoreStamina <= KINDA_SMALL_NUMBER;
    const bool bStaminaOnly = Definition->RestoreStamina > KINDA_SMALL_NUMBER && Definition->RestoreHealth <= KINDA_SMALL_NUMBER && Definition->RestoreMana <= KINDA_SMALL_NUMBER;
    if (bHealthOnly) return NSLOCTEXT("AkumasRPGFramework", "ItemUseHealthAlreadyFull", "Health is already full.");
    if (bManaOnly) return NSLOCTEXT("AkumasRPGFramework", "ItemUseManaAlreadyFull", "Mana is already full.");
    if (bStaminaOnly) return NSLOCTEXT("AkumasRPGFramework", "ItemUseStaminaAlreadyFull", "Stamina is already full.");
    return NSLOCTEXT("AkumasRPGFramework", "ItemUseVitalsAlreadyFull", "The item's restored vitals are already full.");
}

bool UARPGItemUseComponent::CanUseItemNow(FGuid ItemInstanceId) const
{
    const UARPGInventoryComponent* Inventory = GetInventory();
    if (!Inventory || !ItemInstanceId.IsValid()) return false;

    const FARPGInventoryEntry* Entry = Inventory->Items.FindByPredicate([&](const FARPGInventoryEntry& Candidate)
    {
        return Candidate.InstanceId == ItemInstanceId && Candidate.Quantity > 0;
    });
    if (!Entry) return false;

    UARPGItemDefinition* Definition = Inventory->ResolveItemDefinition(*Entry);
    if (!Definition || !Definition->bUsable) return false;
    const int32 ConsumeQuantity = Definition->bConsumeOnUse ? FMath::Max(1, Definition->ConsumeQuantity) : 0;
    if (Definition->bConsumeOnUse && Entry->Quantity < ConsumeQuantity) return false;
    if (GetCooldownRemaining(Entry->ItemId) > KINDA_SMALL_NUMBER) return false;

    const AARPGCharacter* Character = GetARPGCharacter();
    const UARPGStatsComponent* Stats = Character && Character->Stats ? Character->Stats.Get() : (GetOwner() ? GetOwner()->FindComponentByClass<UARPGStatsComponent>() : nullptr);
    const bool bHasConfiguredVitalRestore = HasConfiguredBuiltInVitalRestore(Definition);
    const bool bHasOtherPotentialEffect = Definition->UseGameplayEffect != nullptr || Definition->UseBehaviorClass != nullptr;

    // v2.13.3: restored vitals are now a hard usefulness gate by default, even when the same item also
    // has a Gameplay Effect or custom behavior. This closes the real full-HP potion loophole.
    if (ShouldBlockForFullConfiguredVitals(Definition, Stats)) return false;
    if (bHasConfiguredVitalRestore && HasUsefulBuiltInVitalRestore(Definition, Stats)) return true;
    return bHasOtherPotentialEffect;
}

bool UARPGItemUseComponent::UseItem(FGuid ItemInstanceId, EARPGItemUseSource Source, int32 QuickAccessSlot)
{
    if (!GetOwner() || !ItemInstanceId.IsValid()) return false;
    if (!GetOwner()->HasAuthority())
    {
        if (!CanUseItemNow(ItemInstanceId)) return false;
        ServerUseItem(ItemInstanceId, Source, QuickAccessSlot);
        return true;
    }

    FText FailureReason;
    UARPGItemDefinition* Definition = GetInventory() ? GetInventory()->GetItemDefinitionForInstance(ItemInstanceId) : nullptr;
    const EARPGItemUseResult Result = UseItemAuthority(ItemInstanceId, Source, QuickAccessSlot, FailureReason);
    SendResultToOwner(Result, ItemInstanceId, Definition, FailureReason);
    return Result == EARPGItemUseResult::Success;
}

bool UARPGItemUseComponent::UseFirstItemById(FName ItemId, EARPGItemUseSource Source)
{
    const UARPGInventoryComponent* Inventory = GetInventory();
    if (!Inventory || ItemId.IsNone()) return false;
    const FARPGInventoryEntry* Entry = Inventory->Items.FindByPredicate([&](const FARPGInventoryEntry& Candidate)
    {
        return Candidate.ItemId == ItemId && Candidate.InstanceId.IsValid() && Candidate.Quantity > 0;
    });
    return Entry ? UseItem(Entry->InstanceId, Source, 0) : false;
}

EARPGItemUseResult UARPGItemUseComponent::UseItemAuthority(FGuid ItemInstanceId, EARPGItemUseSource Source, int32 QuickAccessSlot, FText& OutFailureReason)
{
    OutFailureReason = FText::GetEmpty();
    if (!GetOwner() || !GetOwner()->HasAuthority() || !ItemInstanceId.IsValid()) return EARPGItemUseResult::InvalidItem;

    UARPGInventoryComponent* Inventory = GetInventory();
    AARPGCharacter* Character = GetARPGCharacter();
    if (!Inventory || !Character) return EARPGItemUseResult::ItemUnavailable;

    const FARPGInventoryEntry* Entry = Inventory->Items.FindByPredicate([&](const FARPGInventoryEntry& Candidate)
    {
        return Candidate.InstanceId == ItemInstanceId && Candidate.Quantity > 0;
    });
    if (!Entry) return EARPGItemUseResult::ItemUnavailable;

    UARPGItemDefinition* Definition = Inventory->ResolveItemDefinition(*Entry);
    if (!Definition) return EARPGItemUseResult::ItemUnavailable;
    if (!Definition->bUsable) return EARPGItemUseResult::ItemNotUsable;

    const int32 QuantityBeforeUse = Entry->Quantity;
    const int32 ConsumeQuantity = Definition->bConsumeOnUse ? FMath::Max(1, Definition->ConsumeQuantity) : 0;
    if (Definition->bConsumeOnUse && QuantityBeforeUse < ConsumeQuantity) return EARPGItemUseResult::InsufficientQuantity;

    const float Now = GetServerTimeSeconds();
    if (GetCooldownEnd(Entry->ItemId) > Now + KINDA_SMALL_NUMBER) return EARPGItemUseResult::OnCooldown;

    UARPGStatsComponent* Stats = Character->Stats.Get();
    if (!Stats) Stats = Character->FindComponentByClass<UARPGStatsComponent>();

    // This check runs on authority before custom behavior, GAS application, consumption, cooldown, or
    // presentation. Client preflight is only UX; this is the rule that actually protects inventory.
    if (ShouldBlockForFullConfiguredVitals(Definition, Stats))
    {
        OutFailureReason = BuildFullVitalsFailureReason(Definition);
        return EARPGItemUseResult::NoUsefulEffect;
    }

    FARPGItemUseContext Context;
    Context.User = Character;
    Context.ItemDefinition = Definition;
    Context.ItemInstanceId = Entry->InstanceId;
    Context.ItemId = Entry->ItemId;
    Context.QuantityBeforeUse = QuantityBeforeUse;
    Context.ConsumeQuantity = ConsumeQuantity;
    Context.Source = Source;
    Context.QuickAccessSlot = Source == EARPGItemUseSource::QuickAccess ? FMath::Max(0, QuickAccessSlot) : 0;

    UARPGItemUseBehavior* CustomBehavior = CreateUseBehavior(Definition);
    if (CustomBehavior)
    {
        FText CustomFailure;
        if (!CustomBehavior->CanUseItem(Context, CustomFailure))
        {
            OutFailureReason = CustomFailure;
            return EARPGItemUseResult::CustomUseRejected;
        }
    }

    const bool bCanRestoreHealth = Stats && Stats->Health > 0.f && ARPGCanRestoreVital(Stats->Health, Stats->MaxHealth, Definition->RestoreHealth);
    const bool bCanRestoreMana = Stats && ARPGCanRestoreVital(Stats->Mana, Stats->MaxMana, Definition->RestoreMana);
    const bool bCanRestoreStamina = Stats && ARPGCanRestoreVital(Stats->Stamina, Stats->MaxStamina, Definition->RestoreStamina);
    const bool bHasGameplayEffect = Definition->UseGameplayEffect != nullptr;
    const bool bHasCustomBehavior = CustomBehavior != nullptr;

    if (!bCanRestoreHealth && !bCanRestoreMana && !bCanRestoreStamina && !bHasGameplayEffect && !bHasCustomBehavior)
    {
        OutFailureReason = HasConfiguredBuiltInVitalRestore(Definition)
            ? BuildFullVitalsFailureReason(Definition)
            : NSLOCTEXT("AkumasRPGFramework", "ItemUseNoConfiguredEffect", "The item has no usable effect configured.");
        return EARPGItemUseResult::NoUsefulEffect;
    }

    bool bAppliedAnything = false;

    // Custom behavior executes on server authority before consumption. Its Can Use Item hook should contain
    // validation; Execute Item Use should perform the actual mutation and return whether it succeeded.
    if (CustomBehavior) bAppliedAnything |= CustomBehavior->ExecuteItemUse(Context);

    if (Stats)
    {
        if (bCanRestoreHealth)
        {
            const float Before = Stats->Health;
            Stats->Heal(Definition->RestoreHealth);
            bAppliedAnything |= Stats->Health > Before + KINDA_SMALL_NUMBER;
        }
        if (bCanRestoreMana)
        {
            const float Before = Stats->Mana;
            Stats->RestoreMana(Definition->RestoreMana);
            bAppliedAnything |= Stats->Mana > Before + KINDA_SMALL_NUMBER;
        }
        if (bCanRestoreStamina)
        {
            const float Before = Stats->Stamina;
            Stats->RestoreStamina(Definition->RestoreStamina);
            bAppliedAnything |= Stats->Stamina > Before + KINDA_SMALL_NUMBER;
        }
    }

    if (bHasGameplayEffect)
    {
        IAbilitySystemInterface* AbilityInterface = Cast<IAbilitySystemInterface>(Character);
        UAbilitySystemComponent* ASC = AbilityInterface ? AbilityInterface->GetAbilitySystemComponent() : Character->FindComponentByClass<UAbilitySystemComponent>();
        if (ASC)
        {
            FGameplayEffectContextHandle EffectContext = ASC->MakeEffectContext();
            EffectContext.AddSourceObject(Definition);
            FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(Definition->UseGameplayEffect, FMath::Max(0.01f, Definition->UseGameplayEffectLevel), EffectContext);
            if (Spec.IsValid())
            {
                const FActiveGameplayEffectHandle AppliedHandle = ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
                // A valid spec is not the same thing as a successfully applied Gameplay Effect. Tags,
                // immunities or custom requirements may reject it. Only a confirmed application counts
                // toward item-use success/consumption; instant effects report success via the sentinel handle.
                bAppliedAnything |= AppliedHandle.WasSuccessfullyApplied();
            }
        }
    }

    if (!bAppliedAnything) return EARPGItemUseResult::UseFailed;

    // Consume only after an actual effect succeeded. This prevents failed/full-vitals/custom-rejected uses
    // from destroying an owned runtime item.
    if (Definition->bConsumeOnUse && !Inventory->RemoveItemInstance(ItemInstanceId, ConsumeQuantity))
        return EARPGItemUseResult::UseFailed;

    const float CooldownEnd = Now + FMath::Max(0.f, Definition->UseCooldownSeconds);
    SetCooldownAuthority(Context.ItemId, CooldownEnd);

    // Gameplay listeners get one authoritative success event even on dedicated servers. Remote clients receive
    // the same event from the presentation multicast below, without duplicating it on listen/standalone authority.
    OnItemUsed.Broadcast(Context);
    MulticastPlayItemUsePresentation(Context.ItemInstanceId, Definition, QuantityBeforeUse, ConsumeQuantity, Source, Context.QuickAccessSlot);
    return EARPGItemUseResult::Success;
}

void UARPGItemUseComponent::PlayPresentationLocal(const FARPGItemUseContext& Context)
{
    if (!Context.ItemDefinition || !GetOwner() || GetOwner()->GetNetMode() == NM_DedicatedServer) return;

    const UARPGItemDefinition* Definition = Context.ItemDefinition;
    if (!Definition->UseSound.IsNull())
    {
        if (USoundBase* Sound = Definition->UseSound.LoadSynchronous())
        {
            const float PitchLow = FMath::Min(Definition->UseAudioPitchMin, Definition->UseAudioPitchMax);
            const float PitchHigh = FMath::Max(Definition->UseAudioPitchMin, Definition->UseAudioPitchMax);
            UGameplayStatics::PlaySoundAtLocation(this, Sound, GetOwner()->GetActorLocation(), FMath::Max(0.f, Definition->UseAudioVolume), FMath::FRandRange(PitchLow, PitchHigh));
        }
    }

    if (!Definition->UseMontage.IsNull())
    {
        if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
            if (UAnimMontage* Montage = Definition->UseMontage.LoadSynchronous()) Character->PlayAnimMontage(Montage);
    }

    if (UARPGItemUseBehavior* CustomBehavior = CreateUseBehavior(Definition))
        CustomBehavior->PlayItemUsePresentation(Context);

    if (!GetOwner()->HasAuthority()) OnItemUsed.Broadcast(Context);
}

void UARPGItemUseComponent::SendResultToOwner(EARPGItemUseResult Result, FGuid ItemInstanceId, UARPGItemDefinition* Definition, const FText& FailureReason)
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;
    ClientReceiveItemUseResult(Result, ItemInstanceId, Definition, FailureReason);
}

void UARPGItemUseComponent::ServerUseItem_Implementation(FGuid ItemInstanceId, EARPGItemUseSource Source, int32 QuickAccessSlot)
{
    UARPGItemDefinition* Definition = GetInventory() ? GetInventory()->GetItemDefinitionForInstance(ItemInstanceId) : nullptr;
    FText FailureReason;
    const EARPGItemUseResult Result = UseItemAuthority(ItemInstanceId, Source, QuickAccessSlot, FailureReason);
    SendResultToOwner(Result, ItemInstanceId, Definition, FailureReason);
}

void UARPGItemUseComponent::ClientReceiveItemUseResult_Implementation(EARPGItemUseResult Result, FGuid ItemInstanceId, UARPGItemDefinition* ItemDefinition, const FText& FailureReason)
{
    OnItemUseResult.Broadcast(Result, ItemInstanceId, ItemDefinition, FailureReason);
}

void UARPGItemUseComponent::MulticastPlayItemUsePresentation_Implementation(FGuid ItemInstanceId, UARPGItemDefinition* ItemDefinition, int32 QuantityBeforeUse, int32 ConsumeQuantity, EARPGItemUseSource Source, int32 QuickAccessSlot)
{
    FARPGItemUseContext Context;
    Context.User = GetARPGCharacter();
    Context.ItemDefinition = ItemDefinition;
    Context.ItemInstanceId = ItemInstanceId;
    Context.ItemId = ItemDefinition ? (ItemDefinition->DefinitionId.IsNone() ? ItemDefinition->GetFName() : ItemDefinition->DefinitionId) : NAME_None;
    Context.QuantityBeforeUse = QuantityBeforeUse;
    Context.ConsumeQuantity = ConsumeQuantity;
    Context.Source = Source;
    Context.QuickAccessSlot = QuickAccessSlot;
    PlayPresentationLocal(Context);
}

void UARPGItemUseComponent::OnRep_Cooldowns()
{
    OnItemUseCooldownsChanged.Broadcast();
}

void UARPGItemUseComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME_CONDITION(UARPGItemUseComponent, Cooldowns, COND_OwnerOnly);
}
