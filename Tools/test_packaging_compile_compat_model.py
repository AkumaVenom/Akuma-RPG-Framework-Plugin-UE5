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

print("Packaging compile compatibility model: PASS")
