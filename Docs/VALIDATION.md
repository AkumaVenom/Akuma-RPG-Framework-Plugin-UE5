# Validation Report

Package: **Akuma's RPG Framework — 1.0.2-alpha**  
Target: **Unreal Engine 5.8 source plugin**

## Static validation performed in generation environment

The source tree was checked for:

- duplicate exported/reflected class names
- basic brace balance
- `.generated.h` placement rules
- declared Unreal RPCs without matching `_Implementation` methods
- timer bindings to incompatible non-void handlers in the validated patterns
- missing plugin-local `ARPG...` includes
- valid `.uplugin` JSON
- obvious `TODO`/`FIXME`/placeholder markers in runtime source

Result for the 1.0.2 source tree: **no issues or warnings reported by these checks**.

## Scope of this validation

These are repository/static checks only. They do **not** replace Unreal Header Tool, Unreal Build Tool, linker, PIE, dedicated/listen server, save migration, cook/package or performance testing.

The generation container does not contain the user's local UE 5.8 installation, Visual Studio toolchain integration or game project assets, so a real Unreal compile cannot be truthfully claimed here.

## Required local validation

Before relying on the plugin in a production project:

1. Build the project's Development Editor target with UE 5.8.
2. Resolve any API differences specific to the exact 5.8 point release/toolchain.
3. Open all framework-derived Blueprints and compile them.
4. Test single-player save -> exit -> relaunch -> load.
5. Test world build/chest/furnace save -> reload.
6. Run 2+ player PIE/listen-server tests for inventory/vendor/storage/crafting/quest/Slayer/building authority.
7. Test direct LAN connection on two machines.
8. Test packet loss/late join/reconnect scenarios relevant to the game.
9. Cook/package a Development build and verify Primary Asset discovery.
10. Run gameplay/performance/security QA before shipping.

## Package metrics

At final source freeze:

- 66 exported framework classes
- 113 C++ header/source files
- 6,080 C++ source lines
- zero issues/warnings from `Tools/validate_source.py`

## UE 5.8.1 build-log fixes incorporated

The 1.0.2 source tree additionally incorporates corrections found by an actual user-side UE 5.8.1 Development Editor compile attempt. That build reached UHT successfully and compiled most framework translation units before exposing a small set of C++ issues. This revision corrects the reported AttributeSet replication macro/brace error, battle-pet cooldown variable conflict, missing CharacterMovement/PlayerState includes, and mount Controller shadowing.

Because the generation environment itself does not contain UE 5.8.1, the next local build remains required to prove that no later compile/link error appears after these corrections.
