#include "Gathering/ARPGMineableRock.h"
#include "Building/ARPGBuildPieceActor.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/ARPGMiningComponent.h"
#include "Components/ARPGInventoryComponent.h"
#include "Components/ARPGEventRouterComponent.h"
#include "Data/ARPGItemDefinition.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "NiagaraComponentPoolMethodEnum.h"
#include "Kismet/GameplayStatics.h"
#include "Particles/ParticleSystem.h"
#include "Particles/WorldPSCPool.h"
#include "Sound/SoundBase.h"
#include "Net/UnrealNetwork.h"
#include "EngineUtils.h"
#include "TimerManager.h"

AARPGMineableRock::AARPGMineableRock()
{
    bReplicates = true;
    SetReplicateMovement(false);
    PrimaryActorTick.bCanEverTick = false;

    Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(Root);

    VisualRoot = CreateDefaultSubobject<USceneComponent>(TEXT("VisualRoot"));
    VisualRoot->SetupAttachment(Root);

    RockMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RockMesh"));
    RockMesh->SetupAttachment(VisualRoot);
    RockMesh->SetMobility(EComponentMobility::Movable);
    RockMesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));

    DepletedMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DepletedMesh"));
    DepletedMesh->SetupAttachment(VisualRoot);
    DepletedMesh->SetMobility(EComponentMobility::Movable);
    DepletedMesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));
    DepletedMesh->SetHiddenInGame(true);
    DepletedMesh->SetVisibility(false);

    RequiredToolTag = FGameplayTag::RequestGameplayTag(TEXT("Item.Tool.Pickaxe"), false);
}

void AARPGMineableRock::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    bBaseVisualTransformCached = false;
    if (DepletedMesh) DepletedMesh->SetStaticMesh(DepletedMeshAsset);
    if (SelectedRockMeshIndex == INDEX_NONE && RockMeshes.Num() > 0) SelectedRockMeshIndex = 0;
    ApplySelectedRockMesh();
    ApplySelectedRockVisualTransform();
    ApplyRockStateVisuals();
}

void AARPGMineableRock::BeginPlay()
{
    Super::BeginPlay();
    CacheBaseVisualTransform();
    if (HasAuthority())
    {
        CurrentMiningHealth = FMath::Max(1.f, MaxMiningHealth);
        if (RockMeshes.Num() > 0)
        {
            if (bRandomizeRockMesh) SelectRandomRockMesh();
            else SetRockMeshIndex(FMath::Clamp(SelectedRockMeshIndex == INDEX_NONE ? 0 : SelectedRockMeshIndex, 0, RockMeshes.Num() - 1));
        }

        float MinScale = 1.f;
        float MaxScale = 1.f;
        GetSanitizedMeshScaleRange(MinScale, MaxScale);
        if (bRandomizeRockMeshScale) SelectRandomRockMeshScale();
        else SetRockMeshScale(FMath::Clamp(SelectedRockMeshScale, MinScale, MaxScale));

        if (bRandomizeRockYaw) SelectRandomRockYaw();
        else SetRockYaw(SelectedRockYawDegrees);
        RockState = EARPGMineableRockState::Available;
    }

    ApplySelectedRockMesh();
    ApplySelectedRockVisualTransform();
    ApplyRockStateVisuals();
    CacheAvailableRespawnBounds();
    if (HasAuthority()) RefreshBuildingRespawnSuppression();
}

float AARPGMineableRock::GetMiningHealthPercent() const
{
    return FMath::Clamp(CurrentMiningHealth / FMath::Max(1.f, MaxMiningHealth), 0.f, 1.f);
}

UStaticMesh* AARPGMineableRock::GetSelectedRockMesh() const
{
    if (RockMeshes.IsValidIndex(SelectedRockMeshIndex)) return RockMeshes[SelectedRockMeshIndex].Get();
    return RockMesh ? RockMesh->GetStaticMesh() : nullptr;
}

FVector AARPGMineableRock::GetMiningImpactLocation(AActor* Harvester) const
{
    if (RockMesh && RockMesh->Bounds.SphereRadius > KINDA_SMALL_NUMBER)
    {
        const FBox Bounds = RockMesh->Bounds.GetBox();
        if (Harvester)
        {
            FVector Point = Bounds.GetClosestPointTo(Harvester->GetActorLocation());
            Point.Z = FMath::Clamp(Point.Z, Bounds.Min.Z, Bounds.Max.Z);
            return Point;
        }
        return FVector(Bounds.GetCenter().X, Bounds.GetCenter().Y,
            FMath::Clamp(GetActorLocation().Z + MiningImpactHeight, Bounds.Min.Z, Bounds.Max.Z));
    }
    return GetActorLocation() + FVector(0.f, 0.f, MiningImpactHeight);
}

