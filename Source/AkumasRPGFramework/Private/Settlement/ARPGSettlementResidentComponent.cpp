#include "Settlement/ARPGSettlementResidentComponent.h"

#include "AIController.h"
#include "AkumasRPGFramework.h"
#include "Actors/ARPGCharacter.h"
#include "Actors/ARPGAICharacter.h"
#include "Animation/AnimMontage.h"
#include "Components/ARPGAICombatComponent.h"
#include "Components/ARPGFactionComponent.h"
#include "Components/ARPGEquipmentComponent.h"
#include "Components/ARPGInventoryComponent.h"
#include "Components/ARPGWandererComponent.h"
#include "Data/ARPGItemDefinition.h"
#include "Equipment/ARPGEquipmentVisualActor.h"
#include "Data/ARPGSettlementDefinition.h"
#include "Gathering/ARPGTree.h"
#include "GameFramework/Character.h"
#include "Net/UnrealNetwork.h"
#include "NavigationSystem.h"
#include "Navigation/PathFollowingComponent.h"
#include "Settlement/ARPGBuildBedActor.h"
#include "Settlement/ARPGSettlementHubActor.h"
#include "Settlement/ARPGSettlementVillagerCharacter.h"
#include "TimerManager.h"
#include "EngineUtils.h"

namespace
{
    const FName SettlementWorkPauseReason(TEXT("SettlementWork"));
    constexpr float SettlementWorkMovementProofDistance = 8.f;
    constexpr float SettlementWorkMovementProofInterval = 0.20f;
    constexpr int32 SettlementWorkMovementProofMaxChecks = 10;

    static AAIController* ResolveSettlementAIController(AActor* Owner)
    {
        APawn* Pawn = Cast<APawn>(Owner);
        if (!Pawn) return nullptr;
        AAIController* AI = Cast<AAIController>(Pawn->GetController());
        if (!AI && Pawn->HasAuthority())
        {
            // Settlement residents are autonomous framework AI. Blueprint subclasses should normally
            // inherit AutoPossessAI, but recover safely if a child Blueprint changed that setting.
            Pawn->SpawnDefaultController();
            AI = Cast<AAIController>(Pawn->GetController());
        }
        return AI;
    }
}

UARPGSettlementResidentComponent::UARPGSettlementResidentComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(true);
}

void UARPGSettlementResidentComponent::BeginPlay()
{
    Super::BeginPlay();
    // Cosmetic work tools are reconstructed entirely from replicated resident state + Settlement Definition.
    // Do this locally on every net role; dedicated servers intentionally create no presentation actor.
    RefreshWoodcuttingToolVisual();
    if (GetOwner() && GetOwner()->HasAuthority() && SettlementHub) StartRuntime();
}

void UARPGSettlementResidentComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    StopRuntime();
    Super::EndPlay(EndPlayReason);
}

bool UARPGSettlementResidentComponent::InitializeSettlementResident(AARPGSettlementHubActor* Hub, AARPGBuildBedActor* Bed, FGuid InResidentId)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || !Hub) return false;
    SettlementHub = Hub;
    ResidentId = InResidentId.IsValid() ? InResidentId : FGuid::NewGuid();
    AssignedBed = nullptr;
    if (Bed) AssignBed(Bed);

    if (AARPGAICharacter* AI = Cast<AARPGAICharacter>(GetOwner()))
    {
        if (AI->AIWanderer)
        {
            if (const UARPGSettlementDefinition* Def = Hub->GetSettlementDefinition())
            {
                AI->AIWanderer->WanderRadius = FMath::Max(100.f, Def->VillagerWanderRadius);
                AI->AIWanderer->ThinkInterval = FMath::Max(0.2f, Def->VillagerThinkInterval);
            }
            AI->AIWanderer->bStayNearHome = true;
            AI->AIWanderer->SetHomeLocation(ResolveHomeLocation());
            AI->AIWanderer->SetWandererEnabled(true);
        }
        if (AI->AICombat)
        {
            AI->AICombat->HomeLocation = ResolveHomeLocation();
            AI->AICombat->bUseHomeLeash = true;
        }
    }

    SetResidentState(AssignedBed ? EARPGSettlementResidentState::AtHome : EARPGSettlementResidentState::Homeless);
    StartRuntime();
    GetOwner()->ForceNetUpdate();
    return true;
}

