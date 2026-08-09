#include "Subsystems/ARPGSaveSubsystem.h"
#include "Subsystems/ARPGAccountSubsystem.h"
#include "Save/ARPGSaveGame.h"
#include "Actors/ARPGCharacter.h"
#include "Actors/ARPGDungeonManager.h"
#include "Building/ARPGBuildPieceActor.h"
#include "Crafting/ARPGStorageActor.h"
#include "Crafting/ARPGCraftingStationActor.h"
#include "Components/ARPGFactionOwnershipComponent.h"
#include "Components/ARPGStatsComponent.h"
#include "Components/ARPGProgressionComponent.h"
#include "Components/ARPGInventoryComponent.h"
#include "Components/ARPGQuestComponent.h"
#include "Components/ARPGSkillComponent.h"
#include "Components/ARPGSlayerComponent.h"
#include "Components/ARPGFactionComponent.h"
#include "Components/ARPGCurrencyComponent.h"
#include "Components/ARPGBattlePetComponent.h"
#include "Components/ARPGClassComponent.h"
#include "Mounts/ARPGMountComponent.h"
#include "Social/ARPGGroupComponent.h"
#include "Data/ARPGClassDefinition.h"
#include "Data/ARPGBuildPieceDefinition.h"
#include "Data/ARPGDungeonDefinition.h"
#include "Utilities/ARPGAssetLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"
#include "Engine/World.h"

FString UARPGSaveSubsystem::MakeCharacterSlotName(const FGuid& CharacterId) const
{
    const UARPGAccountSubsystem* Accounts = GetGameInstance() ? GetGameInstance()->GetSubsystem<UARPGAccountSubsystem>() : nullptr;
    const FString Prefix = Accounts ? Accounts->GetCurrentAccountSlotPrefix() : TEXT("ARPG_Guest");
    return FString::Printf(TEXT("%s_Char_%s"), *Prefix, *CharacterId.ToString(EGuidFormats::Digits));
}

FString UARPGSaveSubsystem::MakeWorldSlotName(FString WorldId) const
{
    WorldId = WorldId.TrimStartAndEnd();
    if (WorldId.IsEmpty()) WorldId = TEXT("DefaultWorld");
    WorldId.ReplaceInline(TEXT(" "), TEXT("_"));
    return FString::Printf(TEXT("ARPG_World_%s"), *WorldId.Left(64));
}

bool UARPGSaveSubsystem::SaveCharacter(AActor* CharacterActor, FString SlotOverride)
{
    AARPGCharacter* Character = Cast<AARPGCharacter>(CharacterActor);
    if (!Character || (Character->GetNetMode() == NM_Client && !Character->HasAuthority())) return false;
    Character->EnsureCharacterId();
    UARPGSaveGame* Save = Cast<UARPGSaveGame>(UGameplayStatics::CreateSaveGameObject(UARPGSaveGame::StaticClass())); if (!Save) return false;
    if (UARPGAccountSubsystem* Accounts = GetGameInstance() ? GetGameInstance()->GetSubsystem<UARPGAccountSubsystem>() : nullptr)
    { Save->AccountId = Accounts->CurrentAccountId; Accounts->RegisterCharacterId(Character->CharacterId); }
    FARPGCharacterSaveData& D = Save->Character;
    D.CharacterId = Character->CharacterId; D.CharacterName = Character->RPGCharacterName; D.Location = Character->GetActorLocation(); D.Rotation = Character->GetActorRotation();
    if (Character->ClassComponent) D.ClassId = Character->ClassComponent->GetClassId();
    if (Character->Faction) { D.PrimaryFactionId = Character->Faction->GetPrimaryFactionId(); D.Reputation = Character->Faction->Reputation; }
    if (Character->Stats) { D.Health=Character->Stats->Health; D.Mana=Character->Stats->Mana; D.Stamina=Character->Stats->Stamina; }
    if (Character->Progression) { D.Level = Character->Progression->Level; D.XP = Character->Progression->XP; }
    if (Character->Inventory) D.Inventory = Character->Inventory->Items;
    if (Character->Quests) D.Quests = Character->Quests->Quests;
    if (Character->Skills) D.Skills = Character->Skills->Skills;
    if (Character->Slayer) { D.SlayerTask = Character->Slayer->ActiveTask; D.SlayerPoints = Character->Slayer->SlayerPoints; D.SlayerStreak = Character->Slayer->TaskStreak; }
    if (Character->Currencies) D.Currencies = Character->Currencies->Balances;
    if (Character->BattlePets) { D.BattlePets = Character->BattlePets->Pets; D.BattlePetTeam = Character->BattlePets->ActiveTeam; }
    if (Character->Mounts) D.Mounts = Character->Mounts->MakeMountSaveState();
    if (Character->Group) { D.GroupMembership = Character->Group->Membership; D.GuildId = Character->Group->GuildId; }
    Save->SavedAtUtc = FDateTime::UtcNow();
    const FString Slot = SlotOverride.IsEmpty() ? MakeCharacterSlotName(Character->CharacterId) : SlotOverride;
    FAsyncSaveGameToSlotDelegate Delegate; Delegate.BindUObject(this, &UARPGSaveSubsystem::HandleAsyncSaveComplete);
    UGameplayStatics::AsyncSaveGameToSlot(Save, Slot, 0, Delegate); return true;
}