bool AARPGMineableRock::CanBeMinedBy(AActor* Harvester, FText& OutFailureReason) const
{
    OutFailureReason = FText::GetEmpty();
    if (!IsValid(Harvester))
    {
        OutFailureReason = FText::FromString(TEXT("No miner supplied."));
        return false;
    }
    if (!IsAvailable())
    {
        OutFailureReason = FText::FromString(TEXT("This resource node is depleted."));
        return false;
    }
    if (bBuildingRespawnSuppressed)
    {
        OutFailureReason = FText::FromString(TEXT("This resource location is occupied by a building."));
        return false;
    }

    const UARPGMiningComponent* Mining = Harvester->FindComponentByClass<UARPGMiningComponent>();
    if (!Mining)
    {
        OutFailureReason = FText::FromString(TEXT("This actor has no Mining component."));
        return false;
    }
    if (Mining->GetMiningLevel() < RequiredMiningLevel)
    {
        OutFailureReason = FText::Format(FText::FromString(TEXT("Requires Mining level {0}.")), FText::AsNumber(RequiredMiningLevel));
        return false;
    }
    if (!Mining->HasValidToolForRock(this))
    {
        OutFailureReason = MinimumToolTier > 0
            ? FText::Format(FText::FromString(TEXT("Requires a valid Mining tool of tier {0} or better.")), FText::AsNumber(MinimumToolTier))
            : FText::FromString(TEXT("Requires a valid Mining tool."));
        return false;
    }
    return true;
}

bool AARPGMineableRock::ApplyMiningStrike(AActor* Harvester, float MiningPower)
{
    if (!HasAuthority() || !IsValid(Harvester) || MiningPower <= 0.f) return false;
    FText FailureReason;
    if (!CanBeMinedBy(Harvester, FailureReason)) return false;

    const float DamageApplied = FMath::Min(CurrentMiningHealth, FMath::Max(0.01f, MiningPower / FMath::Max(0.05f, MiningResistance)));
    CurrentMiningHealth = FMath::Max(0.f, CurrentMiningHealth - DamageApplied);
    AwardMiningXP(Harvester, XPPerSuccessfulStrike);

    const FVector ImpactLocation = GetMiningImpactLocation(Harvester);
    UARPGItemDefinition* EquippedTool = nullptr;
    if (UARPGMiningComponent* Mining = Harvester->FindComponentByClass<UARPGMiningComponent>())
        EquippedTool = Mining->GetBestEquippedMiningTool();

    MulticastPlayStrikeFeedback(Harvester, ImpactLocation, EquippedTool);
    GrantNormalDropArray(Harvester, StrikeDrops, EARPGMiningBonusDropTrigger::SuccessfulStrike);
    GrantBonusDrops(Harvester, EARPGMiningBonusDropTrigger::SuccessfulStrike);
    OnRockStruck.Broadcast(this, Harvester, DamageApplied, CurrentMiningHealth);

    if (CurrentMiningHealth <= KINDA_SMALL_NUMBER) DepleteRock(Harvester);
    ForceNetUpdate();
    return true;
}

bool AARPGMineableRock::DepleteRock(AActor* Harvester)
{
    if (!HasAuthority() || !IsAvailable()) return false;

    CurrentMiningHealth = 0.f;
    const EARPGMineableRockState OldState = RockState;
    RockState = EARPGMineableRockState::Depleted;
    ApplyRockStateVisuals();

    AwardMiningXP(Harvester, XPOnDepletion);
    GrantNormalDropArray(Harvester, DepletionDrops, EARPGMiningBonusDropTrigger::Depletion);
    GrantBonusDrops(Harvester, EARPGMiningBonusDropTrigger::Depletion);
    MulticastPlayDepletedFeedback(GetActorLocation());
    OnRockDepleted.Broadcast(this, Harvester);
    if (OldState != RockState) OnRockStateChanged.Broadcast(RockState);

    if (bRespawn)
    {
        const float Delay = FMath::Max(0.05f, RespawnSeconds);
        RespawnEligibleServerTime = GetWorld() ? GetWorld()->GetTimeSeconds() + Delay : Delay;
        GetWorldTimerManager().SetTimer(RespawnTimer, this, &AARPGMineableRock::TryRespawnAuthority, Delay, false);
    }
    ForceNetUpdate();
    return true;
}

