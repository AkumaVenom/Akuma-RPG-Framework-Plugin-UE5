from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
h = (ROOT / "Source/AkumasRPGFramework/Public/Components/ARPGProgressionComponent.h").read_text()
c = (ROOT / "Source/AkumasRPGFramework/Private/Components/ARPGProgressionComponent.cpp").read_text()
sh = (ROOT / "Source/AkumasRPGFramework/Public/Components/ARPGStatsComponent.h").read_text()

required_h = [
    'DisplayName="Base Character Level"',
    'bEnableManualLevelOverride',
    'ManualLevelOverride',
    'bResetXPWhenApplyingManualLevel',
    'void SetLevel(int32 NewLevel, bool bResetCurrentXP = false)',
    'CallInEditor',
    'bool ApplyManualLevelOverrideNow()',
    'virtual void BeginPlay() override',
]
for token in required_h:
    assert token in h, token

assert 'ApplyManualLevelOverrideNow();' in c
assert 'SetProgression(ManualLevelOverride, bResetXPWhenApplyingManualLevel ? 0 : XP);' in c
assert 'SetProgression(NewLevel, bResetCurrentXP ? 0 : XP);' in c
assert 'OnLevelChanged.Broadcast(OldLevel, Level);' in c
assert 'Category="ARPG|Stats|Level"' in sh
print("Manual level authoring/testing model: PASS")
