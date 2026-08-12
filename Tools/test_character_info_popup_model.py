#!/usr/bin/env python3
"""Static/deterministic guard for v2.10.1 automatic proximity character/NPC info popups."""
from pathlib import Path

root = Path(__file__).resolve().parents[1]
public = root / "Source" / "AkumasRPGFramework" / "Public"
private = root / "Source" / "AkumasRPGFramework" / "Private"

component_h = (public / "Components" / "ARPGCharacterInfoComponent.h").read_text(errors="replace")
component_cpp = (private / "Components" / "ARPGCharacterInfoComponent.cpp").read_text(errors="replace")
widget_h = (public / "UI" / "ARPGCharacterInfoWidget.h").read_text(errors="replace")
widget_cpp = (private / "UI" / "ARPGCharacterInfoWidget.cpp").read_text(errors="replace")
character_h = (public / "Actors" / "ARPGCharacter.h").read_text(errors="replace")
character_cpp = (private / "Actors" / "ARPGCharacter.cpp").read_text(errors="replace")

# Base-character integration: every ARPG character exposes one inherited scene/UI component.
assert "TObjectPtr<UARPGCharacterInfoComponent> CharacterInfo" in character_h
assert 'CreateDefaultSubobject<UARPGCharacterInfoComponent>(TEXT("CharacterInfo"))' in character_cpp
assert "CharacterInfo->SetupAttachment(GetRootComponent())" in character_cpp

# NPC-first defaults and editor-facing presentation controls.
for required in (
    "bEnableInfoPopup = true",
    "bShowOnAICharacters = true",
    "bShowOnPlayerControlledCharacters = false",
    "bHideLocalPlayerSelf = true",
    "bHideWhenDead = true",
    "bHideDuringSpawnEntrance = true",
    "ShowDistance = 1100.f",
    "HideDistance = 1350.f",
    "ProximityCheckInterval = 0.20f",
    "bAutoHeightFromCapsule = true",
    "bLazyCreateWidget = true",
    "bReleaseWidgetWhenFar = true",
):
    assert required in component_h, f"Missing info-popup authoring default: {required}"

# UE5.8 compile hygiene: do not shadow USceneComponent::bVisible (C4458 is fatal under the project's warning policy).
assert "SetPopupVisibleLocal(bool bShouldBeVisible, ULocalPlayer* LocalPlayer)" in component_h
assert "SetPopupVisibleLocal(bool bShouldBeVisible, ULocalPlayer* LocalPlayer)" in component_cpp
assert "SetPopupVisibleLocal(bool bVisible" not in component_h
assert "SetPopupVisibleLocal(bool bVisible" not in component_cpp

# Local-only proximity decisions stay timer-driven, but the inherited UWidgetComponent must retain
# its native TickComponent capability while visible so Screen-space projection can actually render.
assert "PrimaryComponentTick.bCanEverTick = true" in component_cpp
assert "PrimaryComponentTick.bStartWithTickEnabled = false" in component_cpp
assert "SetComponentTickEnabled(true)" in component_cpp
assert "SetComponentTickEnabled(false)" in component_cpp
assert "SetTickMode(ETickMode::Automatic)" in component_cpp
assert "RequestRenderUpdate()" in component_cpp
assert "RemoveWidgetFromScreen()" in component_cpp
assert "SetIsReplicatedByDefault(false)" in component_cpp
assert "SetTimer(ProximityTimer" in component_cpp
assert "NM_DedicatedServer" in component_cpp
assert "DOREPLIFETIME" not in component_cpp

# Show/hide hysteresis must use the larger hide range once visible.
assert "ActiveDistance = bPopupVisible ? SafeHideDistance : SafeShowDistance" in component_cpp
assert "SafeHideDistance = FMath::Max(SafeShowDistance, HideDistance)" in component_cpp

# Data comes from already-replicated framework identity/stats/progression, including scaled NPC effective level.
for required in (
    "Character->RPGCharacterName",
    "Character->Stats->GetEffectiveLevel()",
    "Character->Stats->Health",
    "Character->Stats->MaxHealth",
    "Character->Stats->GetHealthPercent()",
):
    assert required in component_cpp, f"Missing automatic info source: {required}"

# v2.9 entrance and local viewer ownership are respected.
assert "Entrance->IsGroundRiseActive()" in component_cpp
assert "GameInstance->GetLocalPlayers()" in component_cpp
assert "SetOwnerPlayer(LocalPlayer)" in component_cpp
assert "PC->LineOfSightTo(Character)" in component_cpp

# Far characters can retire UMG allocation without destroying the configured widget class.
assert "InitWidget()" in component_cpp
assert "FarWidgetReleaseDelay" in component_cpp
assert "ReleaseWidgetInstance()" in component_cpp
release_body = component_cpp.split("void UARPGCharacterInfoComponent::ReleaseWidgetInstance()", 1)[1].split("void UARPGCharacterInfoComponent::ApplySnapshotToWidget", 1)[0]
assert "SetWidget(nullptr)" in release_body
assert "SetWidgetClass(nullptr)" not in release_body

# Native fallback/base widget and zero-graph ordinary UserWidget mapping both exist.
for required in (
    "FARPGCharacterInfoSnapshot",
    "BP_OnCharacterInfoUpdated",
    "CharacterNameText",
    "LevelText",
    "HealthBar",
    "HealthText",
):
    assert required in widget_h, f"Missing native info widget API: {required}"
for required in (
    'TEXT("CharacterNameText")',
    'TEXT("LevelText")',
    'TEXT("HealthBar")',
    'TEXT("HealthText")',
    "SetPercent",
):
    assert required in widget_cpp, f"Missing native info widget field: {required}"
for required in (
    "GetWidgetFromName(NameTextWidgetName)",
    "GetWidgetFromName(LevelTextWidgetName)",
    "GetWidgetFromName(HealthBarWidgetName)",
    "GetWidgetFromName(HealthTextWidgetName)",
):
    assert required in component_cpp, f"Missing ordinary UserWidget automatic binding: {required}"

# Deterministic hysteresis sanity check.
show_distance = 1100.0
hide_distance = max(show_distance, 1350.0)
visible = False
for distance in (1500.0, 1200.0, 1099.0):
    active = hide_distance if visible else show_distance
    visible = distance <= active
assert visible, "Popup should become visible inside show distance"
for distance in (1150.0, 1300.0):
    active = hide_distance if visible else show_distance
    visible = distance <= active
assert visible, "Popup should remain visible within hide hysteresis"
active = hide_distance if visible else show_distance
visible = 1351.0 <= active
assert not visible, "Popup should hide outside hide distance"

print("Character info popup model: PASS")