void AARPGMineableRock::ForceRespawn()
{
    if (!HasAuthority()) return;
    bForcedRespawnPending = true;
    RespawnEligibleServerTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
    TryRespawnAuthority();
}

void AARPGMineableRock::TryRespawnAuthority()
{
    if (!HasAuthority()) return;
    GetWorldTimerManager().ClearTimer(RespawnTimer);
    RefreshBuildingRespawnSuppression();
    if (bBuildingRespawnSuppressed)
    {
        ScheduleSuppressionRecheck();
        return;
    }
    if (!bRespawn && !bForcedRespawnPending) return;

    const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : RespawnEligibleServerTime;
    const double Remaining = RespawnEligibleServerTime - Now;
    if (Remaining > KINDA_SMALL_NUMBER)
    {
        GetWorldTimerManager().SetTimer(RespawnTimer, this, &AARPGMineableRock::TryRespawnAuthority, static_cast<float>(Remaining), false);
        return;
    }
    CompleteRespawnAuthority();
}

void AARPGMineableRock::CompleteRespawnAuthority()
{
    if (!HasAuthority() || bBuildingRespawnSuppressed) return;
    GetWorldTimerManager().ClearTimer(RespawnTimer);
    GetWorldTimerManager().ClearTimer(BuildingSuppressionRecheckTimer);
    BuildingRespawnBlockers.Reset();
    bForcedRespawnPending = false;
    RespawnEligibleServerTime = 0.0;

    if (bRerollRockMeshOnRespawn && RockMeshes.Num() > 0) SelectRandomRockMesh();
    if (bRerollRockMeshScaleOnRespawn) SelectRandomRockMeshScale();
    if (bRerollRockYawOnRespawn) SelectRandomRockYaw();

    CurrentMiningHealth = FMath::Max(1.f, MaxMiningHealth);
    const EARPGMineableRockState OldState = RockState;
    RockState = EARPGMineableRockState::Available;
    ApplyRockStateVisuals();
    CacheAvailableRespawnBounds();
    if (OldState != RockState) OnRockStateChanged.Broadcast(RockState);
    OnRockRespawned.Broadcast(this);
    ForceNetUpdate();
}

void AARPGMineableRock::CacheAvailableRespawnBounds()
{
    if (RockState == EARPGMineableRockState::Available && RockMesh && RockMesh->Bounds.SphereRadius > KINDA_SMALL_NUMBER)
        CachedAvailableRespawnBounds = RockMesh->Bounds.GetBox();
}

bool AARPGMineableRock::IsRespawnBlockedByBuildPiece(const AARPGBuildPieceActor* Building) const
{
    if (!bSuppressRespawnWhileBuiltOver || !IsValid(Building) || !Building->Definition || Building->IsActorBeingDestroyed()) return false;

    FBox Bounds = CachedAvailableRespawnBounds;
    if (!Bounds.IsValid && RockMesh && RockMesh->Bounds.SphereRadius > KINDA_SMALL_NUMBER) Bounds = RockMesh->Bounds.GetBox();

    float MinZ = GetActorLocation().Z - 25.f;
    float MaxZ = GetActorLocation().Z + FMath::Max(150.f, MiningImpactHeight * 2.f);
    if (Bounds.IsValid)
    {
        MinZ = FMath::Min(MinZ, Bounds.Min.Z);
        MaxZ = FMath::Max(MaxZ, Bounds.Max.Z);
    }
    return Building->DoesLogicalPlacementOverlapWorldCylinder(GetActorLocation(), FMath::Max(0.f, BuildingRespawnBlockRadius), MinZ, MaxZ);
}

void AARPGMineableRock::NotifyBuildPieceOccupancyChanged(AARPGBuildPieceActor* Building, bool bPresent)
{
    if (!HasAuthority() || !Building) return;
    if (bPresent && IsRespawnBlockedByBuildPiece(Building)) BuildingRespawnBlockers.Add(Building);
    else BuildingRespawnBlockers.Remove(Building);
    UpdateBuildingSuppressionStateAuthority();
}

