#pragma once

#include "CoreMinimal.h"
#include "ARPGStatTypes.generated.h"

UENUM(BlueprintType)
enum class EARPGPrimaryStat : uint8
{
    Strength UMETA(DisplayName="Strength"),
    Vitality UMETA(DisplayName="Vitality"),
    Magic UMETA(DisplayName="Magic"),
    Spirit UMETA(DisplayName="Spirit"),
    Dexterity UMETA(DisplayName="Dexterity (Speed)"),
    Luck UMETA(DisplayName="Luck")
};

UENUM(BlueprintType)
enum class EARPGDerivedStat : uint8
{
    MeleeAttackPower UMETA(DisplayName="Melee Damage / Attack Power"),
    RangedAttackPower UMETA(DisplayName="Ranged Damage / Attack Power"),
    MagicAttackPower UMETA(DisplayName="Magic Damage / Attack Power"),
    PhysicalDefense UMETA(DisplayName="Physical Defense"),
    MagicDefense UMETA(DisplayName="Magic Defense"),
    Accuracy UMETA(DisplayName="Accuracy"),
    Evasion UMETA(DisplayName="Physical Evasion"),
    MagicEvasion UMETA(DisplayName="Magic Evasion"),
    Speed UMETA(DisplayName="Speed"),
    CriticalChance UMETA(DisplayName="Critical Chance"),
    CriticalDamageMultiplier UMETA(DisplayName="Critical Damage Multiplier"),
    AttackSpeedMultiplier UMETA(DisplayName="Attack Speed Multiplier"),
    MovementSpeedMultiplier UMETA(DisplayName="Movement Speed Multiplier"),
    MaxHealth UMETA(DisplayName="Max Health"),
    MaxMana UMETA(DisplayName="Max Mana"),
    MaxStamina UMETA(DisplayName="Max Stamina")
};

USTRUCT(BlueprintType)
struct AKUMASRPGFRAMEWORK_API FARPGPrimaryStatBlock
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Primary Stats") float Strength = 10.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Primary Stats") float Vitality = 10.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Primary Stats") float Magic = 10.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Primary Stats") float Spirit = 10.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Primary Stats") float Dexterity = 10.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Primary Stats") float Luck = 10.f;
};

USTRUCT(BlueprintType)
struct AKUMASRPGFRAMEWORK_API FARPGPrimaryStatPointAllocation
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Attribute Points", meta=(ClampMin="0")) int32 Strength = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Attribute Points", meta=(ClampMin="0")) int32 Vitality = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Attribute Points", meta=(ClampMin="0")) int32 Magic = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Attribute Points", meta=(ClampMin="0")) int32 Spirit = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Attribute Points", meta=(ClampMin="0")) int32 Dexterity = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Attribute Points", meta=(ClampMin="0")) int32 Luck = 0;

    int32 GetTotalPoints() const { return Strength + Vitality + Magic + Spirit + Dexterity + Luck; }
};

USTRUCT(BlueprintType)
struct AKUMASRPGFRAMEWORK_API FARPGDerivedStatBlock
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Damage") float MeleeAttackPower = 10.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Damage") float RangedAttackPower = 10.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Damage") float MagicAttackPower = 10.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Defense") float PhysicalDefense = 0.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Defense") float MagicDefense = 0.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Combat") float Accuracy = 100.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Combat") float Evasion = 0.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Combat") float MagicEvasion = 0.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Combat") float Speed = 10.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Critical") float CriticalChance = 0.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Critical") float CriticalDamageMultiplier = 1.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Speed") float AttackSpeedMultiplier = 1.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Speed") float MovementSpeedMultiplier = 1.f;
};

USTRUCT(BlueprintType)
struct AKUMASRPGFRAMEWORK_API FARPGStatGrowthSettings
{
    GENERATED_BODY()