void UARPGSettlementResidentComponent::RestoreResidentLinks(AARPGSettlementHubActor* Hub, AARPGBuildBedActor* Bed, FGuid InResidentId)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || !Hub) return;
    SettlementHub = Hub;
    ResidentId = InResidentId.IsValid() ? InResidentId : FGuid::NewGuid();
    AssignedBed = Bed;
    if (AssignedBed && AssignedBed->BedRole == EARPGBedRole::Villager) AssignedBed->AssignResident(ResidentId);
    if (AARPGAICharacter* AI = Cast<AARPGAICharacter>(GetOwner()))
    {
        if (AI->AIWanderer)
        {
            if (const UARPGSettlementDefinition* Def = Hub->GetSettlementDefinition())
            {
                AI->AIWanderer->WanderRadius = FMath::Max(100.f, Def->VillagerWanderRadius);
                AI->AIWanderer->ThinkInterval = FMath::Max(0.2f, Def->VillagerThinkInterval);
            }
            AI->AIWanderer->bStayNearHome = true;
            AI->AIWanderer->SetHomeLocation(ResolveHomeLocation());
            AI->AIWanderer->SetWandererEnabled(true);
        }
        if (AI->AICombat)
        {
            AI->AICombat->HomeLocation = ResolveHomeLocation();
            AI->AICombat->bUseHomeLeash = true;
        }
    }
    RepairInvalidHomeStoryPosition();
    SetResidentState(AssignedBed ? EARPGSettlementResidentState::AtHome : EARPGSettlementResidentState::Homeless);
    StartRuntime();
    GetOwner()->ForceNetUpdate();
}

bool UARPGSettlementResidentComponent::AssignBed(AARPGBuildBedActor* NewBed)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || !NewBed || !SettlementHub) return false;
    if (NewBed->BedRole != EARPGBedRole::Villager || !SettlementHub->CanManageBuilding(NewBed)) return false;
    if (NewBed->AssignedResidentId.IsValid() && NewBed->AssignedResidentId != ResidentId) return false;

    if (AssignedBed && AssignedBed != NewBed) AssignedBed->ClearResidentAssignment(ResidentId);
    AssignedBed = NewBed;
    AssignedBed->AssignResident(ResidentId);
    if (AARPGAICharacter* AI = Cast<AARPGAICharacter>(GetOwner()))
    {
        if (AI->AIWanderer) AI->AIWanderer->SetHomeLocation(ResolveHomeLocation());
        if (AI->AICombat) AI->AICombat->HomeLocation = ResolveHomeLocation();
    }
    RepairInvalidHomeStoryPosition();
    OnResidentBedChanged.Broadcast(AssignedBed);
    GetOwner()->ForceNetUpdate();
    return true;
}

void UARPGSettlementResidentComponent::ClearBed()
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;
    if (AssignedBed) AssignedBed->ClearResidentAssignment(ResidentId);
    AssignedBed = nullptr;
    OnResidentBedChanged.Broadcast(nullptr);
    SetResidentState(EARPGSettlementResidentState::Homeless);
    if (AARPGAICharacter* AI = Cast<AARPGAICharacter>(GetOwner()))
        if (AI->AIWanderer) AI->AIWanderer->SetHomeLocation(ResolveHomeLocation());
    GetOwner()->ForceNetUpdate();
}

bool UARPGSettlementResidentComponent::HasValidHome() const
{
    if (!SettlementHub || !AssignedBed || AssignedBed->BedRole != EARPGBedRole::Villager || AssignedBed->AssignedResidentId != ResidentId) return false;
    FARPGSettlementHomeValidation Validation;
    return SettlementHub->ValidateHomeForBed(AssignedBed, Validation) && Validation.bValid;
}

bool UARPGSettlementResidentComponent::CanBypassTreeRequirements(const AARPGTree* Tree) const
{
    if (!Tree || !SettlementHub || !SettlementHub->IsSettlementOperational()) return false;
    const UARPGSettlementDefinition* Def = SettlementHub->GetSettlementDefinition();
    return Def && Def->bEnableVillagerWoodcutting && Def->bVillagersIgnoreTreeSkillAndToolRequirements;
}

