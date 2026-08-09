#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "ARPGCharacter.generated.h"

class UAbilitySystemComponent;
class UARPGStatsComponent;
class UARPGCombatComponent;
class UARPGProgressionComponent;
class UARPGInventoryComponent;
class UARPGEquipmentComponent;
class UARPGCurrencyComponent;
class UARPGQuestComponent;
class UARPGSkillComponent;
class UARPGSlayerComponent;
class UARPGFactionComponent;
class UARPGBattlePetComponent;
class UARPGBattlePetBattleComponent;
class UARPGClassComponent;
class UARPGAbilityBridgeComponent;
class UARPGEventRouterComponent;
class UARPGPersistenceComponent;
class UARPGInteractionComponent;
class UARPGBuildingComponent;
class UARPGMountComponent;
class UARPGGroupComponent;

UCLASS(BlueprintType, Blueprintable)
class AKUMASRPGFRAMEWORK_API AARPGCharacter : public ACharacter, public IAbilitySystemInterface
{
    GENERATED_BODY()
public:
    AARPGCharacter();

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, SaveGame, Category="Identity") FGuid CharacterId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, SaveGame, Category="Identity") FString RPGCharacterName = TEXT("Hero");

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="ARPG") TObjectPtr<UAbilitySystemComponent> AbilitySystem;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="ARPG") TObjectPtr<UARPGStatsComponent> Stats;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="ARPG") TObjectPtr<UARPGCombatComponent> Combat;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="ARPG") TObjectPtr<UARPGProgressionComponent> Progression;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="ARPG") TObjectPtr<UARPGInventoryComponent> Inventory;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="ARPG") TObjectPtr<UARPGEquipmentComponent> Equipment;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="ARPG") TObjectPtr<UARPGCurrencyComponent> Currencies;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="ARPG") TObjectPtr<UARPGQuestComponent> Quests;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="ARPG") TObjectPtr<UARPGSkillComponent> Skills;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="ARPG") TObjectPtr<UARPGSlayerComponent> Slayer;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="ARPG") TObjectPtr<UARPGFactionComponent> Faction;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="ARPG") TObjectPtr<UARPGBattlePetComponent> BattlePets;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="ARPG") TObjectPtr<UARPGBattlePetBattleComponent> BattlePetBattle;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="ARPG") TObjectPtr<UARPGClassComponent> ClassComponent;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="ARPG") TObjectPtr<UARPGAbilityBridgeComponent> AbilityBridge;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="ARPG") TObjectPtr<UARPGEventRouterComponent> EventRouter;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="ARPG") TObjectPtr<UARPGPersistenceComponent> Persistence;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="ARPG") TObjectPtr<UARPGInteractionComponent> Interaction;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="ARPG") TObjectPtr<UARPGBuildingComponent> Building;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="ARPG") TObjectPtr<UARPGMountComponent> Mounts;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="ARPG") TObjectPtr<UARPGGroupComponent> Group;

    virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override { return AbilitySystem; }
    virtual void BeginPlay() override;
    virtual void PossessedBy(AController* NewController) override;
    virtual void OnRep_PlayerState() override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UFUNCTION(BlueprintCallable, Category="ARPG|Identity", meta=(BlueprintAuthorityOnly)) void EnsureCharacterId();
};