    // PS1-era JRPG style: natural primary stats grow automatically with level, independently of player-spent points.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Growth") FARPGPrimaryStatBlock BasePrimaryStats;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Growth", meta=(DisplayName="Primary Growth Per Level")) FARPGPrimaryStatBlock PrimaryGrowthPerLevel;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Growth", meta=(ClampMin="1.0")) float PrimaryStatCap = 255.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Growth") bool bRoundPrimaryStatsToWholeNumbers = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vitals", meta=(ClampMin="1.0")) float BaseMaxHealth = 100.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vitals", meta=(ClampMin="0.0")) float MaxHealthPerLevel = 8.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vitals", meta=(ClampMin="0.0")) float MaxHealthPerVitality = 4.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vitals", meta=(ClampMin="1.0")) float BaseMaxMana = 100.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vitals", meta=(ClampMin="0.0")) float MaxManaPerLevel = 4.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vitals", meta=(ClampMin="0.0")) float MaxManaPerMagic = 2.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vitals", meta=(ClampMin="0.0")) float MaxManaPerSpirit = 1.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vitals", meta=(ClampMin="1.0")) float BaseMaxStamina = 100.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vitals", meta=(ClampMin="0.0")) float MaxStaminaPerLevel = 3.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vitals", meta=(ClampMin="0.0")) float MaxStaminaPerVitality = 1.5f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vitals", meta=(ClampMin="0.0")) float MaxStaminaPerDexterity = 1.f;

    FARPGStatGrowthSettings()
    {
        PrimaryGrowthPerLevel.Strength = 0.80f;
        PrimaryGrowthPerLevel.Vitality = 0.75f;
        PrimaryGrowthPerLevel.Magic = 0.80f;
        PrimaryGrowthPerLevel.Spirit = 0.70f;
        PrimaryGrowthPerLevel.Dexterity = 0.75f;
        PrimaryGrowthPerLevel.Luck = 0.35f;
    }
};

USTRUCT(BlueprintType)
struct AKUMASRPGFRAMEWORK_API FARPGDerivedStatFormula
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Damage", meta=(ClampMin="0.0")) float MeleePowerPerStrength = 1.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Damage", meta=(ClampMin="0.0")) float RangedPowerPerDexterity = 0.80f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Damage", meta=(ClampMin="0.0")) float RangedPowerPerStrength = 0.20f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Damage", meta=(ClampMin="0.0")) float MagicPowerPerMagic = 1.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Defense", meta=(ClampMin="0.0")) float PhysicalDefensePerVitality = 1.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Defense", meta=(ClampMin="0.0")) float MagicDefensePerSpirit = 1.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Accuracy") float BaseAccuracy = 95.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Accuracy", meta=(ClampMin="0.0")) float AccuracyPerDexterity = 0.25f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Accuracy", meta=(ClampMin="0.0")) float EvasionPerDexterity = 0.10f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Accuracy", meta=(ClampMin="0.0")) float EvasionPerLuck = 0.05f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Accuracy", meta=(ClampMin="0.0")) float MagicEvasionPerSpirit = 0.08f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Accuracy", meta=(ClampMin="0.0")) float MagicEvasionPerLuck = 0.04f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Accuracy") bool bEnableAccuracyEvasionHitChecks = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Accuracy", meta=(EditCondition="bEnableAccuracyEvasionHitChecks", ClampMin="0.0", ClampMax="1.0")) float MinimumHitChance = 0.05f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Accuracy", meta=(EditCondition="bEnableAccuracyEvasionHitChecks", ClampMin="0.0", ClampMax="1.0")) float MaximumHitChance = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Critical", meta=(ClampMin="0.0")) float CriticalChancePerLuck = 0.0025f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Critical", meta=(ClampMin="0.0")) float CriticalDamageBonusPerLuck = 0.0015f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Speed", meta=(ClampMin="0.0")) float AttackSpeedPerSpeedPointAboveBase = 0.0025f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Speed", meta=(ClampMin="0.01")) float MinAttackSpeedMultiplier = 0.65f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Speed", meta=(ClampMin="0.01")) float MaxAttackSpeedMultiplier = 1.75f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Speed", meta=(ClampMin="0.0")) float MovementSpeedPerSpeedPointAboveBase = 0.0015f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Speed", meta=(ClampMin="0.01")) float MinMovementSpeedMultiplier = 0.75f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Speed", meta=(ClampMin="0.01")) float MaxMovementSpeedMultiplier = 1.50f;
};

