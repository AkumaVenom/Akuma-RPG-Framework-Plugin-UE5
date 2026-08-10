#include "Gathering/ARPGTree.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/ARPGWoodcuttingComponent.h"
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
#include "TimerManager.h"

AARPGTree::AARPGTree()
{
    bReplicates = true;
    SetReplicateMovement(false);
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;

    Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(Root);

    FallPivot = CreateDefaultSubobject<USceneComponent>(TEXT("FallPivot"));
    FallPivot->SetupAttachment(Root);

    TreeMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TreeMesh"));
    TreeMesh->SetupAttachment(FallPivot);
    TreeMesh->SetMobility(EComponentMobility::Movable);
    TreeMesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));

    StumpMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StumpMesh"));
    StumpMesh->SetupAttachment(Root);
    StumpMesh->SetMobility(EComponentMobility::Movable);
    StumpMesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));
    StumpMesh->SetHiddenInGame(true);
    StumpMesh->SetVisibility(false);
}

void AARPGTree::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    bBaseVisualScalesCached = false;
    if (StumpMesh) StumpMesh->SetStaticMesh(StumpMeshAsset);
    if (SelectedTreeMeshIndex == INDEX_NONE && TreeMeshes.Num() > 0) SelectedTreeMeshIndex = 0;
    ApplySelectedTreeMesh();
    ApplyTreeStateVisuals(true);
}

void AARPGTree::BeginPlay()
{
    Super::BeginPlay();
    CacheBaseVisualScales();
    if (HasAuthority())
    {
        CurrentChopHealth = FMath::Max(1.f, MaxChopHealth);
        if (TreeMeshes.Num() > 0)
        {
            if (bRandomizeTreeMesh) SelectRandomTreeMesh();
            else SetTreeMeshIndex(FMath::Clamp(SelectedTreeMeshIndex == INDEX_NONE ? 0 : SelectedTreeMeshIndex, 0, TreeMeshes.Num() - 1));
        }

        float MinScale = 1.f;
        float MaxScale = 1.f;
        GetSanitizedMeshScaleRange(MinScale, MaxScale);
        if (bRandomizeTreeMeshScale) SelectRandomTreeMeshScale();
        else SetTreeMeshScale(FMath::Clamp(SelectedTreeMeshScale, MinScale, MaxScale));

        TreeState = EARPGTreeState::Standing;
    }
    ApplySelectedTreeMesh();
    ApplySelectedTreeMeshScale();
    ApplyTreeStateVisuals(true);
}

float AARPGTree::GetChopHealthPercent() const
{
    return FMath::Clamp(CurrentChopHealth / FMath::Max(1.f, MaxChopHealth), 0.f, 1.f);
}

UStaticMesh* AARPGTree::GetSelectedTreeMesh() const
{
    if (TreeMeshes.IsValidIndex(SelectedTreeMeshIndex))
    {
        return TreeMeshes[SelectedTreeMeshIndex].Get();
    }

    return TreeMesh ? TreeMesh->GetStaticMesh() : nullptr;
}

FVector AARPGTree::GetChopImpactLocation(AActor* Harvester) const
{
    FVector Location = GetActorLocation() + FVector(0.f, 0.f, ChopImpactHeight);
    if (TreeMesh && TreeMesh->Bounds.SphereRadius > KINDA_SMALL_NUMBER)
    {
        const FBoxSphereBounds Bounds = TreeMesh->Bounds;
        const float Low = Bounds.Origin.Z - Bounds.BoxExtent.Z;
        const float High = Bounds.Origin.Z + Bounds.BoxExtent.Z;
        Location.Z = FMath::Clamp(GetActorLocation().Z + ChopImpactHeight, Low, High);
    }
    return Location;
}

