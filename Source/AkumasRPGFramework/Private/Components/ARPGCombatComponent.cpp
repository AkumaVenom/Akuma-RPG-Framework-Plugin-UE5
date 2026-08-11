#include "Components/ARPGCombatComponent.h"

#include "Actors/ARPGCharacter.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Combat/ARPGCombatProjectile.h"
#include "Components/ARPGAICombatComponent.h"
#include "Components/ARPGClassComponent.h"
#include "Components/ARPGEventRouterComponent.h"
#include "Components/ARPGFactionComponent.h"
#include "Components/ARPGInventoryComponent.h"
#include "Components/ARPGLootComponent.h"
#include "Components/ARPGStatsComponent.h"
#include "Components/ARPGThreatComponent.h"
#include "Components/ARPGTargetingComponent.h"
#include "Components/ARPGWoodcuttingComponent.h"
#include "Components/ARPGEquipmentComponent.h"
#include "Data/ARPGClassDefinition.h"
#include "Data/ARPGItemDefinition.h"
#include "AIController.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Net/UnrealNetwork.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponentPoolMethodEnum.h"
#include "NiagaraSystem.h"
#include "Particles/ParticleSystem.h"
#include "Particles/WorldPSCPool.h"
#include "Sound/SoundBase.h"
#include "Components/PrimitiveComponent.h"
#include "TimerManager.h"
#include "Utilities/ARPGAssetLibrary.h"

UARPGCombatComponent::UARPGCombatComponent()
{
    SetIsReplicatedByDefault(true);
    PrimaryComponentTick.bCanEverTick = false;
}

void UARPGCombatComponent::BeginPlay()
{
    Super::BeginPlay();
    if (GetOwner()) RespawnTransform = GetOwner()->GetActorTransform();
    CacheRagdollRestoreState();
    if (UARPGStatsComponent* Stats = GetOwner() ? GetOwner()->FindComponentByClass<UARPGStatsComponent>() : nullptr)
        Stats->OnDeath.AddDynamic(this, &UARPGCombatComponent::HandleStatsDeath);
}

FARPGCombatProfile UARPGCombatComponent::GetCombatProfile() const
{
    if (bUseClassCombatProfile && GetOwner())
    {
        if (const UARPGClassComponent* ClassComp = GetOwner()->FindComponentByClass<UARPGClassComponent>())
            if (ClassComp->ClassDefinition) return ClassComp->ClassDefinition->CombatProfile;
    }
    return OverrideCombatProfile;
}

int32 UARPGCombatComponent::GetComboCount() const
{
    const FARPGCombatProfile Profile = GetCombatProfile();
    if (Profile.DetailedComboSteps.Num() > 0) return Profile.DetailedComboSteps.Num();

    if (bUseClassCombatProfile && GetOwner())
    {
        if (const UARPGClassComponent* ClassComp = GetOwner()->FindComponentByClass<UARPGClassComponent>())
        {
            if (ClassComp->ClassDefinition)
            {
                const FARPGCombatMontageSet& Set = ClassComp->ClassDefinition->AnimationSet;
                if (Profile.BasicAttackType == EARPGBasicAttackType::Magic) return Set.MagicCasts.Num();
                if (Profile.BasicAttackType == EARPGBasicAttackType::Ranged) return Set.RangedAttacks.Num();
                return Set.MeleeAttacks.Num();
            }
        }
    }

    if (Profile.BasicAttackType == EARPGBasicAttackType::Magic) return Montages.MagicCasts.Num();
    if (Profile.BasicAttackType == EARPGBasicAttackType::Ranged) return Montages.RangedAttacks.Num();
    return Montages.MeleeAttacks.Num();
}

UAnimMontage* UARPGCombatComponent::GetFallbackMontageForIndex(int32 ComboIndex) const
{
    const FARPGCombatProfile Profile = GetCombatProfile();
    const TArray<TSoftObjectPtr<UAnimMontage>>* Source = nullptr;

    if (bUseClassCombatProfile && GetOwner())
    {
        if (const UARPGClassComponent* ClassComp = GetOwner()->FindComponentByClass<UARPGClassComponent>())
        {
            if (ClassComp->ClassDefinition)
            {
                const FARPGCombatMontageSet& Set = ClassComp->ClassDefinition->AnimationSet;
                Source = Profile.BasicAttackType == EARPGBasicAttackType::Magic ? &Set.MagicCasts :
                         (Profile.BasicAttackType == EARPGBasicAttackType::Ranged ? &Set.RangedAttacks : &Set.MeleeAttacks);
            }
        }
    }

    if (!Source)
        Source = Profile.BasicAttackType == EARPGBasicAttackType::Magic ? &Montages.MagicCasts :
                 (Profile.BasicAttackType == EARPGBasicAttackType::Ranged ? &Montages.RangedAttacks : &Montages.MeleeAttacks);

    if (!Source || Source->Num() == 0) return nullptr;
    const int32 Index = FMath::Abs(ComboIndex) % Source->Num();
    return (*Source)[Index].LoadSynchronous();
}

bool UARPGCombatComponent::BuildAttackStep(int32 ComboIndex, FARPGAttackStepDefinition& OutStep) const
{
    const FARPGCombatProfile Profile = GetCombatProfile();
    if (Profile.DetailedComboSteps.Num() > 0)
        OutStep = Profile.DetailedComboSteps[FMath::Abs(ComboIndex) % Profile.DetailedComboSteps.Num()];
    else
        OutStep = FARPGAttackStepDefinition();

    if (OutStep.Montage.IsNull())
        OutStep.Montage = GetFallbackMontageForIndex(ComboIndex);

    OutStep.RecoveryTime = FMath::Max(0.05f, OutStep.RecoveryTime);
    OutStep.ImpactDelay = FMath::Max(0.f, OutStep.ImpactDelay);
    OutStep.ComboQueueOpenTime = FMath::Max(0.f, OutStep.ComboQueueOpenTime);
    OutStep.ComboQueueCloseTime = FMath::Max(OutStep.ComboQueueOpenTime, OutStep.ComboQueueCloseTime);
    return true;
}

bool UARPGCombatComponent::CanPerformBasicAttack() const
{
    if (!GetOwner() || LifeState != EARPGLifeState::Alive || bIsDodging || bGuardBroken || bIsBlocking || bIsStaggered) return false;
    return true;
}

bool UARPGCombatComponent::CanDodge() const
{
    if (!GetOwner() || LifeState != EARPGLifeState::Alive || bIsDodging || bGuardBroken || bIsStaggered) return false;
    const FARPGDodgeSettings Dodge = GetCombatProfile().Dodge;
    if (!Dodge.bEnabled) return false;
    if (bIsAttacking && !Dodge.bDodgeCancelsAttacks) return false;
    const UWorld* World = GetWorld();
    if (World && World->GetTimeSeconds() - LastDodgeAt < Dodge.Cooldown) return false;
    const UARPGStatsComponent* Stats = GetOwner()->FindComponentByClass<UARPGStatsComponent>();
    return !Stats || Stats->Stamina >= Dodge.StaminaCost;
}

bool UARPGCombatComponent::CanBlock() const
{
    if (!GetOwner() || LifeState != EARPGLifeState::Alive || bIsDodging || bIsAttacking || bGuardBroken || bIsStaggered) return false;
    const FARPGBlockSettings Block = GetCombatProfile().Block;
    if (!Block.bEnabled || !HasRequiredBlockEquipment(Block)) return false;
    const UARPGStatsComponent* Stats = GetOwner()->FindComponentByClass<UARPGStatsComponent>();
    return !Stats || Stats->Stamina >= Block.MinimumStaminaToBlock;
}

bool UARPGCombatComponent::PerformBasicAttack(AActor* OptionalTarget)
{
    if (!GetOwner()) return false;
    if (!GetOwner()->HasAuthority())
    {
        ServerPerformBasicAttack(OptionalTarget);
        return true;
    }
    return StartAttackAuthority(OptionalTarget);
}

void UARPGCombatComponent::ServerPerformBasicAttack_Implementation(AActor* OptionalTarget)
{
    StartAttackAuthority(OptionalTarget);
}

