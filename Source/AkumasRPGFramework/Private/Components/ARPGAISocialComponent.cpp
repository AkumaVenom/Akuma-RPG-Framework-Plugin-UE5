#include "Components/ARPGAISocialComponent.h"

#include "AIController.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/ARPGAICombatComponent.h"
#include "Components/ARPGAISplineComponent.h"
#include "Components/ARPGCombatComponent.h"
#include "Components/ARPGFactionComponent.h"
#include "Components/ARPGWandererComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

namespace
{
    const FName ARPGSocialWanderPauseReason(TEXT("SocialInteraction"));
}

UARPGAISocialComponent::UARPGAISocialComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(true);

    FARPGAISocialInteractionDefinition Conversation;
    Conversation.InteractionId = TEXT("Conversation");
    Conversation.Weight = 1.f;
    Conversation.MinDuration = 2.5f;
    Conversation.MaxDuration = 5.5f;
    InteractionPool.Add(Conversation);
}

void UARPGAISocialComponent::BeginPlay()
{
    Super::BeginPlay();

    if (UARPGAICombatComponent* AICombat = GetOwner() ? GetOwner()->FindComponentByClass<UARPGAICombatComponent>() : nullptr)
        AICombat->OnTargetChanged.AddDynamic(this, &UARPGAISocialComponent::HandleAICombatTargetChanged);

    if (UARPGCombatComponent* Combat = GetOwner() ? GetOwner()->FindComponentByClass<UARPGCombatComponent>() : nullptr)
    {
        Combat->OnCombatHitReceived.AddDynamic(this, &UARPGAISocialComponent::HandleCombatHitReceived);
        Combat->OnLifeStateChanged.AddDynamic(this, &UARPGAISocialComponent::HandleLifeStateChanged);
    }

    if (GetOwner() && GetOwner()->HasAuthority() && bEnableSocialInteractions)
    {
        // Give a freshly spawned NPC time to receive its spawner movement configuration and
        // choose an initial roam destination before ambient social opportunities begin.
        if (GetWorld())
        {
            const float DelayMin = FMath::Max(0.1f, FMath::Min(OpportunityRetryMin, OpportunityRetryMax));
            const float DelayMax = FMath::Max(DelayMin, FMath::Max(OpportunityRetryMin, OpportunityRetryMax));
            NextOpportunityAt = GetWorld()->GetTimeSeconds() + FMath::FRandRange(DelayMin, DelayMax);
        }
        EnsureScanTimer();
    }
}

void UARPGAISocialComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (GetOwner() && GetOwner()->HasAuthority() && IsSociallyEngaged())
        CancelSocialInteraction();

    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(ScanTimer);
        GetWorld()->GetTimerManager().ClearTimer(ActiveInteractionTimer);
    }

    if (UARPGAICombatComponent* AICombat = GetOwner() ? GetOwner()->FindComponentByClass<UARPGAICombatComponent>() : nullptr)
        AICombat->OnTargetChanged.RemoveDynamic(this, &UARPGAISocialComponent::HandleAICombatTargetChanged);

    if (UARPGCombatComponent* Combat = GetOwner() ? GetOwner()->FindComponentByClass<UARPGCombatComponent>() : nullptr)
    {
        Combat->OnCombatHitReceived.RemoveDynamic(this, &UARPGAISocialComponent::HandleCombatHitReceived);
        Combat->OnLifeStateChanged.RemoveDynamic(this, &UARPGAISocialComponent::HandleLifeStateChanged);
    }

    Super::EndPlay(EndPlayReason);
}

void UARPGAISocialComponent::EnsureScanTimer()
{
    if (!GetWorld() || !GetOwner() || !GetOwner()->HasAuthority() || !bEnableSocialInteractions || !bCanInitiateInteractions) return;
    if (GetWorld()->GetTimerManager().IsTimerActive(ScanTimer)) return;

    const float Interval = FMath::Max(0.25f, ScanInterval);
    GetWorld()->GetTimerManager().SetTimer(
        ScanTimer,
        this,
        &UARPGAISocialComponent::ScanForSocialOpportunity,
        Interval,
        true,
        FMath::FRandRange(0.05f, Interval));
}

void UARPGAISocialComponent::SetSocialInteractionsEnabled(bool bNewEnabled)
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;
    const bool bWasEnabled = bEnableSocialInteractions;
    bEnableSocialInteractions = bNewEnabled;

    if (!GetWorld()) return;
    if (bEnableSocialInteractions)
    {
        if (!bWasEnabled)
        {
            const float DelayMin = FMath::Max(0.1f, FMath::Min(OpportunityRetryMin, OpportunityRetryMax));
            const float DelayMax = FMath::Max(DelayMin, FMath::Max(OpportunityRetryMin, OpportunityRetryMax));
            NextOpportunityAt = GetWorld()->GetTimeSeconds() + FMath::FRandRange(DelayMin, DelayMax);
        }
        GetWorld()->GetTimerManager().ClearTimer(ScanTimer);
        EnsureScanTimer();
    }
    else
    {
        GetWorld()->GetTimerManager().ClearTimer(ScanTimer);
        if (IsSociallyEngaged()) CancelSocialInteraction();
    }
}