bool AARPGTree::CanBeChoppedBy(AActor* Harvester, FText& OutFailureReason) const
{
    OutFailureReason = FText::GetEmpty();
    if (!IsValid(Harvester))
    {
        OutFailureReason = FText::FromString(TEXT("No harvester supplied."));
        return false;
    }
    if (!IsStanding())
    {
        OutFailureReason = FText::FromString(TEXT("This tree has already been felled."));
        return false;
    }
    const UARPGWoodcuttingComponent* Woodcutting = Harvester->FindComponentByClass<UARPGWoodcuttingComponent>();
    if (!Woodcutting)
    {
        OutFailureReason = FText::FromString(TEXT("This actor has no Woodcutting component."));
        return false;
    }
    if (Woodcutting->GetWoodcuttingLevel() < RequiredWoodcuttingLevel)
    {
        OutFailureReason = FText::Format(FText::FromString(TEXT("Requires Woodcutting level {0}.")), FText::AsNumber(RequiredWoodcuttingLevel));
        return false;
    }
    if (!Woodcutting->HasValidToolForTree(this))
    {
        OutFailureReason = MinimumToolTier > 0
            ? FText::Format(FText::FromString(TEXT("Requires a valid Woodcutting tool of tier {0} or better.")), FText::AsNumber(MinimumToolTier))
            : FText::FromString(TEXT("Requires a valid Woodcutting tool."));
        return false;
    }
    return true;
}

bool AARPGTree::ApplyChop(AActor* Harvester, float ChopPower)
{
    if (!HasAuthority() || !IsValid(Harvester) || ChopPower <= 0.f) return false;
    FText FailureReason;
    if (!CanBeChoppedBy(Harvester, FailureReason)) return false;

    const float DamageApplied = FMath::Min(CurrentChopHealth, FMath::Max(0.01f, ChopPower / FMath::Max(0.05f, ChopResistance)));
    CurrentChopHealth = FMath::Max(0.f, CurrentChopHealth - DamageApplied);
    AwardWoodcuttingXP(Harvester, XPPerSuccessfulChop);

    const FVector ImpactLocation = GetChopImpactLocation(Harvester);
    UARPGItemDefinition* EquippedTool = nullptr;
    if (UARPGWoodcuttingComponent* Woodcutting = Harvester->FindComponentByClass<UARPGWoodcuttingComponent>())
        EquippedTool = Woodcutting->GetBestEquippedWoodcuttingTool();
    MulticastPlayChopFeedback(Harvester, ImpactLocation, EquippedTool);
    OnTreeChopped.Broadcast(this, Harvester, DamageApplied, CurrentChopHealth);

    if (CurrentChopHealth <= KINDA_SMALL_NUMBER) FellTree(Harvester);
    return true;
}

bool AARPGTree::FellTree(AActor* Harvester)
{
    if (!HasAuthority() || !IsStanding()) return false;

    CurrentChopHealth = 0.f;
    FVector Direction = FVector::ForwardVector;
    if (bFallAwayFromHarvester && Harvester)
        Direction = (GetActorLocation() - Harvester->GetActorLocation()).GetSafeNormal2D();
    if (Direction.IsNearlyZero()) Direction = GetActorForwardVector().GetSafeNormal2D();
    if (FallDirectionRandomDegrees > KINDA_SMALL_NUMBER)
        Direction = Direction.RotateAngleAxis(FMath::FRandRange(-FallDirectionRandomDegrees, FallDirectionRandomDegrees), FVector::UpVector).GetSafeNormal2D();

    FallDirection = Direction;
    TreeState = EARPGTreeState::Falling;
    MulticastBeginTreeFall(Direction);
    MulticastPlayFellFeedback(GetActorLocation());

    AwardWoodcuttingXP(Harvester, XPOnFell);
    if (bGrantRewardsOnFell) GrantRewards(Harvester);
    OnTreeFelled.Broadcast(this, Harvester);
    OnTreeStateChanged.Broadcast(TreeState);

    const float StumpDelay = FMath::Max(0.05f, FallDuration + FallenTreeVisibleSeconds);
    GetWorldTimerManager().SetTimer(StumpTimer, this, &AARPGTree::EnterStumpAuthority, StumpDelay, false);
    if (bRespawn)
    {
        const float RespawnDelay = FMath::Max(StumpDelay + 0.05f, RespawnSeconds);
        GetWorldTimerManager().SetTimer(RespawnTimer, this, &AARPGTree::ForceRespawn, RespawnDelay, false);
    }
    return true;
}

void AARPGTree::EnterStumpAuthority()
{
    if (!HasAuthority() || TreeState == EARPGTreeState::Standing) return;
    const EARPGTreeState OldState = TreeState;
    TreeState = EARPGTreeState::Stump;
    ApplyTreeStateVisuals();
    if (OldState != TreeState) OnTreeStateChanged.Broadcast(TreeState);
}

