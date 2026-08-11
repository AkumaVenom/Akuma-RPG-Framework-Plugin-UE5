#!/usr/bin/env python3
"""Small deterministic model for v2.6 ambient NPC social interaction invariants."""
from dataclasses import dataclass, field

@dataclass
class NPC:
    name: str
    faction: str = ""
    relationships: dict = field(default_factory=dict)
    enabled: bool = True
    can_initiate: bool = True
    can_respond: bool = True
    allow_same: bool = True
    allow_friendly: bool = True
    allow_neutral: bool = True
    allow_factionless: bool = True
    tags: set = field(default_factory=set)
    required: set = field(default_factory=set)
    blocked: set = field(default_factory=set)
    interactions: dict = field(default_factory=lambda: {"Conversation": (True, True, 1.0)})
    state: str = "Idle"
    combat: bool = False
    partner: str = ""

    def relationship_to(self, other):
        if self.faction and self.faction == other.faction:
            return 100
        direct = self.relationships.get(other.faction, 0)
        if direct != 0:
            return direct
        return other.relationships.get(self.faction, 0)


def tags_accept(a, b):
    return not (a.blocked & b.tags) and (not a.required or a.required <= b.tags)


def faction_accept(a, b):
    if not a.faction or not b.faction:
        return a.allow_factionless and b.allow_factionless
    if a.faction == b.faction:
        return a.allow_same and b.allow_same
    ab, ba = a.relationship_to(b), b.relationship_to(a)
    if ab < 0 or ba < 0:
        return False
    if ab > 0 or ba > 0:
        return a.allow_friendly and b.allow_friendly
    return a.allow_neutral and b.allow_neutral


def common_interactions(a, b):
    out = []
    for iid, (init, _, weight) in a.interactions.items():
        other = b.interactions.get(iid)
        if init and weight > 0 and other and other[1]:
            out.append(iid)
    return out


def can_pair(a, b):
    return (
        a.enabled and b.enabled and a.can_initiate and b.can_respond
        and a.state == "Idle" and b.state == "Idle"
        and not a.combat and not b.combat
        and tags_accept(a, b) and tags_accept(b, a)
        and faction_accept(a, b)
        and bool(common_interactions(a, b))
    )


def reserve(a, b, iid):
    assert can_pair(a, b)
    a.state, b.state = "Approaching", "Approaching"
    a.partner, b.partner = b.name, a.name
    return iid


def begin(a, b):
    assert a.partner == b.name and b.partner == a.name
    a.state = b.state = "Interacting"


def interrupt(a, b):
    a.state = b.state = "Idle"
    a.partner = b.partner = ""


def main():
    villager_a = NPC("A", faction="Town")
    villager_b = NPC("B", faction="Town")
    assert can_pair(villager_a, villager_b)

    guard = NPC("Guard", faction="Guard", relationships={"Town": 50})
    assert can_pair(guard, villager_a), "friendly cross-faction pair should be valid"

    trader = NPC("Trader", faction="Traders")
    assert can_pair(trader, villager_a), "neutral cross-faction pair is allowed by default"

    raider = NPC("Raider", faction="Raiders", relationships={"Town": -100})
    assert not can_pair(raider, villager_a), "hostility must always win over social ambience"

    merchant = NPC("Merchant", tags={"Social.Merchant"})
    customer = NPC("Customer", required={"Social.Merchant"})
    assert can_pair(customer, merchant)
    customer.blocked.add("Social.Merchant")
    assert not can_pair(customer, merchant)
    customer.blocked.clear()

    custom_a = NPC("CustomA", interactions={"Wave": (True, True, 1.0)})
    custom_b = NPC("CustomB", interactions={"Conversation": (True, True, 1.0)})
    assert not can_pair(custom_a, custom_b), "pairs need a common locally-authored interaction id"
    custom_b.interactions["Wave"] = (False, True, 1.0)
    assert can_pair(custom_a, custom_b)

    iid = reserve(villager_a, villager_b, "Conversation")
    assert iid == "Conversation" and villager_a.state == villager_b.state == "Approaching"
    begin(villager_a, villager_b)
    assert villager_a.state == villager_b.state == "Interacting"
    villager_b.combat = True
    assert villager_b.combat
    interrupt(villager_a, villager_b)
    assert villager_a.state == villager_b.state == "Idle" and not villager_a.partner and not villager_b.partner

    print("AI social interaction model: PASS")

if __name__ == "__main__":
    main()