bool UARPGCombatComponent::StartAttackAuthority(AActor* OptionalTarget)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || !CanPerformBasicAttack()) return false;
    const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;

    if (bIsAttacking)
    {
        if (Now >= ComboQueueOpenAt && Now <= ComboQueueCloseAt)
        {
            bComboQueued = true;
            return true;
        }
        return false;
    }

    // Context-sensitive gathering: with a valid equipped axe, Basic Attack becomes one Woodcutting chop
    // when the intended target is an ARPGTree. A real combat/lock-on target retains normal combat priority.
    if (UARPGWoodcuttingComponent* Woodcutting = GetOwner()->FindComponentByClass<UARPGWoodcuttingComponent>())
    {
        bool bHandledAsWoodcutting = false;
        const bool bWoodcuttingStarted = Woodcutting->TryHandleBasicAttackAsWoodcutting(OptionalTarget, bHandledAsWoodcutting);
        if (bHandledAsWoodcutting) return bWoodcuttingStarted;
    }

    if (OptionalTarget && OptionalTarget != GetOwner())
        SetCombatTarget(OptionalTarget);

    if (Now > ComboExpiresAt)
        CurrentComboIndex = 0;

    const int32 Count = FMath::Max(1, GetComboCount());
    CurrentComboIndex = FMath::Clamp(CurrentComboIndex, 0, Count - 1);
    if (!BuildAttackStep(CurrentComboIndex, ActiveAttackStep)) return false;

    UARPGStatsComponent* Stats = GetOwner()->FindComponentByClass<UARPGStatsComponent>();
    if (Stats)
    {
        if (ActiveAttackStep.StaminaCost > 0.f && Stats->Stamina < ActiveAttackStep.StaminaCost) return false;
        if (ActiveAttackStep.ManaCost > 0.f && Stats->Mana < ActiveAttackStep.ManaCost) return false;
        if (ActiveAttackStep.StaminaCost > 0.f) Stats->SpendStamina(ActiveAttackStep.StaminaCost);
        if (ActiveAttackStep.ManaCost > 0.f) Stats->SpendMana(ActiveAttackStep.ManaCost);
    }

    const FARPGCombatProfile Profile = GetCombatProfile();
    if (Profile.bAutoFaceCombatTarget && IsValid(CombatTarget))
    {
        if (UARPGTargetingComponent* Targeting = GetOwner()->FindComponentByClass<UARPGTargetingComponent>())
            Targeting->RequestAttackFacing();
        else
        {
            FVector ToTarget = CombatTarget->GetActorLocation() - GetOwner()->GetActorLocation();
            ToTarget.Z = 0.f;
            if (!ToTarget.IsNearlyZero()) GetOwner()->SetActorRotation(ToTarget.Rotation());
        }
    }

    bIsAttacking = true;
    SetLooseCombatTag(TEXT("Combat.State.Attacking"), true);
    bComboQueued = false;
    bImpactResolved = false;
    ++AttackSerial;
    HitActorsThisSwing.Reset();
    AttackStartedAt = Now;
    AttackImpactAt = Now + ActiveAttackStep.ImpactDelay;
    ComboQueueOpenAt = Now + ActiveAttackStep.ComboQueueOpenTime;
    ComboQueueCloseAt = Now + ActiveAttackStep.ComboQueueCloseTime;

    UAnimMontage* Montage = ActiveAttackStep.Montage.LoadSynchronous();
    if (Montage) MulticastPlayMontage(Montage, 1.f);
    OnAttackStarted.Broadcast(CurrentComboIndex, Montage);
    const EARPGCombatFeedbackCue AttackCue =
        Profile.BasicAttackType == EARPGBasicAttackType::Magic ? EARPGCombatFeedbackCue::MagicCast :
        (Profile.BasicAttackType == EARPGBasicAttackType::Ranged ? EARPGCombatFeedbackCue::RangedAttack : EARPGCombatFeedbackCue::MeleeSwing);
    MulticastPlayCombatCue(AttackCue, GetOwner()->GetActorLocation(), GetOwner()->GetActorForwardVector());

    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(AttackImpactTimer);
        GetWorld()->GetTimerManager().ClearTimer(AttackFinishTimer);
        if (Profile.bAutomaticTimedImpact)
            GetWorld()->GetTimerManager().SetTimer(AttackImpactTimer, this, &UARPGCombatComponent::ResolveAttackImpactAuthority, FMath::Max(0.001f, ActiveAttackStep.ImpactDelay), false);
        const float FinishDelay = FMath::Max(ActiveAttackStep.RecoveryTime, ActiveAttackStep.ImpactDelay + 0.05f);
        GetWorld()->GetTimerManager().SetTimer(AttackFinishTimer, this, &UARPGCombatComponent::FinishAttackAuthority, FinishDelay, false);
    }
    return true;
}

void UARPGCombatComponent::NotifyAttackImpact()
{
    if (!GetOwner()) return;
    if (!GetOwner()->HasAuthority()) { ServerNotifyAttackImpact(); return; }
    ResolveAttackImpactAuthority();
}

void UARPGCombatComponent::ServerNotifyAttackImpact_Implementation()
{
    ResolveAttackImpactAuthority();
}

float UARPGCombatComponent::CalculateAttackDamage(const FARPGAttackStepDefinition& Step, bool& bOutCritical) const
{
    const FARPGCombatProfile Profile = GetCombatProfile();
    const UARPGStatsComponent* Stats = GetOwner() ? GetOwner()->FindComponentByClass<UARPGStatsComponent>() : nullptr;
    float Power = 0.f;
    if (Stats)
        Power = Profile.BasicAttackType == EARPGBasicAttackType::Magic ? Stats->SpellPower * Profile.SpellPowerScale : Stats->AttackPower * Profile.AttackPowerScale;

    float Damage = (Profile.BaseDamage + Power) * FMath::Max(0.f, Step.DamageMultiplier);
    const float Variance = FMath::Clamp(Profile.DamageVariance, 0.f, 1.f);
    Damage *= FMath::FRandRange(1.f - Variance, 1.f + Variance);
    bOutCritical = FMath::FRand() <= FMath::Clamp(Profile.CriticalChance, 0.f, 1.f);
    if (bOutCritical) Damage *= FMath::Max(1.f, Profile.CriticalMultiplier);
    return FMath::Max(0.f, Damage);
}

void UARPGCombatComponent::ResolveAttackImpactAuthority()
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || !bIsAttacking || bImpactResolved) return;
    bImpactResolved = true;
    bool bCritical = false;
    const float RawDamage = CalculateAttackDamage(ActiveAttackStep, bCritical);
    if (GetCombatProfile().BasicAttackType == EARPGBasicAttackType::Melee)
        ResolveMeleeAttack(ActiveAttackStep, RawDamage, bCritical);
    else
        ResolveRangedOrMagicAttack(ActiveAttackStep, RawDamage, bCritical);
}

void UARPGCombatComponent::ResolveMeleeAttack(const FARPGAttackStepDefinition& Step, float RawDamage, bool bCritical)
{
    if (!GetWorld() || !GetOwner()) return;
    const FARPGCombatProfile Profile = GetCombatProfile();
    const float Range = Step.RangeOverride > 0.f ? Step.RangeOverride : Profile.DefaultRange;
    const float Radius = Step.TraceRadiusOverride > 0.f ? Step.TraceRadiusOverride : Profile.DefaultTraceRadius;

    FVector Start = GetOwner()->GetActorLocation() + FVector(0.f, 0.f, 60.f);
    FVector AttackDirection = GetOwner()->GetActorForwardVector();
    if (Profile.bAutoFaceCombatTarget && IsValid(CombatTarget))
    {
        const FVector ToTarget = CombatTarget->GetActorLocation() - Start;
        if (!ToTarget.IsNearlyZero()) AttackDirection = ToTarget.GetSafeNormal();
    }
    FVector End = Start + AttackDirection * Range;
    if (const ACharacter* Character = Cast<ACharacter>(GetOwner()))
    {
        if (USkeletalMeshComponent* Mesh = Character->GetMesh())
        {
            if (!Step.TraceStartSocket.IsNone() && !Step.TraceEndSocket.IsNone() && Mesh->DoesSocketExist(Step.TraceStartSocket) && Mesh->DoesSocketExist(Step.TraceEndSocket))
            {
                Start = Mesh->GetSocketLocation(Step.TraceStartSocket);
                End = Mesh->GetSocketLocation(Step.TraceEndSocket);
            }
        }
    }

    FCollisionObjectQueryParams Objects;
    Objects.AddObjectTypesToQuery(ECC_Pawn);
    FCollisionQueryParams Params(SCENE_QUERY_STAT(ARPGMeleeAttack), false, GetOwner());
    TArray<FHitResult> Hits;
    GetWorld()->SweepMultiByObjectType(Hits, Start, End, FQuat::Identity, Objects, FCollisionShape::MakeSphere(FMath::Max(1.f, Radius)), Params);

    Hits.Sort([&](const FHitResult& A, const FHitResult& B)
    {
        return FVector::DistSquared(Start, A.ImpactPoint) < FVector::DistSquared(Start, B.ImpactPoint);
    });

    int32 HitCount = 0;
    for (const FHitResult& Hit : Hits)
    {
        AActor* Target = Hit.GetActor();
        if (!Target || Target == GetOwner() || HitActorsThisSwing.Contains(TWeakObjectPtr<AActor>(Target)) || !CanDamageActor(Target)) continue;
        HitActorsThisSwing.Add(TWeakObjectPtr<AActor>(Target));
        ResolveHitOnActor(Target, Step, RawDamage, bCritical, Hit.ImpactPoint);
        if (++HitCount >= FMath::Max(1, Profile.MaxMeleeTargets)) break;
    }
}

