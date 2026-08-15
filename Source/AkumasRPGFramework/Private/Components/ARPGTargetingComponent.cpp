#include "Components/ARPGTargetingComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Abilities/GameplayAbility.h"
#include "Abilities/ARPGGameplayAbility.h"
#include "Actors/ARPGCharacter.h"
#include "Components/ARPGCombatComponent.h"
#include "Components/ARPGAICombatComponent.h"
#include "Components/ARPGFactionComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/WidgetComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/Texture2D.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInterface.h"
#include "Targeting/ARPGTargetMarkerWidget.h"
#include "TimerManager.h"

UARPGTargetingComponent::UARPGTargetingComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickInterval = 0.f;
    SetIsReplicatedByDefault(true);
}

void UARPGTargetingComponent::BeginPlay()
{
    Super::BeginPlay();
    if (GetOwner())
        PrimaryComponentTick.AddPrerequisite(GetOwner(), GetOwner()->PrimaryActorTick);
    if (UARPGCombatComponent* Combat = GetOwner() ? GetOwner()->FindComponentByClass<UARPGCombatComponent>() : nullptr)
    {
        Combat->OnCombatTargetChanged.AddDynamic(this, &UARPGTargetingComponent::HandleCombatTargetChanged);
        if (IsValid(Combat->CombatTarget)) ApplyTargetLocal(Combat->CombatTarget, false);
    }
    UAbilitySystemComponent* ASC = nullptr;
    if (IAbilitySystemInterface* AbilityOwner = Cast<IAbilitySystemInterface>(GetOwner())) ASC = AbilityOwner->GetAbilitySystemComponent();
    if (!ASC && GetOwner()) ASC = GetOwner()->FindComponentByClass<UAbilitySystemComponent>();
    if (ASC) ASC->AbilityActivatedCallbacks.AddUObject(this, &UARPGTargetingComponent::HandleAbilityActivated);
}

void UARPGTargetingComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    ApplyLockedMovementFacing(false);
    ApplyLockedCameraRig(false);
    if (GetOwner())
    {
        if (UARPGCombatComponent* Combat = GetOwner()->FindComponentByClass<UARPGCombatComponent>())
            Combat->OnCombatTargetChanged.RemoveDynamic(this, &UARPGTargetingComponent::HandleCombatTargetChanged);
        UAbilitySystemComponent* ASC = nullptr;
        if (IAbilitySystemInterface* AbilityOwner = Cast<IAbilitySystemInterface>(GetOwner())) ASC = AbilityOwner->GetAbilitySystemComponent();
        if (!ASC) ASC = GetOwner()->FindComponentByClass<UAbilitySystemComponent>();
        if (ASC) ASC->AbilityActivatedCallbacks.RemoveAll(this);
    }
    DestroyTargetMarker(false);
    Super::EndPlay(EndPlayReason);
}

APlayerController* UARPGTargetingComponent::GetOwningPlayerController() const
{
    const APawn* Pawn = Cast<APawn>(GetOwner());
    return Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
}

bool UARPGTargetingComponent::IsLocallyControlledPlayer() const
{
    const APawn* Pawn = Cast<APawn>(GetOwner());
    return Pawn && Pawn->IsLocallyControlled() && GetOwningPlayerController() != nullptr;
}

void UARPGTargetingComponent::GetViewData(FVector& OutLocation, FVector& OutForward, FVector& OutRight) const
{
    OutLocation = GetOwner() ? GetOwner()->GetActorLocation() : FVector::ZeroVector;
    FRotator ViewRotation = GetOwner() ? GetOwner()->GetActorRotation() : FRotator::ZeroRotator;
    if (APlayerController* PC = GetOwningPlayerController())
        PC->GetPlayerViewPoint(OutLocation, ViewRotation);
    OutForward = ViewRotation.Vector().GetSafeNormal();
    OutRight = FRotationMatrix(ViewRotation).GetScaledAxis(EAxis::Y).GetSafeNormal();
}

