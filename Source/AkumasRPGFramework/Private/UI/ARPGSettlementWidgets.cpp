#include "UI/ARPGSettlementWidgets.h"

#include "Actors/ARPGCharacter.h"
#include "Blueprint/WidgetTree.h"
#include "Components/ARPGInventoryComponent.h"
#include "Components/ARPGSettlementUIComponent.h"
#include "Components/ARPGInteractionComponent.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Data/ARPGBuildPieceDefinition.h"
#include "Settlement/ARPGBuildBedActor.h"
#include "Settlement/ARPGSettlementHubActor.h"
#include "Settlement/ARPGSettlementResidentComponent.h"
#include "Settlement/ARPGSettlementVillagerCharacter.h"

namespace
{
    UTextBlock* MakeSettlementText(UWidgetTree* Tree, FName Name, const FText& Text, int32 Size=12, FLinearColor Color=FLinearColor::White)
    {
        UTextBlock* T=Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),Name);
        T->SetText(Text); T->SetColorAndOpacity(Color); T->SetAutoWrapText(true);
        FSlateFontInfo Font=T->GetFont(); Font.Size=Size; T->SetFont(Font); return T;
    }

    UBorder* MakeSettlementPanel(UWidgetTree* Tree, FName Name)
    {
        UBorder* B=Tree->ConstructWidget<UBorder>(UBorder::StaticClass(),Name);
        B->SetPadding(FMargin(16)); B->SetBrushColor(FLinearColor(.018f,.024f,.032f,.94f)); return B;
    }

    UButton* MakeLabeledButton(UWidgetTree* Tree, FName Name, const FText& Label)
    {
        UButton* B=Tree->ConstructWidget<UButton>(UButton::StaticClass(),Name);
        B->AddChild(MakeSettlementText(Tree,NAME_None,Label,11,FLinearColor(.93f,.80f,.36f,1.f))); return B;
    }

    FText ResidentStateText(EARPGSettlementResidentState State)
    {
        switch(State)
        {
            case EARPGSettlementResidentState::AtHome: return NSLOCTEXT("AkumasRPGFramework","ResidentAtHome","At Home");
            case EARPGSettlementResidentState::Roaming: return NSLOCTEXT("AkumasRPGFramework","ResidentRoaming","Roaming");
            case EARPGSettlementResidentState::GoingToWork: return NSLOCTEXT("AkumasRPGFramework","ResidentGoingToWork","Going to Work");
            case EARPGSettlementResidentState::Woodcutting: return NSLOCTEXT("AkumasRPGFramework","ResidentWoodcutting","Woodcutting");
            case EARPGSettlementResidentState::ReturningHome: return NSLOCTEXT("AkumasRPGFramework","ResidentReturningHome","Returning Home");
            default: return NSLOCTEXT("AkumasRPGFramework","ResidentHomeless","Homeless");
        }
    }

    FText FormatBedRoleText(EARPGBedRole Role)
    {
        switch(Role)
        {
            case EARPGBedRole::Player: return NSLOCTEXT("AkumasRPGFramework","PlayerBedRole","PLAYER BED");
            case EARPGBedRole::Villager: return NSLOCTEXT("AkumasRPGFramework","VillagerBedRole","VILLAGER BED");
            default: return NSLOCTEXT("AkumasRPGFramework","UnassignedBedRole","UNASSIGNED BED");
        }
    }

    FARPGSettlementResidentView MakeResidentView(AARPGSettlementVillagerCharacter* Resident)
    {
        FARPGSettlementResidentView V; V.Resident=Resident;
        if (!Resident || !Resident->SettlementResident) return V;
        V.ResidentId=Resident->SettlementResident->ResidentId;
        V.DisplayName=FText::FromString(Resident->RPGCharacterName.IsEmpty()?TEXT("Villager"):Resident->RPGCharacterName);
        V.State=Resident->SettlementResident->ResidentState; V.StateText=ResidentStateText(V.State);
        V.bWorking=Resident->SettlementResident->IsWorking(); V.bHasValidHome=Resident->SettlementResident->HasValidHome();
        if (AARPGBuildBedActor* Bed=Resident->SettlementResident->AssignedBed)
            V.BedText=Bed->Definition && !Bed->Definition->DisplayName.IsEmpty()?Bed->Definition->DisplayName:NSLOCTEXT("AkumasRPGFramework","AssignedVillagerBed","Assigned Bed");
        else V.BedText=NSLOCTEXT("AkumasRPGFramework","NoAssignedBed","No Bed");
        return V;
    }

    UCanvasPanel* MakeCenteredCanvasPanel(UWidgetTree* Tree, UBorder*& OutPanel, FVector2D Size)
    {
        UCanvasPanel* Canvas=Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(),TEXT("SettlementRoot"));
        OutPanel=MakeSettlementPanel(Tree,TEXT("SettlementPanelBorder"));
        UCanvasPanelSlot* CenterPanelSlot=Canvas->AddChildToCanvas(OutPanel);
        CenterPanelSlot->SetAnchors(FAnchors(.5f,.5f)); CenterPanelSlot->SetAlignment(FVector2D(.5f,.5f)); CenterPanelSlot->SetPosition(FVector2D::ZeroVector); CenterPanelSlot->SetSize(Size);
        return Canvas;
    }
}