bool UARPGAISocialComponent::IsOwnerAvailableForSocial(bool bIgnoreCurrentSocialState) const
{
    const AActor* Owner = GetOwner();
    if (!Owner || !Owner->HasAuthority() || !bEnableSocialInteractions) return false;
    if (!bIgnoreCurrentSocialState && SocialState != EARPGAISocialState::Idle) return false;

    const APawn* Pawn = Cast<APawn>(Owner);
    if (Pawn && Pawn->IsPlayerControlled() && !bAllowPlayerControlledParticipants) return false;

    if (const UARPGCombatComponent* Combat = Owner->FindComponentByClass<UARPGCombatComponent>())
    {
        if (!Combat->IsAlive()) return false;
        if (Combat->bIsAttacking || Combat->IsInDefensiveState() || IsValid(Combat->CombatTarget)) return false;
    }

    if (const UARPGAICombatComponent* AICombat = Owner->FindComponentByClass<UARPGAICombatComponent>())
        if (IsValid(AICombat->CurrentTarget)) return false;

    // Do not steal an NPC from another temporary ambient movement owner (for example,
    // spawner group-cohesion recovery). During an existing social pair our own pause is ignored.
    if (const UARPGWandererComponent* Wanderer = Owner->FindComponentByClass<UARPGWandererComponent>())
    {
        if (Wanderer->HasMovementPauseOtherThan(ARPGSocialWanderPauseReason)) return false;

        // A Free-Roam pawn is not socially available until Unreal has actually accepted at least one
        // navigation request. This prevents startup social focus/rotation from masking a possession/NavMesh
        // readiness failure and guarantees the NPC first establishes normal locomotion.
        if (Wanderer->bEnabled && !Wanderer->HasEstablishedFreeRoam()) return false;
    }

    return true;
}

bool UARPGAISocialComponent::PassesTagRules(const UARPGAISocialComponent* Other) const
{
    if (!Other) return false;
    if (!BlockedPartnerTags.IsEmpty() && Other->SocialIdentityTags.HasAny(BlockedPartnerTags)) return false;
    if (!RequiredPartnerTags.IsEmpty() && !Other->SocialIdentityTags.HasAll(RequiredPartnerTags)) return false;
    return true;
}

bool UARPGAISocialComponent::PassesFactionRules(const UARPGAISocialComponent* Other) const
{
    if (!Other || !GetOwner() || !Other->GetOwner()) return false;

    const UARPGFactionComponent* Mine = GetOwner()->FindComponentByClass<UARPGFactionComponent>();
    const UARPGFactionComponent* Theirs = Other->GetOwner()->FindComponentByClass<UARPGFactionComponent>();
    const FName MyId = Mine ? Mine->GetPrimaryFactionId() : NAME_None;
    const FName TheirId = Theirs ? Theirs->GetPrimaryFactionId() : NAME_None;

    if (MyId.IsNone() || TheirId.IsNone())
        return bAllowFactionlessNPCs && Other->bAllowFactionlessNPCs;

    if (MyId == TheirId)
        return bAllowSameFaction && Other->bAllowSameFaction;

    const int32 MineToTheirs = Mine ? Mine->GetBaseRelationshipTo(Theirs) : 0;
    const int32 TheirsToMine = Theirs ? Theirs->GetBaseRelationshipTo(Mine) : 0;

    // Social ambience never overrides authored hostility in either direction.
    if (MineToTheirs < 0 || TheirsToMine < 0) return false;

    if (MineToTheirs > 0 || TheirsToMine > 0)
        return bAllowFriendlyFactions && Other->bAllowFriendlyFactions;

    return bAllowNeutralFactions && Other->bAllowNeutralFactions;
}

bool UARPGAISocialComponent::HasLineOfSightTo(const AActor* OtherActor) const
{
    if (!bRequireLineOfSight) return true;
    if (!GetOwner() || !OtherActor || !GetWorld()) return false;
    FHitResult Hit;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(ARPGAISocialLOS), false, GetOwner());
    const FVector Start = GetOwner()->GetActorLocation() + FVector(0.f, 0.f, 60.f);
    const FVector End = OtherActor->GetActorLocation() + FVector(0.f, 0.f, 60.f);
    if (!GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params)) return true;
    return Hit.GetActor() == OtherActor;
}