void UARPGSettlementResidentComponent::StartRuntime()
{
    if (!GetWorld() || !GetOwner() || !GetOwner()->HasAuthority() || !SettlementHub) return;
    const UARPGSettlementDefinition* Def = SettlementHub->GetSettlementDefinition();
    const float Interval = Def ? FMath::Max(0.5f, Def->VillagerThinkInterval) : 3.f;
    GetWorld()->GetTimerManager().ClearTimer(ActivityTimer);
    GetWorld()->GetTimerManager().SetTimer(ActivityTimer, this, &UARPGSettlementResidentComponent::ThinkAuthority, Interval, true, FMath::FRandRange(0.1f, Interval));
}

void UARPGSettlementResidentComponent::StopRuntime()
{
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(ActivityTimer);
        GetWorld()->GetTimerManager().ClearTimer(ChopTimer);
        GetWorld()->GetTimerManager().ClearTimer(WorkMoveProofTimer);
    }
    CurrentWorkTree = nullptr;
    WorkMoveProofChecks = 0;
    SetWoodcuttingToolVisualActive(false, false);
    if (AARPGAICharacter* AI = Cast<AARPGAICharacter>(GetOwner()))
        if (AI->AIWanderer) AI->AIWanderer->ReleaseMovementPause(SettlementWorkPauseReason, false);
}

void UARPGSettlementResidentComponent::ForceChooseNewActivity()
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;
    StopWoodcutting(false);
    ThinkAuthority();
}

void UARPGSettlementResidentComponent::ReturnHome()
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;
    StopWoodcutting(false);
    const FVector Home = ResolveHomeLocation();
    if (MoveToLocation(Home, 90.f))
        SetResidentState(AssignedBed ? EARPGSettlementResidentState::ReturningHome : EARPGSettlementResidentState::Homeless);
    else
        SetResidentState(AssignedBed ? EARPGSettlementResidentState::Roaming : EARPGSettlementResidentState::Homeless);
}

void UARPGSettlementResidentComponent::ThinkAuthority()
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;
    if (!SettlementHub || !SettlementHub->IsSettlementOperational())
    {
        StopWoodcutting(false);
        SetResidentState(EARPGSettlementResidentState::Homeless);
        return;
    }

    if (const AARPGAICharacter* AI = Cast<AARPGAICharacter>(GetOwner()))
        if (AI->AICombat && AI->AICombat->CurrentTarget) return;

    if (AssignedBed && (AssignedBed->BedRole != EARPGBedRole::Villager || AssignedBed->AssignedResidentId != ResidentId || !SettlementHub->CanManageBuilding(AssignedBed)))
        ClearBed();

    if (CurrentWorkTree)
    {
        if (!CurrentWorkTree->IsStanding())
        {
            // A build piece can intentionally suppress a tree without harvesting it. Do not interpret
            // that environmental removal as a fell/reward event or sweep unrelated carried logs into
            // the Hub merely because a Foundation replaced the resource location.
            if (!CurrentWorkTree->IsRespawnSuppressedByBuilding()) DepositTreeRewardsToHub(CurrentWorkTree);
            StopWoodcutting(true);
            return;
        }
        const UARPGSettlementDefinition* Def = SettlementHub->GetSettlementDefinition();
        const float Acceptance = Def ? FMath::Max(25.f, Def->WoodcuttingAcceptanceRadius) : 140.f;
        if (FVector::DistSquared2D(GetOwner()->GetActorLocation(), CurrentWorkTree->GetActorLocation()) > FMath::Square(Acceptance + 50.f))
        {
            if (MoveToActor(CurrentWorkTree, Acceptance))
            {
                SetResidentState(EARPGSettlementResidentState::GoingToWork);
                StartWorkMovementProof();
            }
            else
            {
                StopWoodcutting(true);
            }
        }
        else if (!GetWorld()->GetTimerManager().IsTimerActive(ChopTimer))
        {
            BeginChoppingTree(CurrentWorkTree);
        }
        return;
    }

    const UARPGSettlementDefinition* Def = SettlementHub->GetSettlementDefinition();
    if (Def && Def->bEnableVillagerWoodcutting && GetWorld()->GetTimeSeconds() >= NextTreeSearchTime &&
        FMath::FRand() <= FMath::Clamp(Def->WoodcuttingDutyChance, 0.f, 1.f) && SettlementHub->CanResidentStartWoodcutting(this))
    {
        NextTreeSearchTime = GetWorld()->GetTimeSeconds() + FMath::Max(0.5f, Def->WoodcuttingSearchInterval);
        if (TryBeginWoodcutting()) return;
    }

    if (AARPGAICharacter* AI = Cast<AARPGAICharacter>(GetOwner()))
    {
        if (AI->AIWanderer)
        {
            AI->AIWanderer->ReleaseMovementPause(SettlementWorkPauseReason, true);
            AI->AIWanderer->SetHomeLocation(ResolveHomeLocation());
            AI->AIWanderer->SetWandererEnabled(true);
        }
    }
    SetResidentState(AssignedBed ? EARPGSettlementResidentState::Roaming : EARPGSettlementResidentState::Homeless);
}

