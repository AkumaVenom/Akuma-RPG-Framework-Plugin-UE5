#include "Components/ARPGStatsComponent.h"
#include "Components/ARPGProgressionComponent.h"
#include "Components/ARPGInventoryComponent.h"
#include "Components/ARPGCombatComponent.h"
#include "Data/ARPGItemDefinition.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"

namespace
{
    static void AddPrimary(FARPGPrimaryStatBlock& A, const FARPGPrimaryStatBlock& B)
    {
        A.Strength += B.Strength;
        A.Vitality += B.Vitality;
        A.Magic += B.Magic;
        A.Spirit += B.Spirit;
        A.Dexterity += B.Dexterity;
        A.Luck += B.Luck;
    }

    static void AddModifier(FARPGStatModifier& A, const FARPGStatModifier& B)
    {
        AddPrimary(A.PrimaryAdd, B.PrimaryAdd);
        A.MeleeAttackPower += B.MeleeAttackPower;
        A.RangedAttackPower += B.RangedAttackPower;
        A.MagicAttackPower += B.MagicAttackPower;
        A.PhysicalDefense += B.PhysicalDefense;
        A.MagicDefense += B.MagicDefense;
        A.Accuracy += B.Accuracy;
        A.Evasion += B.Evasion;
        A.MagicEvasion += B.MagicEvasion;
        A.Speed += B.Speed;
        A.CriticalChance += B.CriticalChance;
        A.CriticalDamageMultiplierBonus += B.CriticalDamageMultiplierBonus;
        A.AttackSpeedMultiplierBonus += B.AttackSpeedMultiplierBonus;
        A.MovementSpeedMultiplierBonus += B.MovementSpeedMultiplierBonus;
        A.MaxHealth += B.MaxHealth;
        A.MaxMana += B.MaxMana;
        A.MaxStamina += B.MaxStamina;
    }
}

UARPGStatsComponent::UARPGStatsComponent()
{
    SetIsReplicatedByDefault(true);
    PrimaryComponentTick.bCanEverTick = false;
}

void UARPGStatsComponent::BeginPlay()
{
    Super::BeginPlay();

    if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
        if (UCharacterMovementComponent* Move = Character->GetCharacterMovement())
            CachedBaseWalkSpeed = FMath::Max(1.f, Move->MaxWalkSpeed);

    if (UARPGProgressionComponent* Progression = GetOwner() ? GetOwner()->FindComponentByClass<UARPGProgressionComponent>() : nullptr)
        Progression->OnLevelChanged.AddDynamic(this, &UARPGStatsComponent::HandleLevelChanged);

    if (UARPGInventoryComponent* Inventory = GetOwner() ? GetOwner()->FindComponentByClass<UARPGInventoryComponent>() : nullptr)
        Inventory->OnInventoryChanged.AddDynamic(this, &UARPGStatsComponent::HandleInventoryChanged);

    // v2.5.3: Scale NPC To Player is the master opt-in for AI level/stat scaling. Do not let an
    // editor dependency make the feature silently inert: a non-player Pawn that explicitly enables
    // scaling automatically opts into the JRPG stat layer on the authority at runtime. Player pawns
    // are never auto-converted by this NPC-only setting. Designers can still enable JRPG Stats in the
    // editor to author Growth/Derived settings before play.
    if (GetOwner() && GetOwner()->HasAuthority() && NPCLevelScalingSettings.bScaleToPlayer && !bEnableJRPGStatSystem)
    {
        if (const APawn* OwnerPawn = Cast<APawn>(GetOwner()))
        {
            if (!OwnerPawn->IsPlayerControlled())
                bEnableJRPGStatSystem = true;
        }
    }

    if (GetOwner() && GetOwner()->HasAuthority() && bEnableJRPGStatSystem)
    {
        const int32 BaseLevel = GetBaseProgressionLevel();
        InitializeStatProgressionForLevel(BaseLevel, false);
        if (CanUseNPCLevelScaling())
        {
            RefreshNPCLevelScalingInternal(true, false);
            EnsureNPCLevelScalingTimer();
        }
        else
        {
            NPCLevelScalingRuntime.BaseLevel = BaseLevel;
            NPCLevelScalingRuntime.EffectiveLevel = BaseLevel;
            RecalculateJRPGStatsInternal(false);
        }
    }
}

void UARPGStatsComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    ClearNPCLevelScalingTimer();
    Super::EndPlay(EndPlayReason);
}

bool UARPGStatsComponent::ApplyDamage(float Amount)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || Amount <= 0.f || Health <= 0.f) return false;
    const float Old = Health;
    Health = FMath::Clamp(Health - Amount, 0.f, MaxHealth);
    OnHealthChanged.Broadcast(Health, Health - Old);
    if (Health <= 0.f) OnDeath.Broadcast();
    return true;
}

bool UARPGStatsComponent::Heal(float Amount)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || Amount <= 0.f || Health <= 0.f) return false;
    const float Old = Health;
    Health = FMath::Clamp(Health + Amount, 0.f, MaxHealth);
    const float AppliedDelta = Health - Old;
    if (AppliedDelta <= KINDA_SMALL_NUMBER) return false;
    OnHealthChanged.Broadcast(Health, AppliedDelta);
    return true;
}

bool UARPGStatsComponent::SpendMana(float Amount)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || Amount < 0.f || Mana < Amount) return false;
    Mana -= Amount;
    return true;
}

bool UARPGStatsComponent::SpendStamina(float Amount)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || Amount < 0.f || Stamina < Amount) return false;
    Stamina = FMath::Clamp(Stamina - Amount, 0.f, MaxStamina);
    return true;
}

void UARPGStatsComponent::RestoreMana(float Amount)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || Amount <= 0.f) return;
    Mana = FMath::Clamp(Mana + Amount, 0.f, MaxMana);
}

void UARPGStatsComponent::RestoreStamina(float Amount)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || Amount <= 0.f) return;
    Stamina = FMath::Clamp(Stamina + Amount, 0.f, MaxStamina);
}

