from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
daynight = (ROOT / "Source/AkumasRPGFramework/Private/World/ARPGDayNightCycle.cpp").read_text(errors="replace")
inventory = (ROOT / "Source/AkumasRPGFramework/Private/Components/ARPGInventoryComponent.cpp").read_text(errors="replace")

assert "ExternalSunLight->GetComponent()" not in daynight
assert "ExternalMoonLight->GetComponent()" not in daynight
assert "ExternalSunLight->FindComponentByClass<UDirectionalLightComponent>()" in daynight
assert "ExternalMoonLight->FindComponentByClass<UDirectionalLightComponent>()" in daynight

# UE 5.8 warns on assigning a const raw UARPGItemDefinition* directly to TSoftObjectPtr.
assert "Entry.ItemDefinition = ExplicitDefinition" not in inventory
assert inventory.count("FSoftObjectPath(ExplicitDefinition)") >= 2


settlement_ui_h = (ROOT / "Source/AkumasRPGFramework/Public/Components/ARPGSettlementUIComponent.h").read_text(errors="replace")
settlement_ui_cpp = (ROOT / "Source/AkumasRPGFramework/Private/Components/ARPGSettlementUIComponent.cpp").read_text(errors="replace")
settlement_widgets = (ROOT / "Source/AkumasRPGFramework/Private/UI/ARPGSettlementWidgets.cpp").read_text(errors="replace")

# UE 5.8 cannot convert a forward-declared widget pointer to UObject inside an inline header accessor,
# even after TObjectPtr::Get(). Keep the BlueprintPure declarations in the header and perform
# IsValid only out-of-line where ARPGSettlementWidgets.h provides complete widget definitions.
assert 'bool IsSettlementPanelOpen() const;' in settlement_ui_h
assert 'bool IsBedPanelOpen() const;' in settlement_ui_h
assert 'IsSettlementPanelOpen() const {' not in settlement_ui_h
assert 'IsBedPanelOpen() const {' not in settlement_ui_h
assert 'bool UARPGSettlementUIComponent::IsSettlementPanelOpen() const' in settlement_ui_cpp
assert 'bool UARPGSettlementUIComponent::IsBedPanelOpen() const' in settlement_ui_cpp
assert 'return IsValid(ActiveSettlementPanel.Get());' in settlement_ui_cpp
assert 'return IsValid(ActiveBedPanel.Get());' in settlement_ui_cpp

# UE 5.8 warnings-as-errors reports C4458 when a UUserWidget member function declares local `Slot`.
assert "UCanvasPanelSlot*Slot=Canvas->AddChildToCanvas(Panel)" not in settlement_widgets
assert "UCanvasPanelSlot* HUDPanelSlot" in settlement_widgets or "UCanvasPanelSlot*HUDPanelSlot" in settlement_widgets

print("Packaging compile compatibility model: PASS")

# v2.18.1 UE5.8 frontend/network compile contracts.
network_h = (ROOT / "Source/AkumasRPGFramework/Public/Subsystems/ARPGNetworkSubsystem.h").read_text(errors="replace")
network_cpp = (ROOT / "Source/AkumasRPGFramework/Private/Subsystems/ARPGNetworkSubsystem.cpp").read_text(errors="replace")
frontend_widgets_h = (ROOT / "Source/AkumasRPGFramework/Public/Frontend/ARPGFrontendWidgets.h").read_text(errors="replace")

# FEngine network/travel failure delegates use the nested ::Type enum aliases in UE5.8.
for required in (
    "ENetworkFailure::Type FailureType",
    "ETravelFailure::Type FailureType",
):
    assert required in network_h, required
    assert required in network_cpp, required
assert "ENetworkFailure FailureType" not in network_cpp
assert "ETravelFailure FailureType" not in network_cpp

# UHT emits reflected FText/FString Blueprint event parameters as const references; declarations
# must match generated definitions exactly or the generated .gen.cpp cannot compile.
for required in (
    "BP_OnLoginStatusChanged(const FText& Message, bool bIsError)",
    "BP_OnMainMenuRefreshed(const FString& Username, FARPGFrontendSessionSettings Settings)",
    "BP_OnMainMenuStatusChanged(const FText& Message, bool bIsError)",
):
    assert required in frontend_widgets_h, required

print("v2.18.1 UE5.8 frontend/network signature compatibility: PASS")
