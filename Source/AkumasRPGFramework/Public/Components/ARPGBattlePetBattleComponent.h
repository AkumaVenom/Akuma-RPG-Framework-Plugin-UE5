#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ARPGTypes.h"
#include "ARPGBattlePetBattleComponent.generated.h"

class UARPGBattlePetDefinition;
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FARPGBattlePetBattleStateChanged, EARPGPetBattleState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FARPGBattlePetActionResolved, bool, bPlayerAction, FName, AbilityId, float, Amount);

UCLASS(ClassGroup=(ARPG), meta=(BlueprintSpawnableComponent))
class AKUMASRPGFRAMEWORK_API UARPGBattlePetBattleComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UARPGBattlePetBattleComponent();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, ReplicatedUsing=OnRep_BattleState, Category="Battle Pets") EARPGPetBattleState BattleState = EARPGPetBattleState::None;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category="Battle Pets") TArray<FARPGPetBattleUnit> PlayerTeam;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category="Battle Pets") TArray<FARPGPetBattleUnit> EnemyTeam;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category="Battle Pets") int32 ActivePlayerIndex = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category="Battle Pets") int32 ActiveEnemyIndex = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category="Battle Pets") int32 TurnNumber = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Replicated, Category="Battle Pets") bool bWildBattle = false;
    UPROPERTY(BlueprintAssignable) FARPGBattlePetBattleStateChanged OnBattleStateChanged;
    UPROPERTY(BlueprintAssignable) FARPGBattlePetActionResolved OnActionResolved;

    UFUNCTION(BlueprintCallable, Category="ARPG|Battle Pets") bool StartWildBattle(UARPGBattlePetDefinition* WildDefinition, int32 WildLevel=1, EARPGRarity Quality=EARPGRarity::Common);
    UFUNCTION(BlueprintCallable, Category="ARPG|Battle Pets") bool ChooseAbility(FName AbilityId);
    UFUNCTION(BlueprintCallable, Category="ARPG|Battle Pets") bool SwapActivePet(int32 TeamIndex);
    UFUNCTION(BlueprintCallable, Category="ARPG|Battle Pets") bool AttemptCapture(float CaptureBonus=0.f);
    UFUNCTION(BlueprintCallable, Category="ARPG|Battle Pets", meta=(BlueprintAuthorityOnly)) void EndBattle(EARPGPetBattleState Result);
    UFUNCTION(BlueprintPure, Category="ARPG|Battle Pets") bool IsBattleActive() const { return BattleState == EARPGPetBattleState::ChoosingAction || BattleState == EARPGPetBattleState::ResolvingTurn; }

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
protected:
    UFUNCTION(Server, Reliable) void ServerStartWildBattle(UARPGBattlePetDefinition* WildDefinition, int32 WildLevel, EARPGRarity Quality);
    UFUNCTION(Server, Reliable) void ServerChooseAbility(FName AbilityId);
    UFUNCTION(Server, Reliable) void ServerSwapActivePet(int32 TeamIndex);
    UFUNCTION(Server, Reliable) void ServerAttemptCapture(float CaptureBonus);
    UFUNCTION() void OnRep_BattleState(EARPGPetBattleState OldState);

    bool StartWildBattleAuthority(UARPGBattlePetDefinition* WildDefinition, int32 WildLevel, EARPGRarity Quality);
    bool ChooseAbilityAuthority(FName AbilityId);
    bool SwapActivePetAuthority(int32 TeamIndex);
    bool AttemptCaptureAuthority(float CaptureBonus);
    FARPGPetBattleUnit MakeUnitFromCollectionPet(const FARPGPetInstance& Pet) const;
    FARPGPetBattleUnit MakeWildUnit(UARPGBattlePetDefinition* Definition, int32 Level, EARPGRarity Quality) const;
    const struct FARPGPetAbilityDefinition* ResolveAbility(const FARPGPetBattleUnit& Unit, FName AbilityId) const;
    FName ChooseEnemyAbility() const;
    bool IsAbilityReady(const FARPGPetBattleUnit& Unit, FName AbilityId) const;
    void ResolveTurn(FName PlayerAbilityId);
    float ApplyAbility(FARPGPetBattleUnit& Source, FARPGPetBattleUnit& Target, const struct FARPGPetAbilityDefinition& Ability, bool bPlayerAction);
    void TickCooldowns(FARPGPetBattleUnit& Unit);
    void SetCooldown(FARPGPetBattleUnit& Unit, FName AbilityId, int32 Turns);
    bool AdvanceIfKnockedOut(TArray<FARPGPetBattleUnit>& Team, int32& ActiveIndex);
};