void UARPGStatsComponent::RestoreAllVitals()
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;
    const float Old = Health;
    Health = MaxHealth;
    Mana = MaxMana;
    Stamina = MaxStamina;
    OnHealthChanged.Broadcast(Health, Health - Old);
}

float UARPGStatsComponent::GetHealthPercent() const
{
    return MaxHealth > 0.f ? Health / MaxHealth : 0.f;
}

int32* UARPGStatsComponent::ResolveAllocationPtr(EARPGPrimaryStat Stat)
{
    switch (Stat)
    {
        case EARPGPrimaryStat::Strength: return &StatProgression.AllocatedPoints.Strength;
        case EARPGPrimaryStat::Vitality: return &StatProgression.AllocatedPoints.Vitality;
        case EARPGPrimaryStat::Magic: return &StatProgression.AllocatedPoints.Magic;
        case EARPGPrimaryStat::Spirit: return &StatProgression.AllocatedPoints.Spirit;
        case EARPGPrimaryStat::Dexterity: return &StatProgression.AllocatedPoints.Dexterity;
        case EARPGPrimaryStat::Luck: return &StatProgression.AllocatedPoints.Luck;
        default: return nullptr;
    }
}

const int32* UARPGStatsComponent::ResolveAllocationPtr(EARPGPrimaryStat Stat) const
{
    switch (Stat)
    {
        case EARPGPrimaryStat::Strength: return &StatProgression.AllocatedPoints.Strength;
        case EARPGPrimaryStat::Vitality: return &StatProgression.AllocatedPoints.Vitality;
        case EARPGPrimaryStat::Magic: return &StatProgression.AllocatedPoints.Magic;
        case EARPGPrimaryStat::Spirit: return &StatProgression.AllocatedPoints.Spirit;
        case EARPGPrimaryStat::Dexterity: return &StatProgression.AllocatedPoints.Dexterity;
        case EARPGPrimaryStat::Luck: return &StatProgression.AllocatedPoints.Luck;
        default: return nullptr;
    }
}

int32 UARPGStatsComponent::GetAllocatedAttributePoints(EARPGPrimaryStat Stat) const
{
    const int32* Points = ResolveAllocationPtr(Stat);
    return Points ? *Points : 0;
}

bool UARPGStatsComponent::CanSpendAttributePoints(EARPGPrimaryStat Stat, int32 Points) const
{
    if (!bEnableJRPGStatSystem || Points <= 0 || StatProgression.UnspentAttributePoints < Points) return false;
    const int32* Allocation = ResolveAllocationPtr(Stat);
    if (!Allocation) return false;
    if (*Allocation + Points > FMath::Max(0, AttributePointSettings.MaxAllocatedPointsPerStat)) return false;
    const float AddedValue = static_cast<float>(Points) * FMath::Max(0.01f, AttributePointSettings.StatValuePerPoint);
    return GetPrimaryStatValue(Stat) + AddedValue <= FMath::Max(1.f, GrowthSettings.PrimaryStatCap) + KINDA_SMALL_NUMBER;
}

float UARPGStatsComponent::PreviewPrimaryStatAfterSpend(EARPGPrimaryStat Stat, int32 Points) const
{
    if (Points <= 0) return GetPrimaryStatValue(Stat);
    const float AddedValue = static_cast<float>(Points) * FMath::Max(0.01f, AttributePointSettings.StatValuePerPoint);
    return FMath::Clamp(GetPrimaryStatValue(Stat) + AddedValue, 0.f, FMath::Max(1.f, GrowthSettings.PrimaryStatCap));
}

bool UARPGStatsComponent::SpendAttributePoints(EARPGPrimaryStat Stat, int32 Points)
{
    if (!GetOwner() || !CanSpendAttributePoints(Stat, Points)) return false;
    if (!GetOwner()->HasAuthority())
    {
        // Local replicated state performs an immediate UI-friendly preflight; authority repeats the
        // validation so a modified client still cannot overspend or bypass caps.
        ServerSpendAttributePoints(Stat, Points);
        return true;
    }
    return SpendAttributePointsAuthority(Stat, Points);
}

void UARPGStatsComponent::ServerSpendAttributePoints_Implementation(EARPGPrimaryStat Stat, int32 Points)
{
    SpendAttributePointsAuthority(Stat, Points);
}

bool UARPGStatsComponent::SpendAttributePointsAuthority(EARPGPrimaryStat Stat, int32 Points)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || !bEnableJRPGStatSystem || Points <= 0) return false;
    if (!StatProgression.bInitialized)
    {
        const UARPGProgressionComponent* Progression = GetOwner()->FindComponentByClass<UARPGProgressionComponent>();
        InitializeStatProgressionForLevel(Progression ? Progression->Level : 1, false);
    }
    if (!CanSpendAttributePoints(Stat, Points)) return false;

    int32* Allocation = ResolveAllocationPtr(Stat);
    if (!Allocation) return false;
    *Allocation += Points;
    StatProgression.UnspentAttributePoints -= Points;
    RecalculateJRPGStatsInternal(false);
    OnAttributePointsChanged.Broadcast(StatProgression.UnspentAttributePoints, StatProgression.TotalAttributePointsEarned);
    return true;
}

bool UARPGStatsComponent::AddAttributePoints(int32 Points)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || !bEnableJRPGStatSystem || Points <= 0) return false;
    StatProgression.bInitialized = true;
    StatProgression.UnspentAttributePoints = FMath::Max(0, StatProgression.UnspentAttributePoints + Points);
    StatProgression.TotalAttributePointsEarned = FMath::Max(0, StatProgression.TotalAttributePointsEarned + Points);
    ++StatRevision;
    OnAttributePointsChanged.Broadcast(StatProgression.UnspentAttributePoints, StatProgression.TotalAttributePointsEarned);
    BroadcastStatSnapshot();
    return true;
}

bool UARPGStatsComponent::RefundAllAttributePoints()
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || !bEnableJRPGStatSystem) return false;
    const int32 Refund = StatProgression.AllocatedPoints.GetTotalPoints();
    if (Refund <= 0) return false;
    StatProgression.AllocatedPoints = FARPGPrimaryStatPointAllocation();
    StatProgression.UnspentAttributePoints += Refund;
    RecalculateJRPGStatsInternal(false);
    OnAttributePointsChanged.Broadcast(StatProgression.UnspentAttributePoints, StatProgression.TotalAttributePointsEarned);
    return true;
}

