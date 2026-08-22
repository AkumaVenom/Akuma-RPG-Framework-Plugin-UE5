"""Frontend -> gameplay possession/input/fresh-profile bootstrap regression model — v2.18.2."""
from pathlib import Path
import json

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "Source/AkumasRPGFramework"
ACCOUNT_H = (SRC / "Public/Subsystems/ARPGAccountSubsystem.h").read_text(errors="replace")
ACCOUNT_CPP = (SRC / "Private/Subsystems/ARPGAccountSubsystem.cpp").read_text(errors="replace")
FRONT_H = (SRC / "Public/Frontend/ARPGFrontendPlayerController.h").read_text(errors="replace")
FRONT_CPP = (SRC / "Private/Frontend/ARPGFrontendPlayerController.cpp").read_text(errors="replace")
PC_H = (SRC / "Public/Actors/ARPGPlayerController.h").read_text(errors="replace")
PC_CPP = (SRC / "Private/Actors/ARPGPlayerController.cpp").read_text(errors="replace")
GM_CPP = (SRC / "Private/Actors/ARPGGameMode.cpp").read_text(errors="replace")
PERSIST_CPP = (SRC / "Private/Components/ARPGPersistenceComponent.cpp").read_text(errors="replace")
INV_CPP = (SRC / "Private/Components/ARPGInventoryComponent.cpp").read_text(errors="replace")
DESCRIPTOR = json.loads((ROOT / "AkumasRPGFramework.uplugin").read_text())

assert DESCRIPTOR["Version"] == 21805
assert DESCRIPTOR["VersionName"] == "2.18.5-alpha"

# Frontend UIOnly must be explicitly relinquished before every gameplay travel path.
assert "void AARPGFrontendPlayerController::PrepareForGameplayTravel()" in FRONT_CPP
prep = FRONT_CPP[FRONT_CPP.index("void AARPGFrontendPlayerController::PrepareForGameplayTravel()"):FRONT_CPP.index("void AARPGFrontendPlayerController::ShowLoginScreen")]
for required in (
    "RemoveActiveFrontendWidget()",
    "FInputModeGameOnly",
    "SetInputMode(Mode)",
    "bShowMouseCursor = false",
    "SetIgnoreMoveInput(false)",
    "SetIgnoreLookInput(false)",
):
    assert required in prep, required
for fn in ("StartSinglePlayer", "HostAndPlay", "JoinDirectIP"):
    start = FRONT_CPP.index(f"bool AARPGFrontendPlayerController::{fn}")
    end = FRONT_CPP.find("\nbool AARPGFrontendPlayerController::", start + 10)
    if end < 0: end = FRONT_CPP.find("\nvoid AARPGFrontendPlayerController::", start + 10)
    if end < 0: end = len(FRONT_CPP)
    assert "PrepareForGameplayTravel();" in FRONT_CPP[start:end], fn

# Gameplay controller supplies a second safety net after travel.
assert "bRestoreGameplayInputOnBeginPlay = true" in PC_H
assert "ApplyGameplayInputModeIfNeeded();" in PC_CPP
apply_start = PC_CPP.index("void AARPGPlayerController::ApplyGameplayInputModeIfNeeded")
apply_end = PC_CPP.index("FString AARPGPlayerController::SanitizeProfileUsername", apply_start)
apply = PC_CPP[apply_start:apply_end]
for required in ("FInputModeGameOnly", "bShowMouseCursor = false", "SetIgnoreMoveInput(false)", "SetIgnoreLookInput(false)"):
    assert required in apply, required

# Account creation/login must guarantee a stable CharacterId BEFORE map travel.
assert "GetOrCreateLastCharacterId" in ACCOUNT_H and "GetOrCreateLastCharacterId" in ACCOUNT_CPP
create_start = ACCOUNT_CPP.index("bool UARPGAccountSubsystem::CreateLocalAccount")
create_end = ACCOUNT_CPP.index("bool UARPGAccountSubsystem::CreateAndLoginLocalAccount", create_start)
create = ACCOUNT_CPP[create_start:create_end]
assert "NewAccount.LastCharacterId = FGuid::NewGuid()" in create
assert "NewAccount.CharacterIds.Add(NewAccount.LastCharacterId)" in create
login_start = ACCOUNT_CPP.index("bool UARPGAccountSubsystem::LoginLocalAccount")
login_end = ACCOUNT_CPP.index("void UARPGAccountSubsystem::Logout", login_start)
assert "GetOrCreateLastCharacterId().IsValid()" in ACCOUNT_CPP[login_start:login_end]
for fn in ("StartSinglePlayer", "HostAndPlay", "JoinDirectIP"):
    start = FRONT_CPP.index(f"bool AARPGFrontendPlayerController::{fn}")
    end = FRONT_CPP.find("\nbool AARPGFrontendPlayerController::", start + 10)
    if end < 0: end = FRONT_CPP.find("\nvoid AARPGFrontendPlayerController::", start + 10)
    if end < 0: end = len(FRONT_CPP)
    assert "GetOrCreateLastCharacterId().IsValid()" in FRONT_CPP[start:end], fn

# Local identity acceptance must not re-enter GameMode spawn/possession. Only a held remote handshake resumes spawn.
assert "bProfileIdentitySpawnPending" in PC_H
handshake_start = PC_CPP.index("void AARPGPlayerController::BeginProfileIdentityHandshake")
handshake_end = PC_CPP.index("void AARPGPlayerController::HandleProfileIdentityTimeout", handshake_start)
assert "bProfileIdentitySpawnPending = true" in PC_CPP[handshake_start:handshake_end]
accept_start = PC_CPP.index("bool AARPGPlayerController::AcceptProfileIdentityOnAuthority")
accept_end = PC_CPP.index("void AARPGPlayerController::ClientProfileIdentityResult_Implementation", accept_start)
accept = PC_CPP[accept_start:accept_end]
assert "const bool bResumeDeferredSpawn = bProfileIdentitySpawnPending" in accept
assert "if (bResumeDeferredSpawn)" in accept
assert "GameMode->NotifyProfileIdentityAccepted(this)" in accept

# GameMode still owns normal local spawn once identity is accepted.
handle_start = GM_CPP.index("void AARPGGameMode::HandleStartingNewPlayer_Implementation")
handle_end = GM_CPP.index("void AARPGGameMode::NotifyProfileIdentityAccepted", handle_start)
handle = GM_CPP[handle_start:handle_end]
assert "InitializeAuthorityProfileIdentityFromLocalAccount()" in handle
assert "if (RPGPC->IsProfileIdentityAccepted())" in handle
assert "Super::HandleStartingNewPlayer_Implementation(NewPlayer)" in handle

# Persistence fallback also repairs/migrates a missing stable account character identity.
attempt_start = PERSIST_CPP.index("void UARPGPersistenceComponent::AttemptAutoLoad")
attempt_end = PERSIST_CPP.index("void UARPGPersistenceComponent::ResolveInitialAutoLoad", attempt_start)
attempt = PERSIST_CPP[attempt_start:attempt_end]
assert "Accounts->GetOrCreateLastCharacterId()" in attempt
assert "ResolveStartingItemsAfterInitialPersistence" in INV_CPP

# Simple state-model checks: one account retains one character ID; a second account differs.
records = {}
def get_or_create(account):
    records.setdefault(account, f"char-{len(records)+1}")
    return records[account]
assert get_or_create("A") == get_or_create("A")
assert get_or_create("A") != get_or_create("B")

print("frontend -> gameplay input/identity/bootstrap regression model: PASS")