USTRUCT(BlueprintType)
struct AKUMASRPGFRAMEWORK_API FARPGAttributePointSettings
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attribute Points") bool bGrantPointsOnLevelUp = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attribute Points", meta=(ClampMin="0")) int32 StartingAttributePoints = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attribute Points", meta=(ClampMin="0")) int32 AttributePointsPerLevel = 3;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attribute Points", meta=(ClampMin="0.01")) float StatValuePerPoint = 1.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attribute Points", meta=(ClampMin="0")) int32 MaxAllocatedPointsPerStat = 255;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Level Up") bool bRestoreVitalsOnLevelUp = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vitals") bool bPreserveVitalPercentWhenMaxChanges = true;
};

USTRUCT(BlueprintType)
struct AKUMASRPGFRAMEWORK_API FARPGStatModifier
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Primary") FARPGPrimaryStatBlock PrimaryAdd;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Damage") float MeleeAttackPower = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Damage") float RangedAttackPower = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Damage") float MagicAttackPower = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Defense") float PhysicalDefense = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Defense") float MagicDefense = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat") float Accuracy = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat") float Evasion = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat") float MagicEvasion = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat") float Speed = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Critical", meta=(ClampMin="-1.0", ClampMax="1.0")) float CriticalChance = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Critical") float CriticalDamageMultiplierBonus = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Speed") float AttackSpeedMultiplierBonus = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Speed") float MovementSpeedMultiplierBonus = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vitals") float MaxHealth = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vitals") float MaxMana = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vitals") float MaxStamina = 0.f;

    FARPGStatModifier()
    {
        PrimaryAdd.Strength = 0.f;
        PrimaryAdd.Vitality = 0.f;
        PrimaryAdd.Magic = 0.f;
        PrimaryAdd.Spirit = 0.f;
        PrimaryAdd.Dexterity = 0.f;
        PrimaryAdd.Luck = 0.f;
    }
};

USTRUCT(BlueprintType)
struct AKUMASRPGFRAMEWORK_API FARPGStatProgressionSaveState
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) bool bInitialized = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame) FARPGPrimaryStatPointAllocation AllocatedPoints;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, meta=(ClampMin="0")) int32 UnspentAttributePoints = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, meta=(ClampMin="0")) int32 TotalAttributePointsEarned = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, meta=(ClampMin="1")) int32 LastProcessedLevel = 1;
};

USTRUCT(BlueprintType)
struct AKUMASRPGFRAMEWORK_API FARPGStatSnapshot
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) bool bJRPGStatSystemEnabled = false;
    UPROPERTY(BlueprintReadOnly) int32 Level = 1;
    UPROPERTY(BlueprintReadOnly) FARPGPrimaryStatBlock PrimaryStats;
    UPROPERTY(BlueprintReadOnly) FARPGPrimaryStatPointAllocation AllocatedPoints;
    UPROPERTY(BlueprintReadOnly) FARPGDerivedStatBlock DerivedStats;
    UPROPERTY(BlueprintReadOnly) int32 UnspentAttributePoints = 0;
    UPROPERTY(BlueprintReadOnly) int32 TotalAttributePointsEarned = 0;
    UPROPERTY(BlueprintReadOnly) float MaxHealth = 100.f;
    UPROPERTY(BlueprintReadOnly) float MaxMana = 100.f;
    UPROPERTY(BlueprintReadOnly) float MaxStamina = 100.f;
};
