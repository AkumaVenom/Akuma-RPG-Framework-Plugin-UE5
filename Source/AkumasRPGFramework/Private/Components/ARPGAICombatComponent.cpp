#include "Components/ARPGAICombatComponent.h"
#include "Components/ARPGAISplineComponent.h"

#include "AIController.h"
#include "Components/ARPGAbilityBridgeComponent.h"
#include "Components/ARPGCombatComponent.h"
#include "Components/ARPGFactionComponent.h"
#include "Components/ARPGStatsComponent.h"
#include "Components/ARPGThreatComponent.h"
#include "Components/ARPGWandererComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "NavigationSystem.h"
#include "TimerManager.h"

UARPGAICombatComponent::UARPGAICombatComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UARPGAICombatComponent::BeginPlay()
{
    Super::BeginPlay();
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;

    HomeLocation = GetOwner()->GetActorLocation();

    if (UARPGCombatComponent* Combat = GetOwner()->FindComponentByClass<UARPGCombatComponent>())
    {
        Combat->OnCombatHitReceived.RemoveDynamic(this, &UARPGAICombatComponent::HandleCombatHitReceived);
        Combat->OnCombatHitReceived.AddDynamic(this, &UARPGAICombatComponent::HandleCombatHitReceived);
    }

    if (GetWorld())
    {
        GetWorld()->GetTimerManager().SetTimer(
            ThinkTimer,
            this,
            &UARPGAICombatComponent::Think,
            FMath::Max(0.05f, ThinkInterval),
            true,
            FMath::FRandRange(0.05f, FMath::Max(0.06f, ThinkInterval)));
    }
}

void UARPGAICombatComponent::SetAICombatEnabled(bool bNewEnabled)
{
    bEnabled = bNewEnabled;
    if (!bEnabled && GetOwner() && GetOwner()->HasAuthority()) ClearCombatTarget(false);
}

void UARPGAICombatComponent::SetSpawnGroupOwner(AActor* NewSpawnGroupOwner)
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;
    SpawnGroupOwner = NewSpawnGroupOwner;
}

void UARPGAICombatComponent::ForgetTemporaryAggressionAgainst(AActor* Actor, bool bClearThreat)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || !Actor) return;
    RetaliationMemory.Remove(TWeakObjectPtr<AActor>(Actor));
    if (bClearThreat)
        if (UARPGThreatComponent* Threat = GetOwner()->FindComponentByClass<UARPGThreatComponent>())
            Threat->RemoveActor(Actor);
}

void UARPGAICombatComponent::ForgetAllTemporaryAggression(bool bClearThreat)
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;

    TArray<TWeakObjectPtr<AActor>> RememberedActors;
    RememberedActors.Reserve(RetaliationMemory.Num());
    for (const TPair<TWeakObjectPtr<AActor>, float>& Pair : RetaliationMemory)
        RememberedActors.Add(Pair.Key);
    RetaliationMemory.Reset();

    if (bClearThreat)
    {
        if (UARPGThreatComponent* Threat = GetOwner()->FindComponentByClass<UARPGThreatComponent>())
        {
            for (const TWeakObjectPtr<AActor>& Remembered : RememberedActors)
                if (AActor* Actor = Remembered.Get()) Threat->RemoveActor(Actor);
        }
    }
}

bool UARPGAICombatComponent::HasAIController() const
{
    const APawn* Pawn = Cast<APawn>(GetOwner());
    return Pawn && Cast<AAIController>(Pawn->GetController()) != nullptr;
}

bool UARPGAICombatComponent::IsTargetHostile(AActor* Candidate) const
{
    if (!GetOwner() || !Candidate || Candidate == GetOwner()) return false;
    const UARPGCombatComponent* TargetCombat = Candidate->FindComponentByClass<UARPGCombatComponent>();
    if (TargetCombat && !TargetCombat->IsAlive()) return false;
    const UARPGFactionComponent* Mine = GetOwner()->FindComponentByClass<UARPGFactionComponent>();
    const UARPGFactionComponent* Theirs = Candidate->FindComponentByClass<UARPGFactionComponent>();
    return Mine && Theirs && Mine->IsHostileTo(Theirs);
}

bool UARPGAICombatComponent::HasActiveRetaliationAgainst(AActor* Candidate) const
{
    if (!Candidate || !GetWorld()) return false;
    if (const float* Expiry = RetaliationMemory.Find(TWeakObjectPtr<AActor>(Candidate)))
        return *Expiry > GetWorld()->GetTimeSeconds();
    return false;
}

bool UARPGAICombatComponent::CanRetaliateAgainst(AActor* Candidate) const
{
    if (!bRetaliateWhenAttacked || !GetOwner() || !Candidate || Candidate == GetOwner()) return false;

    if (const UARPGCombatComponent* TargetCombat = Candidate->FindComponentByClass<UARPGCombatComponent>())
        if (!TargetCombat->IsAlive()) return false;

    const UARPGFactionComponent* Mine = GetOwner()->FindComponentByClass<UARPGFactionComponent>();
    const UARPGFactionComponent* Theirs = Candidate->FindComponentByClass<UARPGFactionComponent>();
    const bool bMineHasFaction = Mine && Mine->HasFactionIdentity();
    const bool bTheirsHasFaction = Theirs && Theirs->HasFactionIdentity();

    if (!bMineHasFaction || !bTheirsHasFaction)
        return bRetaliateWhenFactionUnknown;

    const int32 Relationship = Mine->GetBaseRelationshipTo(Theirs);
    if (Relationship < 0) return true;
    if (Relationship > 0) return bRetaliateAgainstFriendlyAttackers;
    return bRetaliationOverridesNeutralFaction;
}

