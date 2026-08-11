#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "ARPGTypes.h"
#include "Combat/ARPGCombatTypes.h"
#include "ARPGAISocialComponent.generated.h"

class UAnimMontage;
class USoundBase;
class UARPGAICombatComponent;
class UARPGCombatComponent;
class UARPGFactionComponent;
class UARPGWandererComponent;
class UARPGAISplineComponent;

UENUM(BlueprintType)
enum class EARPGAISocialState : uint8
{
    Idle UMETA(DisplayName="Idle"),
    Approaching UMETA(DisplayName="Approaching Partner"),
    Interacting UMETA(DisplayName="Interacting")
};

/**
 * One locally-authored social interaction style. Two NPCs can use an interaction only when
 * both components contain the same InteractionId. Each participant plays content from its own
 * matching entry, allowing different skeletons/voices to share the same Conversation/Greeting id.
 */
USTRUCT(BlueprintType)
struct AKUMASRPGFRAMEWORK_API FARPGAISocialInteractionDefinition
{
    GENERATED_BODY()

    /** Shared id used to match compatible interaction styles between two NPCs. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Social") FName InteractionId = TEXT("Conversation");
    /** Relative chance when this entry is selected by an initiating NPC. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Social", meta=(ClampMin="0.0")) float Weight = 1.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Social") bool bCanInitiate = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Social") bool bCanRespond = true;
    /** Pair duration uses the overlap of both participants' authored ranges when possible. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Social|Timing", meta=(ClampMin="0.25")) float MinDuration = 2.5f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Social|Timing", meta=(ClampMin="0.25")) float MaxDuration = 5.5f;
    /** Optional local animation choices for this NPC/skeleton. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Social|Presentation") TArray<TObjectPtr<UAnimMontage>> Montages;
    /** Optional local voice/foley choices used on alternating conversation beats. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Social|Presentation") TArray<TObjectPtr<USoundBase>> Sounds;
    /** Optional subtitle/speech-bubble lines emitted on alternating conversation beats. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Social|Presentation", meta=(MultiLine="true")) TArray<FText> Lines;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FARPGAISocialInteractionStarted, AActor*, Partner, FName, InteractionId, bool, bInitiator);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FARPGAISocialInteractionEnded, AActor*, Partner, FName, InteractionId, bool, bInterrupted);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FARPGAISocialLineSpoken, AActor*, Partner, FName, InteractionId, FText, Line);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FARPGAISocialStateChanged, EARPGAISocialState, NewState, AActor*, Partner);

/**
 * Optional server-authoritative ambient NPC social behaviour.
 *
 * The component is intentionally timer driven (no permanent Tick). Eligible NPCs periodically
 * inspect a small Pawn overlap around themselves, use faction/tag compatibility, reserve a partner,
 * pause wandering/spline travel, approach, face one another, exchange randomized social beats, and
 * then resume their prior movement. Combat, damage, dodge/stagger/death always pre-empt social AI.
 */
UCLASS(ClassGroup=(ARPG), meta=(BlueprintSpawnableComponent))
class AKUMASRPGFRAMEWORK_API UARPGAISocialComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UARPGAISocialComponent();

