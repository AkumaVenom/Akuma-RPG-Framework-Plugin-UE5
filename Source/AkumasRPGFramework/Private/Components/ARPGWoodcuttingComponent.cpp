#include "Components/ARPGWoodcuttingComponent.h"
#include "Gathering/ARPGTree.h"
#include "Components/ARPGSkillComponent.h"
#include "Components/ARPGInventoryComponent.h"
#include "Components/ARPGCombatComponent.h"
#include "Data/ARPGSkillDefinition.h"
#include "Data/ARPGItemDefinition.h"
#include "Utilities/ARPGAssetLibrary.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "Animation/AnimMontage.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "Sound/SoundBase.h"
#include "Kismet/GameplayStatics.h"

UARPGWoodcuttingComponent::UARPGWoodcuttingComponent()
{
    SetIsReplicatedByDefault(true);
    PreferredWoodcuttingToolTag = FGameplayTag::RequestGameplayTag(TEXT("Item.Tool.Axe"), false);
}

int32 UARPGWoodcuttingComponent::GetWoodcuttingLevel() const
{
    if (const UARPGSkillComponent* Skills = GetOwner() ? GetOwner()->FindComponentByClass<UARPGSkillComponent>() : nullptr)
        return Skills->GetSkillLevel(SkillDefinition && !SkillDefinition->DefinitionId.IsNone() ? SkillDefinition->DefinitionId : SkillId);
    return 1;
}

int64 UARPGWoodcuttingComponent::GetWoodcuttingXP() const
{
    if (const UARPGSkillComponent* Skills = GetOwner() ? GetOwner()->FindComponentByClass<UARPGSkillComponent>() : nullptr)
        return Skills->GetSkillXP(SkillDefinition && !SkillDefinition->DefinitionId.IsNone() ? SkillDefinition->DefinitionId : SkillId);
    return 0;
}

int64 UARPGWoodcuttingComponent::GetWoodcuttingXPForNextLevel() const
{
    const UARPGSkillComponent* Skills = GetOwner() ? GetOwner()->FindComponentByClass<UARPGSkillComponent>() : nullptr;
    if (!Skills) return 1;
    const int32 Level = GetWoodcuttingLevel();
    int64 Needed = Skills->GetXPForNextLevel(Level);
    if (SkillDefinition)
    {
        const FRichCurve* Curve = SkillDefinition->XPRequiredPerLevel.GetRichCurveConst();
        if (Curve && Curve->GetNumKeys() > 0)
            Needed = FMath::Max<int64>(1, FMath::RoundToInt64(Curve->Eval(static_cast<float>(Level), static_cast<float>(Needed))));
    }
    return FMath::Max<int64>(1, Needed);
}

int64 UARPGWoodcuttingComponent::GetWoodcuttingXPRemaining() const
{
    return FMath::Max<int64>(0, GetWoodcuttingXPForNextLevel() - GetWoodcuttingXP());
}

float UARPGWoodcuttingComponent::GetWoodcuttingLevelProgress() const
{
    const int64 Needed = GetWoodcuttingXPForNextLevel();
    return Needed > 0 ? FMath::Clamp(static_cast<float>(GetWoodcuttingXP()) / static_cast<float>(Needed), 0.f, 1.f) : 1.f;
}

bool UARPGWoodcuttingComponent::HasWoodcuttingUnlock(FGameplayTag UnlockTag) const
{
    if (!UnlockTag.IsValid() || !SkillDefinition) return false;
    const int32 Level = GetWoodcuttingLevel();
    for (const FARPGSkillUnlock& Unlock : SkillDefinition->Unlocks)
        if (Unlock.UnlockTag == UnlockTag && Level >= Unlock.RequiredLevel) return true;
    return false;
}