bool UARPGAISocialComponent::IsPartnerOnCooldown(AActor* Partner) const
{
    if (!Partner || !GetWorld()) return true;
    const TWeakObjectPtr<AActor> PartnerKey(Partner);
    if (const float* Until = PartnerCooldownUntil.Find(PartnerKey))
        return *Until > GetWorld()->GetTimeSeconds();
    return false;
}

bool UARPGAISocialComponent::CanSociallyInteractWith(AActor* Candidate) const
{
    if (!bCanInitiateInteractions || !Candidate || Candidate == GetOwner() || !IsOwnerAvailableForSocial(false)) return false;

    UARPGAISocialComponent* Other = Candidate->FindComponentByClass<UARPGAISocialComponent>();
    if (!Other || !Other->bCanRespondToInteractions || !Other->IsOwnerAvailableForSocial(false)) return false;
    if (!PassesTagRules(Other) || !Other->PassesTagRules(this)) return false;
    if (!PassesFactionRules(Other)) return false;

    const float AllowedDistance = FMath::Min(FMath::Max(100.f, DetectionRadius), FMath::Max(100.f, Other->DetectionRadius));
    if (FVector::DistSquared2D(GetOwner()->GetActorLocation(), Candidate->GetActorLocation()) > FMath::Square(AllowedDistance)) return false;
    if (!HasLineOfSightTo(Candidate) || !Other->HasLineOfSightTo(GetOwner())) return false;
    if (IsPartnerOnCooldown(Candidate) || Other->IsPartnerOnCooldown(GetOwner())) return false;

    FName DummyInteraction;
    return SelectCompatibleInteraction(Other, DummyInteraction);
}

const FARPGAISocialInteractionDefinition* UARPGAISocialComponent::FindInteractionDefinition(FName InteractionId, bool bForInitiation, bool bForResponse) const
{
    if (InteractionId.IsNone()) return nullptr;
    for (const FARPGAISocialInteractionDefinition& Entry : InteractionPool)
    {
        if (Entry.InteractionId != InteractionId) continue;
        if (bForInitiation && !Entry.bCanInitiate) continue;
        if (bForResponse && !Entry.bCanRespond) continue;
        return &Entry;
    }
    return nullptr;
}

bool UARPGAISocialComponent::SelectCompatibleInteraction(const UARPGAISocialComponent* Other, FName& OutInteractionId) const
{
    OutInteractionId = NAME_None;
    if (!Other) return false;

    float TotalWeight = 0.f;
    TArray<const FARPGAISocialInteractionDefinition*> Compatible;
    for (const FARPGAISocialInteractionDefinition& Entry : InteractionPool)
    {
        if (Entry.InteractionId.IsNone() || !Entry.bCanInitiate || Entry.Weight <= 0.f) continue;
        if (!Other->FindInteractionDefinition(Entry.InteractionId, false, true)) continue;
        Compatible.Add(&Entry);
        TotalWeight += Entry.Weight;
    }
    if (Compatible.IsEmpty() || TotalWeight <= 0.f) return false;

    float Roll = FMath::FRandRange(0.f, TotalWeight);
    for (const FARPGAISocialInteractionDefinition* Entry : Compatible)
    {
        Roll -= Entry->Weight;
        if (Roll <= 0.f)
        {
            OutInteractionId = Entry->InteractionId;
            return true;
        }
    }

    OutInteractionId = Compatible.Last()->InteractionId;
    return true;
}

float UARPGAISocialComponent::ResolvePairInteractionDuration(const UARPGAISocialComponent* Other, FName InteractionId) const
{
    const FARPGAISocialInteractionDefinition* Mine = FindInteractionDefinition(InteractionId, true, false);
    const FARPGAISocialInteractionDefinition* Theirs = Other ? Other->FindInteractionDefinition(InteractionId, false, true) : nullptr;
    if (!Mine || !Theirs) return 2.5f;

    const float MyMin = FMath::Max(0.25f, Mine->MinDuration);
    const float MyMax = FMath::Max(MyMin, Mine->MaxDuration);
    const float TheirMin = FMath::Max(0.25f, Theirs->MinDuration);
    const float TheirMax = FMath::Max(TheirMin, Theirs->MaxDuration);
    const float PairMin = FMath::Max(MyMin, TheirMin);
    const float PairMax = FMath::Min(MyMax, TheirMax);
    return PairMax >= PairMin ? FMath::FRandRange(PairMin, PairMax) : PairMin;
}

void UARPGAISocialComponent::PrunePartnerCooldowns()
{
    if (!GetWorld()) return;
    const float Now = GetWorld()->GetTimeSeconds();
    for (auto It = PartnerCooldownUntil.CreateIterator(); It; ++It)
    {
        if (!It.Key().IsValid() || It.Value() <= Now) It.RemoveCurrent();
    }
}