void UARPGSettlementHUDWidget::InitializeSettlementHUD(AARPGCharacter* InCharacter,AARPGSettlementHubActor* InHub){Character=InCharacter;Hub=InHub;EnsureNativeLayoutOrBindings();RefreshSettlementHUD();}
void UARPGSettlementHUDWidget::SetSettlementHub(AARPGSettlementHubActor* InHub){Hub=InHub;RefreshSettlementHUD();}
void UARPGSettlementHUDWidget::EnsureNativeLayoutOrBindings()
{
    if(!WidgetTree)return;
    if(!WidgetTree->RootWidget)
    {
        UCanvasPanel*Canvas=WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(),TEXT("SettlementHUDRoot"));WidgetTree->RootWidget=Canvas;
        UBorder*Panel=MakeSettlementPanel(WidgetTree,TEXT("SettlementHUDBorder")); UCanvasPanelSlot*HUDPanelSlot=Canvas->AddChildToCanvas(Panel);
        HUDPanelSlot->SetAnchors(FAnchors(.02f,.06f));HUDPanelSlot->SetAlignment(FVector2D(0,0));HUDPanelSlot->SetPosition(FVector2D::ZeroVector);HUDPanelSlot->SetSize(FVector2D(310,150));
        UVerticalBox*V=WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());Panel->SetContent(V);
        SettlementNameText=MakeSettlementText(WidgetTree,TEXT("SettlementNameText"),FText::GetEmpty(),18,FLinearColor(.95f,.79f,.30f,1));V->AddChildToVerticalBox(SettlementNameText);
        ResidentsText=MakeSettlementText(WidgetTree,TEXT("ResidentsText"),FText::GetEmpty());V->AddChildToVerticalBox(ResidentsText);
        HomesText=MakeSettlementText(WidgetTree,TEXT("HomesText"),FText::GetEmpty());V->AddChildToVerticalBox(HomesText);
        WorkersText=MakeSettlementText(WidgetTree,TEXT("WorkersText"),FText::GetEmpty());V->AddChildToVerticalBox(WorkersText);
        SettlementStatusText=MakeSettlementText(WidgetTree,TEXT("SettlementStatusText"),FText::GetEmpty(),10,FLinearColor(.62f,.75f,.65f,1));V->AddChildToVerticalBox(SettlementStatusText);
    }
    else
    {
        if(!SettlementNameText)SettlementNameText=Cast<UTextBlock>(GetWidgetFromName(TEXT("SettlementNameText")));
        if(!ResidentsText)ResidentsText=Cast<UTextBlock>(GetWidgetFromName(TEXT("ResidentsText")));
        if(!HomesText)HomesText=Cast<UTextBlock>(GetWidgetFromName(TEXT("HomesText")));
        if(!WorkersText)WorkersText=Cast<UTextBlock>(GetWidgetFromName(TEXT("WorkersText")));
        if(!SettlementStatusText)SettlementStatusText=Cast<UTextBlock>(GetWidgetFromName(TEXT("SettlementStatusText")));
    }
}
void UARPGSettlementHUDWidget::RefreshSettlementHUD()
{
    EnsureNativeLayoutOrBindings();if(!Hub)return;const FARPGSettlementSummary S=Hub->GetSettlementSummary();
    if(SettlementNameText)SettlementNameText->SetText(S.SettlementName);
    if(ResidentsText)ResidentsText->SetText(FText::Format(NSLOCTEXT("AkumasRPGFramework","SettlementHUDResidents","Residents  {0} / {1}"),FText::AsNumber(S.ResidentCount),FText::AsNumber(S.ResidentCapacity)));
    if(HomesText)HomesText->SetText(FText::Format(NSLOCTEXT("AkumasRPGFramework","SettlementHUDHomes","Valid Homes  {0}   Villager Beds  {1}"),FText::AsNumber(S.ValidHomeCount),FText::AsNumber(S.VillagerBedCount)));
    if(WorkersText)WorkersText->SetText(FText::Format(NSLOCTEXT("AkumasRPGFramework","SettlementHUDWorkers","Woodcutters  {0}"),FText::AsNumber(S.WoodcuttersActive)));
    if(SettlementStatusText)SettlementStatusText->SetText(S.bSettlementOperational?NSLOCTEXT("AkumasRPGFramework","SettlementOnline","SETTLEMENT ACTIVE"):NSLOCTEXT("AkumasRPGFramework","SettlementOffline","SETTLEMENT INACTIVE"));
    BP_OnSettlementHUDRefreshed(S);
}

