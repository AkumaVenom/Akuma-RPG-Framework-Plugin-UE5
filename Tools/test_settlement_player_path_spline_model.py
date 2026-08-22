"""Player-built Settlement spline-path regression — v2.16.12 turn-stability baseline."""
from pathlib import Path

root = Path(__file__).resolve().parents[1]
src = root / "Source" / "AkumasRPGFramework"


def read(rel):
    return (src / rel).read_text(errors="ignore")


def require(text, *tokens):
    for token in tokens:
        assert token in text, f"missing token: {token}"


piece_h = read("Public/Data/ARPGBuildPieceDefinition.h")
types_h = read("Public/ARPGTypes.h")
build_h = read("Public/Building/ARPGBuildingComponent.h")
build_cpp = read("Private/Building/ARPGBuildingComponent.cpp")
preview_h = read("Public/Building/ARPGBuildPreviewActor.h")
preview_cpp = read("Private/Building/ARPGBuildPreviewActor.cpp")
path_h = read("Public/Building/ARPGBuildPathActor.h")
path_cpp = read("Private/Building/ARPGBuildPathActor.cpp")
piece_actor_cpp = read("Private/Building/ARPGBuildPieceActor.cpp")
save_h = read("Public/Save/ARPGSaveGame.h")
save_cpp = read("Private/Subsystems/ARPGSaveSubsystem.cpp")
widgets_cpp = read("Private/UI/ARPGBuildingWidgets.cpp")

# Enum stability: the new kind must be appended after the existing Settlement Hub value.
enum_start = piece_h.index("enum class EARPGBuildPieceKind")
enum_end = piece_h.index("};", enum_start)
enum_body = piece_h[enum_start:enum_end]
assert enum_body.index("SettlementHub") < enum_body.index("SettlementPath")
assert enum_body.rstrip().endswith("SettlementPath")

# Data-driven path authoring surface.
require(
    piece_h,
    "enum class EARPGSettlementPathForwardAxis",
    "SettlementPathTerrainSampleSpacing = 125.f",
    "SettlementPathTerrainTraceHeight = 250.f",
    "SettlementPathTerrainTraceDepth = 500.f",
    "SettlementPathMinimumSegmentLength = 75.f",
    "SettlementPathMaximumSegmentLength = 1200.f",
    "SettlementPathTangentScale = 1.f",
    "bSettlementPathCollisionEnabled = false",
    "bSettlementPathCastShadow = true",
)
require(types_h, "PathSegmentTooShort", "PathSegmentTooLong")

# Continuous authoring has distinct first-anchor and segment RPCs; Cancel terminates the chain.
require(
    build_h,
    "ServerBeginSettlementPath",
    "ServerPlaceSettlementPathPoint",
    "ServerCancelSettlementPath",
    "ClientSettlementPathAnchorResult",
    "ClientSettlementPathSegmentResult",
    "bAuthoritySettlementPathActive",
    "AuthoritySettlementPathLastPoint",
    "AuthoritySettlementPathLastSegment",
    "CancelSettlementPathPlacement",
    "OnSettlementPathSessionChanged",
    "OnSettlementPathSegmentPlaced",
)
require(
    build_cpp,
    "BeginSettlementPathAuthority",
    "PlaceSettlementPathPointAuthority",
    "ResetAuthoritySettlementPathSession",
    "bSettlementPathRequestPending",
    "ServerCancelSettlementPath()",
    "EndBuildMode();",
)

# First point is an anchor only: resource consumption belongs to actual segment placement.
begin_start = build_cpp.index("EARPGPlacementResult UARPGBuildingComponent::BeginSettlementPathAuthority")
begin_end = build_cpp.index("EARPGPlacementResult UARPGBuildingComponent::PlaceSettlementPathPointAuthority", begin_start)
begin_body = build_cpp[begin_start:begin_end]
for forbidden in ("ConsumeBuildResources", "SpawnActor", "InitializePathSegment"):
    assert forbidden not in begin_body, f"first path anchor must not create/charge a segment: {forbidden}"