bool AARPGMineableRock::RefreshBuildingRespawnSuppression()
{
    if (!HasAuthority() || !GetWorld()) return bBuildingRespawnSuppressed;
    BuildingRespawnBlockers.Reset();
    if (bSuppressRespawnWhileBuiltOver)
    {
        for (TActorIterator<AARPGBuildPieceActor> It(GetWorld()); It; ++It)
            if (AARPGBuildPieceActor* Building = *It)
                if (IsRespawnBlockedByBuildPiece(Building)) BuildingRespawnBlockers.Add(Building);
    }
    UpdateBuildingSuppressionStateAuthority();
    return bBuildingRespawnSuppressed;
}

void AARPGMineableRock::ScheduleSuppressionRecheck()
{
    if (!HasAuthority() || !bBuildingRespawnSuppressed) return;
    const float Interval = FMath::Max(0.1f, BuildingRespawnRecheckSeconds);
    if (!GetWorldTimerManager().IsTimerActive(BuildingSuppressionRecheckTimer))
        GetWorldTimerManager().SetTimer(BuildingSuppressionRecheckTimer, this, &AARPGMineableRock::RecheckBuildingRespawnSuppressionAuthority, Interval, true);
}

void AARPGMineableRock::RecheckBuildingRespawnSuppressionAuthority()
{
    RefreshBuildingRespawnSuppression();
}

void AARPGMineableRock::UpdateBuildingSuppressionStateAuthority()
{
    if (!HasAuthority()) return;
    for (auto It = BuildingRespawnBlockers.CreateIterator(); It; ++It)
        if (!It->IsValid() || !IsRespawnBlockedByBuildPiece(It->Get())) It.RemoveCurrent();

    const bool bShouldSuppress = bSuppressRespawnWhileBuiltOver && BuildingRespawnBlockers.Num() > 0;
    if (bShouldSuppress == bBuildingRespawnSuppressed)
    {
        if (bShouldSuppress) ScheduleSuppressionRecheck();
        return;
    }

    bBuildingRespawnSuppressed = bShouldSuppress;
    if (bBuildingRespawnSuppressed)
    {
        // Building replacement is environmental suppression, not harvesting. Never award XP or loot.
        if (RockState == EARPGMineableRockState::Available)
        {
            CacheAvailableRespawnBounds();
            CurrentMiningHealth = 0.f;
            const EARPGMineableRockState OldState = RockState;
            RockState = EARPGMineableRockState::Depleted;
            if (OldState != RockState) OnRockStateChanged.Broadcast(RockState);
            if (bRespawn)
            {
                const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
                RespawnEligibleServerTime = FMath::Max(RespawnEligibleServerTime, Now + FMath::Max(0.f, RespawnSeconds));
            }
        }
        GetWorldTimerManager().ClearTimer(RespawnTimer);
        ApplyRockStateVisuals();
        ScheduleSuppressionRecheck();
    }
    else
    {
        GetWorldTimerManager().ClearTimer(BuildingSuppressionRecheckTimer);
        ApplyRockStateVisuals();
        if (bRespawn || bForcedRespawnPending)
        {
            const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : RespawnEligibleServerTime;
            const float Delay = static_cast<float>(FMath::Max(0.05, RespawnEligibleServerTime - Now));
            GetWorldTimerManager().SetTimer(RespawnTimer, this, &AARPGMineableRock::TryRespawnAuthority, Delay, false);
        }
    }

    OnRockBuildingSuppressionChanged.Broadcast(bBuildingRespawnSuppressed);
    ForceNetUpdate();
}

void AARPGMineableRock::SelectRandomRockMesh()
{
    if (!HasAuthority() || RockMeshes.Num() <= 0) return;
    SetRockMeshIndex(FMath::RandRange(0, RockMeshes.Num() - 1));
}

bool AARPGMineableRock::SetRockMeshIndex(int32 NewIndex)
{
    if (!HasAuthority() || !RockMeshes.IsValidIndex(NewIndex)) return false;
    SelectedRockMeshIndex = NewIndex;
    ApplySelectedRockMesh();
    CacheAvailableRespawnBounds();
    return true;
}

