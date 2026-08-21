#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ARPGTypes.h"
#include "ARPGSettlementWidgets.generated.h"

class AARPGCharacter;
class AARPGSettlementHubActor;
class AARPGSettlementVillagerCharacter;
class AARPGBuildBedActor;
class UARPGSettlementUIComponent;
class UButton;
class UTextBlock;
class UVerticalBox;

USTRUCT(BlueprintType)
struct AKUMASRPGFRAMEWORK_API FARPGSettlementResidentView
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="ARPG|Settlement UI") FGuid ResidentId;
    UPROPERTY(BlueprintReadOnly, Category="ARPG|Settlement UI") TObjectPtr<AARPGSettlementVillagerCharacter> Resident = nullptr;
    UPROPERTY(BlueprintReadOnly, Category="ARPG|Settlement UI") FText DisplayName;
    UPROPERTY(BlueprintReadOnly, Category="ARPG|Settlement UI") EARPGSettlementResidentState State = EARPGSettlementResidentState::Homeless;
    UPROPERTY(BlueprintReadOnly, Category="ARPG|Settlement UI") FText StateText;
    UPROPERTY(BlueprintReadOnly, Category="ARPG|Settlement UI") FText BedText;
    UPROPERTY(BlueprintReadOnly, Category="ARPG|Settlement UI") bool bHasValidHome = false;
    UPROPERTY(BlueprintReadOnly, Category="ARPG|Settlement UI") bool bWorking = false;
};

UCLASS(BlueprintType, Blueprintable)
class AKUMASRPGFRAMEWORK_API UARPGSettlementHUDWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="ARPG|Settlement UI") void InitializeSettlementHUD(AARPGCharacter* InCharacter, AARPGSettlementHubActor* InHub);
    UFUNCTION(BlueprintCallable, Category="ARPG|Settlement UI") void SetSettlementHub(AARPGSettlementHubActor* InHub);
    UFUNCTION(BlueprintCallable, Category="ARPG|Settlement UI") void RefreshSettlementHUD();
    UFUNCTION(BlueprintImplementableEvent, Category="ARPG|Settlement UI", meta=(DisplayName="On ARPG Settlement HUD Refreshed")) void BP_OnSettlementHUDRefreshed(FARPGSettlementSummary Summary);

    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Settlement UI|Bindings") TObjectPtr<UTextBlock> SettlementNameText;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Settlement UI|Bindings") TObjectPtr<UTextBlock> ResidentsText;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Settlement UI|Bindings") TObjectPtr<UTextBlock> HomesText;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Settlement UI|Bindings") TObjectPtr<UTextBlock> WorkersText;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Settlement UI|Bindings") TObjectPtr<UTextBlock> SettlementStatusText;
private:
    UPROPERTY(Transient) TObjectPtr<AARPGCharacter> Character = nullptr;
    UPROPERTY(Transient) TObjectPtr<AARPGSettlementHubActor> Hub = nullptr;
    void EnsureNativeLayoutOrBindings();
};

UCLASS(BlueprintType, Blueprintable)
class AKUMASRPGFRAMEWORK_API UARPGSettlementResidentRowWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="ARPG|Settlement UI") void SetResidentView(const FARPGSettlementResidentView& InView);
    UFUNCTION(BlueprintImplementableEvent, Category="ARPG|Settlement UI", meta=(DisplayName="On ARPG Settlement Resident Row Updated")) void BP_OnResidentRowUpdated(FARPGSettlementResidentView ResidentView);

    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Settlement UI|Bindings") TObjectPtr<UTextBlock> ResidentNameText;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Settlement UI|Bindings") TObjectPtr<UTextBlock> ResidentStateText;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Settlement UI|Bindings") TObjectPtr<UTextBlock> ResidentBedText;
protected:
    virtual void NativeOnInitialized() override;
private:
    UPROPERTY(Transient) FARPGSettlementResidentView View;
    void EnsureNativeLayoutOrBindings();
    void ApplyView();
};