int32 UARPGStatsComponent::GetBaseProgressionLevel() const
{
    const UARPGProgressionComponent* Progression = GetOwner() ? GetOwner()->FindComponentByClass<UARPGProgressionComponent>() : nullptr;
    return Progression ? FMath::Max(1, Progression->Level) : 1;
}

int32 UARPGStatsComponent::GetEffectiveLevel() const
{
    if (NPCLevelScalingRuntime.bScalingApplied && NPCLevelScalingRuntime.EffectiveLevel > 0)
        return NPCLevelScalingRuntime.EffectiveLevel;
    return GetBaseProgressionLevel();
}

bool UARPGStatsComponent::CanUseNPCLevelScaling() const
{
    if (!bEnableJRPGStatSystem || !NPCLevelScalingSettings.bScaleToPlayer || !GetOwner()) return false;
    if (const APawn* Pawn = Cast<APawn>(GetOwner()))
        if (Pawn->IsPlayerControlled()) return false;
    return true;
}

bool UARPGStatsComponent::IsOwnerInCombat() const
{
    if (!GetOwner()) return false;
    if (const UARPGCombatComponent* Combat = GetOwner()->FindComponentByClass<UARPGCombatComponent>())
        return Combat->IsAlive() && Combat->CombatTarget != nullptr;
    return false;
}

bool UARPGStatsComponent::IsEligibleScalingPlayer(AActor* Candidate) const
{
    if (!Candidate || Candidate == GetOwner()) return false;
    const APawn* Pawn = Cast<APawn>(Candidate);
    if (!Pawn || !Pawn->IsPlayerControlled()) return false;
    const UARPGProgressionComponent* Progression = Candidate->FindComponentByClass<UARPGProgressionComponent>();
    if (!Progression || Progression->Level <= 0) return false;
    if (NPCLevelScalingSettings.bIgnoreDeadPlayers)
    {
        if (const UARPGCombatComponent* Combat = Candidate->FindComponentByClass<UARPGCombatComponent>())
            if (!Combat->IsAlive()) return false;
        if (const UARPGStatsComponent* Stats = Candidate->FindComponentByClass<UARPGStatsComponent>())
            if (Stats->Health <= 0.f) return false;
    }
    return true;
}

bool UARPGStatsComponent::ResolveNPCScalingReference(int32& OutPlayerLevel) const
{
    OutPlayerLevel = 0;
    if (!GetOwner() || !GetWorld()) return false;

    // The currently fought player is the most stable and intuitive reference for a shared NPC.
    if (NPCLevelScalingSettings.ReferenceMode == EARPGNPCPlayerScalingReference::CombatTargetThenNearest)
    {
        if (const UARPGCombatComponent* Combat = GetOwner()->FindComponentByClass<UARPGCombatComponent>())
        {
            AActor* Target = Combat->CombatTarget;
            if (IsEligibleScalingPlayer(Target))
            {
                if (const UARPGProgressionComponent* Progression = Target->FindComponentByClass<UARPGProgressionComponent>())
                {
                    OutPlayerLevel = FMath::Max(1, Progression->Level);
                    return true;
                }
            }
        }
    }

    const FVector OwnerLocation = GetOwner()->GetActorLocation();
    const float Radius = FMath::Max(0.f, NPCLevelScalingSettings.ReferenceSearchRadius);
    const float RadiusSq = Radius > 0.f ? FMath::Square(Radius) : TNumericLimits<float>::Max();

    int32 Count = 0;
    int64 LevelTotal = 0;
    int32 SelectedLevel = 0;
    float SelectedDistanceSq = TNumericLimits<float>::Max();

    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        const APlayerController* PC = It->Get();
        AActor* Candidate = PC ? PC->GetPawn() : nullptr;
        if (!IsEligibleScalingPlayer(Candidate)) continue;

        const float DistanceSq = FVector::DistSquared(OwnerLocation, Candidate->GetActorLocation());
        if (DistanceSq > RadiusSq) continue;
        const UARPGProgressionComponent* Progression = Candidate->FindComponentByClass<UARPGProgressionComponent>();
        if (!Progression) continue;
        const int32 CandidateLevel = FMath::Max(1, Progression->Level);

        ++Count;
        LevelTotal += CandidateLevel;

        switch (NPCLevelScalingSettings.ReferenceMode)
        {
            case EARPGNPCPlayerScalingReference::HighestLevelPlayer:
                if (SelectedLevel <= 0 || CandidateLevel > SelectedLevel || (CandidateLevel == SelectedLevel && DistanceSq < SelectedDistanceSq))
                {
                    SelectedLevel = CandidateLevel;
                    SelectedDistanceSq = DistanceSq;
                }
                break;
            case EARPGNPCPlayerScalingReference::LowestLevelPlayer:
                if (SelectedLevel <= 0 || CandidateLevel < SelectedLevel || (CandidateLevel == SelectedLevel && DistanceSq < SelectedDistanceSq))
                {
                    SelectedLevel = CandidateLevel;
                    SelectedDistanceSq = DistanceSq;
                }
                break;
            case EARPGNPCPlayerScalingReference::AverageNearbyPlayers:
                break;
            case EARPGNPCPlayerScalingReference::CombatTargetThenNearest:
            case EARPGNPCPlayerScalingReference::NearestPlayer:
            default:
                if (DistanceSq < SelectedDistanceSq)
                {
                    SelectedLevel = CandidateLevel;
                    SelectedDistanceSq = DistanceSq;
                }
                break;
        }
    }

    if (Count <= 0) return false;
    if (NPCLevelScalingSettings.ReferenceMode == EARPGNPCPlayerScalingReference::AverageNearbyPlayers)
        SelectedLevel = FMath::Max(1, FMath::RoundToInt(static_cast<float>(LevelTotal) / static_cast<float>(Count)));

    OutPlayerLevel = FMath::Max(1, SelectedLevel);
    return OutPlayerLevel > 0;
}

