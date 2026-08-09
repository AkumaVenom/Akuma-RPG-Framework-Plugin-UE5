#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ARPGTypes.h"
#include "Combat/ARPGCombatTypes.h"
#include "ARPGCombatComponent.generated.h"

class UAnimMontage;
class UARPGClassDefinition;
class USkeletalMeshComponent;
class USoundBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FARPGLifeStateChanged, EARPGLifeState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FARPGAttackStarted, int32, ComboIndex, UAnimMontage*, Montage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FARPGCombatHitEvent, FARPGCombatHitInfo, HitInfo);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FARPGCombatTargetChanged, AActor*, NewTarget);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FARPGBlockStateChanged, bool, bBlocking);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FARPGDodgeStarted, EARPGDodgeDirection, Direction);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FARPGRagdollStateChanged, bool, bRagdollActive);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FARPGStaggerStateChanged, bool, bStaggered);

UCLASS(ClassGroup=(ARPG), meta=(BlueprintSpawnableComponent))
class AKUMASRPGFRAMEWORK_API UARPGCombatComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UARPGCombatComponent();

    UPROPERTY(EditAnywhere, BlueprintReadOnly, ReplicatedUsing=OnRep_LifeState, Category="Combat") EARPGLifeState LifeState = EARPGLifeState::Alive;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Combat") float RespawnDelay = 5.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat|Respawn") bool bAutoRespawn = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Combat|Respawn") FTransform RespawnTransform;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat|Profile") bool bUseClassCombatProfile = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat|Profile", meta=(EditCondition="!bUseClassCombatProfile")) FARPGCombatProfile OverrideCombatProfile;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category="Combat|Death") FARPGDeathPresentationSettings DeathPresentation;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Animation") FARPGCombatMontageSet Montages;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat|Kill Credit") FName CreatureId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat|Kill Credit") FGameplayTag SlayerCategory;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat|Kill Credit", meta=(ClampMin="0")) int64 CharacterXPReward = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat|Kill Credit") bool bGrantLootToKiller = true;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, ReplicatedUsing=OnRep_CombatTarget, Category="Combat|Runtime") TObjectPtr<AActor> CombatTarget;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category="Combat|Runtime") bool bIsAttacking = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category="Combat|Runtime") bool bIsDodging = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category="Combat|Runtime") bool bIsBlocking = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category="Combat|Runtime") bool bGuardBroken = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, ReplicatedUsing=OnRep_Staggered, Category="Combat|Runtime") bool bIsStaggered = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category="Combat|Runtime") int32 CurrentComboIndex = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category="Combat|Runtime") int32 AttackSerial = 0;

    UPROPERTY(BlueprintAssignable, Category="Combat|Events") FARPGLifeStateChanged OnLifeStateChanged;
    UPROPERTY(BlueprintAssignable, Category="Combat|Events") FARPGAttackStarted OnAttackStarted;
    UPROPERTY(BlueprintAssignable, Category="Combat|Events") FARPGCombatHitEvent OnCombatHitDealt;
    UPROPERTY(BlueprintAssignable, Category="Combat|Events") FARPGCombatHitEvent OnCombatHitReceived;
    UPROPERTY(BlueprintAssignable, Category="Combat|Events") FARPGCombatTargetChanged OnCombatTargetChanged;
    UPROPERTY(BlueprintAssignable, Category="Combat|Events") FARPGBlockStateChanged OnBlockStateChanged;
    UPROPERTY(BlueprintAssignable, Category="Combat|Events") FARPGDodgeStarted OnDodgeStarted;
    UPROPERTY(BlueprintAssignable, Category="Combat|Events") FARPGRagdollStateChanged OnRagdollStateChanged;
    UPROPERTY(BlueprintAssignable, Category="Combat|Events") FARPGStaggerStateChanged OnStaggerStateChanged;

    UFUNCTION(BlueprintCallable, Category="ARPG|Combat") bool PerformBasicAttack(AActor* OptionalTarget = nullptr);
    UFUNCTION(BlueprintCallable, Category="ARPG|Combat") bool PerformDodge(EARPGDodgeDirection Direction = EARPGDodgeDirection::Auto);
    UFUNCTION(BlueprintCallable, Category="ARPG|Combat") bool StartBlocking();
    UFUNCTION(BlueprintCallable, Category="ARPG|Combat") void StopBlocking();
    UFUNCTION(BlueprintCallable, Category="ARPG|Combat") void SetCombatTarget(AActor* NewTarget);
    UFUNCTION(BlueprintCallable, Category="ARPG|Combat|Animation", meta=(DisplayName="Combat Impact / Trace Now")) void NotifyAttackImpact();

    UFUNCTION(BlueprintPure, Category="ARPG|Combat") bool IsAlive() const { return LifeState == EARPGLifeState::Alive; }
    UFUNCTION(BlueprintPure, Category="ARPG|Combat") bool CanPerformBasicAttack() const;
    UFUNCTION(BlueprintPure, Category="ARPG|Combat") bool CanDodge() const;
    UFUNCTION(BlueprintPure, Category="ARPG|Combat") bool CanBlock() const;
    UFUNCTION(BlueprintPure, Category="ARPG|Combat") bool IsInDefensiveState() const { return bIsDodging || bIsBlocking || bGuardBroken || bIsStaggered; }
    UFUNCTION(BlueprintPure, Category="ARPG|Combat") bool IsStaggered() const { return bIsStaggered; }
    UFUNCTION(BlueprintPure, Category="ARPG|Combat") bool IsDodgeInvulnerable() const;
    UFUNCTION(BlueprintPure, Category="ARPG|Combat") float GetAttackImpactSecondsRemaining() const;
    UFUNCTION(BlueprintPure, Category="ARPG|Combat") float GetPreferredCombatRange() const;
    UFUNCTION(BlueprintPure, Category="ARPG|Combat") FARPGCombatProfile GetCombatProfile() const;
    UFUNCTION(BlueprintPure, Category="ARPG|Combat") bool CanDamageActor(AActor* Target) const;
    UFUNCTION(BlueprintPure, Category="ARPG|Combat") int32 GetComboCount() const;
    UFUNCTION(BlueprintPure, Category="ARPG|Combat|Death") bool IsRagdollActive() const { return bRagdollActive; }

    UFUNCTION(BlueprintCallable, Category="ARPG|Combat", meta=(BlueprintAuthorityOnly))
    FARPGCombatHitInfo ReceiveCombatHit(AActor* Attacker, float RawDamage, EARPGBasicAttackType AttackType, bool bCanBeBlocked, bool bCanBeParried, FVector HitLocation, bool bCritical);

    UFUNCTION(BlueprintCallable, Category="ARPG|Combat", meta=(BlueprintAuthorityOnly)) void Kill();
    UFUNCTION(BlueprintCallable, Category="ARPG|Combat", meta=(BlueprintAuthorityOnly)) void RespawnAtTransform(const FTransform& Transform);
    UFUNCTION(BlueprintCallable, Category="ARPG|Combat", meta=(BlueprintAuthorityOnly)) void SetRespawnTransform(const FTransform& Transform);
    UFUNCTION(BlueprintCallable, Category="ARPG|Combat|Animation") UAnimMontage* PickRandomAttackMontage(bool bMagic, bool bRanged) const;

    virtual void BeginPlay() override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
    UFUNCTION() void OnRep_LifeState(EARPGLifeState OldState);
    UFUNCTION() void OnRep_CombatTarget();
    UFUNCTION() void OnRep_Staggered();
    UFUNCTION() void HandleStatsDeath();

    UFUNCTION(Server, Reliable) void ServerPerformBasicAttack(AActor* OptionalTarget);
    UFUNCTION(Server, Reliable) void ServerPerformDodge(EARPGDodgeDirection Direction);
    UFUNCTION(Server, Reliable) void ServerStartBlocking();
    UFUNCTION(Server, Reliable) void ServerStopBlocking();
    UFUNCTION(Server, Reliable) void ServerSetCombatTarget(AActor* NewTarget);
    UFUNCTION(Server, Reliable) void ServerNotifyAttackImpact();
    UFUNCTION(NetMulticast, Unreliable) void MulticastPlayMontage(UAnimMontage* Montage, float PlayRate = 1.f);
    UFUNCTION(NetMulticast, Reliable) void MulticastBeginDeathPresentation(FVector InheritedVelocity, FVector HitDirection, FVector HitLocation);
    UFUNCTION(NetMulticast, Reliable) void MulticastResetDeathPresentation();
    UFUNCTION(NetMulticast, Unreliable) void MulticastPlayCombatCue(EARPGCombatFeedbackCue Cue, FVector Location, FVector Direction);

    bool StartAttackAuthority(AActor* OptionalTarget);
    bool PerformDodgeAuthority(EARPGDodgeDirection Direction);
    bool StartBlockingAuthority();
    void StopBlockingAuthority(bool bPlayEndMontage = true);
    void ResolveAttackImpactAuthority();
    void FinishAttackAuthority();
    void FinishDodgeAuthority();
    void EndGuardBreakAuthority();
    void BeginGuardBreakAuthority(float Duration);
    bool TryApplyCriticalStaggerAuthority(AActor* Attacker, FARPGCombatHitInfo& InOutHitInfo);
    void EndStaggerAuthority();
    void PlayBlockLoopAuthority();
    void AwardKillCredit(AActor* Killer);
    void AutoRespawnAuthority();

    bool BuildAttackStep(int32 ComboIndex, FARPGAttackStepDefinition& OutStep) const;
    float CalculateAttackDamage(const FARPGAttackStepDefinition& Step, bool& bOutCritical) const;
    void ResolveMeleeAttack(const FARPGAttackStepDefinition& Step, float RawDamage, bool bCritical);
    void ResolveRangedOrMagicAttack(const FARPGAttackStepDefinition& Step, float RawDamage, bool bCritical);
    void ResolveHitOnActor(AActor* Target, const FARPGAttackStepDefinition& Step, float RawDamage, bool bCritical, const FVector& HitLocation);
    UAnimMontage* GetFallbackMontageForIndex(int32 ComboIndex) const;
    UAnimMontage* GetHitReactMontage() const;
    UAnimMontage* GetDeathMontage() const;
    UAnimMontage* GetReviveMontage() const;
    bool IsBlockFacingAttacker(AActor* Attacker, float HalfAngleDegrees) const;
    bool HasRequiredBlockEquipment(const FARPGBlockSettings& BlockSettings) const;
    FVector ResolveDodgeWorldDirection(EARPGDodgeDirection& InOutDirection) const;
    UAnimMontage* ResolveDodgeMontage(EARPGDodgeDirection Direction, const FARPGDodgeSettings& DodgeSettings) const;
    void RestoreBlockingMoveSpeed();
    void SetLooseCombatTag(FName TagName, bool bEnabled) const;
    bool ApplyDeathPresentationLocal(const FVector& InheritedVelocity, const FVector& HitDirection, const FVector& HitLocation);
    bool TryStartRagdollLocal(const FVector& InheritedVelocity, const FVector& HitDirection, const FVector& HitLocation);
    void ResetDeathPresentationLocal();
    void CacheRagdollRestoreState();
    void PlayCombatCueLocal(EARPGCombatFeedbackCue Cue, const FVector& Location, const FVector& Direction);
    void SpawnConfiguredImpactFX(EARPGCombatFeedbackCue Cue, const FVector& Location, const FVector& Direction, const FARPGCombatFXSettings& Settings);
    USoundBase* ResolveCombatSound(EARPGCombatFeedbackCue Cue, const FARPGCombatAudioSettings& Settings) const;

    FARPGAttackStepDefinition ActiveAttackStep;
    bool bComboQueued = false;
    bool bImpactResolved = false;
    float AttackStartedAt = -1000.f;
    float AttackImpactAt = -1000.f;
    float ComboQueueOpenAt = -1000.f;
    float ComboQueueCloseAt = -1000.f;
    float ComboExpiresAt = -1000.f;
    float DodgeStartedAt = -1000.f;
    float LastDodgeAt = -1000.f;
    float BlockStartedAt = -1000.f;
    float LastStaggerAt = -1000.f;
    float CachedPreBlockWalkSpeed = 0.f;
    TWeakObjectPtr<AActor> LastDamageInstigator;
    FVector LastReceivedHitLocation = FVector::ZeroVector;
    FVector LastReceivedHitDirection = FVector::ZeroVector;
    bool bKillCreditAwarded = false;
    bool bDeathPresentationActive = false;
    bool bRagdollActive = false;
    bool bRagdollRestoreStateCached = false;
    FTransform CachedMeshRelativeTransform = FTransform::Identity;
    FName CachedMeshCollisionProfile = NAME_None;
    ECollisionEnabled::Type CachedMeshCollisionEnabled = ECollisionEnabled::NoCollision;
    ECollisionEnabled::Type CachedCapsuleCollisionEnabled = ECollisionEnabled::QueryAndPhysics;
    TSet<TWeakObjectPtr<AActor>> HitActorsThisSwing;

    FTimerHandle AttackImpactTimer;
    FTimerHandle AttackFinishTimer;
    FTimerHandle DodgeFinishTimer;
    FTimerHandle GuardBreakTimer;
    FTimerHandle StaggerTimer;
    FTimerHandle BlockLoopTimer;
    FTimerHandle AutoRespawnTimer;
};
