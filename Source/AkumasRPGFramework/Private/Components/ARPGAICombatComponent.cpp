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
    for (auto It = RetaliationMemory.CreateIterator(); It; ++It)
    {
        if (!It.Key().IsValid() || It.Value() <= Now)
            It.RemoveCurrent();
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

    CurrentTarget = NewTarget;
    ActiveMoveTarget.Reset();
    bMovingHome = false;
    ObservedEnemyAttackSerial = INDEX_NONE;
    ReactedEnemyAttackSerial = INDEX_NONE;
    ReactionDueAt = -1.f;
    if (UARPGCombatComponent* Combat = GetOwner()->FindComponentByClass<UARPGCombatComponent>()) Combat->SetCombatTarget(NewTarget);
    SetWandererSuppressed(NewTarget != nullptr);
    OnTargetChanged.Broadcast(NewTarget);
}

void UARPGAICombatComponent::ClearCombatTarget(bool bReturnHome)
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;
    APawn* Pawn = Cast<APawn>(GetOwner());
    SetTargetAuthority(nullptr);
    ActiveMoveTarget.Reset();
    bMovingHome = false;
    if (AAIController* AI = Pawn ? Cast<AAIController>(Pawn->GetController()) : nullptr)
    {
        AI->StopMovement();
        if (UARPGAISplineComponent* SplineMovement = GetOwner()->FindComponentByClass<UARPGAISplineComponent>())
        {
            if (SplineMovement->IsRouteActive())
            {
                SplineMovement->NotifyCombatEnded();
                return;
            }
        }
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

    UARPGCombatComponent* EnemyCombat = CurrentTarget->FindComponentByClass<UARPGCombatComponent>();
    UpdateDefenceAgainstTarget(MyCombat, EnemyCombat);
    if (MyCombat->bIsStaggered)
    {
        AI->StopMovement();
        ActiveMoveTarget.Reset();
        return;
    }
    if (MyCombat->bIsDodging || MyCombat->bGuardBroken) return;

    const float Distance = FVector::Dist2D(GetOwner()->GetActorLocation(), CurrentTarget->GetActorLocation());
    const float DesiredRange = DesiredRangeOverride > 0.f ? DesiredRangeOverride : MyCombat->GetPreferredCombatRange();
    const bool bLOS = !bRequireLineOfSightToAttack || HasLineOfSightTo(CurrentTarget);

    if (Distance > DesiredRange || !bLOS)
    {
        if (!MyCombat->bIsBlocking && ActiveMoveTarget.Get() != CurrentTarget)
        {
            AI->MoveToActor(CurrentTarget, FMath::Max(25.f, MoveAcceptanceRadius), true, true, true, nullptr, true);
            ActiveMoveTarget = CurrentTarget;
        }
        return;
    }

    AI->StopMovement();
    ActiveMoveTarget.Reset();
    FaceTarget(CurrentTarget);
    if (MyCombat->bIsBlocking) return;
    if (TryUseAutomaticAbility()) return;
    MyCombat->PerformBasicAttack(CurrentTarget);
}