void UARPGAISocialComponent::ScanForSocialOpportunity()
{
    if (!GetWorld() || !GetOwner() || !GetOwner()->HasAuthority() || !bEnableSocialInteractions || !bCanInitiateInteractions) return;
    if (!IsOwnerAvailableForSocial(false)) return;

    const float Now = GetWorld()->GetTimeSeconds();
    if (Now < NextOpportunityAt) return;
    PrunePartnerCooldowns();

    FCollisionObjectQueryParams Objects;
    Objects.AddObjectTypesToQuery(ECC_Pawn);
    FCollisionQueryParams Params(SCENE_QUERY_STAT(ARPGAISocialScan), false, GetOwner());
    TArray<FOverlapResult> Overlaps;
    GetWorld()->OverlapMultiByObjectType(
        Overlaps,
        GetOwner()->GetActorLocation(),
        FQuat::Identity,
        Objects,
        FCollisionShape::MakeSphere(FMath::Max(100.f, DetectionRadius)),
        Params);

    struct FCandidate
    {
        UARPGAISocialComponent* Component = nullptr;
        float DistSq = 0.f;
    };
    TArray<FCandidate> Candidates;
    Candidates.Reserve(FMath::Min(MaxCandidatesPerScan, Overlaps.Num()));
    TSet<TWeakObjectPtr<AActor>> SeenActors;

    for (const FOverlapResult& Result : Overlaps)
    {
        AActor* CandidateActor = Result.GetActor();
        if (!CandidateActor || CandidateActor == GetOwner()) continue;
        const TWeakObjectPtr<AActor> CandidateKey(CandidateActor);
        if (SeenActors.Contains(CandidateKey)) continue;
        SeenActors.Add(CandidateKey);
        UARPGAISocialComponent* Other = CandidateActor->FindComponentByClass<UARPGAISocialComponent>();
        if (!Other || !CanSociallyInteractWith(CandidateActor)) continue;

        FCandidate Candidate;
        Candidate.Component = Other;
        Candidate.DistSq = FVector::DistSquared2D(GetOwner()->GetActorLocation(), CandidateActor->GetActorLocation());
        Candidates.Add(Candidate);
    }

    if (Candidates.IsEmpty()) return;
    Candidates.Sort([](const FCandidate& A, const FCandidate& B) { return A.DistSq < B.DistSq; });
    if (Candidates.Num() > FMath::Max(1, MaxCandidatesPerScan))
        Candidates.SetNum(FMath::Max(1, MaxCandidatesPerScan));

    NextOpportunityAt = Now + FMath::FRandRange(
        FMath::Max(0.1f, FMath::Min(OpportunityRetryMin, OpportunityRetryMax)),
        FMath::Max(FMath::Max(0.1f, OpportunityRetryMin), OpportunityRetryMax));

    if (FMath::FRand() > FMath::Clamp(InteractionChance, 0.f, 1.f)) return;

    // Bias toward naturally encountered nearby NPCs without making the closest actor deterministic.
    float TotalCandidateWeight = 0.f;
    TArray<float> Weights;
    Weights.Reserve(Candidates.Num());
    for (const FCandidate& Candidate : Candidates)
    {
        const float Distance = FMath::Sqrt(FMath::Max(1.f, Candidate.DistSq));
        const float Weight = 1.f / FMath::Max(100.f, Distance);
        Weights.Add(Weight);
        TotalCandidateWeight += Weight;
    }

    float Pick = FMath::FRandRange(0.f, TotalCandidateWeight);
    UARPGAISocialComponent* Chosen = Candidates[0].Component;
    for (int32 Index = 0; Index < Candidates.Num(); ++Index)
    {
        Pick -= Weights[Index];
        if (Pick <= 0.f)
        {
            Chosen = Candidates[Index].Component;
            break;
        }
    }

    if (!Chosen) return;
    FName InteractionId;
    if (SelectCompatibleInteraction(Chosen, InteractionId))
        StartPairAuthority(Chosen, InteractionId);
}

bool UARPGAISocialComponent::TryStartSocialInteractionWith(AActor* Partner)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || !Partner || !CanSociallyInteractWith(Partner)) return false;
    UARPGAISocialComponent* Other = Partner->FindComponentByClass<UARPGAISocialComponent>();
    FName InteractionId;
    return Other && SelectCompatibleInteraction(Other, InteractionId) && StartPairAuthority(Other, InteractionId);
}

bool UARPGAISocialComponent::ForceSocialInteractionWith(AActor* Partner, FName InteractionId)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || !Partner || Partner == GetOwner()) return false;
    if (!IsOwnerAvailableForSocial(false)) return false;

    UARPGAISocialComponent* Other = Partner->FindComponentByClass<UARPGAISocialComponent>();
    if (!Other || !Other->bCanRespondToInteractions || !Other->IsOwnerAvailableForSocial(false)) return false;
    if (!PassesTagRules(Other) || !Other->PassesTagRules(this) || !PassesFactionRules(Other)) return false;
    if (!FindInteractionDefinition(InteractionId, true, false) || !Other->FindInteractionDefinition(InteractionId, false, true)) return false;
    return StartPairAuthority(Other, InteractionId);
}