void AARPGTree::ForceRespawn()
{
    if (!HasAuthority()) return;
    GetWorldTimerManager().ClearTimer(StumpTimer);
    GetWorldTimerManager().ClearTimer(RespawnTimer);
    if (bRerollTreeMeshOnRespawn && TreeMeshes.Num() > 0) SelectRandomTreeMesh();
    if (bRerollTreeMeshScaleOnRespawn) SelectRandomTreeMeshScale();
    CurrentChopHealth = FMath::Max(1.f, MaxChopHealth);
    FallDirection = FVector::ForwardVector;
    const EARPGTreeState OldState = TreeState;
    TreeState = EARPGTreeState::Standing;
    ApplyTreeStateVisuals(true);
    if (OldState != TreeState) OnTreeStateChanged.Broadcast(TreeState);
    OnTreeRespawned.Broadcast(this);
}

void AARPGTree::SelectRandomTreeMesh()
{
    if (!HasAuthority() || TreeMeshes.Num() <= 0) return;
    SetTreeMeshIndex(FMath::RandRange(0, TreeMeshes.Num() - 1));
}

bool AARPGTree::SetTreeMeshIndex(int32 NewIndex)
{
    if (!HasAuthority() || !TreeMeshes.IsValidIndex(NewIndex)) return false;
    SelectedTreeMeshIndex = NewIndex;
    ApplySelectedTreeMesh();
    return true;
}

void AARPGTree::GetSanitizedMeshScaleRange(float& OutMinScale, float& OutMaxScale) const
{
    const float A = FMath::Max(0.05f, MinimumMeshScale);
    const float B = FMath::Max(0.05f, MaximumMeshScale);
    OutMinScale = FMath::Min(A, B);
    OutMaxScale = FMath::Max(A, B);
}

void AARPGTree::SelectRandomTreeMeshScale()
{
    if (!HasAuthority()) return;
    float MinScale = 1.f;
    float MaxScale = 1.f;
    GetSanitizedMeshScaleRange(MinScale, MaxScale);
    SetTreeMeshScale(FMath::FRandRange(MinScale, MaxScale));
}

bool AARPGTree::SetTreeMeshScale(float NewScale)
{
    if (!HasAuthority()) return false;
    float MinScale = 1.f;
    float MaxScale = 1.f;
    GetSanitizedMeshScaleRange(MinScale, MaxScale);
    SelectedTreeMeshScale = FMath::Clamp(NewScale, MinScale, MaxScale);
    ApplySelectedTreeMeshScale();
    return true;
}

void AARPGTree::CacheBaseVisualScales()
{
    if (bBaseVisualScalesCached) return;
    BaseFallPivotScale = FallPivot ? FallPivot->GetRelativeScale3D() : FVector::OneVector;
    BaseStumpMeshScale = StumpMesh ? StumpMesh->GetRelativeScale3D() : FVector::OneVector;
    bBaseVisualScalesCached = true;
}

void AARPGTree::ApplySelectedTreeMeshScale()
{
    CacheBaseVisualScales();
    const float Scale = FMath::Max(0.05f, SelectedTreeMeshScale);
    if (FallPivot) FallPivot->SetRelativeScale3D(BaseFallPivotScale * Scale);
    if (StumpMesh) StumpMesh->SetRelativeScale3D(BaseStumpMeshScale * Scale);
}

void AARPGTree::ApplySelectedTreeMesh()
{
    if (!TreeMesh) return;
    if (TreeMeshes.IsValidIndex(SelectedTreeMeshIndex))
        TreeMesh->SetStaticMesh(TreeMeshes[SelectedTreeMeshIndex].Get());
    if (StumpMesh && StumpMeshAsset) StumpMesh->SetStaticMesh(StumpMeshAsset);
}