UARPGItemDefinition* UARPGWoodcuttingComponent::FindBestToolForTree(const AARPGTree* Tree, FGuid* OutInstanceId) const
{
    if (OutInstanceId) *OutInstanceId = FGuid();
    const UARPGInventoryComponent* Inventory = GetOwner() ? GetOwner()->FindComponentByClass<UARPGInventoryComponent>() : nullptr;
    if (!Inventory) return nullptr;

    FGameplayTag DesiredTag;
    if (Tree && Tree->RequiredToolTag.IsValid()) DesiredTag = Tree->RequiredToolTag;
    else DesiredTag = PreferredWoodcuttingToolTag;

    UARPGItemDefinition* Best = nullptr;
    for (const FARPGInventoryEntry& Entry : Inventory->Items)
    {
        // A definition existing in the project is never enough. Woodcutting only accepts a real, positive-quantity
        // runtime inventory instance that is marked equipped in the same slot authored by that definition.
        if (!Entry.InstanceId.IsValid() || Entry.Quantity <= 0 || !Entry.bEquipped || !Entry.EquipmentSlot.IsValid()) continue;
        UARPGItemDefinition* Def = Inventory->ResolveItemDefinition(Entry);
        if (!Def || !Def->bEquippable || !Def->EquipmentSlot.IsValid() || Entry.EquipmentSlot != Def->EquipmentSlot) continue;

        const bool bMatchesDesired = DesiredTag.IsValid()
            ? (Def->GatheringToolTags.HasTag(DesiredTag) || Def->ItemTags.HasTag(DesiredTag))
            : !Def->GatheringToolTags.IsEmpty();
        if (!bMatchesDesired) continue;

        if (!Best || Def->GatheringToolTier > Best->GatheringToolTier ||
            (Def->GatheringToolTier == Best->GatheringToolTier && Def->GatheringPower > Best->GatheringPower))
        {
            Best = Def;
            if (OutInstanceId) *OutInstanceId = Entry.InstanceId;
        }
    }
    return Best;
}

UARPGItemDefinition* UARPGWoodcuttingComponent::GetBestEquippedWoodcuttingTool() const
{
    return FindBestToolForTree(nullptr);
}

FGuid UARPGWoodcuttingComponent::GetBestEquippedWoodcuttingToolInstanceId() const
{
    FGuid InstanceId;
    FindBestToolForTree(nullptr, &InstanceId);
    return InstanceId;
}

float UARPGWoodcuttingComponent::GetBestEquippedToolPower() const
{
    if (const UARPGItemDefinition* Tool = GetBestEquippedWoodcuttingTool()) return FMath::Max(0.01f, Tool->GatheringPower);
    return 1.f;
}

int32 UARPGWoodcuttingComponent::GetBestEquippedToolTier() const
{
    if (const UARPGItemDefinition* Tool = GetBestEquippedWoodcuttingTool()) return FMath::Max(0, Tool->GatheringToolTier);
    return 0;
}

bool UARPGWoodcuttingComponent::HasEquippedWoodcuttingTool() const
{
    return GetBestEquippedWoodcuttingTool() != nullptr;
}

bool UARPGWoodcuttingComponent::HasValidToolForTree(const AARPGTree* Tree) const
{
    if (!Tree || !Tree->bRequireWoodcuttingTool) return true;
    const UARPGItemDefinition* Tool = FindBestToolForTree(Tree);
    return Tool && Tool->GatheringToolTier >= Tree->MinimumToolTier;
}

bool UARPGWoodcuttingComponent::IsOwnerAlive() const
{
    if (const UARPGCombatComponent* Combat = GetOwner() ? GetOwner()->FindComponentByClass<UARPGCombatComponent>() : nullptr)
        return Combat->IsAlive();
    return GetOwner() != nullptr;
}

bool UARPGWoodcuttingComponent::CanChopTree(const AARPGTree* Tree, FText& OutFailureReason) const
{
    OutFailureReason = FText::GetEmpty();
    if (!GetOwner() || !IsValid(Tree))
    {
        OutFailureReason = FText::FromString(TEXT("No tree selected."));
        return false;
    }
    if (!IsOwnerAlive())
    {
        OutFailureReason = FText::FromString(TEXT("You cannot chop while dead."));
        return false;
    }
    if (FVector::DistSquared(GetOwner()->GetActorLocation(), Tree->GetActorLocation()) > FMath::Square(MaxChopDistance))
    {
        OutFailureReason = FText::FromString(TEXT("The tree is too far away."));
        return false;
    }
    if (!Tree->CanBeChoppedBy(GetOwner(), OutFailureReason)) return false;
    return true;
}

