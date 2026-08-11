#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "ARPGCombatTypes.generated.h"

class UAnimMontage;
class AARPGCombatProjectile;
class UNiagaraSystem;
class UParticleSystem;
class USoundBase;

UENUM(BlueprintType)
enum class EARPGBasicAttackType : uint8
{
    Melee,
    Ranged,
    Magic
};

UENUM(BlueprintType)
enum class EARPGDodgeDirection : uint8
{
    Auto,
    Forward,
    Backward,
    Left,
    Right
};

UENUM(BlueprintType)
enum class EARPGCombatHitResult : uint8
{
    Hit,
    Critical,
    Blocked,
    Parried,
    Dodged,
    Immune,
    Friendly,
    Miss
};

UENUM(BlueprintType)
enum class EARPGCombatFeedbackCue : uint8
{
    MeleeSwing,
    RangedAttack,
    MagicCast,
    Hit,
    CriticalHit,
    BlockHit,
    Parry,
    Dodge,
    BlockStart,
    BlockEnd,
    GuardBreak,
    Stagger,
    Death,
    Revive
};

USTRUCT(BlueprintType)
struct AKUMASRPGFRAMEWORK_API FARPGCombatFXSettings
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FX") bool bEnabled = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FX") bool bPreferNiagara = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FX|Hit") TSoftObjectPtr<UNiagaraSystem> HitNiagara;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FX|Hit") TSoftObjectPtr<UParticleSystem> HitCascadeFallback;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FX|Critical") TSoftObjectPtr<UNiagaraSystem> CriticalHitNiagara;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FX|Critical") TSoftObjectPtr<UParticleSystem> CriticalHitCascadeFallback;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FX|Block") TSoftObjectPtr<UNiagaraSystem> BlockNiagara;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FX|Block") TSoftObjectPtr<UParticleSystem> BlockCascadeFallback;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FX|Parry") TSoftObjectPtr<UNiagaraSystem> ParryNiagara;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FX|Parry") TSoftObjectPtr<UParticleSystem> ParryCascadeFallback;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FX|Stagger") TSoftObjectPtr<UNiagaraSystem> StaggerNiagara;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FX|Stagger") TSoftObjectPtr<UParticleSystem> StaggerCascadeFallback;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FX", meta=(ClampMin="0.01")) float EffectScale = 1.f;
};

USTRUCT(BlueprintType)
struct AKUMASRPGFRAMEWORK_API FARPGCombatAudioSettings
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Audio") TSoftObjectPtr<USoundBase> MeleeSwing;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Audio") TSoftObjectPtr<USoundBase> RangedAttack;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Audio") TSoftObjectPtr<USoundBase> MagicCast;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Audio") TSoftObjectPtr<USoundBase> Hit;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Audio") TSoftObjectPtr<USoundBase> CriticalHit;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Audio") TSoftObjectPtr<USoundBase> BlockHit;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Audio") TSoftObjectPtr<USoundBase> Parry;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Audio") TSoftObjectPtr<USoundBase> Dodge;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Audio") TSoftObjectPtr<USoundBase> BlockStart;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Audio") TSoftObjectPtr<USoundBase> BlockEnd;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Audio") TSoftObjectPtr<USoundBase> GuardBreak;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Audio") TSoftObjectPtr<USoundBase> Stagger;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Audio") TSoftObjectPtr<USoundBase> Death;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Audio") TSoftObjectPtr<USoundBase> Revive;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Audio", meta=(ClampMin="0.0")) float VolumeMultiplier = 1.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Audio", meta=(ClampMin="0.01")) float PitchMin = 0.96f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Audio", meta=(ClampMin="0.01")) float PitchMax = 1.04f;
};

USTRUCT(BlueprintType)
struct AKUMASRPGFRAMEWORK_API FARPGStaggerSettings
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stagger") bool bEnabled = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stagger") bool bCriticalHitsCanStagger = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stagger", meta=(ClampMin="0.0", ClampMax="1.0")) float CriticalStaggerChance = 0.35f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stagger", meta=(ClampMin="0.05")) float Duration = 0.55f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stagger", meta=(ClampMin="0.0")) float ImmunitySeconds = 0.35f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stagger") bool bInterruptAttacks = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stagger") bool bApplyKnockback = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stagger", meta=(ClampMin="0.0")) float KnockbackVelocity = 425.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stagger") float UpwardVelocity = 80.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stagger") TSoftObjectPtr<UAnimMontage> StaggerMontage;
};

