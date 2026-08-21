#include "Building/ARPGBuildingComponent.h"
#include "Building/ARPGBuildDoorActor.h"
#include "Building/ARPGBuildLightActor.h"
#include "Building/ARPGBuildPieceActor.h"
#include "Building/ARPGBuildPreviewActor.h"
#include "Building/ARPGBuildWindowActor.h"
#include "Building/ARPGFactionTerritoryVolume.h"
#include "Components/ARPGEventRouterComponent.h"
#include "Components/ARPGFactionComponent.h"
#include "Components/ARPGInventoryComponent.h"
#include "Crafting/ARPGCraftingStationActor.h"
#include "Crafting/ARPGStorageActor.h"
#include "Data/ARPGBuildPieceDefinition.h"
#include "Data/ARPGCraftingStationDefinition.h"
#include "Data/ARPGItemDefinition.h"
#include "Data/ARPGSettlementDefinition.h"
#include "Settlement/ARPGBuildBedActor.h"
#include "Settlement/ARPGSettlementHubActor.h"
#include "Engine/OverlapResult.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Gathering/ARPGTree.h"
#include "Materials/MaterialInterface.h"
#include "LandscapeProxy.h"

static FName ARPGResolveBuildCostId(const FARPGItemAmount& Amount)
{
    if (Amount.Item) return Amount.Item->DefinitionId.IsNone() ? Amount.Item->GetFName() : Amount.Item->DefinitionId;
    return Amount.ItemId;
}

static void ARPGAggregateBuildCosts(const UARPGBuildPieceDefinition* Piece, TMap<FName, int32>& OutCosts)
{
    OutCosts.Reset();
    if (!Piece) return;
    for (const FARPGItemAmount& Cost : Piece->BuildCost)
    {
        const FName Id = ARPGResolveBuildCostId(Cost);
        if (!Id.IsNone() && Cost.Quantity > 0) OutCosts.FindOrAdd(Id) += Cost.Quantity;
    }
}

static bool ARPGIsWallLikeKind(EARPGBuildPieceKind Kind)
{
    return Kind == EARPGBuildPieceKind::Wall || Kind == EARPGBuildPieceKind::WindowWall || Kind == EARPGBuildPieceKind::Doorway;
}

static bool ARPGIsHorizontalStructuralKind(EARPGBuildPieceKind Kind)
{
    return Kind == EARPGBuildPieceKind::Foundation || Kind == EARPGBuildPieceKind::Floor ||
           Kind == EARPGBuildPieceKind::Ceiling || Kind == EARPGBuildPieceKind::Roof;
}

static bool ARPGIsUpperHorizontalStructuralKind(EARPGBuildPieceKind Kind)
{
    return Kind == EARPGBuildPieceKind::Floor || Kind == EARPGBuildPieceKind::Ceiling ||
           Kind == EARPGBuildPieceKind::Roof;
}

static bool ARPGIsInsertSnapPair(EARPGBuildPieceKind TargetKind, EARPGBuildPieceKind IncomingKind)
{
    return (TargetKind == EARPGBuildPieceKind::Doorway && IncomingKind == EARPGBuildPieceKind::Door) ||
           (TargetKind == EARPGBuildPieceKind::WindowWall && IncomingKind == EARPGBuildPieceKind::Window);
}

static bool ARPGIsInsertPieceKind(EARPGBuildPieceKind Kind)
{
    return Kind == EARPGBuildPieceKind::Door || Kind == EARPGBuildPieceKind::Window;
}

/**
 * Landscape is continuous terrain, not a discrete placement obstacle. Its collision components can
 * conservatively overlap an oriented Stair occupancy slice even when the rendered/physical height
 * field is only beneath the flight. For a Stair that has already been proven to match an authoritative
 * Foundation/Floor/Ceiling socket, Landscape contact is therefore terrain embedding/support and must
 * not independently veto the snap. This exception is deliberately ALandscapeProxy-only: static-mesh
 * rocks, cliffs, props, buildings and every other WorldStatic actor continue through normal blocker
 * validation.
 */
static bool ARPGIsLandscapeTerrainActor(const AActor* Actor)
{
    return Actor && Actor->IsA<ALandscapeProxy>();
}

/**
 * Foundations are allowed to replace gatherable vegetation space. A blocking ARPGTree must not steal
 * the placement ray from the terrain/foundation socket behind it, otherwise the preview either floats
 * on the trunk or never reaches the intended snap target. Retry the exact same authoritative trace
 * while ignoring only encountered ARPGTree actors. Every non-tree blocker keeps normal collision.
 */
static bool ARPGTracePlacementSurfaceIgnoringFoundationTrees(
    UWorld* World,
    const UARPGBuildPieceDefinition* Piece,
    const FVector& Start,
    const FVector& End,
    ECollisionChannel TraceChannel,
    const FCollisionQueryParams& BaseParams,
    FHitResult& OutHit)
{
    if (!World || !Piece) return false;
    if (Piece->PieceKind != EARPGBuildPieceKind::Foundation)
        return World->LineTraceSingleByChannel(OutHit, Start, End, TraceChannel, BaseParams);

    FCollisionQueryParams Params = BaseParams;
    constexpr int32 MaxTreePierceCount = 32;
    for (int32 Attempt = 0; Attempt < MaxTreePierceCount; ++Attempt)
    {
        FHitResult Hit;
        if (!World->LineTraceSingleByChannel(Hit, Start, End, TraceChannel, Params)) return false;
        if (AARPGTree* Tree = Cast<AARPGTree>(Hit.GetActor()))
        {
            Params.AddIgnoredActor(Tree);
            continue;
        }
        OutHit = Hit;
        return true;
    }
    return false;
}

/**
 * Flat Stair landing hosts are walkable horizontal structural modules. Roofs remain excluded because
 * their pitch/shape is kit-specific and the standard Stair endpoint contract assumes a flat landing.
 */
static bool ARPGIsStairSupportSnapPair(EARPGBuildPieceKind TargetKind, EARPGBuildPieceKind IncomingKind)
{
    return IncomingKind == EARPGBuildPieceKind::Stair &&
           (TargetKind == EARPGBuildPieceKind::Foundation ||
            TargetKind == EARPGBuildPieceKind::Floor ||
            TargetKind == EARPGBuildPieceKind::Ceiling);
}

/**
 * Stair-to-Stair chaining uses the same endpoint convention as horizontal landing sockets: local +X
 * is uphill, so LOW(incoming)->HIGH(target) continues upward and HIGH(incoming)->LOW(target) continues
 * downward on one exact centerline. Keep this separate from flat-support semantics because side-wall
 * stairwell classification still needs to know whether the active host is a horizontal cell.
 */
static bool ARPGIsStairChainSnapPair(EARPGBuildPieceKind TargetKind, EARPGBuildPieceKind IncomingKind)
{
    return TargetKind == EARPGBuildPieceKind::Stair && IncomingKind == EARPGBuildPieceKind::Stair;
}

static bool ARPGIsAnyStairSnapPair(EARPGBuildPieceKind TargetKind, EARPGBuildPieceKind IncomingKind)
{
    return ARPGIsStairSupportSnapPair(TargetKind, IncomingKind) ||
           ARPGIsStairChainSnapPair(TargetKind, IncomingKind);
}

/**
 * Collision-independent segment/AABB test used only for semantic insert targeting. Doorway and
 * WindowWall art often has an actual hole in its collision, and imported kits may use complex-only
 * collision that is unsuitable for overlap discovery. The overall visible bounds still describe the
 * opening envelope the player is aiming through, so testing the camera ray against that envelope
 * makes insert acquisition reliable without changing normal structural snap collision behaviour.
 */
static bool ARPGSegmentIntersectsLocalBox(
    const FVector& SegmentStart,
    const FVector& SegmentEnd,
    const FVector& BoxMin,
    const FVector& BoxMax,
    float& OutEnterT)
{
    const FVector Delta = SegmentEnd - SegmentStart;
    const float Starts[3] = { SegmentStart.X, SegmentStart.Y, SegmentStart.Z };
    const float Deltas[3] = { Delta.X, Delta.Y, Delta.Z };
    const float Mins[3] = { BoxMin.X, BoxMin.Y, BoxMin.Z };
    const float Maxs[3] = { BoxMax.X, BoxMax.Y, BoxMax.Z };

    float EnterT = 0.f;
    float ExitT = 1.f;
    for (int32 Axis = 0; Axis < 3; ++Axis)
    {
        if (FMath::Abs(Deltas[Axis]) <= KINDA_SMALL_NUMBER)
        {
            if (Starts[Axis] < Mins[Axis] || Starts[Axis] > Maxs[Axis]) return false;
            continue;
        }

        float T0 = (Mins[Axis] - Starts[Axis]) / Deltas[Axis];
        float T1 = (Maxs[Axis] - Starts[Axis]) / Deltas[Axis];
        if (T0 > T1) Swap(T0, T1);
        EnterT = FMath::Max(EnterT, T0);
        ExitT = FMath::Min(ExitT, T1);
        if (EnterT > ExitT) return false;
    }

    OutEnterT = FMath::Clamp(EnterT, 0.f, 1.f);
    return true;
}

/**
 * Third-person cameras are intentionally offset from the controlled pawn and may not put the exact
 * camera centre ray through a WindowWall's OBB even while the opening clearly occupies the player's
 * aim region. This bounded corridor is only a semantic acquisition fallback for Door/Window inserts.
 * It never changes the final snap transform and never affects ordinary structural placement.
 */
static bool ARPGSegmentPassesInsertAimCorridor(
    const FVector& SegmentStart,
    const FVector& SegmentEnd,
    const FTransform& TargetTransform,
    const FVector& TargetLocalCenter,
    const FVector& TargetLocalExtent,
    float ExtraPadding,
    float& OutEnterT,
    float& OutCenterlineErrorSq)
{
    const FVector Segment = SegmentEnd - SegmentStart;
    const float SegmentLength = Segment.Size();
    if (SegmentLength <= KINDA_SMALL_NUMBER) return false;

    const FVector Direction = Segment / SegmentLength;
    const FVector TargetCenterWorld = TargetTransform.TransformPosition(TargetLocalCenter);
    const float Along = FVector::DotProduct(TargetCenterWorld - SegmentStart, Direction);
    if (Along < 0.f || Along > SegmentLength) return false;

    const FVector ClosestPoint = SegmentStart + Direction * Along;
    OutCenterlineErrorSq = FVector::DistSquared(TargetCenterWorld, ClosestPoint);

    const FVector ScaleAbs = TargetTransform.GetScale3D().GetAbs();
    const FVector WorldExtent(
        TargetLocalExtent.X * ScaleAbs.X,
        TargetLocalExtent.Y * ScaleAbs.Y,
        TargetLocalExtent.Z * ScaleAbs.Z);

    // Use the host's largest half-extent rather than its diagonal sphere. This allows aiming anywhere
    // over the visible WindowWall module while avoiding an excessively broad neighbouring-wall cone.
    const float CorridorRadius = FMath::Max(1.f, WorldExtent.GetMax() + FMath::Max(0.f, ExtraPadding));
    if (OutCenterlineErrorSq > FMath::Square(CorridorRadius)) return false;

    OutEnterT = FMath::Clamp(Along / SegmentLength, 0.f, 1.f);
    return true;
}

/**
 * Multiple completed structures can advertise the same geometric snap slot. Wall placement needs
 * deterministic ownership that is independent of overlap iteration order and camera yaw:
 *
 *  1) A horizontal Foundation/Floor/Ceiling/Roof edge owns canonical exterior facing whenever it
 *     exists for that wall column. All flat structural pieces generate Wall candidates from their
 *     finished TOP/walking-surface story plane, while slab thickness extends downward and never adds
 *     vertical height to the next storey.
 *  2) The wall-family piece directly below is the second owner and preserves vertical-stack facing
 *     when no horizontal support edge owns the slot.
 *  3) Lateral/corner walls are the final owner for wall-only continuation/corners.
 *
 * Priority is candidate-aware rather than merely target-kind-aware. It is consulted only for
 * same-physical-slot ties; ordinary distance/yaw scoring remains unchanged for different slots.
 */
static int32 ARPGGetSnapCandidateSemanticPriority(
    const AARPGBuildPieceActor* Target,
    const UARPGBuildPieceDefinition* IncomingPiece,
    const FTransform& Candidate)
{
    if (!Target || !Target->Definition || !IncomingPiece) return 0;
    if (!ARPGIsWallLikeKind(IncomingPiece->PieceKind)) return 0;

    const EARPGBuildPieceKind TargetKind = Target->Definition->PieceKind;
    if (ARPGIsHorizontalStructuralKind(TargetKind))
    {
        // Horizontal supports own wall-facing whenever the same physical wall slot exists on a
        // Foundation/Floor/Ceiling edge. This is true on every story, not only the first: the edge
        // normal is the canonical exterior direction and must beat a coincident vertical-stack
        // candidate inherited from the wall below.
        return 0;
    }

    if (ARPGIsWallLikeKind(TargetKind))
    {
        // Standard vertical Wall-family -> Wall-family stacking is generated at local XY = 0,
        // above the target, with zero relative yaw. Recognize that structural relationship from the
        // authoritative candidate transform instead of relying on overlap iteration order or view yaw.
        const FVector TargetLocalLocation = Target->GetActorTransform().InverseTransformPosition(Candidate.GetLocation());
        const float StackColumnTolerance = FMath::Max(1.f, IncomingPiece->PlacementCollisionClearance + 0.5f);
        const float StackColumnToleranceSq = FMath::Square(StackColumnTolerance);
        const float HorizontalOffsetSq = FMath::Square(TargetLocalLocation.X) + FMath::Square(TargetLocalLocation.Y);
        const float RelativeYawDelta = FMath::Abs(FMath::FindDeltaAngleDegrees(Target->GetActorRotation().Yaw, Candidate.Rotator().Yaw));
        constexpr float StackFacingToleranceDegrees = 1.f;

        const bool bVerticalStackCandidate =
            HorizontalOffsetSq <= StackColumnToleranceSq &&
            TargetLocalLocation.Z > StackColumnTolerance &&
            RelativeYawDelta <= StackFacingToleranceDegrees;

        if (bVerticalStackCandidate)
        {
            // A direct wall below remains the second-best semantic owner. It still beats lateral
            // wall/corner candidates when no horizontal support exists, preserving stable wall-only
            // vertical stacking, but it must not override the outward normal authored by an actual
            // Floor/Foundation edge at the same world slot.
            return 1;
        }

        // Side continuation and +/-90 corner turns remain fully available when they are not
        // competing with a horizontal edge or a direct vertical support for the same physical slot.
        return 2;
    }

    return 3;
}

/**
 * PlacementBounds are validation half-extents, not a promise about where a mesh pivot lives.
 * Modular building meshes are commonly authored with center, bottom-center, or corner pivots.
 * Resolve the visible Build Mesh bounds so ground placement and validation share the real pivot.
 */
static bool ARPGGetBuildPieceLocalBounds(const UARPGBuildPieceDefinition* Piece, FVector& OutMin, FVector& OutMax)
{
    if (!Piece) return false;

    // Match final presentation exactly: an explicitly assigned valid Skeletal Mesh takes precedence,
    // while every legacy Static Mesh definition continues down the original path unchanged. Using the
    // same transformed bounds here keeps pivot-aware ground placement, structural snapping, hosted
    // Window/Door insert centering and placement validation in parity for both visual types.
    if (USkeletalMesh* Mesh = Piece->BuildSkeletalMesh.LoadSynchronous())
    {
        const FBoxSphereBounds Bounds = Mesh->GetBounds();
        const FBox RawBox(Bounds.Origin - Bounds.BoxExtent, Bounds.Origin + Bounds.BoxExtent);
        const FBox ActorLocalBox = RawBox.TransformBy(Piece->MeshRelativeTransform);
        OutMin = ActorLocalBox.Min;
        OutMax = ActorLocalBox.Max;
        return true;
    }
    if (UStaticMesh* Mesh = Piece->BuildMesh.LoadSynchronous())
    {
        const FBoxSphereBounds Bounds = Mesh->GetBounds();
        const FBox RawBox(Bounds.Origin - Bounds.BoxExtent, Bounds.Origin + Bounds.BoxExtent);
        const FBox ActorLocalBox = RawBox.TransformBy(Piece->MeshRelativeTransform);
        OutMin = ActorLocalBox.Min;
        OutMax = ActorLocalBox.Max;
        return true;
    }

    // Safe fallback for definitions that intentionally use only a custom actor.
    OutMin = -Piece->PlacementBounds;
    OutMax = Piece->PlacementBounds;
    return false;
}

static FVector ARPGGetBuildPieceBoundsCenterLocal(const UARPGBuildPieceDefinition* Piece)
{
    FVector Min, Max;
    ARPGGetBuildPieceLocalBounds(Piece, Min, Max);
    return (Min + Max) * 0.5f;
}

static float ARPGDistanceSquaredToBuildPieceBounds(const AARPGBuildPieceActor* Target, const FVector& WorldPoint)
{
    if (!Target || !Target->Definition) return TNumericLimits<float>::Max();

    FVector LocalMin, LocalMax;
    ARPGGetBuildPieceLocalBounds(Target->Definition, LocalMin, LocalMax);
    const FTransform TargetTransform = Target->GetActorTransform();
    const FVector LocalPoint = TargetTransform.InverseTransformPosition(WorldPoint);
    const FVector ClosestLocal(
        FMath::Clamp(LocalPoint.X, LocalMin.X, LocalMax.X),
        FMath::Clamp(LocalPoint.Y, LocalMin.Y, LocalMax.Y),
        FMath::Clamp(LocalPoint.Z, LocalMin.Z, LocalMax.Z));
    const FVector ClosestWorld = TargetTransform.TransformPosition(ClosestLocal);
    return FVector::DistSquared(WorldPoint, ClosestWorld);
}

/**
 * Horizontal pieces are commonly acquired by aiming at the supporting wall edge, not at the
 * generated Floor/Ceiling/Roof actor origin. Measure capture against the incoming candidate's
 * transformed visible envelope so a 300 cm tile can be selected naturally from its 150 cm edge
 * without increasing the global snap radius or making unrelated structural sockets magnetic.
 */
static float ARPGDistanceSquaredToDefinitionBoundsAtTransform(
    const UARPGBuildPieceDefinition* Piece,
    const FTransform& CandidateTransform,
    const FVector& WorldPoint)
{
    if (!Piece) return TNumericLimits<float>::Max();

    FVector LocalMin, LocalMax;
    ARPGGetBuildPieceLocalBounds(Piece, LocalMin, LocalMax);
    const FVector LocalPoint = CandidateTransform.InverseTransformPosition(WorldPoint);
    const FVector ClosestLocal(
        FMath::Clamp(LocalPoint.X, LocalMin.X, LocalMax.X),
        FMath::Clamp(LocalPoint.Y, LocalMin.Y, LocalMax.Y),
        FMath::Clamp(LocalPoint.Z, LocalMin.Z, LocalMax.Z));
    const FVector ClosestWorld = CandidateTransform.TransformPosition(ClosestLocal);
    return FVector::DistSquared(WorldPoint, ClosestWorld);
}

/**
 * Multiple walls around a room can legitimately support the same horizontal tile. Because native
 * Wall -> Floor sockets inherit the supporting wall's yaw, the same physical slot can be advertised
 * as 0/90/180/270 degrees. Collision validation must compare the *placement envelope*, not demand
 * quaternion identity. 180 degrees always preserves a rectangular footprint; 90 degrees preserves
 * it only when X/Y half-extents are equal within the exposed seam tolerance.
 */
static bool ARPGRotationsPreservePlacementFootprint(
    const UARPGBuildPieceDefinition* Piece,
    const FQuat& A,
    const FQuat& B)
{
    if (!Piece) return false;

    const float DeltaYaw = FMath::Abs(FMath::FindDeltaAngleDegrees(A.Rotator().Yaw, B.Rotator().Yaw));
    constexpr float RotationToleranceDegrees = 1.f;
    if (DeltaYaw <= RotationToleranceDegrees || FMath::Abs(DeltaYaw - 180.f) <= RotationToleranceDegrees)
        return true;

    if (FMath::Abs(DeltaYaw - 90.f) > RotationToleranceDegrees)
        return false;

    const float FootprintTolerance = FMath::Max(1.f, Piece->PlacementCollisionClearance + 0.5f);
    return FMath::Abs(Piece->PlacementBounds.X - Piece->PlacementBounds.Y) <= FootprintTolerance;
}

static void ARPGGetDefinitionWorldZRange(
    const UARPGBuildPieceDefinition* Piece,
    const FTransform& Transform,
    float& OutMinZ,
    float& OutMaxZ);

static bool ARPGYawAxesEquivalent(float A, float B);

static FVector ARPGGetBuildPieceBottomAnchorLocal(const UARPGBuildPieceDefinition* Piece)
{
    FVector Min, Max;
    ARPGGetBuildPieceLocalBounds(Piece, Min, Max);
    const FVector Center = (Min + Max) * 0.5f;
    return FVector(Center.X, Center.Y, Min.Z);
}


static bool ARPGIsBuildLightHorizontalHostKind(EARPGBuildPieceKind Kind)
{
    return Kind == EARPGBuildPieceKind::Foundation || Kind == EARPGBuildPieceKind::Floor;
}

static FVector ARPGGetBuildLightWallContactAnchorLocal(const UARPGBuildPieceDefinition* Piece)
{
    FVector Min, Max;
    ARPGGetBuildPieceLocalBounds(Piece, Min, Max);
    const FVector Center = (Min + Max) * 0.5f;

    // Build-light logical convention: local +Y points away from the wall. The visible back plane is
    // therefore local Min.Y after MeshRelativeTransform. Imported kits can be adapted once in the
    // Data Asset without baking mesh-specific offsets into the placement solver.
    return FVector(Center.X, Min.Y, Center.Z);
}

