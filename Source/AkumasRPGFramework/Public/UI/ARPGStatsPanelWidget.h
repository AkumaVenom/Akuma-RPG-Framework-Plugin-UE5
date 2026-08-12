#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Stats/ARPGStatTypes.h"
#include "ARPGStatsPanelWidget.generated.h"

class AARPGCharacter;
class UARPGStatsUIComponent;
class UButton;
class UProgressBar;
class UTextBlock;

/** Complete local presentation snapshot for the player-facing JRPG Stats panel. */
USTRUCT(BlueprintType)
struct AKUMASRPGFRAMEWORK_API FARPGStatsUISnapshot
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="ARPG|Stats UI") TObjectPtr<AARPGCharacter> Character = nullptr;
    UPROPERTY(BlueprintReadOnly, Category="ARPG|Stats UI") FString CharacterName;
    UPROPERTY(BlueprintReadOnly, Category="ARPG|Stats UI") int32 Level = 1;
    UPROPERTY(BlueprintReadOnly, Category="ARPG|Stats UI") int64 CurrentXP = 0;
    UPROPERTY(BlueprintReadOnly, Category="ARPG|Stats UI") int64 XPRequiredForNextLevel = 0;
    UPROPERTY(BlueprintReadOnly, Category="ARPG|Stats UI") float XPPercent = 0.f;
    UPROPERTY(BlueprintReadOnly, Category="ARPG|Stats UI") bool bAtMaxLevel = false;
    UPROPERTY(BlueprintReadOnly, Category="ARPG|Stats UI") bool bJRPGStatSystemEnabled = false;

    UPROPERTY(BlueprintReadOnly, Category="ARPG|Stats UI|Vitals") float Health = 0.f;
    UPROPERTY(BlueprintReadOnly, Category="ARPG|Stats UI|Vitals") float MaxHealth = 0.f;
    UPROPERTY(BlueprintReadOnly, Category="ARPG|Stats UI|Vitals") float HealthPercent = 0.f;
    UPROPERTY(BlueprintReadOnly, Category="ARPG|Stats UI|Vitals") float Mana = 0.f;
    UPROPERTY(BlueprintReadOnly, Category="ARPG|Stats UI|Vitals") float MaxMana = 0.f;
    UPROPERTY(BlueprintReadOnly, Category="ARPG|Stats UI|Vitals") float ManaPercent = 0.f;
    UPROPERTY(BlueprintReadOnly, Category="ARPG|Stats UI|Vitals") float Stamina = 0.f;
    UPROPERTY(BlueprintReadOnly, Category="ARPG|Stats UI|Vitals") float MaxStamina = 0.f;
    UPROPERTY(BlueprintReadOnly, Category="ARPG|Stats UI|Vitals") float StaminaPercent = 0.f;

    UPROPERTY(BlueprintReadOnly, Category="ARPG|Stats UI|JRPG") FARPGPrimaryStatBlock PrimaryStats;
    UPROPERTY(BlueprintReadOnly, Category="ARPG|Stats UI|JRPG") FARPGPrimaryStatPointAllocation AllocatedPoints;
    UPROPERTY(BlueprintReadOnly, Category="ARPG|Stats UI|JRPG") FARPGDerivedStatBlock DerivedStats;
    UPROPERTY(BlueprintReadOnly, Category="ARPG|Stats UI|JRPG") int32 UnspentAttributePoints = 0;
    UPROPERTY(BlueprintReadOnly, Category="ARPG|Stats UI|JRPG") int32 TotalAttributePointsEarned = 0;

    // Useful when the JRPG layer is deliberately disabled on an older character.
    UPROPERTY(BlueprintReadOnly, Category="ARPG|Stats UI|Legacy") float LegacyAttackPower = 0.f;
    UPROPERTY(BlueprintReadOnly, Category="ARPG|Stats UI|Legacy") float LegacySpellPower = 0.f;
    UPROPERTY(BlueprintReadOnly, Category="ARPG|Stats UI|Legacy") float LegacyArmor = 0.f;
};

/**
 * Ready-to-use complete JRPG stats panel.
 *
 * The native class builds a complete usable panel when used directly, including a Close button and
 * six server-authoritative Attribute Point + buttons. A project can subclass it as a Widget Blueprint;
 * matching standard child names are auto-bound, while BP_OnStatsUIUpdated provides a fully custom path.
 */