float UARPGWoodcuttingComponent::CalculateChopPower(const AARPGTree* Tree) const
{
    const int32 Level = FMath::Max(1, GetWoodcuttingLevel());
    const UARPGItemDefinition* Tool = bUseBestEquippedTool ? FindBestToolForTree(Tree) : nullptr;
    const float ToolPower = Tool ? FMath::Max(0.01f, Tool->GatheringPower) : 1.f;
    const float SkillMultiplier = 1.f + static_cast<float>(FMath::Max(0, Level - 1)) * FMath::Max(0.f, SkillPowerPerLevel);
    return FMath::Max(0.01f, BaseChopPower * ToolPower * SkillMultiplier);
}

void UARPGWoodcuttingComponent::AwardWoodcuttingXP(int64 Amount)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || Amount <= 0) return;
    if (UARPGSkillComponent* Skills = GetOwner()->FindComponentByClass<UARPGSkillComponent>())
    {
        if (SkillDefinition) Skills->AddSkillXPFromDefinition(SkillDefinition, Amount);
        else Skills->AddSkillXP(SkillId, Amount);
    }
}

AARPGTree* UARPGWoodcuttingComponent::FindWoodcuttingTreeInView() const
{
    if (!GetOwner() || !GetWorld()) return nullptr;
    FVector Start = GetOwner()->GetActorLocation();
    FRotator ViewRotation = GetOwner()->GetActorRotation();
    if (const APawn* Pawn = Cast<APawn>(GetOwner()))
    {
        if (const APlayerController* PC = Cast<APlayerController>(Pawn->GetController()))
            PC->GetPlayerViewPoint(Start, ViewRotation);
        else Pawn->GetActorEyesViewPoint(Start, ViewRotation);
    }
    const FVector End = Start + ViewRotation.Vector() * FMath::Max(50.f, AutoTargetTraceDistance);
    FHitResult Hit;
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(GetOwner());
    const bool bHit = AutoTargetTraceRadius > KINDA_SMALL_NUMBER
        ? GetWorld()->SweepSingleByChannel(Hit, Start, End, FQuat::Identity, AutoTargetTraceChannel, FCollisionShape::MakeSphere(AutoTargetTraceRadius), Params)
        : GetWorld()->LineTraceSingleByChannel(Hit, Start, End, AutoTargetTraceChannel, Params);
    return bHit ? Cast<AARPGTree>(Hit.GetActor()) : nullptr;
}

AARPGTree* UARPGWoodcuttingComponent::ResolveBasicAttackTree(AActor* OptionalTarget) const
{
    if (OptionalTarget)
    {
        // A real combat/lock-on target always wins. Only redirect an explicit target when it is actually a tree.
        return Cast<AARPGTree>(OptionalTarget);
    }
    return FindWoodcuttingTreeInView();
}

bool UARPGWoodcuttingComponent::TryHandleBasicAttackAsWoodcutting(AActor* OptionalTarget, bool& bOutHandled)
{
    bOutHandled = false;
    if (!GetOwner() || !GetOwner()->HasAuthority() || !bAutoChopTreesWithBasicAttack) return false;

    AARPGTree* Tree = ResolveBasicAttackTree(OptionalTarget);
    if (!IsValid(Tree)) return false;

    // Basic Attack should only become gathering when the player is visibly/effectively holding a Woodcutting tool.
    // Interact/Start Woodcutting remains available for trees that intentionally allow tool-less harvesting.
    if (bBasicAttackRequiresEquippedAxe && !HasEquippedWoodcuttingTool()) return false;

    bOutHandled = true;
    FText Reason;
    if (!CanChopTree(Tree, Reason))
    {
        ClientWoodcuttingResult(false, Reason);
        return false;
    }

    // Manual attack-chops respect the same authored swing interval, preventing input spam from becoming
    // a faster harvesting path than the normal Woodcutting cadence.
    const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
    if (Now - LastBasicAttackChopAt < FMath::Max(0.1f, SwingIntervalSeconds)) return false;
    LastBasicAttackChopAt = Now;

    // Attack input is a manual single chop. The separate Interact workflow keeps its automatic repeated chopping.
    return BeginWoodcuttingAuthority(Tree, true);
}