bool UARPGSettlementResidentComponent::TryBeginWoodcutting()
{
    AARPGTree* Tree = FindBestWorkTree();
    if (!Tree) return false;
    CurrentWorkTree = Tree;
    if (AARPGAICharacter* AI = Cast<AARPGAICharacter>(GetOwner()))
        if (AI->AIWanderer) AI->AIWanderer->AcquireMovementPause(SettlementWorkPauseReason, true);

    const UARPGSettlementDefinition* Def = SettlementHub ? SettlementHub->GetSettlementDefinition() : nullptr;
    const float Acceptance = Def ? FMath::Max(25.f, Def->WoodcuttingAcceptanceRadius) : 140.f;
    if (FVector::DistSquared2D(GetOwner()->GetActorLocation(), Tree->GetActorLocation()) <= FMath::Square(Acceptance + 50.f))
    {
        BeginChoppingTree(Tree);
        GetOwner()->ForceNetUpdate();
        return true;
    }

    // A resident does not claim the GoingToWork state unless navigation actually accepts a request.
    // This prevents a missing controller/NavMesh/unreachable target from creating a permanently stuck worker.
    if (!MoveToActor(Tree, Acceptance))
    {
        StopWoodcutting(true);
        return false;
    }

    SetResidentState(EARPGSettlementResidentState::GoingToWork);
    StartWorkMovementProof();
    GetOwner()->ForceNetUpdate();
    return true;
}

AARPGTree* UARPGSettlementResidentComponent::FindBestWorkTree() const
{
    if (!GetWorld() || !SettlementHub) return nullptr;
    const UARPGSettlementDefinition* Def = SettlementHub->GetSettlementDefinition();
    if (!Def) return nullptr;
    const float RadiusSq = FMath::Square(FMath::Max(100.f, Def->WoodcuttingRadius));
    AARPGTree* Best = nullptr;
    float BestDistSq = TNumericLimits<float>::Max();
    TArray<AARPGSettlementVillagerCharacter*> OtherResidents;
    SettlementHub->GetSettlementResidents(OtherResidents);

    for (TActorIterator<AARPGTree> It(GetWorld()); It; ++It)
    {
        AARPGTree* Tree = *It;
        if (!Tree || !Tree->IsStanding()) continue;
        const float HubDistSq = FVector::DistSquared2D(Tree->GetActorLocation(), SettlementHub->GetActorLocation());
        if (HubDistSq > RadiusSq) continue;
        bool bReserved = false;
        for (AARPGSettlementVillagerCharacter* Other : OtherResidents)
        {
            if (!Other || Other == GetOwner()) continue;
            if (Other->SettlementResident && Other->SettlementResident->CurrentWorkTree == Tree) { bReserved = true; break; }
        }
        if (bReserved) continue;
        FText Reason;
        if (!Tree->CanBeChoppedBy(GetOwner(), Reason)) continue;
        const float DistSq = FVector::DistSquared2D(Tree->GetActorLocation(), GetOwner()->GetActorLocation());
        if (DistSq < BestDistSq) { Best = Tree; BestDistSq = DistSq; }
    }
    return Best;
}

