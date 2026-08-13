#include "Building/ARPGBuildPieceActor.h"
#include "Components/ARPGFactionOwnershipComponent.h"
#include "Components/ARPGInventoryComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Data/ARPGItemDefinition.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/GameStateBase.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"

static FName ARPGResolveBuildAmountId(const FARPGItemAmount& Amount)
{
    if (Amount.Item) return Amount.Item->DefinitionId.IsNone() ? Amount.Item->GetFName() : Amount.Item->DefinitionId;
    return Amount.ItemId;
}

static bool ARPGKindIn(const TArray<EARPGBuildPieceKind>& Kinds, EARPGBuildPieceKind Kind)
{
    return Kinds.Num() == 0 || Kinds.Contains(Kind);
}

static bool ARPGIsWallLike(EARPGBuildPieceKind Kind)
{
    return Kind == EARPGBuildPieceKind::Wall || Kind == EARPGBuildPieceKind::WindowWall || Kind == EARPGBuildPieceKind::Doorway;
}

static void ARPGGetBuildDefinitionLocalBounds(const UARPGBuildPieceDefinition* Piece, FVector& OutMin, FVector& OutMax)
{
    if (Piece)
    {
        if (UStaticMesh* Mesh = Piece->BuildMesh.LoadSynchronous())
        {
            const FBoxSphereBounds Bounds = Mesh->GetBounds();
            OutMin = Bounds.Origin - Bounds.BoxExtent;
            OutMax = Bounds.Origin + Bounds.BoxExtent;
            return;
        }
        OutMin = -Piece->PlacementBounds;
        OutMax = Piece->PlacementBounds;
        return;
    }
    OutMin = FVector::ZeroVector;
    OutMax = FVector::ZeroVector;
}

AARPGBuildPieceActor::AARPGBuildPieceActor()
{
    bReplicates = true;
    SetReplicateMovement(true);
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;

    BuildRoot = CreateDefaultSubobject<USceneComponent>(TEXT("BuildRoot"));
    RootComponent = BuildRoot;
    BuildMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BuildMesh"));
    BuildMesh->SetupAttachment(BuildRoot);
    BuildMesh->SetCollisionProfileName(TEXT("BlockAll"));

    Ownership = CreateDefaultSubobject<UARPGFactionOwnershipComponent>(TEXT("Ownership"));
}

void AARPGBuildPieceActor::BeginPlay()
{
    Super::BeginPlay();
    BaseMeshRelativeLocation = BuildMesh ? BuildMesh->GetRelativeLocation() : FVector::ZeroVector;
    BaseMeshRelativeScale = BuildMesh ? BuildMesh->GetRelativeScale3D() : FVector::OneVector;
    RefreshDefinitionPresentation();
    RefreshConstructionPresentation(true);
}

void AARPGBuildPieceActor::InitializeBuilding(UARPGBuildPieceDefinition* InDefinition, AActor* Builder)
{
    if (!HasAuthority() || !InDefinition) return;
    Definition = InDefinition;
    bRuntimePlaced = true;
    if (!BuildingId.IsValid()) BuildingId = FGuid::NewGuid();
    Health = GetMaxBuildingHealth();
    if (Ownership)
    {
        Ownership->bSameFactionCanUse = InDefinition->bSameFactionCanUse;
        Ownership->bAlliesCanUse = InDefinition->bAlliesCanUse;
        Ownership->bNeutralCanUse = InDefinition->bNeutralCanUse;
        Ownership->bHostilesCanUse = InDefinition->bHostilesCanUse;
        Ownership->bFactionMembersCanModify = InDefinition->bFactionMembersCanModify;
        Ownership->bHostilesCanDamage = InDefinition->bHostilesCanDamage;
        Ownership->InitializeFromActor(Builder, InDefinition->bInheritBuilderFaction);
    }

    RefreshDefinitionPresentation();
    ConstructionDuration = FMath::Max(0.f, InDefinition->ConstructionSeconds);
    bConstructionComplete = ConstructionDuration <= KINDA_SMALL_NUMBER;
    ConstructionStartServerTime = bConstructionComplete ? 0.f : GetAuthoritativeServerTime();
    RefreshConstructionPresentation(true);
    if (!bConstructionComplete && InDefinition->ConstructionStartSound.LoadSynchronous())
        UGameplayStatics::PlaySoundAtLocation(this, InDefinition->ConstructionStartSound.Get(), GetActorLocation());
    ForceNetUpdate();
}