bool UARPGWoodcuttingComponent::TryChopTreeWithBasicAttack(AActor* OptionalTarget)
{
    if (!GetOwner() || !bAutoChopTreesWithBasicAttack) return false;

    AARPGTree* Tree = ResolveBasicAttackTree(OptionalTarget);
    if (!IsValid(Tree)) return false;
    if (bBasicAttackRequiresEquippedAxe && !HasEquippedWoodcuttingTool()) return false;

    if (GetOwner()->HasAuthority())
    {
        bool bHandled = false;
        return TryHandleBasicAttackAsWoodcutting(Tree, bHandled);
    }
    return ChopTreeOnce(Tree);
}

bool UARPGWoodcuttingComponent::StartWoodcuttingFromView()
{
    AARPGTree* Tree = FindWoodcuttingTreeInView();
    if (!Tree)
    {
        OnWoodcuttingResult.Broadcast(false, FText::FromString(TEXT("No harvestable tree in view.")));
        return false;
    }
    return StartWoodcutting(Tree);
}

bool UARPGWoodcuttingComponent::StartWoodcutting(AARPGTree* Tree)
{
    if (!GetOwner() || !IsValid(Tree)) return false;
    FText Reason;
    if (!CanChopTree(Tree, Reason))
    {
        OnWoodcuttingResult.Broadcast(false, Reason);
        return false;
    }
    if (GetOwner()->HasAuthority()) return BeginWoodcuttingAuthority(Tree, false);
    ServerStartWoodcutting(Tree, false);
    return true;
}

bool UARPGWoodcuttingComponent::ChopTreeOnce(AARPGTree* Tree)
{
    if (!GetOwner() || !IsValid(Tree)) return false;
    FText Reason;
    if (!CanChopTree(Tree, Reason))
    {
        OnWoodcuttingResult.Broadcast(false, Reason);
        return false;
    }
    if (GetOwner()->HasAuthority()) return BeginWoodcuttingAuthority(Tree, true);
    ServerStartWoodcutting(Tree, true);
    return true;
}

void UARPGWoodcuttingComponent::StopWoodcutting()
{
    if (!GetOwner()) return;
    if (GetOwner()->HasAuthority()) StopWoodcuttingAuthority();
    else ServerStopWoodcutting();
}

bool UARPGWoodcuttingComponent::BeginWoodcuttingAuthority(AARPGTree* Tree, bool bSingleSwing)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || !IsValid(Tree)) return false;
    FText Reason;
    if (!CanChopTree(Tree, Reason))
    {
        ClientWoodcuttingResult(false, Reason);
        return false;
    }

    AARPGTree* OldTree = CurrentTree;
    CurrentTree = Tree;
    bSingleSwingActive = bSingleSwing || !bAutoRepeatChops;
    if (OldTree != CurrentTree) OnWoodcuttingStarted.Broadcast(CurrentTree);

    GetWorld()->GetTimerManager().ClearTimer(SwingTimer);
    GetWorld()->GetTimerManager().ClearTimer(ImpactTimer);
    StartSwingAuthority();

    if (!bSingleSwingActive && bAutoRepeatChops)
    {
        const float Interval = FMath::Max(0.1f, SwingIntervalSeconds);
        GetWorld()->GetTimerManager().SetTimer(SwingTimer, this, &UARPGWoodcuttingComponent::StartSwingAuthority, Interval, true, Interval);
    }
    ClientWoodcuttingResult(true, FText::FromString(TEXT("Woodcutting started.")));
    return true;
}