bool UARPGAICombatComponent::IsProactiveHostileTarget(AActor* Candidate) const
{
    if (!GetOwner() || !Candidate || Candidate == GetOwner()) return false;

    if (const UARPGCombatComponent* TargetCombat = Candidate->FindComponentByClass<UARPGCombatComponent>())
        if (!TargetCombat->IsAlive()) return false;

    const UARPGFactionComponent* Mine = GetOwner()->FindComponentByClass<UARPGFactionComponent>();
    const UARPGFactionComponent* Theirs = Candidate->FindComponentByClass<UARPGFactionComponent>();

    if (Mine && Theirs && Mine->ShouldAttackOnSight(Theirs))
        return true;

    // Explicit friendliness always wins over fallback aggression.
    if (Mine && Theirs && Mine->GetBaseRelationshipTo(Theirs) > 0)
        return false;

    const APawn* CandidatePawn = Cast<APawn>(Candidate);
    const bool bPlayerControlled = CandidatePawn && CandidatePawn->IsPlayerControlled();
    if (bAttackPlayersOnSightFallback && bPlayerControlled)
        return true;

    if (bAttackUnfactionedPawnsOnSightFallback && CandidatePawn)
    {
        const bool bMineHasFaction = Mine && Mine->HasFactionIdentity();
        const bool bTheirsHasFaction = Theirs && Theirs->HasFactionIdentity();
        if (!bMineHasFaction || !bTheirsHasFaction)
            return true;
    }

    return false;
}

bool UARPGAICombatComponent::IsTargetConsideredHostile(AActor* Candidate) const
{
    if (!GetOwner() || !Candidate || Candidate == GetOwner()) return false;
    if (const UARPGCombatComponent* TargetCombat = Candidate->FindComponentByClass<UARPGCombatComponent>())
        if (!TargetCombat->IsAlive()) return false;

    return IsTargetHostile(Candidate) || HasActiveRetaliationAgainst(Candidate) || IsProactiveHostileTarget(Candidate);
}

bool UARPGAICombatComponent::IsPotentialAlly(AActor* Candidate) const
{
    if (!GetOwner() || !Candidate || Candidate == GetOwner()) return false;

    const UARPGAICombatComponent* OtherAI = Candidate->FindComponentByClass<UARPGAICombatComponent>();
    if (!OtherAI || !OtherAI->bEnabled) return false;
    if (const UARPGCombatComponent* OtherCombat = Candidate->FindComponentByClass<UARPGCombatComponent>())
        if (!OtherCombat->IsAlive()) return false;

    if (bAssistSameSpawnGroup && SpawnGroupOwner && OtherAI->SpawnGroupOwner == SpawnGroupOwner)
        return true;

    if (!AssistGroupId.IsNone() && OtherAI->AssistGroupId == AssistGroupId)
        return true;

    const UARPGFactionComponent* Mine = GetOwner()->FindComponentByClass<UARPGFactionComponent>();
    const UARPGFactionComponent* Theirs = Candidate->FindComponentByClass<UARPGFactionComponent>();
    const bool bMineHasFaction = Mine && Mine->HasFactionIdentity();
    const bool bTheirsHasFaction = Theirs && Theirs->HasFactionIdentity();

    if (bMineHasFaction && bTheirsHasFaction)
    {
        if (bAssistSameFaction && Mine->GetPrimaryFactionId() == Theirs->GetPrimaryFactionId())
            return true;

        const int32 Relationship = Mine->GetBaseRelationshipTo(Theirs);
        if (Relationship > 0 && bAssistAlliedFactions)
            return true;
        if (Relationship < 0)
            return false;
    }

    if (bAssistSameClassWhenFactionUnknown && (!bMineHasFaction || !bTheirsHasFaction))
        return GetOwner()->GetClass() == Candidate->GetClass();

    return false;
}

void UARPGAICombatComponent::PruneRetaliationMemory()
{
    if (!GetWorld()) return;
    const float Now = GetWorld()->GetTimeSeconds();
    TArray<TWeakObjectPtr<AActor>> ThreatEntriesToRemove;

    for (auto It = RetaliationMemory.CreateIterator(); It; ++It)
    {
        AActor* RememberedActor = It.Key().Get();
        const bool bExpired = It.Value() <= Now;
        bool bDead = false;
        if (RememberedActor)
        {
            if (const UARPGCombatComponent* RememberedCombat = RememberedActor->FindComponentByClass<UARPGCombatComponent>())
                bDead = !RememberedCombat->IsAlive();
        }

        const bool bRemoveForDeath = bDead && bRestoreOriginalDispositionAfterTargetDeath;
        if (!RememberedActor || bExpired || bRemoveForDeath)
        {
            if (RememberedActor)
            {
                const bool bTemporaryOnly = !IsTargetHostile(RememberedActor) && !IsProactiveHostileTarget(RememberedActor);
                if ((bRemoveForDeath && bClearThreatAgainstDeadTargets) || (bExpired && bTemporaryOnly))
                    ThreatEntriesToRemove.Add(It.Key());
            }
            It.RemoveCurrent();
        }
    }

    if (ThreatEntriesToRemove.Num() > 0)
    {
        if (UARPGThreatComponent* Threat = GetOwner() ? GetOwner()->FindComponentByClass<UARPGThreatComponent>() : nullptr)
            for (const TWeakObjectPtr<AActor>& Entry : ThreatEntriesToRemove)
                if (AActor* Actor = Entry.Get()) Threat->RemoveActor(Actor);
    }
}

void UARPGAICombatComponent::RememberAggression(AActor* Attacker, float ThreatBonus, bool bWasDirectAttack)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || !IsValid(Attacker) || Attacker == GetOwner()) return;
    if (!CanRetaliateAgainst(Attacker)) return;

    if (GetWorld())
    {
        const float Expiry = GetWorld()->GetTimeSeconds() + FMath::Max(0.25f, RetaliationMemorySeconds);
        RetaliationMemory.FindOrAdd(TWeakObjectPtr<AActor>(Attacker)) = Expiry;
    }

    if (UARPGThreatComponent* Threat = GetOwner()->FindComponentByClass<UARPGThreatComponent>())
        Threat->AddThreat(Attacker, FMath::Max(0.f, ThreatBonus));

    if (!IsValid(CurrentTarget) || (bWasDirectAttack && bDirectRetaliationCanOverrideExistingTarget))
        SetTargetAuthority(Attacker);

    OnAggroTriggered.Broadcast(Attacker, bWasDirectAttack);
}

