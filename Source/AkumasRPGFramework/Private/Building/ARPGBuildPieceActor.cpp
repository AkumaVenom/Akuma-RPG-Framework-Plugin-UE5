#include "Building/ARPGBuildPieceActor.h"
#include "Components/ARPGFactionOwnershipComponent.h"
#include "Components/ARPGInventoryComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/MeshComponent.h"
#include "Data/ARPGItemDefinition.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
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
        // Skeletal presentation is additive and takes precedence only when a valid skeletal asset is
        // explicitly assigned. Existing Static Mesh definitions therefore resolve exactly as before.
        if (USkeletalMesh* Mesh = Piece->BuildSkeletalMesh.LoadSynchronous())
        {
            const FBoxSphereBounds Bounds = Mesh->GetBounds();
            const FBox RawBox(Bounds.Origin - Bounds.BoxExtent, Bounds.Origin + Bounds.BoxExtent);
            const FBox ActorLocalBox = RawBox.TransformBy(Piece->MeshRelativeTransform);
            OutMin = ActorLocalBox.Min;
            OutMax = ActorLocalBox.Max;
            return;
        }
        if (UStaticMesh* Mesh = Piece->BuildMesh.LoadSynchronous())
        {
            const FBoxSphereBounds Bounds = Mesh->GetBounds();
            const FBox RawBox(Bounds.Origin - Bounds.BoxExtent, Bounds.Origin + Bounds.BoxExtent);
            const FBox ActorLocalBox = RawBox.TransformBy(Piece->MeshRelativeTransform);
            OutMin = ActorLocalBox.Min;
            OutMax = ActorLocalBox.Max;
            return;
        }
        OutMin = -Piece->PlacementBounds;
        OutMax = Piece->PlacementBounds;
        return;
    }
    OutMin = FVector::ZeroVector;
    OutMax = FVector::ZeroVector;
}

static FVector ARPGGetBottomAlignedInsertTranslation(
    const FVector& TargetMin,
    const FVector& TargetMax,
    const FVector& IncomingMin,
    const FVector& IncomingMax)
{
    const FVector TargetCenter = (TargetMin + TargetMax) * 0.5f;
    const FVector IncomingCenter = (IncomingMin + IncomingMax) * 0.5f;

    // Doors are floor-standing hosted inserts. Center their visible geometry in the Doorway's XY
    // envelope, but preserve the proven bottom-plane contract on Z. This keeps the existing Door
    // behavior exactly unchanged for bottom-, center- and corner-pivot modular kits.
    return FVector(
        TargetCenter.X - IncomingCenter.X,
        TargetCenter.Y - IncomingCenter.Y,
        TargetMin.Z - IncomingMin.Z);
}

static FVector ARPGGetCenteredInsertTranslation(
    const FVector& TargetMin,
    const FVector& TargetMax,
    const FVector& IncomingMin,
    const FVector& IncomingMax,
    const FVector& HostLocalOffset)
{
    const FVector TargetCenter = (TargetMin + TargetMax) * 0.5f;
    const FVector IncomingCenter = (IncomingMin + IncomingMax) * 0.5f;

    // Windows are suspended hosted inserts, not floor-standing Doors. Center the incoming transformed
    // visible bounds against the WindowWall transformed visible bounds in all three axes. An optional
    // host-local WindowInsertOffset then adapts intentionally off-centre openings (for example a high
    // sill) without changing the Window asset, its generic PlacementOffset, or the structural actor.
    return TargetCenter - IncomingCenter + HostLocalOffset;
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

    BuildSkeletalMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("BuildSkeletalMesh"));
    BuildSkeletalMesh->SetupAttachment(BuildRoot);
    BuildSkeletalMesh->SetCollisionProfileName(TEXT("BlockAll"));
    BuildSkeletalMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    BuildSkeletalMesh->SetGenerateOverlapEvents(false);
    BuildSkeletalMesh->SetVisibility(false, true);
    BuildSkeletalMesh->SetHiddenInGame(true, true);
    // A plain skeletal build visual is a stable posed asset, not an always-running character. Keep its
    // component Tick dormant; dedicated animated actors (for example the Window interaction layer)
    // can enable it only while animation is actually required.
    BuildSkeletalMesh->SetComponentTickEnabled(false);

    Ownership = CreateDefaultSubobject<UARPGFactionOwnershipComponent>(TEXT("Ownership"));
}

