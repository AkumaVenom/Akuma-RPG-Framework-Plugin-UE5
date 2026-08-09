#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "ARPGTypes.h"
#include "ARPGEventRouterComponent.generated.h"

UCLASS(ClassGroup=(ARPG), meta=(BlueprintSpawnableComponent))
class AKUMASRPGFRAMEWORK_API UARPGEventRouterComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="ARPG|Events", meta=(BlueprintAuthorityOnly)) void ReportKill(FName CreatureId, FGameplayTag SlayerCategory, int64 CharacterXP=0);
    UFUNCTION(BlueprintCallable, Category="ARPG|Events", meta=(BlueprintAuthorityOnly)) void ReportItemLooted(FName ItemId, int32 Quantity);
    UFUNCTION(BlueprintCallable, Category="ARPG|Events", meta=(BlueprintAuthorityOnly)) void ReportPetCaptured(FName SpeciesId);
    UFUNCTION(BlueprintCallable, Category="ARPG|Events", meta=(BlueprintAuthorityOnly)) void ReportPetBattleWon(FName EncounterId=NAME_None);
    UFUNCTION(BlueprintCallable, Category="ARPG|Events", meta=(BlueprintAuthorityOnly)) void ReportDungeonCompleted(FName DungeonId);
    UFUNCTION(BlueprintCallable, Category="ARPG|Events", meta=(BlueprintAuthorityOnly)) void ReportRaidBossDefeated(FName BossId);
    UFUNCTION(BlueprintCallable, Category="ARPG|Events", meta=(BlueprintAuthorityOnly)) void ReportCrafted(FName RecipeId, int32 Quantity=1);
    UFUNCTION(BlueprintCallable, Category="ARPG|Events", meta=(BlueprintAuthorityOnly)) void ReportBuilt(FName PieceId, int32 Quantity=1);
    UFUNCTION(BlueprintCallable, Category="ARPG|Events", meta=(BlueprintAuthorityOnly)) void ReportSkillLevel(FName SkillId, int32 NewLevel);
    UFUNCTION(BlueprintCallable, Category="ARPG|Events", meta=(BlueprintAuthorityOnly)) void ReportMountUnlocked(FName MountId);
    UFUNCTION(BlueprintCallable, Category="ARPG|Events", meta=(BlueprintAuthorityOnly)) void ReportReputationChanged(FName FactionId, int32 NewValue);
    UFUNCTION(BlueprintCallable, Category="ARPG|Events", meta=(BlueprintAuthorityOnly)) void SendEventLogMessage(FText Message, EARPGChatChannel Channel=EARPGChatChannel::System);
};