float AARPGBuildPieceActor::GetAuthoritativeServerTime() const
{
    if (const UWorld* World = GetWorld())
        if (const AGameStateBase* GS = World->GetGameState()) return GS->GetServerWorldTimeSeconds();
    return GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
}

float AARPGBuildPieceActor::GetConstructionProgress01() const
{
    if (bConstructionComplete || ConstructionDuration <= KINDA_SMALL_NUMBER) return 1.f;
    return FMath::Clamp((GetAuthoritativeServerTime() - ConstructionStartServerTime) / ConstructionDuration, 0.f, 1.f);
}

float AARPGBuildPieceActor::GetConstructionRemainingSeconds() const
{
    return bConstructionComplete ? 0.f : FMath::Max(0.f, ConstructionDuration * (1.f - GetConstructionProgress01()));
}

void AARPGBuildPieceActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (bConstructionComplete)
    {
        SetActorTickEnabled(false);
        return;
    }
    RefreshConstructionPresentation();
    if (HasAuthority() && GetConstructionProgress01() >= 1.f - KINDA_SMALL_NUMBER) CompleteConstructionAuthority();
}

void AARPGBuildPieceActor::CompleteConstructionAuthority()
{
    if (!HasAuthority() || bConstructionComplete) return;
    bConstructionComplete = true;
    ConstructionStartServerTime = 0.f;
    RefreshConstructionPresentation(true);
    if (Definition && Definition->ConstructionCompleteSound.LoadSynchronous())
        UGameplayStatics::PlaySoundAtLocation(this, Definition->ConstructionCompleteSound.Get(), GetActorLocation());
    OnConstructionCompleted.Broadcast();
    ForceNetUpdate();
}

void AARPGBuildPieceActor::RestoreConstructionState(bool bWasComplete, float RemainingSeconds)
{
    if (!HasAuthority()) return;
    if (bWasComplete || !Definition || Definition->ConstructionSeconds <= KINDA_SMALL_NUMBER)
    {
        bConstructionComplete = true;
        ConstructionDuration = Definition ? FMath::Max(0.f, Definition->ConstructionSeconds) : 0.f;
        ConstructionStartServerTime = 0.f;
    }
    else
    {
        ConstructionDuration = FMath::Max(0.01f, Definition->ConstructionSeconds);
        const float ClampedRemaining = FMath::Clamp(RemainingSeconds, 0.f, ConstructionDuration);
        const float CompletedSeconds = ConstructionDuration - ClampedRemaining;
        ConstructionStartServerTime = GetAuthoritativeServerTime() - CompletedSeconds;
        bConstructionComplete = ClampedRemaining <= KINDA_SMALL_NUMBER;
    }
    RefreshConstructionPresentation(true);
    ForceNetUpdate();
}

void AARPGBuildPieceActor::RefreshDefinitionPresentation()
{
    if (!BuildMesh || !Definition) return;
    if (UStaticMesh* Mesh = Definition->BuildMesh.LoadSynchronous()) BuildMesh->SetStaticMesh(Mesh);
    BaseMeshRelativeLocation = BuildMesh->GetRelativeLocation();
    BaseMeshRelativeScale = BuildMesh->GetRelativeScale3D();
}