bool UARPGAISocialComponent::StartPairAuthority(UARPGAISocialComponent* Other, FName InteractionId)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || !Other || !Other->GetOwner() || InteractionId.IsNone()) return false;
    if (!IsOwnerAvailableForSocial(false) || !Other->IsOwnerAvailableForSocial(false)) return false;

    ReserveParticipantAuthority(Other->GetOwner(), InteractionId, true);
    Other->ReserveParticipantAuthority(GetOwner(), InteractionId, false);

    const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
    ApproachDeadline = Now + FMath::Max(0.25f, FMath::Min(ApproachTimeout, Other->ApproachTimeout));
    NextApproachMoveAt = 0.f;
    bNextExchangeFromInitiator = true;

    if (GetWorld())
    {
        const float Interval = FMath::Max(0.05f, ActiveUpdateInterval);
        GetWorld()->GetTimerManager().SetTimer(ActiveInteractionTimer, this, &UARPGAISocialComponent::UpdateActiveInteraction, Interval, true, 0.f);
        const float OtherInterval = FMath::Max(0.05f, Other->ActiveUpdateInterval);
        GetWorld()->GetTimerManager().SetTimer(Other->ActiveInteractionTimer, Other, &UARPGAISocialComponent::UpdateActiveInteraction, OtherInterval, true, OtherInterval);
    }

    const float PairDistance = FMath::Max(60.f, FMath::Min(InteractionDistance, Other->InteractionDistance));
    if (FVector::DistSquared2D(GetOwner()->GetActorLocation(), Other->GetOwner()->GetActorLocation()) <= FMath::Square(PairDistance))
        BeginConversationAuthority();

    return true;
}

void UARPGAISocialComponent::ReserveParticipantAuthority(AActor* Partner, FName InteractionId, bool bInitiator)
{
    CurrentPartner = Partner;
    CurrentInteractionId = InteractionId;
    bCurrentRoleInitiator = bInitiator;
    InteractionEndServerTime = 0.f;
    SetRuntimeState(EARPGAISocialState::Approaching);
    PauseAmbientMovementForSocial();
    if (GetOwner()) GetOwner()->ForceNetUpdate();
}

void UARPGAISocialComponent::PauseAmbientMovementForSocial()
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;

    bSocialWandererPauseApplied = false;
    bSplineWasActiveBeforeSocial = false;

    if (UARPGWandererComponent* Wanderer = GetOwner()->FindComponentByClass<UARPGWandererComponent>())
    {
        // Pause ownership is independent from bEnabled. If the spawner enables Free Roam while
        // this social reservation is active, the Wanderer remains safely paused and will resume
        // when this exact pause reason is released.
        Wanderer->AcquireMovementPause(ARPGSocialWanderPauseReason, true);
        bSocialWandererPauseApplied = true;
    }

    if (UARPGAISplineComponent* Spline = GetOwner()->FindComponentByClass<UARPGAISplineComponent>())
    {
        bSplineWasActiveBeforeSocial = Spline->IsRouteActive();
        if (bSplineWasActiveBeforeSocial) Spline->PauseRoute(true);
    }

    if (APawn* Pawn = Cast<APawn>(GetOwner()))
        if (AAIController* AI = Cast<AAIController>(Pawn->GetController())) AI->StopMovement();
}

void UARPGAISocialComponent::RestoreAmbientMovementAfterSocial()
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;

    const UARPGAICombatComponent* AICombat = GetOwner()->FindComponentByClass<UARPGAICombatComponent>();
    const bool bInCombat = AICombat && IsValid(AICombat->CurrentTarget);

    UARPGAISplineComponent* Spline = GetOwner()->FindComponentByClass<UARPGAISplineComponent>();
    if (bSplineWasActiveBeforeSocial && Spline)
    {
        // ResumeRoute clears the social/manual pause. If combat started during the exchange,
        // the spline's own combat suspension remains authoritative and prevents actual movement.
        Spline->ResumeRoute();
    }

    if (UARPGWandererComponent* Wanderer = GetOwner()->FindComponentByClass<UARPGWandererComponent>())
    {
        if (bSocialWandererPauseApplied)
        {
            const bool bSplineOwnsMovement = Spline && Spline->IsRouteActive();
            // Releasing our token never changes bEnabled. The Wanderer recurring timer is restored
            // if Free Roam is still authored/enabled, while immediate movement waits for combat/route ownership.
            Wanderer->ReleaseMovementPause(ARPGSocialWanderPauseReason, !bInCombat && !bSplineOwnsMovement);
        }
    }

    if (APawn* Pawn = Cast<APawn>(GetOwner()))
    {
        if (AAIController* AI = Cast<AAIController>(Pawn->GetController()))
        {
            if (!bInCombat) AI->ClearFocus(EAIFocusPriority::Gameplay);
        }
    }

    bSocialWandererPauseApplied = false;
    bSplineWasActiveBeforeSocial = false;
}