bool UARPGSettlementResidentComponent::MoveToLocation(const FVector& Location, float AcceptanceRadius)
{
    AAIController* AI = ResolveSettlementAIController(GetOwner());
    UNavigationSystemV1* Nav = GetWorld() ? FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()) : nullptr;
    if (!AI || !Nav) return false;

    FNavLocation Projected;
    const FVector QueryExtent(250.f, 250.f, 350.f);
    if (!Nav->ProjectPointToNavigation(Location, Projected, QueryExtent)) return false;
    const EPathFollowingRequestResult::Type Result = AI->MoveToLocation(
        Projected.Location, FMath::Max(5.f, AcceptanceRadius), true, true, true, false, nullptr, true);
    return Result != EPathFollowingRequestResult::Failed;
}

bool UARPGSettlementResidentComponent::MoveToActor(AActor* Actor, float AcceptanceRadius)
{
    if (!Actor) return false;
    AAIController* AI = ResolveSettlementAIController(GetOwner());
    UNavigationSystemV1* Nav = GetWorld() ? FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()) : nullptr;
    if (!AI || !Nav) return false;

    // Trees frequently carve navigation around their trunk. Move toward the nearest navigable point
    // around the target instead of demanding that the tree actor origin itself lie on NavMesh.
    const float SafeAcceptance = FMath::Max(25.f, AcceptanceRadius);
    FNavLocation Projected;
    const float HorizontalExtent = FMath::Max(250.f, SafeAcceptance + 160.f);
    if (!Nav->ProjectPointToNavigation(Actor->GetActorLocation(), Projected, FVector(HorizontalExtent, HorizontalExtent, 450.f)))
        return false;

    const EPathFollowingRequestResult::Type Result = AI->MoveToLocation(
        Projected.Location, SafeAcceptance, true, true, true, false, nullptr, true);
    return Result != EPathFollowingRequestResult::Failed;
}

void UARPGSettlementResidentComponent::StartWorkMovementProof()
{
    if (!GetWorld() || !GetOwner()) return;
    WorkMoveProofChecks = 0;
    WorkMoveProofStartLocation = GetOwner()->GetActorLocation();
    GetWorld()->GetTimerManager().ClearTimer(WorkMoveProofTimer);
    GetWorld()->GetTimerManager().SetTimer(
        WorkMoveProofTimer, this, &UARPGSettlementResidentComponent::VerifyWorkMovement,
        SettlementWorkMovementProofInterval, false);
}

void UARPGSettlementResidentComponent::CancelWorkMovementProof()
{
    if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(WorkMoveProofTimer);
    WorkMoveProofChecks = 0;
}

void UARPGSettlementResidentComponent::VerifyWorkMovement()
{
    if (!GetWorld() || !GetOwner() || !GetOwner()->HasAuthority() || !CurrentWorkTree ||
        ResidentState != EARPGSettlementResidentState::GoingToWork)
    {
        CancelWorkMovementProof();
        return;
    }

    const UARPGSettlementDefinition* Def = SettlementHub ? SettlementHub->GetSettlementDefinition() : nullptr;
    const float Acceptance = Def ? FMath::Max(25.f, Def->WoodcuttingAcceptanceRadius) : 140.f;
    if (FVector::DistSquared2D(GetOwner()->GetActorLocation(), CurrentWorkTree->GetActorLocation()) <= FMath::Square(Acceptance + 50.f))
    {
        CancelWorkMovementProof();
        if (!GetWorld()->GetTimerManager().IsTimerActive(ChopTimer)) BeginChoppingTree(CurrentWorkTree);
        return;
    }

    if (FVector::Dist2D(GetOwner()->GetActorLocation(), WorkMoveProofStartLocation) >= SettlementWorkMovementProofDistance)
    {
        // Real translation is established. The normal resident activity timer now owns arrival checking.
        CancelWorkMovementProof();
        return;
    }

    AAIController* AI = ResolveSettlementAIController(GetOwner());
    ++WorkMoveProofChecks;
    if (AI && AI->GetMoveStatus() == EPathFollowingStatus::Moving && WorkMoveProofChecks < SettlementWorkMovementProofMaxChecks)
    {
        GetWorld()->GetTimerManager().SetTimer(
            WorkMoveProofTimer, this, &UARPGSettlementResidentComponent::VerifyWorkMovement,
            SettlementWorkMovementProofInterval, false);
        return;
    }

    // Accepted-but-motionless path requests are not valid work assignments. Release the reservation
    // and restore normal roaming; a later activity pass may choose another reachable tree.
    if (AI) AI->StopMovement();
    UE_LOG(LogARPG, Warning, TEXT("Settlement villager %s abandoned an unreachable/stalled woodcutting route to %s."),
        *GetNameSafe(GetOwner()), *GetNameSafe(CurrentWorkTree));
    CancelWorkMovementProof();
    StopWoodcutting(true);
}