FVector UARPGTargetingComponent::GetTargetAimLocation(AActor* Target) const
{
    if (!Target) return FVector::ZeroVector;
    if (!TargetAimSocketName.IsNone())
    {
        if (const ACharacter* Character = Cast<ACharacter>(Target))
            if (const USkeletalMeshComponent* Mesh = Character->GetMesh())
                if (Mesh->DoesSocketExist(TargetAimSocketName)) return Mesh->GetSocketLocation(TargetAimSocketName);
    }
    FVector Origin = Target->GetActorLocation();
    FVector Extent = FVector::ZeroVector;
    Target->GetActorBounds(true, Origin, Extent, false);
    return Origin + FVector(0.f, 0.f, Extent.Z * 0.35f);
}

bool UARPGTargetingComponent::HasLineOfSightToTarget(AActor* Candidate) const
{
    if (!Candidate || !GetWorld() || !GetOwner()) return false;
    FVector ViewLocation, Forward, Right;
    GetViewData(ViewLocation, Forward, Right);
    const FVector End = GetTargetAimLocation(Candidate);
    FHitResult Hit;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(ARPGTargetingLOS), false, GetOwner());
    Params.AddIgnoredActor(GetOwner());
    const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, ViewLocation, End, VisibilityTraceChannel, Params);
    if (!bHit) return true;
    AActor* HitActor = Hit.GetActor();
    return HitActor == Candidate || (HitActor && HitActor->IsOwnedBy(Candidate));
}

bool UARPGTargetingComponent::IsValidLockOnTarget(AActor* Candidate) const
{
    if (!bEnabled || !GetOwner() || !Candidate || Candidate == GetOwner() || Candidate->IsActorBeingDestroyed()) return false;

    if (const UARPGCombatComponent* TargetCombat = Candidate->FindComponentByClass<UARPGCombatComponent>())
        if (!TargetCombat->IsAlive()) return false;

    if (bOnlyHostileTargets)
    {
        const UARPGFactionComponent* MyFaction = GetOwner()->FindComponentByClass<UARPGFactionComponent>();
        const UARPGFactionComponent* TheirFaction = Candidate->FindComponentByClass<UARPGFactionComponent>();
        const bool bBothHaveFactionIdentity =
            MyFaction && TheirFaction && MyFaction->HasFactionIdentity() && TheirFaction->HasFactionIdentity();

        bool bHostile = bBothHaveFactionIdentity && MyFaction->IsHostileTo(TheirFaction);

        // Hostility can also be owned by the target AI (retaliation, explicit fallback aggression, etc.).
        // This closes the player-vs-AI asymmetry where an NPC can actively consider the player hostile
        // while local lock-on rejects that same NPC because the base faction relation is neutral/unknown.
        if (!bHostile)
        {
            if (const UARPGAICombatComponent* TargetAI = Candidate->FindComponentByClass<UARPGAICombatComponent>())
                bHostile = TargetAI->bEnabled && TargetAI->IsTargetConsideredHostile(GetOwner());
        }

        // When one/both faction identities are genuinely unresolved, fall back to the combat permission
        // contract instead of treating the mere presence of two empty Faction components as neutrality.
        if (!bHostile && !bBothHaveFactionIdentity)
        {
            if (const UARPGCombatComponent* MyCombat = GetOwner()->FindComponentByClass<UARPGCombatComponent>())
                bHostile = MyCombat->CanDamageActor(Candidate);
        }

        if (!bHostile) return false;
    }
    else if (const UARPGCombatComponent* MyCombat = GetOwner()->FindComponentByClass<UARPGCombatComponent>())
    {
        if (!MyCombat->CanDamageActor(Candidate)) return false;
    }

    return true;
}

TArray<AActor*> UARPGTargetingComponent::GatherCandidates(float Radius) const
{
    TArray<AActor*> Result;
    if (!GetWorld() || !GetOwner()) return Result;

    FCollisionObjectQueryParams Objects;
    Objects.AddObjectTypesToQuery(ECC_Pawn);
    FCollisionQueryParams Params(SCENE_QUERY_STAT(ARPGTargetingCandidates), false, GetOwner());
    TArray<FOverlapResult> Overlaps;
    if (!GetWorld()->OverlapMultiByObjectType(Overlaps, GetOwner()->GetActorLocation(), FQuat::Identity, Objects,
                                              FCollisionShape::MakeSphere(FMath::Max(100.f, Radius)), Params))
        return Result;

    TSet<AActor*> Unique;
    for (const FOverlapResult& Overlap : Overlaps)
    {
        AActor* Candidate = Overlap.GetActor();
        if (Candidate && !Unique.Contains(Candidate) && IsValidLockOnTarget(Candidate))
        {
            Unique.Add(Candidate);
            Result.Add(Candidate);
        }
    }
    return Result;
}

