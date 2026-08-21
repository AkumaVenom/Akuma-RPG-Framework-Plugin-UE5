#include "Components/ARPGMiningComponent.h"
#include "Gathering/ARPGMineableRock.h"
#include "Components/ARPGSkillComponent.h"
#include "Components/ARPGInventoryComponent.h"
#include "Components/ARPGCombatComponent.h"
#include "Data/ARPGItemDefinition.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "Animation/AnimMontage.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "Sound/SoundBase.h"
#include "Kismet/GameplayStatics.h"

UARPGMiningComponent::UARPGMiningComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(true);
    PreferredMiningToolTag = FGameplayTag::RequestGameplayTag(TEXT("Item.Tool.Pickaxe"), false);
}

EARPGSkillXPModel UARPGMiningComponent::GetNativeXPModel() const
{
    return bUseRuneScapeStyleXPWithoutSkillDefinition ? EARPGSkillXPModel::RuneScapeStyle99 : EARPGSkillXPModel::FrameworkPower;
}

int32 UARPGMiningComponent::GetMiningLevel() const
{
    if (const UARPGSkillComponent* Skills = GetOwner() ? GetOwner()->FindComponentByClass<UARPGSkillComponent>() : nullptr)
        return Skills->GetSkillLevel(SkillDefinition && !SkillDefinition->DefinitionId.IsNone() ? SkillDefinition->DefinitionId : SkillId);
    return 1;
}

int64 UARPGMiningComponent::GetMiningXP() const
{
    if (const UARPGSkillComponent* Skills = GetOwner() ? GetOwner()->FindComponentByClass<UARPGSkillComponent>() : nullptr)
        return Skills->GetSkillXP(SkillDefinition && !SkillDefinition->DefinitionId.IsNone() ? SkillDefinition->DefinitionId : SkillId);
    return 0;
}

int64 UARPGMiningComponent::GetMiningXPForNextLevel() const
{
    const UARPGSkillComponent* Skills = GetOwner() ? GetOwner()->FindComponentByClass<UARPGSkillComponent>() : nullptr;
    if (!Skills) return 1;
    const int32 Level = GetMiningLevel();
    return SkillDefinition
        ? Skills->GetXPForNextLevelFromDefinition(SkillDefinition, Level)
        : Skills->GetXPForNextLevelForModel(Level, GetNativeXPModel());
}

int64 UARPGMiningComponent::GetMiningXPRemaining() const
{
    return FMath::Max<int64>(0, GetMiningXPForNextLevel() - GetMiningXP());
}

float UARPGMiningComponent::GetMiningLevelProgress() const
{
    const int64 Needed = GetMiningXPForNextLevel();
    return Needed > 0 ? FMath::Clamp(static_cast<float>(GetMiningXP()) / static_cast<float>(Needed), 0.f, 1.f) : 1.f;
}

bool UARPGMiningComponent::HasMiningUnlock(FGameplayTag UnlockTag) const
{
    if (!UnlockTag.IsValid() || !SkillDefinition) return false;
    const int32 Level = GetMiningLevel();
    for (const FARPGSkillUnlock& Unlock : SkillDefinition->Unlocks)
        if (Unlock.UnlockTag == UnlockTag && Level >= Unlock.RequiredLevel) return true;
    return false;
}