void UARPGCombatComponent::ResolveRangedOrMagicAttack(const FARPGAttackStepDefinition& Step, float RawDamage, bool bCritical)
{
    if (!GetWorld() || !GetOwner()) return;
    const FARPGCombatProfile Profile = GetCombatProfile();
    const float MaxRange = Profile.BasicAttackType == EARPGBasicAttackType::Magic ? Profile.MagicMaxRange : Profile.RangedMaxRange;

    FVector Start = GetOwner()->GetActorLocation() + FVector(0.f, 0.f, 60.f);
    if (const ACharacter* Character = Cast<ACharacter>(GetOwner()))
        if (USkeletalMeshComponent* Mesh = Character->GetMesh())
            if (!Profile.ProjectileSpawnSocket.IsNone() && Mesh->DoesSocketExist(Profile.ProjectileSpawnSocket))
                Start = Mesh->GetSocketLocation(Profile.ProjectileSpawnSocket);

    AActor* DesiredTarget = IsValid(CombatTarget) && FVector::DistSquared(Start, CombatTarget->GetActorLocation()) <= FMath::Square(MaxRange) ? CombatTarget.Get() : nullptr;
    FVector End = DesiredTarget ? DesiredTarget->GetActorLocation() : Start + GetOwner()->GetActorForwardVector() * MaxRange;

    FHitResult OcclusionHit;
    FCollisionQueryParams VisibilityParams(SCENE_QUERY_STAT(ARPGRangedLOS), false, GetOwner());
    if (GetWorld()->LineTraceSingleByChannel(OcclusionHit, Start, End, ECC_Visibility, VisibilityParams))
    {
        if (DesiredTarget && OcclusionHit.GetActor() && OcclusionHit.GetActor() != DesiredTarget)
            DesiredTarget = nullptr;
        if (!DesiredTarget) End = OcclusionHit.ImpactPoint;
    }

    if (Profile.ProjectileClass)
    {
        if (DesiredTarget && !CanDamageActor(DesiredTarget)) return;
        const FRotator Rotation = (End - Start).Rotation();
        FActorSpawnParameters Params;
        Params.Owner = GetOwner();
        Params.Instigator = Cast<APawn>(GetOwner());
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        if (AARPGCombatProjectile* Projectile = GetWorld()->SpawnActor<AARPGCombatProjectile>(Profile.ProjectileClass, Start, Rotation, Params))
            Projectile->InitializeCombatProjectile(GetOwner(), DesiredTarget, RawDamage, Profile.BasicAttackType, Step.bCanBeBlocked && !Step.bUnblockable, Step.bCanBeParried && !Step.bUnblockable, bCritical, Profile.ProjectileSpeed, Profile.bProjectileHoming);
        return;
    }

    if (DesiredTarget && CanDamageActor(DesiredTarget))
    {
        ResolveHitOnActor(DesiredTarget, Step, RawDamage, bCritical, DesiredTarget->GetActorLocation());
        return;
    }

    FCollisionObjectQueryParams Objects;
    Objects.AddObjectTypesToQuery(ECC_Pawn);
    FCollisionQueryParams Params(SCENE_QUERY_STAT(ARPGRangedAttack), false, GetOwner());
    TArray<FHitResult> Hits;
    GetWorld()->SweepMultiByObjectType(Hits, Start, End, FQuat::Identity, Objects, FCollisionShape::MakeSphere(12.f), Params);
    Hits.Sort([&](const FHitResult& A, const FHitResult& B)
    {
        return FVector::DistSquared(Start, A.ImpactPoint) < FVector::DistSquared(Start, B.ImpactPoint);
    });
    for (const FHitResult& Hit : Hits)
    {
        if (AActor* Target = Hit.GetActor())
        {
            if (CanDamageActor(Target))
            {
                ResolveHitOnActor(Target, Step, RawDamage, bCritical, Hit.ImpactPoint);
                break;
            }
        }
    }
}

void UARPGCombatComponent::ResolveHitOnActor(AActor* Target, const FARPGAttackStepDefinition& Step, float RawDamage, bool bCritical, const FVector& HitLocation)
{
    if (!Target || !CanDamageActor(Target)) return;
    FARPGCombatHitInfo Info;
    if (UARPGCombatComponent* TargetCombat = Target->FindComponentByClass<UARPGCombatComponent>())
        Info = TargetCombat->ReceiveCombatHit(GetOwner(), RawDamage, GetCombatProfile().BasicAttackType, Step.bCanBeBlocked && !Step.bUnblockable, Step.bCanBeParried && !Step.bUnblockable, HitLocation, bCritical);
    else if (UARPGStatsComponent* TargetStats = Target->FindComponentByClass<UARPGStatsComponent>())
    {
        Info.Attacker = GetOwner();
        Info.Target = Target;
        Info.AttackType = GetCombatProfile().BasicAttackType;
        Info.RawDamage = RawDamage;
        Info.AppliedDamage = RawDamage;
        Info.bCritical = bCritical;
        Info.Result = bCritical ? EARPGCombatHitResult::Critical : EARPGCombatHitResult::Hit;
        Info.HitLocation = HitLocation;
        TargetStats->ApplyDamage(RawDamage);
        MulticastPlayCombatCue(bCritical ? EARPGCombatFeedbackCue::CriticalHit : EARPGCombatFeedbackCue::Hit,
                               HitLocation, (Target->GetActorLocation() - GetOwner()->GetActorLocation()).GetSafeNormal());
    }
    OnCombatHitDealt.Broadcast(Info);
}

FARPGCombatHitInfo UARPGCombatComponent::ReceiveCombatHit(AActor* Attacker, float RawDamage, EARPGBasicAttackType AttackType, bool bCanBeBlocked, bool bCanBeParried, FVector HitLocation, bool bCritical)
{
    FARPGCombatHitInfo Info;
    Info.Attacker = Attacker;
    Info.Target = GetOwner();
    Info.AttackType = AttackType;
    Info.RawDamage = FMath::Max(0.f, RawDamage);
    Info.HitLocation = HitLocation;
    Info.bCritical = bCritical;
    Info.Result = EARPGCombatHitResult::Miss;

    if (!GetOwner() || !GetOwner()->HasAuthority() || LifeState != EARPGLifeState::Alive || !Attacker)
    {
        Info.Result = EARPGCombatHitResult::Immune;
        return Info;
    }

    if (const UARPGCombatComponent* AttackerCombat = Attacker->FindComponentByClass<UARPGCombatComponent>())
    {
        if (!AttackerCombat->CanDamageActor(GetOwner()))
        {
            Info.Result = EARPGCombatHitResult::Friendly;
            OnCombatHitReceived.Broadcast(Info);
            return Info;
        }
    }

    LastDamageInstigator = Attacker;
    LastReceivedHitLocation = HitLocation.IsNearlyZero() ? GetOwner()->GetActorLocation() : HitLocation;
    LastReceivedHitDirection = (GetOwner()->GetActorLocation() - Attacker->GetActorLocation()).GetSafeNormal();
    if (LastReceivedHitDirection.IsNearlyZero()) LastReceivedHitDirection = -Attacker->GetActorForwardVector().GetSafeNormal();

    if (IsDodgeInvulnerable())
    {
        Info.Result = EARPGCombatHitResult::Dodged;
        OnCombatHitReceived.Broadcast(Info);
        return Info;
    }

    float Damage = Info.RawDamage;
    const FARPGCombatProfile Profile = GetCombatProfile();
    UARPGStatsComponent* Stats = GetOwner()->FindComponentByClass<UARPGStatsComponent>();

    const bool bFacing = IsBlockFacingAttacker(Attacker, Profile.Block.BlockHalfAngleDegrees);
    const bool bTypeBlockable = AttackType == EARPGBasicAttackType::Melee ? Profile.Block.bCanBlockMelee :
                                (AttackType == EARPGBasicAttackType::Ranged ? Profile.Block.bCanBlockRanged : Profile.Block.bCanBlockMagic);

    if (bIsBlocking && bCanBeBlocked && bTypeBlockable && bFacing && !bGuardBroken)
    {
        const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
        if (bCanBeParried && Profile.Block.PerfectBlockWindow > 0.f && Now - BlockStartedAt <= Profile.Block.PerfectBlockWindow)
        {
            Info.Result = EARPGCombatHitResult::Parried;
            if (UAnimMontage* Parry = Profile.Block.ParryMontage.LoadSynchronous()) MulticastPlayMontage(Parry, 1.f);
            if (UARPGCombatComponent* AttackerCombat = Attacker->FindComponentByClass<UARPGCombatComponent>())
                AttackerCombat->BeginGuardBreakAuthority(FMath::Max(0.35f, Profile.Block.GuardBreakDuration * 0.5f));
            MulticastPlayCombatCue(EARPGCombatFeedbackCue::Parry, LastReceivedHitLocation, LastReceivedHitDirection);
            OnCombatHitReceived.Broadcast(Info);
            return Info;
        }

        const float StaminaCost = Profile.Block.StaminaCostPerHit + Damage * Profile.Block.StaminaCostPerDamage;
        if (Stats && Stats->Stamina < StaminaCost)
        {
            BeginGuardBreakAuthority(Profile.Block.GuardBreakDuration);
        }
        else
        {
            if (Stats && StaminaCost > 0.f) Stats->SpendStamina(StaminaCost);
            const float Reduction = AttackType == EARPGBasicAttackType::Melee ? Profile.Block.PhysicalDamageReduction :
                                    (AttackType == EARPGBasicAttackType::Ranged ? Profile.Block.RangedDamageReduction : Profile.Block.MagicDamageReduction);
            Damage *= 1.f - FMath::Clamp(Reduction, 0.f, 1.f);
            Info.Result = EARPGCombatHitResult::Blocked;
            if (UAnimMontage* BlockHit = Profile.Block.BlockHitMontage.LoadSynchronous()) MulticastPlayMontage(BlockHit, 1.f);
        }
    }

    if (Stats && AttackType != EARPGBasicAttackType::Magic)
    {
        const float Armor = FMath::Max(0.f, Stats->Armor);
        Damage *= 100.f / (100.f + Armor);
    }

    Damage = FMath::Max(0.f, Damage);
    Info.AppliedDamage = Damage;
    if (Info.Result != EARPGCombatHitResult::Blocked)
        Info.Result = bCritical ? EARPGCombatHitResult::Critical : EARPGCombatHitResult::Hit;

    if (Stats && Damage > 0.f) Stats->ApplyDamage(Damage);

    if (UARPGThreatComponent* Threat = GetOwner()->FindComponentByClass<UARPGThreatComponent>())
        Threat->AddThreat(Attacker, FMath::Max(1.f, Damage));

    if (Damage > 0.f && LifeState == EARPGLifeState::Alive && Info.Result != EARPGCombatHitResult::Blocked)
        TryApplyCriticalStaggerAuthority(Attacker, Info);

    // Light hit reactions are cosmetic and must not silently cancel an attack montage while the
    // authoritative attack timers continue running. Real interruption is owned by stagger, parry,
    // guard break, dodge/death, etc. This keeps visible animation and gameplay state synchronized.
    const bool bCanPlayLightHitReact = Damage > 0.f && LifeState == EARPGLifeState::Alive &&
                                       !Info.bStaggered && Info.Result != EARPGCombatHitResult::Blocked &&
                                       !bIsAttacking;
    if (bCanPlayLightHitReact)
        if (UAnimMontage* HitReact = GetHitReactMontage()) MulticastPlayMontage(HitReact, 1.f);

    if (Info.Result == EARPGCombatHitResult::Blocked)
    {
        MulticastPlayCombatCue(EARPGCombatFeedbackCue::BlockHit, LastReceivedHitLocation, LastReceivedHitDirection);
    }
    else if (Damage > 0.f)
    {
        // Normal/critical impact feedback belongs to the attacker's combat profile so a class/weapon
        // can define its hit feel once instead of every victim class duplicating the same blood/impact set.
        if (UARPGCombatComponent* AttackerCombat = Attacker->FindComponentByClass<UARPGCombatComponent>())
        {
            AttackerCombat->MulticastPlayCombatCue(bCritical ? EARPGCombatFeedbackCue::CriticalHit : EARPGCombatFeedbackCue::Hit,
                                                   LastReceivedHitLocation, LastReceivedHitDirection);
        }
        else
        {
            MulticastPlayCombatCue(bCritical ? EARPGCombatFeedbackCue::CriticalHit : EARPGCombatFeedbackCue::Hit,
                                   LastReceivedHitLocation, LastReceivedHitDirection);
        }
    }

    OnCombatHitReceived.Broadcast(Info);
    return Info;
}

