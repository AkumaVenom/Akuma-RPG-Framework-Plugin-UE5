"""Explicit gameplay-GameMode frontend travel regression — v2.18.4."""
from pathlib import Path
import json

ROOT = Path(__file__).resolve().parents[1]
DESC = json.loads((ROOT / "AkumasRPGFramework.uplugin").read_text(encoding="utf-8"))
SETTINGS_H = (ROOT / "Source/AkumasRPGFramework/Public/ARPGDeveloperSettings.h").read_text(encoding="utf-8")
NET_H = (ROOT / "Source/AkumasRPGFramework/Public/Subsystems/ARPGNetworkSubsystem.h").read_text(encoding="utf-8")
NET_CPP = (ROOT / "Source/AkumasRPGFramework/Private/Subsystems/ARPGNetworkSubsystem.cpp").read_text(encoding="utf-8")
FRONT_CPP = (ROOT / "Source/AkumasRPGFramework/Private/Frontend/ARPGFrontendPlayerController.cpp").read_text(encoding="utf-8")
WIDGET_CPP = (ROOT / "Source/AkumasRPGFramework/Private/Frontend/ARPGFrontendWidgets.cpp").read_text(encoding="utf-8")

assert DESC["Version"] == 21805
assert DESC["VersionName"] == "2.18.5-alpha"

# Project-authored gameplay class is a first-class, type-restricted setting.
assert "TSoftClassPtr<AARPGGameMode> DefaultGameplayGameMode" in SETTINGS_H
assert "TSoftClassPtr<AARPGGameMode> GameplayGameMode" in NET_H
assert "ConfigureGameplayGameMode(TSoftClassPtr<AARPGGameMode>" in NET_H
assert "Settings->DefaultGameplayGameMode" in FRONT_CPP

# Local travel refuses ambiguity and validates the class contract before opening the map.
assert "BuildGameplayTravelOptions" in NET_CPP
assert "GameplayGameMode.IsNull()" in NET_CPP
assert "GameplayGameMode.LoadSynchronous()" in NET_CPP
assert "IsChildOf(AARPGGameMode::StaticClass())" in NET_CPP
assert "Default Gameplay GameMode is not configured" in NET_CPP

# Unreal's highest-priority game= URL option is explicit for both singleplayer and listen hosting.
assert 'TEXT("game=%s")' in NET_CPP
sp = NET_CPP[NET_CPP.index("bool UARPGNetworkSubsystem::StartSinglePlayer"):NET_CPP.index("bool UARPGNetworkSubsystem::HostListenServer")]
host = NET_CPP[NET_CPP.index("bool UARPGNetworkSubsystem::HostListenServer"):NET_CPP.index("FString UARPGNetworkSubsystem::NormalizeAddress")]
assert "BuildGameplayTravelOptions(false" in sp
assert "OpenLevel(World, MapName, true, Options)" in sp
assert "BuildGameplayTravelOptions(true" in host
assert "?listen?Port=%d?ARPG_LAN=%d" in NET_CPP
assert "OpenLevel(World, MapName, true, Options)" in host

# Direct-IP clients do not select the remote server's GameMode; server travel remains authoritative.
join = NET_CPP[NET_CPP.index("bool UARPGNetworkSubsystem::JoinByIP"):NET_CPP.index("void UARPGNetworkSubsystem::DisconnectToMap")]
assert "ClientTravel(Final, TRAVEL_Absolute)" in join
assert "BuildGameplayTravelOptions" not in join

# Native UI surfaces configuration failures instead of travelling silently under the wrong mode.
assert "Network->LastNetworkMessage" in WIDGET_CPP

print("PASS: explicit gameplay GameMode is forced for local frontend travel and ambiguity fails closed")