bool UARPGSaveSubsystem::LoadCharacter(AActor* CharacterActor, FString SlotOverride)
{
    AARPGCharacter* Character = Cast<AARPGCharacter>(CharacterActor); if (!Character) return false;
    if (!Character->CharacterId.IsValid() && SlotOverride.IsEmpty()) return false;
    if (Character->GetNetMode() == NM_Client && !Character->HasAuthority()) return false;
    const FString Slot = SlotOverride.IsEmpty() ? MakeCharacterSlotName(Character->CharacterId) : SlotOverride;
    UARPGSaveGame* Save = Cast<UARPGSaveGame>(UGameplayStatics::LoadGameFromSlot(Slot, 0)); if (!Save) return false;
    const FARPGCharacterSaveData& D = Save->Character;
    Character->CharacterId = D.CharacterId; Character->RPGCharacterName = D.CharacterName;
    Character->SetActorLocationAndRotation(D.Location, D.Rotation, false, nullptr, ETeleportType::TeleportPhysics);
    if (Character->Progression) Character->Progression->SetProgression(D.Level, D.XP);
    if (Character->Stats) { Character->Stats->Health=FMath::Clamp(D.Health,0.f,Character->Stats->MaxHealth); Character->Stats->Mana=FMath::Clamp(D.Mana,0.f,Character->Stats->MaxMana); Character->Stats->Stamina=FMath::Clamp(D.Stamina,0.f,Character->Stats->MaxStamina); }
    if (Character->Inventory) Character->Inventory->ReplaceInventory(D.Inventory);
    if (Character->Quests) Character->Quests->ReplaceQuests(D.Quests);
    if (Character->Skills) Character->Skills->ReplaceSkills(D.Skills);
    if (Character->Slayer) Character->Slayer->RestoreSlayerState(D.SlayerTask, D.SlayerPoints, D.SlayerStreak);
    if (Character->Faction) { Character->Faction->SetPrimaryFactionId(D.PrimaryFactionId); Character->Faction->ReplaceReputation(D.Reputation); }
    if (Character->Currencies) Character->Currencies->ReplaceBalances(D.Currencies);
    if (Character->BattlePets) Character->BattlePets->ReplacePetState(D.BattlePets, D.BattlePetTeam);
    if (Character->Mounts) Character->Mounts->ReplaceMountState(D.Mounts);
    if (Character->Group) { Character->Group->SetMembership(D.GroupMembership); Character->Group->SetGuild(D.GuildId); }
    if (Character->ClassComponent && !D.ClassId.IsNone())
        if (UARPGClassDefinition* Def=Cast<UARPGClassDefinition>(UARPGAssetLibrary::ResolveDefinitionById(UARPGClassDefinition::StaticClass(),D.ClassId))) Character->ClassComponent->ApplyClassDefinition(Def,false);
    return true;
}