UARPGItemDefinition* UARPGMiningComponent::FindBestToolForRock(const AARPGMineableRock* Rock, FGuid* OutInstanceId) const
{
    if (OutInstanceId) *OutInstanceId = FGuid();
    const UARPGInventoryComponent* Inventory = GetOwner() ? GetOwner()->FindComponentByClass<UARPGInventoryComponent>() : nullptr;
    if (!Inventory) return nullptr;

    FGameplayTag DesiredTag;
    if (Rock && Rock->RequiredToolTag.IsValid()) DesiredTag = Rock->RequiredToolTag;
    else DesiredTag = PreferredMiningToolTag;

    UARPGItemDefinition* Best = nullptr;
    for (const FARPGInventoryEntry& Entry : Inventory->Items)
    {
        // Never infer equipment from a Data Asset merely existing. Mining consumes a real equipped runtime
        // instance with positive quantity, matching slot, valid durability and the required Gathering tag.
        if (!Entry.InstanceId.IsValid() || Entry.Quantity <= 0 || !Entry.bEquipped || !Entry.EquipmentSlot.IsValid()) continue;
        UARPGItemDefinition* Definition = Inventory->ResolveItemDefinition(Entry);
        if (!Definition || !Definition->bEquippable || !Definition->EquipmentSlot.IsValid() || Entry.EquipmentSlot != Definition->EquipmentSlot) continue;
        if (Definition->bUsesDurability && Entry.Durability <= KINDA_SMALL_NUMBER) continue;

        const bool bMatchesDesired = DesiredTag.IsValid()
            ? (Definition->GatheringToolTags.HasTag(DesiredTag) || Definition->ItemTags.HasTag(DesiredTag))
            : !Definition->GatheringToolTags.IsEmpty();
        if (!bMatchesDesired) continue;

        if (!Best || Definition->GatheringToolTier > Best->GatheringToolTier ||
            (Definition->GatheringToolTier == Best->GatheringToolTier && Definition->GatheringPower > Best->GatheringPower))
        {
            Best = Definition;
            if (OutInstanceId) *OutInstanceId = Entry.InstanceId;
        }
    }
    return Best;
}

UARPGItemDefinition* UARPGMiningComponent::GetBestEquippedMiningTool() const
{
    return FindBestToolForRock(nullptr);
}

FGuid UARPGMiningComponent::GetBestEquippedMiningToolInstanceId() const
{
    FGuid InstanceId;
    FindBestToolForRock(nullptr, &InstanceId);
    return InstanceId;
}

float UARPGMiningComponent::GetBestEquippedToolPower() const
{
    if (const UARPGItemDefinition* Tool = GetBestEquippedMiningTool()) return FMath::Max(0.01f, Tool->GatheringPower);
    return 1.f;
}

int32 UARPGMiningComponent::GetBestEquippedToolTier() const
{
    if (const UARPGItemDefinition* Tool = GetBestEquippedMiningTool()) return FMath::Max(0, Tool->GatheringToolTier);
    return 0;
}

bool UARPGMiningComponent::HasEquippedMiningTool() const
{
    return GetBestEquippedMiningTool() != nullptr;
}

bool UARPGMiningComponent::HasValidToolForRock(const AARPGMineableRock* Rock) const
{
    if (!Rock || !Rock->bRequireMiningTool) return true;
    const UARPGItemDefinition* Tool = FindBestToolForRock(Rock);
    return Tool && Tool->GatheringToolTier >= Rock->MinimumToolTier;
}

bool UARPGMiningComponent::IsOwnerAlive() const
{
    if (const UARPGCombatComponent* Combat = GetOwner() ? GetOwner()->FindComponentByClass<UARPGCombatComponent>() : nullptr)
        return Combat->IsAlive();
    return GetOwner() != nullptr;
}

bool UARPGMiningComponent::CanMineRock(const AARPGMineableRock* Rock, FText& OutFailureReason) const
{
    OutFailureReason = FText::GetEmpty();
    if (!GetOwner() || !IsValid(Rock))
    {
        OutFailureReason = FText::FromString(TEXT("No mineable rock selected."));
        return false;
    }
    if (!IsOwnerAlive())
    {
        OutFailureReason = FText::FromString(TEXT("You cannot mine while dead."));
        return false;
    }
    if (FVector::DistSquared(GetOwner()->GetActorLocation(), Rock->GetActorLocation()) > FMath::Square(MaxMiningDistance))
    {
        OutFailureReason = FText::FromString(TEXT("The resource node is too far away."));
        return false;
    }
    return Rock->CanBeMinedBy(GetOwner(), OutFailureReason);
}

float UARPGMiningComponent::CalculateMiningPower(const AARPGMineableRock* Rock) const
{
    const int32 Level = FMath::Max(1, GetMiningLevel());
    const UARPGItemDefinition* Tool = bUseBestEquippedTool ? FindBestToolForRock(Rock) : nullptr;
    const float ToolPower = Tool ? FMath::Max(0.01f, Tool->GatheringPower) : 1.f;
    const float SkillMultiplier = 1.f + static_cast<float>(FMath::Max(0, Level - 1)) * FMath::Max(0.f, SkillPowerPerLevel);
    return FMath::Max(0.01f, BaseMiningPower * ToolPower * SkillMultiplier);
}

