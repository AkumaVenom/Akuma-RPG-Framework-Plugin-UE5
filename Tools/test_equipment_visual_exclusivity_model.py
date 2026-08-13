from dataclasses import dataclass

@dataclass
class Item:
    name: str
    logical_slot: str
    visual_socket: str
    equipped: bool = False

class EquipmentState:
    def __init__(self, items):
        self.items = items

    def equip(self, target):
        for other in self.items:
            if other is target or not other.equipped:
                continue
            same_logical_slot = other.logical_slot == target.logical_slot
            same_physical_socket = bool(other.visual_socket) and other.visual_socket == target.visual_socket
            if same_logical_slot or same_physical_socket:
                other.equipped = False
        target.equipped = True

    def projected_visuals(self, preferred=None):
        by_socket = {}
        for item in self.items:
            if not item.equipped:
                continue
            socket = item.visual_socket
            if not socket:
                continue
            existing = by_socket.get(socket)
            if existing is None or (item is preferred and existing is not preferred):
                by_socket[socket] = item
        return list(by_socket.values())

# Inventory -> Quick Access: Tool and Weapon use different gameplay slots but the same right-hand socket.
axe = Item("Stone Axe", "Equipment.Tool.MainHand", "weapon_r", True)
sword = Item("Iron Sword", "Equipment.Weapon.MainHand", "weapon_r", False)
state = EquipmentState([axe, sword])
state.equip(sword)
assert sword.equipped and not axe.equipped, "Quick Access equip must retire Inventory-equipped held item"
assert state.projected_visuals(preferred=sword) == [sword]

# Quick Access -> Inventory must be equally exclusive.
axe.equipped = False
sword.equipped = True
state.equip(axe)
assert axe.equipped and not sword.equipped, "Inventory equip must retire Quick-Access-equipped held item"
assert state.projected_visuals(preferred=sword) == [axe]

# Different physical sockets remain independent (e.g. weapon + offhand/armor).
shield = Item("Shield", "Equipment.OffHand", "hand_l", True)
state.items.append(shield)
state.equip(sword)
assert sword.equipped and shield.equipped and not axe.equipped
assert {item.name for item in state.projected_visuals(preferred=sword)} == {"Iron Sword", "Shield"}

# Legacy/corrupt duplicate state still projects only one visual on the same socket.
axe.equipped = True
sword.equipped = True
visuals = state.projected_visuals(preferred=sword)
assert sword in visuals and axe not in visuals, "visual safety net must never render both hand meshes"

print("equipment visual exclusivity model: PASS")