static FTransform ARPGMakeHorizontalBuildLightTransform(
    const UARPGBuildPieceDefinition* Piece,
    const FVector& SurfacePoint,
    float DesiredYaw)
{
    if (!Piece) return FTransform::Identity;

    FRotator Rotation(0.f, DesiredYaw, 0.f);
    Rotation.Yaw = FMath::GridSnap(Rotation.Yaw, FMath::Max(1.f, Piece->RotationStepDegrees));
    const FQuat Q = Rotation.Quaternion();
    const FVector BottomAnchor = ARPGGetBuildPieceBottomAnchorLocal(Piece);
    const FVector SurfaceAnchor = SurfacePoint + FVector::UpVector * FMath::Max(0.f, Piece->LightSurfaceOffset);

    FTransform Result(Q, FVector::ZeroVector);
    Result.SetLocation(SurfaceAnchor - Q.RotateVector(BottomAnchor) + Q.RotateVector(Piece->PlacementOffset));
    return Result;
}

static bool ARPGMakeWallBuildLightTransform(
    const UARPGBuildPieceDefinition* Piece,
    const FVector& SurfacePoint,
    const FVector& SurfaceNormal,
    FTransform& OutTransform)
{
    if (!Piece) return false;

    FVector Normal2D(SurfaceNormal.X, SurfaceNormal.Y, 0.f);
    if (!Normal2D.Normalize()) return false;

    // Reject top/bottom faces. Wall fixtures are intentionally restricted to near-vertical surfaces.
    if (FMath::Abs(SurfaceNormal.GetSafeNormal().Z) > 0.35f) return false;

    const float NormalYaw = Normal2D.Rotation().Yaw;
    const FQuat Q = FRotator(0.f, NormalYaw - 90.f, 0.f).Quaternion(); // local +Y -> outward normal
    const FVector ContactAnchor = ARPGGetBuildLightWallContactAnchorLocal(Piece);
    const FVector SurfaceAnchor = SurfacePoint + Normal2D * FMath::Max(0.f, Piece->LightSurfaceOffset);

    OutTransform = FTransform(Q, SurfaceAnchor - Q.RotateVector(ContactAnchor) + Q.RotateVector(Piece->PlacementOffset));
    return true;
}

static bool ARPGResolveBuildLightPlacementFromHit(
    const UARPGBuildPieceDefinition* Piece,
    const FHitResult& Hit,
    float DesiredYaw,
    FTransform& OutTransform,
    AARPGBuildPieceActor*& OutSupport)
{
    OutSupport = nullptr;
    if (!Piece || Piece->PieceKind != EARPGBuildPieceKind::Light || !Hit.bBlockingHit) return false;

    AARPGBuildPieceActor* BuildHit = Cast<AARPGBuildPieceActor>(Hit.GetActor());
    if (Piece->LightPlacementMode == EARPGBuildLightPlacementMode::WallSurface)
    {
        if (!BuildHit || !BuildHit->Definition || !BuildHit->IsConstructionComplete() ||
            !ARPGIsWallLikeKind(BuildHit->Definition->PieceKind))
            return false;

        if (!ARPGMakeWallBuildLightTransform(Piece, Hit.ImpactPoint, Hit.ImpactNormal, OutTransform))
            return false;

        OutSupport = BuildHit;
        return true;
    }

    // Horizontal fixtures may sit on terrain or on the finished top surface of Foundation/Floor only.
    if (BuildHit)
    {
        if (!BuildHit->Definition || !BuildHit->IsConstructionComplete() ||
            !ARPGIsBuildLightHorizontalHostKind(BuildHit->Definition->PieceKind))
            return false;
        OutSupport = BuildHit;
    }
    else if (!Piece->bAllowGroundPlacement)
    {
        return false;
    }

    const FVector N = Hit.ImpactNormal.GetSafeNormal();
    if (N.IsNearlyZero()) return false;
    const float MaxSlope = FMath::Clamp(Piece->MaxGroundSlopeDegrees, 0.f, 89.f);
    const float MinUpDot = FMath::Cos(FMath::DegreesToRadians(MaxSlope));
    if (FVector::DotProduct(N, FVector::UpVector) < MinUpDot) return false;

    OutTransform = ARPGMakeHorizontalBuildLightTransform(Piece, Hit.ImpactPoint, DesiredYaw);
    return true;
}

static bool ARPGFindBuildLightSurfaceFromDesired(
    UWorld* World,
    const UARPGBuildPieceDefinition* Piece,
    const FTransform& Desired,
    FTransform& OutTransform,
    AARPGBuildPieceActor*& OutSupport)
{
    OutSupport = nullptr;
    if (!World || !Piece || Piece->PieceKind != EARPGBuildPieceKind::Light) return false;

    const float Capture = FMath::Max(4.f, Piece->SnapCaptureDistance);
    const float CaptureSq = FMath::Square(Capture);
    float BestScore = TNumericLimits<float>::Max();

    if (Piece->LightPlacementMode == EARPGBuildLightPlacementMode::WallSurface)
    {
        const FVector ContactAnchor = ARPGGetBuildLightWallContactAnchorLocal(Piece);
        const FVector PlacementCorrection = Desired.GetRotation().RotateVector(Piece->PlacementOffset);
        const FVector DesiredContactWorld = Desired.TransformPosition(ContactAnchor) - PlacementCorrection;

        for (TActorIterator<AARPGBuildPieceActor> It(World); It; ++It)
        {
            AARPGBuildPieceActor* Host = *It;
            if (!Host || !Host->Definition || !Host->IsConstructionComplete() ||
                !ARPGIsWallLikeKind(Host->Definition->PieceKind))
                continue;

            FVector HostMin, HostMax;
            ARPGGetBuildPieceLocalBounds(Host->Definition, HostMin, HostMax);
            const FTransform HostTransform = Host->GetActorTransform();
            const FVector LocalPoint = HostTransform.InverseTransformPosition(DesiredContactWorld);

            const float EdgePadding = FMath::Max(2.f, Piece->PlacementCollisionClearance + 1.f);
            if (LocalPoint.X < HostMin.X - EdgePadding || LocalPoint.X > HostMax.X + EdgePadding ||
                LocalPoint.Z < HostMin.Z - EdgePadding || LocalPoint.Z > HostMax.Z + EdgePadding)
                continue;

            const float DistToMinFace = FMath::Abs(LocalPoint.Y - HostMin.Y);
            const float DistToMaxFace = FMath::Abs(LocalPoint.Y - HostMax.Y);
            const bool bUseMaxFace = DistToMaxFace <= DistToMinFace;
            const float FaceY = bUseMaxFace ? HostMax.Y : HostMin.Y;
            const FVector SurfaceLocal(
                FMath::Clamp(LocalPoint.X, HostMin.X, HostMax.X),
                FaceY,
                FMath::Clamp(LocalPoint.Z, HostMin.Z, HostMax.Z));
            const FVector SurfaceWorld = HostTransform.TransformPosition(SurfaceLocal);
            const float DistSq = FVector::DistSquared(DesiredContactWorld, SurfaceWorld);
            if (DistSq > CaptureSq || DistSq >= BestScore) continue;

            const FVector NormalLocal = bUseMaxFace ? FVector::YAxisVector : -FVector::YAxisVector;
            const FVector NormalWorld = HostTransform.TransformVectorNoScale(NormalLocal).GetSafeNormal();
            FTransform Candidate;
            if (!ARPGMakeWallBuildLightTransform(Piece, SurfaceWorld, NormalWorld, Candidate)) continue;

            BestScore = DistSq;
            OutTransform = Candidate;
            OutSupport = Host;
        }
        return OutSupport != nullptr;
    }

    const FVector BottomAnchor = ARPGGetBuildPieceBottomAnchorLocal(Piece);
    const FVector PlacementCorrection = Desired.GetRotation().RotateVector(Piece->PlacementOffset);
    const FVector DesiredBottomWorld = Desired.TransformPosition(BottomAnchor) - PlacementCorrection;

    for (TActorIterator<AARPGBuildPieceActor> It(World); It; ++It)
    {
        AARPGBuildPieceActor* Host = *It;
        if (!Host || !Host->Definition || !Host->IsConstructionComplete() ||
            !ARPGIsBuildLightHorizontalHostKind(Host->Definition->PieceKind))
            continue;

        FVector HostMin, HostMax;
        ARPGGetBuildPieceLocalBounds(Host->Definition, HostMin, HostMax);
        const FTransform HostTransform = Host->GetActorTransform();
        const FVector LocalPoint = HostTransform.InverseTransformPosition(DesiredBottomWorld);
        const float EdgePadding = FMath::Max(2.f, Piece->PlacementCollisionClearance + 1.f);
        if (LocalPoint.X < HostMin.X - EdgePadding || LocalPoint.X > HostMax.X + EdgePadding ||
            LocalPoint.Y < HostMin.Y - EdgePadding || LocalPoint.Y > HostMax.Y + EdgePadding)
            continue;

        const FVector SurfaceLocal(
            FMath::Clamp(LocalPoint.X, HostMin.X, HostMax.X),
            FMath::Clamp(LocalPoint.Y, HostMin.Y, HostMax.Y),
            HostMax.Z);
        const FVector SurfaceWorld = HostTransform.TransformPosition(SurfaceLocal);
        const float DistSq = FVector::DistSquared(DesiredBottomWorld, SurfaceWorld);
        if (DistSq > CaptureSq || DistSq >= BestScore) continue;

        BestScore = DistSq;
        OutTransform = ARPGMakeHorizontalBuildLightTransform(Piece, SurfaceWorld, Desired.Rotator().Yaw);
        OutSupport = Host;
    }

    return OutSupport != nullptr;
}

static bool ARPGIsSettlementSurfacePiece(const UARPGBuildPieceDefinition* Piece)
{
    return Piece && (Piece->PieceKind == EARPGBuildPieceKind::Bed || Piece->PieceKind == EARPGBuildPieceKind::SettlementHub);
}

static bool ARPGIsSettlementHorizontalHostKind(EARPGBuildPieceKind Kind)
{
    return Kind == EARPGBuildPieceKind::Foundation || Kind == EARPGBuildPieceKind::Floor;
}

static float ARPGGetSettlementSurfaceOffset(const UARPGBuildPieceDefinition* Piece)
{
    if (!Piece) return 0.f;
    if (Piece->PieceKind == EARPGBuildPieceKind::Bed) return FMath::Max(0.f, Piece->BedSurfaceOffset);
    if (Piece->PieceKind == EARPGBuildPieceKind::SettlementHub) return FMath::Max(0.f, Piece->SettlementHubSurfaceOffset);
    return 0.f;
}

static FTransform ARPGMakeSettlementHorizontalTransform(
    const UARPGBuildPieceDefinition* Piece,
    const FVector& SurfacePoint,
    float DesiredYaw)
{
    if (!Piece) return FTransform::Identity;
    FRotator Rotation(0.f, DesiredYaw, 0.f);
    Rotation.Yaw = FMath::GridSnap(Rotation.Yaw, FMath::Max(1.f, Piece->RotationStepDegrees));
    const FQuat Q = Rotation.Quaternion();
    const FVector BottomAnchor = ARPGGetBuildPieceBottomAnchorLocal(Piece);
    const FVector SurfaceAnchor = SurfacePoint + FVector::UpVector * ARPGGetSettlementSurfaceOffset(Piece);
    FTransform Result(Q, FVector::ZeroVector);
    Result.SetLocation(SurfaceAnchor - Q.RotateVector(BottomAnchor) + Q.RotateVector(Piece->PlacementOffset));
    return Result;
}

/**
 * Beds and Settlement Hubs use a native furnishing/utility surface contract rather than structural
 * snap points. Beds deliberately require a completed Foundation/Floor host; Hubs may additionally
 * opt into terrain via bAllowGroundPlacement. This branch is kept entirely separate from structural
 * Stair/Wall/insert topology so settlement content cannot perturb the proven construction graph.
 */
static bool ARPGResolveSettlementPlacementFromHit(
    const UARPGBuildPieceDefinition* Piece,
    const FHitResult& Hit,
    float DesiredYaw,
    FTransform& OutTransform,
    AARPGBuildPieceActor*& OutSupport)
{
    OutSupport = nullptr;
    if (!ARPGIsSettlementSurfacePiece(Piece) || !Hit.bBlockingHit) return false;

    AARPGBuildPieceActor* BuildHit = Cast<AARPGBuildPieceActor>(Hit.GetActor());
    if (BuildHit)
    {
        if (!BuildHit->Definition || !BuildHit->IsConstructionComplete() ||
            !ARPGIsSettlementHorizontalHostKind(BuildHit->Definition->PieceKind))
            return false;
        OutSupport = BuildHit;
    }
    else
    {
        if (Piece->PieceKind == EARPGBuildPieceKind::Bed || !Piece->bAllowGroundPlacement) return false;
    }

    const FVector Normal = Hit.ImpactNormal.GetSafeNormal();
    if (Normal.IsNearlyZero()) return false;
    const float MaxSlope = FMath::Clamp(Piece->MaxGroundSlopeDegrees, 0.f, 89.f);
    if (FVector::DotProduct(Normal, FVector::UpVector) < FMath::Cos(FMath::DegreesToRadians(MaxSlope))) return false;

    OutTransform = ARPGMakeSettlementHorizontalTransform(Piece, Hit.ImpactPoint, DesiredYaw);
    return true;
}

static bool ARPGFindSettlementSurfaceFromDesired(
    UWorld* World,
    const UARPGBuildPieceDefinition* Piece,
    const FTransform& Desired,
    ECollisionChannel TraceChannel,
    FTransform& OutTransform,
    AARPGBuildPieceActor*& OutSupport)
{
    OutSupport = nullptr;
    if (!World || !ARPGIsSettlementSurfacePiece(Piece)) return false;

    const FVector BottomAnchor = ARPGGetBuildPieceBottomAnchorLocal(Piece);
    const FVector PlacementCorrection = Desired.GetRotation().RotateVector(Piece->PlacementOffset);
    const FVector DesiredBottomWorld = Desired.TransformPosition(BottomAnchor) - PlacementCorrection;
    const float Capture = FMath::Max(4.f, Piece->SnapCaptureDistance);
    const float CaptureSq = FMath::Square(Capture);
    float BestScore = TNumericLimits<float>::Max();

    for (TActorIterator<AARPGBuildPieceActor> It(World); It; ++It)
    {
        AARPGBuildPieceActor* Host = *It;
        if (!Host || !Host->Definition || !Host->IsConstructionComplete() ||
            !ARPGIsSettlementHorizontalHostKind(Host->Definition->PieceKind)) continue;

        FVector HostMin, HostMax;
        ARPGGetBuildPieceLocalBounds(Host->Definition, HostMin, HostMax);
        const FTransform HostTransform = Host->GetActorTransform();
        const FVector LocalPoint = HostTransform.InverseTransformPosition(DesiredBottomWorld);
        const float Padding = FMath::Max(2.f, Piece->PlacementCollisionClearance + 1.f);
        if (LocalPoint.X < HostMin.X - Padding || LocalPoint.X > HostMax.X + Padding ||
            LocalPoint.Y < HostMin.Y - Padding || LocalPoint.Y > HostMax.Y + Padding) continue;

        const FVector SurfaceLocal(
            FMath::Clamp(LocalPoint.X, HostMin.X, HostMax.X),
            FMath::Clamp(LocalPoint.Y, HostMin.Y, HostMax.Y),
            HostMax.Z);
        const FVector SurfaceWorld = HostTransform.TransformPosition(SurfaceLocal);
        const float DistSq = FVector::DistSquared(DesiredBottomWorld, SurfaceWorld);
        if (DistSq > CaptureSq || DistSq >= BestScore) continue;

        BestScore = DistSq;
        OutSupport = Host;
        OutTransform = ARPGMakeSettlementHorizontalTransform(Piece, SurfaceWorld, Desired.Rotator().Yaw);
    }
    if (OutSupport) return true;

    // Only Hubs can opt into terrain. Re-seat on an authority-side trace instead of trusting client Z.
    if (Piece->PieceKind != EARPGBuildPieceKind::SettlementHub || !Piece->bAllowGroundPlacement) return false;
    const float ProbeLift = FMath::Max(4.f, Piece->PlacementCollisionClearance + 2.f);
    FCollisionQueryParams Params(SCENE_QUERY_STAT(ARPGSettlementUtilitySupport), false);
    FHitResult Hit;
    const FVector Start = DesiredBottomWorld + FVector::UpVector * ProbeLift;
    const FVector End = DesiredBottomWorld - FVector::UpVector * FMath::Max(1.f, Piece->SupportTraceDepth);
    if (!World->LineTraceSingleByChannel(Hit, Start, End, TraceChannel, Params)) return false;
    if (AARPGBuildPieceActor* HitBuild = Cast<AARPGBuildPieceActor>(Hit.GetActor()))
    {
        if (!HitBuild->Definition || !HitBuild->IsConstructionComplete() ||
            !ARPGIsSettlementHorizontalHostKind(HitBuild->Definition->PieceKind)) return false;
        OutSupport = HitBuild;
    }
    const FVector Normal = Hit.ImpactNormal.GetSafeNormal();
    const float MaxSlope = FMath::Clamp(Piece->MaxGroundSlopeDegrees, 0.f, 89.f);
    if (Normal.IsNearlyZero() || FVector::DotProduct(Normal, FVector::UpVector) < FMath::Cos(FMath::DegreesToRadians(MaxSlope))) return false;
    OutTransform = ARPGMakeSettlementHorizontalTransform(Piece, Hit.ImpactPoint, Desired.Rotator().Yaw);
    return true;
}

/**
 * Resolve Wall-family presentation from the actual horizontal support edge after the winning snap
 * transform has been selected. This is intentionally a post-snap normalization step: a Floor edge,
 * a direct wall stack and a lateral/corner wall can all advertise the same logical wall column, but
 * only the horizontal cell edge knows the unambiguous outward normal of an exterior wall.
 *
 * The normal is reconstructed from the candidate's POSITION in the support's local grid, not from
 * the support actor yaw or from whichever candidate won the semantic tie. That makes front/back
 * facing invariant when a square Floor has inherited 0/90/180/270 yaw from different support walls.
 * If two horizontal cells claim the same wall slot from opposite sides (an interior partition), the
 * result is deliberately treated as ambiguous and the already-selected facing is preserved. The
 * existing snap-search overlap set is reused, so this normalization adds no second world query.
 */
static bool ARPGTryGetHorizontalWallFacingClaim(
    const AARPGBuildPieceActor* Support,
    const UARPGBuildPieceDefinition* IncomingPiece,
    const FTransform& WallTransform,
    float PositionTolerance,
    float& OutNativeYaw,
    FVector2D& OutWorldOutward)
{
    if (!Support || !Support->Definition || !IncomingPiece || !Support->IsConstructionComplete()) return false;
    if (!ARPGIsHorizontalStructuralKind(Support->Definition->PieceKind)) return false;

    const FTransform SupportTransform = Support->GetActorTransform();
    const float HalfGrid = FMath::Max(1.f, Support->Definition->SnapSize) * 0.5f;
    const FVector WallLocal = SupportTransform.InverseTransformPosition(WallTransform.GetLocation());
    const float PositionToleranceSq = FMath::Square(PositionTolerance);

    struct FARPGBuildFacingEdge
    {
        FVector2D LocalCenter;
        FVector LocalOutward;
        float TangentRelativeYaw;
    };

    const FARPGBuildFacingEdge Edges[] =
    {
        { FVector2D(0.f,  HalfGrid), FVector(0.f,  1.f, 0.f),   0.f },
        { FVector2D(0.f, -HalfGrid), FVector(0.f, -1.f, 0.f),   0.f },
        { FVector2D( HalfGrid, 0.f), FVector( 1.f, 0.f, 0.f),  90.f },
        { FVector2D(-HalfGrid, 0.f), FVector(-1.f, 0.f, 0.f),  90.f }
    };

    const FARPGBuildFacingEdge* MatchedEdge = nullptr;
    for (const FARPGBuildFacingEdge& Edge : Edges)
    {
        const FVector2D Delta(WallLocal.X - Edge.LocalCenter.X, WallLocal.Y - Edge.LocalCenter.Y);
        if (Delta.SizeSquared() > PositionToleranceSq) continue;

        const float ExpectedWorldAxisYaw = SupportTransform.Rotator().Yaw + Edge.TangentRelativeYaw;
        if (!ARPGYawAxesEquivalent(ExpectedWorldAxisYaw, WallTransform.Rotator().Yaw)) continue;
        MatchedEdge = &Edge;
        break;
    }
    if (!MatchedEdge) return false;

    // Only horizontal cells that actually own this wall's story baseline are allowed to vote on
    // facade direction. A Foundation owns the first-story wall at its visible TOP. Upper horizontal
    // pieces own the next wall at their visible BOTTOM/story plane (v2.15.20 canonical story grid).
    float WallMinZ, WallMaxZ, SupportMinZ, SupportMaxZ;
    ARPGGetDefinitionWorldZRange(IncomingPiece, WallTransform, WallMinZ, WallMaxZ);
    ARPGGetDefinitionWorldZRange(Support->Definition, SupportTransform, SupportMinZ, SupportMaxZ);
    const bool bCorrectStoryPlane =
        Support->Definition->PieceKind == EARPGBuildPieceKind::Foundation
            ? FMath::Abs(WallMinZ - SupportMaxZ) <= PositionTolerance
            : FMath::Abs(WallMinZ - SupportMinZ) <= PositionTolerance;
    if (!bCorrectStoryPlane) return false;

    // The occupied cell tells us WHICH side is outside, but its own native Wall socket tells us the
    // actual authored yaw. This deliberately restores the proven v2.15.6 socket-facing contract and
    // avoids deriving art orientation from an assumed actor-local +Y mesh front.
    TArray<FTransform> NativeCandidates;
    Support->GetSnapTransformsFor(IncomingPiece, NativeCandidates);
    float BestCandidateDistSq = TNumericLimits<float>::Max();
    bool bFoundNativeCandidate = false;
    float NativeYaw = WallTransform.Rotator().Yaw;
    for (const FTransform& Candidate : NativeCandidates)
    {
        const FVector CandidateDelta = Candidate.GetLocation() - WallTransform.GetLocation();
        const FVector2D CandidateDeltaXY(CandidateDelta.X, CandidateDelta.Y);
        if (CandidateDeltaXY.SizeSquared() > PositionToleranceSq) continue;

        float CandidateMinZ, CandidateMaxZ;
        ARPGGetDefinitionWorldZRange(IncomingPiece, Candidate, CandidateMinZ, CandidateMaxZ);
        if (FMath::Abs(CandidateMinZ - WallMinZ) > PositionTolerance) continue;
        if (!ARPGYawAxesEquivalent(Candidate.Rotator().Yaw, WallTransform.Rotator().Yaw)) continue;

        const float CandidateDistSq = CandidateDeltaXY.SizeSquared() + FMath::Square(CandidateMinZ - WallMinZ);
        if (!bFoundNativeCandidate || CandidateDistSq < BestCandidateDistSq)
        {
            bFoundNativeCandidate = true;
            BestCandidateDistSq = CandidateDistSq;
            NativeYaw = Candidate.Rotator().Yaw;
        }
    }
    if (!bFoundNativeCandidate) return false;

    const FVector WorldOutward3D = SupportTransform.GetRotation().RotateVector(MatchedEdge->LocalOutward);
    const FVector2D WorldOutward(WorldOutward3D.X, WorldOutward3D.Y);
    if (WorldOutward.IsNearlyZero()) return false;

    OutWorldOutward = WorldOutward.GetSafeNormal();
    OutNativeYaw = FRotator::NormalizeAxis(NativeYaw);
    return true;
}