    /** Master opt-in. Disabled by default so existing NPC behaviour is unchanged. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI Social|Core") bool bEnableSocialInteractions = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI Social|Core", meta=(EditCondition="bEnableSocialInteractions")) bool bCanInitiateInteractions = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI Social|Core", meta=(EditCondition="bEnableSocialInteractions")) bool bCanRespondToInteractions = true;
    /** Players are excluded by default; this system is intended for ambient NPC-to-NPC life. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI Social|Core", meta=(EditCondition="bEnableSocialInteractions")) bool bAllowPlayerControlledParticipants = false;

    /** Chance rolled once per eligible opportunity, not every frame. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI Social|Opportunity", meta=(EditCondition="bEnableSocialInteractions", ClampMin="0.0", ClampMax="1.0")) float InteractionChance = 0.35f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI Social|Opportunity", meta=(EditCondition="bEnableSocialInteractions", ClampMin="0.25")) float ScanInterval = 1.5f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI Social|Opportunity", meta=(EditCondition="bEnableSocialInteractions", ClampMin="100.0")) float DetectionRadius = 650.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI Social|Opportunity", meta=(EditCondition="bEnableSocialInteractions", ClampMin="1", ClampMax="64")) int32 MaxCandidatesPerScan = 10;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI Social|Opportunity", meta=(EditCondition="bEnableSocialInteractions")) bool bRequireLineOfSight = true;
    /** After an eligible encounter is evaluated, wait before rolling another opportunity. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI Social|Opportunity", meta=(EditCondition="bEnableSocialInteractions", ClampMin="0.1")) float OpportunityRetryMin = 3.5f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI Social|Opportunity", meta=(EditCondition="bEnableSocialInteractions", ClampMin="0.1")) float OpportunityRetryMax = 7.f;

    /** How close the initiator tries to get before the actual exchange begins. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI Social|Movement", meta=(EditCondition="bEnableSocialInteractions", ClampMin="60.0")) float InteractionDistance = 190.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI Social|Movement", meta=(EditCondition="bEnableSocialInteractions", ClampMin="0.25")) float ApproachTimeout = 5.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI Social|Movement", meta=(EditCondition="bEnableSocialInteractions", ClampMin="0.05")) float ActiveUpdateInterval = 0.10f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI Social|Movement", meta=(EditCondition="bEnableSocialInteractions")) bool bFacePartnerDuringInteraction = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI Social|Movement", meta=(EditCondition="bEnableSocialInteractions", ClampMin="1.0")) float FacingTurnRateDegreesPerSecond = 420.f;

    /** Hostile relationships are always rejected. Same/friendly/neutral categories can be authored here. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI Social|Faction", meta=(EditCondition="bEnableSocialInteractions")) bool bAllowSameFaction = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI Social|Faction", meta=(EditCondition="bEnableSocialInteractions")) bool bAllowFriendlyFactions = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI Social|Faction", meta=(EditCondition="bEnableSocialInteractions")) bool bAllowNeutralFactions = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI Social|Faction", meta=(EditCondition="bEnableSocialInteractions")) bool bAllowFactionlessNPCs = true;

    /** Optional archetype matching. Example identity tags: Social.Villager, Social.Guard, Social.Merchant. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI Social|Matching", meta=(EditCondition="bEnableSocialInteractions")) FGameplayTagContainer SocialIdentityTags;
    /** Empty = no required partner tag. When populated, the other NPC must contain all required tags. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI Social|Matching", meta=(EditCondition="bEnableSocialInteractions")) FGameplayTagContainer RequiredPartnerTags;
    /** Any matching blocked tag rejects the pair. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI Social|Matching", meta=(EditCondition="bEnableSocialInteractions")) FGameplayTagContainer BlockedPartnerTags;

    /** Both NPCs need a matching InteractionId; each actor uses its own animation/audio/line entry. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI Social|Interactions", meta=(EditCondition="bEnableSocialInteractions")) TArray<FARPGAISocialInteractionDefinition> InteractionPool;
    /** Alternating speaker interval while the pair is interacting. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI Social|Interactions", meta=(EditCondition="bEnableSocialInteractions", ClampMin="0.25")) float ExchangeIntervalMin = 1.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI Social|Interactions", meta=(EditCondition="bEnableSocialInteractions", ClampMin="0.25")) float ExchangeIntervalMax = 2.0f;

    /** General post-interaction cooldown prevents town crowds from constantly chaining conversations. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI Social|Cooldowns", meta=(EditCondition="bEnableSocialInteractions", ClampMin="0.0")) float InteractionCooldownMin = 10.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI Social|Cooldowns", meta=(EditCondition="bEnableSocialInteractions", ClampMin="0.0")) float InteractionCooldownMax = 20.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI Social|Cooldowns", meta=(EditCondition="bEnableSocialInteractions", ClampMin="0.0")) float SamePartnerCooldown = 35.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI Social|Cooldowns", meta=(EditCondition="bEnableSocialInteractions", ClampMin="0.0")) float InterruptedCooldown = 2.5f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category="AI Social|Runtime") EARPGAISocialState SocialState = EARPGAISocialState::Idle;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category="AI Social|Runtime") TObjectPtr<AActor> CurrentPartner;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category="AI Social|Runtime") FName CurrentInteractionId = NAME_None;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category="AI Social|Runtime") bool bCurrentRoleInitiator = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category="AI Social|Runtime") float InteractionEndServerTime = 0.f;

    UPROPERTY(BlueprintAssignable, Category="AI Social|Events") FARPGAISocialInteractionStarted OnSocialInteractionStarted;
    UPROPERTY(BlueprintAssignable, Category="AI Social|Events") FARPGAISocialInteractionEnded OnSocialInteractionEnded;
    UPROPERTY(BlueprintAssignable, Category="AI Social|Events") FARPGAISocialLineSpoken OnSocialLineSpoken;
    UPROPERTY(BlueprintAssignable, Category="AI Social|Events") FARPGAISocialStateChanged OnSocialStateChanged;

    UFUNCTION(BlueprintCallable, Category="ARPG|AI Social", meta=(BlueprintAuthorityOnly)) void SetSocialInteractionsEnabled(bool bNewEnabled);
    /** Attempts a normal compatible interaction immediately, bypassing only the random opportunity roll. */
    UFUNCTION(BlueprintCallable, Category="ARPG|AI Social", meta=(BlueprintAuthorityOnly)) bool TryStartSocialInteractionWith(AActor* Partner);
    /** Testing/cinematic helper: selects a specific shared InteractionId while still respecting safety/faction/tag compatibility. */
    UFUNCTION(BlueprintCallable, Category="ARPG|AI Social", meta=(BlueprintAuthorityOnly)) bool ForceSocialInteractionWith(AActor* Partner, FName InteractionId = TEXT("Conversation"));
    UFUNCTION(BlueprintCallable, Category="ARPG|AI Social", meta=(BlueprintAuthorityOnly)) void CancelSocialInteraction();
    UFUNCTION(BlueprintPure, Category="ARPG|AI Social") bool IsSociallyEngaged() const { return SocialState != EARPGAISocialState::Idle && IsValid(CurrentPartner); }
    UFUNCTION(BlueprintPure, Category="ARPG|AI Social") bool CanSociallyInteractWith(AActor* Candidate) const;

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
    FTimerHandle ScanTimer;
    FTimerHandle ActiveInteractionTimer;
    float NextOpportunityAt = 0.f;
    float ApproachDeadline = 0.f;
    float NextApproachMoveAt = 0.f;
    float NextExchangeAt = 0.f;
    bool bNextExchangeFromInitiator = true;
    bool bSocialWandererPauseApplied = false;
    bool bSplineWasActiveBeforeSocial = false;
    TMap<TWeakObjectPtr<AActor>, float> PartnerCooldownUntil;
    TWeakObjectPtr<UAnimMontage> LocalPresentationMontage;

