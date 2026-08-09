#include "Building/ARPGBuildPieceActor.h"
#include "Data/ARPGBuildPieceDefinition.h"
#include "Components/ARPGFactionOwnershipComponent.h"
#include "Net/UnrealNetwork.h"

AARPGBuildPieceActor::AARPGBuildPieceActor()
{
    bReplicates = true; SetReplicateMovement(true);
    Ownership = CreateDefaultSubobject<UARPGFactionOwnershipComponent>(TEXT("Ownership"));
}

void AARPGBuildPieceActor::InitializeBuilding(UARPGBuildPieceDefinition* InDefinition, AActor* Builder)
{
    if (!HasAuthority() || !InDefinition) return;
    Definition = InDefinition;
    bRuntimePlaced = true;
    if (!BuildingId.IsValid()) BuildingId = FGuid::NewGuid();
    Health = GetMaxBuildingHealth();
    if (Ownership)
    {
        Ownership->bSameFactionCanUse = InDefinition->bSameFactionCanUse; Ownership->bAlliesCanUse = InDefinition->bAlliesCanUse;
        Ownership->bNeutralCanUse = InDefinition->bNeutralCanUse; Ownership->bHostilesCanUse = InDefinition->bHostilesCanUse;
        Ownership->bFactionMembersCanModify = InDefinition->bFactionMembersCanModify; Ownership->bHostilesCanDamage = InDefinition->bHostilesCanDamage;
        Ownership->InitializeFromActor(Builder, InDefinition->bInheritBuilderFaction);
    }
}

float AARPGBuildPieceActor::GetMaxBuildingHealth() const { return Definition ? FMath::Max(1.f, Definition->MaxHealth) : 100.f; }
bool AARPGBuildPieceActor::CanActorUse(AActor* Actor) const { return !Ownership || Ownership->CanActorUse(Actor); }
bool AARPGBuildPieceActor::CanActorModify(AActor* Actor) const { return !Ownership || Ownership->CanActorModify(Actor); }

bool AARPGBuildPieceActor::ApplyBuildingDamage(float Amount, AActor* DamageCauser)
{
    if (!HasAuthority() || Amount <= 0.f || Health <= 0.f || (DamageCauser && Ownership && !Ownership->CanActorDamage(DamageCauser))) return false;
    const float Old = Health; Health = FMath::Clamp(Health - Amount, 0.f, GetMaxBuildingHealth()); OnBuildingHealthChanged.Broadcast(Health, Health - Old);
    if (Health <= 0.f) { OnBuildingDestroyed.Broadcast(); Destroy(); } return true;
}

bool AARPGBuildPieceActor::RepairBuilding(float Amount, AActor* Repairer)
{
    if (!HasAuthority() || Amount <= 0.f || Health <= 0.f || (Repairer && Ownership && !Ownership->CanActorModify(Repairer))) return false;
    const float Old = Health; Health = FMath::Clamp(Health + Amount, 0.f, GetMaxBuildingHealth()); OnBuildingHealthChanged.Broadcast(Health, Health - Old); return Health > Old;
}

bool AARPGBuildPieceActor::Demolish(AActor* Requester)
{
    if (!HasAuthority() || !bAllowDemolish || (Requester && Ownership && !Ownership->CanActorModify(Requester))) return false;
    OnBuildingDestroyed.Broadcast(); Destroy(); return true;
}
void AARPGBuildPieceActor::OnRep_Definition() {}
void AARPGBuildPieceActor::OnRep_Health(float OldHealth) { OnBuildingHealthChanged.Broadcast(Health, Health - OldHealth); }
void AARPGBuildPieceActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps); DOREPLIFETIME(AARPGBuildPieceActor, BuildingId); DOREPLIFETIME(AARPGBuildPieceActor, Definition); DOREPLIFETIME(AARPGBuildPieceActor, Health); DOREPLIFETIME(AARPGBuildPieceActor, UpgradeLevel); DOREPLIFETIME(AARPGBuildPieceActor, bRuntimePlaced);
}