USTRUCT(BlueprintType)
struct AKUMASRPGFRAMEWORK_API FARPGAttackStepDefinition
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attack") TSoftObjectPtr<UAnimMontage> Montage;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attack", meta=(ClampMin="0.0")) float DamageMultiplier = 1.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attack", meta=(ClampMin="0.0")) float ImpactDelay = 0.25f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attack", meta=(ClampMin="0.01")) float RecoveryTime = 0.65f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attack", meta=(ClampMin="0.0")) float ComboQueueOpenTime = 0.20f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attack", meta=(ClampMin="0.0")) float ComboQueueCloseTime = 0.60f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attack", meta=(ClampMin="0.0")) float RangeOverride = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attack", meta=(ClampMin="0.0")) float TraceRadiusOverride = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attack", meta=(ClampMin="0.0")) float StaminaCost = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attack", meta=(ClampMin="0.0")) float ManaCost = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attack") bool bCanBeBlocked = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attack") bool bCanBeParried = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attack") bool bUnblockable = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attack") FName TraceStartSocket = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Attack") FName TraceEndSocket = NAME_None;
};

USTRUCT(BlueprintType)
struct AKUMASRPGFRAMEWORK_API FARPGDodgeSettings
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dodge") bool bEnabled = true;
    /** Automatic AI defence chance for this combat profile. 0 = never dodge, 1 = always choose dodge when a valid reaction opportunity exists. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dodge", meta=(DisplayName="AI Dodge Chance", ClampMin="0.0", ClampMax="1.0", UIMin="0.0", UIMax="1.0")) float AIDodgeChance = 0.35f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dodge") TSoftObjectPtr<UAnimMontage> ForwardMontage;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dodge") TSoftObjectPtr<UAnimMontage> BackwardMontage;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dodge") TSoftObjectPtr<UAnimMontage> LeftMontage;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dodge") TSoftObjectPtr<UAnimMontage> RightMontage;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dodge", meta=(ClampMin="0.05")) float Duration = 0.55f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dodge", meta=(ClampMin="0.0")) float Distance = 450.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dodge", meta=(ClampMin="0.0")) float StaminaCost = 20.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dodge", meta=(ClampMin="0.0")) float Cooldown = 0.35f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dodge", meta=(ClampMin="0.0")) float InvulnerabilityStart = 0.05f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dodge", meta=(ClampMin="0.0")) float InvulnerabilityEnd = 0.40f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dodge") bool bUseRootMotionOnly = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dodge") bool bDodgeCancelsAttacks = true;
    /** When enabled, beginning a dodge immediately clears an active stagger state/timer and replaces the stagger reaction with the dodge. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dodge", meta=(DisplayName="Dodge Cancels Stagger")) bool bDodgeCancelsStagger = true;
};

USTRUCT(BlueprintType)
struct AKUMASRPGFRAMEWORK_API FARPGBlockSettings
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Block") bool bEnabled = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Block") TSoftObjectPtr<UAnimMontage> BlockStartMontage;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Block") TSoftObjectPtr<UAnimMontage> BlockLoopMontage;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Block") TSoftObjectPtr<UAnimMontage> BlockHitMontage;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Block") TSoftObjectPtr<UAnimMontage> ParryMontage;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Block") TSoftObjectPtr<UAnimMontage> GuardBreakMontage;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Block") TSoftObjectPtr<UAnimMontage> BlockEndMontage;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Block", meta=(ClampMin="0.0", ClampMax="180.0")) float BlockHalfAngleDegrees = 70.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Block", meta=(ClampMin="0.0", ClampMax="1.0")) float PhysicalDamageReduction = 0.80f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Block", meta=(ClampMin="0.0", ClampMax="1.0")) float RangedDamageReduction = 0.75f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Block", meta=(ClampMin="0.0", ClampMax="1.0")) float MagicDamageReduction = 0.25f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Block", meta=(ClampMin="0.0")) float StaminaCostPerHit = 12.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Block", meta=(ClampMin="0.0")) float StaminaCostPerDamage = 0.05f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Block", meta=(ClampMin="0.0")) float MinimumStaminaToBlock = 1.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Block", meta=(ClampMin="0.0")) float PerfectBlockWindow = 0.18f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Block", meta=(ClampMin="0.0")) float GuardBreakDuration = 1.25f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Block", meta=(ClampMin="0.0", ClampMax="1.0")) float BlockingMoveSpeedMultiplier = 0.45f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Block") bool bCanBlockMelee = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Block") bool bCanBlockRanged = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Block") bool bCanBlockMagic = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Block") bool bRequiresTaggedEquipment = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Block") FGameplayTag RequiredEquipmentTag;
};

USTRUCT(BlueprintType)
struct AKUMASRPGFRAMEWORK_API FARPGDeathPresentationSettings
{
    GENERATED_BODY()

    // Generic characters leave ragdoll off; ARPGAICharacter enables it by default.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Death") bool bUseRagdollOnDeath = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Death") bool bFallbackToDeathMontage = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Death") bool bDisableCapsuleCollision = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Death") bool bTransferCharacterVelocity = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Death") bool bApplyHitDirectionImpulse = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Death", meta=(ClampMin="0.0")) float HitDirectionImpulseVelocity = 180.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Death") FName RagdollCollisionProfile = TEXT("Ragdoll");
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Death") bool bSimulateOnDedicatedServer = false;
};

USTRUCT(BlueprintType)
struct AKUMASRPGFRAMEWORK_API FARPGCombatProfile
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Basic Attack") EARPGBasicAttackType BasicAttackType = EARPGBasicAttackType::Melee;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Basic Attack") TArray<FARPGAttackStepDefinition> DetailedComboSteps;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Basic Attack", meta=(ClampMin="0.0")) float BaseDamage = 10.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Basic Attack", meta=(ClampMin="0.0")) float AttackPowerScale = 1.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Basic Attack", meta=(ClampMin="0.0")) float SpellPowerScale = 1.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Basic Attack", meta=(ClampMin="0.0")) float DefaultRange = 220.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Basic Attack", meta=(ClampMin="0.0")) float DefaultTraceRadius = 45.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Basic Attack", meta=(ClampMin="0.0")) float RangedMaxRange = 2500.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Basic Attack", meta=(ClampMin="0.0")) float MagicMaxRange = 2200.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Basic Attack", meta=(ClampMin="0.0")) float ComboResetTime = 1.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Basic Attack") bool bAutomaticTimedImpact = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Basic Attack", meta=(ClampMin="1")) int32 MaxMeleeTargets = 4;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Basic Attack", meta=(ClampMin="0.0", ClampMax="1.0")) float CriticalChance = 0.05f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Basic Attack", meta=(ClampMin="1.0")) float CriticalMultiplier = 1.75f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Basic Attack", meta=(ClampMin="0.0", ClampMax="1.0")) float DamageVariance = 0.10f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Basic Attack") bool bAllowFriendlyFire = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Basic Attack") bool bAllowNeutralDamage = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Basic Attack") bool bAutoFaceCombatTarget = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Basic Attack") FName ProjectileSpawnSocket = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Basic Attack") TSubclassOf<AARPGCombatProjectile> ProjectileClass;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Basic Attack", meta=(ClampMin="1.0")) float ProjectileSpeed = 2200.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Basic Attack") bool bProjectileHoming = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Abilities") TArray<FGameplayTag> AIAutoAbilityTags;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Abilities", meta=(ClampMin="0.0", ClampMax="1.0")) float AIAbilityUseChance = 0.25f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Defence") FARPGDodgeSettings Dodge;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Defence") FARPGBlockSettings Block;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Defence") FARPGStaggerSettings Stagger;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Feedback") FARPGCombatFXSettings ImpactFX;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Feedback") FARPGCombatAudioSettings Audio;
};

USTRUCT(BlueprintType)
struct AKUMASRPGFRAMEWORK_API FARPGCombatHitInfo
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) TObjectPtr<AActor> Attacker = nullptr;
    UPROPERTY(BlueprintReadOnly) TObjectPtr<AActor> Target = nullptr;
    UPROPERTY(BlueprintReadOnly) EARPGBasicAttackType AttackType = EARPGBasicAttackType::Melee;
    UPROPERTY(BlueprintReadOnly) EARPGCombatHitResult Result = EARPGCombatHitResult::Miss;
    UPROPERTY(BlueprintReadOnly) float RawDamage = 0.f;
    UPROPERTY(BlueprintReadOnly) float AppliedDamage = 0.f;
    UPROPERTY(BlueprintReadOnly) bool bCritical = false;
    UPROPERTY(BlueprintReadOnly) bool bStaggered = false;
    UPROPERTY(BlueprintReadOnly) FVector HitLocation = FVector::ZeroVector;
    UPROPERTY(BlueprintReadOnly) FVector KnockbackVelocity = FVector::ZeroVector;
};