void AARPGMineableRock::GetSanitizedMeshScaleRange(float& OutMinScale, float& OutMaxScale) const
{
    const float A = FMath::Max(0.05f, MinimumMeshScale);
    const float B = FMath::Max(0.05f, MaximumMeshScale);
    OutMinScale = FMath::Min(A, B);
    OutMaxScale = FMath::Max(A, B);
}

void AARPGMineableRock::SelectRandomRockMeshScale()
{
    if (!HasAuthority()) return;
    float MinScale = 1.f;
    float MaxScale = 1.f;
    GetSanitizedMeshScaleRange(MinScale, MaxScale);
    SetRockMeshScale(FMath::FRandRange(MinScale, MaxScale));
}

bool AARPGMineableRock::SetRockMeshScale(float NewScale)
{
    if (!HasAuthority()) return false;
    float MinScale = 1.f;
    float MaxScale = 1.f;
    GetSanitizedMeshScaleRange(MinScale, MaxScale);
    SelectedRockMeshScale = FMath::Clamp(NewScale, MinScale, MaxScale);
    ApplySelectedRockVisualTransform();
    CacheAvailableRespawnBounds();
    return true;
}

void AARPGMineableRock::GetSanitizedYawRange(float& OutMinYaw, float& OutMaxYaw) const
{
    OutMinYaw = FMath::Min(MinimumRandomYawDegrees, MaximumRandomYawDegrees);
    OutMaxYaw = FMath::Max(MinimumRandomYawDegrees, MaximumRandomYawDegrees);
}

void AARPGMineableRock::SelectRandomRockYaw()
{
    if (!HasAuthority()) return;
    float MinYaw = 0.f;
    float MaxYaw = 360.f;
    GetSanitizedYawRange(MinYaw, MaxYaw);
    SetRockYaw(FMath::FRandRange(MinYaw, MaxYaw));
}

bool AARPGMineableRock::SetRockYaw(float NewYawDegrees)
{
    if (!HasAuthority()) return false;
    SelectedRockYawDegrees = FMath::UnwindDegrees(NewYawDegrees);
    ApplySelectedRockVisualTransform();
    CacheAvailableRespawnBounds();
    return true;
}

void AARPGMineableRock::CacheBaseVisualTransform()
{
    if (bBaseVisualTransformCached) return;
    BaseVisualRootScale = VisualRoot ? VisualRoot->GetRelativeScale3D() : FVector::OneVector;
    BaseVisualRootRotation = VisualRoot ? VisualRoot->GetRelativeRotation() : FRotator::ZeroRotator;
    bBaseVisualTransformCached = true;
}

void AARPGMineableRock::ApplySelectedRockVisualTransform()
{
    CacheBaseVisualTransform();
    if (!VisualRoot) return;
    VisualRoot->SetRelativeScale3D(BaseVisualRootScale * FMath::Max(0.05f, SelectedRockMeshScale));
    FRotator Rotation = BaseVisualRootRotation;
    Rotation.Yaw += SelectedRockYawDegrees;
    VisualRoot->SetRelativeRotation(Rotation);
}

void AARPGMineableRock::ApplySelectedRockMesh()
{
    if (RockMesh && RockMeshes.IsValidIndex(SelectedRockMeshIndex)) RockMesh->SetStaticMesh(RockMeshes[SelectedRockMeshIndex].Get());
    if (DepletedMesh) DepletedMesh->SetStaticMesh(DepletedMeshAsset);
}