bool UARPGCombatComponent::CanDamageActor(AActor* Target) const
{
    if (!GetOwner() || !Target || Target == GetOwner()) return false;
    if (const UARPGCombatComponent* TargetCombat = Target->FindComponentByClass<UARPGCombatComponent>())
        if (!TargetCombat->IsAlive()) return false;

    const FARPGCombatProfile Profile = GetCombatProfile();
    const UARPGFactionComponent* Mine = GetOwner()->FindComponentByClass<UARPGFactionComponent>();
    const UARPGFactionComponent* Theirs = Target->FindComponentByClass<UARPGFactionComponent>();
    if (!Mine || !Theirs) return true;

    const int32 Relationship = Mine->GetBaseRelationshipTo(Theirs);

    // AI retaliation/assist can intentionally create temporary hostility against an otherwise neutral
    // or unresolved faction target. This keeps a passive creature capable of actually fighting back
    // even when its class profile normally disallows neutral damage. Explicit friendship is still
    // protected unless the AI Combat component was configured to retaliate against friendly attackers.
    if (const UARPGAICombatComponent* AICombat = GetOwner()->FindComponentByClass<UARPGAICombatComponent>())
    {
        if (AICombat->bEnabled && AICombat->IsTargetConsideredHostile(Target))
            return true;
    }

    if (Relationship > 0) return Profile.bAllowFriendlyFire;
    if (Relationship == 0) return Profile.bAllowNeutralDamage;
    return true;
}

bool UARPGCombatComponent::PerformDodge(EARPGDodgeDirection Direction)
{
    if (!GetOwner()) return false;
    if (!GetOwner()->HasAuthority()) { ServerPerformDodge(Direction); return true; }
    return PerformDodgeAuthority(Direction);
}

void UARPGCombatComponent::ServerPerformDodge_Implementation(EARPGDodgeDirection Direction)
{
    PerformDodgeAuthority(Direction);
}

FVector UARPGCombatComponent::ResolveDodgeWorldDirection(EARPGDodgeDirection& InOutDirection) const
{
    if (!GetOwner()) return FVector::ForwardVector;
    const FVector Forward = GetOwner()->GetActorForwardVector().GetSafeNormal2D();
    const FVector Right = GetOwner()->GetActorRightVector().GetSafeNormal2D();

    if (InOutDirection == EARPGDodgeDirection::Auto)
    {
        const FVector Velocity = GetOwner()->GetVelocity().GetSafeNormal2D();
        if (Velocity.IsNearlyZero()) InOutDirection = EARPGDodgeDirection::Backward;
        else
        {
            const float F = FVector::DotProduct(Velocity, Forward);
            const float R = FVector::DotProduct(Velocity, Right);
            if (FMath::Abs(F) >= FMath::Abs(R)) InOutDirection = F >= 0.f ? EARPGDodgeDirection::Forward : EARPGDodgeDirection::Backward;
            else InOutDirection = R >= 0.f ? EARPGDodgeDirection::Right : EARPGDodgeDirection::Left;
        }
    }

    switch (InOutDirection)
    {
        case EARPGDodgeDirection::Forward: return Forward;
        case EARPGDodgeDirection::Left: return -Right;
        case EARPGDodgeDirection::Right: return Right;
        case EARPGDodgeDirection::Backward:
        default: return -Forward;
    }
}

UAnimMontage* UARPGCombatComponent::ResolveDodgeMontage(EARPGDodgeDirection Direction, const FARPGDodgeSettings& DodgeSettings) const
{
    switch (Direction)
    {
        case EARPGDodgeDirection::Forward: return DodgeSettings.ForwardMontage.LoadSynchronous();
        case EARPGDodgeDirection::Left: return DodgeSettings.LeftMontage.LoadSynchronous();
        case EARPGDodgeDirection::Right: return DodgeSettings.RightMontage.LoadSynchronous();
        case EARPGDodgeDirection::Backward: return DodgeSettings.BackwardMontage.LoadSynchronous();
        default: return nullptr;
    }
}

bool UARPGCombatComponent::PerformDodgeAuthority(EARPGDodgeDirection Direction)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || !CanDodge()) return false;
    const FARPGDodgeSettings Dodge = GetCombatProfile().Dodge;
    UARPGStatsComponent* Stats = GetOwner()->FindComponentByClass<UARPGStatsComponent>();
    if (Stats && Dodge.StaminaCost > 0.f && !Stats->SpendStamina(Dodge.StaminaCost)) return false;

    if (bIsBlocking) StopBlockingAuthority(false);
    if (bIsAttacking && Dodge.bDodgeCancelsAttacks)
    {
        bIsAttacking = false;
        SetLooseCombatTag(TEXT("Combat.State.Attacking"), false);
        bComboQueued = false;
        if (GetWorld())
        {
            GetWorld()->GetTimerManager().ClearTimer(AttackImpactTimer);
            GetWorld()->GetTimerManager().ClearTimer(AttackFinishTimer);
        }
    }

    EARPGDodgeDirection ResolvedDirection = Direction;
    const FVector WorldDirection = ResolveDodgeWorldDirection(ResolvedDirection);
    bIsDodging = true;
    SetLooseCombatTag(TEXT("Combat.State.Dodging"), true);
    DodgeStartedAt = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
    LastDodgeAt = DodgeStartedAt;

    if (UAnimMontage* Montage = ResolveDodgeMontage(ResolvedDirection, Dodge)) MulticastPlayMontage(Montage, 1.f);
    if (!Dodge.bUseRootMotionOnly)
        if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
            Character->LaunchCharacter(WorldDirection * (Dodge.Distance / FMath::Max(0.05f, Dodge.Duration)), true, false);

    OnDodgeStarted.Broadcast(ResolvedDirection);
    MulticastPlayCombatCue(EARPGCombatFeedbackCue::Dodge, GetOwner()->GetActorLocation(), WorldDirection);
    if (GetWorld()) GetWorld()->GetTimerManager().SetTimer(DodgeFinishTimer, this, &UARPGCombatComponent::FinishDodgeAuthority, FMath::Max(0.05f, Dodge.Duration), false);
    return true;
}

void UARPGCombatComponent::FinishDodgeAuthority()
{
    if (GetOwner() && GetOwner()->HasAuthority())
    {
        bIsDodging = false;
        SetLooseCombatTag(TEXT("Combat.State.Dodging"), false);
    }
}

bool UARPGCombatComponent::IsDodgeInvulnerable() const
{
    if (!bIsDodging || !GetWorld()) return false;
    const FARPGDodgeSettings Dodge = GetCombatProfile().Dodge;
    const float Elapsed = GetWorld()->GetTimeSeconds() - DodgeStartedAt;
    return Elapsed >= FMath::Max(0.f, Dodge.InvulnerabilityStart) && Elapsed <= FMath::Max(Dodge.InvulnerabilityStart, Dodge.InvulnerabilityEnd);
}