void UARPGMiningComponent::AwardMiningXP(int64 Amount)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || Amount <= 0) return;
    if (UARPGSkillComponent* Skills = GetOwner()->FindComponentByClass<UARPGSkillComponent>())
    {
        if (SkillDefinition) Skills->AddSkillXPFromDefinition(SkillDefinition, Amount);
        else Skills->AddSkillXPWithModel(SkillId, Amount, FMath::Max(1, NativeMiningMaxLevel), GetNativeXPModel());
    }
}

AARPGMineableRock* UARPGMiningComponent::FindMineableRockInView() const
{
    if (!GetOwner() || !GetWorld()) return nullptr;
    FVector Start = GetOwner()->GetActorLocation();
    FRotator ViewRotation = GetOwner()->GetActorRotation();
    if (const APawn* Pawn = Cast<APawn>(GetOwner()))
    {
        if (const APlayerController* PC = Cast<APlayerController>(Pawn->GetController())) PC->GetPlayerViewPoint(Start, ViewRotation);
        else Pawn->GetActorEyesViewPoint(Start, ViewRotation);
    }

    const FVector End = Start + ViewRotation.Vector() * FMath::Max(50.f, AutoTargetTraceDistance);
    FHitResult Hit;
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(GetOwner());
    const bool bHit = AutoTargetTraceRadius > KINDA_SMALL_NUMBER
        ? GetWorld()->SweepSingleByChannel(Hit, Start, End, FQuat::Identity, AutoTargetTraceChannel, FCollisionShape::MakeSphere(AutoTargetTraceRadius), Params)
        : GetWorld()->LineTraceSingleByChannel(Hit, Start, End, AutoTargetTraceChannel, Params);
    return bHit ? Cast<AARPGMineableRock>(Hit.GetActor()) : nullptr;
}

AARPGMineableRock* UARPGMiningComponent::ResolveBasicAttackRock(AActor* OptionalTarget) const
{
    if (OptionalTarget) return Cast<AARPGMineableRock>(OptionalTarget);
    return FindMineableRockInView();
}

bool UARPGMiningComponent::TryHandleBasicAttackAsMining(AActor* OptionalTarget, bool& bOutHandled)
{
    bOutHandled = false;
    if (!GetOwner() || !GetOwner()->HasAuthority() || !bAutoMineRocksWithBasicAttack) return false;

    AARPGMineableRock* Rock = ResolveBasicAttackRock(OptionalTarget);
    if (!IsValid(Rock)) return false;
    if (bBasicAttackRequiresEquippedPickaxe && !HasEquippedMiningTool()) return false;

    bOutHandled = true;
    FText Reason;
    if (!CanMineRock(Rock, Reason))
    {
        ClientMiningResult(false, Reason);
        return false;
    }

    const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
    if (Now - LastBasicAttackMiningAt < FMath::Max(0.1f, SwingIntervalSeconds)) return false;
    LastBasicAttackMiningAt = Now;
    return BeginMiningAuthority(Rock, true);
}

bool UARPGMiningComponent::TryMineRockWithBasicAttack(AActor* OptionalTarget)
{
    if (!GetOwner() || !bAutoMineRocksWithBasicAttack) return false;
    AARPGMineableRock* Rock = ResolveBasicAttackRock(OptionalTarget);
    if (!IsValid(Rock)) return false;
    if (bBasicAttackRequiresEquippedPickaxe && !HasEquippedMiningTool()) return false;

    if (GetOwner()->HasAuthority())
    {
        bool bHandled = false;
        return TryHandleBasicAttackAsMining(Rock, bHandled);
    }
    return MineRockOnce(Rock);
}

bool UARPGMiningComponent::StartMiningFromView()
{
    AARPGMineableRock* Rock = FindMineableRockInView();
    if (!Rock)
    {
        OnMiningResult.Broadcast(false, FText::FromString(TEXT("No mineable resource in view.")));
        return false;
    }
    return StartMining(Rock);
}