void AARPGMineableRock::ApplyRockStateVisuals()
{
    if (!RockMesh || !DepletedMesh) return;
    if (bBuildingRespawnSuppressed)
    {
        RockMesh->SetVisibility(false, true);
        RockMesh->SetHiddenInGame(true, true);
        RockMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        DepletedMesh->SetVisibility(false, true);
        DepletedMesh->SetHiddenInGame(true, true);
        DepletedMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        return;
    }

    const bool bAvailable = RockState == EARPGMineableRockState::Available;
    RockMesh->SetVisibility(bAvailable, true);
    RockMesh->SetHiddenInGame(!bAvailable, true);
    RockMesh->SetCollisionEnabled(bAvailable ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);

    const bool bShowDepleted = !bAvailable && DepletedMesh->GetStaticMesh() != nullptr;
    DepletedMesh->SetVisibility(bShowDepleted, true);
    DepletedMesh->SetHiddenInGame(!bShowDepleted, true);
    DepletedMesh->SetCollisionEnabled(bShowDepleted ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
}

float AARPGMineableRock::GetEffectiveBonusDropChance(const FARPGMiningBonusDrop& Drop, AActor* Harvester) const
{
    float Chance = FMath::Clamp(Drop.BaseChance, 0.f, 1.f);
    if (!Harvester) return FMath::Min(Chance, FMath::Clamp(MaximumEffectiveBonusChance, 0.f, 1.f));
    if (const UARPGMiningComponent* Mining = Harvester->FindComponentByClass<UARPGMiningComponent>())
    {
        if (Drop.bScaleChanceWithMiningLevel)
            Chance += static_cast<float>(FMath::Max(0, Mining->GetMiningLevel() - Drop.RequiredMiningLevel)) * FMath::Max(0.f, BonusChancePerMiningLevel);
        if (Drop.bScaleChanceWithToolTier)
            Chance += static_cast<float>(FMath::Max(0, Mining->GetBestEquippedToolTier() - Drop.MinimumToolTier)) * FMath::Max(0.f, BonusChancePerToolTier);
    }
    return FMath::Clamp(Chance, 0.f, FMath::Clamp(MaximumEffectiveBonusChance, 0.f, 1.f));
}

bool AARPGMineableRock::GrantOneReward(AActor* Harvester, UARPGItemDefinition* Item, int32 MinQuantity, int32 MaxQuantity, bool bBonus, EARPGMiningBonusDropTrigger Moment)
{
    if (!IsValid(Harvester) || !Item) return false;
    const int32 MinQ = FMath::Max(0, FMath::Min(MinQuantity, MaxQuantity));
    const int32 MaxQ = FMath::Max(MinQ, FMath::Max(MinQuantity, MaxQuantity));
    const int32 Quantity = MaxQ > 0 ? FMath::RandRange(MinQ, MaxQ) : 0;
    if (Quantity <= 0) return false;

    UARPGInventoryComponent* Inventory = Harvester->FindComponentByClass<UARPGInventoryComponent>();
    const bool bAdded = Inventory && Inventory->CanAddItemDefinition(Item, Quantity) && Inventory->AddItemDefinition(Item, Quantity);
    if (bAdded)
    {
        if (UARPGEventRouterComponent* Events = Harvester->FindComponentByClass<UARPGEventRouterComponent>())
        {
            const FName StableItemId = Item->DefinitionId.IsNone() ? Item->GetFName() : Item->DefinitionId;
            if (!StableItemId.IsNone()) Events->ReportItemLooted(StableItemId, Quantity);
        }
    }
    OnRockRewardGranted.Broadcast(Harvester, Item, Quantity, bAdded, bBonus, Moment);
    return bAdded;
}

void AARPGMineableRock::GrantNormalDropArray(AActor* Harvester, const TArray<FARPGMiningDrop>& Drops, EARPGMiningBonusDropTrigger Moment)
{
    for (const FARPGMiningDrop& Drop : Drops)
    {
        if (!Drop.Item) continue;
        if (FMath::FRand() <= FMath::Clamp(Drop.Chance, 0.f, 1.f))
            GrantOneReward(Harvester, Drop.Item, Drop.MinQuantity, Drop.MaxQuantity, false, Moment);
    }
}

void AARPGMineableRock::GrantBonusDrops(AActor* Harvester, EARPGMiningBonusDropTrigger Moment)
{
    if (!Harvester) return;
    const UARPGMiningComponent* Mining = Harvester->FindComponentByClass<UARPGMiningComponent>();
    const int32 MiningLevel = Mining ? Mining->GetMiningLevel() : 1;
    const int32 ToolTier = Mining ? Mining->GetBestEquippedToolTier() : 0;

    for (const FARPGMiningBonusDrop& Drop : BonusDrops)
    {
        if (!Drop.Item) continue;
        const bool bMomentMatches = Drop.Trigger == EARPGMiningBonusDropTrigger::Both || Drop.Trigger == Moment;
        if (!bMomentMatches || MiningLevel < FMath::Max(1, Drop.RequiredMiningLevel) || ToolTier < FMath::Max(0, Drop.MinimumToolTier)) continue;
        if (FMath::FRand() <= GetEffectiveBonusDropChance(Drop, Harvester))
            GrantOneReward(Harvester, Drop.Item, Drop.MinQuantity, Drop.MaxQuantity, true, Moment);
    }
}

void AARPGMineableRock::AwardMiningXP(AActor* Harvester, int64 Amount) const
{
    if (!Harvester || Amount <= 0) return;
    if (UARPGMiningComponent* Mining = Harvester->FindComponentByClass<UARPGMiningComponent>()) Mining->AwardMiningXP(Amount);
}

void AARPGMineableRock::MulticastPlayStrikeFeedback_Implementation(AActor* Harvester, FVector_NetQuantize ImpactLocation, UARPGItemDefinition* EquippedTool)
{
    PlayFeedbackLocal(false, ImpactLocation);
    if (!Harvester || Harvester->GetNetMode() == NM_DedicatedServer || !EquippedTool) return;
    if (!EquippedTool->GatheringHitSound.IsNull())
        if (USoundBase* Sound = EquippedTool->GatheringHitSound.LoadSynchronous())
        {
            const float PitchLow = FMath::Min(EquippedTool->EquipmentAudioPitchMin, EquippedTool->EquipmentAudioPitchMax);
            const float PitchHigh = FMath::Max(EquippedTool->EquipmentAudioPitchMin, EquippedTool->EquipmentAudioPitchMax);
            UGameplayStatics::PlaySoundAtLocation(this, Sound, ImpactLocation, FMath::Max(0.f, EquippedTool->EquipmentAudioVolume), FMath::FRandRange(PitchLow, PitchHigh), 0.f, nullptr, nullptr, nullptr);
        }
}

void AARPGMineableRock::MulticastPlayDepletedFeedback_Implementation(FVector_NetQuantize Location)
{
    PlayFeedbackLocal(true, Location);
}

void AARPGMineableRock::PlayFeedbackLocal(bool bDepleted, const FVector& Location) const
{
    UNiagaraSystem* Niagara = bDepleted ? DepletedNiagara : StrikeNiagara;
    UParticleSystem* Cascade = bDepleted ? DepletedCascadeFallback : StrikeCascadeFallback;
    USoundBase* Sound = bDepleted ? DepletedSound : StrikeSound;
    const FVector Scale(FMath::Max(0.f, FeedbackScale));
    if (Niagara) UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, Niagara, Location, FRotator::ZeroRotator, Scale, true, true, ENCPoolMethod::AutoRelease, true);
    else if (Cascade) UGameplayStatics::SpawnEmitterAtLocation(this, Cascade, Location, FRotator::ZeroRotator, Scale, true, EPSCPoolMethod::AutoRelease, true);
    if (Sound) UGameplayStatics::PlaySoundAtLocation(this, Sound, Location, 1.f, 1.f, 0.f, nullptr, nullptr, nullptr);
}