float UARPGTargetingComponent::ScoreCandidate(AActor* Candidate, const FVector& ViewLocation, const FVector& ViewForward) const
{
    if (!Candidate) return TNumericLimits<float>::Max();
    const FVector ToTarget = GetTargetAimLocation(Candidate) - ViewLocation;
    const float Distance = ToTarget.Size();
    if (Distance <= KINDA_SMALL_NUMBER || Distance > MaxAcquireDistance) return TNumericLimits<float>::Max();
    const FVector Direction = ToTarget / Distance;
    const float Dot = FMath::Clamp(FVector::DotProduct(ViewForward, Direction), -1.f, 1.f);
    const float Angle = FMath::RadiansToDegrees(FMath::Acos(Dot));
    if (Angle > AcquireHalfAngleDegrees) return TNumericLimits<float>::Max();
    if (bRequireLineOfSightOnAcquire && !HasLineOfSightToTarget(Candidate)) return TNumericLimits<float>::Max();

    const float AngleNormalized = Angle / FMath::Max(1.f, AcquireHalfAngleDegrees);
    const float DistanceNormalized = Distance / FMath::Max(100.f, MaxAcquireDistance);
    return AngleNormalized * FMath::Max(0.f, ScreenCenterWeight) + DistanceNormalized * FMath::Max(0.f, DistanceWeight);
}

AActor* UARPGTargetingComponent::FindBestTargetInternal() const
{
    FVector ViewLocation, ViewForward, ViewRight;
    GetViewData(ViewLocation, ViewForward, ViewRight);
    AActor* Best = nullptr;
    float BestScore = TNumericLimits<float>::Max();
    for (AActor* Candidate : GatherCandidates(MaxAcquireDistance))
    {
        const float Score = ScoreCandidate(Candidate, ViewLocation, ViewForward);
        if (Score < BestScore)
        {
            BestScore = Score;
            Best = Candidate;
        }
    }
    return Best;
}

AActor* UARPGTargetingComponent::FindSwitchTargetInternal(bool bRight) const
{
    if (!CurrentTarget) return FindBestTargetInternal();
    FVector ViewLocation, ViewForward, ViewRight;
    GetViewData(ViewLocation, ViewForward, ViewRight);

    auto SignedViewAngle = [&](AActor* Actor) -> float
    {
        FVector Direction = (GetTargetAimLocation(Actor) - ViewLocation).GetSafeNormal();
        const float X = FVector::DotProduct(Direction, ViewForward);
        const float Y = FVector::DotProduct(Direction, ViewRight);
        return FMath::RadiansToDegrees(FMath::Atan2(Y, X));
    };

    const float CurrentAngle = SignedViewAngle(CurrentTarget);
    AActor* Best = nullptr;
    float BestDelta = TNumericLimits<float>::Max();
    AActor* WrapBest = nullptr;
    float WrapValue = bRight ? TNumericLimits<float>::Max() : -TNumericLimits<float>::Max();

    for (AActor* Candidate : GatherCandidates(MaxSwitchDistance))
    {
        if (!Candidate || Candidate == CurrentTarget) continue;
        if (bRequireLineOfSightOnAcquire && !HasLineOfSightToTarget(Candidate)) continue;
        const float CandidateAngle = SignedViewAngle(Candidate);
        const float Delta = FMath::FindDeltaAngleDegrees(CurrentAngle, CandidateAngle);
        const bool bCorrectSide = bRight ? Delta >= MinimumSwitchAngleDegrees : Delta <= -MinimumSwitchAngleDegrees;
        if (bCorrectSide)
        {
            const float AbsDelta = FMath::Abs(Delta);
            if (AbsDelta < BestDelta)
            {
                BestDelta = AbsDelta;
                Best = Candidate;
            }
        }

        if (bWrapTargetSwitching)
        {
            if (bRight)
            {
                if (CandidateAngle < WrapValue) { WrapValue = CandidateAngle; WrapBest = Candidate; }
            }
            else
            {
                if (CandidateAngle > WrapValue) { WrapValue = CandidateAngle; WrapBest = Candidate; }
            }
        }
    }

    return Best ? Best : WrapBest;
}