bool UARPGCombatComponent::StartBlocking()
{
    if (!GetOwner()) return false;
    if (!GetOwner()->HasAuthority()) { ServerStartBlocking(); return true; }
    return StartBlockingAuthority();
}

void UARPGCombatComponent::ServerStartBlocking_Implementation()
{
    StartBlockingAuthority();
}

bool UARPGCombatComponent::StartBlockingAuthority()
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return false;
    if (bIsBlocking) return true;
    if (!CanBlock()) return false;

    const FARPGBlockSettings Block = GetCombatProfile().Block;
    bIsBlocking = true;
    SetLooseCombatTag(TEXT("Combat.State.Blocking"), true);
    BlockStartedAt = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;

    if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
    {
        if (UCharacterMovementComponent* Move = Character->GetCharacterMovement())
        {
            CachedPreBlockWalkSpeed = Move->MaxWalkSpeed;
            Move->MaxWalkSpeed = CachedPreBlockWalkSpeed * FMath::Clamp(Block.BlockingMoveSpeedMultiplier, 0.f, 1.f);
        }
    }

    UAnimMontage* StartMontage = Block.BlockStartMontage.LoadSynchronous();
    if (StartMontage) MulticastPlayMontage(StartMontage, 1.f);
    else if (UAnimMontage* Loop = Block.BlockLoopMontage.LoadSynchronous()) MulticastPlayMontage(Loop, 1.f);

    if (StartMontage && !Block.BlockLoopMontage.IsNull() && GetWorld())
    {
        const float Delay = FMath::Clamp(StartMontage->GetPlayLength(), 0.05f, 1.f);
        GetWorld()->GetTimerManager().SetTimer(BlockLoopTimer, this, &UARPGCombatComponent::PlayBlockLoopAuthority, Delay, false);
    }
    OnBlockStateChanged.Broadcast(true);
    MulticastPlayCombatCue(EARPGCombatFeedbackCue::BlockStart, GetOwner()->GetActorLocation(), GetOwner()->GetActorForwardVector());
    return true;
}

void UARPGCombatComponent::PlayBlockLoopAuthority()
{
    if (!bIsBlocking || !GetOwner() || !GetOwner()->HasAuthority()) return;
    if (UAnimMontage* Loop = GetCombatProfile().Block.BlockLoopMontage.LoadSynchronous()) MulticastPlayMontage(Loop, 1.f);
}

void UARPGCombatComponent::StopBlocking()
{
    if (!GetOwner()) return;
    if (!GetOwner()->HasAuthority()) { ServerStopBlocking(); return; }
    StopBlockingAuthority(true);
}

void UARPGCombatComponent::ServerStopBlocking_Implementation()
{
    StopBlockingAuthority(true);
}

void UARPGCombatComponent::RestoreBlockingMoveSpeed()
{
    if (CachedPreBlockWalkSpeed <= 0.f) return;
    if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
        if (UCharacterMovementComponent* Move = Character->GetCharacterMovement()) Move->MaxWalkSpeed = CachedPreBlockWalkSpeed;
    CachedPreBlockWalkSpeed = 0.f;
}

void UARPGCombatComponent::StopBlockingAuthority(bool bPlayEndMontage)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || !bIsBlocking) return;
    bIsBlocking = false;
    SetLooseCombatTag(TEXT("Combat.State.Blocking"), false);
    RestoreBlockingMoveSpeed();
    if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(BlockLoopTimer);
    if (bPlayEndMontage)
    {
        if (UAnimMontage* End = GetCombatProfile().Block.BlockEndMontage.LoadSynchronous()) MulticastPlayMontage(End, 1.f);
        MulticastPlayCombatCue(EARPGCombatFeedbackCue::BlockEnd, GetOwner()->GetActorLocation(), GetOwner()->GetActorForwardVector());
    }
    OnBlockStateChanged.Broadcast(false);
}

void UARPGCombatComponent::BeginGuardBreakAuthority(float Duration)
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;
    if (bIsStaggered)
    {
        if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(StaggerTimer);
        bIsStaggered = false;
        SetLooseCombatTag(TEXT("Combat.State.Staggered"), false);
        OnStaggerStateChanged.Broadcast(false);
    }
    if (bIsBlocking) StopBlockingAuthority(false);
    bGuardBroken = true;
    SetLooseCombatTag(TEXT("Combat.State.GuardBroken"), true);
    bIsAttacking = false;
    SetLooseCombatTag(TEXT("Combat.State.Attacking"), false);
    bComboQueued = false;
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(AttackImpactTimer);
        GetWorld()->GetTimerManager().ClearTimer(AttackFinishTimer);
        GetWorld()->GetTimerManager().ClearTimer(GuardBreakTimer);
        GetWorld()->GetTimerManager().SetTimer(GuardBreakTimer, this, &UARPGCombatComponent::EndGuardBreakAuthority, FMath::Max(0.05f, Duration), false);
    }
    if (UAnimMontage* BreakMontage = GetCombatProfile().Block.GuardBreakMontage.LoadSynchronous()) MulticastPlayMontage(BreakMontage, 1.f);
    MulticastPlayCombatCue(EARPGCombatFeedbackCue::GuardBreak, GetOwner()->GetActorLocation(), GetOwner()->GetActorForwardVector());
}

bool UARPGCombatComponent::TryApplyCriticalStaggerAuthority(AActor* Attacker, FARPGCombatHitInfo& InOutHitInfo)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || !GetWorld() || !Attacker) return false;
    if (!InOutHitInfo.bCritical || LifeState != EARPGLifeState::Alive || bIsStaggered || bGuardBroken || bIsDodging) return false;

    const FARPGStaggerSettings Stagger = GetCombatProfile().Stagger;
    if (!Stagger.bEnabled || !Stagger.bCriticalHitsCanStagger) return false;

    const float Now = GetWorld()->GetTimeSeconds();
    if (Now - LastStaggerAt < FMath::Max(0.f, Stagger.ImmunitySeconds)) return false;
    if (FMath::FRand() > FMath::Clamp(Stagger.CriticalStaggerChance, 0.f, 1.f)) return false;

    LastStaggerAt = Now;
    bIsStaggered = true;
    SetLooseCombatTag(TEXT("Combat.State.Staggered"), true);

    if (bIsBlocking) StopBlockingAuthority(false);
    if (Stagger.bInterruptAttacks)
    {
        bIsAttacking = false;
        bComboQueued = false;
        SetLooseCombatTag(TEXT("Combat.State.Attacking"), false);
        GetWorld()->GetTimerManager().ClearTimer(AttackImpactTimer);
        GetWorld()->GetTimerManager().ClearTimer(AttackFinishTimer);
    }
    if (bIsDodging)
    {
        bIsDodging = false;
        SetLooseCombatTag(TEXT("Combat.State.Dodging"), false);
        GetWorld()->GetTimerManager().ClearTimer(DodgeFinishTimer);
    }

    // Cancel path-following immediately instead of waiting for the AI think timer. Otherwise an
    // active MoveTo can fight LaunchCharacter for a frame or two and make knockback look like it
    // never happened. This is harmless for player-controlled characters.
    if (APawn* Pawn = Cast<APawn>(GetOwner()))
        if (AAIController* AI = Cast<AAIController>(Pawn->GetController()))
            AI->StopMovement();

    UAnimMontage* StaggerMontage = Stagger.StaggerMontage.LoadSynchronous();
    if (!StaggerMontage) StaggerMontage = GetHitReactMontage();
    if (StaggerMontage) MulticastPlayMontage(StaggerMontage, 1.f);

    FVector Away = (GetOwner()->GetActorLocation() - Attacker->GetActorLocation()).GetSafeNormal2D();
    if (Away.IsNearlyZero()) Away = -Attacker->GetActorForwardVector().GetSafeNormal2D();
    const FVector KnockbackVelocity = Away * FMath::Max(0.f, Stagger.KnockbackVelocity) + FVector::UpVector * Stagger.UpwardVelocity;

    if (Stagger.bApplyKnockback && !KnockbackVelocity.IsNearlyZero())
    {
        if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
        {
            if (UCharacterMovementComponent* Move = Character->GetCharacterMovement())
                Move->StopMovementImmediately();
            // Override both horizontal and vertical launch velocity so navigation/current velocity
            // cannot dilute a configured stagger impulse.
            Character->LaunchCharacter(KnockbackVelocity, true, true);
        }
        else if (UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(GetOwner()->GetRootComponent()))
        {
            if (Primitive->IsSimulatingPhysics())
                Primitive->AddImpulse(KnockbackVelocity * Primitive->GetMass());
        }
    }

    InOutHitInfo.bStaggered = true;
    InOutHitInfo.KnockbackVelocity = KnockbackVelocity;
    OnStaggerStateChanged.Broadcast(true);
    MulticastPlayCombatCue(EARPGCombatFeedbackCue::Stagger, LastReceivedHitLocation, LastReceivedHitDirection);

    GetWorld()->GetTimerManager().ClearTimer(StaggerTimer);
    GetWorld()->GetTimerManager().SetTimer(StaggerTimer, this, &UARPGCombatComponent::EndStaggerAuthority, FMath::Max(0.05f, Stagger.Duration), false);
    return true;
}