bool UARPGMiningComponent::StartMining(AARPGMineableRock* Rock)
{
    if (!GetOwner() || !IsValid(Rock)) return false;
    FText Reason;
    if (!CanMineRock(Rock, Reason))
    {
        OnMiningResult.Broadcast(false, Reason);
        return false;
    }
    if (GetOwner()->HasAuthority()) return BeginMiningAuthority(Rock, false);
    ServerStartMining(Rock, false);
    return true;
}

bool UARPGMiningComponent::MineRockOnce(AARPGMineableRock* Rock)
{
    if (!GetOwner() || !IsValid(Rock)) return false;
    FText Reason;
    if (!CanMineRock(Rock, Reason))
    {
        OnMiningResult.Broadcast(false, Reason);
        return false;
    }
    if (GetOwner()->HasAuthority()) return BeginMiningAuthority(Rock, true);
    ServerStartMining(Rock, true);
    return true;
}

void UARPGMiningComponent::StopMining()
{
    if (!GetOwner()) return;
    if (GetOwner()->HasAuthority()) StopMiningAuthority();
    else ServerStopMining();
}

bool UARPGMiningComponent::BeginMiningAuthority(AARPGMineableRock* Rock, bool bSingleSwing)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || !IsValid(Rock)) return false;
    FText Reason;
    if (!CanMineRock(Rock, Reason))
    {
        ClientMiningResult(false, Reason);
        return false;
    }

    AARPGMineableRock* OldRock = CurrentRock;
    CurrentRock = Rock;
    bSingleSwingActive = bSingleSwing || !bAutoRepeatMining;
    if (OldRock != CurrentRock) OnMiningStarted.Broadcast(CurrentRock);

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(SwingTimer);
        World->GetTimerManager().ClearTimer(ImpactTimer);
    }
    StartSwingAuthority();

    if (!bSingleSwingActive && bAutoRepeatMining)
    {
        const float Interval = FMath::Max(0.1f, SwingIntervalSeconds);
        GetWorld()->GetTimerManager().SetTimer(SwingTimer, this, &UARPGMiningComponent::StartSwingAuthority, Interval, true, Interval);
    }
    ClientMiningResult(true, FText::FromString(TEXT("Mining started.")));
    return true;
}

void UARPGMiningComponent::StartSwingAuthority()
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || !IsValid(CurrentRock))
    {
        StopMiningAuthority(FText::FromString(TEXT("The resource node is no longer available.")), true);
        return;
    }
    if (GetWorld()->GetTimerManager().IsTimerActive(ImpactTimer)) return;

    FText Reason;
    if (!CanMineRock(CurrentRock, Reason))
    {
        StopMiningAuthority(Reason, true);
        return;
    }

    MulticastPlayMiningMontage(CurrentRock, FindBestToolForRock(CurrentRock));
    const float Delay = FMath::Clamp(SwingImpactDelay, 0.f, FMath::Max(0.f, SwingIntervalSeconds * 0.9f));
    if (Delay <= KINDA_SMALL_NUMBER) ResolveSwingImpactAuthority();
    else GetWorld()->GetTimerManager().SetTimer(ImpactTimer, this, &UARPGMiningComponent::ResolveSwingImpactAuthority, Delay, false);
}

void UARPGMiningComponent::ResolveSwingImpactAuthority()
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || !IsValid(CurrentRock))
    {
        StopMiningAuthority(FText::FromString(TEXT("The resource node is no longer available.")), true);
        return;
    }

    FText Reason;
    if (!CanMineRock(CurrentRock, Reason))
    {
        StopMiningAuthority(Reason, true);
        return;
    }

    AARPGMineableRock* Rock = CurrentRock;
    FGuid ToolInstanceId;
    UARPGItemDefinition* ToolDefinition = FindBestToolForRock(Rock, &ToolInstanceId);
    const float Power = CalculateMiningPower(Rock);
    const bool bSuccess = Rock->ApplyMiningStrike(GetOwner(), Power);
    if (!bSuccess)
    {
        StopMiningAuthority(FText::FromString(TEXT("The mining strike could not be applied.")), true);
        return;
    }

    // Durability is charged only to the exact equipped runtime tool that successfully produced the strike.
    if (ToolDefinition && ToolDefinition->bUsesDurability && ToolDefinition->bLoseDurabilityOnGatheringHit && ToolInstanceId.IsValid())
    {
        if (UARPGInventoryComponent* Inventory = GetOwner()->FindComponentByClass<UARPGInventoryComponent>())
            Inventory->DamageItemDurability(ToolInstanceId, FMath::Max(0.f, ToolDefinition->GatheringDurabilityLossPerSuccessfulHit));
    }

    OnMiningSwing.Broadcast(Rock, Power, Rock->CurrentMiningHealth);
    if (bSingleSwingActive || !Rock->IsAvailable()) StopMiningAuthority();
}