void UARPGAICombatComponent::CallForHelp(AActor* Attacker)
{
    if (!bCallForHelpWhenAttacked || !GetOwner() || !GetWorld() || !IsValid(Attacker)) return;

    FCollisionObjectQueryParams Objects;
    Objects.AddObjectTypesToQuery(ECC_Pawn);
    FCollisionQueryParams Params(SCENE_QUERY_STAT(ARPGAIAllyAssist), false, GetOwner());
    TArray<FOverlapResult> Overlaps;
    GetWorld()->OverlapMultiByObjectType(
        Overlaps,
        GetOwner()->GetActorLocation(),
        FQuat::Identity,
        Objects,
        FCollisionShape::MakeSphere(FMath::Max(100.f, AllyAssistRadius)),
        Params);

    TSet<TWeakObjectPtr<AActor>> Processed;
    for (const FOverlapResult& Result : Overlaps)
    {
        AActor* Ally = Result.GetActor();
        if (!Ally || Ally == GetOwner() || Processed.Contains(TWeakObjectPtr<AActor>(Ally))) continue;
        Processed.Add(TWeakObjectPtr<AActor>(Ally));
        if (!IsPotentialAlly(Ally)) continue;

        if (UARPGAICombatComponent* AllyAI = Ally->FindComponentByClass<UARPGAICombatComponent>())
        {
            AllyAI->ReceiveAggroCall(Attacker, GetOwner());
            OnAllyAssistTriggered.Broadcast(Ally, Attacker);
        }
    }
}

void UARPGAICombatComponent::ReceiveAggroCall(AActor* Attacker, AActor* Caller)
{
    if (!bEnabled || !GetOwner() || !GetOwner()->HasAuthority() || !IsValid(Attacker) || !IsValid(Caller)) return;
    if (!IsPotentialAlly(Caller) || !CanRetaliateAgainst(Attacker)) return;
    if (IsValid(CurrentTarget) && !bAssistCanOverrideExistingTarget) return;

    RememberAggression(Attacker, FMath::Max(0.f, AllyAssistThreatBonus), false);
}

void UARPGAICombatComponent::HandleCombatHitReceived(FARPGCombatHitInfo HitInfo)
{
    if (!bEnabled || !bRetaliateWhenAttacked || !GetOwner() || !GetOwner()->HasAuthority()) return;
    AActor* Attacker = HitInfo.Attacker;
    if (!IsValid(Attacker) || Attacker == GetOwner()) return;

    // Immune means the attack never meaningfully reached this combatant. Friendly is only allowed
    // to aggro when the designer explicitly opted into retaliating against friendly attackers.
    if (HitInfo.Result == EARPGCombatHitResult::Immune) return;
    if (HitInfo.Result == EARPGCombatHitResult::Friendly && !bRetaliateAgainstFriendlyAttackers) return;
    if (!CanRetaliateAgainst(Attacker)) return;

    RememberAggression(Attacker, FMath::Max(1.f, RetaliationThreatBonus), true);
    CallForHelp(Attacker);
}

AActor* UARPGAICombatComponent::FindBestHostileTarget() const
{
    if (!GetOwner() || !GetWorld()) return nullptr;

    if (bUseThreatFirst)
    {
        if (const UARPGThreatComponent* Threat = GetOwner()->FindComponentByClass<UARPGThreatComponent>())
        {
            if (AActor* ThreatTarget = Threat->GetHighestThreatActor())
            {
                if (IsTargetConsideredHostile(ThreatTarget) && FVector::DistSquared(GetOwner()->GetActorLocation(), ThreatTarget->GetActorLocation()) <= FMath::Square(LoseTargetRadius))
                    return ThreatTarget;
            }
        }
    }

    if (!bAutoAcquireHostileTargets) return nullptr;

    FCollisionObjectQueryParams Objects;
    Objects.AddObjectTypesToQuery(ECC_Pawn);
    FCollisionQueryParams Params(SCENE_QUERY_STAT(ARPGAITargetScan), false, GetOwner());
    TArray<FOverlapResult> Overlaps;
    GetWorld()->OverlapMultiByObjectType(Overlaps, GetOwner()->GetActorLocation(), FQuat::Identity, Objects, FCollisionShape::MakeSphere(FMath::Max(100.f, DetectionRadius)), Params);

    AActor* Best = nullptr;
    float BestDistanceSq = TNumericLimits<float>::Max();
    for (const FOverlapResult& Result : Overlaps)
    {
        AActor* Candidate = Result.GetActor();
        if (!IsProactiveHostileTarget(Candidate)) continue;
        const float DistanceSq = FVector::DistSquared(GetOwner()->GetActorLocation(), Candidate->GetActorLocation());
        if (DistanceSq < BestDistanceSq)
        {
            Best = Candidate;
            BestDistanceSq = DistanceSq;
        }
    }
    return Best;
}

void UARPGAICombatComponent::ForceCombatTarget(AActor* NewTarget)
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;
    if (IsValid(NewTarget) && NewTarget != GetOwner() && GetWorld())
    {
        const float Expiry = GetWorld()->GetTimeSeconds() + FMath::Max(0.25f, RetaliationMemorySeconds);
        RetaliationMemory.FindOrAdd(TWeakObjectPtr<AActor>(NewTarget)) = Expiry;
    }
    SetTargetAuthority(NewTarget);
}

void UARPGAICombatComponent::BindTargetLifeState(AActor* Target)
{
    UnbindTargetLifeState();
    if (!Target) return;
    if (UARPGCombatComponent* TargetCombat = Target->FindComponentByClass<UARPGCombatComponent>())
    {
        TargetCombat->OnLifeStateChanged.RemoveDynamic(this, &UARPGAICombatComponent::HandleTargetLifeStateChanged);
        TargetCombat->OnLifeStateChanged.AddDynamic(this, &UARPGAICombatComponent::HandleTargetLifeStateChanged);
        BoundTargetCombat = TargetCombat;
    }
}

