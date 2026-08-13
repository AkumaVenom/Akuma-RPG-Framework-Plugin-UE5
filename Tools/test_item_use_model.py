#!/usr/bin/env python3
"""Repository/model guard for v2.13.3 full-vitals consumable hard-gate fix.

This is not an Unreal compile. It verifies the intended authority semantics in a compact model and
checks the source keeps Inventory UI and Quick Access on the same ItemUse path.
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
TOL = 0.01


def can_restore(current, maximum, amount):
    return amount > 0 and maximum > 0 and current + TOL < maximum


def use_model(*, qty, consume, health, max_health, mana=100, max_mana=100, stamina=100, max_stamina=100,
              restore_health=0, restore_mana=0, restore_stamina=0, cooldown_remaining=0,
              custom_can=True, custom_applied=False, gameplay_effect_applied=False,
              has_custom=False, has_gameplay_effect=False, allow_other_when_full=False):
    if qty <= 0:
        return "unavailable", qty, health, mana, stamina
    if consume > 0 and qty < consume:
        return "insufficient", qty, health, mana, stamina
    if cooldown_remaining > 0:
        return "cooldown", qty, health, mana, stamina

    has_vital_restore = restore_health > 0 or restore_mana > 0 or restore_stamina > 0
    useful_vital = (
        (health > 0 and can_restore(health, max_health, restore_health))
        or can_restore(mana, max_mana, restore_mana)
        or can_restore(stamina, max_stamina, restore_stamina)
    )
    has_other = has_custom or has_gameplay_effect

    # v2.13.3 root fix: configured vital restoration is a hard usefulness gate by default.
    # A GE/custom behavior can bypass it only when the item explicitly opts in.
    if has_vital_restore and not useful_vital and not (allow_other_when_full and has_other):
        return "no_effect", qty, health, mana, stamina

    if has_custom and not custom_can:
        return "custom_rejected", qty, health, mana, stamina

    applied = False
    if has_custom:
        applied |= custom_applied

    if health > 0 and can_restore(health, max_health, restore_health):
        before = health
        health = min(max_health, health + restore_health)
        applied |= health > before
    if can_restore(mana, max_mana, restore_mana):
        before = mana
        mana = min(max_mana, mana + restore_mana)
        applied |= mana > before
    if can_restore(stamina, max_stamina, restore_stamina):
        before = stamina
        stamina = min(max_stamina, stamina + restore_stamina)
        applied |= stamina > before

    if has_gameplay_effect:
        # Mirrors FActiveGameplayEffectHandle::WasSuccessfullyApplied(), not merely a valid spec.
        applied |= gameplay_effect_applied

    if not applied:
        return "no_effect", qty, health, mana, stamina
    if consume > 0:
        qty -= consume
    return "success", qty, health, mana, stamina


def main():
    # Health potion: effect then exact-stack consumption.
    result = use_model(qty=3, consume=1, health=40, max_health=100, restore_health=50)
    assert result == ("success", 2, 90, 100, 100)

    # Full Health/Mana/Stamina restoratives cannot be wasted.
    result = use_model(qty=3, consume=1, health=100, max_health=100, restore_health=50)
    assert result == ("no_effect", 3, 100, 100, 100)
    result = use_model(qty=2, consume=1, health=100, max_health=100, mana=100, max_mana=100, restore_mana=40)
    assert result == ("no_effect", 2, 100, 100, 100)
    result = use_model(qty=2, consume=1, health=100, max_health=100, stamina=100, max_stamina=100, restore_stamina=40)
    assert result == ("no_effect", 2, 100, 100, 100)

    # This is the exact v2.13.2 loophole: a secondary GE/custom behavior must NOT bypass a full vital by default.
    result = use_model(qty=2, consume=1, health=100, max_health=100, restore_health=50,
                       has_gameplay_effect=True, gameplay_effect_applied=True)
    assert result == ("no_effect", 2, 100, 100, 100)
    result = use_model(qty=2, consume=1, health=100, max_health=100, restore_health=50,
                       has_custom=True, custom_applied=True)
    assert result == ("no_effect", 2, 100, 100, 100)

    # Designers can deliberately opt out for a mixed heal+buff item.
    result = use_model(qty=2, consume=1, health=100, max_health=100, restore_health=50,
                       has_gameplay_effect=True, gameplay_effect_applied=True, allow_other_when_full=True)
    assert result == ("success", 1, 100, 100, 100)

    # Multi-vital item is valid when any configured vital can really increase.
    result = use_model(qty=2, consume=1, health=100, max_health=100, mana=60, max_mana=100,
                       restore_health=25, restore_mana=25)
    assert result == ("success", 1, 100, 85, 100)

    # Tiny floating-point residue below tolerance does not waste an item.
    result = use_model(qty=2, consume=1, health=99.995, max_health=100, restore_health=50)
    assert result == ("no_effect", 2, 99.995, 100, 100)

    # Cooldown blocks before mutation/consumption.
    result = use_model(qty=3, consume=1, health=20, max_health=100, restore_health=50, cooldown_remaining=2.0)
    assert result == ("cooldown", 3, 20, 100, 100)

    # Custom-only items remain controlled by custom Can Use / Execute semantics.
    result = use_model(qty=1, consume=1, health=50, max_health=100, has_custom=True, custom_can=False, custom_applied=True)
    assert result == ("custom_rejected", 1, 50, 100, 100)
    result = use_model(qty=2, consume=1, health=50, max_health=100, has_custom=True, custom_applied=True)
    assert result == ("success", 1, 50, 100, 100)

    # A rejected GAS application cannot consume an item merely because its spec was valid.
    result = use_model(qty=2, consume=1, health=50, max_health=100,
                       has_gameplay_effect=True, gameplay_effect_applied=False)
    assert result == ("no_effect", 2, 50, 100, 100)

    item_h = (ROOT / "Source/AkumasRPGFramework/Public/Data/ARPGItemDefinition.h").read_text(errors="replace")
    behavior_h = (ROOT / "Source/AkumasRPGFramework/Public/Items/ARPGItemUseBehavior.h").read_text(errors="replace")
    use_h = (ROOT / "Source/AkumasRPGFramework/Public/Components/ARPGItemUseComponent.h").read_text(errors="replace")
    use_cpp = (ROOT / "Source/AkumasRPGFramework/Private/Components/ARPGItemUseComponent.cpp").read_text(errors="replace")
    quick_cpp = (ROOT / "Source/AkumasRPGFramework/Private/Components/ARPGQuickAccessComponent.cpp").read_text(errors="replace")
    inv_ui_h = (ROOT / "Source/AkumasRPGFramework/Public/Components/ARPGInventoryUIComponent.h").read_text(errors="replace")
    inv_ui_cpp = (ROOT / "Source/AkumasRPGFramework/Private/Components/ARPGInventoryUIComponent.cpp").read_text(errors="replace")
    widget_cpp = (ROOT / "Source/AkumasRPGFramework/Private/UI/ARPGInventoryWidgets.cpp").read_text(errors="replace")
    stats_cpp = (ROOT / "Source/AkumasRPGFramework/Private/Components/ARPGStatsComponent.cpp").read_text(errors="replace")

    assert "bAllowOtherEffectsWhenRestoredVitalsFull = false" in item_h
    assert "UseBehaviorClass" in item_h
    assert "CanUseItem" in behavior_h and "ExecuteItemUse" in behavior_h and "PlayItemUsePresentation" in behavior_h
    assert "CanUseItemNow(FGuid" in use_h
    assert "HasConfiguredBuiltInVitalRestore" in use_h and "ShouldBlockForFullConfiguredVitals" in use_h
    assert "ShouldBlockForFullConfiguredVitals(Definition, Stats)" in use_cpp
    assert "ARPGCanRestoreVital" in use_cpp
    assert "Health is already full." in use_cpp and "Mana is already full." in use_cpp and "Stamina is already full." in use_cpp
    assert use_cpp.index("ShouldBlockForFullConfiguredVitals(Definition, Stats)") < use_cpp.index("CustomBehavior->CanUseItem")
    assert "Stats->Health > Before + KINDA_SMALL_NUMBER" in use_cpp
    assert "Stats->Mana > Before + KINDA_SMALL_NUMBER" in use_cpp
    assert "Stats->Stamina > Before + KINDA_SMALL_NUMBER" in use_cpp
    assert "AppliedHandle.WasSuccessfullyApplied()" in use_cpp
    assert "bAppliedAnything = true;" not in use_cpp[use_cpp.index("if (bHasGameplayEffect)"):use_cpp.index("if (!bAppliedAnything)")]
    assert "AppliedDelta <= KINDA_SMALL_NUMBER" in stats_cpp
    assert "UseItemAuthority(OutInstanceId, EARPGItemUseSource::QuickAccess" in quick_cpp
    assert "CanUseItemNow(Entry->InstanceId)" in quick_cpp
    assert "CanUseInventoryItemNow" in inv_ui_h and "CanUseInventoryItemNow" in inv_ui_cpp
    assert "Character->ItemUse->UseItem(ItemInstanceId, EARPGItemUseSource::InventoryUI" in inv_ui_cpp
    assert "PrimaryActionButton" in widget_cpp and '"Use"' in widget_cpp
    assert "CanUseInventoryItemNow(SelectedSlotView.ItemInstanceId)" in widget_cpp

    print("PASS: v2.13.3 full-vitals hard-gate + GE application-success model")


if __name__ == "__main__":
    main()