bool UARPGTargetingComponent::ToggleLockOn()
{
    if (!bEnabled) return false;
    if (CurrentTarget) { UnlockTarget(); return true; }
    return LockOnBestTarget();
}

bool UARPGTargetingComponent::LockOnBestTarget()
{
    if (!bEnabled) return false;
    if (AActor* Target = FindBestTargetInternal()) return SetLockOnTarget(Target);
    return false;
}

bool UARPGTargetingComponent::SwitchTargetLeft()
{
    if (!bEnabled || !bAllowTargetSwitching) return false;
    if (AActor* Target = FindSwitchTargetInternal(false)) return SetLockOnTarget(Target);
    return false;
}

bool UARPGTargetingComponent::SwitchTargetRight()
{
    if (!bEnabled || !bAllowTargetSwitching) return false;
    if (AActor* Target = FindSwitchTargetInternal(true)) return SetLockOnTarget(Target);
    return false;
}

bool UARPGTargetingComponent::SetLockOnTarget(AActor* NewTarget)
{
    if (!IsValidLockOnTarget(NewTarget)) return false;
    const float Distance = FVector::Dist(GetOwner()->GetActorLocation(), NewTarget->GetActorLocation());
    if (Distance > MaxMaintainDistance) return false;
    if (bRequireLineOfSightOnAcquire && !HasLineOfSightToTarget(NewTarget)) return false;

    ApplyTargetLocal(NewTarget, true);
    return true;
}

void UARPGTargetingComponent::UnlockTarget()
{
    ApplyTargetLocal(nullptr, true);
}

void UARPGTargetingComponent::ServerSetLockOnTarget_Implementation(AActor* NewTarget)
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;
    UARPGCombatComponent* Combat = GetOwner()->FindComponentByClass<UARPGCombatComponent>();
    if (NewTarget)
    {
        const bool bValid = IsValidLockOnTarget(NewTarget) &&
            FVector::DistSquared(GetOwner()->GetActorLocation(), NewTarget->GetActorLocation()) <= FMath::Square(MaxMaintainDistance);
        if (!bValid)
        {
            ClientConfirmLockOnTarget(Combat ? Combat->CombatTarget.Get() : nullptr);
            return;
        }
    }
    if (Combat) Combat->SetCombatTarget(NewTarget);
    ClientConfirmLockOnTarget(NewTarget);
}

void UARPGTargetingComponent::ClientConfirmLockOnTarget_Implementation(AActor* ConfirmedTarget)
{
    if (CurrentTarget != ConfirmedTarget) ApplyTargetLocal(ConfirmedTarget, false);
}

void UARPGTargetingComponent::ApplyTargetLocal(AActor* NewTarget, bool bSyncCombatTarget)
{
    if (NewTarget == GetOwner()) NewTarget = nullptr;
    if (CurrentTarget == NewTarget)
    {
        if (bSyncCombatTarget && GetOwner() && !GetOwner()->HasAuthority()) ServerSetLockOnTarget(NewTarget);
        return;
    }

    AActor* OldTarget = CurrentTarget;
    DestroyTargetMarker(OldTarget != nullptr && NewTarget == nullptr);
    CurrentTarget = NewTarget;
    LineOfSightLostAt = -1.f;
    ForceFacingUntil = -1.f;
    SetLockedOnGameplayTag(CurrentTarget != nullptr);
    ApplyLockedMovementFacing(CurrentTarget != nullptr);
    ApplyLockedCameraRig(CurrentTarget != nullptr);

    if (CurrentTarget && bAutoCreateTargetMarker && IsLocallyControlledPlayer()) CreateTargetMarker(CurrentTarget);
    OnTargetChanged.Broadcast(OldTarget, CurrentTarget);
    OnLockStateChanged.Broadcast(CurrentTarget != nullptr);

    if (bSyncCombatTarget && GetOwner())
    {
        if (GetOwner()->HasAuthority())
        {
            if (UARPGCombatComponent* Combat = GetOwner()->FindComponentByClass<UARPGCombatComponent>())
            {
                bApplyingCombatTargetChange = true;
                Combat->SetCombatTarget(NewTarget);
                bApplyingCombatTargetChange = false;
            }
        }
        else
        {
            ServerSetLockOnTarget(NewTarget);
        }
    }
}

