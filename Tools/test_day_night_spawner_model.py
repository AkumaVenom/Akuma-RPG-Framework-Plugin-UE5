"""Small behavioral regression model for v2.4 day/night AI population swapping.

This intentionally models lifecycle semantics rather than Unreal object spawning: disabled mode must
preserve the legacy table, midnight replaces an active population, morning replaces it back, and an
unloaded distance population must only remember the phase (not resurrect the prior phase).
"""

class SpawnerModel:
    def __init__(self, enabled=True, active=True, day_start=6.0):
        self.enabled = enabled
        self.active = active
        self.day_start = day_start
        self.midnight = False
        self.population = []
        self.day_table = ["Chicken", "Deer"]
        self.midnight_table = ["Skeleton", "Wolf"]
        self.preserved_count = 0
        self.desired_count = 0

    def table(self):
        return self.midnight_table if self.enabled and self.midnight else self.day_table

    def is_midnight_window(self, hour):
        return self.enabled and self.day_start > 0.0 and hour < self.day_start

    def spawn(self):
        if not self.active:
            return
        self.desired_count = 2
        self.population = list(self.table())[:2]
        self.preserved_count = len(self.population)

    def sync(self, hour, force=False):
        if not self.enabled:
            return
        wanted = self.is_midnight_window(hour)
        if not force and wanted == self.midnight:
            return
        self.midnight = wanted
        self.population.clear()
        self.preserved_count = 0
        self.desired_count = 0
        if self.active:
            self.spawn()

    def activate_distance_population(self):
        self.active = True
        self.spawn()

# Disabled mode: existing population is untouched across clock changes.
s = SpawnerModel(enabled=False, active=True)
s.spawn()
legacy = list(s.population)
s.sync(0.0)
assert s.population == legacy and s.table() == s.day_table

# Enabled: normal/daylight entries remain through evening, then true midnight replaces them.
s = SpawnerModel(enabled=True, active=True)
s.sync(22.0, force=True)
assert s.population == ["Chicken", "Deer"]
s.sync(0.0)
assert s.midnight and s.population == ["Skeleton", "Wolf"]

# Midnight population remains through pre-day dawn and switches at semantic Day Start Hour.
s.sync(5.5)
assert s.midnight and s.population == ["Skeleton", "Wolf"]
s.sync(6.0)
assert not s.midnight and s.population == ["Chicken", "Deer"]

# Distance-unloaded spawner changes phase without spawning or preserving old-phase counts.
s = SpawnerModel(enabled=True, active=False)
s.preserved_count = 2
s.desired_count = 2
s.sync(1.0)
assert s.midnight and s.population == [] and s.preserved_count == 0 and s.desired_count == 0
s.activate_distance_population()
assert s.population == ["Skeleton", "Wolf"]

# Clock jumps are state-based, not dependent on observing exactly 00:00.
s = SpawnerModel(enabled=True, active=True)
s.sync(23.0, force=True)
s.sync(2.0)
assert s.midnight and s.population == ["Skeleton", "Wolf"]

print("day/night spawner population model: PASS")
