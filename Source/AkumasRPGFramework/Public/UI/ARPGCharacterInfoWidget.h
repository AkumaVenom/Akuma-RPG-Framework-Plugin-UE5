#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ARPGCharacterInfoWidget.generated.h"

class AARPGCharacter;
class UProgressBar;
class UTextBlock;

/** Compact presentation snapshot consumed by overhead character-info widgets. */
USTRUCT(BlueprintType)
struct AKUMASRPGFRAMEWORK_API FARPGCharacterInfoSnapshot
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="ARPG|NPC Info") TObjectPtr<AARPGCharacter> Character = nullptr;
    UPROPERTY(BlueprintReadOnly, Category="ARPG|NPC Info") FString CharacterName;
    UPROPERTY(BlueprintReadOnly, Category="ARPG|NPC Info") int32 Level = 1;
    UPROPERTY(BlueprintReadOnly, Category="ARPG|NPC Info") float Health = 0.f;
    UPROPERTY(BlueprintReadOnly, Category="ARPG|NPC Info") float MaxHealth = 0.f;
    UPROPERTY(BlueprintReadOnly, Category="ARPG|NPC Info") float HealthPercent = 0.f;
    UPROPERTY(BlueprintReadOnly, Category="ARPG|NPC Info") bool bAlive = true;
};

/**
 * Native default overhead info widget.
 *
 * It works with no Widget Blueprint at all. A project may subclass it for custom art and either use
 * the standard child names (CharacterNameText, LevelText, HealthBar, HealthText) or handle the
 * On ARPG Character Info Updated Blueprint event for completely custom presentation.
 */
UCLASS(BlueprintType, Blueprintable)
class AKUMASRPGFRAMEWORK_API UARPGCharacterInfoWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category="ARPG|NPC Info")
    void SetCharacterInfo(const FARPGCharacterInfoSnapshot& InSnapshot);

    UFUNCTION(BlueprintPure, Category="ARPG|NPC Info")
    FARPGCharacterInfoSnapshot GetCharacterInfo() const { return CharacterInfo; }

    UFUNCTION(BlueprintPure, Category="ARPG|NPC Info")
    AARPGCharacter* GetObservedCharacter() const { return CharacterInfo.Character.Get(); }

    UFUNCTION(BlueprintImplementableEvent, Category="ARPG|NPC Info", meta=(DisplayName="On ARPG Character Info Updated"))
    void BP_OnCharacterInfoUpdated(FARPGCharacterInfoSnapshot Snapshot);

    /** Standard native/custom field references. They are populated automatically when matching names exist. */
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|NPC Info") TObjectPtr<UTextBlock> CharacterNameText;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|NPC Info") TObjectPtr<UTextBlock> LevelText;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|NPC Info") TObjectPtr<UProgressBar> HealthBar;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|NPC Info") TObjectPtr<UTextBlock> HealthText;

protected:
    virtual void NativeOnInitialized() override;

private:
    UPROPERTY(Transient)
    FARPGCharacterInfoSnapshot CharacterInfo;

    void EnsureNativeLayoutOrBindings();
    void ApplySnapshotToStandardFields();
};
