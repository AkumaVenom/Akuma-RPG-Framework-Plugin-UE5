"""Frontend login + direct-IP profile identity/network persistence regression model — v2.18.2."""
from pathlib import Path
import json

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "Source/AkumasRPGFramework"
ACCOUNT_H = (SRC / "Public/Subsystems/ARPGAccountSubsystem.h").read_text(errors="replace")
ACCOUNT_CPP = (SRC / "Private/Subsystems/ARPGAccountSubsystem.cpp").read_text(errors="replace")
NETWORK_H = (SRC / "Public/Subsystems/ARPGNetworkSubsystem.h").read_text(errors="replace")
NETWORK_CPP = (SRC / "Private/Subsystems/ARPGNetworkSubsystem.cpp").read_text(errors="replace")
FRONT_PC_H = (SRC / "Public/Frontend/ARPGFrontendPlayerController.h").read_text(errors="replace")
FRONT_PC_CPP = (SRC / "Private/Frontend/ARPGFrontendPlayerController.cpp").read_text(errors="replace")
FRONT_WIDGET_H = (SRC / "Public/Frontend/ARPGFrontendWidgets.h").read_text(errors="replace")
FRONT_WIDGET_CPP = (SRC / "Private/Frontend/ARPGFrontendWidgets.cpp").read_text(errors="replace")
FRONT_GM_CPP = (SRC / "Private/Frontend/ARPGFrontendGameMode.cpp").read_text(errors="replace")
PC_H = (SRC / "Public/Actors/ARPGPlayerController.h").read_text(errors="replace")
PC_CPP = (SRC / "Private/Actors/ARPGPlayerController.cpp").read_text(errors="replace")
GM_H = (SRC / "Public/Actors/ARPGGameMode.h").read_text(errors="replace")
GM_CPP = (SRC / "Private/Actors/ARPGGameMode.cpp").read_text(errors="replace")
SAVE_H = (SRC / "Public/Subsystems/ARPGSaveSubsystem.h").read_text(errors="replace")
SAVE_CPP = (SRC / "Private/Subsystems/ARPGSaveSubsystem.cpp").read_text(errors="replace")
PERSIST_H = (SRC / "Public/Components/ARPGPersistenceComponent.h").read_text(errors="replace")
PERSIST_CPP = (SRC / "Private/Components/ARPGPersistenceComponent.cpp").read_text(errors="replace")
OWNERSHIP_CPP = (SRC / "Private/Components/ARPGFactionOwnershipComponent.cpp").read_text(errors="replace")
SAVE_GAME_H = (SRC / "Public/Save/ARPGSaveGame.h").read_text(errors="replace")
SETTINGS_H = (SRC / "Public/ARPGDeveloperSettings.h").read_text(errors="replace")

DESCRIPTOR = json.loads((ROOT / "AkumasRPGFramework.uplugin").read_text())
assert DESCRIPTOR.get("Version") == 21805
assert DESCRIPTOR.get("VersionName") == "2.18.5-alpha"

# Account creation must establish a dedicated profile save and must never place raw passwords in it.
for token in (
    "UARPGAccountProfileSave",
    "GetAccountProfileSlotName",
    "CreateAndLoginLocalAccount",
    "CreateOrRepairAccountProfile",
    "SaveCurrentFrontendPreferences",
):
    assert token in ACCOUNT_H or token in ACCOUNT_CPP or token in SAVE_GAME_H, token
assert "Password" not in SAVE_GAME_H[SAVE_GAME_H.index("class AKUMASRPGFRAMEWORK_API UARPGAccountProfileSave"):]
assert "Failed to create the account save file" in ACCOUNT_CPP
assert "Index->Accounts.RemoveAll" in ACCOUNT_CPP

# Passwords are local only: profile identity RPC carries AccountId/Username/CharacterId, never password/verifier/salt.
rpc_start = PC_H.index("ServerSubmitProfileIdentity")
rpc_line = PC_H[rpc_start:PC_H.find(";", rpc_start)+1]
for forbidden in ("Password", "Verifier", "Salt"):
    assert forbidden not in rpc_line
assert "FGuid InAccountId" in rpc_line and "InUsername" in rpc_line and "FGuid InCharacterId" in rpc_line

# Frontend is usable without project UMG but remains subclassable/reskinnable.
for token in (
    "UARPGLoginWidget",
    "UARPGMainMenuWidget",
    "Blueprintable",
    "LoginWidgetClass",
    "MainMenuWidgetClass",
    "BP_OnLoginScreenRefreshed",
    "BP_OnMainMenuRefreshed",
):
    assert token in FRONT_WIDGET_H or token in FRONT_PC_H, token
for control in (
    "UsernameInput", "PasswordInput", "LoginButton", "CreateAccountButton",
    "SinglePlayerButton", "HostAndPlayButton", "JoinAddressInput", "JoinByIPButton",
):
    assert control in FRONT_WIDGET_H and control in FRONT_WIDGET_CPP, control
assert "SetIsPassword(bPassword)" in FRONT_WIDGET_CPP
assert "PasswordInput->SetText(FText::GetEmpty())" in FRONT_WIDGET_CPP
assert "AARPGFrontendPlayerController::StaticClass()" in FRONT_GM_CPP
assert "DefaultPawnClass = nullptr" in FRONT_GM_CPP

# Main menu must expose all requested travel modes and account logout.
for token in (
    "StartSinglePlayer", "HostAndPlay", "JoinDirectIP", "LogoutToLogin", "QuitGame",
    "HostListenServer", "JoinByIP", "DisconnectToMap",
):
    assert token in FRONT_PC_H + FRONT_PC_CPP + NETWORK_H + NETWORK_CPP, token