/**
 * Final Wall-family presentation is resolved from structural occupancy, not from camera yaw and not
 * from a guessed mesh-front axis. Every Foundation/Floor/Ceiling/Roof cell touching the selected wall
 * edge casts a facing claim. A perimeter edge has one occupied side, so that cell's own native wall
 * socket is authoritative. Two opposite occupied cells mean an intentional interior partition; in
 * that case the framework preserves vertical-stack continuity (or the already-selected yaw on the
 * first story) rather than arbitrarily choosing one room as "outside".
 *
 * This is deliberately multi-cell aware: a 1x2, 2x2 or larger foundation footprint behaves exactly
 * like a single foundation around its OUTER perimeter, regardless of whether the player is standing
 * inside or outside while placing the wall. The existing snap-search overlap set is reused, so this
 * adds no second world query to the realtime placement preview.
 */
static void ARPGCanonicalizeWallFacingFromHorizontalSupport(
    const TArray<FOverlapResult>& SupportOverlaps,
    const UARPGBuildPieceDefinition* IncomingPiece,
    FTransform& InOutTransform)
{
    if (!IncomingPiece || !ARPGIsWallLikeKind(IncomingPiece->PieceKind)) return;

    const float PositionTolerance = FMath::Max(1.f, IncomingPiece->PlacementCollisionClearance + 0.5f);
    const float PositionToleranceSq = FMath::Square(PositionTolerance);

    bool bFoundHorizontalClaim = false;
    bool bAmbiguousHorizontalClaim = false;
    FVector2D FirstOutward = FVector2D::ZeroVector;
    float FirstNativeYaw = InOutTransform.Rotator().Yaw;

    TSet<const AARPGBuildPieceActor*> SeenHorizontalSupports;
    for (const FOverlapResult& Overlap : SupportOverlaps)
    {
        const AARPGBuildPieceActor* Support = Cast<AARPGBuildPieceActor>(Overlap.GetActor());
        if (!Support || SeenHorizontalSupports.Contains(Support) || !Support->Definition || !Support->IsConstructionComplete()) continue;
        SeenHorizontalSupports.Add(Support);
        if (!ARPGIsHorizontalStructuralKind(Support->Definition->PieceKind)) continue;

        float NativeYaw = InOutTransform.Rotator().Yaw;
        FVector2D WorldOutward = FVector2D::ZeroVector;
        if (!ARPGTryGetHorizontalWallFacingClaim(
                Support,
                IncomingPiece,
                InOutTransform,
                PositionTolerance,
                NativeYaw,
                WorldOutward))
            continue;

        if (!bFoundHorizontalClaim)
        {
            bFoundHorizontalClaim = true;
            FirstOutward = WorldOutward;
            FirstNativeYaw = NativeYaw;
            continue;
        }

        const float DirectionDot = FVector2D::DotProduct(FirstOutward, WorldOutward);
        if (DirectionDot < 0.f)
        {
            // Two occupied cells on opposite sides of the same edge describe an interior partition.
            // Neither cell is "outside". Preserve the established wall column below if one exists.
            bAmbiguousHorizontalClaim = true;
        }
        else if (DirectionDot > 0.999f)
        {
            // Duplicate/coincident support claims on the same side must agree on native authored yaw.
            // If unusual custom content disagrees, treat it as ambiguous instead of view-order dependent.
            if (FMath::Abs(FMath::FindDeltaAngleDegrees(FirstNativeYaw, NativeYaw)) > 1.f)
                bAmbiguousHorizontalClaim = true;
        }
        else
        {
            // Perpendicular claims at one wall origin are non-canonical/custom topology. Do not invent
            // a facade direction; fall back to established vertical continuity or selected yaw.
            bAmbiguousHorizontalClaim = true;
        }
    }

    if (bFoundHorizontalClaim && !bAmbiguousHorizontalClaim)
    {
        FRotator Rotation = InOutTransform.Rotator();
        Rotation.Pitch = 0.f;
        Rotation.Roll = 0.f;
        Rotation.Yaw = FirstNativeYaw;
        InOutTransform.SetRotation(Rotation.Quaternion());
        return;
    }

    // Interior partitions, wall-only upper stories and unusual/custom topology have no unique occupied
    // exterior cell. In those cases, preserve the exact native vertical-stack facing from the Wall-family
    // piece directly below when it owns this same structural column.
    for (const FOverlapResult& Overlap : SupportOverlaps)
    {
        const AARPGBuildPieceActor* VerticalSupport = Cast<AARPGBuildPieceActor>(Overlap.GetActor());
        if (!VerticalSupport || !VerticalSupport->Definition || !VerticalSupport->IsConstructionComplete()) continue;
        if (!ARPGIsWallLikeKind(VerticalSupport->Definition->PieceKind)) continue;

        TArray<FTransform> StackCandidates;
        VerticalSupport->GetSnapTransformsFor(IncomingPiece, StackCandidates);
        for (const FTransform& StackCandidate : StackCandidates)
        {
            if (FVector::DistSquared(StackCandidate.GetLocation(), InOutTransform.GetLocation()) > PositionToleranceSq) continue;

            const FVector CandidateLocal = VerticalSupport->GetActorTransform().InverseTransformPosition(StackCandidate.GetLocation());
            const float HorizontalOffsetSq = FMath::Square(CandidateLocal.X) + FMath::Square(CandidateLocal.Y);
            const float RelativeYawDelta = FMath::Abs(FMath::FindDeltaAngleDegrees(
                VerticalSupport->GetActorRotation().Yaw, StackCandidate.Rotator().Yaw));
            const bool bDirectVerticalStack =
                HorizontalOffsetSq <= PositionToleranceSq &&
                CandidateLocal.Z > PositionTolerance &&
                RelativeYawDelta <= 1.f;
            if (!bDirectVerticalStack) continue;

            FRotator Rotation = InOutTransform.Rotator();
            Rotation.Pitch = 0.f;
            Rotation.Roll = 0.f;
            Rotation.Yaw = VerticalSupport->GetActorRotation().Yaw;
            InOutTransform.SetRotation(Rotation.Quaternion());
            return;
        }
    }

    // First-story interior partitions intentionally have no globally correct exterior side. Keep the
    // selected native snap yaw so manual rotation/custom sockets remain meaningful instead of forcing
    // an arbitrary 180-degree flip.
}

/**
 * Structural placement is authored on logical module volumes, while the runtime Static Mesh collision
 * can contain decorative posts, braces, lips and beams that intentionally extend across a modular seam.
 * Placement validation must therefore distinguish a true logical-volume conflict from harmless art
 * collision. The incoming preview already uses PlacementBounds; build-vs-build validation must use the
 * neighbour's PlacementBounds as well instead of comparing one logical box against the other actor's
 * rendered collision hull.
 */
struct FARPGPlacementOBB
{
    FVector Center = FVector::ZeroVector;
    FVector Axis[3] = { FVector::ForwardVector, FVector::RightVector, FVector::UpVector };
    FVector Extents = FVector::OneVector;
    FQuat Rotation = FQuat::Identity;
};

static FARPGPlacementOBB ARPGMakePlacementOBB(const UARPGBuildPieceDefinition* Piece, const FTransform& Transform)
{
    FARPGPlacementOBB Result;
    if (!Piece) return Result;

    const float Clearance = FMath::Max(0.f, Piece->PlacementCollisionClearance);
    FVector Extents = (Piece->PlacementBounds - FVector(Clearance)).ComponentMax(FVector(1.f));
    const FVector Scale = Transform.GetScale3D().GetAbs();
    Extents = Extents * Scale;

    const FQuat Rotation = Transform.GetRotation().GetNormalized();
    Result.Rotation = Rotation;
    Result.Center = Transform.TransformPosition(ARPGGetBuildPieceBoundsCenterLocal(Piece));
    Result.Axis[0] = Rotation.RotateVector(FVector::ForwardVector).GetSafeNormal();
    Result.Axis[1] = Rotation.RotateVector(FVector::RightVector).GetSafeNormal();
    Result.Axis[2] = Rotation.RotateVector(FVector::UpVector).GetSafeNormal();
    Result.Extents = Extents;
    return Result;
}

static bool ARPGPlacementOBBsOverlap(const FARPGPlacementOBB& A, const FARPGPlacementOBB& B)
{
    // Full 15-axis OBB SAT. A tiny numerical epsilon prevents a flush modular seam from becoming a
    // false penetration because of floating-point transform composition, without hiding real overlap.
    float R[3][3];
    float AbsR[3][3];
    constexpr float SatEpsilon = 1.e-4f;
    for (int32 I = 0; I < 3; ++I)
    {
        for (int32 J = 0; J < 3; ++J)
        {
            R[I][J] = FVector::DotProduct(A.Axis[I], B.Axis[J]);
            AbsR[I][J] = FMath::Abs(R[I][J]) + SatEpsilon;
        }
    }

    const FVector WorldDelta = B.Center - A.Center;
    const float T[3] =
    {
        FVector::DotProduct(WorldDelta, A.Axis[0]),
        FVector::DotProduct(WorldDelta, A.Axis[1]),
        FVector::DotProduct(WorldDelta, A.Axis[2])
    };

    for (int32 I = 0; I < 3; ++I)
    {
        const float RA = A.Extents[I];
        const float RB = B.Extents[0] * AbsR[I][0] + B.Extents[1] * AbsR[I][1] + B.Extents[2] * AbsR[I][2];
        if (FMath::Abs(T[I]) > RA + RB) return false;
    }

    for (int32 J = 0; J < 3; ++J)
    {
        const float RA = A.Extents[0] * AbsR[0][J] + A.Extents[1] * AbsR[1][J] + A.Extents[2] * AbsR[2][J];
        const float RB = B.Extents[J];
        const float ProjectedT = FMath::Abs(T[0] * R[0][J] + T[1] * R[1][J] + T[2] * R[2][J]);
        if (ProjectedT > RA + RB) return false;
    }

    for (int32 I = 0; I < 3; ++I)
    {
        const int32 I1 = (I + 1) % 3;
        const int32 I2 = (I + 2) % 3;
        for (int32 J = 0; J < 3; ++J)
        {
            const int32 J1 = (J + 1) % 3;
            const int32 J2 = (J + 2) % 3;
            const float RA = A.Extents[I1] * AbsR[I2][J] + A.Extents[I2] * AbsR[I1][J];
            const float RB = B.Extents[J1] * AbsR[I][J2] + B.Extents[J2] * AbsR[I][J1];
            const float ProjectedT = FMath::Abs(T[I2] * R[I1][J] - T[I1] * R[I2][J]);
            if (ProjectedT > RA + RB) return false;
        }
    }

    return true;
}

/**
 * Build the logical occupancy primitives used by BOTH broad placement querying and the final
 * build-vs-build blocker decision. Keeping those two stages on one geometry contract is critical:
 * v2.15.26 queried Stairs with diagonal slices but later rebuilt them as one full PlacementBounds OBB
 * in ARPGPlacementVolumesOverlapMeaningfully(), which reintroduced the exact false blocker the slice
 * query was meant to remove.
 *
 * Standard pieces use one authored OBB. Stairs use eight low-to-high slices (local +X uphill, local +Z
 * up) so the large empty triangular volume inside the Stair AABB never becomes structural occupancy.
 */
static void ARPGBuildPlacementOccupancyOBBs(
    const UARPGBuildPieceDefinition* Piece,
    const FTransform& Transform,
    TArray<FARPGPlacementOBB>& OutOBBs)
{
    OutOBBs.Reset();
    if (!Piece) return;

    if (Piece->PieceKind != EARPGBuildPieceKind::Stair)
    {
        OutOBBs.Add(ARPGMakePlacementOBB(Piece, Transform));
        return;
    }

    constexpr int32 StairProfileSliceCount = 8;
    const float Clearance = FMath::Max(0.f, Piece->PlacementCollisionClearance);
    const FVector BoundsCenterLocal = ARPGGetBuildPieceBoundsCenterLocal(Piece);
    const FVector LogicalMin = BoundsCenterLocal - Piece->PlacementBounds;
    const FVector LogicalMax = BoundsCenterLocal + Piece->PlacementBounds;
    const FVector AbsScale = Transform.GetScale3D().GetAbs();
    const FQuat Rotation = Transform.GetRotation().GetNormalized();

    OutOBBs.Reserve(StairProfileSliceCount);
    for (int32 SliceIndex = 0; SliceIndex < StairProfileSliceCount; ++SliceIndex)
    {
        const float T0 = static_cast<float>(SliceIndex) / StairProfileSliceCount;
        const float T1 = static_cast<float>(SliceIndex + 1) / StairProfileSliceCount;
        const float TMid = (T0 + T1) * 0.5f;

        const float LocalX0 = FMath::Lerp(LogicalMin.X, LogicalMax.X, T0);
        const float LocalX1 = FMath::Lerp(LogicalMin.X, LogicalMax.X, T1);
        const float LocalZ0 = FMath::Lerp(LogicalMin.Z, LogicalMax.Z, T0);
        const float LocalZ1 = FMath::Lerp(LogicalMin.Z, LogicalMax.Z, T1);

        const FVector SliceCenterLocal(
            FMath::Lerp(LogicalMin.X, LogicalMax.X, TMid),
            BoundsCenterLocal.Y,
            FMath::Lerp(LogicalMin.Z, LogicalMax.Z, TMid));
        FVector SliceExtentsLocal(
            FMath::Max(1.f, (LocalX1 - LocalX0) * 0.5f - Clearance),
            FMath::Max(1.f, Piece->PlacementBounds.Y - Clearance),
            FMath::Max(1.f, (LocalZ1 - LocalZ0) * 0.5f - Clearance));

        FARPGPlacementOBB Slice;
        Slice.Center = Transform.TransformPosition(SliceCenterLocal);
        Slice.Rotation = Rotation;
        Slice.Axis[0] = Rotation.RotateVector(FVector::ForwardVector).GetSafeNormal();
        Slice.Axis[1] = Rotation.RotateVector(FVector::RightVector).GetSafeNormal();
        Slice.Axis[2] = Rotation.RotateVector(FVector::UpVector).GetSafeNormal();
        Slice.Extents = SliceExtentsLocal * AbsScale;
        OutOBBs.Add(Slice);
    }
}

static bool ARPGPlacementVolumesOverlapMeaningfully(
    const AARPGBuildPieceActor* Neighbor,
    const UARPGBuildPieceDefinition* IncomingPiece,
    const FTransform& IncomingFinal)
{
    if (!Neighbor || !Neighbor->Definition || !IncomingPiece) return true;

    TArray<FARPGPlacementOBB> IncomingVolumes;
    TArray<FARPGPlacementOBB> NeighborVolumes;
    ARPGBuildPlacementOccupancyOBBs(IncomingPiece, IncomingFinal, IncomingVolumes);
    ARPGBuildPlacementOccupancyOBBs(Neighbor->Definition, Neighbor->GetActorTransform(), NeighborVolumes);

    for (const FARPGPlacementOBB& IncomingVolume : IncomingVolumes)
    {
        for (const FARPGPlacementOBB& NeighborVolume : NeighborVolumes)
        {
            if (ARPGPlacementOBBsOverlap(IncomingVolume, NeighborVolume)) return true;
        }
    }
    return false;
}

/**
 * Stairs are not box-shaped occupancy. Use the same occupancy primitives as the final blocker check so
 * the broad physics query and semantic build-vs-build validation cannot disagree about Stair shape.
 */
static void ARPGGatherPlacementOverlaps(
    UWorld* World,
    const UARPGBuildPieceDefinition* Piece,
    const FTransform& Final,
    ECollisionChannel CollisionChannel,
    const FCollisionQueryParams& Params,
    TArray<FOverlapResult>& OutOverlaps)
{
    OutOverlaps.Reset();
    if (!World || !Piece) return;

    TArray<FARPGPlacementOBB> OccupancyVolumes;
    ARPGBuildPlacementOccupancyOBBs(Piece, Final, OccupancyVolumes);

    TSet<AActor*> SeenActors;
    for (const FARPGPlacementOBB& Volume : OccupancyVolumes)
    {
        TArray<FOverlapResult> VolumeOverlaps;
        World->OverlapMultiByChannel(
            VolumeOverlaps,
            Volume.Center,
            Volume.Rotation,
            CollisionChannel,
            FCollisionShape::MakeBox(Volume.Extents),
            Params);

        for (const FOverlapResult& Overlap : VolumeOverlaps)
        {
            AActor* Actor = Overlap.GetActor();
            if (Actor)
            {
                if (SeenActors.Contains(Actor)) continue;
                SeenActors.Add(Actor);
            }
            OutOverlaps.Add(Overlap);
        }
    }
}

static bool ARPGWallOccupiesHorizontalStructuralEdge(
    const FTransform& HorizontalTransform,
    float HalfGrid,
    const FVector& WallWorldLocation,
    float WallWorldYaw,
    float PositionTolerance);

enum class EARPGStructuralOccupancyRelation : uint8
{
    NotApplicable,
    CompatibleSeam,
    Conflict
};

static void ARPGGetDefinitionWorldZRange(
    const UARPGBuildPieceDefinition* Piece,
    const FTransform& Transform,
    float& OutMinZ,
    float& OutMaxZ)
{
    FVector LocalMin, LocalMax;
    ARPGGetBuildPieceLocalBounds(Piece, LocalMin, LocalMax);
    OutMinZ = TNumericLimits<float>::Max();
    OutMaxZ = -TNumericLimits<float>::Max();
    for (int32 X = 0; X < 2; ++X)
    {
        for (int32 Y = 0; Y < 2; ++Y)
        {
            for (int32 Z = 0; Z < 2; ++Z)
            {
                const FVector Corner(
                    X ? LocalMax.X : LocalMin.X,
                    Y ? LocalMax.Y : LocalMin.Y,
                    Z ? LocalMax.Z : LocalMin.Z);
                const float WorldZ = Transform.TransformPosition(Corner).Z;
                OutMinZ = FMath::Min(OutMinZ, WorldZ);
                OutMaxZ = FMath::Max(OutMaxZ, WorldZ);
            }
        }
    }
}

/**
 * Wall-family structural occupancy belongs to the authored story lattice, not the rendered mesh
 * height. The visible/art bounds are still used to find the wall bottom/pivot compensation, but the
 * structural top is always exactly StandardWallHeight above that bottom. This prevents a wall mesh
 * that is 298/302 cm tall (or has decorative trim) from changing Floor placement, vertical stacking,
 * or seam classification on every successive storey.
 */
static void ARPGGetWallStructuralWorldZRange(
    const UARPGBuildPieceDefinition* Piece,
    const FTransform& Transform,
    float& OutMinZ,
    float& OutMaxZ)
{
    ARPGGetDefinitionWorldZRange(Piece, Transform, OutMinZ, OutMaxZ);
    if (Piece && ARPGIsWallLikeKind(Piece->PieceKind))
    {
        OutMaxZ = OutMinZ + FMath::Max(1.f, Piece->StandardWallHeight);
    }
}

/**
 * Wall snap ownership is compared by structural slot, not merely by target actor identity. Standard
 * v2.15.20+ horizontal Wall sockets and direct vertical-stack sockets now share the same canonical
 * story plane, so they normally compare equal by position before this helper is needed. Keep this
 * structural fallback for unusual/custom authored candidates that still resolve to the same wall
 * column without making camera yaw decide exterior facing.
 */
