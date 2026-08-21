from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "Source" / "AkumasRPGFramework"

def text(path):
    return (SRC / path).read_text(errors="replace")

def require(blob, *tokens):
    for token in tokens:
        assert token in blob, f"missing: {token}"

bdh = text("Public/Data/ARPGBuildPieceDefinition.h")
wh = text("Public/Building/ARPGBuildWindowActor.h")
wc = text("Private/Building/ARPGBuildWindowActor.cpp")
ih = text("Public/Components/ARPGInteractionComponent.h")
ic = text("Private/Components/ARPGInteractionComponent.cpp")
bui = text("Private/Components/ARPGBuildingUIComponent.cpp")
building = text("Private/Building/ARPGBuildingComponent.cpp")
types = text("Public/ARPGTypes.h")
saveh = text("Public/Save/ARPGSaveGame.h")
savec = text("Private/Subsystems/ARPGSaveSubsystem.cpp")

# Data-driven skeletal Window animation authoring. A single Open sequence is sufficient because close can reverse it.
require(bdh, "WindowOpenAnimation", "WindowCloseAnimation", "WindowAnimationPlayRate",
        "WindowOpenSound", "WindowCloseSound", "bDisableWindowCollisionWhenOpen")

# Native state is authoritative, replicated, Blueprint-observable and save-restorable.
require(wh, "ReplicatedUsing=OnRep_WindowOpen", "SaveGame", "SetWindowOpen", "ToggleWindow",
        "RestoreWindowOpenState", "IsWindowOpen", "IsWindowTransitioning", "OnWindowStateChanged")
require(wc, "DOREPLIFETIME(AARPGBuildWindowActor, bWindowOpen)", "HasAuthority()",
        "IsConstructionComplete()", "CanActorUse(Requester)", "ForceNetUpdate()")

# Skeletal runtime behavior: forward Open / explicit Close / reverse fallback, with dormant tick at rest.
require(wc, "ResolveTransitionAnimation", "WindowOpenAnimation.LoadSynchronous()",
        "WindowCloseAnimation.LoadSynchronous()", "bOutReverse = true", "SetPlayRate(bReverse ? -RateMagnitude : RateMagnitude)",
        "SetPosition(bReverse ? Length : 0.f, false)", "SetComponentTickEnabled(true)", "SetComponentTickEnabled(false)")

# Collision is separated from imported Physics Assets. Opening is non-blocking immediately; closing becomes solid only at rest.
require(wh, "WindowCollision", "WindowInteractionCollision")
require(wc, "BuildMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision)",
        "BuildSkeletalMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision)",
        "if (bWindowOpen || bWindowTransitioning) bShouldCollide = false",
        "WindowInteractionCollision->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block)",
        "WindowInteractionCollision->SetCollisionEnabled(IsConstructionComplete() ? ECollisionEnabled::QueryOnly")
require(building, "Hosted inserts are singleton occupants of their semantic host socket",
        "ARPGInsertActorMatchesHost(ExistingInsert, SnapTarget)", "return EARPGPlacementResult::Blocked")

# Same player-owned interaction RPC and same AARPGCharacter view-wrapper path used by Doors.
require(ih, "ToggleBuiltWindow", "ServerToggleBuiltWindow")
require(ic, "ServerToggleBuiltWindow_Implementation", "IsActorInRange(Window)", "Window->ToggleWindow(GetOwner())")
require(bui, "Cast<AARPGBuildWindowActor>", "Character->Interaction->ToggleBuiltWindow(Window)",
        "StructureInteractionTraceChannel != ECC_Visibility")

# v2.15.48: conservative WindowWall collision may be the first trace hit. Resolve only the Window
# occupying that exact host socket instead of tracing through arbitrary world blockers.
require(bui, "ARPGWindowOccupiesWindowWallHost", "ARPGFindHostedWindowForWindowWall",
        "Host->Definition->PieceKind == EARPGBuildPieceKind::WindowWall", "Host->GetSnapTransformsFor(Window->Definition, Candidates)",
        "HandleHitOrHostedWindow", "ARPGFindHostedWindowForWindowWall(GetWorld(), Host)")

# World persistence has advanced to schema-v7 for buildable lights; the v6 Window migration remains explicit and older worlds still restore Windows closed.
require(types, "bWindowOpen = false")
require(saveh, "SaveVersion = 8")
require(savec, "R.bWindowOpen=Window->IsWindowOpen()",
        "RestoreWindowOpenState(Save->SaveVersion>=6 ? R.bWindowOpen : false)")

print("PASS: native replicated skeletal Window interaction/animation model")