void AARPGMineableRock::OnRep_RockState(EARPGMineableRockState OldState)
{
    ApplyRockStateVisuals();
    if (OldState != RockState) OnRockStateChanged.Broadcast(RockState);
}

void AARPGMineableRock::OnRep_BuildingRespawnSuppressed()
{
    ApplyRockStateVisuals();
    OnRockBuildingSuppressionChanged.Broadcast(bBuildingRespawnSuppressed);
}

void AARPGMineableRock::OnRep_SelectedRockMeshIndex()
{
    ApplySelectedRockMesh();
    CacheAvailableRespawnBounds();
}

void AARPGMineableRock::OnRep_SelectedRockMeshScale()
{
    ApplySelectedRockVisualTransform();
    CacheAvailableRespawnBounds();
}

void AARPGMineableRock::OnRep_SelectedRockYaw()
{
    ApplySelectedRockVisualTransform();
    CacheAvailableRespawnBounds();
}

void AARPGMineableRock::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    GetWorldTimerManager().ClearTimer(RespawnTimer);
    GetWorldTimerManager().ClearTimer(BuildingSuppressionRecheckTimer);
    BuildingRespawnBlockers.Reset();
    Super::EndPlay(EndPlayReason);
}

void AARPGMineableRock::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AARPGMineableRock, SelectedRockMeshIndex);
    DOREPLIFETIME(AARPGMineableRock, SelectedRockMeshScale);
    DOREPLIFETIME(AARPGMineableRock, SelectedRockYawDegrees);
    DOREPLIFETIME(AARPGMineableRock, CurrentMiningHealth);
    DOREPLIFETIME(AARPGMineableRock, RockState);
    DOREPLIFETIME(AARPGMineableRock, bBuildingRespawnSuppressed);
}