void UARPGSettlementResidentRowWidget::NativeOnInitialized(){Super::NativeOnInitialized();EnsureNativeLayoutOrBindings();}
void UARPGSettlementResidentRowWidget::SetResidentView(const FARPGSettlementResidentView& InView){View=InView;ApplyView();BP_OnResidentRowUpdated(View);}
void UARPGSettlementResidentRowWidget::EnsureNativeLayoutOrBindings()
{
    if(!WidgetTree)return;if(!WidgetTree->RootWidget)
    {
        UBorder*B=WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(),TEXT("ResidentRowBorder"));B->SetPadding(FMargin(8));B->SetBrushColor(FLinearColor(.035f,.045f,.06f,.90f));WidgetTree->RootWidget=B;
        UHorizontalBox*H=WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());B->SetContent(H);
        ResidentNameText=MakeSettlementText(WidgetTree,TEXT("ResidentNameText"),FText::GetEmpty(),12,FLinearColor(.95f,.80f,.34f,1));if(UHorizontalBoxSlot*S=H->AddChildToHorizontalBox(ResidentNameText)){FSlateChildSize F;F.SizeRule=ESlateSizeRule::Fill;S->SetSize(F);}
        ResidentStateText=MakeSettlementText(WidgetTree,TEXT("ResidentStateText"),FText::GetEmpty(),10);if(UHorizontalBoxSlot*S=H->AddChildToHorizontalBox(ResidentStateText)){S->SetPadding(FMargin(8,0));}
        ResidentBedText=MakeSettlementText(WidgetTree,TEXT("ResidentBedText"),FText::GetEmpty(),10,FLinearColor(.65f,.70f,.78f,1));H->AddChildToHorizontalBox(ResidentBedText);
    }
    else{if(!ResidentNameText)ResidentNameText=Cast<UTextBlock>(GetWidgetFromName(TEXT("ResidentNameText")));if(!ResidentStateText)ResidentStateText=Cast<UTextBlock>(GetWidgetFromName(TEXT("ResidentStateText")));if(!ResidentBedText)ResidentBedText=Cast<UTextBlock>(GetWidgetFromName(TEXT("ResidentBedText")));}
}
void UARPGSettlementResidentRowWidget::ApplyView(){EnsureNativeLayoutOrBindings();if(ResidentNameText)ResidentNameText->SetText(View.DisplayName);if(ResidentStateText)ResidentStateText->SetText(View.StateText);if(ResidentBedText)ResidentBedText->SetText(View.BedText);}