void UARPGMiningComponent::StopMiningAuthority(const FText& Message, bool bReportResult)
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(SwingTimer);
        World->GetTimerManager().ClearTimer(ImpactTimer);
    }
    AARPGMineableRock* OldRock = CurrentRock;
    CurrentRock = nullptr;
    bSingleSwingActive = false;
    if (OldRock) OnMiningStopped.Broadcast(OldRock);
    if (bReportResult) ClientMiningResult(false, Message);
}

void UARPGMiningComponent::ServerStartMining_Implementation(AARPGMineableRock* Rock, bool bSingleSwing)
{
    BeginMiningAuthority(Rock, bSingleSwing);
}

void UARPGMiningComponent::ServerStopMining_Implementation()
{
    StopMiningAuthority();
}

void UARPGMiningComponent::ClientMiningResult_Implementation(bool bSuccess, const FText& Message)
{
    OnMiningResult.Broadcast(bSuccess, Message);
}

void UARPGMiningComponent::MulticastPlayMiningMontage_Implementation(AARPGMineableRock* Rock, UARPGItemDefinition* EquippedTool)
{
    ACharacter* Character = Cast<ACharacter>(GetOwner());
    if (!Character) return;

    TSoftObjectPtr<UAnimMontage> MontageAsset = Rock && !Rock->MiningMontageOverride.IsNull() ? Rock->MiningMontageOverride : DefaultMiningMontage;
    UAnimMontage* Montage = MontageAsset.LoadSynchronous();
    if (!Montage && bUseCombatMeleeMontageAsMiningFallback)
    {
        if (const UARPGCombatComponent* Combat = GetOwner() ? GetOwner()->FindComponentByClass<UARPGCombatComponent>() : nullptr)
            Montage = Combat->PickRandomAttackMontage(false, false);
    }
    if (Montage) Character->PlayAnimMontage(Montage);

    if (EquippedTool)
    {
        const TSoftObjectPtr<USoundBase>& SwingAsset = !EquippedTool->GatheringSwingSound.IsNull() ? EquippedTool->GatheringSwingSound : EquippedTool->CombatSwingSound;
        if (!SwingAsset.IsNull() && GetOwner()->GetNetMode() != NM_DedicatedServer)
            if (USoundBase* Sound = SwingAsset.LoadSynchronous())
            {
                const float PitchLow = FMath::Min(EquippedTool->EquipmentAudioPitchMin, EquippedTool->EquipmentAudioPitchMax);
                const float PitchHigh = FMath::Max(EquippedTool->EquipmentAudioPitchMin, EquippedTool->EquipmentAudioPitchMax);
                UGameplayStatics::PlaySoundAtLocation(this, Sound, GetOwner()->GetActorLocation(), FMath::Max(0.f, EquippedTool->EquipmentAudioVolume), FMath::FRandRange(PitchLow, PitchHigh), 0.f, nullptr, nullptr, nullptr);
            }
    }
}

void UARPGMiningComponent::OnRep_CurrentRock(AARPGMineableRock* OldRock)
{
    if (OldRock && OldRock != CurrentRock) OnMiningStopped.Broadcast(OldRock);
    if (CurrentRock && OldRock != CurrentRock) OnMiningStarted.Broadcast(CurrentRock);
}

void UARPGMiningComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(SwingTimer);
        World->GetTimerManager().ClearTimer(ImpactTimer);
    }
    CurrentRock = nullptr;
    bSingleSwingActive = false;
    Super::EndPlay(EndPlayReason);
}

void UARPGMiningComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(UARPGMiningComponent, CurrentRock);
}