void UARPGAICombatComponent::UnbindTargetLifeState()
{
    if (UARPGCombatComponent* TargetCombat = BoundTargetCombat.Get())
        TargetCombat->OnLifeStateChanged.RemoveDynamic(this, &UARPGAICombatComponent::HandleTargetLifeStateChanged);
    BoundTargetCombat.Reset();
}

void UARPGAICombatComponent::HandleTargetLifeStateChanged(EARPGLifeState NewState)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || NewState == EARPGLifeState::Alive) return;
    AActor* DeadTarget = CurrentTarget;
    if (!DeadTarget) return;

    if (bRestoreOriginalDispositionAfterTargetDeath)
        ForgetTemporaryAggressionAgainst(DeadTarget, bClearThreatAgainstDeadTargets);

    ClearCombatTarget(bReturnHomeAfterCombat);
}

void UARPGAICombatComponent::ResetActiveMoveState()
{
    ActiveMoveTarget.Reset();
    ActiveMoveLocation = FVector::ZeroVector;
    bHasActiveMoveLocation = false;
    LastMoveIssuedAt = -1000.f;
}

void UARPGAICombatComponent::ResetGroupCombatRuntime()
{
    bHadAttackSlotLastThink = false;
    AttackSlotGrantedAt = -1.f;
    NextAttackSlotEligibleAt = -1.f;
    SetGroupCombatRole(EARPGGroupCombatRole::Solo, 1, 0, true);
}

void UARPGAICombatComponent::SetGroupCombatRole(EARPGGroupCombatRole NewRole, int32 GroupSize, int32 SlotIndex, bool bHasAttackSlot)
{
    const bool bChanged = CurrentGroupCombatRole != NewRole || EngagementGroupSize != GroupSize || EngagementSlotIndex != SlotIndex || bHasMeleeAttackSlot != bHasAttackSlot;
    CurrentGroupCombatRole = NewRole;
    EngagementGroupSize = FMath::Max(1, GroupSize);
    EngagementSlotIndex = FMath::Max(0, SlotIndex);
    bHasMeleeAttackSlot = bHasAttackSlot;
    if (bChanged) OnGroupCombatRoleChanged.Broadcast(CurrentGroupCombatRole, EngagementGroupSize, EngagementSlotIndex);
}

void UARPGAICombatComponent::SetTargetAuthority(AActor* NewTarget)
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;
    if (NewTarget == GetOwner()) NewTarget = nullptr;
    if (CurrentTarget == NewTarget) return;

    const bool bWasInCombat = IsValid(CurrentTarget);
    const bool bEnteringCombat = !bWasInCombat && IsValid(NewTarget);
    const bool bLeavingCombat = bWasInCombat && !IsValid(NewTarget);
    if (UARPGAISplineComponent* SplineMovement = GetOwner()->FindComponentByClass<UARPGAISplineComponent>())
    {
        if (bEnteringCombat && SplineMovement->IsRouteActive())
        {
            if (SplineMovement->bUseRouteAsCombatLeashAnchor) HomeLocation = SplineMovement->GetCurrentRouteAnchorLocation();
            SplineMovement->NotifyCombatStarted();
        }
        else if (bLeavingCombat)
        {
            SplineMovement->NotifyCombatEnded();
        }
    }

    UnbindTargetLifeState();
    CurrentTarget = NewTarget;
    BindTargetLifeState(NewTarget);
    ResetActiveMoveState();
    ResetGroupCombatRuntime();
    bMovingHome = false;
    ObservedEnemyAttackSerial = INDEX_NONE;
    ReactedEnemyAttackSerial = INDEX_NONE;
    ReactionDueAt = -1.f;

    APawn* Pawn = Cast<APawn>(GetOwner());
    if (AAIController* AI = Pawn ? Cast<AAIController>(Pawn->GetController()) : nullptr)
    {
        if (NewTarget) AI->SetFocus(NewTarget, EAIFocusPriority::Gameplay);
        else AI->ClearFocus(EAIFocusPriority::Gameplay);
    }

    if (UARPGCombatComponent* Combat = GetOwner()->FindComponentByClass<UARPGCombatComponent>()) Combat->SetCombatTarget(NewTarget);
    SetWandererSuppressed(NewTarget != nullptr);
    OnTargetChanged.Broadcast(NewTarget);
}

void UARPGAICombatComponent::ClearCombatTarget(bool bReturnHome)
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;
    APawn* Pawn = Cast<APawn>(GetOwner());
    SetTargetAuthority(nullptr);
    ResetActiveMoveState();
    bMovingHome = false;
    if (AAIController* AI = Pawn ? Cast<AAIController>(Pawn->GetController()) : nullptr)
    {
        AI->StopMovement();
        if (const UARPGAISplineComponent* SplineMovement = GetOwner()->FindComponentByClass<UARPGAISplineComponent>())
            if (SplineMovement->IsRouteActive()) return;

        if (bReturnHome)
        {
            AI->MoveToLocation(HomeLocation, 100.f, true, true, true, false, nullptr, true);
            bMovingHome = true;
        }
    }
}

void UARPGAICombatComponent::SetWandererSuppressed(bool bSuppressed) const
{
    if (!GetOwner()) return;
    if (UARPGWandererComponent* Wanderer = GetOwner()->FindComponentByClass<UARPGWandererComponent>())
    {
        if (!bSuppressed)
        {
            if (const UARPGAISplineComponent* SplineMovement = GetOwner()->FindComponentByClass<UARPGAISplineComponent>())
                if (SplineMovement->IsRouteActive()) return;
        }
        Wanderer->SetWandererEnabled(!bSuppressed);
    }
}