void UARPGSettlementPanelWidget::NativeOnInitialized(){Super::NativeOnInitialized();EnsureNativeLayoutOrBindings();if(RefreshSettlementButton)RefreshSettlementButton->OnClicked.AddUniqueDynamic(this,&UARPGSettlementPanelWidget::HandleRefresh);if(OpenStockpileButton)OpenStockpileButton->OnClicked.AddUniqueDynamic(this,&UARPGSettlementPanelWidget::HandleOpenStockpile);if(CloseSettlementButton)CloseSettlementButton->OnClicked.AddUniqueDynamic(this,&UARPGSettlementPanelWidget::HandleClose);}
void UARPGSettlementPanelWidget::InitializeSettlementPanel(AARPGCharacter*InCharacter,AARPGSettlementHubActor*InHub,UARPGSettlementUIComponent*InUI){Character=InCharacter;Hub=InHub;SettlementUI=InUI;EnsureNativeLayoutOrBindings();if(RefreshSettlementButton)RefreshSettlementButton->OnClicked.AddUniqueDynamic(this,&UARPGSettlementPanelWidget::HandleRefresh);if(OpenStockpileButton)OpenStockpileButton->OnClicked.AddUniqueDynamic(this,&UARPGSettlementPanelWidget::HandleOpenStockpile);if(CloseSettlementButton)CloseSettlementButton->OnClicked.AddUniqueDynamic(this,&UARPGSettlementPanelWidget::HandleClose);RefreshSettlementPanel();}
void UARPGSettlementPanelWidget::EnsureNativeLayoutOrBindings()
{
    if(!WidgetTree)return;if(!WidgetTree->RootWidget)
    {
        UBorder*Panel=nullptr;WidgetTree->RootWidget=MakeCenteredCanvasPanel(WidgetTree,Panel,FVector2D(760,610));UVerticalBox*Main=WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());Panel->SetContent(Main);
        UHorizontalBox*Header=WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());Main->AddChildToVerticalBox(Header);
        SettlementTitleText=MakeSettlementText(WidgetTree,TEXT("SettlementTitleText"),FText::GetEmpty(),23,FLinearColor(.95f,.78f,.28f,1));if(UHorizontalBoxSlot*S=Header->AddChildToHorizontalBox(SettlementTitleText)){FSlateChildSize F;F.SizeRule=ESlateSizeRule::Fill;S->SetSize(F);}
        RefreshSettlementButton=MakeLabeledButton(WidgetTree,TEXT("RefreshSettlementButton"),NSLOCTEXT("AkumasRPGFramework","SettlementRefresh","Refresh"));Header->AddChildToHorizontalBox(RefreshSettlementButton);
        OpenStockpileButton=MakeLabeledButton(WidgetTree,TEXT("OpenStockpileButton"),NSLOCTEXT("AkumasRPGFramework","SettlementStockpile","Stockpile"));if(UHorizontalBoxSlot*S=Header->AddChildToHorizontalBox(OpenStockpileButton))S->SetPadding(FMargin(6,0,0,0));
        CloseSettlementButton=MakeLabeledButton(WidgetTree,TEXT("CloseSettlementButton"),NSLOCTEXT("AkumasRPGFramework","SettlementClose","Close"));if(UHorizontalBoxSlot*S=Header->AddChildToHorizontalBox(CloseSettlementButton))S->SetPadding(FMargin(6,0,0,0));
        SettlementSummaryText=MakeSettlementText(WidgetTree,TEXT("SettlementSummaryText"),FText::GetEmpty(),12);if(UVerticalBoxSlot*S=Main->AddChildToVerticalBox(SettlementSummaryText))S->SetPadding(FMargin(0,12,0,6));
        SettlementStockpileText=MakeSettlementText(WidgetTree,TEXT("SettlementStockpileText"),FText::GetEmpty(),11,FLinearColor(.72f,.76f,.84f,1));Main->AddChildToVerticalBox(SettlementStockpileText);
        Main->AddChildToVerticalBox(MakeSettlementText(WidgetTree,NAME_None,NSLOCTEXT("AkumasRPGFramework","SettlementResidentsHeader","RESIDENTS"),14,FLinearColor(.95f,.78f,.28f,1)));
        UScrollBox*Scroll=WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());if(UVerticalBoxSlot*S=Main->AddChildToVerticalBox(Scroll)){FSlateChildSize F;F.SizeRule=ESlateSizeRule::Fill;S->SetSize(F);S->SetPadding(FMargin(0,6,0,0));}
        ResidentListBox=WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(),TEXT("ResidentListBox"));Scroll->AddChild(ResidentListBox);
        Main->AddChildToVerticalBox(MakeSettlementText(WidgetTree,NAME_None,NSLOCTEXT("AkumasRPGFramework","SettlementPanelHelp","Villager Beds inside complete configured homes (2x2+ by default) recruit automatically. Residents roam the settlement and can harvest nearby ARPG trees into the Hub stockpile."),10,FLinearColor(.55f,.61f,.70f,1)));
    }
    else{if(!SettlementTitleText)SettlementTitleText=Cast<UTextBlock>(GetWidgetFromName(TEXT("SettlementTitleText")));if(!SettlementSummaryText)SettlementSummaryText=Cast<UTextBlock>(GetWidgetFromName(TEXT("SettlementSummaryText")));if(!SettlementStockpileText)SettlementStockpileText=Cast<UTextBlock>(GetWidgetFromName(TEXT("SettlementStockpileText")));if(!ResidentListBox)ResidentListBox=Cast<UVerticalBox>(GetWidgetFromName(TEXT("ResidentListBox")));if(!RefreshSettlementButton)RefreshSettlementButton=Cast<UButton>(GetWidgetFromName(TEXT("RefreshSettlementButton")));if(!OpenStockpileButton)OpenStockpileButton=Cast<UButton>(GetWidgetFromName(TEXT("OpenStockpileButton")));if(!CloseSettlementButton)CloseSettlementButton=Cast<UButton>(GetWidgetFromName(TEXT("CloseSettlementButton")));}
}
void UARPGSettlementPanelWidget::RefreshSettlementPanel()
{
    EnsureNativeLayoutOrBindings();if(!Hub)return;const FARPGSettlementSummary S=Hub->GetSettlementSummary();if(SettlementTitleText)SettlementTitleText->SetText(S.SettlementName);
    if(SettlementSummaryText)SettlementSummaryText->SetText(FText::Format(NSLOCTEXT("AkumasRPGFramework","SettlementPanelSummary","Residents {0}/{1}  •  Villager Beds {2}  •  Valid Homes {3}  •  Woodcutters {4}"),FText::AsNumber(S.ResidentCount),FText::AsNumber(S.ResidentCapacity),FText::AsNumber(S.VillagerBedCount),FText::AsNumber(S.ValidHomeCount),FText::AsNumber(S.WoodcuttersActive)));
    int32 Slots=0,Qty=0;if(Hub->Inventory){Slots=Hub->Inventory->Items.Num();for(const FARPGInventoryEntry&E:Hub->Inventory->Items)Qty+=FMath::Max(0,E.Quantity);}if(SettlementStockpileText)SettlementStockpileText->SetText(FText::Format(NSLOCTEXT("AkumasRPGFramework","SettlementStockpileSummary","Hub Stockpile: {0} occupied stacks • {1} total items"),FText::AsNumber(Slots),FText::AsNumber(Qty)));
    if(ResidentListBox){ResidentListBox->ClearChildren();TArray<AARPGSettlementVillagerCharacter*>Residents;Hub->GetSettlementResidents(Residents);TSubclassOf<UARPGSettlementResidentRowWidget>RowClass;if(SettlementUI)RowClass=SettlementUI->GetResidentRowWidgetClass();if(!RowClass)RowClass=UARPGSettlementResidentRowWidget::StaticClass();for(AARPGSettlementVillagerCharacter*R:Residents){UARPGSettlementResidentRowWidget*Row=CreateWidget<UARPGSettlementResidentRowWidget>(GetOwningPlayer(),RowClass);if(Row){Row->SetResidentView(MakeResidentView(R));ResidentListBox->AddChildToVerticalBox(Row);}}}
    BP_OnSettlementPanelRefreshed(S);
}
void UARPGSettlementPanelWidget::HandleRefresh(){if(SettlementUI&&Hub)SettlementUI->RequestSettlementRefresh(Hub);}
void UARPGSettlementPanelWidget::HandleOpenStockpile(){if(SettlementUI&&Hub)SettlementUI->OpenSettlementStockpile(Hub);}
void UARPGSettlementPanelWidget::HandleClose(){if(SettlementUI)SettlementUI->CloseSettlementPanel();}

