#!/usr/bin/env python3
"""Deterministic model for v2.7.2 Free-Roam translation proof/social ownership invariants."""

class Wanderer:
    def __init__(self, enabled=False):
        self.enabled = enabled
        self.pauses = set()
        self.timer = enabled
        self.retry = False
        self.established = False
        self.awaiting_proof = False

    def set_enabled(self, enabled):
        was = self.enabled
        self.enabled = enabled
        if enabled and not self.pauses:
            self.timer = True
            if not was or not self.established:
                self.retry = True
        elif not enabled:
            self.timer = False
            self.retry = False
            self.established = False
            self.awaiting_proof = False

    def movement_request(self, result):
        if not self.enabled or self.pauses:
            return
        if result == "RequestSuccessful":
            self.established = False
            self.awaiting_proof = True
            self.retry = False
        elif not self.established:
            self.retry = True

    def movement_proof(self, translated):
        if not self.awaiting_proof:
            return
        if translated:
            self.awaiting_proof = False
            self.established = True
            self.retry = False
        else:
            self.awaiting_proof = False
            self.established = False
            self.retry = True

    def acquire(self, reason):
        self.pauses.add(reason)
        self.timer = False
        self.retry = False
        self.awaiting_proof = False

    def release(self, reason, immediate=True):
        self.pauses.discard(reason)
        if self.enabled and not self.pauses:
            self.timer = True
            if immediate:
                self.retry = True

    @property
    def social_ready(self):
        return (not self.enabled) or self.established

def main():
    SOCIAL = "SocialInteraction"
    w = Wanderer(False)
    w.set_enabled(True)
    assert w.retry and not w.social_ready

    # Accepted request is insufficient: rotate-only/no-translation stays socially unavailable.
    w.movement_request("RequestSuccessful")
    assert w.awaiting_proof and not w.established and not w.social_ready
    w.movement_proof(False)
    assert w.retry and not w.established and not w.social_ready

    # Real translation establishes Free Roam.
    w.movement_request("RequestSuccessful")
    w.movement_proof(True)
    assert w.established and w.social_ready

    # Social pause owns only its token and releases back into a fresh movement attempt.
    w.acquire(SOCIAL)
    assert w.enabled and not w.timer
    w.release(SOCIAL, True)
    assert w.enabled and w.timer and w.retry

    print("Free-Roam translation-proof/social handoff model: PASS")

if __name__ == "__main__":
    main()