void UARPGAICombatComponent::GatherCoordinatedMeleeAttackers(AActor* Target, TArray<UARPGAICombatComponent*>& OutMembers) const
{
    OutMembers.Reset();
    if (!bEnableGroupCombatCoordination || !GetOwner() || !GetWorld() || !Target) return;

    FCollisionObjectQueryParams Objects;
    Objects.AddObjectTypesToQuery(ECC_Pawn);
    FCollisionQueryParams Params(SCENE_QUERY_STAT(ARPGAIGroupCombatScan), false, Target);
    TArray<FOverlapResult> Overlaps;
    GetWorld()->OverlapMultiByObjectType(
        Overlaps,
        Target->GetActorLocation(),
        FQuat::Identity,
        Objects,
        FCollisionShape::MakeSphere(FMath::Max(300.f, GroupCombatCoordinationRadius)),
        Params);

    TSet<TWeakObjectPtr<AActor>> Processed;
    for (const FOverlapResult& Result : Overlaps)
    {
        AActor* CandidateActor = Result.GetActor();
        if (!CandidateActor || Processed.Contains(TWeakObjectPtr<AActor>(CandidateActor))) continue;
        Processed.Add(TWeakObjectPtr<AActor>(CandidateActor));

        UARPGAICombatComponent* CandidateAI = CandidateActor->FindComponentByClass<UARPGAICombatComponent>();
        UARPGCombatComponent* CandidateCombat = CandidateActor->FindComponentByClass<UARPGCombatComponent>();
        if (!CandidateAI || !CandidateCombat || !CandidateAI->bEnabled || !CandidateCombat->IsAlive()) continue;
        if (CandidateAI->CurrentTarget != Target) continue;
        if (CandidateCombat->GetCombatProfile().BasicAttackType != EARPGBasicAttackType::Melee) continue;
        if (bCoordinateOnlyWithAllies && CandidateAI != this && !IsPotentialAlly(CandidateActor)) continue;
        OutMembers.Add(CandidateAI);
    }

    if (!OutMembers.Contains(const_cast<UARPGAICombatComponent*>(this)))
        OutMembers.Add(const_cast<UARPGAICombatComponent*>(this));

    OutMembers.Sort([](const UARPGAICombatComponent& A, const UARPGAICombatComponent& B)
    {
        const AActor* OwnerA = A.GetOwner();
        const AActor* OwnerB = B.GetOwner();
        const uint32 IdA = OwnerA ? OwnerA->GetUniqueID() : 0u;
        const uint32 IdB = OwnerB ? OwnerB->GetUniqueID() : 0u;
        return IdA < IdB;
    });
}

bool UARPGAICombatComponent::EvaluateMeleeAttackSlot(AActor* Target, UARPGCombatComponent* MyCombat, int32& OutSlotIndex, int32& OutGroupSize, int32& OutOrbitDirectionSign)
{
    OutSlotIndex = 0;
    OutGroupSize = 1;
    OutOrbitDirectionSign = 1;
    if (!Target || !MyCombat || !GetWorld() || !bEnableGroupCombatCoordination) return true;

    const float Now = GetWorld()->GetTimeSeconds();
    if (bHadAttackSlotLastThink && !MyCombat->bIsAttacking && AttackSlotGrantedAt >= 0.f && Now - AttackSlotGrantedAt >= FMath::Max(0.5f, AttackSlotMaxHoldSeconds))
    {
        bHadAttackSlotLastThink = false;
        AttackSlotGrantedAt = -1.f;
        NextAttackSlotEligibleAt = FMath::Max(NextAttackSlotEligibleAt, Now + FMath::Max(0.f, AttackSlotYieldSeconds));
    }

    TArray<UARPGAICombatComponent*> Members;
    GatherCoordinatedMeleeAttackers(Target, Members);
    OutGroupSize = FMath::Max(1, Members.Num());
    if (Members.Num() == 0) return true;

    int32 MaxAttackers = MAX_int32;
    for (int32 Index = 0; Index < Members.Num(); ++Index)
    {
        UARPGAICombatComponent* Member = Members[Index];
        if (!Member) continue;
        MaxAttackers = FMath::Min(MaxAttackers, FMath::Max(1, Member->MaxSimultaneousMeleeAttackers));
        if (Member == this) OutSlotIndex = Index;
    }
    MaxAttackers = FMath::Clamp(MaxAttackers == MAX_int32 ? 1 : MaxAttackers, 1, Members.Num());
    OutOrbitDirectionSign = (Members[0] && Members[0]->GetOwner() && (Members[0]->GetOwner()->GetUniqueID() & 1u)) ? -1 : 1;

    if (Members.Num() <= MaxAttackers)
    {
        if (!MyCombat->bIsAttacking && !bHadAttackSlotLastThink)
        {
            bHadAttackSlotLastThink = true;
            AttackSlotGrantedAt = Now;
        }
        return true;
    }

    TArray<UARPGAICombatComponent*> Candidates = Members;
    Candidates.Sort([Now, Target](const UARPGAICombatComponent& A, const UARPGAICombatComponent& B)
    {
        const UARPGCombatComponent* CombatA = A.GetOwner() ? A.GetOwner()->FindComponentByClass<UARPGCombatComponent>() : nullptr;
        const UARPGCombatComponent* CombatB = B.GetOwner() ? B.GetOwner()->FindComponentByClass<UARPGCombatComponent>() : nullptr;
        const bool bAttackingA = CombatA && CombatA->bIsAttacking;
        const bool bAttackingB = CombatB && CombatB->bIsAttacking;
        if (bAttackingA != bAttackingB) return bAttackingA;

        const bool bLeaseA = A.bHadAttackSlotLastThink && A.AttackSlotGrantedAt >= 0.f && Now - A.AttackSlotGrantedAt < FMath::Max(0.5f, A.AttackSlotMaxHoldSeconds);
        const bool bLeaseB = B.bHadAttackSlotLastThink && B.AttackSlotGrantedAt >= 0.f && Now - B.AttackSlotGrantedAt < FMath::Max(0.5f, B.AttackSlotMaxHoldSeconds);
        if (bLeaseA != bLeaseB) return bLeaseA;

        const bool bEligibleA = Now >= A.NextAttackSlotEligibleAt;
        const bool bEligibleB = Now >= B.NextAttackSlotEligibleAt;
        if (bEligibleA != bEligibleB) return bEligibleA;

        if (!FMath::IsNearlyEqual(A.LastAttackCommitAt, B.LastAttackCommitAt)) return A.LastAttackCommitAt < B.LastAttackCommitAt;

        const float DistA = A.GetOwner() ? FVector::DistSquared2D(A.GetOwner()->GetActorLocation(), Target->GetActorLocation()) : TNumericLimits<float>::Max();
        const float DistB = B.GetOwner() ? FVector::DistSquared2D(B.GetOwner()->GetActorLocation(), Target->GetActorLocation()) : TNumericLimits<float>::Max();
        if (!FMath::IsNearlyEqual(DistA, DistB)) return DistA < DistB;

        const uint32 IdA = A.GetOwner() ? A.GetOwner()->GetUniqueID() : 0u;
        const uint32 IdB = B.GetOwner() ? B.GetOwner()->GetUniqueID() : 0u;
        return IdA < IdB;
    });

    bool bHasSlot = false;
    int32 Assigned = 0;
    for (UARPGAICombatComponent* Candidate : Candidates)
    {
        if (!Candidate) continue;
        UARPGCombatComponent* CandidateCombat = Candidate->GetOwner() ? Candidate->GetOwner()->FindComponentByClass<UARPGCombatComponent>() : nullptr;
        const bool bCurrentlyAttacking = CandidateCombat && CandidateCombat->bIsAttacking;
        const bool bLeaseActive = Candidate->bHadAttackSlotLastThink && Candidate->AttackSlotGrantedAt >= 0.f && Now - Candidate->AttackSlotGrantedAt < FMath::Max(0.5f, Candidate->AttackSlotMaxHoldSeconds);
        const bool bEligible = Now >= Candidate->NextAttackSlotEligibleAt;
        if (!bCurrentlyAttacking && !bLeaseActive && !bEligible) continue;
        if (Assigned >= MaxAttackers) break;
        if (Candidate == this) bHasSlot = true;
        ++Assigned;
    }

    if (bHasSlot)
    {
        if (!MyCombat->bIsAttacking && !bHadAttackSlotLastThink)
        {
            bHadAttackSlotLastThink = true;
            AttackSlotGrantedAt = Now;
        }
    }
    else
    {
        bHadAttackSlotLastThink = false;
        AttackSlotGrantedAt = -1.f;
    }
    return bHasSlot;
}