int32 UARPGStatsComponent::ComputeNPCScaledLevel(int32 BaseLevel, int32 PlayerLevel) const
{
    BaseLevel = FMath::Max(1, BaseLevel);
    PlayerLevel = FMath::Max(1, PlayerLevel);

    int32 TargetLevel = FMath::Max(1, PlayerLevel + NPCLevelScalingSettings.LevelOffset);
    if (!NPCLevelScalingSettings.bAllowScaleUp) TargetLevel = FMath::Min(TargetLevel, BaseLevel);
    if (!NPCLevelScalingSettings.bAllowScaleDown) TargetLevel = FMath::Max(TargetLevel, BaseLevel);

    const float MatchStrength = FMath::Clamp(NPCLevelScalingSettings.LevelMatchStrength, 0.f, 1.f);
    int32 EffectiveLevel = FMath::RoundToInt(FMath::Lerp(static_cast<float>(BaseLevel), static_cast<float>(TargetLevel), MatchStrength));

    const UARPGProgressionComponent* Progression = GetOwner() ? GetOwner()->FindComponentByClass<UARPGProgressionComponent>() : nullptr;
    const int32 ProgressionMaxLevel = Progression ? FMath::Max(1, Progression->MaxLevel) : FMath::Max(1, NPCLevelScalingSettings.MaximumScaledLevel);
    const int32 MaxLevel = FMath::Max(1, FMath::Min(FMath::Max(1, NPCLevelScalingSettings.MaximumScaledLevel), ProgressionMaxLevel));
    const int32 MinLevel = FMath::Clamp(FMath::Max(1, NPCLevelScalingSettings.MinimumScaledLevel), 1, MaxLevel);
    return FMath::Clamp(EffectiveLevel, MinLevel, MaxLevel);
}

void UARPGStatsComponent::RefreshNPCLevelScalingNow()
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || !bEnableJRPGStatSystem) return;
    RefreshNPCLevelScalingInternal(true, false);
    if (CanUseNPCLevelScaling()) EnsureNPCLevelScalingTimer();
    else ClearNPCLevelScalingTimer();
}

void UARPGStatsComponent::HandleNPCLevelScalingTimer()
{
    if (GetOwner() && GetOwner()->HasAuthority())
        RefreshNPCLevelScalingInternal(false, false);
}

void UARPGStatsComponent::EnsureNPCLevelScalingTimer()
{
    if (!GetWorld() || !CanUseNPCLevelScaling()) return;
    const float Interval = FMath::Clamp(NPCLevelScalingSettings.RefreshInterval, 0.10f, 10.f);
    if (GetWorld()->GetTimerManager().IsTimerActive(NPCLevelScalingTimer)) return;
    const float FirstDelay = FMath::FRandRange(0.05f, FMath::Max(0.05f, Interval));
    GetWorld()->GetTimerManager().SetTimer(NPCLevelScalingTimer, this, &UARPGStatsComponent::HandleNPCLevelScalingTimer, Interval, true, FirstDelay);
}

void UARPGStatsComponent::ClearNPCLevelScalingTimer()
{
    if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(NPCLevelScalingTimer);
}

void UARPGStatsComponent::RefreshNPCLevelScalingInternal(bool bForceRecalculate, bool bFromLevelUp)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || !bEnableJRPGStatSystem) return;

    const int32 BaseLevel = GetBaseProgressionLevel();
    const bool bCanScale = CanUseNPCLevelScaling();
    const bool bInCombat = IsOwnerInCombat();

    FARPGNPCLevelScalingRuntimeState NewState = NPCLevelScalingRuntime;
    NewState.BaseLevel = BaseLevel;

    if (!bCanScale)
    {
        bNPCScalingEncounterLocked = false;
        NewState.bHasPlayerReference = false;
        NewState.bEncounterLevelLocked = false;
        NewState.bScalingApplied = false;
        NewState.ReferencePlayerLevel = 0;
        NewState.EffectiveLevel = BaseLevel;
    }
    else if (NPCLevelScalingSettings.bLockLevelWhileInCombat && bInCombat && bNPCScalingEncounterLocked)
    {
        // Encounter snapshot: keep the level/stat budget stable until combat ends, even if another
        // player walks closer or the reference player levels during the fight.
        NewState.bEncounterLevelLocked = true;
        NewState.EffectiveLevel = FMath::Max(1, NPCLevelScalingRuntime.EffectiveLevel);
    }
    else
    {
        if (!bInCombat) bNPCScalingEncounterLocked = false;

        int32 ReferenceLevel = 0;
        const bool bHasReference = ResolveNPCScalingReference(ReferenceLevel);
        NewState.bHasPlayerReference = bHasReference;
        NewState.ReferencePlayerLevel = bHasReference ? ReferenceLevel : 0;

        if (bHasReference)
        {
            NewState.EffectiveLevel = ComputeNPCScaledLevel(BaseLevel, ReferenceLevel);
            NewState.bScalingApplied = true;
        }
        else if (NPCLevelScalingSettings.bReturnToBaseLevelWithoutPlayer)
        {
            NewState.EffectiveLevel = BaseLevel;
            NewState.bScalingApplied = false;
        }
        else
        {
            NewState.EffectiveLevel = FMath::Max(1, NPCLevelScalingRuntime.EffectiveLevel > 0 ? NPCLevelScalingRuntime.EffectiveLevel : BaseLevel);
            NewState.bScalingApplied = NPCLevelScalingRuntime.bScalingApplied;
        }

        if (NPCLevelScalingSettings.bLockLevelWhileInCombat && bInCombat)
            bNPCScalingEncounterLocked = true;
        NewState.bEncounterLevelLocked = bNPCScalingEncounterLocked;
    }

    const bool bRuntimeChanged =
        NewState.bScalingApplied != NPCLevelScalingRuntime.bScalingApplied ||
        NewState.bHasPlayerReference != NPCLevelScalingRuntime.bHasPlayerReference ||
        NewState.bEncounterLevelLocked != NPCLevelScalingRuntime.bEncounterLevelLocked ||
        NewState.BaseLevel != NPCLevelScalingRuntime.BaseLevel ||
        NewState.ReferencePlayerLevel != NPCLevelScalingRuntime.ReferencePlayerLevel ||
        NewState.EffectiveLevel != NPCLevelScalingRuntime.EffectiveLevel;
    const bool bEffectiveLevelChanged = NewState.EffectiveLevel != NPCLevelScalingRuntime.EffectiveLevel;

    NPCLevelScalingRuntime = NewState;
    if (bRuntimeChanged)
        OnNPCLevelScalingChanged.Broadcast(NPCLevelScalingRuntime);

    if (bEffectiveLevelChanged || bForceRecalculate)
        RecalculateJRPGStatsInternal(bFromLevelUp);
    else if (bRuntimeChanged)
        BroadcastStatSnapshot();
}

