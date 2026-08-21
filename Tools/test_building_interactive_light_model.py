"""v2.15.54 interactive buildable-light regression model.

Protects the newly additive Light PieceKind and, importantly, verifies that the project-confirmed
v2.15.53 Stair/Wall-family boundary implementation is byte-identical. Buildable lights are surface
fixtures, not a new structural occupancy family.
"""
from pathlib import Path
import hashlib
import re

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "Source/AkumasRPGFramework"
DEF = (SRC / "Public/Data/ARPGBuildPieceDefinition.h").read_text(encoding="utf-8")
LIGHT_H = (SRC / "Public/Building/ARPGBuildLightActor.h").read_text(encoding="utf-8")
LIGHT_CPP = (SRC / "Private/Building/ARPGBuildLightActor.cpp").read_text(encoding="utf-8")
BUILD_CPP = (SRC / "Private/Building/ARPGBuildingComponent.cpp").read_text(encoding="utf-8")
UI_CPP = (SRC / "Private/Components/ARPGBuildingUIComponent.cpp").read_text(encoding="utf-8")
INT_H = (SRC / "Public/Components/ARPGInteractionComponent.h").read_text(encoding="utf-8")
INT_CPP = (SRC / "Private/Components/ARPGInteractionComponent.cpp").read_text(encoding="utf-8")
TYPES = (SRC / "Public/ARPGTypes.h").read_text(encoding="utf-8")
SAVE_H = (SRC / "Public/Save/ARPGSaveGame.h").read_text(encoding="utf-8")
SAVE_CPP = (SRC / "Private/Subsystems/ARPGSaveSubsystem.cpp").read_text(encoding="utf-8")

# PieceKind is appended after Custom so every pre-v2.15.54 serialized enum value remains stable.
enum_body = re.search(r"enum class EARPGBuildPieceKind\s*:\s*uint8\s*\{(.*?)\};", DEF, re.S)
assert enum_body, "PieceKind enum not found"
entries = [re.sub(r"/\*.*?\*/", "", x, flags=re.S).strip().split()[0].rstrip(",")
           for x in enum_body.group(1).splitlines() if x.strip() and not x.strip().startswith("/**")]
entries = [x for x in entries if x]
assert entries.index("Light") == entries.index("Custom") + 1, f"Light must remain immediately after Custom, got tail={entries[-6:]}"
assert entries[-2:] == ["Bed", "SettlementHub"], f"Settlement kinds must append after Light, got tail={entries[-6:]}"

for token in (
    "EARPGBuildLightPlacementMode",
    "HorizontalSurface UMETA(DisplayName=\"Ground / Foundation / Floor\")",
    "WallSurface UMETA(DisplayName=\"Built Wall Surface\")",
    "EARPGBuildLightType",
    "EARPGBuildLightFXMode",
    "LightSurfaceOffset",
    "LightMinimumSpacing",
    "LightInteractionRadius",
    "bLightStartsOn",
    "LightFadeSeconds",
    "LightComponentRelativeTransform",
    "LightIntensity",
    "LightAttenuationRadius",
    "LightColor",
    "bLightUseTemperature",
    "LightTemperature",
    "bLightCastShadows",
    "LightSourceRadius",
    "LightSoftSourceRadius",
    "LightSpotInnerConeAngle",
    "LightSpotOuterConeAngle",
    "LightNiagaraSystem",
    "LightCascadeSystem",
    "LightEffectRelativeTransform",
    "LightEmissiveMaterialParameter",
):
    assert token in DEF, f"missing build-light Data Asset property: {token}"

# Surface authoring: horizontal fixtures are exactly terrain/Foundation/Floor; wall fixtures use the
# already-proven Wall/WindowWall/Doorway family and the actual aimed face rather than a new snap graph.
for token in (
    "ARPGIsBuildLightHorizontalHostKind",
    "Kind == EARPGBuildPieceKind::Foundation || Kind == EARPGBuildPieceKind::Floor",
    "ARPGResolveBuildLightPlacementFromHit",
    "ARPGFindBuildLightSurfaceFromDesired",
    "ARPGMakeHorizontalBuildLightTransform",
    "ARPGMakeWallBuildLightTransform",
    "Hit.ImpactPoint",
    "Hit.ImpactNormal",
    "ARPGIsWallLikeKind(BuildHit->Definition->PieceKind)",
    "SurfacePoint + Normal2D * FMath::Max(0.f, Piece->LightSurfaceOffset)",
    "local +Y -> outward normal",
    "Piece->PieceKind == EARPGBuildPieceKind::Light",
    "case EARPGBuildPieceKind::Light: return AARPGBuildLightActor::StaticClass();",
):
    assert token in BUILD_CPP, f"missing native build-light placement contract: {token}"

# Light fixtures are deliberately non-structural occupants. Their imported collisions must not undo
# the confirmed Stair/Wall/Window placement combinations. Duplicate fixtures use semantic spacing.
for token in (
    "BuildMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision)",
    "BuildSkeletalMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision)",
    "LightMinimumSpacing",
    "TActorIterator<AARPGBuildLightActor>",
    "return EARPGPlacementResult::Blocked;",
):
    assert token in LIGHT_CPP or token in BUILD_CPP, f"missing non-blocking/spacing contract: {token}"