    void EnsureScanTimer();
    void ScanForSocialOpportunity();
    bool IsOwnerAvailableForSocial(bool bIgnoreCurrentSocialState = false) const;
    bool PassesFactionRules(const UARPGAISocialComponent* Other) const;
    bool PassesTagRules(const UARPGAISocialComponent* Other) const;
    bool HasLineOfSightTo(const AActor* OtherActor) const;
    bool IsPartnerOnCooldown(AActor* Partner) const;
    const FARPGAISocialInteractionDefinition* FindInteractionDefinition(FName InteractionId, bool bForInitiation, bool bForResponse) const;
    bool SelectCompatibleInteraction(const UARPGAISocialComponent* Other, FName& OutInteractionId) const;
    float ResolvePairInteractionDuration(const UARPGAISocialComponent* Other, FName InteractionId) const;
    bool StartPairAuthority(UARPGAISocialComponent* Other, FName InteractionId);
    void ReserveParticipantAuthority(AActor* Partner, FName InteractionId, bool bInitiator);
    void BeginConversationAuthority();
    void BeginParticipantPresentationAuthority(float EndServerTime);
    void UpdateActiveInteraction();
    void EmitExchangeBeatAuthority();
    void EndPairAuthority(bool bInterrupted);
    void EndParticipantAuthority(bool bInterrupted, float CooldownUntil, float PartnerCooldownTime);
    void PauseAmbientMovementForSocial();
    void RestoreAmbientMovementAfterSocial();
    void FacePartner(float DeltaSeconds);
    void SetRuntimeState(EARPGAISocialState NewState);
    void PrunePartnerCooldowns();

    UFUNCTION() void HandleAICombatTargetChanged(AActor* NewTarget);
    UFUNCTION() void HandleCombatHitReceived(struct FARPGCombatHitInfo HitInfo);
    UFUNCTION() void HandleLifeStateChanged(EARPGLifeState NewState);

    UFUNCTION(NetMulticast, Reliable) void MulticastSocialStarted(AActor* Partner, FName InteractionId, bool bInitiator, UAnimMontage* Montage);
    UFUNCTION(NetMulticast, Reliable) void MulticastSocialBeat(AActor* Partner, FName InteractionId, USoundBase* Sound, const FText& Line);
    UFUNCTION(NetMulticast, Reliable) void MulticastSocialEnded(AActor* Partner, FName InteractionId, bool bInterrupted);
};