void UARPGTargetingComponent::HandleCombatTargetChanged(AActor* NewCombatTarget)
{
    if (bApplyingCombatTargetChange) return;
    if (CurrentTarget != NewCombatTarget) ApplyTargetLocal(NewCombatTarget, false);
}

AActor* UARPGTargetingComponent::GetCurrentTarget() const
{
    if (IsValid(CurrentTarget)) return CurrentTarget;
    if (const UARPGCombatComponent* Combat = GetOwner() ? GetOwner()->FindComponentByClass<UARPGCombatComponent>() : nullptr)
        return IsValid(Combat->CombatTarget) ? Combat->CombatTarget.Get() : nullptr;
    return nullptr;
}

void UARPGTargetingComponent::ValidateCurrentTarget(float DeltaTime)
{
    if (!CurrentTarget) return;
    const float DistanceSq = GetOwner() ? FVector::DistSquared(GetOwner()->GetActorLocation(), CurrentTarget->GetActorLocation()) : TNumericLimits<float>::Max();
    const bool bBasicValid = IsValidLockOnTarget(CurrentTarget) && DistanceSq <= FMath::Square(MaxMaintainDistance);
    if (!bBasicValid)
    {
        const bool bShouldReacquire = bAutoReacquireWhenTargetLost;
        ApplyTargetLocal(nullptr, true);
        if (bShouldReacquire) LockOnBestTarget();
        return;
    }

    if (bRequireLineOfSightToMaintain)
    {
        const bool bHasLOS = HasLineOfSightToTarget(CurrentTarget);
        if (!bHasLOS)
        {
            const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
            if (LineOfSightLostAt < 0.f) LineOfSightLostAt = Now;
            if (Now - LineOfSightLostAt >= LostLineOfSightGraceSeconds)
            {
                const bool bShouldReacquire = bAutoReacquireWhenTargetLost;
                ApplyTargetLocal(nullptr, true);
                if (bShouldReacquire) LockOnBestTarget();
            }
        }
        else
        {
            LineOfSightLostAt = -1.f;
        }
    }
}


void UARPGTargetingComponent::HandleAbilityActivated(UGameplayAbility* Ability)
{
    if (!bAutoFaceOnAbilities || !Ability || !GetCurrentTarget()) return;
    if (const UARPGGameplayAbility* ARPGAbility = Cast<UARPGGameplayAbility>(Ability))
    {
        if (ARPGAbility->TargetingPolicy == EARPGAbilityTargetingPolicy::IgnoreLockOn || !ARPGAbility->bAutoFaceLockOnTarget) return;
    }
    const FGameplayTag IgnoreTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Combat.Targeting.IgnoreAutoFace")), false);
    if (IgnoreTag.IsValid() && Ability->GetAssetTags().HasTag(IgnoreTag)) return;
    if (!AbilityFacingIgnoredTags.IsEmpty() && Ability->GetAssetTags().HasAny(AbilityFacingIgnoredTags)) return;
    RequestAbilityFacing();
}

void UARPGTargetingComponent::RequestAttackFacing()
{
    if (bAutoFaceOnBasicAttack) RequestActionFacing(ActionFacingDuration, false);
}

void UARPGTargetingComponent::RequestAbilityFacing()
{
    if (bAutoFaceOnAbilities) RequestActionFacing(ActionFacingDuration, false);
}

void UARPGTargetingComponent::RequestActionFacing(float Duration, bool bInstant)
{
    if (!GetCurrentTarget() || !GetWorld()) return;
    ForceFacingUntil = FMath::Max(ForceFacingUntil, GetWorld()->GetTimeSeconds() + FMath::Max(0.f, Duration));
    if (bInstant) FaceCurrentTargetNow(true);
}

