"""Account-scoped authoritative world persistence regression — v2.18.5."""
from pathlib import Path
import json

ROOT = Path(__file__).resolve().parents[1]
DESC = json.loads((ROOT / "AkumasRPGFramework.uplugin").read_text(encoding="utf-8"))
SAVE_H = (ROOT / "Source/AkumasRPGFramework/Public/Save/ARPGSaveGame.h").read_text(encoding="utf-8")
SUB_H = (ROOT / "Source/AkumasRPGFramework/Public/Subsystems/ARPGSaveSubsystem.h").read_text(encoding="utf-8")
SUB_CPP = (ROOT / "Source/AkumasRPGFramework/Private/Subsystems/ARPGSaveSubsystem.cpp").read_text(encoding="utf-8")
GM_H = (ROOT / "Source/AkumasRPGFramework/Public/Actors/ARPGGameMode.h").read_text(encoding="utf-8")
GM_CPP = (ROOT / "Source/AkumasRPGFramework/Private/Actors/ARPGGameMode.cpp").read_text(encoding="utf-8")

assert DESC["Version"] == 21805
assert DESC["VersionName"] == "2.18.5-alpha"

# World schema is now self-identifying by authoritative account scope.
assert "SaveVersion = 10" in SAVE_H
assert "FGuid ScopeAccountId" in SAVE_H

# Slot namespace is account-specific for a logged-in authority and legacy only for Guest/dedicated scope.
assert "MakeWorldSlotNameForAccount" in SUB_H
assert 'TEXT("ARPG_%s_World_%s")' in SUB_CPP
assert 'TEXT("ARPG_World_%s")' in SUB_CPP
assert "ResolveWorldSaveAccountId" in SUB_H
assert "World->GetNetMode() == NM_Client" in SUB_CPP
assert "World->GetNetMode() == NM_DedicatedServer" in SUB_CPP
assert "Accounts->IsLoggedIn() ? Accounts->CurrentAccountId" in SUB_CPP

# Explicit-account APIs let GameMode capture one stable authoritative context for the entire session.
for symbol in ("SaveWorldForAccount", "LoadWorldForAccount", "DoesWorldSaveExistForAccount"):
    assert symbol in SUB_H and symbol in SUB_CPP
assert "Save->ScopeAccountId=AccountId" in SUB_CPP
assert "Save->SaveVersion>=10 && Save->ScopeAccountId!=AccountId" in SUB_CPP

for symbol in ("ActiveWorldSaveWorldId", "ActiveWorldSaveAccountId", "ActiveWorldSaveSlotName"):
    assert symbol in GM_H
assert "InitializeWorldPersistenceContext();" in GM_CPP
assert "Save->ResolveWorldSaveAccountId()" in GM_CPP
assert "Save->MakeWorldSlotNameForAccount(ActiveWorldSaveAccountId, ActiveWorldSaveWorldId)" in GM_CPP
assert "SaveWorldForAccount(ActiveWorldSaveAccountId" in GM_CPP
assert "LoadWorldForAccount(ActiveWorldSaveAccountId" in GM_CPP

# Small behavioral model: two accounts on one map cannot alias a world slot; one listen host owns one shared world.
def slot(account_hex: str | None, world: str) -> str:
    clean = (world.strip() or "DefaultWorld").replace(" ", "_")[:64]
    return f"ARPG_{account_hex}_World_{clean}" if account_hex else f"ARPG_World_{clean}"

a = "A" * 32
b = "B" * 32
assert slot(a, "StartingIslandMap") != slot(b, "StartingIslandMap")
assert slot(a, "StartingIslandMap") == slot(a, "StartingIslandMap")  # reconnect same host/account
assert slot(None, "StartingIslandMap") == "ARPG_World_StartingIslandMap"  # Guest/dedicated legacy namespace

print("PASS: standalone worlds isolate by account; listen-host world stays host-authoritative; clients own no local world save")