UCLASS(BlueprintType, Blueprintable)
class AKUMASRPGFRAMEWORK_API UARPGSettlementPanelWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="ARPG|Settlement UI") void InitializeSettlementPanel(AARPGCharacter* InCharacter, AARPGSettlementHubActor* InHub, UARPGSettlementUIComponent* InUI);
    UFUNCTION(BlueprintCallable, Category="ARPG|Settlement UI") void RefreshSettlementPanel();
    UFUNCTION(BlueprintImplementableEvent, Category="ARPG|Settlement UI", meta=(DisplayName="On ARPG Settlement Panel Refreshed")) void BP_OnSettlementPanelRefreshed(FARPGSettlementSummary Summary);

    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Settlement UI|Bindings") TObjectPtr<UTextBlock> SettlementTitleText;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Settlement UI|Bindings") TObjectPtr<UTextBlock> SettlementSummaryText;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Settlement UI|Bindings") TObjectPtr<UTextBlock> SettlementStockpileText;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Settlement UI|Bindings") TObjectPtr<UVerticalBox> ResidentListBox;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Settlement UI|Bindings") TObjectPtr<UButton> RefreshSettlementButton;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Settlement UI|Bindings") TObjectPtr<UButton> OpenStockpileButton;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Settlement UI|Bindings") TObjectPtr<UButton> CloseSettlementButton;
protected:
    virtual void NativeOnInitialized() override;
private:
    UPROPERTY(Transient) TObjectPtr<AARPGCharacter> Character = nullptr;
    UPROPERTY(Transient) TObjectPtr<AARPGSettlementHubActor> Hub = nullptr;
    UPROPERTY(Transient) TObjectPtr<UARPGSettlementUIComponent> SettlementUI = nullptr;
    UFUNCTION() void HandleRefresh();
    UFUNCTION() void HandleOpenStockpile();
    UFUNCTION() void HandleClose();
    void EnsureNativeLayoutOrBindings();
};

UCLASS(BlueprintType, Blueprintable)
class AKUMASRPGFRAMEWORK_API UARPGBedPanelWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="ARPG|Settlement UI") void InitializeBedPanel(AARPGCharacter* InCharacter, AARPGBuildBedActor* InBed, UARPGSettlementUIComponent* InUI);
    UFUNCTION(BlueprintCallable, Category="ARPG|Settlement UI") void RefreshBedPanel();
    UFUNCTION(BlueprintImplementableEvent, Category="ARPG|Settlement UI", meta=(DisplayName="On ARPG Bed Panel Refreshed")) void BP_OnBedPanelRefreshed(EARPGBedRole BedRole, FARPGSettlementHomeValidation HomeValidation);

    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Settlement UI|Bindings") TObjectPtr<UTextBlock> BedTitleText;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Settlement UI|Bindings") TObjectPtr<UTextBlock> BedRoleText;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Settlement UI|Bindings") TObjectPtr<UTextBlock> BedHomeStatusText;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Settlement UI|Bindings") TObjectPtr<UTextBlock> BedOccupantText;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Settlement UI|Bindings") TObjectPtr<UButton> PlayerBedButton;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Settlement UI|Bindings") TObjectPtr<UButton> VillagerBedButton;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Settlement UI|Bindings") TObjectPtr<UButton> UnassignedBedButton;
    UPROPERTY(BlueprintReadOnly, Transient, Category="ARPG|Settlement UI|Bindings") TObjectPtr<UButton> CloseBedButton;
protected:
    virtual void NativeOnInitialized() override;
private:
    UPROPERTY(Transient) TObjectPtr<AARPGCharacter> Character = nullptr;
    UPROPERTY(Transient) TObjectPtr<AARPGBuildBedActor> Bed = nullptr;
    UPROPERTY(Transient) TObjectPtr<UARPGSettlementUIComponent> SettlementUI = nullptr;
    UFUNCTION() void HandlePlayerBed();
    UFUNCTION() void HandleVillagerBed();
    UFUNCTION() void HandleUnassignedBed();
    UFUNCTION() void HandleClose();
    void EnsureNativeLayoutOrBindings();
};