bool UARPGSaveSubsystem::SaveWorld(FString WorldId, FString SlotOverride)
{
    UWorld* W=GetWorld(); if(!W || W->GetNetMode()==NM_Client) return false;
    UARPGWorldSaveGame* Save=Cast<UARPGWorldSaveGame>(UGameplayStatics::CreateSaveGameObject(UARPGWorldSaveGame::StaticClass())); if(!Save)return false;
    FARPGWorldSaveData& D=Save->World; D.WorldId=WorldId; D.SavedAtUtc=FDateTime::UtcNow();
    for(TActorIterator<AARPGBuildPieceActor> It(W);It;++It)
    {
        AARPGBuildPieceActor* B=*It; if(!B || !B->bRuntimePlaced || !B->BuildingId.IsValid())continue;
        FARPGPlacedBuildingSave R; R.BuildingId=B->BuildingId; R.PieceId=B->Definition?B->Definition->DefinitionId:NAME_None; R.ActorClass=FSoftClassPath(B->GetClass()->GetPathName()); R.Transform=B->GetActorTransform(); R.Health=B->Health; R.UpgradeLevel=B->UpgradeLevel;
        if(B->Ownership){R.OwnerAccountId=B->Ownership->OwnerAccountId;R.OwnerCharacterId=B->Ownership->OwnerCharacterId;R.OwnerFactionId=B->Ownership->OwnerFactionId;} D.Buildings.Add(R);
    }
    for(TActorIterator<AARPGStorageActor> It(W);It;++It)
    {
        AARPGStorageActor* S=*It; if(!S||!S->bPersistent||!S->ContainerId.IsValid()||!S->bRuntimePlaced)continue;
        FARPGContainerSave R; R.ContainerId=S->ContainerId; R.LinkedBuildingId=S->BuildingId; R.ActorClass=FSoftClassPath(S->GetClass()->GetPathName()); R.Transform=S->GetActorTransform(); if(S->Inventory)R.Items=S->Inventory->Items;
        if(S->Ownership){R.OwnerAccountId=S->Ownership->OwnerAccountId;R.OwnerCharacterId=S->Ownership->OwnerCharacterId;R.OwnerFactionId=S->Ownership->OwnerFactionId;}
        if(AARPGCraftingStationActor* C=Cast<AARPGCraftingStationActor>(S)){R.CraftQueue=C->CraftQueue;if(C->OutputInventory)R.OutputItems=C->OutputInventory->Items;} D.Containers.Add(R);
    }
    for(TActorIterator<AARPGDungeonManager> It(W);It;++It)
    {
        AARPGDungeonManager* M=*It; if(!M||!M->Definition||M->Definition->DefinitionId.IsNone())continue;
        FARPGDungeonSaveState R;R.DungeonId=M->Definition->DefinitionId;R.Encounters=M->Encounters;R.Checkpoint=M->CurrentCheckpoint;R.bComplete=M->bDungeonComplete;D.Dungeons.Add(R);
    }
    const FString Slot=SlotOverride.IsEmpty()?MakeWorldSlotName(WorldId):SlotOverride; FAsyncSaveGameToSlotDelegate Delegate;Delegate.BindUObject(this,&UARPGSaveSubsystem::HandleAsyncSaveComplete);UGameplayStatics::AsyncSaveGameToSlot(Save,Slot,0,Delegate);return true;
}