void UARPGAISocialComponent::BeginConversationAuthority()
{
    if (!bCurrentRoleInitiator || SocialState != EARPGAISocialState::Approaching || !IsValid(CurrentPartner) || !GetWorld()) return;
    UARPGAISocialComponent* Other = CurrentPartner->FindComponentByClass<UARPGAISocialComponent>();
    if (!Other || Other->CurrentPartner != GetOwner())
    {
        EndPairAuthority(true);
        return;
    }

    if (APawn* Pawn = Cast<APawn>(GetOwner()))
        if (AAIController* AI = Cast<AAIController>(Pawn->GetController())) AI->StopMovement();
    if (APawn* Pawn = Cast<APawn>(Other->GetOwner()))
        if (AAIController* AI = Cast<AAIController>(Pawn->GetController())) AI->StopMovement();

    const float Duration = ResolvePairInteractionDuration(Other, CurrentInteractionId);
    const float EndTime = GetWorld()->GetTimeSeconds() + Duration;
    InteractionEndServerTime = EndTime;
    Other->InteractionEndServerTime = EndTime;
    SetRuntimeState(EARPGAISocialState::Interacting);
    Other->SetRuntimeState(EARPGAISocialState::Interacting);
    BeginParticipantPresentationAuthority(EndTime);
    Other->BeginParticipantPresentationAuthority(EndTime);

    NextExchangeAt = GetWorld()->GetTimeSeconds() + FMath::Min(0.35f, FMath::Max(0.05f, Duration * 0.15f));
    bNextExchangeFromInitiator = true;
    if (GetOwner()) GetOwner()->ForceNetUpdate();
    if (Other->GetOwner()) Other->GetOwner()->ForceNetUpdate();
}

void UARPGAISocialComponent::BeginParticipantPresentationAuthority(float EndServerTime)
{
    InteractionEndServerTime = EndServerTime;
    const FARPGAISocialInteractionDefinition* Entry = FindInteractionDefinition(CurrentInteractionId, bCurrentRoleInitiator, !bCurrentRoleInitiator);
    UAnimMontage* Montage = nullptr;
    if (Entry && !Entry->Montages.IsEmpty())
    {
        const int32 Index = FMath::RandRange(0, Entry->Montages.Num() - 1);
        Montage = Entry->Montages[Index];
    }
    MulticastSocialStarted(CurrentPartner, CurrentInteractionId, bCurrentRoleInitiator, Montage);
}

void UARPGAISocialComponent::FacePartner(float DeltaSeconds)
{
    if (!bFacePartnerDuringInteraction || !GetOwner() || !IsValid(CurrentPartner)) return;

    FVector Direction = CurrentPartner->GetActorLocation() - GetOwner()->GetActorLocation();
    Direction.Z = 0.f;
    if (Direction.IsNearlyZero()) return;

    const FRotator Desired(0.f, Direction.Rotation().Yaw, 0.f);
    const FRotator Current = GetOwner()->GetActorRotation();
    const float StepSeconds = FMath::Max(0.01f, DeltaSeconds);
    const FRotator NewRotation = FMath::RInterpConstantTo(Current, Desired, StepSeconds, FMath::Max(1.f, FacingTurnRateDegreesPerSecond));
    GetOwner()->SetActorRotation(NewRotation);

    if (APawn* Pawn = Cast<APawn>(GetOwner()))
        if (AAIController* AI = Cast<AAIController>(Pawn->GetController())) AI->SetFocus(CurrentPartner, EAIFocusPriority::Gameplay);
}