void UARPGStatsComponent::InitializeStatProgressionForLevel(int32 Level, bool bForceReset)
{
    Level = FMath::Max(1, Level);
    if (StatProgression.bInitialized && !bForceReset)
    {
        StatProgression.LastProcessedLevel = FMath::Max(1, StatProgression.LastProcessedLevel);
        return;
    }

    StatProgression = FARPGStatProgressionSaveState();
    StatProgression.bInitialized = true;
    StatProgression.LastProcessedLevel = Level;
    const int32 LevelPoints = AttributePointSettings.bGrantPointsOnLevelUp
        ? FMath::Max(0, Level - 1) * FMath::Max(0, AttributePointSettings.AttributePointsPerLevel)
        : 0;
    StatProgression.TotalAttributePointsEarned = FMath::Max(0, AttributePointSettings.StartingAttributePoints) + LevelPoints;
    StatProgression.UnspentAttributePoints = StatProgression.TotalAttributePointsEarned;
}

float UARPGStatsComponent::GetNaturalPrimaryStatForLevel(EARPGPrimaryStat Stat, int32 Level) const
{
    const int32 GrowthLevels = FMath::Max(0, Level - 1);
    float Base = 0.f;
    float Growth = 0.f;
    switch (Stat)
    {
        case EARPGPrimaryStat::Strength: Base = GrowthSettings.BasePrimaryStats.Strength; Growth = GrowthSettings.PrimaryGrowthPerLevel.Strength; break;
        case EARPGPrimaryStat::Vitality: Base = GrowthSettings.BasePrimaryStats.Vitality; Growth = GrowthSettings.PrimaryGrowthPerLevel.Vitality; break;
        case EARPGPrimaryStat::Magic: Base = GrowthSettings.BasePrimaryStats.Magic; Growth = GrowthSettings.PrimaryGrowthPerLevel.Magic; break;
        case EARPGPrimaryStat::Spirit: Base = GrowthSettings.BasePrimaryStats.Spirit; Growth = GrowthSettings.PrimaryGrowthPerLevel.Spirit; break;
        case EARPGPrimaryStat::Dexterity: Base = GrowthSettings.BasePrimaryStats.Dexterity; Growth = GrowthSettings.PrimaryGrowthPerLevel.Dexterity; break;
        case EARPGPrimaryStat::Luck: Base = GrowthSettings.BasePrimaryStats.Luck; Growth = GrowthSettings.PrimaryGrowthPerLevel.Luck; break;
        default: break;
    }
    float Value = Base + Growth * static_cast<float>(GrowthLevels);
    if (GrowthSettings.bRoundPrimaryStatsToWholeNumbers) Value = FMath::RoundToFloat(Value);
    return FMath::Clamp(Value, 0.f, FMath::Max(1.f, GrowthSettings.PrimaryStatCap));
}

FARPGStatModifier UARPGStatsComponent::CollectEquippedStatModifier() const
{
    FARPGStatModifier Result;
    if (!GetOwner()) return Result;
    const UARPGInventoryComponent* Inventory = GetOwner()->FindComponentByClass<UARPGInventoryComponent>();
    if (!Inventory) return Result;

    for (const FARPGInventoryEntry& Entry : Inventory->Items)
    {
        if (!Entry.bEquipped || Entry.Quantity <= 0) continue;
        if (const UARPGItemDefinition* Definition = Inventory->ResolveItemDefinition(Entry))
            AddModifier(Result, Definition->EquippedStatModifier);
    }
    return Result;
}

void UARPGStatsComponent::RecalculateJRPGStats()
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || !bEnableJRPGStatSystem) return;
    RecalculateJRPGStatsInternal(false);
}