void UARPGCombatComponent::EndStaggerAuthority()
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || !bIsStaggered) return;
    bIsStaggered = false;
    SetLooseCombatTag(TEXT("Combat.State.Staggered"), false);
    OnStaggerStateChanged.Broadcast(false);
}

void UARPGCombatComponent::EndGuardBreakAuthority()
{
    if (GetOwner() && GetOwner()->HasAuthority())
    {
        bGuardBroken = false;
        SetLooseCombatTag(TEXT("Combat.State.GuardBroken"), false);
    }
}

bool UARPGCombatComponent::IsBlockFacingAttacker(AActor* Attacker, float HalfAngleDegrees) const
{
    if (!GetOwner() || !Attacker) return false;
    FVector ToAttacker = (Attacker->GetActorLocation() - GetOwner()->GetActorLocation()).GetSafeNormal2D();
    const FVector Forward = GetOwner()->GetActorForwardVector().GetSafeNormal2D();
    if (ToAttacker.IsNearlyZero()) return true;
    const float Dot = FMath::Clamp(FVector::DotProduct(Forward, ToAttacker), -1.f, 1.f);
    const float Angle = FMath::RadiansToDegrees(FMath::Acos(Dot));
    return Angle <= FMath::Clamp(HalfAngleDegrees, 0.f, 180.f);
}

bool UARPGCombatComponent::HasRequiredBlockEquipment(const FARPGBlockSettings& BlockSettings) const
{
    if (!BlockSettings.bRequiresTaggedEquipment) return true;
    if (!BlockSettings.RequiredEquipmentTag.IsValid() || !GetOwner()) return false;
    const UARPGInventoryComponent* Inventory = GetOwner()->FindComponentByClass<UARPGInventoryComponent>();
    if (!Inventory) return false;
    for (const FARPGInventoryEntry& Entry : Inventory->Items)
    {
        if (!Entry.InstanceId.IsValid() || Entry.Quantity <= 0 || !Entry.bEquipped || !Entry.EquipmentSlot.IsValid()) continue;
        const UARPGItemDefinition* Item = Inventory->ResolveItemDefinition(Entry);
        if (Item && Item->bEquippable && Item->EquipmentSlot == Entry.EquipmentSlot && Item->ItemTags.HasTag(BlockSettings.RequiredEquipmentTag)) return true;
    }
    return false;
}

void UARPGCombatComponent::SetCombatTarget(AActor* NewTarget)
{
    if (!GetOwner()) return;
    if (!GetOwner()->HasAuthority()) { ServerSetCombatTarget(NewTarget); return; }
    if (NewTarget == GetOwner()) NewTarget = nullptr;
    if (CombatTarget == NewTarget) return;
    CombatTarget = NewTarget;
    OnCombatTargetChanged.Broadcast(NewTarget);
}

void UARPGCombatComponent::ServerSetCombatTarget_Implementation(AActor* NewTarget)
{
    SetCombatTarget(NewTarget);
}

void UARPGCombatComponent::OnRep_CombatTarget()
{
    OnCombatTargetChanged.Broadcast(CombatTarget);
}

float UARPGCombatComponent::GetAttackImpactSecondsRemaining() const
{
    if (!bIsAttacking || !GetWorld()) return -1.f;
    return FMath::Max(0.f, AttackImpactAt - GetWorld()->GetTimeSeconds());
}

float UARPGCombatComponent::GetPreferredCombatRange() const
{
    const FARPGCombatProfile Profile = GetCombatProfile();
    if (Profile.BasicAttackType == EARPGBasicAttackType::Ranged) return FMath::Max(150.f, Profile.RangedMaxRange * 0.70f);
    if (Profile.BasicAttackType == EARPGBasicAttackType::Magic) return FMath::Max(150.f, Profile.MagicMaxRange * 0.70f);
    return FMath::Max(75.f, Profile.DefaultRange * 0.85f);
}

void UARPGCombatComponent::FinishAttackAuthority()
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;
    bIsAttacking = false;
    SetLooseCombatTag(TEXT("Combat.State.Attacking"), false);
    const int32 Count = FMath::Max(1, GetComboCount());
    CurrentComboIndex = (CurrentComboIndex + 1) % Count;
    ComboExpiresAt = (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f) + FMath::Max(0.f, GetCombatProfile().ComboResetTime);

    if (bComboQueued)
    {
        bComboQueued = false;
        StartAttackAuthority(CombatTarget);
    }
}

void UARPGCombatComponent::HandleStatsDeath()
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;
    if (!bKillCreditAwarded && LastDamageInstigator.IsValid())
    {
        bKillCreditAwarded = true;
        AwardKillCredit(LastDamageInstigator.Get());
    }
    Kill();
}

void UARPGCombatComponent::AwardKillCredit(AActor* Killer)
{
    if (!Killer || !GetOwner() || !GetOwner()->HasAuthority()) return;
    if (UARPGEventRouterComponent* Events = Killer->FindComponentByClass<UARPGEventRouterComponent>())
        Events->ReportKill(CreatureId, SlayerCategory, CharacterXPReward);
    if (bGrantLootToKiller)
        if (UARPGLootComponent* Loot = GetOwner()->FindComponentByClass<UARPGLootComponent>()) Loot->GrantLootTo(Killer);
}

void UARPGCombatComponent::Kill()
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || LifeState == EARPGLifeState::Dead) return;
    bIsAttacking = false;
    bIsDodging = false;
    SetLooseCombatTag(TEXT("Combat.State.Attacking"), false);
    SetLooseCombatTag(TEXT("Combat.State.Dodging"), false);
    SetLooseCombatTag(TEXT("Combat.State.Blocking"), false);
    SetLooseCombatTag(TEXT("Combat.State.GuardBroken"), false);
    SetLooseCombatTag(TEXT("Combat.State.Staggered"), false);
    if (bIsBlocking) StopBlockingAuthority(false);
    bGuardBroken = false;
    if (bIsStaggered)
    {
        bIsStaggered = false;
        OnStaggerStateChanged.Broadcast(false);
    }
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(AttackImpactTimer);
        GetWorld()->GetTimerManager().ClearTimer(AttackFinishTimer);
        GetWorld()->GetTimerManager().ClearTimer(DodgeFinishTimer);
        GetWorld()->GetTimerManager().ClearTimer(GuardBreakTimer);
        GetWorld()->GetTimerManager().ClearTimer(StaggerTimer);
    }
    SetCombatTarget(nullptr);
    FVector InheritedVelocity = FVector::ZeroVector;
    if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
    {
        if (UCharacterMovementComponent* Move = Character->GetCharacterMovement())
        {
            InheritedVelocity = Move->Velocity;
            Move->DisableMovement();
        }
    }
    LifeState = EARPGLifeState::Dead;
    SetLooseCombatTag(TEXT("Combat.State.Dead"), true);
    OnLifeStateChanged.Broadcast(LifeState);
    MulticastPlayCombatCue(EARPGCombatFeedbackCue::Death, LastReceivedHitLocation.IsNearlyZero() ? GetOwner()->GetActorLocation() : LastReceivedHitLocation, LastReceivedHitDirection);
    MulticastBeginDeathPresentation(InheritedVelocity, LastReceivedHitDirection, LastReceivedHitLocation);
    if (bAutoRespawn && GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(AutoRespawnTimer);
        GetWorld()->GetTimerManager().SetTimer(AutoRespawnTimer, this, &UARPGCombatComponent::AutoRespawnAuthority, FMath::Max(0.01f, RespawnDelay), false);
    }
}

void UARPGCombatComponent::AutoRespawnAuthority()
{
    if (GetOwner() && GetOwner()->HasAuthority() && LifeState == EARPGLifeState::Dead) RespawnAtTransform(RespawnTransform);
}

void UARPGCombatComponent::SetRespawnTransform(const FTransform& Transform)
{
    if (GetOwner() && GetOwner()->HasAuthority()) RespawnTransform = Transform;
}

void UARPGCombatComponent::RespawnAtTransform(const FTransform& Transform)
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;
    RespawnTransform = Transform;
    LifeState = EARPGLifeState::Respawning;
    OnLifeStateChanged.Broadcast(LifeState);
    MulticastResetDeathPresentation();
    GetOwner()->SetActorTransform(Transform, false, nullptr, ETeleportType::TeleportPhysics);
    if (UARPGStatsComponent* Stats = GetOwner()->FindComponentByClass<UARPGStatsComponent>()) Stats->RestoreAllVitals();
    if (ACharacter* Character = Cast<ACharacter>(GetOwner())) Character->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    LastDamageInstigator.Reset();
    LastReceivedHitLocation = FVector::ZeroVector;
    LastReceivedHitDirection = FVector::ZeroVector;
    bKillCreditAwarded = false;
    bIsStaggered = false;
    LastStaggerAt = -1000.f;
    SetLooseCombatTag(TEXT("Combat.State.Staggered"), false);
    CurrentComboIndex = 0;
    LifeState = EARPGLifeState::Alive;
    SetLooseCombatTag(TEXT("Combat.State.Dead"), false);
    OnLifeStateChanged.Broadcast(LifeState);
    if (UAnimMontage* ReviveMontage = GetReviveMontage()) MulticastPlayMontage(ReviveMontage, 1.f);
    MulticastPlayCombatCue(EARPGCombatFeedbackCue::Revive, GetOwner()->GetActorLocation(), GetOwner()->GetActorForwardVector());
}

