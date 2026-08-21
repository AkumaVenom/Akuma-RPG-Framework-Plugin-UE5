#include "Components/ARPGSettlementUIComponent.h"

#include "Actors/ARPGCharacter.h"
#include "Building/ARPGBuildingComponent.h"
#include "Components/ARPGBuildingUIComponent.h"
#include "Components/ARPGInteractionComponent.h"
#include "Components/ARPGInventoryUIComponent.h"
#include "Data/ARPGBuildPieceDefinition.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Settlement/ARPGBuildBedActor.h"
#include "Settlement/ARPGSettlementHubActor.h"
#include "TimerManager.h"
#include "UI/ARPGSettlementWidgets.h"

UARPGSettlementUIComponent::UARPGSettlementUIComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

bool UARPGSettlementUIComponent::IsSettlementPanelOpen() const
{
    return IsValid(ActiveSettlementPanel.Get());
}

bool UARPGSettlementUIComponent::IsBedPanelOpen() const
{
    return IsValid(ActiveBedPanel.Get());
}

bool UARPGSettlementUIComponent::ResolveLocalPlayer(AARPGCharacter*& OutCharacter, APlayerController*& OutController) const
{
    OutCharacter = Cast<AARPGCharacter>(GetOwner());
    OutController = OutCharacter ? Cast<APlayerController>(OutCharacter->GetController()) : nullptr;
    return OutCharacter && OutController && OutController->IsLocalController();
}

void UARPGSettlementUIComponent::BeginPlay()
{
    Super::BeginPlay();
    AARPGCharacter* Character=nullptr; APlayerController* Controller=nullptr;
    if (!ResolveLocalPlayer(Character, Controller) || !GetWorld()) return;
    CachedPlayerController=Controller;
    GetWorld()->GetTimerManager().SetTimer(ProximityTimer,this,&UARPGSettlementUIComponent::PollNearbySettlement,FMath::Max(0.1f,ProximityPollInterval),true,0.1f);
    PollNearbySettlement();
}

void UARPGSettlementUIComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(ProximityTimer);
    CloseAllSettlementUI();
    HideSettlementHUD();
    Super::EndPlay(EndPlayReason);
}

AARPGSettlementHubActor* UARPGSettlementUIComponent::FindNearestUsableSettlement(AARPGCharacter* Character) const
{
    if (!Character || !GetWorld()) return nullptr;
    AARPGSettlementHubActor* Best=nullptr; float BestSq=TNumericLimits<float>::Max();
    for (TActorIterator<AARPGSettlementHubActor> It(GetWorld()); It; ++It)
    {
        AARPGSettlementHubActor* Hub=*It;
        if (!Hub || !Hub->IsConstructionComplete() || !Hub->CanActorUse(Character)) continue;
        const float DistSq=FVector::DistSquared2D(Character->GetActorLocation(),Hub->GetActorLocation());
        if (DistSq>FMath::Square(Hub->GetSettlementHUDRadius()) || DistSq>=BestSq) continue;
        Best=Hub; BestSq=DistSq;
    }
    return Best;
}

void UARPGSettlementUIComponent::PollNearbySettlement()
{
    AARPGCharacter* Character=nullptr; APlayerController* Controller=nullptr;
    if (!ResolveLocalPlayer(Character, Controller)) { HideSettlementHUD(); return; }
    AARPGSettlementHubActor* NewHub=FindNearestUsableSettlement(Character);
    if (NearbySettlementHub.Get()!=NewHub)
    {
        NearbySettlementHub=NewHub;
        OnNearbySettlementChanged.Broadcast(NewHub);
    }
    if (bAutoShowNearbySettlementHUD && NewHub) ShowSettlementHUD(NewHub,Controller); else HideSettlementHUD();
    if (ActiveSettlementHUD) ActiveSettlementHUD->RefreshSettlementHUD();
    if (ActiveSettlementPanel) ActiveSettlementPanel->RefreshSettlementPanel();
    if (ActiveBedPanel) ActiveBedPanel->RefreshBedPanel();

    if (AARPGSettlementHubActor* PanelHub=PanelSettlementHub.Get())
    {
        const float MaxDist=FMath::Max(PanelHub->GetSettlementHUDRadius(), PanelHub->Definition ? PanelHub->Definition->SettlementHubInteractionRadius : 140.f);
        if (!PanelHub->CanActorUse(Character) || FVector::DistSquared2D(Character->GetActorLocation(),PanelHub->GetActorLocation())>FMath::Square(MaxDist)) CloseSettlementPanel();
    }
    if (AARPGBuildBedActor* Bed=PanelBed.Get())
    {
        const float MaxDist=Bed->Definition ? FMath::Max(100.f,Bed->Definition->BedInteractionRadius*5.f) : 650.f;
        if (!Bed->CanActorUse(Character) || FVector::DistSquared(Character->GetActorLocation(),Bed->GetActorLocation())>FMath::Square(MaxDist)) CloseBedPanel();
    }
}