static bool ARPGWallSnapCandidatesShareStructuralSlot(
    const UARPGBuildPieceDefinition* IncomingPiece,
    const AARPGBuildPieceActor* TargetA,
    const FTransform& CandidateA,
    const AARPGBuildPieceActor* TargetB,
    const FTransform& CandidateB,
    float PositionTolerance)
{
    if (!IncomingPiece || !TargetA || !TargetA->Definition || !TargetB || !TargetB->Definition) return false;

    const float PositionToleranceSq = FMath::Square(PositionTolerance);
    const FVector Delta = CandidateA.GetLocation() - CandidateB.GetLocation();
    if (Delta.SizeSquared() <= PositionToleranceSq) return true;
    if (!ARPGIsWallLikeKind(IncomingPiece->PieceKind)) return false;

    const FVector2D DeltaXY(Delta.X, Delta.Y);
    if (DeltaXY.SizeSquared() > PositionToleranceSq) return false;

    const bool bAHorizontal = ARPGIsHorizontalStructuralKind(TargetA->Definition->PieceKind);
    const bool bBHorizontal = ARPGIsHorizontalStructuralKind(TargetB->Definition->PieceKind);
    const bool bAWall = ARPGIsWallLikeKind(TargetA->Definition->PieceKind);
    const bool bBWall = ARPGIsWallLikeKind(TargetB->Definition->PieceKind);
    if (!((bAHorizontal && bBWall) || (bBHorizontal && bAWall))) return false;

    const AARPGBuildPieceActor* HorizontalTarget = bAHorizontal ? TargetA : TargetB;
    const FTransform& HorizontalCandidate = bAHorizontal ? CandidateA : CandidateB;
    const FTransform& WallCandidate = bAHorizontal ? CandidateB : CandidateA;

    float HorizontalMinZ, HorizontalMaxZ;
    ARPGGetDefinitionWorldZRange(
        HorizontalTarget->Definition,
        HorizontalTarget->GetActorTransform(),
        HorizontalMinZ,
        HorizontalMaxZ);

    float HorizontalCandidateMinZ, HorizontalCandidateMaxZ;
    float WallCandidateMinZ, WallCandidateMaxZ;
    ARPGGetDefinitionWorldZRange(IncomingPiece, HorizontalCandidate, HorizontalCandidateMinZ, HorizontalCandidateMaxZ);
    ARPGGetDefinitionWorldZRange(IncomingPiece, WallCandidate, WallCandidateMinZ, WallCandidateMaxZ);

    const bool bHorizontalCandidateStartsOnStoryPlane =
        FMath::Abs(HorizontalCandidateMinZ - HorizontalMaxZ) <= PositionTolerance;
    const bool bWallStackCandidateStartsOnStoryPlane =
        FMath::Abs(WallCandidateMinZ - HorizontalMaxZ) <= PositionTolerance;
    const bool bCandidateBottomsAgree =
        FMath::Abs(HorizontalCandidateMinZ - WallCandidateMinZ) <= PositionTolerance;

    return bHorizontalCandidateStartsOnStoryPlane &&
           bWallStackCandidateStartsOnStoryPlane &&
           bCandidateBottomsAgree;
}

struct FARPGWallStructuralSegment
{
    FVector2D Center = FVector2D::ZeroVector;
    FVector2D Axis = FVector2D(1.f, 0.f);
    float HalfLength = 1.f;
    float MinZ = 0.f;
    float MaxZ = 0.f;
};

static FARPGWallStructuralSegment ARPGMakeWallStructuralSegment(
    const UARPGBuildPieceDefinition* Piece,
    const FTransform& Transform)
{
    FARPGWallStructuralSegment Result;
    const FVector WorldLocation = Transform.GetLocation();
    Result.Center = FVector2D(WorldLocation.X, WorldLocation.Y);
    const FVector WorldAxis3D = Transform.GetRotation().RotateVector(FVector::ForwardVector);
    Result.Axis = FVector2D(WorldAxis3D.X, WorldAxis3D.Y).GetSafeNormal();
    if (Result.Axis.IsNearlyZero()) Result.Axis = FVector2D(1.f, 0.f);
    Result.HalfLength = FMath::Max(1.f, Piece ? Piece->SnapSize * 0.5f : 1.f);
    ARPGGetWallStructuralWorldZRange(Piece, Transform, Result.MinZ, Result.MaxZ);
    return Result;
}

static float ARPGCross2D(const FVector2D& A, const FVector2D& B)
{
    return A.X * B.Y - A.Y * B.X;
}

/**
 * Classify two Wall-family actors by the structural slots they occupy rather than by their art/physics
 * hulls. A wall slot is a grid-edge line segment inside one vertical story bay. Two slots are compatible
 * when they only meet at a shared endpoint (an L-corner), are collinear neighbours that only meet at an
 * endpoint, or are vertically stacked and only meet at the story boundary. The same edge in the same
 * story is a duplicate/conflict, and two perpendicular segments crossing through their interiors are a
 * real structural conflict.
 */
static EARPGStructuralOccupancyRelation ARPGClassifyWallWallStructuralOccupancy(
    const UARPGBuildPieceDefinition* IncomingPiece,
    const FTransform& IncomingFinal,
    const UARPGBuildPieceDefinition* NeighborPiece,
    const FTransform& NeighborTransform)
{
    if (!IncomingPiece || !NeighborPiece ||
        !ARPGIsWallLikeKind(IncomingPiece->PieceKind) || !ARPGIsWallLikeKind(NeighborPiece->PieceKind))
        return EARPGStructuralOccupancyRelation::NotApplicable;

    const FARPGWallStructuralSegment A = ARPGMakeWallStructuralSegment(IncomingPiece, IncomingFinal);
    const FARPGWallStructuralSegment B = ARPGMakeWallStructuralSegment(NeighborPiece, NeighborTransform);
    const float Tolerance = FMath::Max(
        1.f,
        FMath::Max(IncomingPiece->PlacementCollisionClearance, NeighborPiece->PlacementCollisionClearance) + 0.5f);

    const float VerticalOverlap = FMath::Min(A.MaxZ, B.MaxZ) - FMath::Max(A.MinZ, B.MinZ);
    if (VerticalOverlap <= Tolerance)
        return EARPGStructuralOccupancyRelation::NotApplicable;

    const FVector2D Delta = B.Center - A.Center;
    const float AxisDot = FVector2D::DotProduct(A.Axis, B.Axis);
    const float AbsAxisDot = FMath::Abs(AxisDot);

    // Same structural run axis (front/back reversal is deliberately equivalent).
    if (AbsAxisDot >= 0.999f)
    {
        const FVector2D Normal(-A.Axis.Y, A.Axis.X);
        const float Across = FMath::Abs(FVector2D::DotProduct(Delta, Normal));
        if (Across > Tolerance)
            return EARPGStructuralOccupancyRelation::NotApplicable;

        const float Along = FMath::Abs(FVector2D::DotProduct(Delta, A.Axis));
        const float EndToEnd = A.HalfLength + B.HalfLength;
        if (Along <= Tolerance)
            return EARPGStructuralOccupancyRelation::Conflict; // same edge / same story slot
        if (FMath::Abs(Along - EndToEnd) <= Tolerance)
            return EARPGStructuralOccupancyRelation::CompatibleSeam; // exact endpoint continuation
        if (Along < EndToEnd - Tolerance)
            return EARPGStructuralOccupancyRelation::Conflict; // collinear penetration
        return EARPGStructuralOccupancyRelation::NotApplicable; // separated slots; OBB fallback decides
    }

    // Canonical modular corners are perpendicular. Find the infinite-line intersection and then check
    // whether it lies at the endpoint of both finite wall segments. Endpoint/end-point is a valid corner;
    // an interior crossing is a true conflict. Non-intersecting segments are unrelated compatible slots.
    const float Denominator = ARPGCross2D(A.Axis, B.Axis);
    if (FMath::Abs(Denominator) <= KINDA_SMALL_NUMBER)
        return EARPGStructuralOccupancyRelation::NotApplicable;

    const float TA = ARPGCross2D(Delta, B.Axis) / Denominator;
    const float TB = ARPGCross2D(Delta, A.Axis) / Denominator;
    if (FMath::Abs(TA) > A.HalfLength + Tolerance || FMath::Abs(TB) > B.HalfLength + Tolerance)
        return EARPGStructuralOccupancyRelation::NotApplicable;

    const bool bAtAEndpoint = FMath::Abs(FMath::Abs(TA) - A.HalfLength) <= Tolerance;
    const bool bAtBEndpoint = FMath::Abs(FMath::Abs(TB) - B.HalfLength) <= Tolerance;
    return (bAtAEndpoint && bAtBEndpoint)
        ? EARPGStructuralOccupancyRelation::CompatibleSeam
        : EARPGStructuralOccupancyRelation::Conflict;
}

/**
 * Classify Wall-family versus horizontal structural occupancy symmetrically. A wall on a tile edge is
 * a legitimate structural seam when the horizontal slab meets either the wall's bottom or top story
 * boundary (including the framework's deliberate slab-thickness overlap for pre-stacked walls). A wall
 * through the tile interior is a conflict. This rule is independent of which actor supplied SnapTarget.
 */
static EARPGStructuralOccupancyRelation ARPGClassifyWallHorizontalStructuralOccupancy(
    const UARPGBuildPieceDefinition* WallPiece,
    const FTransform& WallTransform,
    const UARPGBuildPieceDefinition* HorizontalPiece,
    const FTransform& HorizontalTransform)
{
    if (!WallPiece || !HorizontalPiece || !ARPGIsWallLikeKind(WallPiece->PieceKind) ||
        !ARPGIsHorizontalStructuralKind(HorizontalPiece->PieceKind))
        return EARPGStructuralOccupancyRelation::NotApplicable;

    const float Tolerance = FMath::Max(
        1.f,
        FMath::Max(WallPiece->PlacementCollisionClearance, HorizontalPiece->PlacementCollisionClearance) + 0.5f);
    const float HalfGrid = FMath::Max(1.f, HorizontalPiece->SnapSize) * 0.5f;
    if (!ARPGWallOccupiesHorizontalStructuralEdge(
        HorizontalTransform,
        HalfGrid,
        WallTransform.GetLocation(),
        WallTransform.Rotator().Yaw,
        Tolerance))
        return EARPGStructuralOccupancyRelation::NotApplicable;

    float WallMinZ, WallMaxZ, HorizontalMinZ, HorizontalMaxZ;
    ARPGGetWallStructuralWorldZRange(WallPiece, WallTransform, WallMinZ, WallMaxZ);
    ARPGGetDefinitionWorldZRange(HorizontalPiece, HorizontalTransform, HorizontalMinZ, HorizontalMaxZ);

    const float VerticalOverlap = FMath::Min(WallMaxZ, HorizontalMaxZ) - FMath::Max(WallMinZ, HorizontalMinZ);
    if (VerticalOverlap <= Tolerance)
        return EARPGStructuralOccupancyRelation::CompatibleSeam;

    const bool bWallBoundaryAtSlabBoundary =
        FMath::Abs(WallMinZ - HorizontalMinZ) <= Tolerance ||
        FMath::Abs(WallMinZ - HorizontalMaxZ) <= Tolerance ||
        FMath::Abs(WallMaxZ - HorizontalMinZ) <= Tolerance ||
        FMath::Abs(WallMaxZ - HorizontalMaxZ) <= Tolerance;
    if (bWallBoundaryAtSlabBoundary)
        return EARPGStructuralOccupancyRelation::CompatibleSeam;

    return EARPGStructuralOccupancyRelation::Conflict;
}

static EARPGStructuralOccupancyRelation ARPGClassifyStandardStructuralOccupancy(
    const AARPGBuildPieceActor* Neighbor,
    const UARPGBuildPieceDefinition* IncomingPiece,
    const FTransform& IncomingFinal)
{
    if (!Neighbor || !Neighbor->Definition || !IncomingPiece || !Neighbor->IsConstructionComplete())
        return EARPGStructuralOccupancyRelation::NotApplicable;

    const UARPGBuildPieceDefinition* NeighborPiece = Neighbor->Definition;
    const FTransform NeighborTransform = Neighbor->GetActorTransform();

    if (ARPGIsWallLikeKind(IncomingPiece->PieceKind) && ARPGIsWallLikeKind(NeighborPiece->PieceKind))
        return ARPGClassifyWallWallStructuralOccupancy(IncomingPiece, IncomingFinal, NeighborPiece, NeighborTransform);

    if (ARPGIsWallLikeKind(IncomingPiece->PieceKind) && ARPGIsHorizontalStructuralKind(NeighborPiece->PieceKind))
        return ARPGClassifyWallHorizontalStructuralOccupancy(IncomingPiece, IncomingFinal, NeighborPiece, NeighborTransform);

    if (ARPGIsHorizontalStructuralKind(IncomingPiece->PieceKind) && ARPGIsWallLikeKind(NeighborPiece->PieceKind))
        return ARPGClassifyWallHorizontalStructuralOccupancy(NeighborPiece, NeighborTransform, IncomingPiece, IncomingFinal);

    return EARPGStructuralOccupancyRelation::NotApplicable;
}

/**
 * Door/Window inserts occupy a designated Wall-family host opening. Generic build-vs-build OBB
 * validation must not reject the insert merely because the Floor/Ceiling/adjacent Wall framing that
 * legitimately connects to that host touches the insert's authored bounds. Validate those surrounding
 * structures against the host's own semantic structural slot instead. A duplicate/conflicting wall
 * slot is deliberately not accepted, and duplicate inserts are blocked by explicit semantic host occupancy even when an open insert has moved or disabled gameplay collision.
 */
static bool ARPGIsCompatibleInsertHostStructuralNeighbor(
    const AARPGBuildPieceActor* Neighbor,
    const AARPGBuildPieceActor* InsertHost,
    const UARPGBuildPieceDefinition* IncomingPiece)
{
    if (!Neighbor || !Neighbor->Definition || !InsertHost || !InsertHost->Definition || !IncomingPiece) return false;
    if (!ARPGIsInsertSnapPair(InsertHost->Definition->PieceKind, IncomingPiece->PieceKind)) return false;
    if (!Neighbor->IsConstructionComplete() || !InsertHost->IsConstructionComplete()) return false;

    const EARPGBuildPieceKind NeighborKind = Neighbor->Definition->PieceKind;
    if (!ARPGIsWallLikeKind(NeighborKind) && !ARPGIsHorizontalStructuralKind(NeighborKind)) return false;

    return ARPGClassifyStandardStructuralOccupancy(
        Neighbor,
        InsertHost->Definition,
        InsertHost->GetActorTransform()) == EARPGStructuralOccupancyRelation::CompatibleSeam;
}

/**
 * Reverse hosted-insert validation for structural placement. A completed Door/Window is gameplay
 * content *inside* a Doorway/WindowWall structural host; it must not become an independent blocker
 * when the player later closes a legitimate Wall/Floor/Ceiling/Roof seam around that host.
 *
 * Host identity is verified from the host's own native insert snap transform instead of actor class,
 * distance alone, or a global ignore flag. This keeps duplicate inserts and unrelated geometry blocked.
 */
static bool ARPGInsertActorMatchesHost(
    const AARPGBuildPieceActor* InsertActor,
    const AARPGBuildPieceActor* CandidateHost)
{
    if (!InsertActor || !InsertActor->Definition || !CandidateHost || !CandidateHost->Definition) return false;
    if (!InsertActor->IsConstructionComplete() || !CandidateHost->IsConstructionComplete()) return false;
    if (!ARPGIsInsertSnapPair(CandidateHost->Definition->PieceKind, InsertActor->Definition->PieceKind)) return false;

    const float Tolerance = FMath::Max(
        1.f,
        FMath::Max(InsertActor->Definition->PlacementCollisionClearance, CandidateHost->Definition->PlacementCollisionClearance) + 0.5f);
    const float ToleranceSq = FMath::Square(Tolerance);

    TArray<FTransform> InsertCandidates;
    CandidateHost->GetSnapTransformsFor(InsertActor->Definition, InsertCandidates);
    for (const FTransform& Candidate : InsertCandidates)
    {
        if (FVector::DistSquared(Candidate.GetLocation(), InsertActor->GetActorLocation()) > ToleranceSq) continue;
        const float YawDelta = FMath::Abs(FMath::FindDeltaAngleDegrees(
            Candidate.Rotator().Yaw, InsertActor->GetActorRotation().Yaw));
        if (YawDelta <= 1.f) return true;
    }
    return false;
}

static bool ARPGHostedInsertAllowsStructuralNeighbor(
    const AARPGBuildPieceActor* InsertActor,
    const AARPGBuildPieceActor* InsertHost,
    const UARPGBuildPieceDefinition* IncomingPiece,
    const FTransform& IncomingFinal)
{
    if (!InsertActor || !InsertHost || !IncomingPiece) return false;
    if (!ARPGIsWallLikeKind(IncomingPiece->PieceKind) && !ARPGIsHorizontalStructuralKind(IncomingPiece->PieceKind)) return false;
    if (!ARPGInsertActorMatchesHost(InsertActor, InsertHost)) return false;

    // Prefer the host's authored/native socket contract when it advertises this exact structural
    // placement. For square horizontal tiles, cardinal yaw variants are physically equivalent.
    TArray<FTransform> HostStructuralCandidates;
    InsertHost->GetSnapTransformsFor(IncomingPiece, HostStructuralCandidates);
    const float PositionTolerance = FMath::Max(1.f, IncomingPiece->PlacementCollisionClearance + 0.5f);
    const float PositionToleranceSq = FMath::Square(PositionTolerance);
    for (const FTransform& HostCandidate : HostStructuralCandidates)
    {
        if (FVector::DistSquared(HostCandidate.GetLocation(), IncomingFinal.GetLocation()) > PositionToleranceSq) continue;
        if (ARPGRotationsPreservePlacementFootprint(
            IncomingPiece, HostCandidate.GetRotation(), IncomingFinal.GetRotation()))
            return true;
    }

    // Native sockets are intentionally not exhaustive for inverse build orders, so semantic grid
    // occupancy is the authoritative fallback for a valid host seam.
    return ARPGClassifyStandardStructuralOccupancy(
        InsertHost, IncomingPiece, IncomingFinal) == EARPGStructuralOccupancyRelation::CompatibleSeam;
}

static bool ARPGYawAxesEquivalent(float A, float B)
{
    const float Delta = FMath::Abs(FMath::FindDeltaAngleDegrees(A, B));
    constexpr float AxisToleranceDegrees = 1.f;
    return Delta <= AxisToleranceDegrees || FMath::Abs(Delta - 180.f) <= AxisToleranceDegrees;
}

static bool ARPGTransformMatchesStairHostCandidate(
    const AARPGBuildPieceActor* Host,
    const UARPGBuildPieceDefinition* IncomingPiece,
    const FTransform& IncomingTransform)
{
    if (!Host || !Host->Definition || !IncomingPiece || !Host->IsConstructionComplete()) return false;

    TArray<FTransform> Candidates;
    Host->GetSnapTransformsFor(IncomingPiece, Candidates);
    const float PositionTolerance = FMath::Max(1.f, IncomingPiece->PlacementCollisionClearance + 0.5f);
    const float PositionToleranceSq = FMath::Square(PositionTolerance);
    for (const FTransform& Candidate : Candidates)
    {
        if (FVector::DistSquared(Candidate.GetLocation(), IncomingTransform.GetLocation()) > PositionToleranceSq) continue;
        const float YawDelta = FMath::Abs(FMath::FindDeltaAngleDegrees(
            Candidate.Rotator().Yaw, IncomingTransform.Rotator().Yaw));
        if (YawDelta <= 1.f) return true;
    }
    return false;
}

/**
 * One authoritative bidirectional Stair <-> Wall-family boundary-seam classifier.
 *
 * The current settlement kit intentionally lets Wall-family modules frame a Stair flight on any exact
 * structural boundary of its 300 cm cell. That includes BOTH:
 *   - longitudinal side seams: Wall/WindowWall/Doorway run parallel to the Stair and their actor snap
 *     origin sits on local Y = +/- SnapSize/2; and
 *   - LOW/HIGH endpoint seams: Wall-family run is perpendicular to the Stair and its actor snap origin
 *     sits on local X = +/- SnapSize/2.
 *
 * v2.15.52 only modeled the first case. Three cardinal rotations happened to present side-seam geometry
 * in the reported layouts, while the remaining rotation presented the same authored perimeter module as
 * an endpoint seam and fell through to generic collision as `Blocked by another object`.
 *
 * This helper deliberately classifies structural boundary ownership rather than visible mesh facing:
 *   - Stair local +X is the travel/run axis after the final snap transform;
 *   - Wall-family ACTOR SNAP ORIGIN remains authoritative (never mesh/bounds centre);
 *   - side seams require parallel run axes and exact local +/-Y half-grid ownership;
 *   - endpoint seams require perpendicular run axes and exact local +/-X half-grid ownership;
 *   - the Wall-family SnapSize segment must actually overlap the Stair authored occupancy on the other
 *     horizontal axis, including the Wood Stair's intentional 334 cm longitudinal envelope and 300 cm
 *     width;
 *   - centreline/interior walls, distant modules and unrelated collision remain blockers.
 *
 * The same predicate is consumed in both build orders and by verified hosted Window/Door inheritance,
 * so all four 90-degree Stair rotations receive identical semantics without a global collision bypass.
 */