FVector UARPGAICombatComponent::ComputeCombatRingPosition(AActor* Target, int32 SlotIndex, int32 GroupSize, int32 OrbitDirectionSign, float Radius, bool bOrbit) const
{
    if (!Target) return GetOwner() ? GetOwner()->GetActorLocation() : FVector::ZeroVector;
    const int32 SafeGroupSize = FMath::Max(1, GroupSize);
    float AngleDegrees = (360.f * static_cast<float>(FMath::Max(0, SlotIndex))) / static_cast<float>(SafeGroupSize);
    AngleDegrees += Target->GetActorRotation().Yaw;
    if (bOrbit && GetWorld())
        AngleDegrees += GetWorld()->GetTimeSeconds() * FMath::Max(0.f, OrbitDegreesPerSecond) * static_cast<float>(OrbitDirectionSign >= 0 ? 1 : -1);

    const float Radians = FMath::DegreesToRadians(AngleDegrees);
    const FVector Offset(FMath::Cos(Radians) * Radius, FMath::Sin(Radians) * Radius, 0.f);
    return Target->GetActorLocation() + Offset;
}

bool UARPGAICombatComponent::ProjectCombatPositionToNavigation(const FVector& Desired, FVector& OutProjected) const
{
    OutProjected = Desired;
    if (!bProjectCombatPositionsToNavMesh) return true;
    UNavigationSystemV1* Nav = GetWorld() ? FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()) : nullptr;
    if (!Nav) return false;
    FNavLocation Projected;
    if (!Nav->ProjectPointToNavigation(Desired, Projected, CombatNavProjectionExtent)) return false;
    OutProjected = Projected.Location;
    return true;
}

void UARPGAICombatComponent::MoveToCombatPosition(AAIController* AI, const FVector& DesiredGoal, float AcceptanceRadius)
{
    if (!AI || !GetWorld()) return;
    FVector Goal = DesiredGoal;
    if (!ProjectCombatPositionToNavigation(DesiredGoal, Goal)) return;

    const float Now = GetWorld()->GetTimeSeconds();
    const bool bGoalMoved = !bHasActiveMoveLocation || FVector::DistSquared2D(ActiveMoveLocation, Goal) >= FMath::Square(FMath::Max(10.f, CombatMoveGoalRefreshDistance));
    const bool bRefreshDue = Now - LastMoveIssuedAt >= FMath::Max(0.1f, CombatMoveGoalRefreshSeconds);
    if (!bGoalMoved && !bRefreshDue) return;

    AI->MoveToLocation(Goal, FMath::Max(25.f, AcceptanceRadius), true, true, true, false, nullptr, true);
    ActiveMoveTarget.Reset();
    ActiveMoveLocation = Goal;
    bHasActiveMoveLocation = true;
    LastMoveIssuedAt = Now;
}

bool UARPGAICombatComponent::HasLineOfSightTo(AActor* Target) const
{
    if (!GetOwner() || !Target || !GetWorld()) return false;
    FHitResult Hit;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(ARPGAICombatLOS), false, GetOwner());
    const FVector Start = GetOwner()->GetActorLocation() + FVector(0.f, 0.f, 60.f);
    const FVector End = Target->GetActorLocation() + FVector(0.f, 0.f, 40.f);
    if (!GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params)) return true;
    return Hit.GetActor() == Target;
}

void UARPGAICombatComponent::FaceTarget(AActor* Target) const
{
    if (!GetOwner() || !Target) return;
    FVector Direction = Target->GetActorLocation() - GetOwner()->GetActorLocation();
    Direction.Z = 0.f;
    if (!Direction.IsNearlyZero()) GetOwner()->SetActorRotation(Direction.Rotation());
}

