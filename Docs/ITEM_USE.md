## Full-vital protection (v2.13.3)

Vital restoration is now a **hard usefulness gate by default**. If an Item Definition has any non-zero `Restore Health`, `Restore Mana`, or `Restore Stamina` value, at least one configured restored vital must actually be missing before the item can be used. This rule is evaluated by the local Inventory/Quick Access preflight for responsive UI and independently by server authority before custom behavior, Gameplay Effects, consumption, cooldown, or presentation.

This closes the v2.13.2 loophole where adding a `UseGameplayEffect` or custom `Item Use Behavior Class` made a full-health potion usable again. Secondary effects no longer bypass full-vital protection unless the Item Definition explicitly enables **Allow Other Effects When Restored Vitals Are Full**. Leave that option disabled for normal Health/Mana/Stamina potions. Enable it only for intentionally mixed items whose independent buff/custom effect should still be usable when the restoration portion is no longer useful.

Gameplay Effects also count toward successful item use only when GAS reports that the effect was successfully applied. A valid spec that is rejected by immunity, tags, or other Gameplay Effect requirements no longer causes consumption.

# Generic Item Use — v2.13.0-alpha

v2.13 turns the framework's earlier Quick-Access-only consumable execution into one shared, server-authoritative Item Use pipeline. Inventory UI, Quick Access and Blueprint calls now use the same validation, cooldown, consumption and presentation rules.

## Fastest setup: Health Potion with no Blueprint logic

Create or open an `ARPGItemDefinition` Data Asset and configure:

- **Use -> Usable** = enabled
- **Use -> Consume On Use** = enabled
- **Use -> Consume Quantity** = `1`
- **Use -> Cooldown Seconds** = whatever the potion should use, or `0`
- **Use -> Vitals -> Restore Health** = for example `50`
- Optional **Use -> Presentation -> Use Montage / Use Sound**

That is enough. The potion can now be used directly from the ready Inventory UI with its **Use** button or right-click, or assigned to Quick Access and activated there. It does not need a Blueprint behavior class.

If Health is already full, the use is rejected as having no useful effect and the potion is not consumed. The same rule applies to Mana/Stamina restoration. Consumption only happens after at least one configured effect succeeds.

## Built-in zero-Blueprint use fields

Every `ARPGItemDefinition` already exposes these designer paths and v2.13 routes all of them through `ARPGItemUseComponent`:

- Restore Health
- Restore Mana
- Restore Stamina
- Gameplay Effect + Gameplay Effect Level
- Consume On Use + Consume Quantity
- Use Cooldown Seconds
- Use Montage
- Use Sound + volume/pitch

These can be combined. For example, food can restore Health and apply a timed Gameplay Effect.

## Custom Blueprint item behavior

For an item whose effect cannot be described by the built-in fields:

1. Create a Blueprint Class derived from **`ARPGItemUseBehavior`**.
2. Implement **Can Use Item** for validation only.
3. Implement **Execute Item Use** for the authoritative gameplay effect and return `true` when the effect was applied.
4. Optionally implement **Play Item Use Presentation** for cosmetic VFX/UI/audio after the server confirms success.
5. Assign that Blueprint class to the Item Definition at **Use -> Custom Behavior -> Item Use Behavior Class**.
6. Keep **Usable** enabled on the Item Definition.

The behavior receives an `ARPGItemUseContext` containing:

- User (`AARPGCharacter`)
- Item Definition
- exact runtime Item Instance Id
- Item Id
- quantity before use
- consume quantity
- use source (Inventory UI, Quick Access, Blueprint/direct)
- Quick Access slot when applicable

`Execute Item Use` runs on server authority before the inventory stack is consumed. Use the supplied `User` reference to reach normal framework components such as Stats, Progression, Inventory, Equipment, Currencies, Quests, Skills, Ability System, etc.

### Good custom-use examples

- Teleport stone: validate destination/unlocked state, then move the user.
- Spell scroll: grant or activate a project-specific ability.
- Quest relic: advance custom quest state and consume only on success.
- Summoning item: spawn a server-authoritative actor/pet.
- Food/buff: use the built-in restore fields plus a custom behavior for project-specific side effects.

## Direct Blueprint calls

Every `AARPGCharacter` now automatically owns an inherited **ItemUse** component and exposes:

- `Use Inventory Item` — takes an exact runtime `InstanceId`.
- `Use First Inventory Item By Id` — convenience use by Item Id.

The ItemUse component itself exposes:

- `Use Item`
- `Use First Item By Id`
- `Get Cooldown Remaining`
- `Get Item Instance Cooldown Remaining`
- `Is Item On Cooldown`
- `On Item Used`
- `On Item Use Result`
- `On Item Use Cooldowns Changed`

Client calls are requests. The server re-resolves the exact owned runtime item, validates quantity/usability/cooldown and executes the effect authoritatively.

## Inventory UI behavior

The ready Inventory panel now has a standard `PrimaryActionButton` / `PrimaryActionText` binding.

- Equippable item -> **Equip** / **Unequip**
- Usable item -> **Use**
- Item marked both equippable and usable -> `Quick Access Action = Use` makes **Use** the preferred primary action; otherwise Auto keeps equipment-first behavior.
- Use cooldown -> button disables and shows the remaining whole seconds while the Inventory is open.
- Right-click performs the same primary action.

Custom Inventory Widget Blueprint subclasses can supply widgets named `PrimaryActionButton` and `PrimaryActionText` to receive the zero-graph native behavior.

## Quick Access compatibility

Quick Access no longer owns a second copy of potion logic. Its Use action calls `ARPGItemUseComponent::UseItemAuthority`, then keeps the existing Quick Access action-result and `OnQuickAccessItemUsed` Blueprint event behavior for compatibility.

Cooldowns are owned by ItemUse and projected back into Quick Access slots so existing cooldown UI continues to work. Using an item from Inventory cannot bypass the same item-type cooldown by moving it to another hotbar slot.

## Authority and consumption order

The authoritative order is intentionally:

1. Resolve exact owned runtime inventory instance.
2. Validate `Usable`, quantity and cooldown.
3. Run custom `Can Use Item` if configured.
4. Apply custom/built-in effects.
5. Require at least one successful effect.
6. Consume the configured quantity.
7. Start item-type cooldown.
8. Multicast cosmetic montage/sound/custom presentation.

This means a rejected use, full-vitals use, missing effect target or custom behavior returning failure does not intentionally consume the item.

Custom behavior authors should perform state-changing validation in **Can Use Item** and only mutate gameplay state in **Execute Item Use**. As with any arbitrary Blueprint gameplay callback, side effects performed inside a custom Execute event cannot be automatically rolled back if that Blueprint mutates state and then returns false.