void UARPGStatsComponent::RecalculateJRPGStatsInternal(bool bFromLevelUp)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || !bEnableJRPGStatSystem) return;

    const int32 BaseLevel = GetBaseProgressionLevel();
    const int32 Level = GetEffectiveLevel();
    // Attribute-point ownership follows the authored progression level, never the temporary scaled
    // encounter level. A level-1 chicken scaled to a level-50 player therefore does not mint 147 points.
    if (!StatProgression.bInitialized) InitializeStatProgressionForLevel(BaseLevel, false);

    const float OldMaxHealth = FMath::Max(1.f, MaxHealth);
    const float OldMaxMana = FMath::Max(1.f, MaxMana);
    const float OldMaxStamina = FMath::Max(1.f, MaxStamina);
    const float OldHealthPercent = FMath::Clamp(Health / OldMaxHealth, 0.f, 1.f);
    const float OldManaPercent = FMath::Clamp(Mana / OldMaxMana, 0.f, 1.f);
    const float OldStaminaPercent = FMath::Clamp(Stamina / OldMaxStamina, 0.f, 1.f);
    const float OldHealth = Health;

    const FARPGStatModifier Equipment = CollectEquippedStatModifier();
    const float ValuePerPoint = FMath::Max(0.01f, AttributePointSettings.StatValuePerPoint);
    const float Cap = FMath::Max(1.f, GrowthSettings.PrimaryStatCap);

    auto BuildPrimary = [&](EARPGPrimaryStat Stat, int32 Allocated, float EquipmentAdd)
    {
        return FMath::Clamp(GetNaturalPrimaryStatForLevel(Stat, Level) + static_cast<float>(FMath::Max(0, Allocated)) * ValuePerPoint + EquipmentAdd, 0.f, Cap);
    };

    PrimaryStats.Strength = BuildPrimary(EARPGPrimaryStat::Strength, StatProgression.AllocatedPoints.Strength, Equipment.PrimaryAdd.Strength);
    PrimaryStats.Vitality = BuildPrimary(EARPGPrimaryStat::Vitality, StatProgression.AllocatedPoints.Vitality, Equipment.PrimaryAdd.Vitality);
    PrimaryStats.Magic = BuildPrimary(EARPGPrimaryStat::Magic, StatProgression.AllocatedPoints.Magic, Equipment.PrimaryAdd.Magic);
    PrimaryStats.Spirit = BuildPrimary(EARPGPrimaryStat::Spirit, StatProgression.AllocatedPoints.Spirit, Equipment.PrimaryAdd.Spirit);
    PrimaryStats.Dexterity = BuildPrimary(EARPGPrimaryStat::Dexterity, StatProgression.AllocatedPoints.Dexterity, Equipment.PrimaryAdd.Dexterity);
    PrimaryStats.Luck = BuildPrimary(EARPGPrimaryStat::Luck, StatProgression.AllocatedPoints.Luck, Equipment.PrimaryAdd.Luck);

    DerivedStats.MeleeAttackPower = FMath::Max(0.f, PrimaryStats.Strength * DerivedFormula.MeleePowerPerStrength + Equipment.MeleeAttackPower);
    DerivedStats.RangedAttackPower = FMath::Max(0.f, PrimaryStats.Dexterity * DerivedFormula.RangedPowerPerDexterity + PrimaryStats.Strength * DerivedFormula.RangedPowerPerStrength + Equipment.RangedAttackPower);
    DerivedStats.MagicAttackPower = FMath::Max(0.f, PrimaryStats.Magic * DerivedFormula.MagicPowerPerMagic + Equipment.MagicAttackPower);
    DerivedStats.PhysicalDefense = FMath::Max(0.f, PrimaryStats.Vitality * DerivedFormula.PhysicalDefensePerVitality + Equipment.PhysicalDefense);
    DerivedStats.MagicDefense = FMath::Max(0.f, PrimaryStats.Spirit * DerivedFormula.MagicDefensePerSpirit + Equipment.MagicDefense);
    DerivedStats.Accuracy = FMath::Max(0.f, DerivedFormula.BaseAccuracy + PrimaryStats.Dexterity * DerivedFormula.AccuracyPerDexterity + Equipment.Accuracy);
    DerivedStats.Evasion = FMath::Max(0.f, PrimaryStats.Dexterity * DerivedFormula.EvasionPerDexterity + PrimaryStats.Luck * DerivedFormula.EvasionPerLuck + Equipment.Evasion);
    DerivedStats.MagicEvasion = FMath::Max(0.f, PrimaryStats.Spirit * DerivedFormula.MagicEvasionPerSpirit + PrimaryStats.Luck * DerivedFormula.MagicEvasionPerLuck + Equipment.MagicEvasion);
    DerivedStats.Speed = FMath::Max(0.f, PrimaryStats.Dexterity + Equipment.Speed);
    DerivedStats.CriticalChance = FMath::Clamp(PrimaryStats.Luck * DerivedFormula.CriticalChancePerLuck + Equipment.CriticalChance, 0.f, 1.f);
    DerivedStats.CriticalDamageMultiplier = FMath::Max(1.f, 1.f + PrimaryStats.Luck * DerivedFormula.CriticalDamageBonusPerLuck + Equipment.CriticalDamageMultiplierBonus);

    const float SpeedAboveBase = DerivedStats.Speed - GrowthSettings.BasePrimaryStats.Dexterity;
    const float AttackSpeedMin = FMath::Min(DerivedFormula.MinAttackSpeedMultiplier, DerivedFormula.MaxAttackSpeedMultiplier);
    const float AttackSpeedMax = FMath::Max(DerivedFormula.MinAttackSpeedMultiplier, DerivedFormula.MaxAttackSpeedMultiplier);
    DerivedStats.AttackSpeedMultiplier = FMath::Clamp(1.f + SpeedAboveBase * DerivedFormula.AttackSpeedPerSpeedPointAboveBase + Equipment.AttackSpeedMultiplierBonus, AttackSpeedMin, AttackSpeedMax);
    const float MoveSpeedMin = FMath::Min(DerivedFormula.MinMovementSpeedMultiplier, DerivedFormula.MaxMovementSpeedMultiplier);
    const float MoveSpeedMax = FMath::Max(DerivedFormula.MinMovementSpeedMultiplier, DerivedFormula.MaxMovementSpeedMultiplier);
    DerivedStats.MovementSpeedMultiplier = FMath::Clamp(1.f + SpeedAboveBase * DerivedFormula.MovementSpeedPerSpeedPointAboveBase + Equipment.MovementSpeedMultiplierBonus, MoveSpeedMin, MoveSpeedMax);

    const float LevelDelta = static_cast<float>(FMath::Max(0, Level - 1));
    const float VitalityDelta = PrimaryStats.Vitality - GrowthSettings.BasePrimaryStats.Vitality;
    const float MagicDelta = PrimaryStats.Magic - GrowthSettings.BasePrimaryStats.Magic;
    const float SpiritDelta = PrimaryStats.Spirit - GrowthSettings.BasePrimaryStats.Spirit;
    const float DexterityDelta = PrimaryStats.Dexterity - GrowthSettings.BasePrimaryStats.Dexterity;
    MaxHealth = FMath::Max(1.f, GrowthSettings.BaseMaxHealth + LevelDelta * GrowthSettings.MaxHealthPerLevel + VitalityDelta * GrowthSettings.MaxHealthPerVitality + Equipment.MaxHealth);
    MaxMana = FMath::Max(1.f, GrowthSettings.BaseMaxMana + LevelDelta * GrowthSettings.MaxManaPerLevel + MagicDelta * GrowthSettings.MaxManaPerMagic + SpiritDelta * GrowthSettings.MaxManaPerSpirit + Equipment.MaxMana);
    MaxStamina = FMath::Max(1.f, GrowthSettings.BaseMaxStamina + LevelDelta * GrowthSettings.MaxStaminaPerLevel + VitalityDelta * GrowthSettings.MaxStaminaPerVitality + DexterityDelta * GrowthSettings.MaxStaminaPerDexterity + Equipment.MaxStamina);

    if (bFromLevelUp && AttributePointSettings.bRestoreVitalsOnLevelUp)
    {
        Health = MaxHealth;
        Mana = MaxMana;
        Stamina = MaxStamina;
    }
    else if (AttributePointSettings.bPreserveVitalPercentWhenMaxChanges)
    {
        Health = FMath::Clamp(OldHealthPercent * MaxHealth, 0.f, MaxHealth);
        Mana = FMath::Clamp(OldManaPercent * MaxMana, 0.f, MaxMana);
        Stamina = FMath::Clamp(OldStaminaPercent * MaxStamina, 0.f, MaxStamina);
    }
    else
    {
        Health = FMath::Clamp(Health, 0.f, MaxHealth);
        Mana = FMath::Clamp(Mana, 0.f, MaxMana);
        Stamina = FMath::Clamp(Stamina, 0.f, MaxStamina);
    }

    // Legacy aliases remain synchronized so old Blueprint/UI/combat references continue to report a
    // useful value after the new system is enabled.
    AttackPower = DerivedStats.MeleeAttackPower;
    SpellPower = DerivedStats.MagicAttackPower;
    Armor = DerivedStats.PhysicalDefense;

    ++StatRevision;
    ApplyMovementSpeed();
    if (!FMath::IsNearlyEqual(OldHealth, Health)) OnHealthChanged.Broadcast(Health, Health - OldHealth);
    BroadcastStatSnapshot();
}

