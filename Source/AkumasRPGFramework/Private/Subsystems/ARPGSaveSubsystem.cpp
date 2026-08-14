#include "Subsystems/ARPGSaveSubsystem.h"
#include "Subsystems/ARPGAccountSubsystem.h"
#include "Save/ARPGSaveGame.h"
#include "Actors/ARPGCharacter.h"
#include "Actors/ARPGDungeonManager.h"
#include "Building/ARPGBuildPieceActor.h"
#include "Building/ARPGBuildDoorActor.h"
#include "Crafting/ARPGStorageActor.h"
#include "Crafting/ARPGCraftingStationActor.h"
#include "Components/ARPGFactionOwnershipComponent.h"
#include "Components/ARPGStatsComponent.h"
#include "Components/ARPGCombatComponent.h"
#include "Components/ARPGProgressionComponent.h"
#include "Components/ARPGInventoryComponent.h"
#include "Components/ARPGCraftingComponent.h"
#include "Components/ARPGQuickAccessComponent.h"
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
#include "Data/ARPGItemDefinition.h"
#include "Data/ARPGDungeonDefinition.h"
#include "Utilities/ARPGAssetLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"
#include "Engine/World.h"

namespace
{
    /**
     * Saves written before v2.14 had no serialized durability field. Initialize only those legacy
     * entries from the authored Item Definition. New-schema saves never pass through this path,
     * so a real damaged/broken runtime value (including zero) is preserved exactly.
     */
    void ARPGMigrateLegacyInventoryDurability(UARPGInventoryComponent* Inventory, TArray<FARPGInventoryEntry>& Entries)
    {
        if (!Inventory) return;
        for (FARPGInventoryEntry& Entry : Entries)
        {
            if (const UARPGItemDefinition* Definition = Inventory->ResolveItemDefinition(Entry))
            {
                if (Definition->bUsesDurability)
                    Entry.Durability = FMath::Max(1.f, Definition->MaxDurability);
            }
        }
    }

    /**
     * Pre-v2.15.12 Guest profiles did not persist their CharacterId in the local account index.
     * Runtime buildings correctly persisted OwnerCharacterId, but the next session generated a new
     * player CharacterId. Because build snapping deliberately requires modification access to an
     * existing runtime structure, those loaded buildings then reported Restricted.
     *
     * Recover only the unambiguous local case: one player-controlled locally-owned character and
     * one unique no-account owner identity in the saved world. We intentionally do not guess in a
     * multi-player/multi-owner world. Once recovered, RegisterCharacterId persists the Guest identity
     * so subsequent character/world loads use the normal stable path.
     */
    FString ARPGMakeGuestCharacterSlotName(const FGuid& CharacterId)
    {
        return CharacterId.IsValid()
            ? FString::Printf(TEXT("ARPG_Guest_Char_%s"), *CharacterId.ToString(EGuidFormats::Digits))
            : FString();
    }