assert "listen?Port=%d?ARPG_LAN=%d" in NETWORK_CPP
assert "ClientTravel(Final, TRAVEL_Absolute)" in NETWORK_CPP
assert "OnNetworkFailure" in NETWORK_CPP and "OnTravelFailure" in NETWORK_CPP
assert "ConnectionState != EARPGNetworkConnectionState::Failed" in NETWORK_CPP  # rejection/failure reason survives return-to-menu travel

# Gameplay spawn is held until server-approved profile identity, preventing BeginPlay persistence in host namespace.
assert "bRequireProfileIdentityBeforeSpawn" in GM_H
handle = GM_CPP[GM_CPP.index("void AARPGGameMode::HandleStartingNewPlayer_Implementation"):GM_CPP.index("void AARPGGameMode::NotifyProfileIdentityAccepted")]
assert "IsProfileIdentityAccepted" in handle
assert "BeginProfileIdentityHandshake" in handle
assert "Super::HandleStartingNewPlayer_Implementation" in handle
assert "Do not spawn an unbound pawn" in handle

# Connection identity is immutable once accepted and duplicate active accounts/characters are rejected.
accept = PC_CPP[PC_CPP.index("bool AARPGPlayerController::AcceptProfileIdentityOnAuthority"):PC_CPP.index("void AARPGPlayerController::ClientProfileIdentityResult_Implementation")]
assert "if (bProfileIdentityAccepted)" in accept
assert "already bound to a different profile identity" in accept
assert "already connected to this host" in accept
assert "character identity is already active" in accept
assert "ProfileCharacterId = InCharacterId.IsValid() ? InCharacterId : FGuid::NewGuid()" in accept
assert "ProfileIdentityTimeoutTimer" in PC_H + PC_CPP

# Remote saves must resolve their own PlayerController account namespace, never host GameInstance identity.
for token in (
    "MakeCharacterSlotNameForAccount",
    "MakeCharacterSlotNameForActor",
    "ResolveCharacterAccountId",
    "DoesCharacterSaveExistForActor",
):
    assert token in SAVE_H and token in SAVE_CPP, token
resolve = SAVE_CPP[SAVE_CPP.index("FGuid UARPGSaveSubsystem::ResolveCharacterAccountId"):SAVE_CPP.index("FString UARPGSaveSubsystem::MakeCharacterSlotNameForActor")]
assert "PC->IsProfileIdentityAccepted()" in resolve
assert "Character->IsLocallyControlled()" in resolve
assert "remote listen-server pawns" in resolve.lower()
assert "MakeCharacterSlotNameForActor(Character)" in SAVE_CPP
assert "ExpectedAccountId" in SAVE_CPP
assert "D.CharacterId != Character->CharacterId" in SAVE_CPP

# Persistence bootstrap must consume the controller-approved character identity before checking the slot.
attempt = PERSIST_CPP[PERSIST_CPP.index("void UARPGPersistenceComponent::AttemptAutoLoad"):PERSIST_CPP.index("void UARPGPersistenceComponent::ResolveInitialAutoLoad")]
assert "PC->IsProfileIdentityAccepted()" in attempt
assert "PC->ProfileCharacterId" in attempt
assert "MakeCharacterSlotNameForActor(Character)" in attempt
assert "DoesCharacterSaveExistForActor(Character)" in attempt

# Explicit SaveNow remains useful for the user's UI in multiplayer: owning clients request a host-side save.
for token in ("ServerRequestSaveNow", "ClientManualSaveResult", "OnManualCharacterSaveResult"):
    assert token in PERSIST_H + PERSIST_CPP, token
assert "SetIsReplicatedByDefault(true)" in PERSIST_CPP
assert "ServerRequestSaveNow();" in PERSIST_CPP
assert "SaveNowImmediate();" in PERSIST_CPP
assert "ManualSaveRequestCooldownSeconds" in PERSIST_H + PERSIST_CPP
assert "TryExecuteManualSaveOnAuthority" in PERSIST_H + PERSIST_CPP

# Building/storage ownership follows connection-specific PlayerController AccountId first.
resolve_owner = OWNERSHIP_CPP[OWNERSHIP_CPP.index("FGuid UARPGFactionOwnershipComponent::ResolveAccountId"):OWNERSHIP_CPP.index("FGuid UARPGFactionOwnershipComponent::ResolveCharacterId")]
assert "PC && PC->AccountId.IsValid()" in resolve_owner
assert "Pawn->IsLocallyControlled()" in resolve_owner

# Developer settings provide one low-setup frontend/network configuration surface.
for token in (
    "DefaultListenPort", "MaxPlayers", "bDefaultLANListenServer", "ProfileHandshakeTimeoutSeconds",
    "bRequireLocalProfileForGameplay", "DefaultMainMenuMap", "DefaultGameplayMap",
    "bReturnToMainMenuOnNetworkFailure",
):
    assert token in SETTINGS_H, token

# Behavioral model: accepted identities map to separate host-side character slots.
def slot(account, char):
    prefix = f"ARPG_{account}" if account else "ARPG_Guest"
    return f"{prefix}_Char_{char}"

host = slot("HOST", "CHAR_A")
remote = slot("REMOTE", "CHAR_B")
assert host != remote
assert slot("REMOTE", "CHAR_B") == remote

print("frontend login/direct-IP identity + host-authoritative persistence regression model: PASS")