void UARPGStatsComponent::ApplyMovementSpeed()
{
    if (!bEnableJRPGStatSystem || CachedBaseWalkSpeed <= 0.f || !GetOwner()) return;

    float StateMovementMultiplier = 1.f;
    // A stat/equipment change is allowed while blocking. Re-applying raw movement speed here would
    // otherwise cancel the block movement penalty until the player blocks again. Preserve the active
    // combat-state multiplier and let Combat restore the new full stat speed when blocking ends.
    if (const UARPGCombatComponent* Combat = GetOwner()->FindComponentByClass<UARPGCombatComponent>())
    {
        if (Combat->bIsBlocking)
            StateMovementMultiplier = FMath::Clamp(Combat->GetCombatProfile().Block.BlockingMoveSpeedMultiplier, 0.f, 1.f);
    }

    if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
        if (UCharacterMovementComponent* Move = Character->GetCharacterMovement())
            Move->MaxWalkSpeed = FMath::Max(1.f, CachedBaseWalkSpeed * DerivedStats.MovementSpeedMultiplier * StateMovementMultiplier);
}

void UARPGStatsComponent::RefreshMovementSpeedFromStats()
{
    ApplyMovementSpeed();
}

void UARPGStatsComponent::HandleLevelChanged(int32 OldLevel, int32 NewLevel)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || !bEnableJRPGStatSystem || NewLevel <= 0) return;
    if (!StatProgression.bInitialized) InitializeStatProgressionForLevel(FMath::Max(1, OldLevel), false);

    // LastProcessedLevel is a reward high-water mark, not merely the current level. That prevents an
    // admin command, prestige mechanic or temporary level drain from farming Attribute Points by
    // repeatedly lowering and re-raising the same levels. Natural stats still follow Current Level.
    const int32 RewardedThroughLevel = FMath::Max(1, StatProgression.LastProcessedLevel);
    if (NewLevel > RewardedThroughLevel && AttributePointSettings.bGrantPointsOnLevelUp)
    {
        const int32 Gained = (NewLevel - RewardedThroughLevel) * FMath::Max(0, AttributePointSettings.AttributePointsPerLevel);
        StatProgression.UnspentAttributePoints += Gained;
        StatProgression.TotalAttributePointsEarned += Gained;
    }
    StatProgression.LastProcessedLevel = FMath::Max(RewardedThroughLevel, NewLevel);
    if (CanUseNPCLevelScaling())
        RefreshNPCLevelScalingInternal(true, NewLevel > OldLevel);
    else
        RecalculateJRPGStatsInternal(NewLevel > OldLevel);
    OnAttributePointsChanged.Broadcast(StatProgression.UnspentAttributePoints, StatProgression.TotalAttributePointsEarned);
}

void UARPGStatsComponent::HandleInventoryChanged()
{
    if (GetOwner() && GetOwner()->HasAuthority() && bEnableJRPGStatSystem)
        RecalculateJRPGStatsInternal(false);
}

float UARPGStatsComponent::GetPrimaryStatValue(EARPGPrimaryStat Stat) const
{
    switch (Stat)
    {
        case EARPGPrimaryStat::Strength: return PrimaryStats.Strength;
        case EARPGPrimaryStat::Vitality: return PrimaryStats.Vitality;
        case EARPGPrimaryStat::Magic: return PrimaryStats.Magic;
        case EARPGPrimaryStat::Spirit: return PrimaryStats.Spirit;
        case EARPGPrimaryStat::Dexterity: return PrimaryStats.Dexterity;
        case EARPGPrimaryStat::Luck: return PrimaryStats.Luck;
        default: return 0.f;
    }
}