void UARPGSettlementUIComponent::ShowSettlementHUD(AARPGSettlementHubActor* Hub, APlayerController* Controller)
{
    if (!Hub || !Controller) return;
    if (!ActiveSettlementHUD)
    {
        TSubclassOf<UARPGSettlementHUDWidget> Class=SettlementHUDWidgetClass;
        if (!Class) Class=UARPGSettlementHUDWidget::StaticClass();
        ActiveSettlementHUD=CreateWidget<UARPGSettlementHUDWidget>(Controller,Class);
        if (!ActiveSettlementHUD) return;
        if (!ActiveSettlementHUD->AddToPlayerScreen(SettlementHUDZOrder)) ActiveSettlementHUD->AddToViewport(SettlementHUDZOrder);
    }
    AARPGCharacter* Character=Cast<AARPGCharacter>(GetOwner());
    ActiveSettlementHUD->InitializeSettlementHUD(Character,Hub);
}

void UARPGSettlementUIComponent::HideSettlementHUD()
{
    if (ActiveSettlementHUD) ActiveSettlementHUD->RemoveFromParent();
    ActiveSettlementHUD=nullptr;
}

bool UARPGSettlementUIComponent::OpenSettlementPanel(AARPGSettlementHubActor* Hub)
{
    AARPGCharacter* Character=nullptr; APlayerController* Controller=nullptr;
    if (!ResolveLocalPlayer(Character,Controller) || !Hub || !Hub->IsConstructionComplete() || !Hub->CanActorUse(Character)) return false;
    const float Interaction=Hub->Definition?FMath::Max(10.f,Hub->Definition->SettlementHubInteractionRadius):140.f;
    const float MaxDist=FMath::Max(650.f,Interaction*5.f);
    if (FVector::DistSquared(Character->GetActorLocation(),Hub->GetActorLocation())>FMath::Square(MaxDist)) return false;
    CloseBedPanel(); CloseSettlementPanel();
    if (Character->BuildingUI) { Character->BuildingUI->CloseBuildMenu(); Character->BuildingUI->CloseStructureUI(); }
    if (Character->InventoryUI && Character->InventoryUI->IsInventoryUIOpen()) Character->InventoryUI->CloseInventoryUI();
    if (Character->Building && Character->Building->IsBuildModeActive()) Character->Building->EndBuildMode();
    TSubclassOf<UARPGSettlementPanelWidget> Class=SettlementPanelWidgetClass;
    if (!Class) Class=UARPGSettlementPanelWidget::StaticClass();
    ActiveSettlementPanel=CreateWidget<UARPGSettlementPanelWidget>(Controller,Class);
    if (!ActiveSettlementPanel) return false;
    PanelSettlementHub=Hub; CachedPlayerController=Controller;
    ActiveSettlementPanel->InitializeSettlementPanel(Character,Hub,this);
    if (!ActiveSettlementPanel->AddToPlayerScreen(SettlementPanelZOrder)) ActiveSettlementPanel->AddToViewport(SettlementPanelZOrder);
    ApplyMenuInputMode(Controller);
    return true;
}

bool UARPGSettlementUIComponent::OpenBedPanel(AARPGBuildBedActor* Bed)
{
    AARPGCharacter* Character=nullptr; APlayerController* Controller=nullptr;
    if (!ResolveLocalPlayer(Character,Controller) || !Bed || !Bed->IsConstructionComplete() || !Bed->CanActorUse(Character)) return false;
    const float Interaction=Bed->Definition?FMath::Max(10.f,Bed->Definition->BedInteractionRadius):100.f;
    const float MaxDist=FMath::Max(650.f,Interaction*5.f);
    if (FVector::DistSquared(Character->GetActorLocation(),Bed->GetActorLocation())>FMath::Square(MaxDist)) return false;
    CloseSettlementPanel(); CloseBedPanel();
    if (Character->BuildingUI) { Character->BuildingUI->CloseBuildMenu(); Character->BuildingUI->CloseStructureUI(); }
    if (Character->InventoryUI && Character->InventoryUI->IsInventoryUIOpen()) Character->InventoryUI->CloseInventoryUI();
    if (Character->Building && Character->Building->IsBuildModeActive()) Character->Building->EndBuildMode();
    TSubclassOf<UARPGBedPanelWidget> Class=BedPanelWidgetClass;
    if (!Class) Class=UARPGBedPanelWidget::StaticClass();
    ActiveBedPanel=CreateWidget<UARPGBedPanelWidget>(Controller,Class);
    if (!ActiveBedPanel) return false;
    PanelBed=Bed; CachedPlayerController=Controller;
    ActiveBedPanel->InitializeBedPanel(Character,Bed,this);
    if (!ActiveBedPanel->AddToPlayerScreen(BedPanelZOrder)) ActiveBedPanel->AddToViewport(BedPanelZOrder);
    ApplyMenuInputMode(Controller);
    return true;
}