void UARPGWoodcuttingComponent::StartSwingAuthority()
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || !IsValid(CurrentTree))
    {
        StopWoodcuttingAuthority(FText::FromString(TEXT("The tree is no longer available.")), true);
        return;
    }
    if (GetWorld()->GetTimerManager().IsTimerActive(ImpactTimer)) return;

    FText Reason;
    if (!CanChopTree(CurrentTree, Reason))
    {
        StopWoodcuttingAuthority(Reason, true);
        return;
    }

    MulticastPlayChopMontage(CurrentTree, FindBestToolForTree(CurrentTree));
    const float Delay = FMath::Clamp(SwingImpactDelay, 0.f, FMath::Max(0.f, SwingIntervalSeconds * 0.9f));
    if (Delay <= KINDA_SMALL_NUMBER) ResolveSwingImpactAuthority();
    else GetWorld()->GetTimerManager().SetTimer(ImpactTimer, this, &UARPGWoodcuttingComponent::ResolveSwingImpactAuthority, Delay, false);
}

void UARPGWoodcuttingComponent::ResolveSwingImpactAuthority()
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || !IsValid(CurrentTree))
    {
        StopWoodcuttingAuthority(FText::FromString(TEXT("The tree is no longer available.")), true);
        return;
    }
    FText Reason;
    if (!CanChopTree(CurrentTree, Reason))
    {
        StopWoodcuttingAuthority(Reason, true);
        return;
    }

    AARPGTree* Tree = CurrentTree;
    const float Power = CalculateChopPower(Tree);
    const bool bSuccess = Tree->ApplyChop(GetOwner(), Power);
    if (!bSuccess)
    {
        StopWoodcuttingAuthority(FText::FromString(TEXT("The chop could not be applied.")), true);
        return;
    }

    OnWoodcuttingSwing.Broadcast(Tree, Power, Tree->CurrentChopHealth);
    if (bSingleSwingActive || !Tree->IsStanding())
        StopWoodcuttingAuthority();
}

void UARPGWoodcuttingComponent::StopWoodcuttingAuthority(const FText& Message, bool bReportResult)
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(SwingTimer);
        World->GetTimerManager().ClearTimer(ImpactTimer);
    }
    AARPGTree* OldTree = CurrentTree;
    CurrentTree = nullptr;
    bSingleSwingActive = false;
    if (OldTree) OnWoodcuttingStopped.Broadcast(OldTree);
    if (bReportResult) ClientWoodcuttingResult(false, Message);
}

void UARPGWoodcuttingComponent::ServerStartWoodcutting_Implementation(AARPGTree* Tree, bool bSingleSwing)
{
    BeginWoodcuttingAuthority(Tree, bSingleSwing);
}

void UARPGWoodcuttingComponent::ServerStopWoodcutting_Implementation()
{
    StopWoodcuttingAuthority();
}

void UARPGWoodcuttingComponent::ClientWoodcuttingResult_Implementation(bool bSuccess, const FText& Message)
{
    OnWoodcuttingResult.Broadcast(bSuccess, Message);
}

void UARPGWoodcuttingComponent::MulticastPlayChopMontage_Implementation(AARPGTree* Tree, UARPGItemDefinition* EquippedTool)
{
    ACharacter* Character = Cast<ACharacter>(GetOwner());
    if (!Character) return;

    TSoftObjectPtr<UAnimMontage> MontageAsset = Tree && !Tree->ChopMontageOverride.IsNull() ? Tree->ChopMontageOverride : DefaultChopMontage;
    UAnimMontage* Montage = MontageAsset.LoadSynchronous();
    if (!Montage && bUseCombatMeleeMontageAsChopFallback)
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

void UARPGWoodcuttingComponent::OnRep_CurrentTree(AARPGTree* OldTree)
{
    if (OldTree && OldTree != CurrentTree) OnWoodcuttingStopped.Broadcast(OldTree);
    if (CurrentTree && OldTree != CurrentTree) OnWoodcuttingStarted.Broadcast(CurrentTree);
}

void UARPGWoodcuttingComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(SwingTimer);
        World->GetTimerManager().ClearTimer(ImpactTimer);
    }
    CurrentTree = nullptr;
    bSingleSwingActive = false;
    Super::EndPlay(EndPlayReason);
}

void UARPGWoodcuttingComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(UARPGWoodcuttingComponent, CurrentTree);
}