bool UARPGSaveSubsystem::LoadWorld(FString WorldId, FString SlotOverride)
{
    UWorld* W=GetWorld(); if(!W||W->GetNetMode()==NM_Client)return false; const FString Slot=SlotOverride.IsEmpty()?MakeWorldSlotName(WorldId):SlotOverride;
    UARPGWorldSaveGame* Save=Cast<UARPGWorldSaveGame>(UGameplayStatics::LoadGameFromSlot(Slot,0));if(!Save)return false;
    TMap<FGuid,AARPGBuildPieceActor*> Existing;
    for(TActorIterator<AARPGBuildPieceActor> It(W);It;++It) if(It->bRuntimePlaced&&It->BuildingId.IsValid())Existing.Add(It->BuildingId,*It);
    TSet<FGuid> SavedIds;
    for(const FARPGPlacedBuildingSave& R:Save->World.Buildings)
    {
        SavedIds.Add(R.BuildingId); AARPGBuildPieceActor* B=Existing.FindRef(R.BuildingId);
        if(!B)
        {
            UClass* Cls=R.ActorClass.TryLoadClass<AARPGBuildPieceActor>(); if(!Cls)continue;
            FActorSpawnParameters Params;Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn; B=W->SpawnActor<AARPGBuildPieceActor>(Cls,R.Transform,Params); if(!B)continue;
            UARPGBuildPieceDefinition* Def=Cast<UARPGBuildPieceDefinition>(UARPGAssetLibrary::ResolveDefinitionById(UARPGBuildPieceDefinition::StaticClass(),R.PieceId));
            if(Def)B->InitializeBuilding(Def,nullptr); B->BuildingId=R.BuildingId; B->bRuntimePlaced=true;
        }
        B->SetActorTransform(R.Transform,false,nullptr,ETeleportType::TeleportPhysics);B->Health=R.Health;B->UpgradeLevel=R.UpgradeLevel;
        if(B->Ownership)B->Ownership->SetOwnership(R.OwnerAccountId,R.OwnerCharacterId,R.OwnerFactionId);
    }
    for(const TPair<FGuid,AARPGBuildPieceActor*>& Pair:Existing) if(!SavedIds.Contains(Pair.Key)&&Pair.Value)Pair.Value->Destroy();

    for(const FARPGContainerSave& R:Save->World.Containers)
    {
        AARPGStorageActor* S=nullptr;
        for(TActorIterator<AARPGStorageActor> It(W);It;++It) if((R.LinkedBuildingId.IsValid()&&It->BuildingId==R.LinkedBuildingId)||It->ContainerId==R.ContainerId){S=*It;break;}
        if(!S)continue;
        S->ContainerId=R.ContainerId; if(S->Inventory)S->Inventory->ReplaceInventory(R.Items); if(S->Ownership)S->Ownership->SetOwnership(R.OwnerAccountId,R.OwnerCharacterId,R.OwnerFactionId);
        if(AARPGCraftingStationActor* C=Cast<AARPGCraftingStationActor>(S)){C->CraftQueue=R.CraftQueue;if(C->OutputInventory)C->OutputInventory->ReplaceInventory(R.OutputItems);C->ProcessOfflineElapsed();}
    }
    for(const FARPGDungeonSaveState& R:Save->World.Dungeons)
        for(TActorIterator<AARPGDungeonManager> It(W);It;++It) if(It->Definition&&It->Definition->DefinitionId==R.DungeonId){It->RestoreEncounterProgress(R.Encounters);It->CurrentCheckpoint=R.Checkpoint;It->bDungeonComplete=R.bComplete;break;}
    return true;
}

bool UARPGSaveSubsystem::DoesCharacterSaveExist(const FGuid& CharacterId) const{return CharacterId.IsValid()&&UGameplayStatics::DoesSaveGameExist(MakeCharacterSlotName(CharacterId),0);}
bool UARPGSaveSubsystem::DoesWorldSaveExist(FString WorldId) const{return UGameplayStatics::DoesSaveGameExist(MakeWorldSlotName(WorldId),0);}
void UARPGSaveSubsystem::HandleAsyncSaveComplete(const FString& SlotName,const int32 UserIndex,bool bSuccess){OnSaveComplete.Broadcast(SlotName,bSuccess);}