bool UARPGSettlementUIComponent::CloseSettlementPanel()
{
    const bool bHad=IsValid(ActiveSettlementPanel.Get());
    if (ActiveSettlementPanel) ActiveSettlementPanel->RemoveFromParent();
    ActiveSettlementPanel=nullptr; PanelSettlementHub.Reset();
    if (!ActiveBedPanel) if (APlayerController* PC=CachedPlayerController.Get()) RestoreGameInputMode(PC);
    return bHad;
}

bool UARPGSettlementUIComponent::CloseBedPanel()
{
    const bool bHad=IsValid(ActiveBedPanel.Get());
    if (ActiveBedPanel) ActiveBedPanel->RemoveFromParent();
    ActiveBedPanel=nullptr; PanelBed.Reset();
    if (!ActiveSettlementPanel) if (APlayerController* PC=CachedPlayerController.Get()) RestoreGameInputMode(PC);
    return bHad;
}

bool UARPGSettlementUIComponent::CloseAllSettlementUI()
{
    const bool bHad=IsValid(ActiveSettlementPanel.Get())||IsValid(ActiveBedPanel.Get());
    if (ActiveSettlementPanel) ActiveSettlementPanel->RemoveFromParent();
    if (ActiveBedPanel) ActiveBedPanel->RemoveFromParent();
    ActiveSettlementPanel=nullptr; ActiveBedPanel=nullptr; PanelSettlementHub.Reset(); PanelBed.Reset();
    if (APlayerController* PC=CachedPlayerController.Get()) RestoreGameInputMode(PC);
    return bHad;
}

bool UARPGSettlementUIComponent::SetBedRole(AARPGBuildBedActor* Bed, EARPGBedRole NewRole)
{
    AARPGCharacter* Character=Cast<AARPGCharacter>(GetOwner());
    if (!Character || !Character->Interaction || !Bed) return false;
    Character->Interaction->SetBuiltBedRole(Bed,NewRole);
    RefreshOpenSettlementUI();
    return true;
}

bool UARPGSettlementUIComponent::RequestSettlementRefresh(AARPGSettlementHubActor* Hub)
{
    AARPGCharacter* Character=Cast<AARPGCharacter>(GetOwner());
    if (!Character || !Character->Interaction || !Hub) return false;
    Character->Interaction->RefreshSettlementHub(Hub);
    RefreshOpenSettlementUI();
    return true;
}

bool UARPGSettlementUIComponent::OpenSettlementStockpile(AARPGSettlementHubActor* Hub)
{
    AARPGCharacter* Character=nullptr; APlayerController* Controller=nullptr;
    if (!ResolveLocalPlayer(Character,Controller) || !Hub || !Hub->IsConstructionComplete() || !Hub->CanActorUse(Character) || !Character->BuildingUI) return false;
    const float Interaction=Hub->Definition?FMath::Max(10.f,Hub->Definition->SettlementHubInteractionRadius):140.f;
    const float MaxDist=FMath::Max(650.f,Interaction*5.f);
    if (FVector::DistSquared(Character->GetActorLocation(),Hub->GetActorLocation())>FMath::Square(MaxDist)) return false;
    CloseSettlementPanel();
    return Character->BuildingUI->OpenStorageUI(Hub);
}

void UARPGSettlementUIComponent::RefreshOpenSettlementUI()
{
    if (ActiveSettlementHUD) ActiveSettlementHUD->RefreshSettlementHUD();
    if (ActiveSettlementPanel) ActiveSettlementPanel->RefreshSettlementPanel();
    if (ActiveBedPanel) ActiveBedPanel->RefreshBedPanel();
}

void UARPGSettlementUIComponent::ApplyMenuInputMode(APlayerController* Controller)
{
    if (!Controller || !bManageInputMode) return;
    if (!bCursorStateCaptured) { bPreviousMouseCursor=Controller->bShowMouseCursor; bCursorStateCaptured=true; }
    FInputModeGameAndUI Mode; Mode.SetHideCursorDuringCapture(false); Controller->SetInputMode(Mode);
    if (bShowMouseCursorWhilePanelOpen) Controller->bShowMouseCursor=true;
}

void UARPGSettlementUIComponent::RestoreGameInputMode(APlayerController* Controller)
{
    if (!Controller || !bManageInputMode) return;
    FInputModeGameOnly Mode; Controller->SetInputMode(Mode);
    if (bCursorStateCaptured) Controller->bShowMouseCursor=bPreviousMouseCursor;
    bCursorStateCaptured=false;
}
