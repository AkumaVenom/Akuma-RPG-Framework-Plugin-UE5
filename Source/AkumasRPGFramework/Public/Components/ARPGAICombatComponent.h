#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ARPGTypes.h"
#include "Combat/ARPGCombatTypes.h"
#include "ARPGAICombatComponent.generated.h"

class AAIController;
class UARPGCombatComponent;
class UARPGFactionComponent;

UENUM(BlueprintType)
enum class EARPGGroupCombatRole : uint8
{
    Solo UMETA(DisplayName="Solo / Uncoordinated"),
    ActiveAttacker UMETA(DisplayName="Active Attacker"),
    WaitingOrbit UMETA(DisplayName="Waiting / Orbiting")
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FARPGAICombatTargetChanged, AActor*, NewTarget);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FARPGAIAggroTriggered, AActor*, AggroTarget, bool, bWasDirectAttack);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FARPGAIAllyAssistTriggered, AActor*, Ally, AActor*, SharedTarget);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FARPGAIGroupCombatRoleChanged, EARPGGroupCombatRole, NewRole, int32, GroupSize, int32, SlotIndex);

UCLASS(ClassGroup=(ARPG), meta=(BlueprintSpawnableComponent))
class AKUMASRPGFRAMEWORK_API UARPGAICombatComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UARPGAICombatComponent();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI Combat") bool bEnabled = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI Combat") bool bAutoAcquireHostileTargets = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI Combat") bool bUseThreatFirst = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI Combat", meta=(ClampMin="0.05")) float ThinkInterval = 0.20f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI Combat", meta=(ClampMin="100.0")) float DetectionRadius = 1800.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI Combat", meta=(ClampMin="100.0")) float LoseTargetRadius = 3200.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI Combat", meta=(ClampMin="0.0")) float DesiredRangeOverride = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI Combat", meta=(ClampMin="5.0")) float MoveAcceptanceRadius = 75.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI Combat") bool bRequireLineOfSightToAttack = true;

    /** Automatic safety net: being attacked can create a temporary hostile target even when factions are neutral or not configured. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI Combat|Aggro & Retaliation") bool bRetaliateWhenAttacked = true;
    /** Neutral faction relationships normally do not aggro. Enable this so a neutral NPC still fights back against the actor that attacked it. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI Combat|Aggro & Retaliation") bool bRetaliationOverridesNeutralFaction = true;
    /** If either side has no usable faction id/data, still retaliate against the actor that attacked this NPC. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI Combat|Aggro & Retaliation") bool bRetaliateWhenFactionUnknown = true;
    /** Off by default so an accidental friendly hit cannot turn same-faction NPCs against each other. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI Combat|Aggro & Retaliation") bool bRetaliateAgainstFriendlyAttackers = false;
    /** How long a neutral/unknown attacker remains a valid combat target after aggression. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI Combat|Aggro & Retaliation", meta=(ClampMin="0.25")) float RetaliationMemorySeconds = 20.f;
    /** Extra threat added immediately when a hit is received so retaliation wins over passive threat noise. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI Combat|Aggro & Retaliation", meta=(ClampMin="0.0")) float RetaliationThreatBonus = 50.f;
    /** When true, a fresh direct hit can immediately retarget this AI to the new aggressor. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI Combat|Aggro & Retaliation") bool bDirectRetaliationCanOverrideExistingTarget = true;
    /** Optional faction-free proactive override. Leave off for passive creatures such as chickens that should only retaliate. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI Combat|Aggro & Retaliation", meta=(DisplayName="Fallback: Attack Players On Sight")) bool bAttackPlayersOnSightFallback = false;
    /** Optional broad fallback for actors with no faction identity. Intended for monster-style AI, not neutral wildlife. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI Combat|Aggro & Retaliation", meta=(DisplayName="Fallback: Attack Unfactioned Pawns On Sight")) bool bAttackUnfactionedPawnsOnSightFallback = false;
    /** Recommended default: once a temporary retaliation target dies, forget the temporary hostility so a respawned neutral player is neutral again. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI Combat|Aggro & Retaliation") bool bRestoreOriginalDispositionAfterTargetDeath = true;
    /** Removes stale threat against dead temporary targets together with retaliation memory. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI Combat|Aggro & Retaliation") bool bClearThreatAgainstDeadTargets = true;

    /** Nearby allied AI can join combat automatically when this NPC is attacked. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI Combat|Ally Assist") bool bCallForHelpWhenAttacked = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI Combat|Ally Assist", meta=(ClampMin="100.0")) float AllyAssistRadius = 1400.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI Combat|Ally Assist") bool bAssistSameFaction = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI Combat|Ally Assist") bool bAssistAlliedFactions = true;
    /** Spawner-created members from the same ARPG AI Spawner can assist even if faction data is missing. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI Combat|Ally Assist") bool bAssistSameSpawnGroup = true;
    /** Useful fallback for placed wildlife: same Blueprint/class actors can help each other only when faction identity is missing. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI Combat|Ally Assist") bool bAssistSameClassWhenFactionUnknown = true;
    /** Optional designer-defined assist family for placed AI that are not spawned by the same spawner. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI Combat|Ally Assist") FName AssistGroupId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI Combat|Ally Assist", meta=(ClampMin="0.0")) float AllyAssistThreatBonus = 25.f;
    /** Leave false so an ally already fighting a meaningful target is not constantly retargeted by help calls. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI Combat|Ally Assist") bool bAssistCanOverrideExistingTarget = false;


    /** Coordinates allied AI that are attacking the same target so large groups do not all occupy the same point. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI Combat|Group Combat") bool bEnableGroupCombatCoordination = true;
    /** When true, only actors considered allies by the framework share engagement slots. Disable for arena-style global crowd spacing. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI Combat|Group Combat") bool bCoordinateOnlyWithAllies = true;
    /** Maximum melee NPCs allowed to actively commit to the same target at once. Other melee NPCs wait/orbit for an opening. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI Combat|Group Combat", meta=(ClampMin="1", ClampMax="16")) int32 MaxSimultaneousMeleeAttackers = 3;
    /** Radius around the shared target used to find other coordinated attackers. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI Combat|Group Combat", meta=(ClampMin="300.0")) float GroupCombatCoordinationRadius = 2600.f;
    /** Active melee attackers approach a distributed ring instead of all pathing to the exact target origin. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI Combat|Group Combat", meta=(ClampMin="0.35", ClampMax="1.0")) float AttackApproachRadiusMultiplier = 0.72f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI Combat|Group Combat", meta=(ClampMin="50.0")) float MinimumAttackApproachRadius = 90.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI Combat|Group Combat", meta=(ClampMin="25.0")) float AttackPositionTolerance = 120.f;
    /** Prevents one NPC from reserving an attack opening forever if it cannot reach its slot. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI Combat|Group Combat", meta=(ClampMin="0.5")) float AttackSlotMaxHoldSeconds = 4.0f;
    /** Brief cooldown after committing an attack so another waiting NPC gets a turn. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI Combat|Group Combat", meta=(ClampMin="0.0")) float AttackSlotCooldownAfterAttack = 0.75f;
    /** Cooldown applied when an NPC yields an unreachable/unused attack slot. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI Combat|Group Combat", meta=(ClampMin="0.0")) float AttackSlotYieldSeconds = 1.25f;
    /** Waiting melee attackers move around the target instead of forming a stationary pile. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI Combat|Group Combat") bool bOrbitWhileWaiting = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI Combat|Group Combat", meta=(ClampMin="100.0")) float WaitingRingRadiusMin = 375.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI Combat|Group Combat", meta=(ClampMin="100.0")) float WaitingRingRadiusMax = 650.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI Combat|Group Combat", meta=(ClampMin="0.0")) float OrbitDegreesPerSecond = 18.f;
    /** Uses AI focus while in combat so waiting/repositioning NPCs keep watching the target. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI Combat|Group Combat") bool bFaceTargetWhileWaiting = true;
    /** Waiting melee NPCs do not normally fire arbitrary abilities through the attack-slot limit. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI Combat|Group Combat") bool bAllowAbilitiesWhileWaitingForMeleeSlot = false;
    /** Limits how often moving combat ring positions restart path requests as the target moves. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI Combat|Group Combat", meta=(ClampMin="0.1")) float CombatMoveGoalRefreshSeconds = 0.45f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI Combat|Group Combat", meta=(ClampMin="10.0")) float CombatMoveGoalRefreshDistance = 100.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI Combat|Group Combat") bool bProjectCombatPositionsToNavMesh = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI Combat|Group Combat") FVector CombatNavProjectionExtent = FVector(250.f, 250.f, 400.f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI Combat|Defence") bool bUseAutomaticDefence = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI Combat|Defence", meta=(ClampMin="0.0", ClampMax="1.0")) float DodgeChance = 0.35f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI Combat|Defence", meta=(ClampMin="0.0", ClampMax="1.0")) float BlockChance = 0.50f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI Combat|Defence", meta=(ClampMin="0.0")) float ReactionTimeMin = 0.15f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI Combat|Defence", meta=(ClampMin="0.0")) float ReactionTimeMax = 0.45f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI Combat|Defence", meta=(ClampMin="0.05")) float BlockHoldSeconds = 0.65f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI Combat|Abilities", meta=(ClampMin="0.1")) float AbilityTryInterval = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI Combat|Leash") bool bUseHomeLeash = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI Combat|Leash", meta=(ClampMin="100.0")) float MaxChaseDistanceFromHome = 3000.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI Combat|Leash") bool bRestoreVitalsOnLeashReset = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI Combat|Leash") bool bReturnHomeAfterCombat = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="AI Combat|Runtime") TObjectPtr<AActor> CurrentTarget;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="AI Combat|Runtime") FVector HomeLocation = FVector::ZeroVector;
    /** Server-side source spawner used to identify true spawn-group allies without forcing a faction setup. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="AI Combat|Runtime") TObjectPtr<AActor> SpawnGroupOwner;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="AI Combat|Runtime") EARPGGroupCombatRole CurrentGroupCombatRole = EARPGGroupCombatRole::Solo;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="AI Combat|Runtime") int32 EngagementGroupSize = 1;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="AI Combat|Runtime") int32 EngagementSlotIndex = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="AI Combat|Runtime") bool bHasMeleeAttackSlot = true;
    UPROPERTY(BlueprintAssignable, Category="AI Combat|Events") FARPGAICombatTargetChanged OnTargetChanged;
    UPROPERTY(BlueprintAssignable, Category="AI Combat|Events") FARPGAIAggroTriggered OnAggroTriggered;
    UPROPERTY(BlueprintAssignable, Category="AI Combat|Events") FARPGAIAllyAssistTriggered OnAllyAssistTriggered;
    UPROPERTY(BlueprintAssignable, Category="AI Combat|Events") FARPGAIGroupCombatRoleChanged OnGroupCombatRoleChanged;

    UFUNCTION(BlueprintCallable, Category="ARPG|AI Combat") void SetAICombatEnabled(bool bNewEnabled);
    UFUNCTION(BlueprintCallable, Category="ARPG|AI Combat", meta=(BlueprintAuthorityOnly)) void ForceCombatTarget(AActor* NewTarget);
    UFUNCTION(BlueprintCallable, Category="ARPG|AI Combat", meta=(BlueprintAuthorityOnly)) void ClearCombatTarget(bool bReturnHome = true);
    UFUNCTION(BlueprintCallable, Category="ARPG|AI Combat", meta=(BlueprintAuthorityOnly)) AActor* FindBestHostileTarget() const;
    UFUNCTION(BlueprintPure, Category="ARPG|AI Combat") bool IsTargetHostile(AActor* Candidate) const;
    UFUNCTION(BlueprintPure, Category="ARPG|AI Combat") bool IsTargetConsideredHostile(AActor* Candidate) const;
    UFUNCTION(BlueprintPure, Category="ARPG|AI Combat") bool CanRetaliateAgainst(AActor* Candidate) const;
    UFUNCTION(BlueprintPure, Category="ARPG|AI Combat") bool IsPotentialAlly(AActor* Candidate) const;
    UFUNCTION(BlueprintCallable, Category="ARPG|AI Combat", meta=(BlueprintAuthorityOnly)) void ReceiveAggroCall(AActor* Attacker, AActor* Caller);
    UFUNCTION(BlueprintCallable, Category="ARPG|AI Combat", meta=(BlueprintAuthorityOnly)) void SetSpawnGroupOwner(AActor* NewSpawnGroupOwner);
    UFUNCTION(BlueprintCallable, Category="ARPG|AI Combat", meta=(BlueprintAuthorityOnly)) void ForgetTemporaryAggressionAgainst(AActor* Actor, bool bClearThreat = true);
    UFUNCTION(BlueprintCallable, Category="ARPG|AI Combat", meta=(BlueprintAuthorityOnly)) void ForgetAllTemporaryAggression(bool bClearThreat = true);

    virtual void BeginPlay() override;

protected:
    FTimerHandle ThinkTimer;
    int32 ObservedEnemyAttackSerial = INDEX_NONE;
    int32 ReactedEnemyAttackSerial = INDEX_NONE;
    float ReactionDueAt = -1.f;
    float DefenceReleaseAt = -1.f;
    float LastAbilityTryAt = -1000.f;
    TWeakObjectPtr<AActor> ActiveMoveTarget;
    TWeakObjectPtr<UARPGCombatComponent> BoundTargetCombat;
    FVector ActiveMoveLocation = FVector::ZeroVector;
    bool bHasActiveMoveLocation = false;
    float LastMoveIssuedAt = -1000.f;
    float LastAttackCommitAt = -1000.f;
    float AttackSlotGrantedAt = -1.f;
    float NextAttackSlotEligibleAt = -1.f;
    bool bHadAttackSlotLastThink = false;
    bool bMovingHome = false;
    TMap<TWeakObjectPtr<AActor>, float> RetaliationMemory;

    UFUNCTION() void HandleCombatHitReceived(FARPGCombatHitInfo HitInfo);
    UFUNCTION() void HandleTargetLifeStateChanged(EARPGLifeState NewState);

    void Think();
    bool HasAIController() const;
    bool HasLineOfSightTo(AActor* Target) const;
    void SetTargetAuthority(AActor* NewTarget);
    void UpdateDefenceAgainstTarget(UARPGCombatComponent* MyCombat, UARPGCombatComponent* EnemyCombat);
    bool TryUseAutomaticAbility();
    void FaceTarget(AActor* Target) const;
    void SetWandererSuppressed(bool bSuppressed) const;
    bool IsProactiveHostileTarget(AActor* Candidate) const;
    bool HasActiveRetaliationAgainst(AActor* Candidate) const;
    void RememberAggression(AActor* Attacker, float ThreatBonus, bool bWasDirectAttack);
    void CallForHelp(AActor* Attacker);
    void PruneRetaliationMemory();
    void BindTargetLifeState(AActor* Target);
    void UnbindTargetLifeState();
    void ResetGroupCombatRuntime();
    void SetGroupCombatRole(EARPGGroupCombatRole NewRole, int32 GroupSize, int32 SlotIndex, bool bHasAttackSlot);
    void GatherCoordinatedMeleeAttackers(AActor* Target, TArray<UARPGAICombatComponent*>& OutMembers) const;
    bool EvaluateMeleeAttackSlot(AActor* Target, UARPGCombatComponent* MyCombat, int32& OutSlotIndex, int32& OutGroupSize, int32& OutOrbitDirectionSign);
    FVector ComputeCombatRingPosition(AActor* Target, int32 SlotIndex, int32 GroupSize, int32 OrbitDirectionSign, float Radius, bool bOrbit) const;
    bool ProjectCombatPositionToNavigation(const FVector& Desired, FVector& OutProjected) const;
    void MoveToCombatPosition(AAIController* AI, const FVector& DesiredGoal, float AcceptanceRadius);
    void ResetActiveMoveState();
};
