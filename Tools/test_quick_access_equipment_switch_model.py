from dataclasses import dataclass

@dataclass
class Item:
    name: str
    equipped: bool = False

class QuickAccessEquipmentState:
    def __init__(self):
        self.tracked = None

    def capture_active_before_reassign(self, active_item):
        if active_item and active_item.equipped:
            self.tracked = active_item

    def activate_equip(self, new_item, previous_active=None):
        previous = self.tracked if self.tracked and self.tracked.equipped else previous_active
        if previous and previous is not new_item and previous.equipped:
            previous.equipped = False
        new_item.equipped = True
        self.tracked = new_item

axe=Item("Stone Axe", True)
sword=Item("Iron Sword", False)
state=QuickAccessEquipmentState()

# Normal slot-to-slot switch.
state.tracked=axe
state.activate_equip(sword, previous_active=axe)
assert sword.equipped and not axe.equipped

# Replacing the currently active slot must retain the old held item until the new slot is activated.
axe.equipped=True
sword.equipped=False
state.tracked=None
state.capture_active_before_reassign(axe)
state.activate_equip(sword)
assert sword.equipped and not axe.equipped

print("quick-access equipment switch model: PASS")