void UARPGTargetingComponent::FaceCurrentTargetNow(bool bInstant)
{
    AActor* Target = GetCurrentTarget();
    if (!Target || !GetOwner()) return;
    FVector Delta = GetTargetAimLocation(Target) - GetOwner()->GetActorLocation();
    if (bIgnorePitchWhenFacing) Delta.Z = 0.f;
    if (Delta.IsNearlyZero()) return;
    FRotator Desired = Delta.Rotation();
    if (bIgnorePitchWhenFacing) { Desired.Pitch = 0.f; Desired.Roll = 0.f; }
    if (bInstant || !bSmoothFacing || !GetWorld()) GetOwner()->SetActorRotation(Desired);
    else GetOwner()->SetActorRotation(FMath::RInterpTo(GetOwner()->GetActorRotation(), Desired, GetWorld()->GetDeltaSeconds(), RotationInterpSpeed));
}

void UARPGTargetingComponent::ApplyLockedMovementFacing(bool bLocked)
{
    ACharacter* Character = Cast<ACharacter>(GetOwner());
    if (!Character || !Character->IsPlayerControlled()) return;
    UCharacterMovementComponent* Movement = Character->GetCharacterMovement();

    if (bLocked)
    {
        if (!bOverrideMovementFacingWhileLocked || bMovementFacingCached) return;
        bMovementFacingCached = true;
        bCachedUseControllerRotationYaw = Character->bUseControllerRotationYaw;
        if (Movement)
        {
            bCachedOrientRotationToMovement = Movement->bOrientRotationToMovement;
            bCachedUseControllerDesiredRotation = Movement->bUseControllerDesiredRotation;
            Movement->bOrientRotationToMovement = false;
            Movement->bUseControllerDesiredRotation = false;
        }
        // ARPG targeting owns yaw while locked. Movement input can now strafe/orbit without
        // pulling the character away from the selected opponent.
        Character->bUseControllerRotationYaw = false;
        return;
    }

    if (!bMovementFacingCached) return;
    Character->bUseControllerRotationYaw = bCachedUseControllerRotationYaw;
    if (Movement)
    {
        Movement->bOrientRotationToMovement = bCachedOrientRotationToMovement;
        Movement->bUseControllerDesiredRotation = bCachedUseControllerDesiredRotation;
    }
    bMovementFacingCached = false;
}

void UARPGTargetingComponent::ApplyLockedCameraRig(bool bLocked)
{
    if (!IsLocallyControlledPlayer() || !GetOwner()) return;

    if (bLocked)
    {
        if (!bLockCameraToTarget || !bAutoConfigureCameraRigForLock || bCameraRigLockCached) return;
        bCameraRigLockCached = true;

        CachedSpringArm = GetOwner()->FindComponentByClass<USpringArmComponent>();
        if (CachedSpringArm.IsValid())
        {
            bCachedSpringArmUsePawnControlRotation = CachedSpringArm->bUsePawnControlRotation;
            CachedSpringArm->bUsePawnControlRotation = true;
            return;
        }

        CachedCamera = GetOwner()->FindComponentByClass<UCameraComponent>();
        if (CachedCamera.IsValid())
        {
            bCachedCameraUsePawnControlRotation = CachedCamera->bUsePawnControlRotation;
            CachedCamera->bUsePawnControlRotation = true;
        }
        return;
    }

    if (!bCameraRigLockCached) return;
    if (CachedSpringArm.IsValid()) CachedSpringArm->bUsePawnControlRotation = bCachedSpringArmUsePawnControlRotation;
    if (CachedCamera.IsValid()) CachedCamera->bUsePawnControlRotation = bCachedCameraUsePawnControlRotation;
    CachedSpringArm.Reset();
    CachedCamera.Reset();
    bCameraRigLockCached = false;
}