static bool ARPGIsStairWallFamilyBoundarySeam(
    const UARPGBuildPieceDefinition* StairPiece,
    const FTransform& StairTransform,
    const UARPGBuildPieceDefinition* WallPiece,
    const FTransform& WallTransform)
{
    if (!StairPiece || !WallPiece) return false;
    if (StairPiece->PieceKind != EARPGBuildPieceKind::Stair || !ARPGIsWallLikeKind(WallPiece->PieceKind)) return false;

    const float AxisDelta = FMath::Abs(FMath::FindDeltaAngleDegrees(
        StairTransform.Rotator().Yaw, WallTransform.Rotator().Yaw));
    constexpr float AxisToleranceDegrees = 1.f;
    const bool bParallelToStairRun =
        AxisDelta <= AxisToleranceDegrees || FMath::Abs(AxisDelta - 180.f) <= AxisToleranceDegrees;
    const bool bPerpendicularToStairRun =
        FMath::Abs(AxisDelta - 90.f) <= AxisToleranceDegrees;
    if (!bParallelToStairRun && !bPerpendicularToStairRun) return false;

    const float PositionTolerance = FMath::Max(
        1.f,
        FMath::Max(StairPiece->PlacementCollisionClearance, WallPiece->PlacementCollisionClearance) + 0.5f);

    FVector StairMin, StairMax;
    ARPGGetBuildPieceLocalBounds(StairPiece, StairMin, StairMax);
    const FVector StairBoundsCenterLocal = (StairMin + StairMax) * 0.5f;
    const FVector StairCenterWorld = StairTransform.TransformPosition(StairBoundsCenterLocal);
    const FTransform StairFrame(StairTransform.GetRotation(), StairCenterWorld);

    // Wall-family structural anchors are actor origins. Never substitute transformed mesh/bounds
    // centres here: imported wall pivots may be offset by MeshRelativeTransform while their snap
    // origins still sit exactly on the authored 300 cm grid boundary.
    const FVector WallAnchorInStair = StairFrame.InverseTransformPosition(WallTransform.GetLocation());

    const float StairGrid = FMath::Max(1.f, StairPiece->SnapSize);
    const float HalfGrid = StairGrid * 0.5f;
    const float WallHalfRun = FMath::Max(
        1.f,
        WallPiece->SnapSize * 0.5f * FMath::Abs(WallTransform.GetScale3D().X));

    // The authored Stair occupancy is the collision-semantic envelope. Its intentional endpoint art/validation overhang
    // is preserved: X may intentionally overhang
    // the structural cell (Wood = 167 cm vs 150 cm), while Y is the full Stair width (Wood = 150 cm).
    const float StairHalfRun = FMath::Max(
        1.f,
        StairPiece->PlacementBounds.X * FMath::Abs(StairTransform.GetScale3D().X));
    const float StairHalfWidth = FMath::Max(
        1.f,
        StairPiece->PlacementBounds.Y * FMath::Abs(StairTransform.GetScale3D().Y));

    if (bParallelToStairRun)
    {
        const bool bOnSidePlane =
            FMath::Abs(FMath::Abs(WallAnchorInStair.Y) - HalfGrid) <= PositionTolerance;
        if (!bOnSidePlane) return false;

        const float WallRunMin = WallAnchorInStair.X - WallHalfRun;
        const float WallRunMax = WallAnchorInStair.X + WallHalfRun;
        const float LongitudinalOverlap =
            FMath::Min(WallRunMax, StairHalfRun) - FMath::Max(WallRunMin, -StairHalfRun);
        return LongitudinalOverlap > PositionTolerance;
    }

    // Cardinal endpoint framing is the rotation that v2.15.52 still rejected. The wall must own the
    // exact LOW/HIGH structural boundary plane; a perpendicular wall through the middle of the flight
    // remains a conflict. Its structural run must also overlap the Stair width rather than merely being
    // somewhere on the same infinite endpoint line.
    const bool bOnEndpointPlane =
        FMath::Abs(FMath::Abs(WallAnchorInStair.X) - HalfGrid) <= PositionTolerance;
    if (!bOnEndpointPlane) return false;

    const float WallAcrossMin = WallAnchorInStair.Y - WallHalfRun;
    const float WallAcrossMax = WallAnchorInStair.Y + WallHalfRun;
    const float LateralOverlap =
        FMath::Min(WallAcrossMax, StairHalfWidth) - FMath::Max(WallAcrossMin, -StairHalfWidth);
    return LateralOverlap > PositionTolerance;
}

/**
 * Structural neighbours around an incoming snapped Stair. Horizontal landing cells keep their strict
 * host-relative rules. Wall-family neighbours use the shared bidirectional boundary-seam classifier and
 * therefore work identically whether the Stair is snapped to a flat landing OR chained from another
 * Stair. The final Stair transform must still be one of the active host's authoritative native sockets.
 */
static bool ARPGIsCompatibleStairHostStructuralNeighbor(
    const AARPGBuildPieceActor* Neighbor,
    const AARPGBuildPieceActor* StairHost,
    const UARPGBuildPieceDefinition* StairPiece,
    const FTransform& StairFinal)
{
    if (!Neighbor || !Neighbor->Definition || !StairHost || !StairHost->Definition || !StairPiece) return false;
    if (!Neighbor->IsConstructionComplete() || !StairHost->IsConstructionComplete()) return false;
    if (!ARPGIsAnyStairSnapPair(StairHost->Definition->PieceKind, StairPiece->PieceKind)) return false;
    if (!ARPGTransformMatchesStairHostCandidate(StairHost, StairPiece, StairFinal)) return false;

    const EARPGBuildPieceKind NeighborKind = Neighbor->Definition->PieceKind;
    if (ARPGIsWallLikeKind(NeighborKind))
    {
        return ARPGIsStairWallFamilyBoundarySeam(
            StairPiece,
            StairFinal,
            Neighbor->Definition,
            Neighbor->GetActorTransform());
    }

    // Horizontal neighbour classification is meaningful only when the Stair itself is snapped to a
    // flat Foundation/Floor/Ceiling host. Stair-chain placement is validated by its endpoint socket and
    // must not reinterpret arbitrary deck cells using the chain Stair as a flat-cell origin.
    if (!ARPGIsStairSupportSnapPair(StairHost->Definition->PieceKind, StairPiece->PieceKind)) return false;

    const bool bNeighborFlatLanding =
        NeighborKind == EARPGBuildPieceKind::Foundation ||
        NeighborKind == EARPGBuildPieceKind::Floor ||
        NeighborKind == EARPGBuildPieceKind::Ceiling;
    if (!bNeighborFlatLanding) return false;

    const float PositionTolerance = FMath::Max(
        1.f,
        FMath::Max(StairPiece->PlacementCollisionClearance, Neighbor->Definition->PlacementCollisionClearance) + 0.5f);

    FVector StairMin, StairMax;
    ARPGGetBuildPieceLocalBounds(StairPiece, StairMin, StairMax);
    const FVector StairCenter = (StairMin + StairMax) * 0.5f;

    const FVector LowEndWorld = StairFinal.TransformPosition(FVector(StairMin.X, StairCenter.Y, StairMin.Z));
    const FVector HighEndWorld = StairFinal.TransformPosition(FVector(StairMax.X, StairCenter.Y, StairMax.Z));
    const FTransform HostTransform = StairHost->GetActorTransform();
    const FVector LowEndHostLocal = HostTransform.InverseTransformPosition(LowEndWorld);
    const FVector HighEndHostLocal = HostTransform.InverseTransformPosition(HighEndWorld);

    const float Grid = FMath::Max(1.f, StairHost->Definition->SnapSize);
    const float HalfGrid = Grid * 0.5f;
    const auto EdgeCenterErrorSq = [HalfGrid](const FVector& P)
    {
        const float PosX = FMath::Square(P.X - HalfGrid) + FMath::Square(P.Y);
        const float NegX = FMath::Square(P.X + HalfGrid) + FMath::Square(P.Y);
        const float PosY = FMath::Square(P.X) + FMath::Square(P.Y - HalfGrid);
        const float NegY = FMath::Square(P.X) + FMath::Square(P.Y + HalfGrid);
        return FMath::Min(FMath::Min(PosX, NegX), FMath::Min(PosY, NegY));
    };

    const bool bLowEndOwnsHostEdge = EdgeCenterErrorSq(LowEndHostLocal) <= EdgeCenterErrorSq(HighEndHostLocal);
    const FVector SharedEndpointHostLocal = bLowEndOwnsHostEdge ? LowEndHostLocal : HighEndHostLocal;
    FVector EdgeDirectionLocal(SharedEndpointHostLocal.X, SharedEndpointHostLocal.Y, 0.f);
    if (!EdgeDirectionLocal.Normalize()) return false;
    FVector EdgeDirectionWorld = HostTransform.TransformVectorNoScale(EdgeDirectionLocal);
    EdgeDirectionWorld.Z = 0.f;
    if (!EdgeDirectionWorld.Normalize()) return false;

    const FVector StairCellCenter = bLowEndOwnsHostEdge
        ? StairHost->GetActorLocation()
        : StairHost->GetActorLocation() + EdgeDirectionWorld * Grid;

    float HostMinZ = 0.f, HostMaxZ = 0.f, NeighborMinZ = 0.f, NeighborMaxZ = 0.f;
    ARPGGetDefinitionWorldZRange(StairHost->Definition, StairHost->GetActorTransform(), HostMinZ, HostMaxZ);
    ARPGGetDefinitionWorldZRange(Neighbor->Definition, Neighbor->GetActorTransform(), NeighborMinZ, NeighborMaxZ);

    const float HostStoryPlane = HostMaxZ;
    const float NeighborStoryPlane = NeighborMaxZ;
    if (FMath::Abs(NeighborStoryPlane - HostStoryPlane) > PositionTolerance) return false;

    FVector ToNeighbor = Neighbor->GetActorLocation() - StairCellCenter;
    ToNeighbor.Z = 0.f;
    if (ToNeighbor.SizeSquared() <= FMath::Square(PositionTolerance))
    {
        // A second horizontal module occupying the actual Stair flight cell closes the travel volume.
        return false;
    }

    const FTransform StairCellFrame(StairFinal.GetRotation(), StairCellCenter);
    const FVector NeighborInCell = StairCellFrame.InverseTransformPosition(Neighbor->GetActorLocation());
    const float AbsX = FMath::Abs(NeighborInCell.X);
    const float AbsY = FMath::Abs(NeighborInCell.Y);
    const bool bImmediateGridNeighbor =
        (FMath::Abs(AbsX - Grid) <= PositionTolerance && AbsY <= PositionTolerance) ||
        (FMath::Abs(AbsY - Grid) <= PositionTolerance && AbsX <= PositionTolerance);

    // If the broad Stair profile touched one immediate same-story deck cell but that cell is not the
    // actual Stair flight cell, the contact is only the intentional 17 cm art/rail overhang at a
    // modular seam. Keep the established tiled-deck acceptance unchanged.
    return bImmediateGridNeighbor;
}

/**
 * Hosted Window/Door inserts inherit the exact Stair-boundary permission of their verified structural
 * WindowWall/Doorway host. This uses the same shared seam classifier and works for flat-support and
 * Stair-chain placement; no insert-specific collision weakening is involved.
 */
static bool ARPGHostedInsertAllowsStairSideNeighbor(
    const AARPGBuildPieceActor* InsertActor,
    const AARPGBuildPieceActor* InsertHost,
    const AARPGBuildPieceActor* StairSnapTarget,
    const UARPGBuildPieceDefinition* StairPiece,
    const FTransform& StairFinal)
{
    if (!InsertActor || !InsertHost || !StairSnapTarget || !StairPiece) return false;
    if (StairPiece->PieceKind != EARPGBuildPieceKind::Stair) return false;
    if (!ARPGInsertActorMatchesHost(InsertActor, InsertHost)) return false;
    if (!StairSnapTarget->Definition ||
        !ARPGIsAnyStairSnapPair(StairSnapTarget->Definition->PieceKind, StairPiece->PieceKind) ||
        !ARPGTransformMatchesStairHostCandidate(StairSnapTarget, StairPiece, StairFinal))
        return false;

    return ARPGIsStairWallFamilyBoundarySeam(
        StairPiece,
        StairFinal,
        InsertHost->Definition,
        InsertHost->GetActorTransform());
}

/**
 * Reverse build order wrapper: an existing Stair and an incoming Wall/WindowWall/Doorway are judged by
 * the exact same side-seam invariant as incoming Stair placement. This is intentionally a thin wrapper
 * so future topology changes cannot diverge by build order again.
 */
static bool ARPGIsValidExistingStairWallSideSeamNeighbor(
    const AARPGBuildPieceActor* ExistingStair,
    const UARPGBuildPieceDefinition* IncomingPiece,
    const FTransform& IncomingFinal)
{
    if (!ExistingStair || !ExistingStair->Definition || !IncomingPiece || !ExistingStair->IsConstructionComplete()) return false;
    return ARPGIsStairWallFamilyBoundarySeam(
        ExistingStair->Definition,
        ExistingStair->GetActorTransform(),
        IncomingPiece,
        IncomingFinal);
}

/**
 * Incoming hosted inserts also inherit the Stair-side relationship of their WindowWall/Doorway host.
 * This is the inverse of ARPGHostedInsertAllowsStairSideNeighbor(): when the Stair already exists and
 * the player later installs a Window/Door, the insert's own authored bounds/collision must not veto a
 * host seam that is already structurally legal. Host identity remains exact through the active
 * WindowWall/Doorway SnapTarget; unrelated Stairs or free inserts are never ignored.
 */
static bool ARPGIsCompatibleInsertHostStairNeighbor(
    const AARPGBuildPieceActor* ExistingStair,
    const AARPGBuildPieceActor* InsertHost,
    const UARPGBuildPieceDefinition* IncomingInsert)
{
    if (!ExistingStair || !ExistingStair->Definition || !InsertHost || !InsertHost->Definition || !IncomingInsert) return false;
    if (!ExistingStair->IsConstructionComplete() || !InsertHost->IsConstructionComplete()) return false;
    if (ExistingStair->Definition->PieceKind != EARPGBuildPieceKind::Stair) return false;
    if (!ARPGIsInsertSnapPair(InsertHost->Definition->PieceKind, IncomingInsert->PieceKind)) return false;

    return ARPGIsStairWallFamilyBoundarySeam(
        ExistingStair->Definition,
        ExistingStair->GetActorTransform(),
        InsertHost->Definition,
        InsertHost->GetActorTransform());
}

/**
 * A snapped edge-landing Stair may legitimately be supported by continuous WorldStatic geometry
 * underneath more than just its very first profile slice. A Landscape is one actor spanning the whole
 * area, so rejecting that actor merely because it appears beneath slice 1/2/etc. makes a correctly
 * snapped Stair report `Blocked by another object` even though the terrain never rises into the Stair.
 *
 * Validate the actual support relationship per overlapping Stair slice instead:
 *   - the Stair must still exactly match a verified horizontal-landing or Stair-chain socket;
 *   - for every Stair profile slice touched by this non-build actor, sample the actor's blocking surface
 *     vertically at the slice centre and both lateral quarters;
 *   - the actor is support only when at least one sample hits that exact actor and every hit stays at or
 *     below the slice underside within the authored placement-clearance tolerance;
 *   - if the actor rises into the Stair profile at any sampled point, or overlaps only from the side with
 *     no supporting surface beneath the slice, it remains a real blocker.
 *
 * This keeps terrain/foundation approaches buildable without ignoring rocks, cliffs or world geometry
 * that actually penetrates the Stair travel/body profile.
 */
static bool ARPGIsValidStairWorldSupportContact(
    UWorld* World,
    AActor* WorldActor,
    const AARPGBuildPieceActor* StairHost,
    const UARPGBuildPieceDefinition* StairPiece,
    const FTransform& StairFinal,
    ECollisionChannel CollisionChannel,
    const FCollisionQueryParams& QueryParams)
{
    if (!World || !WorldActor || !StairHost || !StairPiece) return false;
    if (WorldActor == StairHost || Cast<AARPGBuildPieceActor>(WorldActor) || WorldActor->IsA<APawn>()) return false;
    if (!ARPGIsAnyStairSnapPair(
            StairHost->Definition ? StairHost->Definition->PieceKind : EARPGBuildPieceKind::Custom,
            StairPiece->PieceKind))
        return false;
    if (!ARPGTransformMatchesStairHostCandidate(StairHost, StairPiece, StairFinal)) return false;

    TArray<FARPGPlacementOBB> StairVolumes;
    ARPGBuildPlacementOccupancyOBBs(StairPiece, StairFinal, StairVolumes);
    if (StairVolumes.Num() == 0) return false;

    const float Clearance = FMath::Max(0.f, StairPiece->PlacementCollisionClearance);
    const float SurfaceTolerance = FMath::Max(2.f, Clearance + 1.f);
    const float TracePadding = FMath::Max(8.f, Clearance + 6.f);
    bool bTouchedThisActor = false;

    for (const FARPGPlacementOBB& Volume : StairVolumes)
    {
        TArray<FOverlapResult> SliceOverlaps;
        World->OverlapMultiByChannel(
            SliceOverlaps,
            Volume.Center,
            Volume.Rotation,
            CollisionChannel,
            FCollisionShape::MakeBox(Volume.Extents),
            QueryParams);

        bool bActorTouchesSlice = false;
        for (const FOverlapResult& SliceOverlap : SliceOverlaps)
        {
            if (SliceOverlap.GetActor() == WorldActor)
            {
                bActorTouchesSlice = true;
                bTouchedThisActor = true;
                break;
            }
        }
        if (!bActorTouchesSlice) continue;

        // Sample the support surface underneath this exact slice. Axis[2] is the Stair's local up
        // transformed to world space; current standard Stair sockets are yaw-only, but using the OBB
        // axis keeps this correct if authored transforms later introduce a rotated local frame.
        const FVector Up = Volume.Axis[2].GetSafeNormal();
        const FVector Side = Volume.Axis[1].GetSafeNormal();
        const FVector SliceBottomCenter = Volume.Center - Up * Volume.Extents.Z;
        const float LateralSample = Volume.Extents.Y * 0.6f;
        const float SideOffsets[] = { -LateralSample, 0.f, LateralSample };

        bool bFoundSupportingSurface = false;
        for (const float SideOffset : SideOffsets)
        {
            const FVector BottomSample = SliceBottomCenter + Side * SideOffset;
            const FVector TraceStart = BottomSample + Up * (Volume.Extents.Z * 2.f + TracePadding);
            const FVector TraceEnd = BottomSample - Up * TracePadding;

            FHitResult SupportHit;
            if (!World->LineTraceSingleByChannel(
                    SupportHit,
                    TraceStart,
                    TraceEnd,
                    CollisionChannel,
                    QueryParams))
                continue;
            if (SupportHit.GetActor() != WorldActor) continue;

            bFoundSupportingSurface = true;
            const float SurfaceAboveSliceBottom = FVector::DotProduct(
                SupportHit.ImpactPoint - BottomSample,
                Up);

            // A supporting surface may be flush with/slightly embedded into the underside seam, but
            // once it rises materially into the Stair occupancy profile it is an obstruction.
            if (SurfaceAboveSliceBottom > SurfaceTolerance)
                return false;
        }

        // The actor overlapped this slice but could not be proven as a surface underneath it. Treat a
        // side-only/overhead intersection conservatively as a real blocker.
        if (!bFoundSupportingSurface)
            return false;
    }

    return bTouchedThisActor;
}

/**
 * Collision validation cares about the wall's structural run axis, not which side of asymmetric wall
 * art is facing outward. A wall facing 180 degrees the other way still occupies the exact same edge.
 * Requiring canonical front/back facing here made otherwise valid rooms depend on build order because
 * wall-only corner/stack sockets legitimately preserve either facing variant.
 */
static bool ARPGWallOccupiesHorizontalStructuralEdge(
    const FTransform& HorizontalTransform,
    float HalfGrid,
    const FVector& WallWorldLocation,
    float WallWorldYaw,
    float PositionTolerance)
{
    const float PositionToleranceSq = FMath::Square(PositionTolerance);
    const FVector WallOriginLocal = HorizontalTransform.InverseTransformPosition(WallWorldLocation);

    struct FARPGBuildEdgeAxis
    {
        FVector2D LocalCenter;
        float TangentRelativeYaw;
    };

    const FARPGBuildEdgeAxis EdgeAxes[] =
    {
        { FVector2D(0.f,  HalfGrid),  0.f },
        { FVector2D(0.f, -HalfGrid),  0.f },
        { FVector2D( HalfGrid, 0.f), 90.f },
        { FVector2D(-HalfGrid, 0.f), 90.f }
    };

    for (const FARPGBuildEdgeAxis& Edge : EdgeAxes)
    {
        const FVector2D Delta(WallOriginLocal.X - Edge.LocalCenter.X, WallOriginLocal.Y - Edge.LocalCenter.Y);
        if (Delta.SizeSquared() > PositionToleranceSq) continue;

        const float ExpectedWorldAxisYaw = HorizontalTransform.Rotator().Yaw + Edge.TangentRelativeYaw;
        if (ARPGYawAxesEquivalent(ExpectedWorldAxisYaw, WallWorldYaw)) return true;
    }

    return false;
}

/**
 * A Floor/Ceiling/Roof can be inserted at an already-built story boundary after the player has
 * vertically stacked the next row of Wall-family pieces. The horizontal slab's finished TOP is the
 * canonical story plane shared by the lower wall structural top and the upper wall bottom; the slab
 * extends downward into the lower wall frame/post. That is a legitimate
 * modular inter-story seam, not arbitrary clipping.
 *
 * Accept this overlap only when all three structural facts are true:
 *   - the Wall/WindowWall/Doorway actor origin is on one of the horizontal tile's four grid edges;
 *   - its wall-run axis is tangent to that edge (front/back reversal is structurally equivalent); and
 *   - its structural top/bottom aligns to the slab finished TOP story plane (support below or wall on top).
 *
 * This makes build order commutative without globally ignoring building collision. A wall through
 * the middle of the tile, a perpendicular wall axis, or a wall at an unrelated height remains blocked.
 */
