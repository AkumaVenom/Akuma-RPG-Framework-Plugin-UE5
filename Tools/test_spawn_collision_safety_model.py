#!/usr/bin/env python3
"""Static/deterministic guard for v2.7.2 collision-safe AI spawning."""
from pathlib import Path

root = Path(__file__).resolve().parents[1]
cpp = (root / "Source/AkumasRPGFramework/Private/Actors/ARPGAISpawner.cpp").read_text()
assert "AdjustIfPossibleButDontSpawnIfColliding" in cpp
assert "AdjustIfPossibleButAlwaysSpawn" not in cpp
assert "MaxSafeSpawnAttempts = 10" in cpp
assert "SpawnShape == EARPGSpawnerShape::Point && Attempt > 0" in cpp
print("Spawn collision safety model: PASS")