void UARPGTargetingComponent::UpdateCameraLock(float DeltaTime)
{
    if (!bLockCameraToTarget || !IsLocallyControlledPlayer()) return;
    AActor* Target = GetCurrentTarget();
    APlayerController* PC = GetOwningPlayerController();
    if (!Target || !PC) return;

    FVector ViewLocation = FVector::ZeroVector;
    FRotator ViewRotation = PC->GetControlRotation();
    PC->GetPlayerViewPoint(ViewLocation, ViewRotation);

    const FVector TargetLocation = GetTargetAimLocation(Target);
    FVector CameraToTarget = TargetLocation - ViewLocation;
    FVector OwnerToTarget = TargetLocation - (GetOwner() ? GetOwner()->GetActorLocation() : ViewLocation);
    OwnerToTarget.Z = 0.f;
    if (CameraToTarget.IsNearlyZero() || OwnerToTarget.IsNearlyZero()) return;

    // Keep camera yaw behind the player-target axis (classic Z-target feel) while pitch uses
    // the actual camera-to-target line so vertical tracking remains natural on slopes/large enemies.
    FRotator Desired = CameraToTarget.Rotation();
    Desired.Yaw = OwnerToTarget.Rotation().Yaw;
    Desired.Roll = 0.f;
    if (!bCameraTracksTargetPitch)
    {
        Desired.Pitch = PC->GetControlRotation().Pitch;
    }
    else
    {
        const float MinPitch = FMath::Min(CameraMinPitch, CameraMaxPitch);
        const float MaxPitch = FMath::Max(CameraMinPitch, CameraMaxPitch);
        Desired.Pitch = FMath::Clamp(FRotator::NormalizeAxis(Desired.Pitch + CameraPitchOffsetDegrees), MinPitch, MaxPitch);
    }

    FRotator Current = PC->GetControlRotation();
    Current.Roll = 0.f;
    const FRotator NewControlRotation = (!bSmoothCameraLock || CameraRotationInterpSpeed <= 0.f)
        ? Desired
        : FMath::RInterpTo(Current, Desired, DeltaTime, CameraRotationInterpSpeed);
    PC->SetControlRotation(NewControlRotation);
}

void UARPGTargetingComponent::UpdateFacing(float DeltaTime)
{
    AActor* Target = GetCurrentTarget();
    if (!Target || !GetOwner() || !GetWorld()) return;
    const float Now = GetWorld()->GetTimeSeconds();
    if (!bFaceContinuouslyWhileLocked && Now > ForceFacingUntil) return;

    FVector Delta = GetTargetAimLocation(Target) - GetOwner()->GetActorLocation();
    if (bIgnorePitchWhenFacing) Delta.Z = 0.f;
    if (Delta.IsNearlyZero()) return;
    FRotator Desired = Delta.Rotation();
    if (bIgnorePitchWhenFacing) { Desired.Pitch = 0.f; Desired.Roll = 0.f; }
    const FRotator NewRotation = (!bSmoothFacing || RotationInterpSpeed <= 0.f)
        ? Desired
        : FMath::RInterpTo(GetOwner()->GetActorRotation(), Desired, DeltaTime, RotationInterpSpeed);
    GetOwner()->SetActorRotation(NewRotation);
}

void UARPGTargetingComponent::SetLockedOnGameplayTag(bool bLocked) const
{
    AActor* Owner = GetOwner();
    if (!Owner) return;
    UAbilitySystemComponent* ASC = nullptr;
    if (IAbilitySystemInterface* AbilityOwner = Cast<IAbilitySystemInterface>(Owner)) ASC = AbilityOwner->GetAbilitySystemComponent();
    if (!ASC) ASC = Owner->FindComponentByClass<UAbilitySystemComponent>();
    if (!ASC) return;
    const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(TEXT("Combat.State.LockedOn")), false);
    if (Tag.IsValid()) ASC->SetLooseGameplayTagCount(Tag, bLocked ? 1 : 0);
}