void UARPGAISocialComponent::UpdateActiveInteraction()
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || SocialState == EARPGAISocialState::Idle || !GetWorld()) return;
    UARPGAISocialComponent* Other = IsValid(CurrentPartner) ? CurrentPartner->FindComponentByClass<UARPGAISocialComponent>() : nullptr;

    if (!bCurrentRoleInitiator)
    {
        // Responders keep a lightweight safety timer so a destroyed/lost coordinator can never
        // leave them permanently reserved. Normal lifecycle remains owned by the initiator.
        if (!Other || Other->CurrentPartner != GetOwner() || !Other->bCurrentRoleInitiator)
        {
            const float Now = GetWorld()->GetTimeSeconds();
            GetWorld()->GetTimerManager().ClearTimer(ActiveInteractionTimer);
            EndParticipantAuthority(true, Now + FMath::Max(0.f, InterruptedCooldown), Now);
            return;
        }
        if (!IsOwnerAvailableForSocial(true)) CancelSocialInteraction();
        return;
    }

    if (!Other || Other->CurrentPartner != GetOwner() || Other->CurrentInteractionId != CurrentInteractionId)
    {
        EndPairAuthority(true);
        return;
    }

    if (!IsOwnerAvailableForSocial(true) || !Other->IsOwnerAvailableForSocial(true))
    {
        EndPairAuthority(true);
        return;
    }

    const float Interval = FMath::Max(0.05f, ActiveUpdateInterval);
    FacePartner(Interval);
    Other->FacePartner(Interval);
    const float Now = GetWorld()->GetTimeSeconds();

    if (SocialState == EARPGAISocialState::Approaching)
    {
        const float PairDistance = FMath::Max(60.f, FMath::Min(InteractionDistance, Other->InteractionDistance));
        const float DistSq = FVector::DistSquared2D(GetOwner()->GetActorLocation(), CurrentPartner->GetActorLocation());
        if (DistSq <= FMath::Square(PairDistance))
        {
            BeginConversationAuthority();
            return;
        }

        if (Now >= ApproachDeadline)
        {
            EndPairAuthority(true);
            return;
        }

        if (Now >= NextApproachMoveAt)
        {
            if (APawn* Pawn = Cast<APawn>(GetOwner()))
            {
                if (AAIController* AI = Cast<AAIController>(Pawn->GetController()))
                    AI->MoveToActor(CurrentPartner, PairDistance * 0.85f, true, true, true, nullptr, true);
            }
            NextApproachMoveAt = Now + 0.45f;
        }
        return;
    }

    if (SocialState == EARPGAISocialState::Interacting)
    {
        if (Other->SocialState != EARPGAISocialState::Interacting)
        {
            EndPairAuthority(true);
            return;
        }

        if (Now >= InteractionEndServerTime)
        {
            EndPairAuthority(false);
            return;
        }

        if (Now >= NextExchangeAt)
        {
            UARPGAISocialComponent* Speaker = bNextExchangeFromInitiator ? this : Other;
            Speaker->EmitExchangeBeatAuthority();
            bNextExchangeFromInitiator = !bNextExchangeFromInitiator;

            const float MinBeat = FMath::Max(0.25f, FMath::Min(ExchangeIntervalMin, ExchangeIntervalMax));
            const float MaxBeat = FMath::Max(MinBeat, FMath::Max(ExchangeIntervalMin, ExchangeIntervalMax));
            NextExchangeAt = Now + FMath::FRandRange(MinBeat, MaxBeat);
        }
    }
}

void UARPGAISocialComponent::EmitExchangeBeatAuthority()
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || SocialState != EARPGAISocialState::Interacting || !IsValid(CurrentPartner)) return;
    const FARPGAISocialInteractionDefinition* Entry = FindInteractionDefinition(CurrentInteractionId, bCurrentRoleInitiator, !bCurrentRoleInitiator);
    if (!Entry) return;

    USoundBase* Sound = nullptr;
    if (!Entry->Sounds.IsEmpty())
        Sound = Entry->Sounds[FMath::RandRange(0, Entry->Sounds.Num() - 1)];

    FText Line;
    if (!Entry->Lines.IsEmpty())
        Line = Entry->Lines[FMath::RandRange(0, Entry->Lines.Num() - 1)];

    if (Sound || !Line.IsEmpty())
        MulticastSocialBeat(CurrentPartner, CurrentInteractionId, Sound, Line);
}

void UARPGAISocialComponent::CancelSocialInteraction()
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || SocialState == EARPGAISocialState::Idle) return;

    UARPGAISocialComponent* Other = IsValid(CurrentPartner) ? CurrentPartner->FindComponentByClass<UARPGAISocialComponent>() : nullptr;
    if (!bCurrentRoleInitiator && Other && Other->bCurrentRoleInitiator)
    {
        Other->EndPairAuthority(true);
        return;
    }
    EndPairAuthority(true);
}

void UARPGAISocialComponent::EndPairAuthority(bool bInterrupted)
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;

    UARPGAISocialComponent* Other = IsValid(CurrentPartner) ? CurrentPartner->FindComponentByClass<UARPGAISocialComponent>() : nullptr;
    const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
    const float Cooldown = bInterrupted
        ? FMath::Max(0.f, InterruptedCooldown)
        : FMath::FRandRange(
            FMath::Max(0.f, FMath::Min(InteractionCooldownMin, InteractionCooldownMax)),
            FMath::Max(FMath::Max(0.f, InteractionCooldownMin), InteractionCooldownMax));
    const float PartnerCooldown = Now + FMath::Max(0.f, SamePartnerCooldown);

    if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(ActiveInteractionTimer);
    EndParticipantAuthority(bInterrupted, Now + Cooldown, PartnerCooldown);

    if (Other && Other->CurrentPartner == GetOwner())
    {
        const float OtherCooldown = bInterrupted
            ? FMath::Max(0.f, Other->InterruptedCooldown)
            : FMath::FRandRange(
                FMath::Max(0.f, FMath::Min(Other->InteractionCooldownMin, Other->InteractionCooldownMax)),
                FMath::Max(FMath::Max(0.f, Other->InteractionCooldownMin), Other->InteractionCooldownMax));
        Other->EndParticipantAuthority(bInterrupted, Now + OtherCooldown, Now + FMath::Max(0.f, Other->SamePartnerCooldown));
    }
}