void UARPGBedPanelWidget::NativeOnInitialized(){Super::NativeOnInitialized();EnsureNativeLayoutOrBindings();if(PlayerBedButton)PlayerBedButton->OnClicked.AddUniqueDynamic(this,&UARPGBedPanelWidget::HandlePlayerBed);if(VillagerBedButton)VillagerBedButton->OnClicked.AddUniqueDynamic(this,&UARPGBedPanelWidget::HandleVillagerBed);if(UnassignedBedButton)UnassignedBedButton->OnClicked.AddUniqueDynamic(this,&UARPGBedPanelWidget::HandleUnassignedBed);if(CloseBedButton)CloseBedButton->OnClicked.AddUniqueDynamic(this,&UARPGBedPanelWidget::HandleClose);}
void UARPGBedPanelWidget::InitializeBedPanel(AARPGCharacter*InCharacter,AARPGBuildBedActor*InBed,UARPGSettlementUIComponent*InUI){Character=InCharacter;Bed=InBed;SettlementUI=InUI;EnsureNativeLayoutOrBindings();if(PlayerBedButton)PlayerBedButton->OnClicked.AddUniqueDynamic(this,&UARPGBedPanelWidget::HandlePlayerBed);if(VillagerBedButton)VillagerBedButton->OnClicked.AddUniqueDynamic(this,&UARPGBedPanelWidget::HandleVillagerBed);if(UnassignedBedButton)UnassignedBedButton->OnClicked.AddUniqueDynamic(this,&UARPGBedPanelWidget::HandleUnassignedBed);if(CloseBedButton)CloseBedButton->OnClicked.AddUniqueDynamic(this,&UARPGBedPanelWidget::HandleClose);RefreshBedPanel();}
void UARPGBedPanelWidget::EnsureNativeLayoutOrBindings()
{
    if(!WidgetTree)return;if(!WidgetTree->RootWidget)
    {
        UBorder*Panel=nullptr;WidgetTree->RootWidget=MakeCenteredCanvasPanel(WidgetTree,Panel,FVector2D(560,390));UVerticalBox*Main=WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());Panel->SetContent(Main);
        BedTitleText=MakeSettlementText(WidgetTree,TEXT("BedTitleText"),NSLOCTEXT("AkumasRPGFramework","BedPanelTitle","BED ASSIGNMENT"),22,FLinearColor(.95f,.78f,.28f,1));Main->AddChildToVerticalBox(BedTitleText);
        BedRoleText=MakeSettlementText(WidgetTree,TEXT("BedRoleText"),FText::GetEmpty(),15);if(UVerticalBoxSlot*S=Main->AddChildToVerticalBox(BedRoleText))S->SetPadding(FMargin(0,12,0,4));
        BedHomeStatusText=MakeSettlementText(WidgetTree,TEXT("BedHomeStatusText"),FText::GetEmpty(),11,FLinearColor(.70f,.75f,.82f,1));Main->AddChildToVerticalBox(BedHomeStatusText);
        BedOccupantText=MakeSettlementText(WidgetTree,TEXT("BedOccupantText"),FText::GetEmpty(),11);if(UVerticalBoxSlot*S=Main->AddChildToVerticalBox(BedOccupantText))S->SetPadding(FMargin(0,6,0,12));
        UHorizontalBox*Buttons=WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());Main->AddChildToVerticalBox(Buttons);
        PlayerBedButton=MakeLabeledButton(WidgetTree,TEXT("PlayerBedButton"),NSLOCTEXT("AkumasRPGFramework","SetPlayerBed","Player Bed"));Buttons->AddChildToHorizontalBox(PlayerBedButton);
        VillagerBedButton=MakeLabeledButton(WidgetTree,TEXT("VillagerBedButton"),NSLOCTEXT("AkumasRPGFramework","SetVillagerBed","Villager Bed"));if(UHorizontalBoxSlot*S=Buttons->AddChildToHorizontalBox(VillagerBedButton))S->SetPadding(FMargin(6,0));
        UnassignedBedButton=MakeLabeledButton(WidgetTree,TEXT("UnassignedBedButton"),NSLOCTEXT("AkumasRPGFramework","SetUnassignedBed","Unassigned"));Buttons->AddChildToHorizontalBox(UnassignedBedButton);
        CloseBedButton=MakeLabeledButton(WidgetTree,TEXT("CloseBedButton"),NSLOCTEXT("AkumasRPGFramework","BedClose","Close"));if(UVerticalBoxSlot*S=Main->AddChildToVerticalBox(CloseBedButton))S->SetPadding(FMargin(0,14,0,0));
        Main->AddChildToVerticalBox(MakeSettlementText(WidgetTree,NAME_None,NSLOCTEXT("AkumasRPGFramework","BedPanelHelp","Villager Beds recruit only when a Settlement Hub manages this building and the home passes the configured Foundation footprint (2x2+ by default), overhead cover, perimeter Wall, Doorway and Door validation."),9,FLinearColor(.52f,.58f,.67f,1)));
    }
    else{if(!BedTitleText)BedTitleText=Cast<UTextBlock>(GetWidgetFromName(TEXT("BedTitleText")));if(!BedRoleText)BedRoleText=Cast<UTextBlock>(GetWidgetFromName(TEXT("BedRoleText")));if(!BedHomeStatusText)BedHomeStatusText=Cast<UTextBlock>(GetWidgetFromName(TEXT("BedHomeStatusText")));if(!BedOccupantText)BedOccupantText=Cast<UTextBlock>(GetWidgetFromName(TEXT("BedOccupantText")));if(!PlayerBedButton)PlayerBedButton=Cast<UButton>(GetWidgetFromName(TEXT("PlayerBedButton")));if(!VillagerBedButton)VillagerBedButton=Cast<UButton>(GetWidgetFromName(TEXT("VillagerBedButton")));if(!UnassignedBedButton)UnassignedBedButton=Cast<UButton>(GetWidgetFromName(TEXT("UnassignedBedButton")));if(!CloseBedButton)CloseBedButton=Cast<UButton>(GetWidgetFromName(TEXT("CloseBedButton")));}
}
void UARPGBedPanelWidget::RefreshBedPanel()
{
    EnsureNativeLayoutOrBindings();if(!Bed)return;if(BedTitleText)BedTitleText->SetText(Bed->Definition&&!Bed->Definition->DisplayName.IsEmpty()?Bed->Definition->DisplayName:NSLOCTEXT("AkumasRPGFramework","BedTitleFallback","BED ASSIGNMENT"));if(BedRoleText)BedRoleText->SetText(FormatBedRoleText(Bed->BedRole));
    FARPGSettlementHomeValidation Validation;Bed->GetCurrentHomeValidation(Validation);if(BedHomeStatusText)BedHomeStatusText->SetText(Validation.StatusText);
    FText Occupant=NSLOCTEXT("AkumasRPGFramework","BedNoOccupant","Occupant: None");if(Bed->AssignedResidentId.IsValid())if(AARPGSettlementHubActor*Hub=Bed->FindManagingSettlementHub())if(AARPGSettlementVillagerCharacter*Resident=Hub->FindResidentById(Bed->AssignedResidentId))Occupant=FText::Format(NSLOCTEXT("AkumasRPGFramework","BedOccupantFmt","Occupant: {0}"),FText::FromString(Resident->RPGCharacterName));if(BedOccupantText)BedOccupantText->SetText(Occupant);
    if(PlayerBedButton)PlayerBedButton->SetIsEnabled(Bed->BedRole!=EARPGBedRole::Player);if(VillagerBedButton)VillagerBedButton->SetIsEnabled(Bed->BedRole!=EARPGBedRole::Villager);if(UnassignedBedButton)UnassignedBedButton->SetIsEnabled(Bed->BedRole!=EARPGBedRole::Unassigned);
    BP_OnBedPanelRefreshed(Bed->BedRole,Validation);
}
void UARPGBedPanelWidget::HandlePlayerBed(){if(SettlementUI&&Bed){SettlementUI->SetBedRole(Bed,EARPGBedRole::Player);RefreshBedPanel();}}
void UARPGBedPanelWidget::HandleVillagerBed(){if(SettlementUI&&Bed){SettlementUI->SetBedRole(Bed,EARPGBedRole::Villager);RefreshBedPanel();}}
void UARPGBedPanelWidget::HandleUnassignedBed(){if(SettlementUI&&Bed){SettlementUI->SetBedRole(Bed,EARPGBedRole::Unassigned);RefreshBedPanel();}}
void UARPGBedPanelWidget::HandleClose(){if(SettlementUI)SettlementUI->CloseBedPanel();}