void AARPGBuildPieceActor::BeginPlay()
{
    Super::BeginPlay();
    RefreshDefinitionPresentation();
    if (UMeshComponent* ActiveMesh = GetActiveBuildMeshComponent())
    {
        BaseMeshRelativeLocation = ActiveMesh->GetRelativeLocation();
        BaseMeshRelativeScale = ActiveMesh->GetRelativeScale3D();
    }
    else
    {
        BaseMeshRelativeLocation = FVector::ZeroVector;
        BaseMeshRelativeScale = FVector::OneVector;
    }
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
        // Construction no longer owns Tick once complete. Do not disable Tick here: specialised
        // derived build actors (notably doors) may deliberately enable it for their own short-lived
        // runtime animation. RefreshConstructionPresentation() already disables the construction Tick
        // when construction completes, so leaving an explicitly re-enabled derived Tick alone is safe.
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
    if (!Definition) return;

    // A valid Skeletal Mesh is the explicit opt-in path and takes precedence over the legacy Static
    // Mesh field. This lets a duplicated Static-Mesh definition be converted by assigning the skeletal
    // asset without changing any existing Static-Mesh asset or reflected property name.
    USkeletalMesh* LoadedSkeletalMesh = Definition->BuildSkeletalMesh.IsNull() ? nullptr : Definition->BuildSkeletalMesh.LoadSynchronous();
    UStaticMesh* LoadedStaticMesh = nullptr;
    if (!LoadedSkeletalMesh && !Definition->BuildMesh.IsNull())
        LoadedStaticMesh = Definition->BuildMesh.LoadSynchronous();
    const bool bUseSkeletalMesh = LoadedSkeletalMesh != nullptr;

    if (BuildSkeletalMesh)
    {
        BuildSkeletalMesh->SetSkeletalMesh(LoadedSkeletalMesh, true);
        BuildSkeletalMesh->SetRelativeTransform(Definition->MeshRelativeTransform);
        BuildSkeletalMesh->SetVisibility(bUseSkeletalMesh, true);
        BuildSkeletalMesh->SetHiddenInGame(!bUseSkeletalMesh, true);
        BuildSkeletalMesh->SetCollisionEnabled(bUseSkeletalMesh ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
    }

    if (BuildMesh)
    {
        BuildMesh->SetStaticMesh(bUseSkeletalMesh ? nullptr : LoadedStaticMesh);
        BuildMesh->SetRelativeTransform(Definition->MeshRelativeTransform);
        BuildMesh->SetVisibility(!bUseSkeletalMesh && LoadedStaticMesh != nullptr, true);
        BuildMesh->SetHiddenInGame(bUseSkeletalMesh || LoadedStaticMesh == nullptr, true);
        BuildMesh->SetCollisionEnabled(!bUseSkeletalMesh && LoadedStaticMesh != nullptr ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
    }

    // Keep the gameplay actor on the framework's stable logical axes while letting content creators
    // adapt arbitrary imported Static/Skeletal mesh orientation/pivot/scale entirely from the Build
    // Piece Data Asset. The same transform drives bounds, preview and final presentation.
    if (UMeshComponent* ActiveMesh = GetActiveBuildMeshComponent())
    {
        BaseMeshRelativeLocation = ActiveMesh->GetRelativeLocation();
        BaseMeshRelativeScale = ActiveMesh->GetRelativeScale3D();
    }
    else
    {
        BaseMeshRelativeLocation = FVector::ZeroVector;
        BaseMeshRelativeScale = FVector::OneVector;
    }
}

UMeshComponent* AARPGBuildPieceActor::GetActiveBuildMeshComponent() const
{
    if (BuildSkeletalMesh && BuildSkeletalMesh->GetSkeletalMeshAsset()) return BuildSkeletalMesh;
    if (BuildMesh && BuildMesh->GetStaticMesh()) return BuildMesh;
    return nullptr;
}

bool AARPGBuildPieceActor::GetActiveBuildVisualRawBounds(FVector& OutMin, FVector& OutMax) const
{
    if (BuildSkeletalMesh)
    {
        if (USkeletalMesh* Mesh = BuildSkeletalMesh->GetSkeletalMeshAsset())
        {
            const FBoxSphereBounds Bounds = Mesh->GetBounds();
            OutMin = Bounds.Origin - Bounds.BoxExtent;
            OutMax = Bounds.Origin + Bounds.BoxExtent;
            return true;
        }
    }
    if (BuildMesh)
    {
        if (UStaticMesh* Mesh = BuildMesh->GetStaticMesh())
        {
            const FBoxSphereBounds Bounds = Mesh->GetBounds();
            OutMin = Bounds.Origin - Bounds.BoxExtent;
            OutMax = Bounds.Origin + Bounds.BoxExtent;
            return true;
        }
    }
    OutMin = FVector::ZeroVector;
    OutMax = FVector::ZeroVector;
    return false;
}

bool AARPGBuildPieceActor::GetActiveBuildVisualLocalBounds(FVector& OutMin, FVector& OutMax) const
{
    FVector RawMin;
    FVector RawMax;
    if (!GetActiveBuildVisualRawBounds(RawMin, RawMax))
    {
        OutMin = FVector::ZeroVector;
        OutMax = FVector::ZeroVector;
        return false;
    }

    const UMeshComponent* ActiveMesh = GetActiveBuildMeshComponent();
    if (!ActiveMesh) return false;
    const FBox ActorLocalBox = FBox(RawMin, RawMax).TransformBy(ActiveMesh->GetRelativeTransform());
    if (!ActorLocalBox.IsValid) return false;
    OutMin = ActorLocalBox.Min;
    OutMax = ActorLocalBox.Max;
    return true;
}

void AARPGBuildPieceActor::RefreshConstructionPresentation(bool bForce)
{
    UMeshComponent* ActiveMesh = GetActiveBuildMeshComponent();
    if (!ActiveMesh) return;
    const float P = GetConstructionProgress01();
    if (!bForce && FMath::IsNearlyEqual(P, LastConstructionVisualProgress, 0.002f)) return;
    LastConstructionVisualProgress = P;

    if (bConstructionComplete || !Definition)
    {
        ActiveMesh->SetRelativeScale3D(BaseMeshRelativeScale);
        ActiveMesh->SetRelativeLocation(BaseMeshRelativeLocation);
        ActiveMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        ActiveMesh->SetScalarParameterValueOnMaterials(TEXT("ConstructionProgress"), 1.f);
        ActiveMesh->SetScalarParameterValueOnMaterials(TEXT("BuildProgress"), 1.f);
        SetActorTickEnabled(false);
        OnConstructionProgressChanged.Broadcast(1.f);
        return;
    }

    const float MinZ = FMath::Clamp(Definition->ConstructionStartScaleZ, 0.01f, 1.f);
    const float Reveal = FMath::Lerp(MinZ, 1.f, FMath::SmoothStep(0.f, 1.f, P));
    FVector Scale = BaseMeshRelativeScale;
    Scale.Z *= Reveal;
    ActiveMesh->SetRelativeScale3D(Scale);

    FVector LocalMin;
    FVector LocalMax;
    GetActiveBuildVisualRawBounds(LocalMin, LocalMax);
    const float HalfHeight = FMath::Max(0.f, (LocalMax.Z - LocalMin.Z) * 0.5f * BaseMeshRelativeScale.Z);
    FVector Location = BaseMeshRelativeLocation;
    Location.Z -= HalfHeight * (1.f - Reveal);
    ActiveMesh->SetRelativeLocation(Location);
    ActiveMesh->SetCollisionEnabled(Definition->bCollisionDuringConstruction ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
    ActiveMesh->SetScalarParameterValueOnMaterials(Definition->ConstructionProgressMaterialParameter, P);
    ActiveMesh->SetScalarParameterValueOnMaterials(TEXT("BuildProgress"), P);
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
    // Canonical vertical-story placement must never derive the next storey from rendered mesh
    // height. Wall art can be shorter/taller than StandardWallHeight, and using TargetMax.Z here
    // makes that art error accumulate every time the player adds another storey. For Wall-family
    // targets, the authoritative next-story plane is always: visible/structural wall bottom + the
    // authored StandardWallHeight. Non-Wall targets can continue using their actual top surface.
    const float IncomingOnNextWallStoryPlaneZ = TargetMin.Z + FMath::Max(1.f, Definition->StandardWallHeight) - IncomingMin.Z;
    const float IncomingTopOnNextWallStoryPlaneZ = TargetMin.Z + FMath::Max(1.f, Definition->StandardWallHeight) - IncomingMax.Z;

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

            // Wall-family pieces use the finished horizontal walking surface as the canonical story
            // seam. The horizontal slab extends DOWNWARD from that plane, so its thickness never adds
            // vertical height to the next storey. Foundations already expose their finished top surface;
            // Floor/Ceiling/Roof pieces now use that same top-plane convention on every upper storey.
            // This makes Foundation->Wall, Floor->Wall and Wall-stack->Wall all resolve to one immutable
            // story lattice while allowing the slab to overlap the upper portion of the wall below.
            //
            // Logical wall convention: actor local X is the wall run and actor local +Y is the
            // authored front/exterior side after MeshRelativeTransform. Each support-edge yaw must
            // therefore rotate +Y toward that edge's outward normal. In UE yaw space +90 rotates
            // local +Y toward -X, while -90 rotates local +Y toward +X. Keep these signs paired
            // with their geometric edge; swapping them turns the two X-edge walls inside-out.
            if (ARPGIsWallLike(IncomingKind))
            {
                const float IncomingWallStoryBaseZ = IncomingOnTargetTopZ;

                AddLocal(FTransform(FRotator(0.f,   0.f, 0.f), FVector(0.f,  Half, IncomingWallStoryBaseZ)));
                AddLocal(FTransform(FRotator(0.f, 180.f, 0.f), FVector(0.f, -Half, IncomingWallStoryBaseZ)));
                AddLocal(FTransform(FRotator(0.f, -90.f, 0.f), FVector(Half, 0.f, IncomingWallStoryBaseZ)));
                AddLocal(FTransform(FRotator(0.f,  90.f, 0.f), FVector(-Half, 0.f, IncomingWallStoryBaseZ)));
            }

            if (IncomingKind == EARPGBuildPieceKind::Stair && TargetKind != EARPGBuildPieceKind::Roof)
            {
                // Horizontal modules expose a paired Stair landing contract on every edge:
                //
                //   HIGH-END arrival  -> the Stair descends outward/down from this edge.
                //   LOW-END departure -> the Stair rises inward/up from this same edge.
                //
                // The two sockets deliberately share the same edge center and yaw. This gives a
                // Foundation/Floor/Ceiling one canonical stair centerline that can connect a lower
                // flight arriving at the edge to an upper flight leaving that edge without lateral
                // drift. It also makes the build order independent: either flight may be placed first.
                //
                // Logical Stair convention after MeshRelativeTransform:
                //   local +X = uphill
                //   local  Y = stair width
                //   local +Z = up
                // Imported stairs that rise toward -X can be adapted once in MeshRelativeTransform with
                // a 180-degree yaw; no custom sockets or PlacementOffset are required.
                const FVector IncomingCenter = (IncomingMin + IncomingMax) * 0.5f;

                // Structural Stair endpoints belong to the authored SnapSize grid, not the raw art
                // bounds. The current Wood Stair is 334 cm long on a 300 cm grid, so its visual mesh
                // intentionally overhangs the structural flight by 17 cm at BOTH ends. Using the raw
                // +/-167 cm mesh endpoints as snap anchors shifts the first flight by 17 cm and then
                // advances Stair chains/landings by 334 cm, producing a cumulative 34 cm off-grid drift
                // against 300 cm Floors/Walls. Keep the art untouched and inset the structural anchors
                // to +/-150 cm around the transformed bounds centre instead.
                const float IncomingStairHalfRun = IncomingGrid * 0.5f;
                const FVector StairHighStructuralXYLocal(IncomingCenter.X + IncomingStairHalfRun, IncomingCenter.Y, 0.f);
                const FVector StairLowStructuralXYLocal(IncomingCenter.X - IncomingStairHalfRun, IncomingCenter.Y, 0.f);

                // Stairs are visual traversal art inside the structural story grid; their raw mesh rise
                // must never redefine the building's storey height. The canonical story plane is the
                // FINISHED TOP / walking surface of every flat structural module. Floor thickness extends
                // downward below that plane instead of being stacked on top of the storey. Therefore a
                // 278 cm Stair on a 300 cm story has a natural 22 cm residual first-step rise and its HIGH
                // end meets the next Floor walking surface exactly, with the Stair/stringers penetrating
                // the 18 cm slab as normal modular art rather than leaving an 18 cm landing gap.
                const float TargetStoryPlaneZ = TargetMax.Z;
                const float StairHighArrivalAlignedZ = TargetStoryPlaneZ - IncomingMax.Z;
                // LOW-departure/up-flight stairs start on the CURRENT finished walking surface.
                // Do not align their HIGH art end to the next 300 cm story plane: with a 278 cm Stair
                // that lifts the whole flight by 22 cm (300-278), so every new flight visibly sits on
                // top of its Floor and the landing error repeats on every storey. Align the visible LOW
                // end to the current Floor/Foundation/Ceiling top instead. The 278 cm art then rises to
                // Z+278 while the next 18 cm Floor slab occupies Z+282..Z+300, leaving only the intended
                // 4 cm underside tolerance and a final 22 cm step to the finished upper walking surface.
                const float StairLowDepartureAlignedZ = TargetStoryPlaneZ - IncomingMin.Z;

                struct FStairEdgeSocket
                {
                    FVector EdgeCenter;
                    float Yaw;
                };

                // +X/uphill points inward from the selected edge. The HIGH-arrival candidate therefore
                // occupies the neighbouring outside/down cell, while the LOW-departure candidate rises
                // across the host cell. Both stay on the exact same structural centerline.
                const FStairEdgeSocket StairEdges[] =
                {
                    { FVector(0.f,  Half, 0.f), -90.f }, // +Y edge, uphill points -Y
                    { FVector(0.f, -Half, 0.f),  90.f }, // -Y edge, uphill points +Y
                    { FVector( Half, 0.f, 0.f), 180.f }, // +X edge, uphill points -X
                    { FVector(-Half, 0.f, 0.f),   0.f }  // -X edge, uphill points +X
                };

                for (const FStairEdgeSocket& Socket : StairEdges)
                {
                    const FRotator StairRotation(0.f, Socket.Yaw, 0.f);

                    // Existing/ground-access topology: HIGH end lands on the edge and the Stair runs
                    // outward/down. This is the proven v2.15.31 Landscape-compatible socket.
                    const FVector RotatedHighEnd = StairRotation.RotateVector(StairHighStructuralXYLocal);
                    FVector HighArrivalTranslation = Socket.EdgeCenter - FVector(RotatedHighEnd.X, RotatedHighEnd.Y, 0.f);
                    HighArrivalTranslation.Z = StairHighArrivalAlignedZ;
                    AddLocal(FTransform(StairRotation, HighArrivalTranslation));

                    // Multi-storey topology: LOW end starts on the exact same edge/centerline and the
                    // Stair rises inward/up. A lower Stair arriving at this Foundation/Floor edge and
                    // an upper Stair departing it therefore meet without XY or yaw drift.
                    const FVector RotatedLowEnd = StairRotation.RotateVector(StairLowStructuralXYLocal);
                    FVector LowDepartureTranslation = Socket.EdgeCenter - FVector(RotatedLowEnd.X, RotatedLowEnd.Y, 0.f);
                    LowDepartureTranslation.Z = StairLowDepartureAlignedZ;
                    AddLocal(FTransform(StairRotation, LowDepartureTranslation));
                }
            }

            // On a foundation, the next floor/ceiling/roof finished TOP surface lives one wall story
            // above the foundation's finished top surface. The slab thickness extends downward from
            // that story plane, so it never adds another thickness increment to upper-storey height.
            if (IncomingKind == EARPGBuildPieceKind::Ceiling || IncomingKind == EARPGBuildPieceKind::Floor || IncomingKind == EARPGBuildPieceKind::Roof)
            {
                if (TargetKind == EARPGBuildPieceKind::Foundation)
                {
                    const float StorySurfaceZ = TargetMax.Z + WallHeight - IncomingMax.Z;
                    AddLocal(FTransform(FRotator::ZeroRotator, FVector(0.f, 0.f, StorySurfaceZ)));
                }
            }
        }

        if (TargetKind == EARPGBuildPieceKind::Stair && IncomingKind == EARPGBuildPieceKind::Stair)
        {
            // Stair chains advance on the STRUCTURAL grid in XY while preserving pivot-aware Z. Raw
            // mesh endpoints are art only: the current 334 cm Stair overhangs its 300 cm structural
            // cell by 17 cm at each end. Align structural +/-SnapSize/2 anchors around each transformed
            // bounds centre so different Stair pivots can still share one exact grid landing.
            const FVector TargetCenter = (TargetMin + TargetMax) * 0.5f;
            const FVector IncomingCenter = (IncomingMin + IncomingMax) * 0.5f;
            const float TargetGrid = FMath::Max(1.f, Definition->SnapSize);
            const float ChainStep = 0.5f * (TargetGrid + IncomingGrid);
            const float CenterDeltaX = TargetCenter.X - IncomingCenter.X;

            const float ContinueUpZ = TargetMax.Z + WallHeight - IncomingMax.Z;
            const float ContinueDownZ = TargetMax.Z - WallHeight - IncomingMax.Z;
            AddLocal(FTransform(FRotator::ZeroRotator, FVector(CenterDeltaX + ChainStep, 0.f, ContinueUpZ)));
            AddLocal(FTransform(FRotator::ZeroRotator, FVector(CenterDeltaX - ChainStep, 0.f, ContinueDownZ)));
        }

        if (TargetKind == EARPGBuildPieceKind::Stair &&
            (IncomingKind == EARPGBuildPieceKind::Floor || IncomingKind == EARPGBuildPieceKind::Ceiling))
        {
            // A completed Stair exposes flat landing CELLS on the same 300 cm XY lattice and canonical
            // story Z lattice as Foundation/Floor/Wall construction. Never derive landing-cell centres
            // from raw Stair mesh endpoints: a 334 cm Stair on a 300 cm grid has 17 cm visual overhang at
            // each end, and endpoint math shifts a LOW-departure flight by 17 cm then places its landing
            // another 317 cm away = 334 cm total, walking the entire upper building off-grid.
            //
            // Structural contract after MeshRelativeTransform:
            //   local +X = uphill;
            //   Stair structural HIGH/LOW anchors are +/- Target SnapSize/2 around its bounds centre;
            //   Floor/Ceiling actor centre is one incoming half-grid beyond that anchor;
            //   incoming TOP / walking surface owns the canonical story landing plane.
            const FVector TargetCenter = (TargetMin + TargetMax) * 0.5f;
            const float TargetGrid = FMath::Max(1.f, Definition->SnapSize);
            const float TargetHalfGrid = TargetGrid * 0.5f;
            const float IncomingHalfGrid = IncomingGrid * 0.5f;
            const float LandingCenterOffset = TargetHalfGrid + IncomingHalfGrid;

            // For the standard multi-storey up-flight contract, a Stair actor's visible LOW plane is
            // the canonical current story surface. Therefore Floor/Ceiling landing surfaces are derived
            // from that LOW plane + StandardWallHeight, never from the rendered Stair top. This keeps a
            // 278 cm Stair inside a 300 cm structural storey without letting the missing 22 cm become an
            // actor offset or accumulate into later floors.
            const float StairLowStoryPlaneZ = TargetMin.Z;
            const FVector HighLandingTranslation(
                TargetCenter.X + LandingCenterOffset,
                TargetCenter.Y,
                StairLowStoryPlaneZ + WallHeight - IncomingMax.Z);
            const FVector LowLandingTranslation(
                TargetCenter.X - LandingCenterOffset,
                TargetCenter.Y,
                StairLowStoryPlaneZ - IncomingMax.Z);

            AddLocal(FTransform(FRotator::ZeroRotator, HighLandingTranslation));
            AddLocal(FTransform(FRotator::ZeroRotator, LowLandingTranslation));
        }

        if (ARPGIsWallLike(TargetKind))
        {
            if (ARPGIsWallLike(IncomingKind))
            {
                // Side continuation aligns wall bottoms. The vertical stack candidate uses zero local
                // rotation and XY offset, so composition with the target actor transform explicitly
                // inherits the supporting wall's world facing as well as its structural column.
                AddLocal(FTransform(FRotator::ZeroRotator, FVector(Grid, 0.f, AlignBottomPlaneZ)));
                AddLocal(FTransform(FRotator::ZeroRotator, FVector(-Grid, 0.f, AlignBottomPlaneZ)));
                // Vertical Wall-family stacking is a structural story step, not a mesh-top step.
                // Align the incoming wall bottom to target wall bottom + StandardWallHeight so the
                // same 300 cm lattice is preserved on every storey regardless of wall art height.
                AddLocal(FTransform(FRotator::ZeroRotator, FVector(0.f, 0.f, IncomingOnNextWallStoryPlaneZ)));

                // Perpendicular corner continuation. A modular wall placed on one foundation edge must be
                // allowed to meet a wall on the adjacent edge without requiring the art meshes to stop
                // exactly at the logical grid centerline. These transforms describe the four legitimate
                // L-corner neighbours using the target half-grid along X and incoming half-grid along Y.
                // They also let wall-only construction turn a 90-degree corner without a foundation below.
                const float TargetHalfGrid = Grid * 0.5f;
                const float IncomingHalfGrid = IncomingGrid * 0.5f;
                const FVector CornerOffsets[] =
                {
                    FVector( TargetHalfGrid,  IncomingHalfGrid, AlignBottomPlaneZ),
                    FVector( TargetHalfGrid, -IncomingHalfGrid, AlignBottomPlaneZ),
                    FVector(-TargetHalfGrid,  IncomingHalfGrid, AlignBottomPlaneZ),
                    FVector(-TargetHalfGrid, -IncomingHalfGrid, AlignBottomPlaneZ)
                };
                for (const FVector& CornerOffset : CornerOffsets)
                {
                    // Both +/-90 facings are legitimate at the same geometric L-corner. Keeping both
                    // preserves authored front/back orientation and matches foundation-edge yaw choices.
                    AddLocal(FTransform(FRotator(0.f,  90.f, 0.f), CornerOffset));
                    AddLocal(FTransform(FRotator(0.f, -90.f, 0.f), CornerOffset));
                }
            }

            // Horizontal structural pieces attach by their finished TOP/walking surface to the wall's
            // canonical structural story top instead of stacking slab thickness above the storey.
            if (IncomingKind == EARPGBuildPieceKind::Ceiling || IncomingKind == EARPGBuildPieceKind::Floor || IncomingKind == EARPGBuildPieceKind::Roof)
            {
                // Floors/Ceilings/Roofs supported by a Wall-family piece own the same canonical
                // next-story plane as a vertically stacked wall. Never align them to TargetMax.Z: a
                // decorative wall mesh that is even a few centimetres off would otherwise shift the
                // Floor, and that shifted Floor would become the baseline for every storey above it.
                AddLocal(FTransform(FRotator::ZeroRotator, FVector(0.f,  Half, IncomingTopOnNextWallStoryPlaneZ)));
                AddLocal(FTransform(FRotator::ZeroRotator, FVector(0.f, -Half, IncomingTopOnNextWallStoryPlaneZ)));
            }

            if (TargetKind == EARPGBuildPieceKind::Doorway && IncomingKind == EARPGBuildPieceKind::Door)
            {
                const FVector InsertTranslation = ARPGGetBottomAlignedInsertTranslation(TargetMin, TargetMax, IncomingMin, IncomingMax);
                AddLocal(FTransform(FRotator::ZeroRotator, InsertTranslation));
            }
            if (TargetKind == EARPGBuildPieceKind::WindowWall && IncomingKind == EARPGBuildPieceKind::Window)
            {
                const FVector InsertTranslation = ARPGGetCenteredInsertTranslation(
                    TargetMin, TargetMax, IncomingMin, IncomingMax, Definition->WindowInsertOffset);
                AddLocal(FTransform(FRotator::ZeroRotator, InsertTranslation));
            }
        }

        if (TargetKind == EARPGBuildPieceKind::Pillar && ARPGIsWallLike(IncomingKind))
        {
            const float PillarTopZ = TargetMax.Z - IncomingMin.Z;
            AddLocal(FTransform(FRotator::ZeroRotator, FVector(Half, 0.f, PillarTopZ)));
            AddLocal(FTransform(FRotator(0.f, 90.f, 0.f), FVector(0.f, Half, PillarTopZ)));
        }
    }

    // WindowWall -> Window is an intrinsic hosted-insert contract, not an optional generic structural
    // edge. A WindowWall must always advertise its one semantic Window socket even when a project has
    // disabled Generate Standard Snap Points to author the wall's structural neighbours manually.
    // Keep the existing generated path untouched when standard points are enabled to avoid duplicate
    // candidates and preserve every confirmed Wall/Doorway/Foundation behaviour.
    if (!Definition->bGenerateStandardSnapPoints &&
        Definition->PieceKind == EARPGBuildPieceKind::WindowWall &&
        IncomingPiece->PieceKind == EARPGBuildPieceKind::Window)
    {
        const FVector InsertTranslation = ARPGGetCenteredInsertTranslation(
            TargetMin, TargetMax, IncomingMin, IncomingMax, Definition->WindowInsertOffset);
        AddLocal(FTransform(FRotator::ZeroRotator, InsertTranslation));
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
