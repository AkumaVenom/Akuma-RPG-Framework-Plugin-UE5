#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ARPGTypes.h"
#include "TimerManager.h"
#include "ARPGStatsComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FARPGOnHealthChanged, float, NewHealth, float, Delta);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FARPGOnDeath);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FARPGOnJRPGStatsChanged, FARPGStatSnapshot, Snapshot);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FARPGOnAttributePointsChanged, int32, UnspentPoints, int32, TotalEarnedPoints);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FARPGOnNPCLevelScalingChanged, FARPGNPCLevelScalingRuntimeState, ScalingState);

UCLASS(ClassGroup=(ARPG), BlueprintType, Blueprintable, meta=(BlueprintSpawnableComponent))
class AKUMASRPGFRAMEWORK_API UARPGStatsComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UARPGStatsComponent();

    // Existing vitals/combat properties remain intact for Blueprint compatibility. When the JRPG
    // stat system is enabled, the derived system owns MaxHealth/MaxMana/MaxStamina and keeps the
    // legacy Armor/AttackPower/SpellPower values synchronized automatically.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, ReplicatedUsing=OnRep_Health, Category="Vitals") float Health = 100.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, Category="Vitals") float MaxHealth = 100.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, Category="Vitals") float Mana = 100.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, Category="Vitals") float MaxMana = 100.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, Category="Vitals") float Stamina = 100.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, Category="Vitals") float MaxStamina = 100.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, Category="Combat") float Armor = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, Category="Combat") float AttackPower = 10.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, Category="Combat") float SpellPower = 10.f;

    // Backward-compatible opt-in. Existing characters keep their exact old stat behavior until this
    // is enabled on the Stats component. New player/NPC Blueprints can opt into the full JRPG model.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, Category="JRPG Stats|Setup", meta=(DisplayName="Enable JRPG Stat System")) bool bEnableJRPGStatSystem = false;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="JRPG Stats|Setup", meta=(EditCondition="bEnableJRPGStatSystem", ShowOnlyInnerProperties)) FARPGStatGrowthSettings GrowthSettings;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="JRPG Stats|Setup", meta=(EditCondition="bEnableJRPGStatSystem", ShowOnlyInnerProperties)) FARPGDerivedStatFormula DerivedFormula;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="JRPG Stats|Attribute Points", meta=(EditCondition="bEnableJRPGStatSystem", ShowOnlyInnerProperties)) FARPGAttributePointSettings AttributePointSettings;

    // Optional WoW-style level matching for NPCs. The NPC's authored base stats/growth remain the
    // source of truth; only the runtime level used to evaluate them changes toward the player level.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="JRPG Stats|NPC Player Scaling", meta=(ShowOnlyInnerProperties)) FARPGNPCLevelScalingSettings NPCLevelScalingSettings;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category="JRPG Stats|Runtime") FARPGPrimaryStatBlock PrimaryStats;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category="JRPG Stats|Runtime") FARPGDerivedStatBlock DerivedStats;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, SaveGame, Category="JRPG Stats|Runtime") FARPGStatProgressionSaveState StatProgression;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, ReplicatedUsing=OnRep_StatRevision, Category="JRPG Stats|Runtime") int32 StatRevision = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, ReplicatedUsing=OnRep_NPCLevelScalingRuntime, Category="JRPG Stats|NPC Player Scaling|Runtime") FARPGNPCLevelScalingRuntimeState NPCLevelScalingRuntime;

    UPROPERTY(BlueprintAssignable, Category="Events") FARPGOnHealthChanged OnHealthChanged;
    UPROPERTY(BlueprintAssignable, Category="Events") FARPGOnDeath OnDeath;
    UPROPERTY(BlueprintAssignable, Category="JRPG Stats|Events") FARPGOnJRPGStatsChanged OnJRPGStatsChanged;
    UPROPERTY(BlueprintAssignable, Category="JRPG Stats|Events") FARPGOnAttributePointsChanged OnAttributePointsChanged;
    UPROPERTY(BlueprintAssignable, Category="JRPG Stats|NPC Player Scaling|Events") FARPGOnNPCLevelScalingChanged OnNPCLevelScalingChanged;

    UFUNCTION(BlueprintCallable, Category="ARPG|Stats", meta=(BlueprintAuthorityOnly)) bool ApplyDamage(float Amount);
    UFUNCTION(BlueprintCallable, Category="ARPG|Stats", meta=(BlueprintAuthorityOnly)) bool Heal(float Amount);
    UFUNCTION(BlueprintCallable, Category="ARPG|Stats", meta=(BlueprintAuthorityOnly)) bool SpendMana(float Amount);
    UFUNCTION(BlueprintCallable, Category="ARPG|Stats", meta=(BlueprintAuthorityOnly)) bool SpendStamina(float Amount);
    UFUNCTION(BlueprintCallable, Category="ARPG|Stats", meta=(BlueprintAuthorityOnly)) void RestoreMana(float Amount);
    UFUNCTION(BlueprintCallable, Category="ARPG|Stats", meta=(BlueprintAuthorityOnly)) void RestoreStamina(float Amount);
    UFUNCTION(BlueprintCallable, Category="ARPG|Stats", meta=(BlueprintAuthorityOnly)) void RestoreAllVitals();
    UFUNCTION(BlueprintPure, Category="ARPG|Stats") float GetHealthPercent() const;

    // Player-facing allocation API. Clients request the spend; the authoritative character validates
    // available points, per-stat limits and the global primary-stat cap before applying anything.
    UFUNCTION(BlueprintCallable, Category="ARPG|Stats|Attribute Points") bool SpendAttributePoints(EARPGPrimaryStat Stat, int32 Points = 1);
    UFUNCTION(BlueprintCallable, Category="ARPG|Stats|Attribute Points", meta=(BlueprintAuthorityOnly)) bool AddAttributePoints(int32 Points);
    UFUNCTION(BlueprintCallable, Category="ARPG|Stats|Attribute Points", meta=(BlueprintAuthorityOnly)) bool RefundAllAttributePoints();
    UFUNCTION(BlueprintPure, Category="ARPG|Stats|Attribute Points") int32 GetUnspentAttributePoints() const { return StatProgression.UnspentAttributePoints; }
    UFUNCTION(BlueprintPure, Category="ARPG|Stats|Attribute Points") int32 GetAllocatedAttributePoints(EARPGPrimaryStat Stat) const;
    UFUNCTION(BlueprintPure, Category="ARPG|Stats|Attribute Points") bool CanSpendAttributePoints(EARPGPrimaryStat Stat, int32 Points = 1) const;
    UFUNCTION(BlueprintPure, Category="ARPG|Stats|Attribute Points") float PreviewPrimaryStatAfterSpend(EARPGPrimaryStat Stat, int32 Points = 1) const;

    UFUNCTION(BlueprintCallable, Category="ARPG|Stats|JRPG", meta=(BlueprintAuthorityOnly)) void RecalculateJRPGStats();
    UFUNCTION(BlueprintPure, Category="ARPG|Stats|JRPG") float GetPrimaryStatValue(EARPGPrimaryStat Stat) const;
    UFUNCTION(BlueprintPure, Category="ARPG|Stats|JRPG") float GetDerivedStatValue(EARPGDerivedStat Stat) const;
    UFUNCTION(BlueprintPure, Category="ARPG|Stats|JRPG") FARPGStatSnapshot GetStatSnapshot() const;

    // NPC scaling never mutates Progression.Level. Use Effective Level for combat/nameplate presentation
    // when Scale NPC To Player is enabled, and Base Level when authored progression is required.
    UFUNCTION(BlueprintCallable, Category="ARPG|Stats|NPC Player Scaling", meta=(BlueprintAuthorityOnly)) void RefreshNPCLevelScalingNow();
    /** Runtime combat/stat level. For a scaled NPC this is the player-relative Effective Level; otherwise it is Base Character Level. */
    UFUNCTION(BlueprintPure, Category="ARPG|Stats|Level") int32 GetEffectiveLevel() const;
    /** Authoritative Progression component level. Set Progression -> Base Character Level to author/test this value. */
    UFUNCTION(BlueprintPure, Category="ARPG|Stats|Level") int32 GetBaseProgressionLevel() const;
    UFUNCTION(BlueprintPure, Category="ARPG|Stats|NPC Player Scaling") bool IsNPCLevelScalingApplied() const { return NPCLevelScalingRuntime.bScalingApplied; }
    UFUNCTION(BlueprintPure, Category="ARPG|Stats|NPC Player Scaling") FARPGNPCLevelScalingRuntimeState GetNPCLevelScalingState() const { return NPCLevelScalingRuntime; }

    UFUNCTION(BlueprintPure, Category="ARPG|Stats|Combat") float GetMeleeAttackPower() const { return bEnableJRPGStatSystem ? DerivedStats.MeleeAttackPower : AttackPower; }
    UFUNCTION(BlueprintPure, Category="ARPG|Stats|Combat") float GetRangedAttackPower() const { return bEnableJRPGStatSystem ? DerivedStats.RangedAttackPower : AttackPower; }
    UFUNCTION(BlueprintPure, Category="ARPG|Stats|Combat") float GetMagicAttackPower() const { return bEnableJRPGStatSystem ? DerivedStats.MagicAttackPower : SpellPower; }
    UFUNCTION(BlueprintPure, Category="ARPG|Stats|Combat") float GetPhysicalDefense() const { return bEnableJRPGStatSystem ? DerivedStats.PhysicalDefense : Armor; }
    UFUNCTION(BlueprintPure, Category="ARPG|Stats|Combat") float GetMagicDefense() const { return bEnableJRPGStatSystem ? DerivedStats.MagicDefense : 0.f; }
    UFUNCTION(BlueprintPure, Category="ARPG|Stats|Combat") float GetCriticalChanceBonus() const { return bEnableJRPGStatSystem ? DerivedStats.CriticalChance : 0.f; }
    UFUNCTION(BlueprintPure, Category="ARPG|Stats|Combat") float GetCriticalDamageMultiplier() const { return bEnableJRPGStatSystem ? DerivedStats.CriticalDamageMultiplier : 1.f; }
    UFUNCTION(BlueprintPure, Category="ARPG|Stats|Combat") float GetAttackSpeedMultiplier() const { return bEnableJRPGStatSystem ? DerivedStats.AttackSpeedMultiplier : 1.f; }
    UFUNCTION(BlueprintPure, Category="ARPG|Stats|Combat") float GetAccuracy() const { return bEnableJRPGStatSystem ? DerivedStats.Accuracy : 100.f; }
    UFUNCTION(BlueprintPure, Category="ARPG|Stats|Combat") float GetEvasion() const { return bEnableJRPGStatSystem ? DerivedStats.Evasion : 0.f; }
    UFUNCTION(BlueprintPure, Category="ARPG|Stats|Combat") float GetMagicEvasion() const { return bEnableJRPGStatSystem ? DerivedStats.MagicEvasion : 0.f; }
    UFUNCTION(BlueprintPure, Category="ARPG|Stats|Combat") bool UsesAccuracyEvasionHitChecks() const { return bEnableJRPGStatSystem && DerivedFormula.bEnableAccuracyEvasionHitChecks; }
    UFUNCTION(BlueprintPure, Category="ARPG|Stats|Movement") float GetMovementSpeedMultiplier() const { return bEnableJRPGStatSystem ? DerivedStats.MovementSpeedMultiplier : 1.f; }

    // Internal-system hook used by combat after leaving a movement-constrained state (for example
    // blocking). It is deliberately not reflected to Blueprints; UI/gameplay code should change stats
    // and let the component recalculate normally.
    void RefreshMovementSpeedFromStats();

    // Save-system helpers intentionally restore allocation/earned points separately from current vitals.
    UFUNCTION(BlueprintPure, Category="ARPG|Stats|Persistence") FARPGStatProgressionSaveState MakeStatProgressionSaveState() const { return StatProgression; }
    UFUNCTION(BlueprintCallable, Category="ARPG|Stats|Persistence", meta=(BlueprintAuthorityOnly)) void RestoreStatProgressionState(const FARPGStatProgressionSaveState& State, int32 CharacterLevel, bool bLegacySave = false);

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
    UFUNCTION() void OnRep_Health(float OldHealth);
    UFUNCTION() void OnRep_StatRevision();
    UFUNCTION() void OnRep_NPCLevelScalingRuntime();
    UFUNCTION() void HandleNPCLevelScalingTimer();
    UFUNCTION() void HandleLevelChanged(int32 OldLevel, int32 NewLevel);
    UFUNCTION() void HandleInventoryChanged();
    UFUNCTION(Server, Reliable) void ServerSpendAttributePoints(EARPGPrimaryStat Stat, int32 Points);

    bool SpendAttributePointsAuthority(EARPGPrimaryStat Stat, int32 Points);
    void InitializeStatProgressionForLevel(int32 Level, bool bForceReset);
    void RecalculateJRPGStatsInternal(bool bFromLevelUp = false);
    FARPGStatModifier CollectEquippedStatModifier() const;
    void ApplyMovementSpeed();
    void BroadcastStatSnapshot();
    bool CanUseNPCLevelScaling() const;
    bool IsOwnerInCombat() const;
    bool IsEligibleScalingPlayer(AActor* Candidate) const;
    bool ResolveNPCScalingReference(int32& OutPlayerLevel) const;
    int32 ComputeNPCScaledLevel(int32 BaseLevel, int32 PlayerLevel) const;
    void RefreshNPCLevelScalingInternal(bool bForceRecalculate = false, bool bFromLevelUp = false);
    void EnsureNPCLevelScalingTimer();
    void ClearNPCLevelScalingTimer();
    float GetNaturalPrimaryStatForLevel(EARPGPrimaryStat Stat, int32 Level) const;
    int32* ResolveAllocationPtr(EARPGPrimaryStat Stat);
    const int32* ResolveAllocationPtr(EARPGPrimaryStat Stat) const;

    float CachedBaseWalkSpeed = 0.f;
    FTimerHandle NPCLevelScalingTimer;
    bool bNPCScalingEncounterLocked = false;
};