void AARPGBuildPieceActor::RefreshConstructionPresentation(bool bForce)
{
    if (!BuildMesh) return;
    const float P = GetConstructionProgress01();
    if (!bForce && FMath::IsNearlyEqual(P, LastConstructionVisualProgress, 0.002f)) return;
    LastConstructionVisualProgress = P;

    if (bConstructionComplete || !Definition)
    {
        BuildMesh->SetRelativeScale3D(BaseMeshRelativeScale);
        BuildMesh->SetRelativeLocation(BaseMeshRelativeLocation);
        BuildMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        BuildMesh->SetScalarParameterValueOnMaterials(TEXT("ConstructionProgress"), 1.f);
        BuildMesh->SetScalarParameterValueOnMaterials(TEXT("BuildProgress"), 1.f);
        SetActorTickEnabled(false);
        OnConstructionProgressChanged.Broadcast(1.f);
        return;
    }

    const float MinZ = FMath::Clamp(Definition->ConstructionStartScaleZ, 0.01f, 1.f);
    const float Reveal = FMath::Lerp(MinZ, 1.f, FMath::SmoothStep(0.f, 1.f, P));
    FVector Scale = BaseMeshRelativeScale;
    Scale.Z *= Reveal;
    BuildMesh->SetRelativeScale3D(Scale);

    FVector LocalMin, LocalMax;
    BuildMesh->GetLocalBounds(LocalMin, LocalMax);
    const float HalfHeight = FMath::Max(0.f, (LocalMax.Z - LocalMin.Z) * 0.5f * BaseMeshRelativeScale.Z);
    FVector Location = BaseMeshRelativeLocation;
    Location.Z -= HalfHeight * (1.f - Reveal);
    BuildMesh->SetRelativeLocation(Location);
    BuildMesh->SetCollisionEnabled(Definition->bCollisionDuringConstruction ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
    BuildMesh->SetScalarParameterValueOnMaterials(Definition->ConstructionProgressMaterialParameter, P);
    BuildMesh->SetScalarParameterValueOnMaterials(TEXT("BuildProgress"), P);
    SetActorTickEnabled(true);
    OnConstructionProgressChanged.Broadcast(P);
}

float AARPGBuildPieceActor::GetMaxBuildingHealth() const { return Definition ? FMath::Max(1.f, Definition->MaxHealth) : 100.f; }
bool AARPGBuildPieceActor::CanActorUse(AActor* Actor) const { return !Ownership || Ownership->CanActorUse(Actor); }
bool AARPGBuildPieceActor::CanActorModify(AActor* Actor) const { return !Ownership || Ownership->CanActorModify(Actor); }

bool AARPGBuildPieceActor::ApplyBuildingDamage(float Amount, AActor* DamageCauser)
{
    if (!HasAuthority() || Amount <= 0.f || Health <= 0.f || !bConstructionComplete || (DamageCauser && Ownership && !Ownership->CanActorDamage(DamageCauser))) return false;
    const float Old = Health;
    Health = FMath::Clamp(Health - Amount, 0.f, GetMaxBuildingHealth());
    OnBuildingHealthChanged.Broadcast(Health, Health - Old);
    if (Health <= 0.f) { OnBuildingDestroyed.Broadcast(); Destroy(); }
    return true;
}

bool AARPGBuildPieceActor::RepairBuilding(float Amount, AActor* Repairer)
{
    if (!HasAuthority() || Amount <= 0.f || Health <= 0.f || !bConstructionComplete || (Repairer && Ownership && !Ownership->CanActorModify(Repairer))) return false;
    const float Old = Health;
    Health = FMath::Clamp(Health + Amount, 0.f, GetMaxBuildingHealth());
    OnBuildingHealthChanged.Broadcast(Health, Health - Old);
    return Health > Old;
}

bool AARPGBuildPieceActor::Demolish(AActor* Requester)
{
    if (!HasAuthority() || !bAllowDemolish || (Requester && Ownership && !Ownership->CanActorModify(Requester))) return false;

    if (Requester && Definition && Definition->bRefundOnDemolish && Definition->DemolishRefundFraction > 0.f)
    {
        if (UARPGInventoryComponent* Inventory = Requester->FindComponentByClass<UARPGInventoryComponent>())
        {
            for (const FARPGItemAmount& Cost : Definition->BuildCost)
            {
                const int32 RefundQuantity = FMath::FloorToInt(FMath::Max(0, Cost.Quantity) * Definition->DemolishRefundFraction);
                if (RefundQuantity <= 0) continue;
                if (Cost.Item) Inventory->AddItemDefinition(Cost.Item, RefundQuantity);
                else if (!ARPGResolveBuildAmountId(Cost).IsNone()) Inventory->AddItem(ARPGResolveBuildAmountId(Cost), RefundQuantity);
            }
        }
    }
    OnBuildingDestroyed.Broadcast();
    Destroy();
    return true;
}

void AARPGBuildPieceActor::GetSnapTransformsFor(const UARPGBuildPieceDefinition* IncomingPiece, TArray<FTransform>& OutWorldTransforms) const
{
    OutWorldTransforms.Reset();
    if (!Definition || !IncomingPiece || !bConstructionComplete) return;

    const auto AddLocal = [&](const FTransform& LocalTransform)
    {
        OutWorldTransforms.Add(LocalTransform * GetActorTransform());
    };

    FVector TargetMin, TargetMax, IncomingMin, IncomingMax;
    ARPGGetBuildDefinitionLocalBounds(Definition, TargetMin, TargetMax);
    ARPGGetBuildDefinitionLocalBounds(IncomingPiece, IncomingMin, IncomingMax);

    // Vertical snap positions are derived from the actual mesh bounds/pivots. This makes the
    // structural graph work with bottom-pivot, center-pivot, and mixed modular kits instead of
    // assuming every actor origin is at its geometric center.
    const float AlignTopPlaneZ = TargetMax.Z - IncomingMax.Z;
    const float IncomingOnTargetTopZ = TargetMax.Z - IncomingMin.Z;
    const float AlignBottomPlaneZ = TargetMin.Z - IncomingMin.Z;
    const float IncomingAboveTargetZ = TargetMax.Z - IncomingMin.Z;

    if (Definition->bGenerateStandardSnapPoints)
    {
        const EARPGBuildPieceKind TargetKind = Definition->PieceKind;
        const EARPGBuildPieceKind IncomingKind = IncomingPiece->PieceKind;
        const float Grid = FMath::Max(1.f, Definition->SnapSize);
        const float IncomingGrid = FMath::Max(1.f, IncomingPiece->SnapSize);
        const float Step = 0.5f * (Grid + IncomingGrid);
        const float Half = Grid * 0.5f;
        const float WallHeight = FMath::Max(1.f, Definition->StandardWallHeight);

        const bool bTargetHorizontal = TargetKind == EARPGBuildPieceKind::Foundation || TargetKind == EARPGBuildPieceKind::Floor || TargetKind == EARPGBuildPieceKind::Ceiling || TargetKind == EARPGBuildPieceKind::Roof;
        if (bTargetHorizontal)
        {
            // Continue the same visible structural plane. Top alignment avoids seams when two pieces
            // have different pivots or thicknesses but are intended to share the same finished surface.
            const bool bIncomingSamePlane =
                (TargetKind == EARPGBuildPieceKind::Foundation && IncomingKind == EARPGBuildPieceKind::Foundation) ||
                ((TargetKind == EARPGBuildPieceKind::Floor || TargetKind == EARPGBuildPieceKind::Ceiling) &&
                 (IncomingKind == EARPGBuildPieceKind::Floor || IncomingKind == EARPGBuildPieceKind::Ceiling)) ||
                (TargetKind == EARPGBuildPieceKind::Roof && IncomingKind == EARPGBuildPieceKind::Roof);
            if (bIncomingSamePlane)
            {
                AddLocal(FTransform(FRotator::ZeroRotator, FVector( Step, 0.f, AlignTopPlaneZ)));
                AddLocal(FTransform(FRotator::ZeroRotator, FVector(-Step, 0.f, AlignTopPlaneZ)));
                AddLocal(FTransform(FRotator::ZeroRotator, FVector(0.f,  Step, AlignTopPlaneZ)));
                AddLocal(FTransform(FRotator::ZeroRotator, FVector(0.f, -Step, AlignTopPlaneZ)));
            }

            // Wall-family pieces sit with their actual mesh bottom on the target's actual top.
            if (ARPGIsWallLike(IncomingKind))
            {
                AddLocal(FTransform(FRotator(0.f,   0.f, 0.f), FVector(0.f,  Half, IncomingOnTargetTopZ)));
                AddLocal(FTransform(FRotator(0.f, 180.f, 0.f), FVector(0.f, -Half, IncomingOnTargetTopZ)));
                AddLocal(FTransform(FRotator(0.f,  90.f, 0.f), FVector(Half, 0.f, IncomingOnTargetTopZ)));
                AddLocal(FTransform(FRotator(0.f, -90.f, 0.f), FVector(-Half, 0.f, IncomingOnTargetTopZ)));
            }

            if (IncomingKind == EARPGBuildPieceKind::Stair && TargetKind != EARPGBuildPieceKind::Roof)
            {
                AddLocal(FTransform(FRotator(0.f,   0.f, 0.f), FVector(0.f, 0.f, IncomingOnTargetTopZ)));
                AddLocal(FTransform(FRotator(0.f,  90.f, 0.f), FVector(0.f, 0.f, IncomingOnTargetTopZ)));
                AddLocal(FTransform(FRotator(0.f, 180.f, 0.f), FVector(0.f, 0.f, IncomingOnTargetTopZ)));
                AddLocal(FTransform(FRotator(0.f, -90.f, 0.f), FVector(0.f, 0.f, IncomingOnTargetTopZ)));
            }

            // On a foundation, the next floor/ceiling/roof lives one wall story above the
            // foundation's finished top surface, with the incoming mesh bottom aligned there.
            if (IncomingKind == EARPGBuildPieceKind::Ceiling || IncomingKind == EARPGBuildPieceKind::Floor || IncomingKind == EARPGBuildPieceKind::Roof)
            {
                if (TargetKind == EARPGBuildPieceKind::Foundation)
                {
                    const float StorySurfaceZ = TargetMax.Z + WallHeight - IncomingMin.Z;
                    AddLocal(FTransform(FRotator::ZeroRotator, FVector(0.f, 0.f, StorySurfaceZ)));
                }
            }
        }

        if (ARPGIsWallLike(TargetKind))
        {
            if (ARPGIsWallLike(IncomingKind))
            {
                // Side continuation aligns wall bottoms; stacking aligns the next wall bottom to this wall top.
                AddLocal(FTransform(FRotator::ZeroRotator, FVector(Grid, 0.f, AlignBottomPlaneZ)));
                AddLocal(FTransform(FRotator::ZeroRotator, FVector(-Grid, 0.f, AlignBottomPlaneZ)));
                AddLocal(FTransform(FRotator::ZeroRotator, FVector(0.f, 0.f, IncomingAboveTargetZ)));
            }

            // Horizontal structural pieces attach at the actual wall top instead of assuming a centered pivot.
            if (IncomingKind == EARPGBuildPieceKind::Ceiling || IncomingKind == EARPGBuildPieceKind::Floor || IncomingKind == EARPGBuildPieceKind::Roof)
            {
                AddLocal(FTransform(FRotator::ZeroRotator, FVector(0.f,  Half, IncomingAboveTargetZ)));
                AddLocal(FTransform(FRotator::ZeroRotator, FVector(0.f, -Half, IncomingAboveTargetZ)));
            }

            if (TargetKind == EARPGBuildPieceKind::Doorway && IncomingKind == EARPGBuildPieceKind::Door)
                AddLocal(FTransform::Identity);
            if (TargetKind == EARPGBuildPieceKind::WindowWall && IncomingKind == EARPGBuildPieceKind::Window)
                AddLocal(FTransform::Identity);
        }

        if (TargetKind == EARPGBuildPieceKind::Pillar && ARPGIsWallLike(IncomingKind))
        {
            const float PillarTopZ = TargetMax.Z - IncomingMin.Z;
            AddLocal(FTransform(FRotator::ZeroRotator, FVector(Half, 0.f, PillarTopZ)));
            AddLocal(FTransform(FRotator(0.f, 90.f, 0.f), FVector(0.f, Half, PillarTopZ)));
        }
    }

    for (const FARPGBuildSnapPoint& Point : Definition->CustomSnapPoints)
        if (Point.bEnabled && ARPGKindIn(Point.AcceptedIncomingKinds, IncomingPiece->PieceKind)) AddLocal(Point.IncomingPlacementTransform);
}

void AARPGBuildPieceActor::OnRep_Definition()
{
    RefreshDefinitionPresentation();
    RefreshConstructionPresentation(true);
}
void AARPGBuildPieceActor::OnRep_Health(float OldHealth) { OnBuildingHealthChanged.Broadcast(Health, Health - OldHealth); }
void AARPGBuildPieceActor::OnRep_ConstructionState() { RefreshConstructionPresentation(true); }

void AARPGBuildPieceActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AARPGBuildPieceActor, BuildingId);
    DOREPLIFETIME(AARPGBuildPieceActor, Definition);
    DOREPLIFETIME(AARPGBuildPieceActor, Health);
    DOREPLIFETIME(AARPGBuildPieceActor, UpgradeLevel);
    DOREPLIFETIME(AARPGBuildPieceActor, bRuntimePlaced);
    DOREPLIFETIME(AARPGBuildPieceActor, bConstructionComplete);
    DOREPLIFETIME(AARPGBuildPieceActor, ConstructionStartServerTime);
    DOREPLIFETIME(AARPGBuildPieceActor, ConstructionDuration);
}