UCLASS(BlueprintType, Blueprintable)
class AKUMASRPGFRAMEWORK_API UARPGStatsPanelWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category="ARPG|Stats UI")
    void InitializeStatsUI(AARPGCharacter* InCharacter, UARPGStatsUIComponent* InStatsUIComponent);

    UFUNCTION(BlueprintCallable, Category="ARPG|Stats UI")
    void SetStatsUISnapshot(const FARPGStatsUISnapshot& InSnapshot);

    UFUNCTION(BlueprintCallable, Category="ARPG|Stats UI")
    void RefreshStatsUI();

    UFUNCTION(BlueprintCallable, Category="ARPG|Stats UI")
    void RequestCloseStatsUI();

    UFUNCTION(BlueprintPure, Category="ARPG|Stats UI")
    FARPGStatsUISnapshot GetStatsUISnapshot() const { return StatsUISnapshot; }

    UFUNCTION(BlueprintPure, Category="ARPG|Stats UI")
    AARPGCharacter* GetStatsCharacter() const { return ObservedCharacter.Get(); }

    UFUNCTION(BlueprintImplementableEvent, Category="ARPG|Stats UI", meta=(DisplayName="On ARPG Stats UI Updated"))
    void BP_OnStatsUIUpdated(FARPGStatsUISnapshot Snapshot);

    /** Standard field references. Custom Widget Blueprints using these names get zero-graph auto binding. */
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Stats UI|Bindings") TObjectPtr<UTextBlock> CharacterNameText;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Stats UI|Bindings") TObjectPtr<UTextBlock> LevelText;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Stats UI|Bindings") TObjectPtr<UTextBlock> SystemStateText;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Stats UI|Bindings") TObjectPtr<UTextBlock> XPText;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Stats UI|Bindings") TObjectPtr<UProgressBar> XPBar;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Stats UI|Bindings") TObjectPtr<UTextBlock> HealthText;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Stats UI|Bindings") TObjectPtr<UProgressBar> HealthBar;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Stats UI|Bindings") TObjectPtr<UTextBlock> ManaText;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Stats UI|Bindings") TObjectPtr<UProgressBar> ManaBar;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Stats UI|Bindings") TObjectPtr<UTextBlock> StaminaText;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Stats UI|Bindings") TObjectPtr<UProgressBar> StaminaBar;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Stats UI|Bindings") TObjectPtr<UTextBlock> AttributePointsText;

    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Stats UI|Bindings") TObjectPtr<UTextBlock> StrengthText;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Stats UI|Bindings") TObjectPtr<UTextBlock> VitalityText;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Stats UI|Bindings") TObjectPtr<UTextBlock> MagicText;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Stats UI|Bindings") TObjectPtr<UTextBlock> SpiritText;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Stats UI|Bindings") TObjectPtr<UTextBlock> DexterityText;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Stats UI|Bindings") TObjectPtr<UTextBlock> LuckText;

    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Stats UI|Bindings") TObjectPtr<UTextBlock> StrengthAllocatedText;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Stats UI|Bindings") TObjectPtr<UTextBlock> VitalityAllocatedText;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Stats UI|Bindings") TObjectPtr<UTextBlock> MagicAllocatedText;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Stats UI|Bindings") TObjectPtr<UTextBlock> SpiritAllocatedText;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Stats UI|Bindings") TObjectPtr<UTextBlock> DexterityAllocatedText;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Stats UI|Bindings") TObjectPtr<UTextBlock> LuckAllocatedText;

    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Stats UI|Bindings") TObjectPtr<UTextBlock> MeleeAttackPowerText;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Stats UI|Bindings") TObjectPtr<UTextBlock> RangedAttackPowerText;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Stats UI|Bindings") TObjectPtr<UTextBlock> MagicAttackPowerText;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Stats UI|Bindings") TObjectPtr<UTextBlock> PhysicalDefenseText;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Stats UI|Bindings") TObjectPtr<UTextBlock> MagicDefenseText;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Stats UI|Bindings") TObjectPtr<UTextBlock> AccuracyText;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Stats UI|Bindings") TObjectPtr<UTextBlock> EvasionText;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Stats UI|Bindings") TObjectPtr<UTextBlock> MagicEvasionText;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Stats UI|Bindings") TObjectPtr<UTextBlock> SpeedText;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Stats UI|Bindings") TObjectPtr<UTextBlock> CriticalChanceText;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Stats UI|Bindings") TObjectPtr<UTextBlock> CriticalDamageText;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Stats UI|Bindings") TObjectPtr<UTextBlock> AttackSpeedText;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Stats UI|Bindings") TObjectPtr<UTextBlock> MovementSpeedText;

    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Stats UI|Bindings") TObjectPtr<UButton> CloseButton;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Stats UI|Bindings") TObjectPtr<UButton> StrengthPlusButton;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Stats UI|Bindings") TObjectPtr<UButton> VitalityPlusButton;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Stats UI|Bindings") TObjectPtr<UButton> MagicPlusButton;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Stats UI|Bindings") TObjectPtr<UButton> SpiritPlusButton;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Stats UI|Bindings") TObjectPtr<UButton> DexterityPlusButton;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Stats UI|Bindings") TObjectPtr<UButton> LuckPlusButton;

protected:
    virtual void NativeOnInitialized() override;

private:
    UPROPERTY(Transient) FARPGStatsUISnapshot StatsUISnapshot;
    UPROPERTY(Transient) TObjectPtr<AARPGCharacter> ObservedCharacter;
    UPROPERTY(Transient) TObjectPtr<UARPGStatsUIComponent> OwningStatsUIComponent;

    void EnsureNativeLayoutOrBindings();
    void BindStandardButtons();
    void ApplySnapshotToStandardFields();
    void SpendPoint(EARPGPrimaryStat Stat);

    UFUNCTION() void HandleCloseClicked();
    UFUNCTION() void HandleStrengthPlusClicked();
    UFUNCTION() void HandleVitalityPlusClicked();
    UFUNCTION() void HandleMagicPlusClicked();
    UFUNCTION() void HandleSpiritPlusClicked();
    UFUNCTION() void HandleDexterityPlusClicked();
    UFUNCTION() void HandleLuckPlusClicked();
};
