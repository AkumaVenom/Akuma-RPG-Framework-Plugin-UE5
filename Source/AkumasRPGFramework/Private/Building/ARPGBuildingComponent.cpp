#include "Building/ARPGBuildingComponent.h"
#include "Building/ARPGBuildPieceActor.h"
#include "Data/ARPGBuildPieceDefinition.h"
#include "Components/ARPGInventoryComponent.h"
#include "Components/ARPGFactionComponent.h"
#include "Components/ARPGEventRouterComponent.h"
#include "Building/ARPGFactionTerritoryVolume.h"
#include "EngineUtils.h"
#include "Engine/World.h"

UARPGBuildingComponent::UARPGBuildingComponent()
{
    SetIsReplicatedByDefault(true);
}

FTransform UARPGBuildingComponent::SnapTransform(const UARPGBuildPieceDefinition* Piece, const FTransform& DesiredTransform) const
{
    if (!Piece || !Piece->bSnapPlacement || Piece->SnapSize <= KINDA_SMALL_NUMBER) return DesiredTransform;
    FTransform Result = DesiredTransform;
    FVector L = Result.GetLocation();
    const float S = Piece->SnapSize;
    L.X = FMath::GridSnap(L.X, S);
    L.Y = FMath::GridSnap(L.Y, S);
    L.Z = FMath::GridSnap(L.Z, S);
    Result.SetLocation(L);
    FRotator R = Result.Rotator();
    R.Yaw = FMath::GridSnap(R.Yaw, 90.f);
    R.Pitch = 0.f;
    R.Roll = 0.f;
    Result.SetRotation(R.Quaternion());
    return Result;
}

bool UARPGBuildingComponent::HasBuildResources(const UARPGBuildPieceDefinition* Piece) const
{
    if (!bConsumeResources) return true;
    const UARPGInventoryComponent* Inventory = GetOwner() ? GetOwner()->FindComponentByClass<UARPGInventoryComponent>() : nullptr;
    if (!Inventory || !Piece) return false;
    for (const FARPGItemAmount& Cost : Piece->BuildCost)
        if (!Inventory->HasItem(Cost.ItemId, Cost.Quantity)) return false;
    return true;
}

bool UARPGBuildingComponent::ConsumeBuildResources(const UARPGBuildPieceDefinition* Piece)
{
    if (!bConsumeResources) return true;
    UARPGInventoryComponent* Inventory = GetOwner() ? GetOwner()->FindComponentByClass<UARPGInventoryComponent>() : nullptr;
    if (!Inventory || !Piece || !HasBuildResources(Piece)) return false;
    for (const FARPGItemAmount& Cost : Piece->BuildCost)
        if (!Inventory->RemoveItem(Cost.ItemId, Cost.Quantity)) return false;
    return true;
}

EARPGPlacementResult UARPGBuildingComponent::EvaluatePlacement(const UARPGBuildPieceDefinition* Piece, const FTransform& DesiredTransform) const
{
    if (!Piece) return EARPGPlacementResult::NoPiece;
    const AActor* Owner = GetOwner();
    UWorld* World = GetWorld();
    if (!Owner || !World) return EARPGPlacementResult::Restricted;
    const FTransform Final = SnapTransform(Piece, DesiredTransform);
    if (FVector::DistSquared(Owner->GetActorLocation(), Final.GetLocation()) > FMath::Square(MaxPlacementDistance)) return EARPGPlacementResult::TooFar;
    if (!HasBuildResources(Piece)) return EARPGPlacementResult::MissingResources;
    const UARPGFactionComponent* Faction = Owner->FindComponentByClass<UARPGFactionComponent>();
    if (!Piece->RequiredBuilderFactionId.IsNone())
    {
        if (!Faction || Faction->GetPrimaryFactionId() != Piece->RequiredBuilderFactionId || Faction->GetReputation(Piece->RequiredBuilderFactionId) < Piece->MinimumBuilderReputation)
            return EARPGPlacementResult::Restricted;
    }
    for (TActorIterator<AARPGFactionTerritoryVolume> It(World); It; ++It)
    {
        AARPGFactionTerritoryVolume* Territory = *It;
        if (Territory && Territory->GetComponentsBoundingBox(true).IsInside(Final.GetLocation()) && !Territory->CanActorBuildHere(Owner)) return EARPGPlacementResult::Restricted;
    }

    FCollisionQueryParams Params(SCENE_QUERY_STAT(ARPGBuildPlacement), false, Owner);
    const FCollisionShape Shape = FCollisionShape::MakeBox(Piece->PlacementBounds.ComponentMax(FVector(1.f)));
    if (World->OverlapBlockingTestByChannel(Final.GetLocation(), Final.GetRotation(), PlacementCollisionChannel, Shape, Params))
        return EARPGPlacementResult::Blocked;

    if (Piece->bRequiresSupport)
    {
        FHitResult Hit;
        const FVector Start = Final.GetLocation();
        const FVector End = Start - FVector(0.f, 0.f, FMath::Max(1.f, Piece->SupportTraceDepth));
        if (!World->LineTraceSingleByChannel(Hit, Start, End, PlacementCollisionChannel, Params)) return EARPGPlacementResult::Unsupported;
    }
    return EARPGPlacementResult::Valid;
}

bool UARPGBuildingComponent::RequestPlacePiece(UARPGBuildPieceDefinition* Piece, const FTransform& DesiredTransform)
{
    if (!GetOwner() || !Piece) return false;
    if (!GetOwner()->HasAuthority())
    {
        ServerPlacePiece(Piece, DesiredTransform);
        return true;
    }
    return PlacePieceAuthority(Piece, DesiredTransform);
}

void UARPGBuildingComponent::ServerPlacePiece_Implementation(UARPGBuildPieceDefinition* Piece, FTransform DesiredTransform)
{
    PlacePieceAuthority(Piece, DesiredTransform);
}

bool UARPGBuildingComponent::PlacePieceAuthority(UARPGBuildPieceDefinition* Piece, const FTransform& DesiredTransform)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || !Piece || !GetWorld()) return false;
    const FTransform Final = SnapTransform(Piece, DesiredTransform);
    const EARPGPlacementResult Check = EvaluatePlacement(Piece, Final);
    if (Check != EARPGPlacementResult::Valid)
    {
        OnPlacementResult.Broadcast(Check, nullptr);
        return false;
    }

    UClass* LoadedClass = Piece->ActorClass.LoadSynchronous();
    if (!LoadedClass || !LoadedClass->IsChildOf(AARPGBuildPieceActor::StaticClass()))
    {
        OnPlacementResult.Broadcast(EARPGPlacementResult::NoPiece, nullptr);
        return false;
    }
    if (!ConsumeBuildResources(Piece))
    {
        OnPlacementResult.Broadcast(EARPGPlacementResult::MissingResources, nullptr);
        return false;
    }

    FActorSpawnParameters Params;
    Params.Owner = GetOwner();
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AARPGBuildPieceActor* Spawned = GetWorld()->SpawnActor<AARPGBuildPieceActor>(LoadedClass, Final, Params);
    if (!Spawned) return false;

    Spawned->InitializeBuilding(Piece, GetOwner());
    if (UARPGEventRouterComponent* Router = GetOwner()->FindComponentByClass<UARPGEventRouterComponent>()) Router->ReportBuilt(Piece->DefinitionId, 1);
    OnPlacementResult.Broadcast(EARPGPlacementResult::Valid, Spawned);
    return true;
}