static bool ARPGIsValidUpperHorizontalWallSeamNeighbor(
    const AARPGBuildPieceActor* Neighbor,
    const UARPGBuildPieceDefinition* HorizontalPiece,
    const FTransform& HorizontalFinal)
{
    if (!Neighbor || !Neighbor->Definition || !HorizontalPiece || !Neighbor->IsConstructionComplete()) return false;
    if (!ARPGIsUpperHorizontalStructuralKind(HorizontalPiece->PieceKind) ||
        !ARPGIsWallLikeKind(Neighbor->Definition->PieceKind)) return false;

    const float PositionTolerance = FMath::Max(1.f, HorizontalPiece->PlacementCollisionClearance + 0.5f);
    const float HalfGrid = FMath::Max(1.f, HorizontalPiece->SnapSize) * 0.5f;
    if (!ARPGWallOccupiesHorizontalStructuralEdge(
        HorizontalFinal,
        HalfGrid,
        Neighbor->GetActorLocation(),
        Neighbor->GetActorRotation().Yaw,
        PositionTolerance))
        return false;

    FVector HorizontalMin, HorizontalMax, WallMin, WallMax;
    ARPGGetBuildPieceLocalBounds(HorizontalPiece, HorizontalMin, HorizontalMax);
    ARPGGetBuildPieceLocalBounds(Neighbor->Definition, WallMin, WallMax);

    const FVector HorizontalCenter = (HorizontalMin + HorizontalMax) * 0.5f;
    const FVector WallCenter = (WallMin + WallMax) * 0.5f;
    const float HorizontalTopZ = HorizontalFinal.TransformPosition(
        FVector(HorizontalCenter.X, HorizontalCenter.Y, HorizontalMax.Z)).Z;
    const FTransform NeighborTransform = Neighbor->GetActorTransform();
    const float WallBottomZ = NeighborTransform.TransformPosition(FVector(WallCenter.X, WallCenter.Y, WallMin.Z)).Z;
    const float WallStructuralTopZ = WallBottomZ + FMath::Max(1.f, Neighbor->Definition->StandardWallHeight);

    const bool bSupportingWallBelow = FMath::Abs(WallStructuralTopZ - HorizontalTopZ) <= PositionTolerance;
    const bool bWallBuiltOnStoryPlane = FMath::Abs(WallBottomZ - HorizontalTopZ) <= PositionTolerance;

    return bSupportingWallBelow || bWallBuiltOnStoryPlane;
}


/**
 * Inverse inter-story seam for Wall-family placement. When a Wall/WindowWall/Doorway is snapped to
 * the top of a lower Floor/Ceiling/Foundation, another Floor/Ceiling/Roof may already exist exactly
 * one storey above it. Native horizontal -> Wall sockets describe walls erected on *top* of that
 * upper slab, so the slab cannot advertise the incoming wall whose visible top terminates at its
 * underside. Without this inverse relationship the collision pass reports `Blocked by another
 * object` even though the wall is filling the intended storey bay.
 *
 * Accept the upper horizontal neighbour only when:
 *   - the incoming Wall-family actor lies on one of that slab's exact four structural edges;
 *   - its wall-run axis is tangent to that edge (either front/back facing is valid occupancy); and
 *   - the incoming wall's structural story top meets the upper slab's finished TOP/story plane.
 *
 * This is deliberately one-way and exact. A wall crossing the tile center, perpendicular wall axis,
 * wall that extends through the slab, or slab at an unrelated height remains a collision blocker.
 */
static bool ARPGIsValidWallUnderUpperHorizontalSeamNeighbor(
    const AARPGBuildPieceActor* Neighbor,
    const UARPGBuildPieceDefinition* WallPiece,
    const FTransform& WallFinal)
{
    if (!Neighbor || !Neighbor->Definition || !WallPiece || !Neighbor->IsConstructionComplete()) return false;
    if (!ARPGIsWallLikeKind(WallPiece->PieceKind) ||
        !ARPGIsUpperHorizontalStructuralKind(Neighbor->Definition->PieceKind)) return false;

    const float PositionTolerance = FMath::Max(1.f, WallPiece->PlacementCollisionClearance + 0.5f);
    const float HalfGrid = FMath::Max(1.f, Neighbor->Definition->SnapSize) * 0.5f;
    const FTransform HorizontalTransform = Neighbor->GetActorTransform();
    if (!ARPGWallOccupiesHorizontalStructuralEdge(
        HorizontalTransform,
        HalfGrid,
        WallFinal.GetLocation(),
        WallFinal.Rotator().Yaw,
        PositionTolerance))
        return false;

    FVector WallMin, WallMax, HorizontalMin, HorizontalMax;
    ARPGGetBuildPieceLocalBounds(WallPiece, WallMin, WallMax);
    ARPGGetBuildPieceLocalBounds(Neighbor->Definition, HorizontalMin, HorizontalMax);

    const FVector WallCenter = (WallMin + WallMax) * 0.5f;
    const FVector HorizontalCenter = (HorizontalMin + HorizontalMax) * 0.5f;
    const float WallBottomZ = WallFinal.TransformPosition(FVector(WallCenter.X, WallCenter.Y, WallMin.Z)).Z;
    const float WallStructuralTopZ = WallBottomZ + FMath::Max(1.f, WallPiece->StandardWallHeight);
    const float HorizontalTopZ = HorizontalTransform.TransformPosition(
        FVector(HorizontalCenter.X, HorizontalCenter.Y, HorizontalMax.Z)).Z;

    return FMath::Abs(WallStructuralTopZ - HorizontalTopZ) <= PositionTolerance;
}

/**
 * Insert targeting must not inherit the ordinary placement trace's first hit. In third-person building
 * the camera ray commonly touches the supporting Foundation/Floor before reaching a Doorway opening;
 * truncating the semantic search at that hit makes a perfectly visible doorway impossible to acquire.
 *
 * Instead, Door/Window inserts scan the full placement view segment for compatible completed opening
 * actors, then perform a dedicated line-of-sight test to each opening center. This keeps acquisition
 * collision-independent for hollow/complex frame meshes while still preventing snapping through an
 * unrelated solid wall or world blocker.
 */
static bool ARPGHasClearInsertAimPath(
    UWorld* World,
    const FVector& ViewStart,
    const AARPGBuildPieceActor* Target,
    ECollisionChannel TraceChannel,
    const AActor* IgnoredOwner)
{
    if (!World || !Target || !Target->Definition) return false;

    FVector LocalMin, LocalMax;
    ARPGGetBuildPieceLocalBounds(Target->Definition, LocalMin, LocalMax);
    const FVector LocalCenter = (LocalMin + LocalMax) * 0.5f;
    const FVector LocalExtent = (LocalMax - LocalMin) * 0.5f;

    // Probe the opening's middle plus modest upper/lower points. The center normally lies in the
    // actual doorway/window hole, while the extra probes tolerate unusual frame art or a center brace.
    const float VerticalProbe = FMath::Max(5.f, LocalExtent.Z * 0.22f);
    const FVector ProbeLocals[] =
    {
        LocalCenter,
        LocalCenter + FVector(0.f, 0.f, VerticalProbe),
        LocalCenter - FVector(0.f, 0.f, VerticalProbe)
    };

    FCollisionQueryParams Params(SCENE_QUERY_STAT(ARPGInsertAimOcclusion), false, IgnoredOwner);
    for (const FVector& ProbeLocal : ProbeLocals)
    {
        const FVector ProbeWorld = Target->GetActorTransform().TransformPosition(ProbeLocal);
        FHitResult Hit;
        const bool bHit = World->LineTraceSingleByChannel(Hit, ViewStart, ProbeWorld, TraceChannel, Params);
        if (!bHit || Hit.GetActor() == Target) return true;
    }

    return false;
}

static bool ARPGFindViewDirectedInsertSnap(
    UWorld* World,
    const UARPGBuildPieceDefinition* IncomingPiece,
    const FVector& ViewStart,
    const FVector& ViewEnd,
    ECollisionChannel TraceChannel,
    const AActor* IgnoredOwner,
    AARPGBuildPieceActor* PreferredTraceTarget,
    FTransform& OutTransform,
    AARPGBuildPieceActor*& OutSnapTarget)
{
    OutSnapTarget = nullptr;
    if (!World || !IncomingPiece || !ARPGIsInsertPieceKind(IncomingPiece->PieceKind)) return false;

    const FVector Segment = ViewEnd - ViewStart;
    const float SegmentLength = Segment.Size();
    if (SegmentLength <= KINDA_SMALL_NUMBER) return false;

    // The actor hit by the ordinary placement trace is the strongest possible intent signal when it
    // is already the compatible opening host. This handles aiming directly at a WindowWall frame.
    if (PreferredTraceTarget && PreferredTraceTarget->Definition && PreferredTraceTarget->IsConstructionComplete() &&
        ARPGIsInsertSnapPair(PreferredTraceTarget->Definition->PieceKind, IncomingPiece->PieceKind))
    {
        TArray<FTransform> PreferredCandidates;
        PreferredTraceTarget->GetSnapTransformsFor(IncomingPiece, PreferredCandidates);
        if (PreferredCandidates.Num() > 0)
        {
            OutTransform = PreferredCandidates[0];
            OutSnapTarget = PreferredTraceTarget;
            return true;
        }
    }

    float BestScore = TNumericLimits<float>::Max();
    for (TActorIterator<AARPGBuildPieceActor> It(World); It; ++It)
    {
        AARPGBuildPieceActor* Target = *It;
        if (!Target || !Target->Definition || !Target->IsConstructionComplete()) continue;
        if (!ARPGIsInsertSnapPair(Target->Definition->PieceKind, IncomingPiece->PieceKind)) continue;

        TArray<FTransform> Candidates;
        Target->GetSnapTransformsFor(IncomingPiece, Candidates);
        if (Candidates.Num() == 0) continue;

        FVector LocalMin, LocalMax;
        ARPGGetBuildPieceLocalBounds(Target->Definition, LocalMin, LocalMax);
        const FVector UnpaddedLocalCenter = (LocalMin + LocalMax) * 0.5f;
        const FVector UnpaddedLocalExtent = (LocalMax - LocalMin) * 0.5f;

        // Aim padding is deliberately only a capture aid. It never changes the final structural socket.
        const float AimPadding = FMath::Clamp(IncomingPiece->SnapCaptureDistance * 0.15f, 6.f, 30.f);
        const FVector PaddedLocalMin = LocalMin - FVector(AimPadding);
        const FVector PaddedLocalMax = LocalMax + FVector(AimPadding);

        const FTransform TargetTransform = Target->GetActorTransform();
        const FVector LocalStart = TargetTransform.InverseTransformPosition(ViewStart);
        const FVector LocalEnd = TargetTransform.InverseTransformPosition(ViewEnd);
        float EnterT = 0.f;
        float CenterlineErrorSq = 0.f;
        const bool bDirectBoxAim = ARPGSegmentIntersectsLocalBox(
            LocalStart, LocalEnd, PaddedLocalMin, PaddedLocalMax, EnterT);
        if (!bDirectBoxAim &&
            !ARPGSegmentPassesInsertAimCorridor(
                ViewStart, ViewEnd, TargetTransform, UnpaddedLocalCenter, UnpaddedLocalExtent,
                AimPadding, EnterT, CenterlineErrorSq))
        {
            continue;
        }

        // Do not let the generic placement hit (often the foundation beneath the player) occlude the
        // opening. Validate visibility specifically toward this Doorway/WindowWall instead.
        if (!ARPGHasClearInsertAimPath(World, ViewStart, Target, TraceChannel, IgnoredOwner)) continue;

        const float AlongRayDistance = SegmentLength * EnterT;
        const FVector TargetCenterWorld = TargetTransform.TransformPosition(UnpaddedLocalCenter);
        const FVector ClosestRayPoint = ViewStart + Segment.GetSafeNormal() * AlongRayDistance;
        if (bDirectBoxAim)
            CenterlineErrorSq = FVector::DistSquared(TargetCenterWorld, ClosestRayPoint);

        // Primarily choose the opening nearest along the player's view ray. Centerline error keeps
        // neighbouring WindowWalls deterministic. A small fallback penalty means an actual OBB hit
        // always wins over a corridor-only candidate at otherwise equivalent geometry.
        const float CorridorPenalty = bDirectBoxAim ? 0.f : FMath::Square(AimPadding + 1.f);
        const float Score = FMath::Square(AlongRayDistance) + CenterlineErrorSq * 0.25f + CorridorPenalty;
        if (Score >= BestScore) continue;

        BestScore = Score;
        OutTransform = Candidates[0];
        OutSnapTarget = Target;
    }

    return OutSnapTarget != nullptr;
}

/**
 * A snapped modular seam may intentionally overlap a few centimetres of neighbouring art
 * (for example a 302 cm wall on a 300 cm structural grid, or two wall posts meeting at an L corner).
 * Collision validation must not globally ignore build pieces, because that would permit arbitrary
 * clipping. Instead, an overlapping completed build piece is tolerated only when it advertises the
 * incoming final transform as one of its native/custom snap transforms.
 */
static bool ARPGIsValidSnappedBuildNeighbor(AARPGBuildPieceActor* Neighbor, const UARPGBuildPieceDefinition* IncomingPiece, const FTransform& IncomingFinal)
{
    if (!Neighbor || !IncomingPiece || !Neighbor->IsConstructionComplete()) return false;

    TArray<FTransform> NeighborCandidates;
    Neighbor->GetSnapTransformsFor(IncomingPiece, NeighborCandidates);

    // Native snap candidates are the preferred proof of a legitimate modular neighbour, but do not
    // return early when this neighbour advertises none. Some valid inter-story relationships are
    // intentionally inverse relationships: for example, a pre-stacked upper Wall does not advertise
    // a Floor socket at its own base, and an upper Floor does not advertise a Wall terminating at its
    // underside. Those cases are validated by the strict structural seam fallbacks below.

    // Candidate transforms come from the same authoritative snap graph, so only a small tolerance
    // is needed for transform composition/floating-point drift. PlacementCollisionClearance remains
    // the exposed per-piece seam/validation tuning value and therefore also informs this tolerance.
    const float PositionTolerance = FMath::Max(1.f, IncomingPiece->PlacementCollisionClearance + 0.5f);
    const float PositionToleranceSq = FMath::Square(PositionTolerance);
    constexpr float RotationToleranceDegrees = 1.f;

    for (const FTransform& Candidate : NeighborCandidates)
    {
        if (FVector::DistSquared(Candidate.GetLocation(), IncomingFinal.GetLocation()) > PositionToleranceSq) continue;
        const float RotationDeltaDegrees = FMath::RadiansToDegrees(Candidate.GetRotation().AngularDistance(IncomingFinal.GetRotation()));
        if (RotationDeltaDegrees <= RotationToleranceDegrees) return true;

        // A room perimeter can expose the same Floor/Ceiling/Roof slot from walls facing different
        // cardinal directions. Accept that neighbour only when the incoming validation footprint is
        // physically unchanged by the yaw delta; rectangular 90-degree clipping remains blocked.
        if (ARPGIsUpperHorizontalStructuralKind(IncomingPiece->PieceKind) &&
            ARPGRotationsPreservePlacementFootprint(IncomingPiece, Candidate.GetRotation(), IncomingFinal.GetRotation()))
            return true;
    }

    // Reverse/inter-story seam validation: if the player stacked the next-story Wall-family piece
    // before inserting the horizontal slab, that upper wall does not advertise a Floor socket at
    // its own base. Validate the exact structural edge + facing + story-plane relationship instead.
    // This makes Floor-first and Wall-stack-first construction produce the same valid result.
    if (ARPGIsValidUpperHorizontalWallSeamNeighbor(Neighbor, IncomingPiece, IncomingFinal))
        return true;

    // Inverse story-bay seam: a Wall-family piece snapped to the lower slab may terminate exactly
    // at the underside of an already-built upper Floor/Ceiling/Roof. The upper slab is a legitimate
    // boundary neighbour even though its native sockets only advertise walls built on top of it.
    if (ARPGIsValidWallUnderUpperHorizontalSeamNeighbor(Neighbor, IncomingPiece, IncomingFinal))
        return true;

    // Reverse Stair-boundary seam: when a Stair already exists, a Wall/WindowWall/Doorway snapped to the
    // underlying horizontal grid may intentionally share the Stair's side stringer/frame seam. Accept
    // only the exact structural side/endpoint boundary topology; interior-crossing walls remain blocked.
    if (ARPGIsValidExistingStairWallSideSeamNeighbor(Neighbor, IncomingPiece, IncomingFinal))
        return true;

    return false;
}

UARPGBuildingComponent::UARPGBuildingComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = false;
    SetIsReplicatedByDefault(true);
}

void UARPGBuildingComponent::BeginPlay()
{
    Super::BeginPlay();
    if (bAutoSelectFirstCatalogPiece && BuildCatalog.Num() > 0 && BuildCatalog[0]) SelectedBuildPiece = BuildCatalog[0];
}

void UARPGBuildingComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    DestroyPreviewActor();
    Super::EndPlay(EndPlayReason);
}

bool UARPGBuildingComponent::IsLocalBuildController() const
{
    if (const APawn* Pawn = Cast<APawn>(GetOwner())) return Pawn->IsLocallyControlled();
    return GetOwner() && GetOwner()->HasAuthority();
}

bool UARPGBuildingComponent::BeginBuildMode(UARPGBuildPieceDefinition* Piece)
{
    if (!IsLocalBuildController()) return false;
    if (Piece) SelectedBuildPiece = Piece;
    if (!SelectedBuildPiece && BuildCatalog.Num() > 0) SelectedBuildPiece = BuildCatalog[0];
    if (!SelectedBuildPiece) return false;
    bBuildModeActive = true;
    PreviewYawOffset = 0.f;
    CurrentPreviewResult = EARPGPlacementResult::NoPiece;
    EnsurePreviewActor();
    SetComponentTickEnabled(true);
    UpdatePlacementPreview();
    OnBuildModeChanged.Broadcast(true, SelectedBuildPiece);
    return true;
}

void UARPGBuildingComponent::EndBuildMode()
{
    if (!bBuildModeActive && !ActivePreviewActor) return;
    bBuildModeActive = false;
    CurrentPreviewResult = EARPGPlacementResult::NoPiece;
    CurrentSnapTarget.Reset();
    SetComponentTickEnabled(false);
    DestroyPreviewActor();
    OnBuildModeChanged.Broadcast(false, SelectedBuildPiece);
}

bool UARPGBuildingComponent::ToggleBuildMode(UARPGBuildPieceDefinition* OptionalPiece)
{
    if (bBuildModeActive) { EndBuildMode(); return false; }
    return BeginBuildMode(OptionalPiece);
}

bool UARPGBuildingComponent::SelectBuildPiece(UARPGBuildPieceDefinition* Piece)
{
    if (!Piece) return false;
    SelectedBuildPiece = Piece;
    PreviewYawOffset = 0.f;
    if (bBuildModeActive)
    {
        DestroyPreviewActor();
        EnsurePreviewActor();
        UpdatePlacementPreview();
        OnBuildModeChanged.Broadcast(true, SelectedBuildPiece);
    }
    return true;
}

bool UARPGBuildingComponent::SelectNextBuildPiece()
{
    if (BuildCatalog.Num() == 0) return false;
    int32 Index = BuildCatalog.IndexOfByKey(SelectedBuildPiece);
    Index = (Index == INDEX_NONE) ? 0 : (Index + 1) % BuildCatalog.Num();
    return SelectBuildPiece(BuildCatalog[Index]);
}

bool UARPGBuildingComponent::SelectPreviousBuildPiece()
{
    if (BuildCatalog.Num() == 0) return false;
    int32 Index = BuildCatalog.IndexOfByKey(SelectedBuildPiece);
    Index = (Index == INDEX_NONE) ? 0 : (Index - 1 + BuildCatalog.Num()) % BuildCatalog.Num();
    return SelectBuildPiece(BuildCatalog[Index]);
}

void UARPGBuildingComponent::RotatePreview(float Direction)
{
    if (!SelectedBuildPiece || !bBuildModeActive) return;
    PreviewYawOffset += FMath::Sign(Direction) * FMath::Max(1.f, SelectedBuildPiece->RotationStepDegrees);
    PreviewYawOffset = FMath::Fmod(PreviewYawOffset, 360.f);
    UpdatePlacementPreview();
}

void UARPGBuildingComponent::EnsurePreviewActor()
{
    if (ActivePreviewActor || !GetWorld() || !SelectedBuildPiece) return;
    FActorSpawnParameters Params;
    Params.Owner = GetOwner();
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    ActivePreviewActor = GetWorld()->SpawnActor<AARPGBuildPreviewActor>(AARPGBuildPreviewActor::StaticClass(), FTransform::Identity, Params);
    if (ActivePreviewActor)
        ActivePreviewActor->ConfigurePreview(SelectedBuildPiece, ValidPreviewMaterial.LoadSynchronous(), InvalidPreviewMaterial.LoadSynchronous());
}

void UARPGBuildingComponent::DestroyPreviewActor()
{
    if (ActivePreviewActor) ActivePreviewActor->Destroy();
    ActivePreviewActor = nullptr;
}

void UARPGBuildingComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    if (bBuildModeActive && IsLocalBuildController()) UpdatePlacementPreview();
}