segment_start = begin_end
segment_end = build_cpp.index("void UARPGBuildingComponent::ServerBeginSettlementPath_Implementation", segment_start)
segment_body = build_cpp[segment_start:segment_end]
require(segment_body, "ConsumeBuildResources(Piece)", "InitializePathSegment", "AuthoritySettlementPathLastPoint = ProjectedEnd")
assert segment_body.index("ConsumeBuildResources(Piece)") < segment_body.index("AuthoritySettlementPathLastPoint = ProjectedEnd")

# Dedicated path placement cannot be bypassed through the generic one-shot placement API.
require(
    build_cpp,
    "Piece->PieceKind == EARPGBuildPieceKind::SettlementPath) return false",
    "Piece->PieceKind == EARPGBuildPieceKind::SettlementPath) return;",
)

# Preview is a real pooled spline-mesh preview rather than disconnected point decals.
require(
    preview_h,
    "SetSettlementPathSegmentPreview",
    "ClearSettlementPathSegmentPreview",
    "PathPreviewSpline",
    "PathPreviewMeshComponents",
)
require(
    preview_cpp,
    "while (PathPreviewMeshComponents.Num() < NeededMeshCount)",
    "USplineMeshComponent",
    "SetStartAndEnd",
    "SettlementPathTerrainSampleSpacing",
    "AARPGBuildPathActor* ExistingPath",
    "SampleParams.AddIgnoredActor(ExistingPath)",
    "StartTangentDirectionWorld",
    "AdjacentSpan",
    "MinimumForwardDot",
)

# Built path segment is independently replicated/persistent and terrain conforming.
require(
    path_h,
    "class AKUMASRPGFRAMEWORK_API AARPGBuildPathActor",
    "USplineComponent",
    "PathStartLocal",
    "PathEndLocal",
    "PathStartTangentLocal",
    "PathEndTangentLocal",
    "SetPathEndpointTangentsWorld",
)
require(
    path_cpp,
    "USplineMeshComponent",
    "ESplinePointType::CurveClamped",
    "DOREPLIFETIME(AARPGBuildPathActor, PathStartLocal)",
    "DOREPLIFETIME(AARPGBuildPathActor, PathEndLocal)",
    "DOREPLIFETIME(AARPGBuildPathActor, PathStartTangentLocal)",
    "DOREPLIFETIME(AARPGBuildPathActor, PathEndTangentLocal)",
    "SampleParams.AddIgnoredActor(ExistingPath)",
    "ARPGResolveStableSettlementPathEndpointTangent",
    "AdjacentSpan",
    "MinimumForwardDot",
    ".GetSafeNormal()",
)

# Turn joins share direction only; local sampled-span magnitude prevents Hermite foldover.

require(segment_body, "PreviousSegment->SetPathEndpointTangentsWorld", "Spawned->SetPathEndpointTangentsWorld")
assert "PreviousDirection + NewDirection" in segment_body or "PreviousDirection + NewDirection" in build_cpp
assert "const FVector PreviousJoinTangent = JoinDirection;" in segment_body
assert "const FVector NewJoinTangent = JoinDirection;" in segment_body
assert "PreviousSegment->GetPathSegmentLength()" not in segment_body
assert "FVector::Distance(StartPoint, ProjectedEnd)" not in segment_body
require(build_cpp, "SettlementPathLastPlacedSegment", "PreviewStartTangentWorld")

# Decorative paths are not structural occupancy/tree-regeneration blockers even if collision is enabled.
require(build_cpp, "BuildNeighbor->Definition->PieceKind == EARPGBuildPieceKind::SettlementPath")
logical_overlap_start = piece_actor_cpp.index("bool AARPGBuildPieceActor::DoesLogicalPlacementOverlapWorldCylinder")
logical_overlap_end = piece_actor_cpp.index("void AARPGBuildPieceActor::NotifyNearbyTreesOfOccupancy", logical_overlap_start)
logical_overlap = piece_actor_cpp[logical_overlap_start:logical_overlap_end]
require(logical_overlap, "Definition->PieceKind == EARPGBuildPieceKind::SettlementPath")
notify_start = logical_overlap_end
notify_end = piece_actor_cpp.index("void AARPGBuildPieceActor::RefreshNearbyTreeRespawnSuppression", notify_start)
notify_body = piece_actor_cpp[notify_start:notify_end]
require(notify_body, "Definition->PieceKind == EARPGBuildPieceKind::SettlementPath")