void AARPGTree::ApplyTreeStateVisuals(bool bLateJoinOrImmediate)
{
    if (!TreeMesh || !StumpMesh || !FallPivot) return;
    if (TreeState == EARPGTreeState::Standing)
    {
        bLocalFallActive = false;
        SetActorTickEnabled(false);
        FallPivot->SetRelativeRotation(FRotator::ZeroRotator);
        TreeMesh->SetVisibility(true, true);
        TreeMesh->SetHiddenInGame(false, true);
        TreeMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        StumpMesh->SetVisibility(false, true);
        StumpMesh->SetHiddenInGame(true, true);
        StumpMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }
    else if (TreeState == EARPGTreeState::Falling)
    {
        StumpMesh->SetVisibility(StumpMesh->GetStaticMesh() != nullptr, true);
        StumpMesh->SetHiddenInGame(StumpMesh->GetStaticMesh() == nullptr, true);
        StumpMesh->SetCollisionEnabled(StumpMesh->GetStaticMesh() ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
        TreeMesh->SetVisibility(true, true);
        TreeMesh->SetHiddenInGame(false, true);
        TreeMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        if (bLateJoinOrImmediate)
        {
            const FVector LocalDirection = GetActorTransform().InverseTransformVectorNoScale(FallDirection).GetSafeNormal2D();
            const FVector Axis = FVector::CrossProduct(FVector::UpVector, LocalDirection).GetSafeNormal();
            FallPivot->SetRelativeRotation(FQuat(Axis.IsNearlyZero() ? FVector::RightVector : Axis, FMath::DegreesToRadians(FallAngleDegrees)));
        }
    }
    else
    {
        bLocalFallActive = false;
        SetActorTickEnabled(false);
        TreeMesh->SetVisibility(false, true);
        TreeMesh->SetHiddenInGame(true, true);
        TreeMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        StumpMesh->SetVisibility(StumpMesh->GetStaticMesh() != nullptr, true);
        StumpMesh->SetHiddenInGame(StumpMesh->GetStaticMesh() == nullptr, true);
        StumpMesh->SetCollisionEnabled(StumpMesh->GetStaticMesh() ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
    }
}

void AARPGTree::StartFallVisualLocal(const FVector& InFallDirection)
{
    if (!FallPivot || !TreeMesh) return;
    LocalFallElapsed = 0.f;
    LocalFallDirection = InFallDirection.GetSafeNormal2D();
    if (LocalFallDirection.IsNearlyZero()) LocalFallDirection = GetActorForwardVector().GetSafeNormal2D();
    bLocalFallActive = true;
    FallPivot->SetRelativeRotation(FRotator::ZeroRotator);
    TreeMesh->SetVisibility(true, true);
    TreeMesh->SetHiddenInGame(false, true);
    if (StumpMesh && StumpMesh->GetStaticMesh())
    {
        StumpMesh->SetVisibility(true, true);
        StumpMesh->SetHiddenInGame(false, true);
    }
    SetActorTickEnabled(true);
}

void AARPGTree::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!bLocalFallActive || !FallPivot) return;
    LocalFallElapsed += DeltaSeconds;
    const float Alpha = FMath::Clamp(LocalFallElapsed / FMath::Max(0.05f, FallDuration), 0.f, 1.f);
    const float SmoothAlpha = Alpha * Alpha * (3.f - 2.f * Alpha);
    const FVector LocalDirection = GetActorTransform().InverseTransformVectorNoScale(LocalFallDirection).GetSafeNormal2D();
    FVector Axis = FVector::CrossProduct(FVector::UpVector, LocalDirection).GetSafeNormal();
    if (Axis.IsNearlyZero()) Axis = FVector::RightVector;
    FallPivot->SetRelativeRotation(FQuat(Axis, FMath::DegreesToRadians(FallAngleDegrees * SmoothAlpha)));
    if (Alpha >= 1.f) FinishFallVisualLocal();
}

void AARPGTree::FinishFallVisualLocal()
{
    bLocalFallActive = false;
    SetActorTickEnabled(false);
}

void AARPGTree::MulticastBeginTreeFall_Implementation(FVector_NetQuantizeNormal InFallDirection)
{
    StartFallVisualLocal(InFallDirection);
}

void AARPGTree::MulticastPlayChopFeedback_Implementation(AActor* Harvester, FVector_NetQuantize ImpactLocation, UARPGItemDefinition* EquippedTool)
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

void AARPGTree::MulticastPlayFellFeedback_Implementation(FVector_NetQuantize Location)
{
    PlayFeedbackLocal(true, Location);
}