void UARPGBuildingComponent::UpdatePlacementPreview()
{
    if (!bBuildModeActive || !SelectedBuildPiece || !GetOwner() || !GetWorld()) return;
    EnsurePreviewActor();

    FVector ViewLocation;
    FRotator ViewRotation;
    if (const APawn* Pawn = Cast<APawn>(GetOwner()))
    {
        if (APlayerController* PC = Cast<APlayerController>(Pawn->GetController())) PC->GetPlayerViewPoint(ViewLocation, ViewRotation);
        else { ViewLocation = GetOwner()->GetActorLocation(); ViewRotation = GetOwner()->GetActorRotation(); }
    }
    else { ViewLocation = GetOwner()->GetActorLocation(); ViewRotation = GetOwner()->GetActorRotation(); }

    const FVector End = ViewLocation + ViewRotation.Vector() * MaxPlacementDistance;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(ARPGBuildPreviewTrace), false, GetOwner());
    if (ActivePreviewActor) Params.AddIgnoredActor(ActivePreviewActor);
    FHitResult Hit;
    const bool bHit = ARPGTracePlacementSurfaceIgnoringFoundationTrees(
        GetWorld(), SelectedBuildPiece, ViewLocation, End, PlacementTraceChannel, Params, Hit);

    const FRotator DesiredRotation(0.f, ViewRotation.Yaw + PreviewYawOffset, 0.f);
    FVector Location = bHit ? Hit.ImpactPoint : End;
    if (bHit && SelectedBuildPiece->bAllowGroundPlacement)
    {
        // Put the visible mesh's real bottom-center on the traced surface. This is pivot-aware:
        // bottom-pivot meshes need no artificial lift, while center-pivot meshes are lifted only by
        // their actual local bottom offset. Do not move along ImpactNormal: that causes sideways drift
        // on slopes and was the source of visibly floating bottom-pivot foundations.
        const FVector BottomAnchorLocal = ARPGGetBuildPieceBottomAnchorLocal(SelectedBuildPiece);
        Location = Hit.ImpactPoint - DesiredRotation.RotateVector(BottomAnchorLocal);
    }
    Location += DesiredRotation.RotateVector(SelectedBuildPiece->PlacementOffset);

    FTransform Desired(DesiredRotation, Location);
    AARPGBuildPieceActor* SnapTarget = nullptr;

    if (SelectedBuildPiece->PieceKind == EARPGBuildPieceKind::Light)
    {
        // Buildable lights use surface placement rather than the structural grid/snap graph. A wall
        // fixture follows the actually aimed wall face; a stick/floor light seats its visible bottom
        // on terrain or a Foundation/Floor top plane. No existing structural snap code is touched.
        FTransform SurfaceTransform;
        if (bHit && ARPGResolveBuildLightPlacementFromHit(
            SelectedBuildPiece, Hit, ViewRotation.Yaw + PreviewYawOffset, SurfaceTransform, SnapTarget))
        {
            CurrentPreviewTransform = SurfaceTransform;
        }
        else
        {
            CurrentPreviewTransform = Desired;
        }
    }
    else if (ARPGIsSettlementSurfacePiece(SelectedBuildPiece))
    {
        FTransform SurfaceTransform;
        if (bHit && ARPGResolveSettlementPlacementFromHit(
            SelectedBuildPiece, Hit, ViewRotation.Yaw + PreviewYawOffset, SurfaceTransform, SnapTarget))
            CurrentPreviewTransform = SurfaceTransform;
        else
            CurrentPreviewTransform = Desired;
    }
    else
    {
        // Door/Window inserts use a dedicated full-view semantic search. Do not truncate this at the
        // generic placement trace hit: in third-person that hit is frequently the supporting floor before
        // the camera ray reaches the doorway. Each compatible opening performs its own LOS validation.
        FTransform ViewDirectedInsertTransform;
        AARPGBuildPieceActor* PreferredInsertHost = bHit ? Cast<AARPGBuildPieceActor>(Hit.GetActor()) : nullptr;
        if (ARPGFindViewDirectedInsertSnap(
            GetWorld(), SelectedBuildPiece, ViewLocation, End, PlacementTraceChannel, GetOwner(),
            PreferredInsertHost, ViewDirectedInsertTransform, SnapTarget))
        {
            CurrentPreviewTransform = ViewDirectedInsertTransform;
        }
        else
        {
            CurrentPreviewTransform = ResolvePlacementTransform(SelectedBuildPiece, Desired, SnapTarget);
        }
    }
    CurrentSnapTarget = SnapTarget;
    CurrentPreviewResult = bHit || SnapTarget ? EvaluatePlacementInternal(SelectedBuildPiece, CurrentPreviewTransform, SnapTarget) : EARPGPlacementResult::InvalidSurface;

    if (ActivePreviewActor)
    {
        ActivePreviewActor->SetActorTransform(CurrentPreviewTransform, false, nullptr, ETeleportType::TeleportPhysics);
        ActivePreviewActor->SetPlacementResult(CurrentPreviewResult);
    }
    OnBuildPreviewUpdated.Broadcast(CurrentPreviewResult, CurrentPreviewTransform);
}

FTransform UARPGBuildingComponent::ResolvePlacementTransform(const UARPGBuildPieceDefinition* Piece, const FTransform& DesiredTransform, AARPGBuildPieceActor*& OutSnapTarget) const
{
    OutSnapTarget = nullptr;
    if (!Piece) return DesiredTransform;

    if (Piece->PieceKind == EARPGBuildPieceKind::Light)
    {
        FTransform SurfaceTransform;
        if (ARPGFindBuildLightSurfaceFromDesired(GetWorld(), Piece, DesiredTransform, SurfaceTransform, OutSnapTarget))
            return SurfaceTransform;

        // Ground lights have no build actor as their support, but authority must never trust the
        // client-supplied Z. Re-seat the visible bottom on the server's own support trace so a crafted
        // placement request cannot float a torch anywhere within SupportTraceDepth. If the trace hits
        // a built Foundation/Floor that escaped the proximity scan above, preserve normal host access
        // semantics by returning it as the snap target instead of treating it as anonymous terrain.
        if (Piece->LightPlacementMode == EARPGBuildLightPlacementMode::HorizontalSurface && Piece->bAllowGroundPlacement && GetWorld())
        {
            const FVector BottomAnchor = ARPGGetBuildPieceBottomAnchorLocal(Piece);
            const FVector PlacementCorrection = DesiredTransform.GetRotation().RotateVector(Piece->PlacementOffset);
            const FVector DesiredBottomWorld = DesiredTransform.TransformPosition(BottomAnchor) - PlacementCorrection;
            const float ProbeLift = FMath::Max(4.f, Piece->PlacementCollisionClearance + 2.f);
            const FVector TraceStart = DesiredBottomWorld + FVector::UpVector * ProbeLift;
            const FVector TraceEnd = DesiredBottomWorld - FVector::UpVector * FMath::Max(1.f, Piece->SupportTraceDepth);

            FCollisionQueryParams LightSupportParams(SCENE_QUERY_STAT(ARPGBuildLightAuthoritySupport), false, GetOwner());
            FHitResult SupportHit;
            if (GetWorld()->LineTraceSingleByChannel(SupportHit, TraceStart, TraceEnd, PlacementCollisionChannel, LightSupportParams))
            {
                AARPGBuildPieceActor* BuildSupport = Cast<AARPGBuildPieceActor>(SupportHit.GetActor());
                if (!BuildSupport || (BuildSupport->Definition && BuildSupport->IsConstructionComplete() &&
                    ARPGIsBuildLightHorizontalHostKind(BuildSupport->Definition->PieceKind)))
                {
                    const FVector N = SupportHit.ImpactNormal.GetSafeNormal();
                    const float MaxSlope = FMath::Clamp(Piece->MaxGroundSlopeDegrees, 0.f, 89.f);
                    const float MinUpDot = FMath::Cos(FMath::DegreesToRadians(MaxSlope));
                    if (!N.IsNearlyZero() && FVector::DotProduct(N, FVector::UpVector) >= MinUpDot)
                    {
                        OutSnapTarget = BuildSupport;
                        return ARPGMakeHorizontalBuildLightTransform(Piece, SupportHit.ImpactPoint, DesiredTransform.Rotator().Yaw);
                    }
                }
            }
        }
        return DesiredTransform;
    }

    if (ARPGIsSettlementSurfacePiece(Piece))
    {
        FTransform SurfaceTransform;
        if (ARPGFindSettlementSurfaceFromDesired(GetWorld(), Piece, DesiredTransform, PlacementCollisionChannel, SurfaceTransform, OutSnapTarget))
            return SurfaceTransform;
        return DesiredTransform;
    }

    FTransform Snapped;
    if (Piece->bSnapPlacement && FindBestSnapTransform(Piece, DesiredTransform, Snapped, OutSnapTarget)) return Snapped;

    FTransform Result = DesiredTransform;

    // Rotate around the piece's visible ground anchor instead of blindly around the actor pivot.
    // This keeps corner/bottom authored pivots from walking sideways when yaw is quantized.
    const FVector AnchorLocal = Piece->bAllowGroundPlacement ? ARPGGetBuildPieceBottomAnchorLocal(Piece) : FVector::ZeroVector;
    const FVector AnchorBeforeRotation = Result.TransformPosition(AnchorLocal);
    FRotator R = Result.Rotator();
    R.Pitch = 0.f; R.Roll = 0.f;
    R.Yaw = FMath::GridSnap(R.Yaw, FMath::Max(1.f, Piece->RotationStepDegrees));
    Result.SetRotation(R.Quaternion());
    Result.AddToTranslation(AnchorBeforeRotation - Result.TransformPosition(AnchorLocal));

    if (Piece->bSnapPlacement && Piece->SnapSize > KINDA_SMALL_NUMBER)
    {
        // Grid-snap the visible footprint anchor, not the actor pivot. This remains correct for
        // center, bottom-center and corner pivots. Z is intentionally left on the support surface.
        const FVector AnchorWorld = Result.TransformPosition(AnchorLocal);
        FVector SnappedAnchor = AnchorWorld;
        SnappedAnchor.X = FMath::GridSnap(SnappedAnchor.X, Piece->SnapSize);
        SnappedAnchor.Y = FMath::GridSnap(SnappedAnchor.Y, Piece->SnapSize);
        Result.AddToTranslation(SnappedAnchor - AnchorWorld);
    }
    return Result;
}

bool UARPGBuildingComponent::FindBestSnapTransform(const UARPGBuildPieceDefinition* Piece, const FTransform& DesiredTransform, FTransform& OutTransform, AARPGBuildPieceActor*& OutSnapTarget) const
{
    OutSnapTarget = nullptr;
    UWorld* World = GetWorld();
    if (!Piece || !World) return false;

    TArray<FOverlapResult> Overlaps;
    FCollisionObjectQueryParams ObjectParams;
    ObjectParams.AddObjectTypesToQuery(ECC_WorldStatic);
    ObjectParams.AddObjectTypesToQuery(ECC_WorldDynamic);
    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ARPGBuildSnapSearch), false, GetOwner());
    World->OverlapMultiByObjectType(Overlaps, DesiredTransform.GetLocation(), FQuat::Identity, ObjectParams,
        FCollisionShape::MakeSphere(FMath::Max(1.f, Piece->SnapSearchRadius)), QueryParams);

    float BestScore = TNumericLimits<float>::Max();
    int32 BestSemanticPriority = TNumericLimits<int32>::Max();
    const float CaptureSq = FMath::Square(FMath::Max(1.f, Piece->SnapCaptureDistance));
    // Only use semantic priority as a tie-breaker for transforms that represent the same physical
    // socket. This avoids changing normal snap selection while preventing an overlapping wall-corner
    // candidate from overriding the foundation/floor edge that defines wall exterior orientation.
    const float SameSlotTolerance = FMath::Max(1.f, Piece->PlacementCollisionClearance + 0.5f);
    const float SameSlotToleranceSq = FMath::Square(SameSlotTolerance);
    TSet<AARPGBuildPieceActor*> Seen;

    const auto EvaluateTarget = [&](AARPGBuildPieceActor* Target)
    {
        if (!Target || Seen.Contains(Target) || !Target->Definition || !Target->IsConstructionComplete()) return;
        Seen.Add(Target);

        TArray<FTransform> Candidates;
        Target->GetSnapTransformsFor(Piece, Candidates);
        if (Candidates.Num() == 0) return;

        const bool bInsertSnapPair = ARPGIsInsertSnapPair(Target->Definition->PieceKind, Piece->PieceKind);
        const bool bStairSupportSnapPair = ARPGIsStairSupportSnapPair(Target->Definition->PieceKind, Piece->PieceKind);
        const bool bStairChainSnapPair = ARPGIsStairChainSnapPair(Target->Definition->PieceKind, Piece->PieceKind);
        const bool bAnyStairSnapPair = bStairSupportSnapPair || bStairChainSnapPair;
        const bool bHorizontalFootprintCapture = ARPGIsUpperHorizontalStructuralKind(Piece->PieceKind);
        const bool bCandidateEnvelopeCapture = bHorizontalFootprintCapture || bAnyStairSnapPair;
        const float InsertAimDistSq = bInsertSnapPair
            ? ARPGDistanceSquaredToBuildPieceBounds(Target, DesiredTransform.GetLocation())
            : TNumericLimits<float>::Max();
        const float StairTargetAimDistSq = bAnyStairSnapPair
            ? ARPGDistanceSquaredToBuildPieceBounds(Target, DesiredTransform.GetLocation())
            : TNumericLimits<float>::Max();

        for (const FTransform& Candidate : Candidates)
        {
            const int32 SemanticPriority = ARPGGetSnapCandidateSemanticPriority(Target, Piece, Candidate);
            const float CandidateDistSq = FVector::DistSquared(Candidate.GetLocation(), DesiredTransform.GetLocation());
            const float CandidateEnvelopeDistSq = bCandidateEnvelopeCapture
                ? ARPGDistanceSquaredToDefinitionBoundsAtTransform(Piece, Candidate, DesiredTransform.GetLocation())
                : TNumericLimits<float>::Max();

            // Door/window inserts capture against their supporting opening. Upper horizontal pieces
            // capture against the candidate tile's visible footprint. Stairs may now target either a
            // Foundation/Floor/Ceiling landing or the HIGH/LOW endpoint of another Stair. Capture is
            // permissive against the compatible target bounds, while ranking still uses the candidate
            // envelope so aiming inside the host cell prefers the LOW-departure/up-flight socket and
            // aiming outside/down prefers the HIGH-arrival/down-flight socket. This prevents the paired
            // landing sockets from becoming camera-order dependent.
            float CaptureMetricSq = CandidateDistSq;
            if (bInsertSnapPair)
                CaptureMetricSq = FMath::Min(InsertAimDistSq, CandidateDistSq);
            else if (bAnyStairSnapPair)
                CaptureMetricSq = FMath::Min(StairTargetAimDistSq, FMath::Min(CandidateEnvelopeDistSq, CandidateDistSq));
            else if (bHorizontalFootprintCapture)
                CaptureMetricSq = FMath::Min(CandidateEnvelopeDistSq, CandidateDistSq);
            if (CaptureMetricSq > CaptureSq) continue;

            const float YawDelta = FMath::Abs(FMath::FindDeltaAngleDegrees(Candidate.Rotator().Yaw, DesiredTransform.Rotator().Yaw));
            float SemanticTieBreak = (bInsertSnapPair || bCandidateEnvelopeCapture) ? CandidateDistSq * 0.01f : 0.f;
            if (bAnyStairSnapPair)
            {
                const float CandidateAffinitySq = FMath::Min(CandidateEnvelopeDistSq, CandidateDistSq);
                SemanticTieBreak = CandidateAffinitySq * 0.05f + CandidateDistSq * 0.001f;
            }
            const float Score = CaptureMetricSq + SemanticTieBreak + FMath::Square(YawDelta * 1.5f);

            const bool bSamePhysicalSlot = OutSnapTarget &&
                (FVector::DistSquared(Candidate.GetLocation(), OutTransform.GetLocation()) <= SameSlotToleranceSq ||
                 ARPGWallSnapCandidatesShareStructuralSlot(
                     Piece,
                     Target,
                     Candidate,
                     OutSnapTarget,
                     OutTransform,
                     SameSlotTolerance));
            const bool bBetterSemanticOwner = bSamePhysicalSlot && SemanticPriority < BestSemanticPriority;
            const bool bSameSemanticOwner = !bSamePhysicalSlot || SemanticPriority == BestSemanticPriority;
            if (bBetterSemanticOwner || (bSameSemanticOwner && Score < BestScore))
            {
                BestScore = Score;
                BestSemanticPriority = SemanticPriority;
                OutTransform = Candidate;
                OutSnapTarget = Target;
            }
        }
    };

    // Fast normal path: preserve the existing collision-spatial query for all structural pieces.
    for (const FOverlapResult& Overlap : Overlaps)
        EvaluateTarget(Cast<AARPGBuildPieceActor>(Overlap.GetActor()));

    // Imported Doorway/WindowWall meshes are allowed to use hollow or complex-only collision. If the
    // broad overlap could not discover them, scan only the semantically compatible insert supports.
    // This fallback is intentionally restricted to Door/Window pieces, so the proven Foundation/Wall
    // snap path and its performance characteristics are unchanged. It also lets authority reacquire
    // the exact target from the already-snapped transform sent by the local preview.
    if (ARPGIsInsertPieceKind(Piece->PieceKind))
    {
        for (TActorIterator<AARPGBuildPieceActor> It(World); It; ++It)
        {
            AARPGBuildPieceActor* Target = *It;
            if (!Target || Seen.Contains(Target) || !Target->Definition) continue;
            if (!ARPGIsInsertSnapPair(Target->Definition->PieceKind, Piece->PieceKind)) continue;
            EvaluateTarget(Target);
        }
    }

    // Final Wall-family facing is normalized from the actual horizontal structural edge after snap
    // ownership has been resolved. This removes the last front/back dependency on Floor actor yaw,
    // overlap iteration order and competing vertical/lateral candidates while preserving interior
    // partition ambiguity and wall-only continuation when no horizontal support exists.
    if (OutSnapTarget && ARPGIsWallLikeKind(Piece->PieceKind))
        ARPGCanonicalizeWallFacingFromHorizontalSupport(Overlaps, Piece, OutTransform);

    return OutSnapTarget != nullptr;
}

FTransform UARPGBuildingComponent::SnapTransform(const UARPGBuildPieceDefinition* Piece, const FTransform& DesiredTransform) const
{
    AARPGBuildPieceActor* Target = nullptr;
    return ResolvePlacementTransform(Piece, DesiredTransform, Target);
}

bool UARPGBuildingComponent::HasBuildResources(const UARPGBuildPieceDefinition* Piece) const
{
    if (!bConsumeResources) return true;
    const UARPGInventoryComponent* Inventory = GetOwner() ? GetOwner()->FindComponentByClass<UARPGInventoryComponent>() : nullptr;
    if (!Inventory || !Piece) return false;
    TMap<FName, int32> Costs;
    ARPGAggregateBuildCosts(Piece, Costs);
    for (const TPair<FName, int32>& Cost : Costs) if (!Inventory->HasUnequippedItem(Cost.Key, Cost.Value)) return false;
    return true;
}

int32 UARPGBuildingComponent::GetBuildableCount(const UARPGBuildPieceDefinition* Piece) const
{
    if (!Piece) return 0;
    if (!bConsumeResources) return 999;
    const UARPGInventoryComponent* Inventory = GetOwner() ? GetOwner()->FindComponentByClass<UARPGInventoryComponent>() : nullptr;
    if (!Inventory) return 0;
    TMap<FName, int32> Costs;
    ARPGAggregateBuildCosts(Piece, Costs);
    if (Costs.Num() == 0) return 999;
    int32 Count = MAX_int32;
    for (const TPair<FName, int32>& Cost : Costs)
        Count = FMath::Min(Count, Inventory->GetUnequippedItemCount(Cost.Key) / FMath::Max(1, Cost.Value));
    return FMath::Max(0, Count == MAX_int32 ? 0 : Count);
}

bool UARPGBuildingComponent::ConsumeBuildResources(const UARPGBuildPieceDefinition* Piece)
{
    if (!bConsumeResources) return true;
    UARPGInventoryComponent* Inventory = GetOwner() ? GetOwner()->FindComponentByClass<UARPGInventoryComponent>() : nullptr;
    if (!Inventory || !Piece || !HasBuildResources(Piece)) return false;
    TMap<FName, int32> Costs;
    ARPGAggregateBuildCosts(Piece, Costs);
    TArray<TPair<FName, int32>> Removed;
    for (const TPair<FName, int32>& Cost : Costs)
    {
        if (!Inventory->RemoveUnequippedItem(Cost.Key, Cost.Value))
        {
            for (const TPair<FName, int32>& Rollback : Removed) Inventory->AddItem(Rollback.Key, Rollback.Value);
            return false;
        }
        Removed.Add(Cost);
    }
    return true;
}

void UARPGBuildingComponent::RefundBuildResources(const UARPGBuildPieceDefinition* Piece)
{
    if (!bConsumeResources || !Piece) return;
    UARPGInventoryComponent* Inventory = GetOwner() ? GetOwner()->FindComponentByClass<UARPGInventoryComponent>() : nullptr;
    if (!Inventory) return;
    for (const FARPGItemAmount& Cost : Piece->BuildCost)
    {
        if (Cost.Quantity <= 0) continue;
        if (Cost.Item) Inventory->AddItemDefinition(Cost.Item, Cost.Quantity);
        else if (!ARPGResolveBuildCostId(Cost).IsNone()) Inventory->AddItem(ARPGResolveBuildCostId(Cost), Cost.Quantity);
    }
}