# World-save v9 stores confirmed geometry/tangent overrides and retains character save v5.
assert save_h.count("SaveVersion = 5") >= 1
assert "SaveVersion = 10" in save_h
require(
    types_h,
    "SettlementPathStartLocal",
    "SettlementPathEndLocal",
    "SettlementPathStartTangentLocal",
    "SettlementPathEndTangentLocal",
)
require(
    save_cpp,
    "R.SettlementPathStartLocal=Path->PathStartLocal",
    "Save->SaveVersion>=9",
    "RestorePathGeometry",
)

# Native UI communicates both the chain lifecycle and exact segment-length rejection reason.
require(
    widgets_cpp,
    "Path point is too close to the previous point",
    "Path point is too far from the previous point",
    "Place first path point",
    "Place next path point",
    "Continue until Cancel",
)

# v2.16.12 corner stability model: endpoint tangent magnitude belongs to the adjacent sampled span,
# never the full player-authored segment. This is the root regression behind the reported turn spikes.
def safe_endpoint_tangent(desired, travel, adjacent_span, minimum_forward_dot=0.15):
    import math

    def norm(v):
        length = math.sqrt(sum(x*x for x in v))
        return tuple(x / length for x in v) if length > 1e-6 else (0.0, 0.0, 0.0)

    desired_n = norm(desired)
    travel_n = norm(travel)
    dot = sum(a*b for a, b in zip(desired_n, travel_n))
    if dot < minimum_forward_dot:
        desired_n = travel_n
    return tuple(x * adjacent_span for x in desired_n)

# 90-degree road turn: old v2.16.11 magnitude could be 800 cm on a 125 cm local interval.
import math
bisector = (1.0 / math.sqrt(2.0), 1.0 / math.sqrt(2.0), 0.0)
safe = safe_endpoint_tangent(bisector, (1.0, 0.0, 0.0), 125.0)
assert abs(math.sqrt(sum(x*x for x in safe)) - 125.0) < 1e-5
# Near reversal: do not use a sideways/backward shared tangent that can fold the endpoint.
reverse_safe = safe_endpoint_tangent((-1.0, 0.0, 0.0), (1.0, 0.0, 0.0), 125.0)
assert reverse_safe == (125.0, 0.0, 0.0)

# Small behavioral model: successful segments charge exactly once and rejected points never advance.
class Session:
    def __init__(self, minimum=75.0, maximum=1200.0):
        self.minimum = minimum
        self.maximum = maximum
        self.active = False
        self.last = None
        self.segments = []
        self.costs = 0

    def begin(self, point):
        self.active = True
        self.last = point
        return "Valid"

    def place(self, point):
        assert self.active and self.last is not None
        dx, dy = point[0] - self.last[0], point[1] - self.last[1]
        length = (dx * dx + dy * dy) ** 0.5
        if length < self.minimum:
            return "PathSegmentTooShort"
        if length > self.maximum:
            return "PathSegmentTooLong"
        self.segments.append((self.last, point))
        self.last = point
        self.costs += 1
        return "Valid"

    def cancel(self):
        self.active = False
        self.last = None


session = Session()
assert session.begin((0.0, 0.0)) == "Valid"
assert session.costs == 0 and not session.segments
assert session.place((50.0, 0.0)) == "PathSegmentTooShort"
assert session.last == (0.0, 0.0) and session.costs == 0
assert session.place((1300.0, 0.0)) == "PathSegmentTooLong"
assert session.last == (0.0, 0.0) and session.costs == 0
assert session.place((300.0, 0.0)) == "Valid"
assert session.last == (300.0, 0.0) and session.costs == 1 and len(session.segments) == 1
assert session.place((600.0, 180.0)) == "Valid"
assert session.last == (600.0, 180.0) and session.costs == 2 and len(session.segments) == 2
session.cancel()
assert not session.active and session.last is None

print("Player-built Settlement spline-path regression model: PASS")
