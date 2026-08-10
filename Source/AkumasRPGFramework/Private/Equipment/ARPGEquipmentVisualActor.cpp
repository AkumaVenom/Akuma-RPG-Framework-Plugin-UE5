#include "Equipment/ARPGEquipmentVisualActor.h"
#include "Data/ARPGItemDefinition.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"

AARPGEquipmentVisualActor::AARPGEquipmentVisualActor()
{
    bReplicates = false;
    SetReplicateMovement(false);
    PrimaryActorTick.bCanEverTick = false;
    SetActorEnableCollision(false);

    Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(Root);

    StaticMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StaticMesh"));
    StaticMesh->SetupAttachment(Root);
    StaticMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    StaticMesh->SetGenerateOverlapEvents(false);

    SkeletalMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("SkeletalMesh"));
    SkeletalMesh->SetupAttachment(Root);
    SkeletalMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    SkeletalMesh->SetGenerateOverlapEvents(false);
}

void AARPGEquipmentVisualActor::ConfigureFromItem(const UARPGItemDefinition* ItemDefinition)
{
    if (!ItemDefinition) return;

    USkeletalMesh* LoadedSkeletal = ItemDefinition->EquippedSkeletalMesh.IsNull() ? nullptr : ItemDefinition->EquippedSkeletalMesh.LoadSynchronous();
    UStaticMesh* LoadedStatic = ItemDefinition->EquippedStaticMesh.IsNull() ? nullptr : ItemDefinition->EquippedStaticMesh.LoadSynchronous();

    if (SkeletalMesh)
    {
        SkeletalMesh->SetSkeletalMesh(LoadedSkeletal, true);
        SkeletalMesh->SetVisibility(LoadedSkeletal != nullptr, true);
        SkeletalMesh->SetHiddenInGame(LoadedSkeletal == nullptr, true);
    }
    if (StaticMesh)
    {
        StaticMesh->SetStaticMesh(LoadedSkeletal ? nullptr : LoadedStatic);
        StaticMesh->SetVisibility(!LoadedSkeletal && LoadedStatic != nullptr, true);
        StaticMesh->SetHiddenInGame(LoadedSkeletal || LoadedStatic == nullptr, true);
    }

    OnEquipmentVisualConfigured(ItemDefinition);
}
