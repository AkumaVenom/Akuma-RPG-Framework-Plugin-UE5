"""Destination-authored GameMode travel recovery regression model — v2.18.3."""
from pathlib import Path
import json

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "Source/AkumasRPGFramework"
H = (SRC / "Public/Frontend/ARPGFrontendGameMode.h").read_text(errors="replace")
CPP = (SRC / "Private/Frontend/ARPGFrontendGameMode.cpp").read_text(errors="replace")
NET = (SRC / "Private/Subsystems/ARPGNetworkSubsystem.cpp").read_text(errors="replace")
DESC = json.loads((ROOT / "AkumasRPGFramework.uplugin").read_text())
assert DESC["Version"] == 21805
assert DESC["VersionName"] == "2.18.5-alpha"

# Normal travel remains absolute so ordinary URL options are not intentionally inherited.
assert "UGameplayStatics::OpenLevel(World, MapName, true);" in NET
assert "UGameplayStatics::OpenLevel(World, MapName, true, Options);" in NET

# If UE/PIE still instantiates FrontendGameMode on a destination whose WorldSettings author gameplay,
# the frontend self-recovers exactly once by forcing that authored class into a fresh absolute travel.
for required in ("BeginPlay() override", "RecoverDestinationAuthoredGameModeIfNeeded"):
    assert required in H, required
for required in (
    "WorldSettings->DefaultGameMode",
    "AuthoredGameModeClass->IsChildOf(AARPGFrontendGameMode::StaticClass())",
    "UGameplayStatics::ParseOption(OptionsString, TEXT(\"ARPG_GameModeRecovery\"))",
    "TEXT(\"game=%s?ARPG_GameModeRecovery=1\")",
    "OptionsString.Contains(TEXT(\"listen\")",
    "ParseOption(OptionsString, TEXT(\"Port\"))",
    "ParseOption(OptionsString, TEXT(\"ARPG_LAN\"))",
    "UGameplayStatics::OpenLevel(this, FName(*CurrentMap), true, RecoveryOptions)",
):
    assert required in CPP, required

# Genuine frontend maps must not be redirected, and the marker prevents loops.
assert "if (!RecoveryMarker.IsEmpty())" in CPP
assert "AuthoredGameModeClass->IsChildOf(AARPGFrontendGameMode::StaticClass())" in CPP
print("frontend destination GameMode travel recovery regression model: PASS")