void UARPGTargetingComponent::CreateTargetMarker(AActor* Target)
{
    if (!Target || !GetOwner() || !IsLocallyControlledPlayer() || !bAutoCreateTargetMarker) return;
    DestroyTargetMarker(false);

    TargetMarkerComponent = NewObject<UWidgetComponent>(GetOwner(), NAME_None, RF_Transient);
    if (!TargetMarkerComponent) return;
    TargetMarkerComponent->SetWidgetSpace(EWidgetSpace::Screen);
    TargetMarkerComponent->SetDrawAtDesiredSize(false);
    TargetMarkerComponent->SetDrawSize(TargetMarkerSize);
    TargetMarkerComponent->SetPivot(FVector2D(0.5f, 0.5f));
    TargetMarkerComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    TargetMarkerComponent->SetGenerateOverlapEvents(false);
    GetOwner()->AddInstanceComponent(TargetMarkerComponent);
    TargetMarkerComponent->RegisterComponent();
    if (Target->GetRootComponent()) TargetMarkerComponent->AttachToComponent(Target->GetRootComponent(), FAttachmentTransformRules::KeepWorldTransform);
    UpdateTargetMarkerLocation();

    APlayerController* PC = GetOwningPlayerController();
    if (PC && PC->GetLocalPlayer()) TargetMarkerComponent->SetOwnerPlayer(PC->GetLocalPlayer());
    TSubclassOf<UARPGTargetMarkerWidget> MarkerClass = TargetMarkerWidgetClass;
    if (!MarkerClass)
    {
        MarkerClass = UARPGTargetMarkerWidget::StaticClass();
    }
    TargetMarkerWidget = PC ? CreateWidget<UARPGTargetMarkerWidget>(PC, MarkerClass) : nullptr;
    if (TargetMarkerWidget)
    {
        TargetMarkerComponent->SetWidget(TargetMarkerWidget);
        UTexture2D* Texture = TargetMarkerTexture.IsNull() ? nullptr : TargetMarkerTexture.LoadSynchronous();
        UMaterialInterface* Material = TargetMarkerMaterial.IsNull() ? nullptr : TargetMarkerMaterial.LoadSynchronous();
        TargetMarkerWidget->ConfigureMarker(Texture, Material, TargetMarkerColor,
                                            bAnimateMarkerOnAcquire, MarkerAcquireAnimationDuration,
                                            bPulseMarkerWhileLocked, MarkerPulseSpeed, MarkerPulseScale,
                                            MarkerReleaseAnimationDuration);
    }
}

void UARPGTargetingComponent::DestroyTargetMarker(bool bPlayReleaseAnimation)
{
    if (!TargetMarkerComponent) { TargetMarkerWidget = nullptr; return; }

    UWidgetComponent* ComponentToDestroy = TargetMarkerComponent;
    UARPGTargetMarkerWidget* WidgetToRelease = TargetMarkerWidget;
    TargetMarkerComponent = nullptr;
    TargetMarkerWidget = nullptr;

    if (bPlayReleaseAnimation && WidgetToRelease && GetWorld())
    {
        WidgetToRelease->PlayReleaseAnimation();
        TWeakObjectPtr<UWidgetComponent> WeakComponent(ComponentToDestroy);
        FTimerDelegate DestroyDelegate;
        DestroyDelegate.BindLambda([WeakComponent]()
        {
            if (WeakComponent.IsValid()) WeakComponent->DestroyComponent();
        });
        FTimerHandle TempHandle;
        GetWorld()->GetTimerManager().SetTimer(TempHandle, DestroyDelegate, FMath::Max(0.01f, MarkerReleaseAnimationDuration), false);
        return;
    }

    ComponentToDestroy->DestroyComponent();
}

void UARPGTargetingComponent::UpdateTargetMarkerLocation()
{
    if (!TargetMarkerComponent || !CurrentTarget) return;
    FVector MarkerLocation = FVector::ZeroVector;
    bool bUsedSocket = false;
    if (!TargetMarkerSocketName.IsNone())
    {
        if (const ACharacter* Character = Cast<ACharacter>(CurrentTarget))
        {
            if (const USkeletalMeshComponent* Mesh = Character->GetMesh())
            {
                if (Mesh->DoesSocketExist(TargetMarkerSocketName))
                {
                    MarkerLocation = Mesh->GetSocketLocation(TargetMarkerSocketName) + FVector(0.f, 0.f, TargetMarkerHeightOffset);
                    bUsedSocket = true;
                }
            }
        }
    }
    if (!bUsedSocket)
    {
        FVector Origin, Extent;
        CurrentTarget->GetActorBounds(true, Origin, Extent, false);
        MarkerLocation = Origin + FVector(0.f, 0.f, Extent.Z + TargetMarkerHeightOffset);
    }
    TargetMarkerComponent->SetWorldLocation(MarkerLocation);
}

void UARPGTargetingComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    if (!bEnabled || !GetOwner()) return;

    // UI acquisition/maintenance is intentionally local to the owning player. The selected target itself is mirrored to the server through CombatTarget.
    if (IsLocallyControlledPlayer())
    {
        ValidateCurrentTarget(DeltaTime);
        UpdateTargetMarkerLocation();
        UpdateCameraLock(DeltaTime);
        UpdateFacing(DeltaTime);
    }
    else if (GetOwner()->HasAuthority())
    {
        // Server keeps action-facing authoritative when a player attack/ability requests it.
        UpdateFacing(DeltaTime);
    }
}