UAnimMontage* UARPGCombatComponent::GetHitReactMontage() const
{
    if (bUseClassCombatProfile && GetOwner())
        if (const UARPGClassComponent* C = GetOwner()->FindComponentByClass<UARPGClassComponent>())
            if (C->ClassDefinition && !C->ClassDefinition->AnimationSet.HitReact.IsNull()) return C->ClassDefinition->AnimationSet.HitReact.LoadSynchronous();
    return Montages.HitReact.LoadSynchronous();
}

UAnimMontage* UARPGCombatComponent::GetDeathMontage() const
{
    if (bUseClassCombatProfile && GetOwner())
        if (const UARPGClassComponent* C = GetOwner()->FindComponentByClass<UARPGClassComponent>())
            if (C->ClassDefinition && !C->ClassDefinition->AnimationSet.Death.IsNull()) return C->ClassDefinition->AnimationSet.Death.LoadSynchronous();
    return Montages.Death.LoadSynchronous();
}

UAnimMontage* UARPGCombatComponent::GetReviveMontage() const
{
    if (bUseClassCombatProfile && GetOwner())
        if (const UARPGClassComponent* C = GetOwner()->FindComponentByClass<UARPGClassComponent>())
            if (C->ClassDefinition && !C->ClassDefinition->AnimationSet.Revive.IsNull()) return C->ClassDefinition->AnimationSet.Revive.LoadSynchronous();
    return Montages.Revive.LoadSynchronous();
}

UAnimMontage* UARPGCombatComponent::PickRandomAttackMontage(bool bMagic, bool bRanged) const
{
    const TArray<TSoftObjectPtr<UAnimMontage>>* Source = bMagic ? &Montages.MagicCasts : (bRanged ? &Montages.RangedAttacks : &Montages.MeleeAttacks);
    if (bUseClassCombatProfile && GetOwner())
    {
        if (const UARPGClassComponent* C = GetOwner()->FindComponentByClass<UARPGClassComponent>())
        {
            if (C->ClassDefinition)
            {
                const FARPGCombatMontageSet& Set = C->ClassDefinition->AnimationSet;
                Source = bMagic ? &Set.MagicCasts : (bRanged ? &Set.RangedAttacks : &Set.MeleeAttacks);
            }
        }
    }
    if (!Source || Source->Num() == 0) return nullptr;
    return (*Source)[FMath::RandRange(0, Source->Num() - 1)].LoadSynchronous();
}

USoundBase* UARPGCombatComponent::ResolveCombatSound(EARPGCombatFeedbackCue Cue, const FARPGCombatAudioSettings& Settings) const
{
    const TSoftObjectPtr<USoundBase>* Selected = nullptr;
    switch (Cue)
    {
        case EARPGCombatFeedbackCue::MeleeSwing: Selected = &Settings.MeleeSwing; break;
        case EARPGCombatFeedbackCue::RangedAttack: Selected = &Settings.RangedAttack; break;
        case EARPGCombatFeedbackCue::MagicCast: Selected = &Settings.MagicCast; break;
        case EARPGCombatFeedbackCue::Hit: Selected = &Settings.Hit; break;
        case EARPGCombatFeedbackCue::CriticalHit: Selected = Settings.CriticalHit.IsNull() ? &Settings.Hit : &Settings.CriticalHit; break;
        case EARPGCombatFeedbackCue::BlockHit: Selected = &Settings.BlockHit; break;
        case EARPGCombatFeedbackCue::Parry: Selected = &Settings.Parry; break;
        case EARPGCombatFeedbackCue::Dodge: Selected = &Settings.Dodge; break;
        case EARPGCombatFeedbackCue::BlockStart: Selected = &Settings.BlockStart; break;
        case EARPGCombatFeedbackCue::BlockEnd: Selected = &Settings.BlockEnd; break;
        case EARPGCombatFeedbackCue::GuardBreak: Selected = &Settings.GuardBreak; break;
        case EARPGCombatFeedbackCue::Stagger: Selected = &Settings.Stagger; break;
        case EARPGCombatFeedbackCue::Death: Selected = &Settings.Death; break;
        case EARPGCombatFeedbackCue::Revive: Selected = &Settings.Revive; break;
        default: break;
    }
    return Selected && !Selected->IsNull() ? Selected->LoadSynchronous() : nullptr;
}

void UARPGCombatComponent::SpawnConfiguredImpactFX(EARPGCombatFeedbackCue Cue, const FVector& Location, const FVector& Direction, const FARPGCombatFXSettings& Settings)
{
    if (!Settings.bEnabled || !GetWorld()) return;

    const TSoftObjectPtr<UNiagaraSystem>* NiagaraAsset = nullptr;
    const TSoftObjectPtr<UParticleSystem>* CascadeAsset = nullptr;
    switch (Cue)
    {
        case EARPGCombatFeedbackCue::Hit:
            NiagaraAsset = &Settings.HitNiagara;
            CascadeAsset = &Settings.HitCascadeFallback;
            break;
        case EARPGCombatFeedbackCue::CriticalHit:
            NiagaraAsset = Settings.CriticalHitNiagara.IsNull() ? &Settings.HitNiagara : &Settings.CriticalHitNiagara;
            CascadeAsset = Settings.CriticalHitCascadeFallback.IsNull() ? &Settings.HitCascadeFallback : &Settings.CriticalHitCascadeFallback;
            break;
        case EARPGCombatFeedbackCue::BlockHit:
            NiagaraAsset = &Settings.BlockNiagara;
            CascadeAsset = &Settings.BlockCascadeFallback;
            break;
        case EARPGCombatFeedbackCue::Parry:
            NiagaraAsset = &Settings.ParryNiagara;
            CascadeAsset = &Settings.ParryCascadeFallback;
            break;
        case EARPGCombatFeedbackCue::Stagger:
            NiagaraAsset = &Settings.StaggerNiagara;
            CascadeAsset = &Settings.StaggerCascadeFallback;
            break;
        default:
            return;
    }

    const FVector SafeDirection = Direction.IsNearlyZero() ? FVector::UpVector : Direction.GetSafeNormal();
    const FRotator Rotation = SafeDirection.Rotation();
    const FVector Scale(FMath::Max(0.01f, Settings.EffectScale));

    auto SpawnNiagara = [&]() -> bool
    {
        if (!NiagaraAsset || NiagaraAsset->IsNull()) return false;
        UNiagaraSystem* System = NiagaraAsset->LoadSynchronous();
        if (!System) return false;
        UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, System, Location, Rotation, Scale, true, true, ENCPoolMethod::AutoRelease, true);
        return true;
    };

    auto SpawnCascade = [&]() -> bool
    {
        if (!CascadeAsset || CascadeAsset->IsNull()) return false;
        UParticleSystem* Particle = CascadeAsset->LoadSynchronous();
        if (!Particle) return false;
        UGameplayStatics::SpawnEmitterAtLocation(this, Particle, Location, Rotation, Scale, true, EPSCPoolMethod::AutoRelease, true);
        return true;
    };

    if (Settings.bPreferNiagara)
    {
        if (!SpawnNiagara()) SpawnCascade();
    }
    else
    {
        if (!SpawnCascade()) SpawnNiagara();
    }
}

void UARPGCombatComponent::PlayCombatCueLocal(EARPGCombatFeedbackCue Cue, const FVector& Location, const FVector& Direction)
{
    if (!GetWorld() || (GetOwner() && GetOwner()->GetNetMode() == NM_DedicatedServer)) return;
    const FARPGCombatProfile Profile = GetCombatProfile();

    bool bPlayedEquipmentSwing = false;
    if (Cue == EARPGCombatFeedbackCue::MeleeSwing)
        if (UARPGEquipmentComponent* Equipment = GetOwner() ? GetOwner()->FindComponentByClass<UARPGEquipmentComponent>() : nullptr)
            bPlayedEquipmentSwing = Equipment->PlayEquippedCombatSwingSoundLocal();

    if (!bPlayedEquipmentSwing)
    {
        if (USoundBase* Sound = ResolveCombatSound(Cue, Profile.Audio))
        {
            const float PitchLow = FMath::Min(Profile.Audio.PitchMin, Profile.Audio.PitchMax);
            const float PitchHigh = FMath::Max(Profile.Audio.PitchMin, Profile.Audio.PitchMax);
            UGameplayStatics::PlaySoundAtLocation(this, Sound, Location, FMath::Max(0.f, Profile.Audio.VolumeMultiplier), FMath::FRandRange(PitchLow, PitchHigh), 0.f, nullptr, nullptr, nullptr);
        }
    }

    SpawnConfiguredImpactFX(Cue, Location, Direction, Profile.ImpactFX);
}

void UARPGCombatComponent::MulticastPlayCombatCue_Implementation(EARPGCombatFeedbackCue Cue, FVector Location, FVector Direction)
{
    PlayCombatCueLocal(Cue, Location, Direction);
}

