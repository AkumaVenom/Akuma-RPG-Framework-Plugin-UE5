#include "Settlement/ARPGBuildBedActor.h"

#include "Actors/ARPGCharacter.h"
#include "Components/ARPGFactionOwnershipComponent.h"
#include "Data/ARPGBuildPieceDefinition.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"
#include "Settlement/ARPGSettlementHubActor.h"

AARPGBuildBedActor::AARPGBuildBedActor()
{
    bReplicates = true;
}

void AARPGBuildBedActor::InitializeBuilding(UARPGBuildPieceDefinition* InDefinition, AActor* Builder)
{
    Super::InitializeBuilding(InDefinition, Builder);
    if (!HasAuthority() || !InDefinition) return;
    BedRole = InDefinition->DefaultBedRole;
    AssignedResidentId.Invalidate();
    PlayerBedOwnerCharacterId.Invalidate();
    if (BedRole == EARPGBedRole::Player)
        if (const AARPGCharacter* Character = Cast<AARPGCharacter>(Builder)) PlayerBedOwnerCharacterId = Character->CharacterId;
    ForceNetUpdate();
}

AARPGSettlementHubActor* AARPGBuildBedActor::FindManagingSettlementHub() const
{
    if (!GetWorld() || !IsConstructionComplete()) return nullptr;
    AARPGSettlementHubActor* Best = nullptr;
    float BestDistSq = TNumericLimits<float>::Max();
    for (TActorIterator<AARPGSettlementHubActor> It(GetWorld()); It; ++It)
    {
        AARPGSettlementHubActor* Hub = *It;
        if (!Hub || !Hub->IsConstructionComplete() || !Hub->CanManageBuilding(this)) continue;
        const float Radius = Hub->GetSettlementRadius();
        const float DistSq = FVector::DistSquared2D(GetActorLocation(), Hub->GetActorLocation());
        if (DistSq > FMath::Square(Radius)) continue;
        const bool bCloser = DistSq < BestDistSq - 1.f;
        const bool bSameDistance = FMath::IsNearlyEqual(DistSq, BestDistSq, 1.f);
        const bool bStableTieBreak = bSameDistance && Best && Hub->BuildingId.IsValid() && Best->BuildingId.IsValid() &&
            Hub->BuildingId.ToString(EGuidFormats::Digits) < Best->BuildingId.ToString(EGuidFormats::Digits);
        if (!Best || bCloser || bStableTieBreak)
        {
            Best = Hub;
            BestDistSq = DistSq;
        }
    }
    return Best;
}

bool AARPGBuildBedActor::GetCurrentHomeValidation(FARPGSettlementHomeValidation& OutValidation) const
{
    if (AARPGSettlementHubActor* Hub = FindManagingSettlementHub())
        return Hub->ValidateHomeForBed(const_cast<AARPGBuildBedActor*>(this), OutValidation);
    OutValidation = FARPGSettlementHomeValidation();
    OutValidation.State = EARPGSettlementHomeState::NoSettlement;
    OutValidation.StatusText = FText::FromString(TEXT("No Settlement Hub manages this Bed."));
    return false;
}

bool AARPGBuildBedActor::SetBedRole(EARPGBedRole NewRole, AActor* Requester)
{
    if (!HasAuthority() || !IsConstructionComplete()) return false;
    if (Requester && !CanActorModify(Requester)) return false;

    BedRole = NewRole;
    if (NewRole == EARPGBedRole::Player)
    {
        AssignedResidentId.Invalidate();
        PlayerBedOwnerCharacterId.Invalidate();
        if (const AARPGCharacter* Character = Cast<AARPGCharacter>(Requester)) PlayerBedOwnerCharacterId = Character->CharacterId;
    }
    else
    {
        PlayerBedOwnerCharacterId.Invalidate();
        if (NewRole != EARPGBedRole::Villager) AssignedResidentId.Invalidate();
    }

    OnBedRoleChanged.Broadcast(BedRole);
    OnResidentAssignmentChanged.Broadcast(AssignedResidentId);
    ForceNetUpdate();
    if (AARPGSettlementHubActor* Hub = FindManagingSettlementHub()) Hub->RefreshSettlementNow();
    return true;
}

void AARPGBuildBedActor::AssignResident(FGuid ResidentId)
{
    if (!HasAuthority() || BedRole != EARPGBedRole::Villager) return;
    AssignedResidentId = ResidentId;
    OnResidentAssignmentChanged.Broadcast(AssignedResidentId);
    ForceNetUpdate();
}

void AARPGBuildBedActor::ClearResidentAssignment(FGuid ExpectedResidentId)
{
    if (!HasAuthority()) return;
    if (ExpectedResidentId.IsValid() && AssignedResidentId != ExpectedResidentId) return;
    AssignedResidentId.Invalidate();
    OnResidentAssignmentChanged.Broadcast(AssignedResidentId);
    ForceNetUpdate();
}

void AARPGBuildBedActor::RestoreBedState(EARPGBedRole SavedRole, FGuid SavedResidentId, FGuid SavedPlayerOwnerCharacterId)
{
    if (!HasAuthority()) return;
    BedRole = SavedRole;
    AssignedResidentId = SavedRole == EARPGBedRole::Villager ? SavedResidentId : FGuid();
    PlayerBedOwnerCharacterId = SavedRole == EARPGBedRole::Player ? SavedPlayerOwnerCharacterId : FGuid();
    OnBedRoleChanged.Broadcast(BedRole);
    OnResidentAssignmentChanged.Broadcast(AssignedResidentId);
    ForceNetUpdate();
}

void AARPGBuildBedActor::OnRep_BedRole()
{
    OnBedRoleChanged.Broadcast(BedRole);
}

void AARPGBuildBedActor::OnRep_Assignment()
{
    OnResidentAssignmentChanged.Broadcast(AssignedResidentId);
}

void AARPGBuildBedActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AARPGBuildBedActor, BedRole);
    DOREPLIFETIME(AARPGBuildBedActor, AssignedResidentId);
    DOREPLIFETIME(AARPGBuildBedActor, PlayerBedOwnerCharacterId);
}