void UARPGAICombatComponent::UpdateDefenceAgainstTarget(UARPGCombatComponent* MyCombat, UARPGCombatComponent* EnemyCombat)
{
    if (!bUseAutomaticDefence || !MyCombat || !EnemyCombat || !GetWorld()) return;
    const float Now = GetWorld()->GetTimeSeconds();

    if (MyCombat->bIsBlocking && DefenceReleaseAt > 0.f && Now >= DefenceReleaseAt)
    {
        MyCombat->StopBlocking();
        DefenceReleaseAt = -1.f;
    }

    if (!EnemyCombat->bIsAttacking) return;
    if (ObservedEnemyAttackSerial != EnemyCombat->AttackSerial)
    {
        ObservedEnemyAttackSerial = EnemyCombat->AttackSerial;
        ReactionDueAt = Now + FMath::FRandRange(FMath::Min(ReactionTimeMin, ReactionTimeMax), FMath::Max(ReactionTimeMin, ReactionTimeMax));
    }
    if (ReactedEnemyAttackSerial == EnemyCombat->AttackSerial || ReactionDueAt < 0.f || Now < ReactionDueAt) return;

    const float Remaining = EnemyCombat->GetAttackImpactSecondsRemaining();
    if (Remaining <= 0.01f) { ReactedEnemyAttackSerial = EnemyCombat->AttackSerial; return; }
    ReactedEnemyAttackSerial = EnemyCombat->AttackSerial;

    const bool bCanDodgeNow = MyCombat->CanDodge();
    const bool bCanBlockNow = MyCombat->CanBlock();
    const float DodgeRoll = bCanDodgeNow ? FMath::FRand() : 1.f;
    const float BlockRoll = bCanBlockNow ? FMath::FRand() : 1.f;

    if (bCanDodgeNow && DodgeRoll <= FMath::Clamp(DodgeChance, 0.f, 1.f))
    {
        const EARPGDodgeDirection Direction = FMath::RandBool() ? EARPGDodgeDirection::Left : EARPGDodgeDirection::Right;
        MyCombat->PerformDodge(Direction);
    }
    else if (bCanBlockNow && BlockRoll <= FMath::Clamp(BlockChance, 0.f, 1.f))
    {
        if (MyCombat->StartBlocking()) DefenceReleaseAt = Now + FMath::Max(0.1f, BlockHoldSeconds);
    }
}

bool UARPGAICombatComponent::TryUseAutomaticAbility()
{
    if (!GetOwner() || !GetWorld()) return false;
    UARPGCombatComponent* Combat = GetOwner()->FindComponentByClass<UARPGCombatComponent>();
    if (!Combat) return false;
    const FARPGCombatProfile Profile = Combat->GetCombatProfile();
    if (Profile.AIAutoAbilityTags.Num() == 0 || FMath::FRand() > FMath::Clamp(Profile.AIAbilityUseChance, 0.f, 1.f)) return false;

    const float Now = GetWorld()->GetTimeSeconds();
    if (Now - LastAbilityTryAt < FMath::Max(0.1f, AbilityTryInterval)) return false;
    LastAbilityTryAt = Now;

    UARPGAbilityBridgeComponent* AbilityBridge = GetOwner()->FindComponentByClass<UARPGAbilityBridgeComponent>();
    if (!AbilityBridge) return false;
    const int32 StartIndex = FMath::RandRange(0, Profile.AIAutoAbilityTags.Num() - 1);
    for (int32 Offset = 0; Offset < Profile.AIAutoAbilityTags.Num(); ++Offset)
    {
        const FGameplayTag Tag = Profile.AIAutoAbilityTags[(StartIndex + Offset) % Profile.AIAutoAbilityTags.Num()];
        if (Tag.IsValid() && AbilityBridge->TryActivateAbilityByTag(Tag)) return true;
    }
    return false;
}