void UARPGCombatComponent::MulticastPlayMontage_Implementation(UAnimMontage* Montage, float PlayRate)
{
    if (!Montage || !GetOwner()) return;
    if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
        if (USkeletalMeshComponent* Mesh = Character->GetMesh())
            if (UAnimInstance* Anim = Mesh->GetAnimInstance()) Anim->Montage_Play(Montage, FMath::Max(0.01f, PlayRate));
}

void UARPGCombatComponent::CacheRagdollRestoreState()
{
    if (bRagdollRestoreStateCached) return;
    ACharacter* Character = Cast<ACharacter>(GetOwner());
    if (!Character) return;

    if (USkeletalMeshComponent* Mesh = Character->GetMesh())
    {
        CachedMeshRelativeTransform = Mesh->GetRelativeTransform();
        CachedMeshCollisionProfile = Mesh->GetCollisionProfileName();
        CachedMeshCollisionEnabled = Mesh->GetCollisionEnabled();
    }
    if (UCapsuleComponent* Capsule = Character->GetCapsuleComponent())
        CachedCapsuleCollisionEnabled = Capsule->GetCollisionEnabled();

    bRagdollRestoreStateCached = true;
}

bool UARPGCombatComponent::TryStartRagdollLocal(const FVector& InheritedVelocity, const FVector& HitDirection, const FVector& HitLocation)
{
    ACharacter* Character = Cast<ACharacter>(GetOwner());
    if (!Character) return false;
    USkeletalMeshComponent* Mesh = Character->GetMesh();
    if (!Mesh || !Mesh->GetSkeletalMeshAsset() || !Mesh->GetPhysicsAsset()) return false;

    CacheRagdollRestoreState();

    if (DeathPresentation.bDisableCapsuleCollision)
        if (UCapsuleComponent* Capsule = Character->GetCapsuleComponent()) Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    if (UAnimInstance* Anim = Mesh->GetAnimInstance()) Anim->StopAllMontages(0.05f);

    if (!DeathPresentation.RagdollCollisionProfile.IsNone())
        Mesh->SetCollisionProfileName(DeathPresentation.RagdollCollisionProfile);
    else
    {
        Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        Mesh->SetCollisionObjectType(ECC_PhysicsBody);
    }

    // Dedicated servers only need the gameplay death state/collision unless explicitly asked
    // to spend CPU simulating corpse physics. Clients/listen servers still simulate normally.
    if (GetOwner() && GetOwner()->GetNetMode() == NM_DedicatedServer && !DeathPresentation.bSimulateOnDedicatedServer)
    {
        bRagdollActive = true;
        OnRagdollStateChanged.Broadcast(true);
        return true;
    }

    Mesh->SetAllBodiesSimulatePhysics(true);
    Mesh->SetSimulatePhysics(true);
    Mesh->bBlendPhysics = true;
    Mesh->WakeAllRigidBodies();

    if (!Mesh->IsAnySimulatingPhysics())
    {
        Mesh->SetAllBodiesSimulatePhysics(false);
        Mesh->SetSimulatePhysics(false);
        Mesh->bBlendPhysics = false;
        if (bRagdollRestoreStateCached)
        {
            Mesh->SetCollisionProfileName(CachedMeshCollisionProfile);
            Mesh->SetCollisionEnabled(CachedMeshCollisionEnabled);
            if (UCapsuleComponent* Capsule = Character->GetCapsuleComponent()) Capsule->SetCollisionEnabled(CachedCapsuleCollisionEnabled);
        }
        return false;
    }

    if (DeathPresentation.bTransferCharacterVelocity && !InheritedVelocity.IsNearlyZero())
        Mesh->SetAllPhysicsLinearVelocity(InheritedVelocity, false);

    if (DeathPresentation.bApplyHitDirectionImpulse && DeathPresentation.HitDirectionImpulseVelocity > 0.f && !HitDirection.IsNearlyZero())
    {
        const FVector RequestedLocation = HitLocation.IsNearlyZero() ? Mesh->Bounds.Origin : HitLocation;
        FVector ClosestPoint = RequestedLocation;
        FVector SurfaceNormal = FVector::ZeroVector;
        FName ImpactBone = NAME_None;
        float Distance = 0.f;
        Mesh->K2_GetClosestPointOnPhysicsAsset(RequestedLocation, ClosestPoint, SurfaceNormal, ImpactBone, Distance);
        Mesh->AddVelocityChangeImpulseAtLocation(HitDirection.GetSafeNormal() * DeathPresentation.HitDirectionImpulseVelocity, ClosestPoint, ImpactBone);
    }

    bRagdollActive = true;
    OnRagdollStateChanged.Broadcast(true);
    return true;
}

bool UARPGCombatComponent::ApplyDeathPresentationLocal(const FVector& InheritedVelocity, const FVector& HitDirection, const FVector& HitLocation)
{
    if (bDeathPresentationActive) return bRagdollActive;
    bDeathPresentationActive = true;

    if (DeathPresentation.bUseRagdollOnDeath && TryStartRagdollLocal(InheritedVelocity, HitDirection, HitLocation))
        return true;

    bRagdollActive = false;
    if (DeathPresentation.bFallbackToDeathMontage)
    {
        if (UAnimMontage* DeathMontage = GetDeathMontage())
        {
            if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
                if (USkeletalMeshComponent* Mesh = Character->GetMesh())
                    if (UAnimInstance* Anim = Mesh->GetAnimInstance()) Anim->Montage_Play(DeathMontage, 1.f);
        }
    }
    return false;
}

void UARPGCombatComponent::ResetDeathPresentationLocal()
{
    if (!bDeathPresentationActive && !bRagdollActive) return;
    ACharacter* Character = Cast<ACharacter>(GetOwner());
    if (Character)
    {
        if (USkeletalMeshComponent* Mesh = Character->GetMesh())
        {
            if (UAnimInstance* Anim = Mesh->GetAnimInstance()) Anim->StopAllMontages(0.f);
            Mesh->SetAllBodiesSimulatePhysics(false);
            Mesh->SetSimulatePhysics(false);
            Mesh->bBlendPhysics = false;

            if (bRagdollRestoreStateCached)
            {
                if (UCapsuleComponent* Capsule = Character->GetCapsuleComponent())
                {
                    Mesh->AttachToComponent(Capsule, FAttachmentTransformRules::KeepRelativeTransform);
                    Capsule->SetCollisionEnabled(CachedCapsuleCollisionEnabled);
                }
                Mesh->SetRelativeTransform(CachedMeshRelativeTransform, false, nullptr, ETeleportType::TeleportPhysics);
                Mesh->SetCollisionProfileName(CachedMeshCollisionProfile);
                Mesh->SetCollisionEnabled(CachedMeshCollisionEnabled);
            }
        }
    }

    const bool bWasRagdoll = bRagdollActive;
    bRagdollActive = false;
    bDeathPresentationActive = false;
    if (bWasRagdoll) OnRagdollStateChanged.Broadcast(false);
}

void UARPGCombatComponent::MulticastBeginDeathPresentation_Implementation(FVector InheritedVelocity, FVector HitDirection, FVector HitLocation)
{
    ApplyDeathPresentationLocal(InheritedVelocity, HitDirection, HitLocation);
}

void UARPGCombatComponent::MulticastResetDeathPresentation_Implementation()
{
    ResetDeathPresentationLocal();
}

void UARPGCombatComponent::SetLooseCombatTag(FName TagName, bool bEnabled) const
{
    if (!GetOwner()) return;
    const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(TagName, false);
    if (!Tag.IsValid()) return;
    if (IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(GetOwner()))
    {
        if (UAbilitySystemComponent* ASC = ASI->GetAbilitySystemComponent())
        {
            if (bEnabled) ASC->AddLooseGameplayTag(Tag);
            else ASC->RemoveLooseGameplayTag(Tag);
        }
    }
}

void UARPGCombatComponent::OnRep_Staggered()
{
    SetLooseCombatTag(TEXT("Combat.State.Staggered"), bIsStaggered);
    OnStaggerStateChanged.Broadcast(bIsStaggered);
}

void UARPGCombatComponent::OnRep_LifeState(EARPGLifeState OldState)
{
    OnLifeStateChanged.Broadcast(LifeState);
    if (LifeState == EARPGLifeState::Dead)
    {
        if (!bDeathPresentationActive) ApplyDeathPresentationLocal(FVector::ZeroVector, FVector::ZeroVector, GetOwner() ? GetOwner()->GetActorLocation() : FVector::ZeroVector);
    }
    else if (bDeathPresentationActive || bRagdollActive)
    {
        ResetDeathPresentationLocal();
    }
}

void UARPGCombatComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(UARPGCombatComponent, LifeState);
    DOREPLIFETIME(UARPGCombatComponent, DeathPresentation);
    DOREPLIFETIME(UARPGCombatComponent, CombatTarget);
    DOREPLIFETIME(UARPGCombatComponent, bIsAttacking);
    DOREPLIFETIME(UARPGCombatComponent, bIsDodging);
    DOREPLIFETIME(UARPGCombatComponent, bIsBlocking);
    DOREPLIFETIME(UARPGCombatComponent, bGuardBroken);
    DOREPLIFETIME(UARPGCombatComponent, bIsStaggered);
    DOREPLIFETIME(UARPGCombatComponent, CurrentComboIndex);
    DOREPLIFETIME(UARPGCombatComponent, AttackSerial);
}