    void ARPGRecoverLegacyGuestWorldOwnerIdentity(UWorld* World, UARPGWorldSaveGame* Save)
    {
        if (!World || !Save || !World->GetGameInstance()) return;
        UARPGAccountSubsystem* Accounts = World->GetGameInstance()->GetSubsystem<UARPGAccountSubsystem>();
        if (!Accounts || Accounts->IsLoggedIn()) return;

        AARPGCharacter* SoleLocalPlayer = nullptr;
        int32 PlayerCharacterCount = 0;
        for (TActorIterator<AARPGCharacter> It(World); It; ++It)
        {
            if (!It->IsPlayerControlled()) continue;
            ++PlayerCharacterCount;
            if (It->IsLocallyControlled()) SoleLocalPlayer = *It;
        }
        if (PlayerCharacterCount != 1 || !SoleLocalPlayer) return;

        TSet<FGuid> LegacyGuestOwnerIds;
        for (const FARPGPlacedBuildingSave& Record : Save->World.Buildings)
            if (!Record.OwnerAccountId.IsValid() && Record.OwnerCharacterId.IsValid()) LegacyGuestOwnerIds.Add(Record.OwnerCharacterId);
        for (const FARPGContainerSave& Record : Save->World.Containers)
            if (!Record.OwnerAccountId.IsValid() && Record.OwnerCharacterId.IsValid()) LegacyGuestOwnerIds.Add(Record.OwnerCharacterId);

        if (LegacyGuestOwnerIds.Num() != 1) return;
        const FGuid LegacyOwnerId = LegacyGuestOwnerIds.Array()[0];
        FGuid StableGuestId = Accounts->GetLastCharacterId();

        if (!StableGuestId.IsValid())
        {
            // First v2.15.12 load of a legacy Guest world: preserve the old identity so its existing
            // ARPG_Guest_Char_<id> character save can be found on the following persistence pass.
            StableGuestId = LegacyOwnerId;
            if (!Accounts->RegisterCharacterId(StableGuestId)) return;
        }
        else if (StableGuestId != LegacyOwnerId)
        {
            // A stable Guest identity may already have been registered before a manually triggered
            // legacy world load. Prefer the legacy id only when it has an actual character save and
            // the indexed id does not; otherwise migrate this one unambiguous world's owner records
            // to the current stable Guest profile. This avoids order-dependent Restricted results.
            const FString LegacySlot = ARPGMakeGuestCharacterSlotName(LegacyOwnerId);
            const FString StableSlot = ARPGMakeGuestCharacterSlotName(StableGuestId);
            const bool bLegacyCharacterSaveExists = !LegacySlot.IsEmpty() && UGameplayStatics::DoesSaveGameExist(LegacySlot, 0);
            const bool bStableCharacterSaveExists = !StableSlot.IsEmpty() && UGameplayStatics::DoesSaveGameExist(StableSlot, 0);
            if (bLegacyCharacterSaveExists && !bStableCharacterSaveExists)
            {
                StableGuestId = LegacyOwnerId;
                if (!Accounts->RegisterCharacterId(StableGuestId)) return;
            }
            else
            {
                for (FARPGPlacedBuildingSave& Record : Save->World.Buildings)
                    if (!Record.OwnerAccountId.IsValid() && Record.OwnerCharacterId == LegacyOwnerId) Record.OwnerCharacterId = StableGuestId;
                for (FARPGContainerSave& Record : Save->World.Containers)
                    if (!Record.OwnerAccountId.IsValid() && Record.OwnerCharacterId == LegacyOwnerId) Record.OwnerCharacterId = StableGuestId;
            }
        }

        SoleLocalPlayer->CharacterId = StableGuestId;
    }
}

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
    if (Character->Stats) { D.Health=Character->Stats->Health; D.Mana=Character->Stats->Mana; D.Stamina=Character->Stats->Stamina; D.StatProgression=Character->Stats->MakeStatProgressionSaveState(); }
    if (Character->Progression) { D.Level = Character->Progression->Level; D.XP = Character->Progression->XP; }
    if (Character->Inventory) D.Inventory = Character->Inventory->Items;
    if (Character->Crafting) D.PersonalCraftingState = Character->Crafting->MakeCraftingSaveState();
    if (Character->QuickAccess) { D.QuickAccessSlots = Character->QuickAccess->QuickAccessSlots; D.ActiveQuickAccessSlotNumber = Character->QuickAccess->ActiveSlotNumber; }
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
    if (Character->Combat) Character->Combat->SetRespawnTransform(Character->GetActorTransform());
    if (Character->Progression) Character->Progression->SetProgression(D.Level, D.XP);
    if (Character->Stats)
        Character->Stats->RestoreStatProgressionState(D.StatProgression, D.Level, Save->SaveVersion < 4);

    // Restore equipment before current vitals. Equipped items can now modify max HP/MP/Stamina;
    // clamping a saved current value against the unequipped maximum would permanently lose health/mana
    // from the save before those equipment bonuses were restored. ReplaceInventory triggers one stats
    // recalculation, then current vitals are clamped against the final equipped maxima.
    if (Character->Inventory)
    {
        TArray<FARPGInventoryEntry> InventoryToRestore = D.Inventory;
        if (Save->SaveVersion < 5) ARPGMigrateLegacyInventoryDurability(Character->Inventory, InventoryToRestore);
        Character->Inventory->ReplaceInventory(InventoryToRestore);
    }
    if (Character->Stats)
    {
        Character->Stats->Health=FMath::Clamp(D.Health,0.f,Character->Stats->MaxHealth);
        Character->Stats->Mana=FMath::Clamp(D.Mana,0.f,Character->Stats->MaxMana);
        Character->Stats->Stamina=FMath::Clamp(D.Stamina,0.f,Character->Stats->MaxStamina);
    }
    if (Character->QuickAccess) Character->QuickAccess->ReplaceQuickAccessState(D.QuickAccessSlots, D.ActiveQuickAccessSlotNumber);
    if (Character->Quests) Character->Quests->ReplaceQuests(D.Quests);
    if (Character->Skills) Character->Skills->ReplaceSkills(D.Skills);
    if (Character->Crafting) Character->Crafting->RestoreCraftingSaveState(D.PersonalCraftingState);
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
        FARPGPlacedBuildingSave R; R.BuildingId=B->BuildingId; R.PieceId=B->Definition?B->Definition->DefinitionId:NAME_None; R.ActorClass=FSoftClassPath(B->GetClass()->GetPathName()); R.Transform=B->GetActorTransform(); R.Health=B->Health; R.UpgradeLevel=B->UpgradeLevel; R.bConstructionComplete=B->IsConstructionComplete(); R.ConstructionRemainingSeconds=B->GetConstructionRemainingSeconds(); if(const AARPGBuildDoorActor* Door=Cast<AARPGBuildDoorActor>(B))R.bDoorOpen=Door->IsDoorOpen();
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
    ARPGRecoverLegacyGuestWorldOwnerIdentity(W, Save);
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
            if(Def)
            {
                if(AARPGStorageActor* Storage=Cast<AARPGStorageActor>(B)) if(Storage->Inventory) Storage->Inventory->MaxSlots=FMath::Max(1,Def->StorageSlots);
                if(AARPGCraftingStationActor* Station=Cast<AARPGCraftingStationActor>(B)) Station->ApplyStationDefinition(Def->StationDefinition);
                B->InitializeBuilding(Def,nullptr);
            }
            B->BuildingId=R.BuildingId; B->bRuntimePlaced=true;
        }
        B->SetActorTransform(R.Transform,false,nullptr,ETeleportType::TeleportPhysics);B->Health=R.Health;B->UpgradeLevel=R.UpgradeLevel; if(Save->SaveVersion>=5) B->RestoreConstructionState(R.bConstructionComplete,R.ConstructionRemainingSeconds); else B->RestoreConstructionState(true,0.f); if(AARPGBuildDoorActor* Door=Cast<AARPGBuildDoorActor>(B)) Door->RestoreDoorOpenState(R.bDoorOpen);
        if(B->Ownership)B->Ownership->SetOwnership(R.OwnerAccountId,R.OwnerCharacterId,R.OwnerFactionId);
    }
    for(const TPair<FGuid,AARPGBuildPieceActor*>& Pair:Existing) if(!SavedIds.Contains(Pair.Key)&&Pair.Value)Pair.Value->Destroy();

    for(const FARPGContainerSave& R:Save->World.Containers)
    {
        AARPGStorageActor* S=nullptr;
        for(TActorIterator<AARPGStorageActor> It(W);It;++It) if((R.LinkedBuildingId.IsValid()&&It->BuildingId==R.LinkedBuildingId)||It->ContainerId==R.ContainerId){S=*It;break;}
        if(!S)continue;
        S->ContainerId=R.ContainerId;
        if(S->Inventory)
        {
            TArray<FARPGInventoryEntry> ItemsToRestore=R.Items;
            if(Save->SaveVersion<4) ARPGMigrateLegacyInventoryDurability(S->Inventory,ItemsToRestore);
            S->Inventory->ReplaceInventory(ItemsToRestore);
        }
        if(S->Ownership)S->Ownership->SetOwnership(R.OwnerAccountId,R.OwnerCharacterId,R.OwnerFactionId);
        if(AARPGCraftingStationActor* C=Cast<AARPGCraftingStationActor>(S))
        {
            C->CraftQueue=R.CraftQueue;
            if(C->OutputInventory)
            {
                TArray<FARPGInventoryEntry> OutputsToRestore=R.OutputItems;
                if(Save->SaveVersion<4) ARPGMigrateLegacyInventoryDurability(C->OutputInventory,OutputsToRestore);
                C->OutputInventory->ReplaceInventory(OutputsToRestore);
            }
            C->ProcessOfflineElapsed();
            C->SetActorTickEnabled(C->CraftQueue.Num()>0);
        }
    }
    for(const FARPGDungeonSaveState& R:Save->World.Dungeons)
        for(TActorIterator<AARPGDungeonManager> It(W);It;++It) if(It->Definition&&It->Definition->DefinitionId==R.DungeonId){It->RestoreEncounterProgress(R.Encounters);It->CurrentCheckpoint=R.Checkpoint;It->bDungeonComplete=R.bComplete;break;}
    return true;
}

bool UARPGSaveSubsystem::DoesCharacterSaveExist(const FGuid& CharacterId) const{return CharacterId.IsValid()&&UGameplayStatics::DoesSaveGameExist(MakeCharacterSlotName(CharacterId),0);}
bool UARPGSaveSubsystem::DoesWorldSaveExist(FString WorldId) const{return UGameplayStatics::DoesSaveGameExist(MakeWorldSlotName(WorldId),0);}
void UARPGSaveSubsystem::HandleAsyncSaveComplete(const FString& SlotName,const int32 UserIndex,bool bSuccess){OnSaveComplete.Broadcast(SlotName,bSuccess);}