# Runtime actor: replicated state, short-lived fade ticking, Point/Spot light, Niagara and Cascade.
for token in (
    "ReplicatedUsing=OnRep_LightOn",
    "SaveGame",
    "ToggleLight",
    "RestoreLightState",
    "UPointLightComponent",
    "USpotLightComponent",
    "UNiagaraComponent",
    "UParticleSystemComponent",
):
    assert token in LIGHT_H, f"missing build-light actor API: {token}"
for token in (
    "DOREPLIFETIME(AARPGBuildLightActor, bLightOn)",
    "FMath::SmoothStep",
    "SetActorTickEnabled(bLightFadeActive)",
    "SetIntensity(Intensity)",
    "SetAttenuationRadius",
    "SetLightColor",
    "SetUseTemperature",
    "SetTemperature",
    "SetCastShadows",
    "SetSourceRadius",
    "SetSoftSourceRadius",
    "SetInnerConeAngle",
    "SetOuterConeAngle",
    "GetFXSystemAsset()",
    "NiagaraEffect->Activate",
    "CascadeEffect->Activate",
    "DeactivateImmediate()",
    "SetScalarParameterValueOnMaterials",
):
    assert token in LIGHT_CPP, f"missing build-light runtime path: {token}"

# On/Off uses the existing player-owned authoritative interaction channel; no fuel/inventory mutation
# belongs to the toggle actor path.
for token in ("ToggleBuiltLight", "ServerToggleBuiltLight"):
    assert token in INT_H, f"interaction header missing {token}"
for token in (
    "ServerToggleBuiltLight_Implementation",
    "IsActorInRange(Light)",
    "Light->ToggleLight(GetOwner())",
):
    assert token in INT_CPP, f"interaction authority missing {token}"
for forbidden in ("Consume", "Fuel", "RemoveItem", "Inventory"):
    assert forbidden not in LIGHT_CPP, f"build-light toggle must not require fuel/inventory: found {forbidden}"

# Because fixtures do not block traces/placement, interaction and demolition share bounded semantic
# view acquisition and still require the existing server RPC / modification access.
for token in (
    "ARPGFindBuildLightFromView",
    "LightInteractionRadius",
    "VisualBox.GetClosestPointTo(ViewStart)",
    "ToggleBuiltLight(Light)",
    "DemolishBuiltStructureFromView",
    "DemolishBuilding(Light)",
    "Light->CanActorModify(Character)",
):
    assert token in UI_CPP, f"semantic build-light interaction/demolition missing: {token}"

# World save v7 adds only Light state and preserves v6 Window migration behavior.
assert "bool bLightOn = false;" in TYPES
assert "SaveVersion = 8" in SAVE_H
assert "Save->SaveVersion>=7 ? R.bLightOn" in SAVE_CPP
for token in (
    "R.bLightOn=Light->IsLightOn()",
    "Save->SaveVersion>=7 ? R.bLightOn",
    "Save->SaveVersion>=6 ? R.bWindowOpen : false",
    "bLightStartsOn",
):
    assert token in SAVE_CPP, f"build-light persistence/migration missing: {token}"

# Protected baseline: these four v2.15.53 functions are intentionally byte-identical. Light support is
# additive and must not silently re-open the Stair/Wall-family blocking regressions already fixed in PIE.
def extract_function(text: str, name: str) -> str:
    start = text.find("static bool " + name + "(")
    assert start >= 0, f"protected function missing: {name}"
    brace = text.find("{", start)
    depth = 0
    for i in range(brace, len(text)):
        if text[i] == "{":
            depth += 1
        elif text[i] == "}":
            depth -= 1
            if depth == 0:
                return text[start:i + 1]
    raise AssertionError(f"unterminated function: {name}")

protected_hashes = {
    "ARPGIsStairWallFamilyBoundarySeam": "9c373187bbb929c99267d395876e55ebda588b1740abb05e741fb2430cda9b02",
    "ARPGIsCompatibleStairHostStructuralNeighbor": "3b21473d366e1f47685e06eda00a8f9c4849d9c9255dfae1f38141e1ba1e9639",
    "ARPGHostedInsertAllowsStairSideNeighbor": "ddb193165d1dcac4bd352b555744ee61fafcaa284e56f3bed8ac7d7891b3c6f4",
    "ARPGIsCompatibleInsertHostStairNeighbor": "b43be840f382ed45dfa482d5139a883a6b669db19a3a3a023f736b35ad154e14",
}
for name, expected in protected_hashes.items():
    actual = hashlib.sha256(extract_function(BUILD_CPP, name).encode()).hexdigest()
    assert actual == expected, f"protected v2.15.53 Stair/Wall function changed: {name} {actual} != {expected}"

print("building interactive light surface-placement regression model: PASS")