float UARPGStatsComponent::GetDerivedStatValue(EARPGDerivedStat Stat) const
{
    switch (Stat)
    {
        case EARPGDerivedStat::MeleeAttackPower: return GetMeleeAttackPower();
        case EARPGDerivedStat::RangedAttackPower: return GetRangedAttackPower();
        case EARPGDerivedStat::MagicAttackPower: return GetMagicAttackPower();
        case EARPGDerivedStat::PhysicalDefense: return GetPhysicalDefense();
        case EARPGDerivedStat::MagicDefense: return GetMagicDefense();
        case EARPGDerivedStat::Accuracy: return bEnableJRPGStatSystem ? DerivedStats.Accuracy : 100.f;
        case EARPGDerivedStat::Evasion: return bEnableJRPGStatSystem ? DerivedStats.Evasion : 0.f;
        case EARPGDerivedStat::MagicEvasion: return bEnableJRPGStatSystem ? DerivedStats.MagicEvasion : 0.f;
        case EARPGDerivedStat::Speed: return bEnableJRPGStatSystem ? DerivedStats.Speed : 0.f;
        case EARPGDerivedStat::CriticalChance: return GetCriticalChanceBonus();
        case EARPGDerivedStat::CriticalDamageMultiplier: return GetCriticalDamageMultiplier();
        case EARPGDerivedStat::AttackSpeedMultiplier: return GetAttackSpeedMultiplier();
        case EARPGDerivedStat::MovementSpeedMultiplier: return GetMovementSpeedMultiplier();
        case EARPGDerivedStat::MaxHealth: return MaxHealth;
        case EARPGDerivedStat::MaxMana: return MaxMana;
        case EARPGDerivedStat::MaxStamina: return MaxStamina;
        default: return 0.f;
    }
}

FARPGStatSnapshot UARPGStatsComponent::GetStatSnapshot() const
{
    FARPGStatSnapshot Snapshot;
    Snapshot.bJRPGStatSystemEnabled = bEnableJRPGStatSystem;
    Snapshot.Level = GetEffectiveLevel();
    Snapshot.PrimaryStats = PrimaryStats;
    Snapshot.AllocatedPoints = StatProgression.AllocatedPoints;
    Snapshot.DerivedStats = DerivedStats;
    Snapshot.UnspentAttributePoints = StatProgression.UnspentAttributePoints;
    Snapshot.TotalAttributePointsEarned = StatProgression.TotalAttributePointsEarned;
    Snapshot.MaxHealth = MaxHealth;
    Snapshot.MaxMana = MaxMana;
    Snapshot.MaxStamina = MaxStamina;
    return Snapshot;
}

void UARPGStatsComponent::RestoreStatProgressionState(const FARPGStatProgressionSaveState& State, int32 CharacterLevel, bool bLegacySave)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || !bEnableJRPGStatSystem) return;
    CharacterLevel = FMath::Max(1, CharacterLevel);
    if (bLegacySave || !State.bInitialized)
    {
        InitializeStatProgressionForLevel(CharacterLevel, true);
    }
    else
    {
        StatProgression = State;
        StatProgression.bInitialized = true;
        StatProgression.LastProcessedLevel = FMath::Max(CharacterLevel, FMath::Max(1, State.LastProcessedLevel));
        const int32 MaxPerStat = FMath::Max(0, AttributePointSettings.MaxAllocatedPointsPerStat);
        StatProgression.AllocatedPoints.Strength = FMath::Clamp(StatProgression.AllocatedPoints.Strength, 0, MaxPerStat);
        StatProgression.AllocatedPoints.Vitality = FMath::Clamp(StatProgression.AllocatedPoints.Vitality, 0, MaxPerStat);
        StatProgression.AllocatedPoints.Magic = FMath::Clamp(StatProgression.AllocatedPoints.Magic, 0, MaxPerStat);
        StatProgression.AllocatedPoints.Spirit = FMath::Clamp(StatProgression.AllocatedPoints.Spirit, 0, MaxPerStat);
        StatProgression.AllocatedPoints.Dexterity = FMath::Clamp(StatProgression.AllocatedPoints.Dexterity, 0, MaxPerStat);
        StatProgression.AllocatedPoints.Luck = FMath::Clamp(StatProgression.AllocatedPoints.Luck, 0, MaxPerStat);
        StatProgression.UnspentAttributePoints = FMath::Max(0, StatProgression.UnspentAttributePoints);
        StatProgression.TotalAttributePointsEarned = FMath::Max(StatProgression.TotalAttributePointsEarned, StatProgression.AllocatedPoints.GetTotalPoints() + StatProgression.UnspentAttributePoints);
    }
    RecalculateJRPGStatsInternal(false);
    OnAttributePointsChanged.Broadcast(StatProgression.UnspentAttributePoints, StatProgression.TotalAttributePointsEarned);
}

void UARPGStatsComponent::BroadcastStatSnapshot()
{
    OnJRPGStatsChanged.Broadcast(GetStatSnapshot());
}

void UARPGStatsComponent::OnRep_Health(float OldHealth)
{
    OnHealthChanged.Broadcast(Health, Health - OldHealth);
    if (OldHealth > 0.f && Health <= 0.f) OnDeath.Broadcast();
}

void UARPGStatsComponent::OnRep_StatRevision()
{
    ApplyMovementSpeed();
    BroadcastStatSnapshot();
    OnAttributePointsChanged.Broadcast(StatProgression.UnspentAttributePoints, StatProgression.TotalAttributePointsEarned);
}

void UARPGStatsComponent::OnRep_NPCLevelScalingRuntime()
{
    OnNPCLevelScalingChanged.Broadcast(NPCLevelScalingRuntime);
    BroadcastStatSnapshot();
}

void UARPGStatsComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(UARPGStatsComponent, Health);
    DOREPLIFETIME(UARPGStatsComponent, MaxHealth);
    DOREPLIFETIME(UARPGStatsComponent, Mana);
    DOREPLIFETIME(UARPGStatsComponent, MaxMana);
    DOREPLIFETIME(UARPGStatsComponent, Stamina);
    DOREPLIFETIME(UARPGStatsComponent, MaxStamina);
    DOREPLIFETIME(UARPGStatsComponent, Armor);
    DOREPLIFETIME(UARPGStatsComponent, AttackPower);
    DOREPLIFETIME(UARPGStatsComponent, SpellPower);
    DOREPLIFETIME(UARPGStatsComponent, bEnableJRPGStatSystem);
    DOREPLIFETIME(UARPGStatsComponent, PrimaryStats);
    DOREPLIFETIME(UARPGStatsComponent, DerivedStats);
    DOREPLIFETIME(UARPGStatsComponent, StatProgression);
    DOREPLIFETIME(UARPGStatsComponent, StatRevision);
    DOREPLIFETIME(UARPGStatsComponent, NPCLevelScalingRuntime);
}