void AARPGTree::PlayFeedbackLocal(bool bFell, const FVector& Location) const
{
    UNiagaraSystem* Niagara = bFell ? FellNiagara : ChopNiagara;
    UParticleSystem* Cascade = bFell ? FellCascadeFallback : ChopCascadeFallback;
    USoundBase* Sound = bFell ? FellSound : ChopSound;
    const FVector Scale(FMath::Max(0.f, FeedbackScale));
    if (Niagara) UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, Niagara, Location, FRotator::ZeroRotator, Scale, true, true, ENCPoolMethod::AutoRelease, true);
    else if (Cascade) UGameplayStatics::SpawnEmitterAtLocation(this, Cascade, Location, FRotator::ZeroRotator, Scale, true, EPSCPoolMethod::AutoRelease, true);
    if (Sound) UGameplayStatics::PlaySoundAtLocation(this, Sound, Location, 1.f, 1.f, 0.f, nullptr, nullptr, nullptr);
}

void AARPGTree::AwardWoodcuttingXP(AActor* Harvester, int64 Amount) const
{
    if (!Harvester || Amount <= 0) return;
    if (UARPGWoodcuttingComponent* Woodcutting = Harvester->FindComponentByClass<UARPGWoodcuttingComponent>())
        Woodcutting->AwardWoodcuttingXP(Amount);
}

void AARPGTree::GrantRewards(AActor* Harvester)
{
    if (!IsValid(Harvester)) return;
    UARPGInventoryComponent* Inventory = Harvester->FindComponentByClass<UARPGInventoryComponent>();
    UARPGEventRouterComponent* Events = Harvester->FindComponentByClass<UARPGEventRouterComponent>();

    auto Grant = [&](UARPGItemDefinition* Item, int32 MinQuantity, int32 MaxQuantity)
    {
        if (!Item) return;
        const int32 MinQ = FMath::Max(0, FMath::Min(MinQuantity, MaxQuantity));
        const int32 MaxQ = FMath::Max(MinQ, FMath::Max(MinQuantity, MaxQuantity));
        const int32 Quantity = MaxQ > 0 ? FMath::RandRange(MinQ, MaxQ) : 0;
        if (Quantity <= 0) return;
        const bool bAdded = Inventory && Inventory->CanAddItemDefinition(Item, Quantity) && Inventory->AddItemDefinition(Item, Quantity);
        if (bAdded && Events)
        {
            const FName StableItemId = Item->DefinitionId.IsNone() ? Item->GetFName() : Item->DefinitionId;
            if (!StableItemId.IsNone()) Events->ReportItemLooted(StableItemId, Quantity);
        }
        OnTreeRewardGranted.Broadcast(Harvester, Item, Quantity, bAdded);
    };

    Grant(WoodItem, MinWoodQuantity, MaxWoodQuantity);
    for (const FARPGTreeBonusDrop& Drop : BonusDrops)
        if (Drop.Item && FMath::FRand() <= FMath::Clamp(Drop.Chance, 0.f, 1.f)) Grant(Drop.Item, Drop.MinQuantity, Drop.MaxQuantity);
}

void AARPGTree::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    GetWorldTimerManager().ClearTimer(StumpTimer);
    GetWorldTimerManager().ClearTimer(RespawnTimer);
    bLocalFallActive = false;
    SetActorTickEnabled(false);
    Super::EndPlay(EndPlayReason);
}

void AARPGTree::OnRep_TreeState(EARPGTreeState OldState)
{
    if (TreeState == EARPGTreeState::Falling)
    {
        if (!bLocalFallActive) StartFallVisualLocal(FallDirection);
    }
    else
    {
        ApplyTreeStateVisuals(true);
    }
    if (OldState != TreeState) OnTreeStateChanged.Broadcast(TreeState);
}

void AARPGTree::OnRep_SelectedTreeMeshIndex()
{
    ApplySelectedTreeMesh();
}

void AARPGTree::OnRep_SelectedTreeMeshScale()
{
    ApplySelectedTreeMeshScale();
}

void AARPGTree::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AARPGTree, SelectedTreeMeshIndex);
    DOREPLIFETIME(AARPGTree, SelectedTreeMeshScale);
    DOREPLIFETIME(AARPGTree, CurrentChopHealth);
    DOREPLIFETIME(AARPGTree, TreeState);
    DOREPLIFETIME(AARPGTree, FallDirection);
}