void UARPGSettlementResidentComponent::BeginChoppingTree(AARPGTree* Tree)
{
    if (!GetWorld() || !Tree || !SettlementHub) return;
    const UARPGSettlementDefinition* Def = SettlementHub->GetSettlementDefinition();
    SetResidentState(EARPGSettlementResidentState::Woodcutting);
    if (Def)
        if (UAnimMontage* Montage = Def->VillagerChopMontage.LoadSynchronous()) MulticastPlayWoodcuttingMontage(Montage);
    const float Interval = Def ? FMath::Max(0.2f, Def->VillagerChopInterval) : 1.4f;
    GetWorld()->GetTimerManager().SetTimer(ChopTimer, this, &UARPGSettlementResidentComponent::PerformChopAuthority, Interval, true, 0.05f);
}

void UARPGSettlementResidentComponent::PerformChopAuthority()
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || !SettlementHub || !CurrentWorkTree || !CurrentWorkTree->IsStanding())
    {
        if (CurrentWorkTree && !CurrentWorkTree->IsRespawnSuppressedByBuilding()) DepositTreeRewardsToHub(CurrentWorkTree);
        StopWoodcutting(true);
        return;
    }
    const UARPGSettlementDefinition* Def = SettlementHub->GetSettlementDefinition();
    const float Acceptance = Def ? FMath::Max(25.f, Def->WoodcuttingAcceptanceRadius) : 140.f;
    if (FVector::DistSquared2D(GetOwner()->GetActorLocation(), CurrentWorkTree->GetActorLocation()) > FMath::Square(Acceptance + 60.f))
    {
        GetWorld()->GetTimerManager().ClearTimer(ChopTimer);
        if (MoveToActor(CurrentWorkTree, Acceptance))
        {
            SetResidentState(EARPGSettlementResidentState::GoingToWork);
            StartWorkMovementProof();
        }
        else
        {
            StopWoodcutting(true);
        }
        return;
    }

    if (Def)
        if (UAnimMontage* Montage = Def->VillagerChopMontage.LoadSynchronous()) MulticastPlayWoodcuttingMontage(Montage);
    const float Power = Def ? FMath::Max(1.f, Def->VillagerChopPower) : 25.f;
    AARPGTree* Tree = CurrentWorkTree;
    if (!Tree->ApplyChop(GetOwner(), Power))
    {
        StopWoodcutting(true);
        return;
    }
    if (!Tree->IsStanding())
    {
        DepositTreeRewardsToHub(Tree);
        StopWoodcutting(true);
    }
}

void UARPGSettlementResidentComponent::StopWoodcutting(bool bReturnToRoam)
{
    if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(ChopTimer);
    CancelWorkMovementProof();
    CurrentWorkTree = nullptr;
    RefreshWoodcuttingToolVisual();
    if (AARPGAICharacter* AI = Cast<AARPGAICharacter>(GetOwner()))
        if (AI->AIWanderer) AI->AIWanderer->ReleaseMovementPause(SettlementWorkPauseReason, bReturnToRoam);
    if (bReturnToRoam) SetResidentState(AssignedBed ? EARPGSettlementResidentState::Roaming : EARPGSettlementResidentState::Homeless);
    if (GetOwner()) GetOwner()->ForceNetUpdate();
}

