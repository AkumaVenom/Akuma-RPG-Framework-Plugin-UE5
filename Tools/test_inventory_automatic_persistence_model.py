"""Deterministic automatic player Inventory + Quick Access persistence/bootstrap regression model — v2.18.2 retaining v2.17.3 guarantees."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "Source/AkumasRPGFramework"
INV_CPP = (SRC / "Private/Components/ARPGInventoryComponent.cpp").read_text(encoding="utf-8", errors="ignore")
PERSIST_H = (SRC / "Public/Components/ARPGPersistenceComponent.h").read_text(encoding="utf-8", errors="ignore")
PERSIST_CPP = (SRC / "Private/Components/ARPGPersistenceComponent.cpp").read_text(encoding="utf-8", errors="ignore")
QUICK_H = (SRC / "Public/Components/ARPGQuickAccessComponent.h").read_text(encoding="utf-8", errors="ignore")
SAVE_CPP = (SRC / "Private/Subsystems/ARPGSaveSubsystem.cpp").read_text(encoding="utf-8", errors="ignore")
SAVE_GAME_H = (SRC / "Public/Save/ARPGSaveGame.h").read_text(encoding="utf-8", errors="ignore")
UPLUGIN = (ROOT / "AkumasRPGFramework.uplugin").read_text(encoding="utf-8", errors="ignore")
README = (ROOT / "README.md").read_text(encoding="utf-8", errors="ignore")
DOC = (ROOT / "Docs/EQUIPMENT_INVENTORY.md").read_text(encoding="utf-8", errors="ignore")
QUICK_DOC = (ROOT / "Docs/QUICK_ACCESS.md").read_text(encoding="utf-8", errors="ignore")

# Release/version contract.
assert '"Version": 21805' in UPLUGIN
assert '"VersionName": "2.18.5-alpha"' in UPLUGIN
assert "v2.18.5-alpha" in README and "v2.17.3-alpha" in README

# v2.17.1 startup ownership remains: persistence decides whether Starting Items may seed.
for token in (
    "ResolveStartingItemsAfterInitialPersistence",
    "Persistence->bAutoLoadOnBeginPlay",
    "Persistence->HasInitialAutoLoadResolved()",
    "Persistence->DidInitialCharacterSaveExist()",
):
    assert token in INV_CPP, token
resolve_start = INV_CPP.index("void UARPGInventoryComponent::ResolveStartingItemsAfterInitialPersistence")
resolve_end = INV_CPP.index("bool UARPGInventoryComponent::ApplyStartingItems", resolve_start)
resolve = INV_CPP[resolve_start:resolve_end]
assert "if (bExistingCharacterSave)" in resolve
assert "bStartingItemsApplied = true;" in resolve


# v2.17.3 bootstrap must distinguish a genuinely fresh character from a proven existing save/load failure.
for token in (
    "EARPGInitialCharacterPersistenceState",
    "FreshCharacterNoSave",
    "LoadedExistingSave",
    "ExistingSaveLoadFailed",
    "InitialResolvedCharacterId",
    "InitialResolvedCharacterSaveSlot",
):
    assert token in PERSIST_H or token in PERSIST_CPP, token

attempt_start = PERSIST_CPP.index("void UARPGPersistenceComponent::AttemptAutoLoad")
attempt_end = PERSIST_CPP.index("void UARPGPersistenceComponent::ResolveInitialAutoLoad", attempt_start)
attempt = PERSIST_CPP[attempt_start:attempt_end]
assert "const bool bExistingSaveFound" in attempt
assert "if (!bExistingSaveFound)" in attempt
assert "ResolveInitialAutoLoad(EARPGInitialCharacterPersistenceState::FreshCharacterNoSave)" in attempt
assert "SaveNowImmediate();" in attempt
assert "if (LoadNow())" in attempt
assert "LoadedExistingSave" in attempt
assert "ExistingSaveLoadFailed" in attempt

resolve_boot_start = PERSIST_CPP.index("void UARPGPersistenceComponent::ResolveInitialAutoLoad")
resolve_boot_end = PERSIST_CPP.index("void UARPGPersistenceComponent::HandleAutoSave", resolve_boot_start)
resolve_boot = PERSIST_CPP[resolve_boot_start:resolve_boot_end]
assert "bInitialCharacterSaveFound = NewState == EARPGInitialCharacterPersistenceState::LoadedExistingSave" in resolve_boot
assert "ExistingSaveLoadFailed" in resolve_boot
assert "Inventory->ResolveStartingItemsAfterInitialPersistence(bInitialCharacterSaveFound)" in resolve_boot

# Persistence BeginPlay must never leave an auto-load-enabled character waiting forever solely due to class inheritance.
begin_start = PERSIST_CPP.index("void UARPGPersistenceComponent::BeginPlay")
begin_end = PERSIST_CPP.index("void UARPGPersistenceComponent::AttemptAutoLoad", begin_start)
begin = PERSIST_CPP[begin_start:begin_end]
assert "if (!ARPGIsAccountCharacterPersistenceOwner(GetOwner())) return;" not in begin
assert "SetTimerForNextTick(this, &UARPGPersistenceComponent::AttemptAutoLoad)" in begin

# v2.17.2 must make Inventory + Quick Access mutations first-class persistence triggers.
for token in (
    "bAutoSaveCharacterStateChanges = true",
    "CharacterStateSaveDebounceSeconds = 1.5f",
    "bCharacterStateSavePending",
    "AutomaticCharacterStateSaveCount",
    "FlushPendingCharacterStateSave",
    "HandleInventoryChangedForPersistence",
    "HandleQuickAccessChangedForPersistence",
    "HandleActiveQuickAccessChangedForPersistence",
):
    assert token in PERSIST_H or token in PERSIST_CPP, token

for bind in (
    "Inventory->OnInventoryChanged.AddDynamic(this, &UARPGPersistenceComponent::HandleInventoryChangedForPersistence)",
    "QuickAccess->OnQuickAccessChanged.AddDynamic(this, &UARPGPersistenceComponent::HandleQuickAccessChangedForPersistence)",
    "QuickAccess->OnActiveQuickAccessSlotChanged.AddDynamic(this, &UARPGPersistenceComponent::HandleActiveQuickAccessChangedForPersistence)",
):
    assert bind in PERSIST_CPP, bind

# Load-time broadcasts must be ignored until initial persistence resolution is complete.
schedule_start = PERSIST_CPP.index("void UARPGPersistenceComponent::ScheduleCharacterStateSave")
schedule_end = PERSIST_CPP.index("void UARPGPersistenceComponent::HandleCharacterStateSaveTimer", schedule_start)
schedule = PERSIST_CPP[schedule_start:schedule_end]
assert "if (bAutoLoadOnBeginPlay && !bInitialAutoLoadResolved) return;" in schedule
assert "if (bAutomaticSaveSuppressedAfterLoadFailure) return;" in schedule
assert "ClearTimer(CharacterStateSaveTimer)" in schedule
assert "SetTimer(CharacterStateSaveTimer" in schedule

# Automatic state write is a complete character snapshot, not an Inventory-only side file.
flush_start = PERSIST_CPP.index("bool UARPGPersistenceComponent::FlushPendingCharacterStateSave")
flush_end = PERSIST_CPP.index("bool UARPGPersistenceComponent::SaveNow()", flush_start)
flush = PERSIST_CPP[flush_start:flush_end]
assert "SaveNowImmediate()" in flush
assert "++AutomaticCharacterStateSaveCount" in flush

# Character writes are serialized synchronously so an older async snapshot cannot win after a newer one.
save_char_start = SAVE_CPP.index("bool UARPGSaveSubsystem::SaveCharacter(AActor*")
save_char_end = SAVE_CPP.index("bool UARPGSaveSubsystem::SaveCharacterImmediate", save_char_start)
save_char = SAVE_CPP[save_char_start:save_char_end]
assert "UGameplayStatics::SaveGameToSlot" in save_char
assert "UGameplayStatics::AsyncSaveGameToSlot(" not in save_char
assert "OnSaveComplete.Broadcast" in save_char

# World saving intentionally remains async; the serialization change is scoped to character snapshots.
save_world_start = SAVE_CPP.index("bool UARPGSaveSubsystem::SaveWorld")
save_world_end = SAVE_CPP.index("bool UARPGSaveSubsystem::LoadWorld", save_world_start)
assert "AsyncSaveGameToSlot" in SAVE_CPP[save_world_start:save_world_end]

# Ready Persistence SaveNow must still terminate in the synchronous authority path. v2.18 may route an
# owning client through ServerRequestSaveNow first, but authority executes TryExecuteManualSaveOnAuthority
# and that helper must call SaveNowImmediate (never an async character write).
save_now_start = PERSIST_CPP.index("bool UARPGPersistenceComponent::SaveNow()")
save_now_end = PERSIST_CPP.index("bool UARPGPersistenceComponent::SaveNowImmediate()", save_now_start)
save_now = PERSIST_CPP[save_now_start:save_now_end]
assert "TryExecuteManualSaveOnAuthority" in save_now
assert "ServerRequestSaveNow();" in save_now
manual_start = PERSIST_CPP.index("bool UARPGPersistenceComponent::TryExecuteManualSaveOnAuthority")
manual_end = PERSIST_CPP.index("void UARPGPersistenceComponent::ServerRequestSaveNow_Implementation", manual_start)
assert "SaveNowImmediate();" in PERSIST_CPP[manual_start:manual_end]

# EndPlay is a final guard for every player-character teardown reason; Destroyed must not be excluded.
end_start = PERSIST_CPP.index("void UARPGPersistenceComponent::EndPlay")
end = PERSIST_CPP[end_start:]
assert "SaveNowImmediate();" in end
assert "EndPlayReason != EEndPlayReason::Destroyed" not in end
assert "bAutomaticSaveSuppressedAfterLoadFailure" in end

# Saved payload and load order remain Inventory first, Quick Access second, with no schema bump.
load_start = SAVE_CPP.index("bool UARPGSaveSubsystem::LoadCharacter")
load_end = SAVE_CPP.index("bool UARPGSaveSubsystem::SaveWorld", load_start)
load = SAVE_CPP[load_start:load_end]
inv_pos = load.index("Character->Inventory->ReplaceInventory(InventoryToRestore)")
quick_pos = load.index("Character->QuickAccess->ReplaceQuickAccessState")
assert inv_pos < quick_pos
assert "SaveVersion = 5" in SAVE_GAME_H

# Quick Access exposes both layout and active-slot delegates required by the mutation save bridge.
assert "FARPGQuickAccessChanged OnQuickAccessChanged" in QUICK_H
assert "FARPGActiveQuickAccessSlotChanged OnActiveQuickAccessSlotChanged" in QUICK_H

# Behavioral model: state changes are debounced/latest-state-wins, existing empty saves stay empty.
class DebouncedPersistence:
    def __init__(self, debounce=1.5):
        self.debounce = debounce
        self.pending_deadline = None
        self.runtime = None
        self.disk = None
        self.commits = 0

    def load(self, saved):
        self.runtime = saved
        self.disk = saved

    def mutate(self, now, state):
        self.runtime = state
        self.pending_deadline = now + self.debounce

    def tick(self, now):
        if self.pending_deadline is not None and now >= self.pending_deadline:
            self.disk = self.runtime
            self.pending_deadline = None
            self.commits += 1

    def end_play(self):
        self.disk = self.runtime
        self.pending_deadline = None
        self.commits += 1

p = DebouncedPersistence()
p.load({"inventory": ["StarterAxe"], "hotbar": ["StarterAxe"]})
p.mutate(0.0, {"inventory": ["Ore"], "hotbar": ["Ore"]})
p.mutate(0.4, {"inventory": ["Ore", "Gem"], "hotbar": ["Gem"]})
p.tick(1.0)
assert p.commits == 0
p.tick(2.0)
assert p.commits == 1 and p.disk["inventory"] == ["Ore", "Gem"]

# Immediate teardown before debounce still saves latest runtime state.
p.mutate(3.0, {"inventory": [], "hotbar": []})
p.end_play()
assert p.disk == {"inventory": [], "hotbar": []}

# Documentation must describe the actual write-side fix, not only starter-seeding order.
for phrase in ("Save Inventory And Quick Access Changes Automatically", "debounce", "synchronously", "Starting Items"):
    assert phrase.lower() in (DOC + QUICK_DOC).lower(), phrase


# v2.17.3 behavioral bootstrap model: no-save must seed defaults, loaded existing empty stays empty,
# and an existing-but-unreadable slot must not seed or overwrite.
def bootstrap(existing_slot: bool, load_success: bool, saved_inventory, starters):
    if not existing_slot:
        state = "FreshCharacterNoSave"
        inventory = list(starters)
        first_snapshot = list(inventory)
        return state, inventory, first_snapshot
    if load_success:
        return "LoadedExistingSave", list(saved_inventory), list(saved_inventory)
    return "ExistingSaveLoadFailed", None, None

state, inv, disk = bootstrap(False, False, None, ["StarterAxe", "StarterSword"])
assert state == "FreshCharacterNoSave"
assert inv == ["StarterAxe", "StarterSword"]
assert disk == inv

state, inv, disk = bootstrap(True, True, [], ["StarterAxe"])
assert state == "LoadedExistingSave"
assert inv == [] and disk == []

state, inv, disk = bootstrap(True, False, None, ["StarterAxe"])
assert state == "ExistingSaveLoadFailed"
assert inv is None and disk is None

print("deterministic Inventory + Quick Access persistence/bootstrap regression model: PASS")