EARPGPlacementResult UARPGBuildingComponent::EvaluatePlacementInternal(const UARPGBuildPieceDefinition* Piece, const FTransform& Final, const AARPGBuildPieceActor* SnapTarget) const
{
    if (!Piece) return EARPGPlacementResult::NoPiece;
    AActor* Owner = GetOwner();
    UWorld* World = GetWorld();
    if (!Owner || !World) return EARPGPlacementResult::Restricted;
    if (!bAllowUnlistedBuildRequests && !BuildCatalog.ContainsByPredicate([&](const TObjectPtr<UARPGBuildPieceDefinition>& CatalogPiece){ return CatalogPiece.Get() == Piece; })) return EARPGPlacementResult::Restricted;
    if (SnapTarget && bRequireSnapTargetModificationAccess && !SnapTarget->CanActorModify(Owner)) return EARPGPlacementResult::Restricted;
    if (FVector::DistSquared(Owner->GetActorLocation(), Final.GetLocation()) > FMath::Square(MaxPlacementDistance)) return EARPGPlacementResult::TooFar;
    if (!HasBuildResources(Piece)) return EARPGPlacementResult::MissingResources;
    if (Piece->bRequiresSnapTarget && !SnapTarget) return EARPGPlacementResult::Unsupported;

    if (Piece->PieceKind == EARPGBuildPieceKind::Light)
    {
        if (Piece->LightPlacementMode == EARPGBuildLightPlacementMode::WallSurface)
        {
            if (!SnapTarget || !SnapTarget->Definition || !SnapTarget->IsConstructionComplete() ||
                !ARPGIsWallLikeKind(SnapTarget->Definition->PieceKind))
                return EARPGPlacementResult::Unsupported;
        }
        else if (SnapTarget)
        {
            if (!SnapTarget->Definition || !SnapTarget->IsConstructionComplete() ||
                !ARPGIsBuildLightHorizontalHostKind(SnapTarget->Definition->PieceKind))
                return EARPGPlacementResult::Unsupported;
        }
        else if (!Piece->bAllowGroundPlacement)
        {
            return EARPGPlacementResult::Unsupported;
        }

        // Native light visuals are intentionally non-blocking so they cannot regress structural
        // placement. Preserve a small semantic fixture-spacing rule so exact duplicate lamps are still
        // rejected even though their imported mesh collision is disabled.
        const float MinimumSpacing = FMath::Max(0.f, Piece->LightMinimumSpacing);
        if (MinimumSpacing > KINDA_SMALL_NUMBER)
        {
            const float MinimumSpacingSq = FMath::Square(MinimumSpacing);
            for (TActorIterator<AARPGBuildLightActor> It(World); It; ++It)
            {
                const AARPGBuildLightActor* ExistingLight = *It;
                if (!ExistingLight || !ExistingLight->Definition || !ExistingLight->IsConstructionComplete()) continue;
                if (FVector::DistSquared(ExistingLight->GetActorLocation(), Final.GetLocation()) < MinimumSpacingSq)
                    return EARPGPlacementResult::Blocked;
            }
        }
    }

    if (ARPGIsSettlementSurfacePiece(Piece))
    {
        if (SnapTarget)
        {
            if (!SnapTarget->Definition || !SnapTarget->IsConstructionComplete() ||
                !ARPGIsSettlementHorizontalHostKind(SnapTarget->Definition->PieceKind))
                return EARPGPlacementResult::Unsupported;
        }
        else
        {
            // Beds are furnishings and may not float on terrain. Settlement Hubs can opt into direct
            // terrain placement through the normal bAllowGroundPlacement authoring flag.
            if (Piece->PieceKind == EARPGBuildPieceKind::Bed || !Piece->bAllowGroundPlacement)
                return EARPGPlacementResult::Unsupported;
        }

        // One physical location belongs to one settlement authority. By default Hub influence areas
        // cannot overlap, which prevents the same Bed/home from being recruited by two bases and keeps
        // ownership, HUD proximity and worker stockpile routing deterministic. This is semantic only;
        // it does not alter structural collision or the protected Stair/Wall-family topology.
        if (Piece->PieceKind == EARPGBuildPieceKind::SettlementHub && Piece->SettlementDefinition &&
            Piece->SettlementDefinition->bPreventOverlappingSettlementAreas)
        {
            const float NewRadius = FMath::Max(300.f, Piece->SettlementDefinition->SettlementRadius);
            const float NewPadding = FMath::Max(0.f, Piece->SettlementDefinition->SettlementSeparationPadding);
            for (TActorIterator<AARPGSettlementHubActor> It(World); It; ++It)
            {
                const AARPGSettlementHubActor* ExistingHub = *It;
                if (!ExistingHub || !ExistingHub->IsConstructionComplete()) continue;
                const UARPGSettlementDefinition* ExistingDef = ExistingHub->GetSettlementDefinition();
                const float ExistingRadius = ExistingHub->GetSettlementRadius();
                const float ExistingPadding = ExistingDef && ExistingDef->bPreventOverlappingSettlementAreas
                    ? FMath::Max(0.f, ExistingDef->SettlementSeparationPadding) : 0.f;
                const float RequiredDistance = NewRadius + ExistingRadius + FMath::Max(NewPadding, ExistingPadding);
                if (FVector::DistSquared2D(Final.GetLocation(), ExistingHub->GetActorLocation()) < FMath::Square(RequiredDistance))
                    return EARPGPlacementResult::Blocked;
            }
        }
    }

    // Hosted inserts are singleton occupants of their semantic host socket. Do not depend on current
    // physical collision for duplicate detection: an open Door/Window may legitimately move/disable its
    // gameplay blocker, but its Doorway/WindowWall socket is still occupied until the insert is removed.
    if (SnapTarget && ARPGIsInsertPieceKind(Piece->PieceKind))
    {
        for (TActorIterator<AARPGBuildPieceActor> It(World); It; ++It)
        {
            AARPGBuildPieceActor* ExistingInsert = *It;
            if (!ExistingInsert || ExistingInsert == SnapTarget || !ExistingInsert->Definition) continue;
            if (ExistingInsert->Definition->PieceKind != Piece->PieceKind) continue;
            if (ARPGInsertActorMatchesHost(ExistingInsert, SnapTarget)) return EARPGPlacementResult::Blocked;
        }
    }

    const UARPGFactionComponent* Faction = Owner->FindComponentByClass<UARPGFactionComponent>();
    if (!Piece->RequiredBuilderFactionId.IsNone())
    {
        if (!Faction || Faction->GetPrimaryFactionId() != Piece->RequiredBuilderFactionId || Faction->GetReputation(Piece->RequiredBuilderFactionId) < Piece->MinimumBuilderReputation)
            return EARPGPlacementResult::Restricted;
    }
    for (TActorIterator<AARPGFactionTerritoryVolume> It(World); It; ++It)
    {
        AARPGFactionTerritoryVolume* Territory = *It;
        if (Territory && Territory->GetComponentsBoundingBox(true).IsInside(Final.GetLocation()) && !Territory->CanActorBuildHere(Owner)) return EARPGPlacementResult::Restricted;
    }

    FCollisionQueryParams Params(SCENE_QUERY_STAT(ARPGBuildPlacement), false, Owner);
    if (SnapTarget) Params.AddIgnoredActor(SnapTarget);
    TArray<FOverlapResult> Overlaps;
    ARPGGatherPlacementOverlaps(World, Piece, Final, PlacementCollisionChannel, Params, Overlaps);
    if (Overlaps.Num() > 0)
    {
        for (const FOverlapResult& Overlap : Overlaps)
        {
            AActor* Other = Overlap.GetActor();
            if (!Other || Other == Owner || Other == SnapTarget) continue;

            // Foundations intentionally replace ARPG gatherable vegetation space. Ignore the Tree actor
            // as a placement blocker; once authority places the Foundation, the build actor immediately
            // suppresses that Tree's visuals/collision and prevents respawn until the logical build
            // occupancy clears. Rocks, props, pawns and every non-tree blocker remain unchanged.
            if (Piece->PieceKind == EARPGBuildPieceKind::Foundation && Other->IsA<AARPGTree>())
                continue;

            // A verified snapped Stair may embed into Landscape terrain. Landscape uses large
            // continuous collision components whose broad-phase overlap is intentionally conservative;
            // treating those component overlaps like a discrete rock/prop veto makes a correct edge
            // socket permanently red on ordinary ground. Once the Stair transform is proven to be one
            // of the active horizontal-landing or Stair-chain target's native sockets, Landscape is terrain
            // support rather than an independent blocker. This is Landscape-only: discrete WorldStatic meshes
            // still use the strict sampled support/obstruction test immediately below.
            if (Piece->PieceKind == EARPGBuildPieceKind::Stair &&
                SnapTarget &&
                ARPGIsLandscapeTerrainActor(Other) &&
                ARPGTransformMatchesStairHostCandidate(SnapTarget, Piece, Final))
            {
                continue;
            }

            // Non-Landscape WorldStatic contact remains conservative. A rock/prop/static-mesh surface
            // can support the lower flight only while its sampled surface stays beneath the Stair profile;
            // anything rising into the route remains a real blocker.
            if (Piece->PieceKind == EARPGBuildPieceKind::Stair &&
                SnapTarget &&
                ARPGIsValidStairWorldSupportContact(
                    World,
                    Other,
                    SnapTarget,
                    Piece,
                    Final,
                    PlacementCollisionChannel,
                    Params))
            {
                continue;
            }

            // Build meshes are allowed to contain decorative trim that crosses a module seam. For
            // build-vs-build collision, first accept declared/structural snap neighbours. If that does
            // not apply, compare the two authored PlacementBounds OBBs. Raw mesh collision by itself is
            // not a blocker when the logical placement volumes do not actually penetrate.
            if (AARPGBuildPieceActor* BuildNeighbor = Cast<AARPGBuildPieceActor>(Other))
            {
                // Reverse hosted-insert rule: a completed Door/Window belongs semantically to its
                // Doorway/WindowWall host. It must not become an independent blocker when that verified
                // host participates in a legitimate structural seam. Wall/Floor/Ceiling/Roof keep the
                // established v2.15.22 path; v2.15.49 extends the same build-order symmetry to a Stair
                // snapped to a flat landing when the host is on one of the Stair's exact parallel side
                // edges. The insert is ignored only after BOTH exact insert-host identity and the host's
                // incoming structural/Stair-side relationship are proven.
                const EARPGBuildPieceKind NeighborKind = BuildNeighbor->Definition
                    ? BuildNeighbor->Definition->PieceKind
                    : EARPGBuildPieceKind::Custom;
                const bool bNeighborIsInsert = ARPGIsInsertPieceKind(NeighborKind);
                const bool bIncomingIsStandardStructural =
                    ARPGIsWallLikeKind(Piece->PieceKind) || ARPGIsHorizontalStructuralKind(Piece->PieceKind);
                const bool bIncomingIsStair = Piece->PieceKind == EARPGBuildPieceKind::Stair;
                if (bNeighborIsInsert && (bIncomingIsStandardStructural || bIncomingIsStair))
                {
                    const auto InsertAllowsIncomingNeighbor = [&](const AARPGBuildPieceActor* CandidateHost)
                    {
                        if (!CandidateHost) return false;
                        if (bIncomingIsStair)
                        {
                            return ARPGHostedInsertAllowsStairSideNeighbor(
                                BuildNeighbor, CandidateHost, SnapTarget, Piece, Final);
                        }
                        return ARPGHostedInsertAllowsStructuralNeighbor(
                            BuildNeighbor, CandidateHost, Piece, Final);
                    };

                    const AARPGBuildPieceActor* ResolvedHost = nullptr;
                    // For standard structural placement the active target may itself be the insert host.
                    // A Stair's active target is normally its Foundation/Floor/Ceiling landing, so host
                    // discovery proceeds through the overlap set / narrow semantic world fallback.
                    if (!bIncomingIsStair && SnapTarget && InsertAllowsIncomingNeighbor(SnapTarget))
                    {
                        ResolvedHost = SnapTarget;
                    }
                    else
                    {
                        TSet<const AARPGBuildPieceActor*> CheckedHosts;
                        if (SnapTarget) CheckedHosts.Add(SnapTarget);
                        for (const FOverlapResult& HostOverlap : Overlaps)
                        {
                            const AARPGBuildPieceActor* CandidateHost = Cast<AARPGBuildPieceActor>(HostOverlap.GetActor());
                            if (!CandidateHost || CandidateHost == BuildNeighbor || CheckedHosts.Contains(CandidateHost)) continue;
                            CheckedHosts.Add(CandidateHost);
                            if (!CandidateHost->Definition ||
                                !ARPGIsInsertSnapPair(CandidateHost->Definition->PieceKind, NeighborKind)) continue;
                            if (InsertAllowsIncomingNeighbor(CandidateHost))
                            {
                                ResolvedHost = CandidateHost;
                                break;
                            }
                        }

                        if (!ResolvedHost)
                        {
                            // Hollow/complex Doorway/WindowWall collision can keep the host out of the
                            // broad occupancy overlap. This fallback runs only after an actual hosted
                            // insert overlapped the incoming piece and only tests the compatible host
                            // family, so unrelated structures can never be silently ignored.
                            for (TActorIterator<AARPGBuildPieceActor> It(World); It; ++It)
                            {
                                const AARPGBuildPieceActor* CandidateHost = *It;
                                if (!CandidateHost || CandidateHost == BuildNeighbor || CheckedHosts.Contains(CandidateHost) || !CandidateHost->Definition) continue;
                                if (!ARPGIsInsertSnapPair(CandidateHost->Definition->PieceKind, NeighborKind)) continue;
                                if (InsertAllowsIncomingNeighbor(CandidateHost))
                                {
                                    ResolvedHost = CandidateHost;
                                    break;
                                }
                            }
                        }
                    }

                    if (ResolvedHost)
                    {
                        if (bRequireSnapTargetModificationAccess &&
                            (!BuildNeighbor->CanActorModify(Owner) || !ResolvedHost->CanActorModify(Owner)))
                            return EARPGPlacementResult::Restricted;
                        continue;
                    }
                }

                // A Door/Window is validated as an insert inside its designated host opening.
                // Surrounding structural pieces that form legal seams with that host (lower/upper
                // Floors, Ceilings, Roofs and adjacent Wall-family framing) are not independent
                // blockers for the insert. This keeps upper-story doors usable without disabling
                // collision against duplicate inserts or genuinely conflicting structural slots.
                if (SnapTarget && ARPGIsCompatibleInsertHostStructuralNeighbor(BuildNeighbor, SnapTarget, Piece))
                {
                    if (bRequireSnapTargetModificationAccess && !BuildNeighbor->CanActorModify(Owner))
                        return EARPGPlacementResult::Restricted;
                    continue;
                }

                // A Window/Door installed after a Stair inherits its verified WindowWall/Doorway
                // host's legal Stair-boundary seam. This is the exact reverse of the hosted-insert rule
                // above and closes the remaining build-order asymmetry without weakening duplicate
                // insert or travel-volume collision checks.
                if (SnapTarget && ARPGIsCompatibleInsertHostStairNeighbor(BuildNeighbor, SnapTarget, Piece))
                {
                    if (bRequireSnapTargetModificationAccess && !BuildNeighbor->CanActorModify(Owner))
                        return EARPGPlacementResult::Restricted;
                    continue;
                }

                // Horizontal Stair landings may use either HIGH-arrival/outside-down or LOW-departure/
                // inside-up topology on the same edge. Their stringers can intentionally touch exact
                // side-wall seams, but a horizontal tile genuinely closing the travel cell or an end wall
                // across the low/high opening remains a real blocker. Direct Stair-to-Stair endpoint
                // chains are handled by native snap-neighbour validation immediately below.
                if (SnapTarget && ARPGIsCompatibleStairHostStructuralNeighbor(BuildNeighbor, SnapTarget, Piece, Final))
                {
                    if (bRequireSnapTargetModificationAccess && !BuildNeighbor->CanActorModify(Owner))
                        return EARPGPlacementResult::Restricted;
                    continue;
                }

                if (ARPGIsValidSnappedBuildNeighbor(BuildNeighbor, Piece, Final))
                {
                    // Treat an accepted seam neighbour like an additional snap relationship for access
                    // control, so a player cannot exploit seam tolerance to intersect protected builds.
                    if (bRequireSnapTargetModificationAccess && !BuildNeighbor->CanActorModify(Owner))
                        return EARPGPlacementResult::Restricted;
                    continue;
                }

                // Native socket ownership is not a complete collision contract: when the active snap
                // target is a Floor, an already-built adjacent Wall may not advertise that same final
                // transform even though both pieces occupy compatible grid slots. Classify standard
                // Walls/Floors by semantic grid occupancy before falling back to raw OBB penetration.
                const EARPGStructuralOccupancyRelation StructuralRelation =
                    ARPGClassifyStandardStructuralOccupancy(BuildNeighbor, Piece, Final);
                if (StructuralRelation == EARPGStructuralOccupancyRelation::CompatibleSeam)
                {
                    if (bRequireSnapTargetModificationAccess && !BuildNeighbor->CanActorModify(Owner))
                        return EARPGPlacementResult::Restricted;
                    continue;
                }
                if (StructuralRelation == EARPGStructuralOccupancyRelation::Conflict)
                    return EARPGPlacementResult::Blocked;

                if (!ARPGPlacementVolumesOverlapMeaningfully(BuildNeighbor, Piece, Final))
                {
                    // The physics overlap came only from authored art/collision beyond the logical build
                    // volumes (posts, braces, lips, etc.). That is decorative seam contact, not occupancy.
                    continue;
                }
            }

            return EARPGPlacementResult::Blocked;
        }
    }

    if (Piece->bRequiresSupport && !SnapTarget)
    {
        if (!Piece->bAllowGroundPlacement) return EARPGPlacementResult::Unsupported;
        FHitResult Hit;
        const FVector BottomAnchor = Final.TransformPosition(ARPGGetBuildPieceBottomAnchorLocal(Piece));
        const float ProbeLift = FMath::Max(4.f, Piece->PlacementCollisionClearance + 2.f);
        const FVector Start = BottomAnchor + FVector::UpVector * ProbeLift;
        const FVector End = BottomAnchor - FVector::UpVector * FMath::Max(1.f, Piece->SupportTraceDepth);
        if (!ARPGTracePlacementSurfaceIgnoringFoundationTrees(
            World, Piece, Start, End, PlacementCollisionChannel, Params, Hit)) return EARPGPlacementResult::Unsupported;
        if (Piece->bRequireMostlyFlatGround)
        {
            const float Slope = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(Hit.ImpactNormal.Z, -1.f, 1.f)));
            if (Slope > Piece->MaxGroundSlopeDegrees) return EARPGPlacementResult::InvalidSurface;
        }
    }
    return EARPGPlacementResult::Valid;
}

EARPGPlacementResult UARPGBuildingComponent::EvaluatePlacement(const UARPGBuildPieceDefinition* Piece, const FTransform& DesiredTransform) const
{
    AARPGBuildPieceActor* SnapTarget = nullptr;
    const FTransform Final = ResolvePlacementTransform(Piece, DesiredTransform, SnapTarget);
    return EvaluatePlacementInternal(Piece, Final, SnapTarget);
}

bool UARPGBuildingComponent::ConfirmPreviewPlacement()
{
    if (!bBuildModeActive || !SelectedBuildPiece || CurrentPreviewResult != EARPGPlacementResult::Valid) return false;
    const bool bRequested = RequestPlacePiece(SelectedBuildPiece, CurrentPreviewTransform);
    if (bRequested && !bKeepBuildModeAfterPlacement) EndBuildMode();
    return bRequested;
}

bool UARPGBuildingComponent::RequestPlacePiece(UARPGBuildPieceDefinition* Piece, const FTransform& DesiredTransform)
{
    if (!GetOwner() || !Piece) return false;
    if (!GetOwner()->HasAuthority())
    {
        ServerPlacePiece(Piece, DesiredTransform);
        return true;
    }
    return PlacePieceAuthority(Piece, DesiredTransform);
}

void UARPGBuildingComponent::ServerPlacePiece_Implementation(UARPGBuildPieceDefinition* Piece, FTransform DesiredTransform)
{
    PlacePieceAuthority(Piece, DesiredTransform);
}

UClass* UARPGBuildingComponent::ResolveNativeBuildActorClass(const UARPGBuildPieceDefinition* Piece) const
{
    if (!Piece) return nullptr;
    if (UClass* CustomClass = Piece->ActorClass.LoadSynchronous())
        if (CustomClass->IsChildOf(AARPGBuildPieceActor::StaticClass())) return CustomClass;
    switch (Piece->PieceKind)
    {
        case EARPGBuildPieceKind::Door: return AARPGBuildDoorActor::StaticClass();
        case EARPGBuildPieceKind::Window: return AARPGBuildWindowActor::StaticClass();
        case EARPGBuildPieceKind::Light: return AARPGBuildLightActor::StaticClass();
        case EARPGBuildPieceKind::Bed: return AARPGBuildBedActor::StaticClass();
        case EARPGBuildPieceKind::SettlementHub: return AARPGSettlementHubActor::StaticClass();
        case EARPGBuildPieceKind::Storage: return AARPGStorageActor::StaticClass();
        case EARPGBuildPieceKind::Production: return AARPGCraftingStationActor::StaticClass();
        default: return AARPGBuildPieceActor::StaticClass();
    }
}

bool UARPGBuildingComponent::PlacePieceAuthority(UARPGBuildPieceDefinition* Piece, const FTransform& DesiredTransform)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || !Piece || !GetWorld()) return false;
    AARPGBuildPieceActor* SnapTarget = nullptr;
    const FTransform Final = ResolvePlacementTransform(Piece, DesiredTransform, SnapTarget);
    const EARPGPlacementResult Check = EvaluatePlacementInternal(Piece, Final, SnapTarget);
    if (Check != EARPGPlacementResult::Valid)
    {
        OnPlacementResult.Broadcast(Check, nullptr);
        return false;
    }

    UClass* LoadedClass = ResolveNativeBuildActorClass(Piece);
    if (!LoadedClass)
    {
        OnPlacementResult.Broadcast(EARPGPlacementResult::NoPiece, nullptr);
        return false;
    }
    if (!ConsumeBuildResources(Piece))
    {
        OnPlacementResult.Broadcast(EARPGPlacementResult::MissingResources, nullptr);
        return false;
    }

    FActorSpawnParameters Params;
    Params.Owner = GetOwner();
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AARPGBuildPieceActor* Spawned = GetWorld()->SpawnActor<AARPGBuildPieceActor>(LoadedClass, Final, Params);
    if (!Spawned)
    {
        RefundBuildResources(Piece);
        return false;
    }

    if (AARPGSettlementHubActor* Hub = Cast<AARPGSettlementHubActor>(Spawned))
    {
        if (Hub->Inventory)
        {
            const UARPGSettlementDefinition* SettlementDef = Piece->SettlementDefinition;
            Hub->Inventory->MaxSlots = SettlementDef ? FMath::Max(1, SettlementDef->SettlementStockpileSlots) : 96;
        }
    }
    else if (AARPGStorageActor* Storage = Cast<AARPGStorageActor>(Spawned))
    {
        if (Storage->Inventory) Storage->Inventory->MaxSlots = FMath::Max(1, Piece->StorageSlots);
    }
    if (AARPGCraftingStationActor* Station = Cast<AARPGCraftingStationActor>(Spawned))
        Station->ApplyStationDefinition(Piece->StationDefinition);

    Spawned->InitializeBuilding(Piece, GetOwner());
    if (UARPGEventRouterComponent* Router = GetOwner()->FindComponentByClass<UARPGEventRouterComponent>()) Router->ReportBuilt(Piece->DefinitionId, 1);
    OnPlacementResult.Broadcast(EARPGPlacementResult::Valid, Spawned);
    return true;
}