void UARPGAICombatComponent::Think()
{
    if (!bEnabled || !GetOwner() || !GetOwner()->HasAuthority() || !HasAIController()) return;
    APawn* Pawn = Cast<APawn>(GetOwner());
    AAIController* AI = Pawn ? Cast<AAIController>(Pawn->GetController()) : nullptr;
    UARPGCombatComponent* MyCombat = GetOwner()->FindComponentByClass<UARPGCombatComponent>();
    if (!AI || !MyCombat || !MyCombat->IsAlive()) return;

    PruneRetaliationMemory();

    if (IsValid(CurrentTarget))
    {
        if (UARPGCombatComponent* TargetCombat = CurrentTarget->FindComponentByClass<UARPGCombatComponent>())
        {
            if (!TargetCombat->IsAlive())
            {
                if (bRestoreOriginalDispositionAfterTargetDeath)
                    ForgetTemporaryAggressionAgainst(CurrentTarget, bClearThreatAgainstDeadTargets);
                ClearCombatTarget(bReturnHomeAfterCombat);
                return;
            }
        }
    }

    // The leash constrains a combat chase, not normal patrol/travel. Spline routes may legitimately
    // carry an NPC far from its original spawn point before combat begins.
    if (IsValid(CurrentTarget) && bUseHomeLeash && FVector::DistSquared(HomeLocation, GetOwner()->GetActorLocation()) > FMath::Square(MaxChaseDistanceFromHome))
    {
        if (bRestoreVitalsOnLeashReset)
            if (UARPGStatsComponent* Stats = GetOwner()->FindComponentByClass<UARPGStatsComponent>()) Stats->RestoreAllVitals();
        if (UARPGThreatComponent* Threat = GetOwner()->FindComponentByClass<UARPGThreatComponent>()) Threat->ClearThreat();
        RetaliationMemory.Reset();
        ClearCombatTarget(true);
        return;
    }

    if (!IsValid(CurrentTarget) || !IsTargetConsideredHostile(CurrentTarget) || FVector::DistSquared(GetOwner()->GetActorLocation(), CurrentTarget->GetActorLocation()) > FMath::Square(LoseTargetRadius))
        SetTargetAuthority(FindBestHostileTarget());

    if (!CurrentTarget)
    {
        ResetGroupCombatRuntime();
        if (const UARPGAISplineComponent* SplineMovement = GetOwner()->FindComponentByClass<UARPGAISplineComponent>())
            if (SplineMovement->IsRouteActive()) return;

        const bool bFarFromHome = FVector::DistSquared(GetOwner()->GetActorLocation(), HomeLocation) > FMath::Square(150.f);
        if (bReturnHomeAfterCombat && bFarFromHome && !bMovingHome)
        {
            AI->MoveToLocation(HomeLocation, 100.f, true, true, true, false, nullptr, true);
            bMovingHome = true;
        }
        else if (!bFarFromHome)
        {
            bMovingHome = false;
        }
        return;
    }

    AI->SetFocus(CurrentTarget, EAIFocusPriority::Gameplay);
    UARPGCombatComponent* EnemyCombat = CurrentTarget->FindComponentByClass<UARPGCombatComponent>();
    UpdateDefenceAgainstTarget(MyCombat, EnemyCombat);
    if (MyCombat->bIsStaggered)
    {
        AI->StopMovement();
        ResetActiveMoveState();
        return;
    }
    if (MyCombat->bIsDodging || MyCombat->bGuardBroken) return;

    const float Distance = FVector::Dist2D(GetOwner()->GetActorLocation(), CurrentTarget->GetActorLocation());
    const float DesiredRange = FMath::Max(50.f, DesiredRangeOverride > 0.f ? DesiredRangeOverride : MyCombat->GetPreferredCombatRange());
    const bool bLOS = !bRequireLineOfSightToAttack || HasLineOfSightTo(CurrentTarget);
    const FARPGCombatProfile Profile = MyCombat->GetCombatProfile();
    const bool bMelee = Profile.BasicAttackType == EARPGBasicAttackType::Melee;

    int32 SlotIndex = 0;
    int32 GroupSize = 1;
    int32 OrbitDirectionSign = 1;
    const bool bHasAttackSlot = !bMelee || EvaluateMeleeAttackSlot(CurrentTarget, MyCombat, SlotIndex, GroupSize, OrbitDirectionSign);

    if (bMelee && bEnableGroupCombatCoordination && GroupSize > 1)
    {
        if (!bHasAttackSlot)
        {
            SetGroupCombatRole(EARPGGroupCombatRole::WaitingOrbit, GroupSize, SlotIndex, false);
            const float MinRadius = FMath::Min(WaitingRingRadiusMin, WaitingRingRadiusMax);
            const float MaxRadius = FMath::Max(WaitingRingRadiusMin, WaitingRingRadiusMax);
            const uint32 StableId = GetOwner()->GetUniqueID();
            const float RadiusAlpha = static_cast<float>(StableId % 101u) / 100.f;
            const float WaitingRadius = FMath::Lerp(FMath::Max(DesiredRange * 1.35f, MinRadius), FMath::Max(DesiredRange * 1.35f, MaxRadius), RadiusAlpha);
            const FVector WaitingGoal = ComputeCombatRingPosition(CurrentTarget, SlotIndex, GroupSize, OrbitDirectionSign, WaitingRadius, bOrbitWhileWaiting);

            if (!MyCombat->bIsAttacking && !MyCombat->bIsBlocking)
                MoveToCombatPosition(AI, WaitingGoal, FMath::Max(MoveAcceptanceRadius, 70.f));

            if (bFaceTargetWhileWaiting) FaceTarget(CurrentTarget);
            if (bAllowAbilitiesWhileWaitingForMeleeSlot && !MyCombat->bIsBlocking) TryUseAutomaticAbility();
            return;
        }

        SetGroupCombatRole(EARPGGroupCombatRole::ActiveAttacker, GroupSize, SlotIndex, true);
        const float AttackRadius = FMath::Max(MinimumAttackApproachRadius, DesiredRange * FMath::Clamp(AttackApproachRadiusMultiplier, 0.35f, 1.f));
        const FVector AttackGoal = ComputeCombatRingPosition(CurrentTarget, SlotIndex, GroupSize, OrbitDirectionSign, AttackRadius, false);
        const bool bNearAttackGoal = FVector::DistSquared2D(GetOwner()->GetActorLocation(), AttackGoal) <= FMath::Square(FMath::Max(25.f, AttackPositionTolerance));

        if (!MyCombat->bIsAttacking && (!bNearAttackGoal || Distance > DesiredRange || !bLOS))
        {
            if (!MyCombat->bIsBlocking) MoveToCombatPosition(AI, AttackGoal, FMath::Max(25.f, AttackPositionTolerance * 0.65f));
            FaceTarget(CurrentTarget);
            return;
        }
    }
    else
    {
        SetGroupCombatRole(EARPGGroupCombatRole::Solo, GroupSize, SlotIndex, true);
    }

    if (Distance > DesiredRange || !bLOS)
    {
        if (!MyCombat->bIsBlocking && ActiveMoveTarget.Get() != CurrentTarget)
        {
            AI->MoveToActor(CurrentTarget, FMath::Max(25.f, MoveAcceptanceRadius), true, true, true, nullptr, true);
            ActiveMoveTarget = CurrentTarget;
            bHasActiveMoveLocation = false;
            LastMoveIssuedAt = GetWorld() ? GetWorld()->GetTimeSeconds() : LastMoveIssuedAt;
        }
        return;
    }

    AI->StopMovement();
    ResetActiveMoveState();
    FaceTarget(CurrentTarget);
    if (MyCombat->bIsBlocking) return;
    if (TryUseAutomaticAbility()) return;
    if (MyCombat->PerformBasicAttack(CurrentTarget))
    {
        if (GetWorld())
        {
            LastAttackCommitAt = GetWorld()->GetTimeSeconds();
            NextAttackSlotEligibleAt = LastAttackCommitAt + FMath::Max(0.f, AttackSlotCooldownAfterAttack);
        }
        bHadAttackSlotLastThink = false;
        AttackSlotGrantedAt = -1.f;
    }
}