void UARPGSettlementResidentComponent::DepositTreeRewardsToHub(AARPGTree* Tree)
{
    if (!Tree || !SettlementHub || !SettlementHub->Inventory) return;
    const UARPGSettlementDefinition* Def = SettlementHub->GetSettlementDefinition();
    if (!Def || !Def->bDepositWoodcuttingRewardsToHub) return;
    UARPGInventoryComponent* Source = GetOwner() ? GetOwner()->FindComponentByClass<UARPGInventoryComponent>() : nullptr;
    if (!Source) return;

    auto TransferDefinition = [&](const UARPGItemDefinition* Item)
    {
        if (!Item) return;
        const FName Id = Item->DefinitionId.IsNone() ? Item->GetFName() : Item->DefinitionId;
        const int32 Count = Source->GetItemCount(Id);
        if (Count > 0) Source->TransferItemTo(SettlementHub->Inventory, Id, Count);
    };
    TransferDefinition(Tree->WoodItem);
    for (const FARPGTreeBonusDrop& Drop : Tree->BonusDrops) TransferDefinition(Drop.Item);
}

bool UARPGSettlementResidentComponent::IsWoodcuttingToolVisualActive() const
{
    return IsValid(ActiveWoodcuttingToolVisual);
}

bool UARPGSettlementResidentComponent::ShouldDisplayWoodcuttingTool() const
{
    if (!SettlementHub || !CurrentWorkTree) return false;
    const UARPGSettlementDefinition* Def = SettlementHub->GetSettlementDefinition();
    if (!Def || Def->VillagerWoodcuttingToolItem.IsNull()) return false;
    if (ResidentState == EARPGSettlementResidentState::Woodcutting) return true;
    return Def->bShowWoodcuttingToolWhileGoingToWork && ResidentState == EARPGSettlementResidentState::GoingToWork;
}

UARPGItemDefinition* UARPGSettlementResidentComponent::ResolveWoodcuttingToolDefinition() const
{
    if (!SettlementHub) return nullptr;
    const UARPGSettlementDefinition* Def = SettlementHub->GetSettlementDefinition();
    if (!Def || Def->VillagerWoodcuttingToolItem.IsNull()) return nullptr;
    return Def->VillagerWoodcuttingToolItem.LoadSynchronous();
}

void UARPGSettlementResidentComponent::SetWoodcuttingToolVisualActive(bool bActive, bool bAllowTransitionPresentation)
{
    AARPGCharacter* Character = Cast<AARPGCharacter>(GetOwner());
    UARPGEquipmentComponent* Equipment = Character ? Character->Equipment : nullptr;
    UARPGItemDefinition* DesiredDefinition = bActive ? ResolveWoodcuttingToolDefinition() : nullptr;
    const UARPGSettlementDefinition* SettlementDef = SettlementHub ? SettlementHub->GetSettlementDefinition() : nullptr;
    const bool bPlayPresentation = bAllowTransitionPresentation && SettlementDef && SettlementDef->bPlayWoodcuttingToolEquipPresentation;

    // If the contextual tool changed while active, tear down the old local visual first so only one physical
    // work tool can own the hand socket. This does not touch Inventory/equipment state or gameplay effects.
    if (IsValid(ActiveWoodcuttingToolVisual) && (!bActive || ActiveWoodcuttingToolDefinition != DesiredDefinition))
    {
        if (Equipment)
            Equipment->DestroyTransientEquipmentVisual(ActiveWoodcuttingToolVisual, ActiveWoodcuttingToolDefinition, bPlayPresentation);
        else
            ActiveWoodcuttingToolVisual->Destroy();
        ActiveWoodcuttingToolVisual = nullptr;
        ActiveWoodcuttingToolDefinition = nullptr;
        OnWoodcuttingToolVisualChanged.Broadcast(false, nullptr);
    }

    if (!bActive || !DesiredDefinition || IsValid(ActiveWoodcuttingToolVisual)) return;
    if (!Equipment) return;

    AARPGEquipmentVisualActor* Visual = Equipment->CreateTransientEquipmentVisual(DesiredDefinition, bPlayPresentation);
    if (!Visual) return;
    ActiveWoodcuttingToolVisual = Visual;
    ActiveWoodcuttingToolDefinition = DesiredDefinition;
    OnWoodcuttingToolVisualChanged.Broadcast(true, Visual);
}

void UARPGSettlementResidentComponent::RefreshWoodcuttingToolVisual()
{
    SetWoodcuttingToolVisualActive(ShouldDisplayWoodcuttingTool(), true);
}