void UARPGAISocialComponent::EndParticipantAuthority(bool bInterrupted, float CooldownUntil, float PartnerCooldownTime)
{
    if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(ActiveInteractionTimer);
    AActor* OldPartner = CurrentPartner;
    const FName OldInteraction = CurrentInteractionId;
    if (OldPartner) PartnerCooldownUntil.Add(TWeakObjectPtr<AActor>(OldPartner), PartnerCooldownTime);
    NextOpportunityAt = FMath::Max(NextOpportunityAt, CooldownUntil);

    MulticastSocialEnded(OldPartner, OldInteraction, bInterrupted);
    RestoreAmbientMovementAfterSocial();

    CurrentPartner = nullptr;
    CurrentInteractionId = NAME_None;
    bCurrentRoleInitiator = false;
    InteractionEndServerTime = 0.f;
    SetRuntimeState(EARPGAISocialState::Idle);
    if (GetOwner()) GetOwner()->ForceNetUpdate();
}

void UARPGAISocialComponent::SetRuntimeState(EARPGAISocialState NewState)
{
    if (SocialState == NewState) return;
    SocialState = NewState;
    OnSocialStateChanged.Broadcast(SocialState, CurrentPartner);
}

void UARPGAISocialComponent::HandleAICombatTargetChanged(AActor* NewTarget)
{
    if (NewTarget && GetOwner() && GetOwner()->HasAuthority() && SocialState != EARPGAISocialState::Idle)
        CancelSocialInteraction();
}

void UARPGAISocialComponent::HandleCombatHitReceived(FARPGCombatHitInfo HitInfo)
{
    if (GetOwner() && GetOwner()->HasAuthority() && SocialState != EARPGAISocialState::Idle)
        CancelSocialInteraction();
}

void UARPGAISocialComponent::HandleLifeStateChanged(EARPGLifeState NewState)
{
    if (NewState != EARPGLifeState::Alive && GetOwner() && GetOwner()->HasAuthority() && SocialState != EARPGAISocialState::Idle)
        CancelSocialInteraction();
}

void UARPGAISocialComponent::MulticastSocialStarted_Implementation(AActor* Partner, FName InteractionId, bool bInitiator, UAnimMontage* Montage)
{
    LocalPresentationMontage = Montage;
    if (Montage)
    {
        if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
        {
            if (USkeletalMeshComponent* Mesh = Character->GetMesh())
                if (UAnimInstance* AnimInstance = Mesh->GetAnimInstance()) AnimInstance->Montage_Play(Montage);
        }
    }
    OnSocialInteractionStarted.Broadcast(Partner, InteractionId, bInitiator);
}

void UARPGAISocialComponent::MulticastSocialBeat_Implementation(AActor* Partner, FName InteractionId, USoundBase* Sound, const FText& Line)
{
    if (Sound && GetOwner()) UGameplayStatics::PlaySoundAtLocation(this, Sound, GetOwner()->GetActorLocation());
    if (!Line.IsEmpty()) OnSocialLineSpoken.Broadcast(Partner, InteractionId, Line);
}

void UARPGAISocialComponent::MulticastSocialEnded_Implementation(AActor* Partner, FName InteractionId, bool bInterrupted)
{
    if (LocalPresentationMontage.IsValid())
    {
        if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
        {
            if (USkeletalMeshComponent* Mesh = Character->GetMesh())
            {
                if (UAnimInstance* AnimInstance = Mesh->GetAnimInstance())
                {
                    if (AnimInstance->Montage_IsPlaying(LocalPresentationMontage.Get()))
                        AnimInstance->Montage_Stop(0.15f, LocalPresentationMontage.Get());
                }
            }
        }
    }
    LocalPresentationMontage.Reset();
    OnSocialInteractionEnded.Broadcast(Partner, InteractionId, bInterrupted);
}

void UARPGAISocialComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(UARPGAISocialComponent, SocialState);
    DOREPLIFETIME(UARPGAISocialComponent, CurrentPartner);
    DOREPLIFETIME(UARPGAISocialComponent, CurrentInteractionId);
    DOREPLIFETIME(UARPGAISocialComponent, bCurrentRoleInitiator);
    DOREPLIFETIME(UARPGAISocialComponent, InteractionEndServerTime);
}