FVector UARPGSettlementResidentComponent::ResolveHomeLocation() const
{
    if (SettlementHub && AssignedBed)
    {
        FVector InteriorAnchor;
        if (SettlementHub->ResolveResidentHomeAnchor(AssignedBed, InteriorAnchor)) return InteriorAnchor;
    }
    if (AssignedBed) return AssignedBed->GetActorLocation();
    if (SettlementHub) return SettlementHub->GetActorLocation();
    return GetOwner() ? GetOwner()->GetActorLocation() : FVector::ZeroVector;
}

void UARPGSettlementResidentComponent::RepairInvalidHomeStoryPosition()
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || !SettlementHub || !AssignedBed) return;

    FVector InteriorAnchor;
    if (!SettlementHub->ResolveResidentHomeAnchor(AssignedBed, InteriorAnchor)) return;

    FARPGSettlementHomeValidation Validation;
    if (!SettlementHub->ValidateHomeForBed(AssignedBed, Validation) || !Validation.bValid) return;
    const UARPGSettlementDefinition* Def = SettlementHub->GetSettlementDefinition();
    const float Story = FMath::Max(50.f, Def ? Def->HomeStoryHeight : 300.f);
    const float Tol = FMath::Max(24.f, Def ? Def->HomeGridTolerance * 2.f : 48.f);
    const float FloorZ = Validation.HomeCenter.Z - Validation.HomeExtent.Z;
    const FVector Current = GetOwner()->GetActorLocation();
    const float HomeRadius = FVector2D(Validation.HomeExtent.X, Validation.HomeExtent.Y).Size() + Tol;

    // v2.16.4 could save a resident after collision adjustment pushed the capsule onto the house roof.
    // Only repair positions horizontally over the assigned home and at least half a story above its
    // walkable floor; legitimate residents elsewhere in the settlement keep their saved transform.
    const bool bOverAssignedHome = FVector::DistSquared2D(Current, Validation.HomeCenter) <= FMath::Square(HomeRadius);
    const bool bOnUpperStorySurface = Current.Z > FloorZ + Story * 0.50f;
    if (!bOverAssignedHome || !bOnUpperStorySurface) return;

    if (AAIController* AI = ResolveSettlementAIController(GetOwner())) AI->StopMovement();
    GetOwner()->SetActorLocation(InteriorAnchor, false, nullptr, ETeleportType::TeleportPhysics);
    UE_LOG(LogARPG, Warning, TEXT("Settlement resident %s was recovered from an invalid upper/roof position above Bed %s and returned to the validated interior home story."),
        *GetNameSafe(GetOwner()), *GetNameSafe(AssignedBed));
}

void UARPGSettlementResidentComponent::SetResidentState(EARPGSettlementResidentState NewState)
{
    if (ResidentState == NewState) return;
    ResidentState = NewState;
    RefreshWoodcuttingToolVisual();
    OnResidentStateChanged.Broadcast(ResidentState);
    if (SettlementHub)
        if (AARPGSettlementVillagerCharacter* Resident = Cast<AARPGSettlementVillagerCharacter>(GetOwner())) SettlementHub->NotifyResidentStateChanged(Resident);
    if (GetOwner()) GetOwner()->ForceNetUpdate();
}

void UARPGSettlementResidentComponent::MulticastPlayWoodcuttingMontage_Implementation(UAnimMontage* Montage)
{
    if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
        if (Montage) Character->PlayAnimMontage(Montage);
}

void UARPGSettlementResidentComponent::OnRep_Hub() { RefreshWoodcuttingToolVisual(); }
void UARPGSettlementResidentComponent::OnRep_Bed() { OnResidentBedChanged.Broadcast(AssignedBed); }
void UARPGSettlementResidentComponent::OnRep_State()
{
    RefreshWoodcuttingToolVisual();
    OnResidentStateChanged.Broadcast(ResidentState);
}
void UARPGSettlementResidentComponent::OnRep_CurrentWorkTree() { RefreshWoodcuttingToolVisual(); }

void UARPGSettlementResidentComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(UARPGSettlementResidentComponent, ResidentId);
    DOREPLIFETIME(UARPGSettlementResidentComponent, SettlementHub);
    DOREPLIFETIME(UARPGSettlementResidentComponent, AssignedBed);
    DOREPLIFETIME(UARPGSettlementResidentComponent, ResidentState);
    DOREPLIFETIME(UARPGSettlementResidentComponent, CurrentWorkTree);
}
