#include "UI/ARPGBuildingWidgets.h"

#include "Actors/ARPGCharacter.h"
#include "Blueprint/WidgetTree.h"
#include "Building/ARPGBuildingComponent.h"
#include "Components/ARPGBuildingUIComponent.h"
#include "Components/ARPGInteractionComponent.h"
#include "Components/ARPGInventoryComponent.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Crafting/ARPGCraftingStationActor.h"
#include "Crafting/ARPGStorageActor.h"
#include "Data/ARPGBuildPieceDefinition.h"
#include "Data/ARPGCraftingStationDefinition.h"
#include "Data/ARPGItemDefinition.h"
#include "Data/ARPGRecipeDefinition.h"
#include "Engine/Texture2D.h"

namespace
{
    void StyleBuildingText(UTextBlock* Text, int32 Size, const FLinearColor& Color = FLinearColor::White)
    {
        if (!Text) return;
        FSlateFontInfo Font = Text->GetFont(); Font.Size = Size; Text->SetFont(Font);
        Text->SetColorAndOpacity(FSlateColor(Color));
    }

    UTextBlock* MakeBuildText(UWidgetTree* Tree, const FName Name, const FText& Value, int32 Size = 12, const FLinearColor& Color = FLinearColor::White)
    {
        UTextBlock* Text = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
        Text->SetText(Value); Text->SetAutoWrapText(true); StyleBuildingText(Text, Size, Color); return Text;
    }

    FText BuildItemName(const FARPGItemAmount& Amount)
    {
        if (Amount.Item && !Amount.Item->DisplayName.IsEmpty()) return Amount.Item->DisplayName;
        if (Amount.Item && !Amount.Item->DefinitionId.IsNone()) return FText::FromName(Amount.Item->DefinitionId);
        return Amount.ItemId.IsNone() ? NSLOCTEXT("AkumasRPGFramework", "BuildUnknownItem", "Unknown Item") : FText::FromName(Amount.ItemId);
    }

    FText BuildAmountList(const TArray<FARPGItemAmount>& Amounts)
    {
        if (Amounts.Num() == 0) return NSLOCTEXT("AkumasRPGFramework", "BuildFree", "No materials");
        FString Out;
        for (const FARPGItemAmount& Amount : Amounts)
        {
            if (Amount.Quantity <= 0) continue;
            if (!Out.IsEmpty()) Out += TEXT("  •  ");
            Out += FString::Printf(TEXT("%s x%d"), *BuildItemName(Amount).ToString(), Amount.Quantity);
        }
        return FText::FromString(Out.IsEmpty() ? TEXT("No materials") : Out);
    }

    FText DefinitionName(const UARPGDefinitionBase* Definition, FName Fallback = NAME_None)
    {
        if (Definition && !Definition->DisplayName.IsEmpty()) return Definition->DisplayName;
        if (Definition && !Definition->DefinitionId.IsNone()) return FText::FromName(Definition->DefinitionId);
        return Fallback.IsNone() ? FText::GetEmpty() : FText::FromName(Fallback);
    }

    FText PlacementResultText(EARPGPlacementResult Result)
    {
        switch (Result)
        {
            case EARPGPlacementResult::Valid: return NSLOCTEXT("AkumasRPGFramework", "BuildValid", "VALID PLACEMENT");
            case EARPGPlacementResult::TooFar: return NSLOCTEXT("AkumasRPGFramework", "BuildTooFar", "Too far away");
            case EARPGPlacementResult::Blocked: return NSLOCTEXT("AkumasRPGFramework", "BuildBlocked", "Blocked by another object");
            case EARPGPlacementResult::Unsupported: return NSLOCTEXT("AkumasRPGFramework", "BuildUnsupported", "Needs structural support / snap point");
            case EARPGPlacementResult::MissingResources: return NSLOCTEXT("AkumasRPGFramework", "BuildMissing", "Missing required materials");
            case EARPGPlacementResult::Restricted: return NSLOCTEXT("AkumasRPGFramework", "BuildRestricted", "Building is restricted here");
            case EARPGPlacementResult::InvalidSurface: return NSLOCTEXT("AkumasRPGFramework", "BuildInvalidSurface", "Invalid surface");
            case EARPGPlacementResult::PathSegmentTooShort: return NSLOCTEXT("AkumasRPGFramework", "BuildPathTooShort", "Path point is too close to the previous point");
            case EARPGPlacementResult::PathSegmentTooLong: return NSLOCTEXT("AkumasRPGFramework", "BuildPathTooLong", "Path point is too far from the previous point");
            default: return NSLOCTEXT("AkumasRPGFramework", "BuildNoPiece", "Select a build piece");
        }
    }

    FARPGStructureItemView MakeItemView(UARPGInventoryComponent* Inventory, const FARPGInventoryEntry& Entry, bool bPlayer, bool bOutput)
    {
        FARPGStructureItemView View;
        View.ItemId = Entry.ItemId; View.InstanceId = Entry.InstanceId; View.Quantity = Entry.Quantity;
        View.ItemDefinition = Inventory ? Inventory->ResolveItemDefinition(Entry) : nullptr;
        View.DisplayName = View.ItemDefinition ? DefinitionName(View.ItemDefinition, Entry.ItemId) : FText::FromName(Entry.ItemId);
        View.bFromPlayerInventory = bPlayer; View.bStationOutput = bOutput;
        return View;
    }

    void PopulateItemList(UWidgetTree* Tree, UVerticalBox* Box, UARPGInventoryComponent* Inventory, bool bPlayer, bool bOutput,
        TSubclassOf<UARPGStructureItemRowWidget> RowClass, UUserWidget* OwnerPanel)
    {
        if (!Tree || !Box) return;
        Box->ClearChildren();
        if (!Inventory || Inventory->Items.Num() == 0)
        {
            Box->AddChildToVerticalBox(MakeBuildText(Tree, NAME_None, NSLOCTEXT("AkumasRPGFramework", "StructureEmpty", "Empty"), 11, FLinearColor(0.52f,0.56f,0.64f,1.f)));
            return;
        }
        if (!RowClass) RowClass = UARPGStructureItemRowWidget::StaticClass();
        for (const FARPGInventoryEntry& Entry : Inventory->Items)
        {
            if (Entry.Quantity <= 0) continue;
            UARPGStructureItemRowWidget* Row = Tree->ConstructWidget<UARPGStructureItemRowWidget>(RowClass);
            Row->InitializeStructureItemRow(OwnerPanel, MakeItemView(Inventory, Entry, bPlayer, bOutput));
            if (UVerticalBoxSlot* S = Box->AddChildToVerticalBox(Row)) S->SetPadding(FMargin(0.f,0.f,0.f,4.f));
        }
    }

    FText RecipeAmountList(const TArray<FARPGItemAmount>& Amounts)
    {
        FString Out;
        for (const FARPGItemAmount& A : Amounts)
        {
            if (A.Quantity <= 0) continue;
            if (!Out.IsEmpty()) Out += TEXT(", ");
            Out += FString::Printf(TEXT("%s x%d"), *BuildItemName(A).ToString(), A.Quantity);
        }
        return FText::FromString(Out.IsEmpty() ? TEXT("None") : Out);
    }

    UBorder* MakePanelBorder(UWidgetTree* Tree, const FName Name)
    {
        UBorder* Border = Tree->ConstructWidget<UBorder>(UBorder::StaticClass(), Name);
        Border->SetPadding(FMargin(16.f)); Border->SetBrushColor(FLinearColor(0.012f,0.015f,0.024f,0.975f));
        Border->SetVisibility(ESlateVisibility::SelfHitTestInvisible); return Border;
    }

    UOverlay* MakeCenteredRoot(UWidgetTree* Tree, USizeBox*& OutPanelSize, float Width, float Height)
    {
        UOverlay* Root = Tree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("StructureUIRoot"));
        UBorder* Dim = Tree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("StructureUIDim"));
        Dim->SetBrushColor(FLinearColor(0.f,0.f,0.f,0.42f)); Dim->SetVisibility(ESlateVisibility::HitTestInvisible);
        if (UOverlaySlot* S=Root->AddChildToOverlay(Dim)) { S->SetHorizontalAlignment(HAlign_Fill); S->SetVerticalAlignment(VAlign_Fill); }
        OutPanelSize = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("StructureUIPanelSize"));
        OutPanelSize->SetWidthOverride(Width); OutPanelSize->SetHeightOverride(Height); OutPanelSize->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
        if (UOverlaySlot* S=Root->AddChildToOverlay(OutPanelSize)) { S->SetHorizontalAlignment(HAlign_Center); S->SetVerticalAlignment(VAlign_Center); S->SetPadding(FMargin(24.f,24.f,24.f,90.f)); }
        return Root;
    }
}

// ---------------- Build Piece Row ----------------
void UARPGBuildPieceRowWidget::NativeOnInitialized(){ Super::NativeOnInitialized(); EnsureNativeLayoutOrBindings(); if(BuildPieceButton)BuildPieceButton->OnClicked.AddUniqueDynamic(this,&UARPGBuildPieceRowWidget::HandleClicked); }
void UARPGBuildPieceRowWidget::InitializeBuildPieceRow(UARPGBuildMenuWidget* InOwner,const FARPGBuildPieceView& InView){OwnerMenu=InOwner;EnsureNativeLayoutOrBindings();if(BuildPieceButton)BuildPieceButton->OnClicked.AddUniqueDynamic(this,&UARPGBuildPieceRowWidget::HandleClicked);SetBuildPieceView(InView);}
void UARPGBuildPieceRowWidget::SetBuildPieceView(const FARPGBuildPieceView& InView){View=InView;ApplyView();BP_OnBuildPieceRowUpdated(View);}
void UARPGBuildPieceRowWidget::EnsureNativeLayoutOrBindings()
{
    if(!WidgetTree)return;
    if(!WidgetTree->RootWidget)
    {
        BuildPieceButton=WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(),TEXT("BuildPieceButton"));WidgetTree->RootWidget=BuildPieceButton;
        UHorizontalBox* Row=WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(),TEXT("BuildPieceRow"));BuildPieceButton->AddChild(Row);
        USizeBox* IconBox=WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());IconBox->SetWidthOverride(52.f);IconBox->SetHeightOverride(52.f);
        BuildPieceIcon=WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(),TEXT("BuildPieceIcon"));IconBox->SetContent(BuildPieceIcon);if(UHorizontalBoxSlot*S=Row->AddChildToHorizontalBox(IconBox))S->SetPadding(FMargin(5.f,5.f,10.f,5.f));
        UVerticalBox* Texts=WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());if(UHorizontalBoxSlot*S=Row->AddChildToHorizontalBox(Texts)){FSlateChildSize Fill;Fill.SizeRule=ESlateSizeRule::Fill;S->SetSize(Fill);S->SetVerticalAlignment(VAlign_Center);}
        BuildPieceNameText=MakeBuildText(WidgetTree,TEXT("BuildPieceNameText"),FText::GetEmpty(),14,FLinearColor(0.95f,0.80f,0.34f,1.f));
        BuildPieceCostText=MakeBuildText(WidgetTree,TEXT("BuildPieceCostText"),FText::GetEmpty(),10,FLinearColor(0.68f,0.72f,0.80f,1.f));
        Texts->AddChildToVerticalBox(BuildPieceNameText);Texts->AddChildToVerticalBox(BuildPieceCostText);
    }
    else
    {
        if(!BuildPieceButton)BuildPieceButton=Cast<UButton>(GetWidgetFromName(TEXT("BuildPieceButton")));
        if(!BuildPieceIcon)BuildPieceIcon=Cast<UImage>(GetWidgetFromName(TEXT("BuildPieceIcon")));
        if(!BuildPieceNameText)BuildPieceNameText=Cast<UTextBlock>(GetWidgetFromName(TEXT("BuildPieceNameText")));
        if(!BuildPieceCostText)BuildPieceCostText=Cast<UTextBlock>(GetWidgetFromName(TEXT("BuildPieceCostText")));
    }
}
void UARPGBuildPieceRowWidget::ApplyView()
{
    EnsureNativeLayoutOrBindings();
    if(BuildPieceNameText)BuildPieceNameText->SetText(View.DisplayName);
    if(BuildPieceCostText){BuildPieceCostText->SetText(FText::Format(NSLOCTEXT("AkumasRPGFramework","BuildRowCost","{0}   •   Can build: {1}"),View.CostSummary,FText::AsNumber(View.BuildableCount)));BuildPieceCostText->SetColorAndOpacity(FSlateColor(View.bCanAfford?FLinearColor(0.42f,0.88f,0.52f,1.f):FLinearColor(0.94f,0.48f,0.34f,1.f)));}
    if(BuildPieceIcon){UTexture2D*Tex=View.Piece?View.Piece->Icon.LoadSynchronous():nullptr;BuildPieceIcon->SetBrushFromTexture(Tex,true);BuildPieceIcon->SetOpacity(Tex?1.f:0.12f);}
    if(BuildPieceButton)BuildPieceButton->SetIsEnabled(View.Piece!=nullptr);
}
void UARPGBuildPieceRowWidget::HandleClicked(){if(OwnerMenu&&View.Piece)OwnerMenu->ChooseBuildPiece(View.Piece);}

// ---------------- Build Menu ----------------
void UARPGBuildMenuWidget::NativeOnInitialized(){Super::NativeOnInitialized();EnsureNativeLayoutOrBindings();if(CloseBuildMenuButton)CloseBuildMenuButton->OnClicked.AddUniqueDynamic(this,&UARPGBuildMenuWidget::HandleClose);}
void UARPGBuildMenuWidget::InitializeBuildMenu(AARPGCharacter* InCharacter,UARPGBuildingUIComponent* InUI){Character=InCharacter;BuildingUI=InUI;EnsureNativeLayoutOrBindings();if(CloseBuildMenuButton)CloseBuildMenuButton->OnClicked.AddUniqueDynamic(this,&UARPGBuildMenuWidget::HandleClose);RefreshBuildMenu();}
void UARPGBuildMenuWidget::EnsureNativeLayoutOrBindings()
{
    if(!WidgetTree)return;
    if(!WidgetTree->RootWidget)
    {
        UOverlay* Root=WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(),TEXT("BuildMenuRoot"));WidgetTree->RootWidget=Root;
        UBorder* Dim=WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(),TEXT("BuildMenuDim"));Dim->SetBrushColor(FLinearColor(0.f,0.f,0.f,0.26f));Dim->SetVisibility(ESlateVisibility::HitTestInvisible);if(UOverlaySlot*S=Root->AddChildToOverlay(Dim)){S->SetHorizontalAlignment(HAlign_Fill);S->SetVerticalAlignment(VAlign_Fill);}
        USizeBox* PanelSize=WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(),TEXT("BuildMenuPanelSize"));PanelSize->SetWidthOverride(560.f);PanelSize->SetHeightOverride(650.f);PanelSize->SetVisibility(ESlateVisibility::SelfHitTestInvisible);if(UOverlaySlot*S=Root->AddChildToOverlay(PanelSize)){S->SetHorizontalAlignment(HAlign_Left);S->SetVerticalAlignment(VAlign_Center);S->SetPadding(FMargin(36.f,24.f,0.f,100.f));}
        UBorder* Panel=MakePanelBorder(WidgetTree,TEXT("BuildMenuPanel"));PanelSize->SetContent(Panel);UVerticalBox* Main=WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());Panel->SetContent(Main);
        UHorizontalBox* Header=WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());if(UVerticalBoxSlot*S=Main->AddChildToVerticalBox(Header))S->SetPadding(FMargin(0,0,0,8));
        BuildMenuTitleText=MakeBuildText(WidgetTree,TEXT("BuildMenuTitleText"),NSLOCTEXT("AkumasRPGFramework","BuildMenuTitle","BUILDING"),25,FLinearColor(0.95f,0.78f,0.28f,1.f));if(UHorizontalBoxSlot*S=Header->AddChildToHorizontalBox(BuildMenuTitleText)){FSlateChildSize Fill;Fill.SizeRule=ESlateSizeRule::Fill;S->SetSize(Fill);}
        CloseBuildMenuButton=WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(),TEXT("CloseBuildMenuButton"));CloseBuildMenuButton->AddChild(MakeBuildText(WidgetTree,NAME_None,NSLOCTEXT("AkumasRPGFramework","BuildMenuClose","Close"),11));USizeBox*CloseSize=WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());CloseSize->SetWidthOverride(82.f);CloseSize->SetHeightOverride(32.f);CloseSize->SetContent(CloseBuildMenuButton);Header->AddChildToHorizontalBox(CloseSize);
        BuildMenuHelpText=MakeBuildText(WidgetTree,TEXT("BuildMenuHelpText"),NSLOCTEXT("AkumasRPGFramework","BuildMenuHelp","Choose a piece to enter placement mode. Green = valid. Red = blocked, unsupported, restricted, or missing materials."),11,FLinearColor(0.67f,0.71f,0.80f,1.f));if(UVerticalBoxSlot*S=Main->AddChildToVerticalBox(BuildMenuHelpText))S->SetPadding(FMargin(0,0,0,10));
        UScrollBox* Scroll=WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());if(UVerticalBoxSlot*S=Main->AddChildToVerticalBox(Scroll)){FSlateChildSize Fill;Fill.SizeRule=ESlateSizeRule::Fill;S->SetSize(Fill);}BuildPieceListBox=WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(),TEXT("BuildPieceListBox"));Scroll->AddChild(BuildPieceListBox);
    }
    else
    {
        if(!BuildPieceListBox)BuildPieceListBox=Cast<UVerticalBox>(GetWidgetFromName(TEXT("BuildPieceListBox")));
        if(!BuildMenuTitleText)BuildMenuTitleText=Cast<UTextBlock>(GetWidgetFromName(TEXT("BuildMenuTitleText")));
        if(!BuildMenuHelpText)BuildMenuHelpText=Cast<UTextBlock>(GetWidgetFromName(TEXT("BuildMenuHelpText")));
        if(!CloseBuildMenuButton)CloseBuildMenuButton=Cast<UButton>(GetWidgetFromName(TEXT("CloseBuildMenuButton")));
    }
}
void UARPGBuildMenuWidget::RefreshBuildMenu()
{
    EnsureNativeLayoutOrBindings();if(!BuildPieceListBox||!Character||!Character->Building||!BuildingUI)return;BuildPieceListBox->ClearChildren();
    TSubclassOf<UARPGBuildPieceRowWidget> RowClass=BuildingUI->GetBuildPieceRowWidgetClass();if(!RowClass)RowClass=UARPGBuildPieceRowWidget::StaticClass();
    FName LastCategory=NAME_None;
    for(UARPGBuildPieceDefinition* Piece:Character->Building->BuildCatalog)
    {
        if(!Piece)continue;
        if(Piece->BuildCategory!=LastCategory){LastCategory=Piece->BuildCategory;UTextBlock*Cat=MakeBuildText(WidgetTree,NAME_None,FText::FromName(LastCategory),13,FLinearColor(0.72f,0.76f,0.86f,1.f));if(UVerticalBoxSlot*S=BuildPieceListBox->AddChildToVerticalBox(Cat))S->SetPadding(FMargin(3,7,0,4));}
        FARPGBuildPieceView V;V.Piece=Piece;V.DisplayName=DefinitionName(Piece,Piece->DefinitionId);V.Description=Piece->Description;V.Category=Piece->BuildCategory;V.CostSummary=BuildAmountList(Piece->BuildCost);V.BuildableCount=Character->Building->GetBuildableCount(Piece);V.bCanAfford=V.BuildableCount>0||Piece->BuildCost.Num()==0;V.ConstructionSeconds=Piece->ConstructionSeconds;
        UARPGBuildPieceRowWidget*Row=WidgetTree->ConstructWidget<UARPGBuildPieceRowWidget>(RowClass);Row->InitializeBuildPieceRow(this,V);if(UVerticalBoxSlot*S=BuildPieceListBox->AddChildToVerticalBox(Row))S->SetPadding(FMargin(0,0,0,5));
    }
    BP_OnBuildMenuRefreshed();
}
void UARPGBuildMenuWidget::ChooseBuildPiece(UARPGBuildPieceDefinition* Piece){if(BuildingUI&&Piece)BuildingUI->BeginPlacementFromMenu(Piece);}
void UARPGBuildMenuWidget::HandleClose(){if(BuildingUI)BuildingUI->CloseBuildMenu();}

// ---------------- Placement HUD ----------------
void UARPGBuildPlacementHUDWidget::NativeOnInitialized(){Super::NativeOnInitialized();EnsureNativeLayoutOrBindings();SetVisibility(ESlateVisibility::SelfHitTestInvisible);}
void UARPGBuildPlacementHUDWidget::InitializePlacementHUD(AARPGCharacter* InCharacter){Character=InCharacter;EnsureNativeLayoutOrBindings();SetVisibility(ESlateVisibility::SelfHitTestInvisible);RefreshPlacementHUD();}
void UARPGBuildPlacementHUDWidget::EnsureNativeLayoutOrBindings()
{
    if(!WidgetTree)return;
    if(!WidgetTree->RootWidget)
    {
        UOverlay*Root=WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(),TEXT("PlacementHUDRoot"));Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible);WidgetTree->RootWidget=Root;
        USizeBox*Size=WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());Size->SetWidthOverride(520.f);Size->SetVisibility(ESlateVisibility::SelfHitTestInvisible);if(UOverlaySlot*S=Root->AddChildToOverlay(Size)){S->SetHorizontalAlignment(HAlign_Center);S->SetVerticalAlignment(VAlign_Top);S->SetPadding(FMargin(20,36,20,0));}
        UBorder*Panel=WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());Panel->SetPadding(FMargin(12));Panel->SetBrushColor(FLinearColor(0.008f,0.01f,0.016f,0.88f));Panel->SetVisibility(ESlateVisibility::SelfHitTestInvisible);Size->SetContent(Panel);UVerticalBox*Main=WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());Panel->SetContent(Main);
        PlacementPieceNameText=MakeBuildText(WidgetTree,TEXT("PlacementPieceNameText"),FText::GetEmpty(),18,FLinearColor(0.95f,0.78f,0.28f,1.f));PlacementPieceNameText->SetJustification(ETextJustify::Center);Main->AddChildToVerticalBox(PlacementPieceNameText);
        PlacementCostText=MakeBuildText(WidgetTree,TEXT("PlacementCostText"),FText::GetEmpty(),10,FLinearColor(0.72f,0.76f,0.84f,1.f));PlacementCostText->SetJustification(ETextJustify::Center);Main->AddChildToVerticalBox(PlacementCostText);
        PlacementStatusText=MakeBuildText(WidgetTree,TEXT("PlacementStatusText"),FText::GetEmpty(),13);PlacementStatusText->SetJustification(ETextJustify::Center);if(UVerticalBoxSlot*S=Main->AddChildToVerticalBox(PlacementStatusText))S->SetPadding(FMargin(0,4,0,3));
        PlacementControlsText=MakeBuildText(WidgetTree,TEXT("PlacementControlsText"),NSLOCTEXT("AkumasRPGFramework","PlacementControls","Place • Rotate • Previous / Next Piece • Cancel • Reopen Build Menu"),10,FLinearColor(0.58f,0.62f,0.70f,1.f));PlacementControlsText->SetJustification(ETextJustify::Center);Main->AddChildToVerticalBox(PlacementControlsText);
    }
    else
    {
        if(!PlacementPieceNameText)PlacementPieceNameText=Cast<UTextBlock>(GetWidgetFromName(TEXT("PlacementPieceNameText")));
        if(!PlacementCostText)PlacementCostText=Cast<UTextBlock>(GetWidgetFromName(TEXT("PlacementCostText")));
        if(!PlacementStatusText)PlacementStatusText=Cast<UTextBlock>(GetWidgetFromName(TEXT("PlacementStatusText")));
        if(!PlacementControlsText)PlacementControlsText=Cast<UTextBlock>(GetWidgetFromName(TEXT("PlacementControlsText")));
    }
}
void UARPGBuildPlacementHUDWidget::RefreshPlacementHUD()
{
    EnsureNativeLayoutOrBindings();if(!Character||!Character->Building)return;UARPGBuildPieceDefinition*Piece=Character->Building->GetSelectedBuildPiece();
    if(PlacementPieceNameText)PlacementPieceNameText->SetText(Piece?DefinitionName(Piece,Piece->DefinitionId):FText::GetEmpty());
    if(PlacementCostText)PlacementCostText->SetText(Piece?BuildAmountList(Piece->BuildCost):FText::GetEmpty());
    if(PlacementStatusText){const EARPGPlacementResult R=Character->Building->GetCurrentPreviewResult();PlacementStatusText->SetText(PlacementResultText(R));PlacementStatusText->SetColorAndOpacity(FSlateColor(R==EARPGPlacementResult::Valid?FLinearColor(0.34f,0.94f,0.44f,1.f):FLinearColor(0.96f,0.36f,0.28f,1.f)));}
    if(PlacementControlsText)
    {
        if(Piece&&Piece->PieceKind==EARPGBuildPieceKind::SettlementPath)
        {
            PlacementControlsText->SetText(Character->Building->HasSettlementPathStartPoint()
                ? NSLOCTEXT("AkumasRPGFramework","SettlementPathControlsContinue","Place next path point • Continue until Cancel • Previous / Next Piece")
                : NSLOCTEXT("AkumasRPGFramework","SettlementPathControlsStart","Place first path point • Then continue placing points until Cancel"));
        }
        else PlacementControlsText->SetText(NSLOCTEXT("AkumasRPGFramework","PlacementControls","Place • Rotate • Previous / Next Piece • Cancel • Reopen Build Menu"));
    }
    BP_OnBuildPlacementHUDUpdated();
}

// ---------------- Structure Item Row ----------------
void UARPGStructureItemRowWidget::NativeOnInitialized(){Super::NativeOnInitialized();EnsureNativeLayoutOrBindings();if(TransferOneButton)TransferOneButton->OnClicked.AddUniqueDynamic(this,&UARPGStructureItemRowWidget::HandleTransferOne);if(TransferAllButton)TransferAllButton->OnClicked.AddUniqueDynamic(this,&UARPGStructureItemRowWidget::HandleTransferAll);}
void UARPGStructureItemRowWidget::InitializeStructureItemRow(UUserWidget* InOwnerPanel,const FARPGStructureItemView& InView){OwnerPanel=InOwnerPanel;EnsureNativeLayoutOrBindings();if(TransferOneButton)TransferOneButton->OnClicked.AddUniqueDynamic(this,&UARPGStructureItemRowWidget::HandleTransferOne);if(TransferAllButton)TransferAllButton->OnClicked.AddUniqueDynamic(this,&UARPGStructureItemRowWidget::HandleTransferAll);SetStructureItemView(InView);}
void UARPGStructureItemRowWidget::SetStructureItemView(const FARPGStructureItemView& InView){View=InView;ApplyView();BP_OnStructureItemRowUpdated(View);}
void UARPGStructureItemRowWidget::EnsureNativeLayoutOrBindings()
{
    if(!WidgetTree)return;if(!WidgetTree->RootWidget)
    {
        UBorder*Bg=WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(),TEXT("StructureItemRowBackground"));Bg->SetPadding(FMargin(5));Bg->SetBrushColor(FLinearColor(0.04f,0.045f,0.06f,0.95f));WidgetTree->RootWidget=Bg;UHorizontalBox*Row=WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());Bg->SetContent(Row);
        USizeBox*IconSize=WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());IconSize->SetWidthOverride(36);IconSize->SetHeightOverride(36);ItemIcon=WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(),TEXT("ItemIcon"));IconSize->SetContent(ItemIcon);if(UHorizontalBoxSlot*S=Row->AddChildToHorizontalBox(IconSize))S->SetPadding(FMargin(0,0,7,0));
        ItemNameText=MakeBuildText(WidgetTree,TEXT("ItemNameText"),FText::GetEmpty(),11);if(UHorizontalBoxSlot*S=Row->AddChildToHorizontalBox(ItemNameText)){FSlateChildSize Fill;Fill.SizeRule=ESlateSizeRule::Fill;S->SetSize(Fill);S->SetVerticalAlignment(VAlign_Center);}
        ItemQuantityText=MakeBuildText(WidgetTree,TEXT("ItemQuantityText"),FText::GetEmpty(),10,FLinearColor(0.70f,0.74f,0.82f,1));if(UHorizontalBoxSlot*S=Row->AddChildToHorizontalBox(ItemQuantityText)){S->SetVerticalAlignment(VAlign_Center);S->SetPadding(FMargin(5,0,8,0));}
        TransferOneButton=WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(),TEXT("TransferOneButton"));TransferOneButton->AddChild(MakeBuildText(WidgetTree,NAME_None,NSLOCTEXT("AkumasRPGFramework","TransferOne","1"),10));USizeBox*One=WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());One->SetWidthOverride(36);One->SetHeightOverride(30);One->SetContent(TransferOneButton);Row->AddChildToHorizontalBox(One);
        TransferAllButton=WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(),TEXT("TransferAllButton"));TransferAllButton->AddChild(MakeBuildText(WidgetTree,NAME_None,NSLOCTEXT("AkumasRPGFramework","TransferAll","ALL"),9));USizeBox*All=WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());All->SetWidthOverride(48);All->SetHeightOverride(30);All->SetContent(TransferAllButton);if(UHorizontalBoxSlot*S=Row->AddChildToHorizontalBox(All))S->SetPadding(FMargin(4,0,0,0));
    }
    else
    {
        if(!ItemIcon)ItemIcon=Cast<UImage>(GetWidgetFromName(TEXT("ItemIcon")));if(!ItemNameText)ItemNameText=Cast<UTextBlock>(GetWidgetFromName(TEXT("ItemNameText")));if(!ItemQuantityText)ItemQuantityText=Cast<UTextBlock>(GetWidgetFromName(TEXT("ItemQuantityText")));if(!TransferOneButton)TransferOneButton=Cast<UButton>(GetWidgetFromName(TEXT("TransferOneButton")));if(!TransferAllButton)TransferAllButton=Cast<UButton>(GetWidgetFromName(TEXT("TransferAllButton")));
    }
}
void UARPGStructureItemRowWidget::ApplyView(){EnsureNativeLayoutOrBindings();if(ItemNameText)ItemNameText->SetText(View.DisplayName);if(ItemQuantityText)ItemQuantityText->SetText(FText::Format(NSLOCTEXT("AkumasRPGFramework","StructureQty","x{0}"),FText::AsNumber(View.Quantity)));if(ItemIcon){UTexture2D*T=View.ItemDefinition?View.ItemDefinition->Icon.LoadSynchronous():nullptr;ItemIcon->SetBrushFromTexture(T,true);ItemIcon->SetOpacity(T?1.f:.12f);}if(TransferOneButton)TransferOneButton->SetIsEnabled(View.Quantity>0);if(TransferAllButton)TransferAllButton->SetIsEnabled(View.Quantity>0);}
void UARPGStructureItemRowWidget::HandleTransferOne()
{
    if (UARPGStoragePanelWidget* StoragePanel = Cast<UARPGStoragePanelWidget>(OwnerPanel))
    {
        StoragePanel->HandleItemTransfer(View, 1);
    }
    else if (UARPGCraftingStationPanelWidget* StationPanel = Cast<UARPGCraftingStationPanelWidget>(OwnerPanel))
    {
        StationPanel->HandleItemTransfer(View, 1);
    }
}
void UARPGStructureItemRowWidget::HandleTransferAll()
{
    if (UARPGStoragePanelWidget* StoragePanel = Cast<UARPGStoragePanelWidget>(OwnerPanel))
    {
        StoragePanel->HandleItemTransfer(View, View.Quantity);
    }
    else if (UARPGCraftingStationPanelWidget* StationPanel = Cast<UARPGCraftingStationPanelWidget>(OwnerPanel))
    {
        StationPanel->HandleItemTransfer(View, View.Quantity);
    }
}

// ---------------- Storage Panel ----------------
void UARPGStoragePanelWidget::NativeOnInitialized(){Super::NativeOnInitialized();EnsureNativeLayoutOrBindings();if(CloseStorageButton)CloseStorageButton->OnClicked.AddUniqueDynamic(this,&UARPGStoragePanelWidget::HandleClose);}
void UARPGStoragePanelWidget::InitializeStorageUI(AARPGCharacter*InCharacter,AARPGStorageActor*InStorage,UARPGBuildingUIComponent*InUI){Character=InCharacter;Storage=InStorage;BuildingUI=InUI;EnsureNativeLayoutOrBindings();if(CloseStorageButton)CloseStorageButton->OnClicked.AddUniqueDynamic(this,&UARPGStoragePanelWidget::HandleClose);RefreshStorageUI();}
void UARPGStoragePanelWidget::EnsureNativeLayoutOrBindings()
{
    if(!WidgetTree)return;if(!WidgetTree->RootWidget)
    {
        USizeBox*PanelSize=nullptr;UOverlay*Root=MakeCenteredRoot(WidgetTree,PanelSize,1050,650);WidgetTree->RootWidget=Root;UBorder*Panel=MakePanelBorder(WidgetTree,TEXT("StoragePanel"));PanelSize->SetContent(Panel);UVerticalBox*Main=WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());Panel->SetContent(Main);
        UHorizontalBox*Header=WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());if(UVerticalBoxSlot*S=Main->AddChildToVerticalBox(Header))S->SetPadding(FMargin(0,0,0,10));StorageTitleText=MakeBuildText(WidgetTree,TEXT("StorageTitleText"),NSLOCTEXT("AkumasRPGFramework","StorageTitle","STORAGE"),24,FLinearColor(.95f,.78f,.28f,1));if(UHorizontalBoxSlot*S=Header->AddChildToHorizontalBox(StorageTitleText)){FSlateChildSize Fill;Fill.SizeRule=ESlateSizeRule::Fill;S->SetSize(Fill);}CloseStorageButton=WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(),TEXT("CloseStorageButton"));CloseStorageButton->AddChild(MakeBuildText(WidgetTree,NAME_None,NSLOCTEXT("AkumasRPGFramework","StorageClose","Close"),11));USizeBox*Close=WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());Close->SetWidthOverride(86);Close->SetHeightOverride(32);Close->SetContent(CloseStorageButton);Header->AddChildToHorizontalBox(Close);
        UHorizontalBox*Columns=WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());if(UVerticalBoxSlot*S=Main->AddChildToVerticalBox(Columns)){FSlateChildSize Fill;Fill.SizeRule=ESlateSizeRule::Fill;S->SetSize(Fill);}
        auto MakeColumn=[&](const FText&Title,const FName&ListName,TObjectPtr<UVerticalBox>&OutBox){UBorder*B=WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());B->SetPadding(FMargin(10));B->SetBrushColor(FLinearColor(.025f,.03f,.045f,.96f));UVerticalBox*V=WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());B->SetContent(V);V->AddChildToVerticalBox(MakeBuildText(WidgetTree,NAME_None,Title,15,FLinearColor(.80f,.83f,.90f,1)));UScrollBox*Sc=WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());if(UVerticalBoxSlot*S=V->AddChildToVerticalBox(Sc)){FSlateChildSize Fill;Fill.SizeRule=ESlateSizeRule::Fill;S->SetSize(Fill);S->SetPadding(FMargin(0,8,0,0));}OutBox=WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(),ListName);Sc->AddChild(OutBox);if(UHorizontalBoxSlot*S=Columns->AddChildToHorizontalBox(B)){FSlateChildSize Fill;Fill.SizeRule=ESlateSizeRule::Fill;S->SetSize(Fill);S->SetPadding(FMargin(4));}};
        MakeColumn(NSLOCTEXT("AkumasRPGFramework","StoragePlayerInv","PLAYER INVENTORY"),TEXT("PlayerItemListBox"),PlayerItemListBox);
        MakeColumn(NSLOCTEXT("AkumasRPGFramework","StorageContainerInv","CONTAINER"),TEXT("StorageItemListBox"),StorageItemListBox);
        UTextBlock*Help=MakeBuildText(WidgetTree,NAME_None,NSLOCTEXT("AkumasRPGFramework","StorageHelp","Use 1 or ALL to transfer. Server authority validates range, ownership, capacity, equipped state and durable runtime item state."),10,FLinearColor(.56f,.60f,.68f,1));if(UVerticalBoxSlot*S=Main->AddChildToVerticalBox(Help))S->SetPadding(FMargin(0,8,0,0));
    }
    else
    {
        if(!StorageTitleText)StorageTitleText=Cast<UTextBlock>(GetWidgetFromName(TEXT("StorageTitleText")));if(!PlayerItemListBox)PlayerItemListBox=Cast<UVerticalBox>(GetWidgetFromName(TEXT("PlayerItemListBox")));if(!StorageItemListBox)StorageItemListBox=Cast<UVerticalBox>(GetWidgetFromName(TEXT("StorageItemListBox")));if(!CloseStorageButton)CloseStorageButton=Cast<UButton>(GetWidgetFromName(TEXT("CloseStorageButton")));
    }
}
void UARPGStoragePanelWidget::RefreshStorageUI(){EnsureNativeLayoutOrBindings();if(!Character||!Storage||!BuildingUI)return;if(StorageTitleText)StorageTitleText->SetText(Storage->Definition?DefinitionName(Storage->Definition,Storage->GetFName()):NSLOCTEXT("AkumasRPGFramework","StorageTitleFallback","STORAGE"));TSubclassOf<UARPGStructureItemRowWidget>RowClass=BuildingUI->GetStructureItemRowWidgetClass();PopulateItemList(WidgetTree,PlayerItemListBox,Character->Inventory,true,false,RowClass,this);PopulateItemList(WidgetTree,StorageItemListBox,Storage->Inventory,false,false,RowClass,this);BP_OnStorageUIRefreshed();}
void UARPGStoragePanelWidget::HandleItemTransfer(const FARPGStructureItemView&View,int32 Quantity){if(!Character||!Storage||!Character->Interaction||Quantity<=0||!View.InstanceId.IsValid())return;if(View.bFromPlayerInventory)Character->Interaction->DepositToStorageInstance(Storage,View.InstanceId,Quantity);else Character->Interaction->WithdrawFromStorageInstance(Storage,View.InstanceId,Quantity);}
void UARPGStoragePanelWidget::HandleClose(){if(BuildingUI)BuildingUI->CloseStructureUI();}

// ---------------- Station Recipe Row ----------------
void UARPGStationRecipeRowWidget::NativeOnInitialized(){Super::NativeOnInitialized();EnsureNativeLayoutOrBindings();if(QueueRecipeButton)QueueRecipeButton->OnClicked.AddUniqueDynamic(this,&UARPGStationRecipeRowWidget::HandleQueue);}
void UARPGStationRecipeRowWidget::InitializeStationRecipeRow(UARPGCraftingStationPanelWidget*InOwner,const FARPGStationRecipeView&InView){OwnerPanel=InOwner;EnsureNativeLayoutOrBindings();if(QueueRecipeButton)QueueRecipeButton->OnClicked.AddUniqueDynamic(this,&UARPGStationRecipeRowWidget::HandleQueue);SetStationRecipeView(InView);}
void UARPGStationRecipeRowWidget::SetStationRecipeView(const FARPGStationRecipeView&InView){View=InView;ApplyView();BP_OnStationRecipeRowUpdated(View);}
void UARPGStationRecipeRowWidget::EnsureNativeLayoutOrBindings(){if(!WidgetTree)return;if(!WidgetTree->RootWidget){QueueRecipeButton=WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(),TEXT("QueueRecipeButton"));WidgetTree->RootWidget=QueueRecipeButton;UVerticalBox*V=WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());QueueRecipeButton->AddChild(V);StationRecipeNameText=MakeBuildText(WidgetTree,TEXT("StationRecipeNameText"),FText::GetEmpty(),12,FLinearColor(.95f,.80f,.34f,1));StationRecipeDetailsText=MakeBuildText(WidgetTree,TEXT("StationRecipeDetailsText"),FText::GetEmpty(),9,FLinearColor(.67f,.71f,.79f,1));V->AddChildToVerticalBox(StationRecipeNameText);V->AddChildToVerticalBox(StationRecipeDetailsText);}else{if(!QueueRecipeButton)QueueRecipeButton=Cast<UButton>(GetWidgetFromName(TEXT("QueueRecipeButton")));if(!StationRecipeNameText)StationRecipeNameText=Cast<UTextBlock>(GetWidgetFromName(TEXT("StationRecipeNameText")));if(!StationRecipeDetailsText)StationRecipeDetailsText=Cast<UTextBlock>(GetWidgetFromName(TEXT("StationRecipeDetailsText")));}}
void UARPGStationRecipeRowWidget::ApplyView(){EnsureNativeLayoutOrBindings();if(StationRecipeNameText)StationRecipeNameText->SetText(View.DisplayName);if(StationRecipeDetailsText)StationRecipeDetailsText->SetText(FText::Format(NSLOCTEXT("AkumasRPGFramework","StationRecipeDetails","IN: {0}\nOUT: {1}\n{2} • {3}s"),View.InputSummary,View.OutputSummary,View.FuelSummary,FText::AsNumber(View.CraftSeconds)));if(QueueRecipeButton)QueueRecipeButton->SetIsEnabled(View.Recipe!=nullptr&&View.bCanQueue);}
void UARPGStationRecipeRowWidget::HandleQueue(){if(OwnerPanel&&View.Recipe)OwnerPanel->QueueRecipe(View.Recipe);}

// ---------------- Crafting Station Panel ----------------
void UARPGCraftingStationPanelWidget::NativeOnInitialized(){Super::NativeOnInitialized();EnsureNativeLayoutOrBindings();if(CloseStationButton)CloseStationButton->OnClicked.AddUniqueDynamic(this,&UARPGCraftingStationPanelWidget::HandleClose);}
void UARPGCraftingStationPanelWidget::InitializeStationUI(AARPGCharacter*InCharacter,AARPGCraftingStationActor*InStation,UARPGBuildingUIComponent*InUI){Character=InCharacter;Station=InStation;BuildingUI=InUI;EnsureNativeLayoutOrBindings();if(CloseStationButton)CloseStationButton->OnClicked.AddUniqueDynamic(this,&UARPGCraftingStationPanelWidget::HandleClose);RefreshStationUI();}
void UARPGCraftingStationPanelWidget::EnsureNativeLayoutOrBindings()
{
    if(!WidgetTree)return;if(!WidgetTree->RootWidget)
    {
        USizeBox*PanelSize=nullptr;UOverlay*Root=MakeCenteredRoot(WidgetTree,PanelSize,1280,720);WidgetTree->RootWidget=Root;UBorder*Panel=MakePanelBorder(WidgetTree,TEXT("StationPanel"));PanelSize->SetContent(Panel);UVerticalBox*Main=WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());Panel->SetContent(Main);
        UHorizontalBox*Header=WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());if(UVerticalBoxSlot*S=Main->AddChildToVerticalBox(Header))S->SetPadding(FMargin(0,0,0,8));StationTitleText=MakeBuildText(WidgetTree,TEXT("StationTitleText"),NSLOCTEXT("AkumasRPGFramework","StationTitle","PRODUCTION STATION"),24,FLinearColor(.95f,.78f,.28f,1));if(UHorizontalBoxSlot*S=Header->AddChildToHorizontalBox(StationTitleText)){FSlateChildSize Fill;Fill.SizeRule=ESlateSizeRule::Fill;S->SetSize(Fill);}CloseStationButton=WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(),TEXT("CloseStationButton"));CloseStationButton->AddChild(MakeBuildText(WidgetTree,NAME_None,NSLOCTEXT("AkumasRPGFramework","StationClose","Close"),11));USizeBox*Close=WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());Close->SetWidthOverride(86);Close->SetHeightOverride(32);Close->SetContent(CloseStationButton);Header->AddChildToHorizontalBox(Close);
        UHorizontalBox*Columns=WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());if(UVerticalBoxSlot*S=Main->AddChildToVerticalBox(Columns)){FSlateChildSize Fill;Fill.SizeRule=ESlateSizeRule::Fill;S->SetSize(Fill);}
        auto MakeColumn=[&](const FText&Title,const FName&ListName,TObjectPtr<UVerticalBox>&OutBox,float Weight){UBorder*B=WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());B->SetPadding(FMargin(8));B->SetBrushColor(FLinearColor(.025f,.03f,.045f,.96f));UVerticalBox*V=WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());B->SetContent(V);V->AddChildToVerticalBox(MakeBuildText(WidgetTree,NAME_None,Title,13,FLinearColor(.80f,.83f,.90f,1)));UScrollBox*Sc=WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());if(UVerticalBoxSlot*S=V->AddChildToVerticalBox(Sc)){FSlateChildSize Fill;Fill.SizeRule=ESlateSizeRule::Fill;S->SetSize(Fill);S->SetPadding(FMargin(0,6,0,0));}OutBox=WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(),ListName);Sc->AddChild(OutBox);if(UHorizontalBoxSlot*S=Columns->AddChildToHorizontalBox(B)){FSlateChildSize Fill;Fill.SizeRule=ESlateSizeRule::Fill;Fill.Value=Weight;S->SetSize(Fill);S->SetPadding(FMargin(3));}};
        MakeColumn(NSLOCTEXT("AkumasRPGFramework","StationPlayerItems","PLAYER"),TEXT("PlayerItemListBox"),PlayerItemListBox,1.f);
        MakeColumn(NSLOCTEXT("AkumasRPGFramework","StationInputFuel","INPUT + FUEL"),TEXT("StationInputListBox"),StationInputListBox,1.f);
        MakeColumn(NSLOCTEXT("AkumasRPGFramework","StationRecipes","RECIPES"),TEXT("StationRecipeListBox"),StationRecipeListBox,1.15f);
        MakeColumn(NSLOCTEXT("AkumasRPGFramework","StationOutput","OUTPUT"),TEXT("StationOutputListBox"),StationOutputListBox,.95f);
        StationProgressBar=WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(),TEXT("StationProgressBar"));if(UVerticalBoxSlot*S=Main->AddChildToVerticalBox(StationProgressBar))S->SetPadding(FMargin(0,9,0,3));StationStateText=MakeBuildText(WidgetTree,TEXT("StationStateText"),FText::GetEmpty(),10,FLinearColor(.68f,.72f,.80f,1));Main->AddChildToVerticalBox(StationStateText);
    }
    else
    {
        if(!StationTitleText)StationTitleText=Cast<UTextBlock>(GetWidgetFromName(TEXT("StationTitleText")));if(!PlayerItemListBox)PlayerItemListBox=Cast<UVerticalBox>(GetWidgetFromName(TEXT("PlayerItemListBox")));if(!StationInputListBox)StationInputListBox=Cast<UVerticalBox>(GetWidgetFromName(TEXT("StationInputListBox")));if(!StationOutputListBox)StationOutputListBox=Cast<UVerticalBox>(GetWidgetFromName(TEXT("StationOutputListBox")));if(!StationRecipeListBox)StationRecipeListBox=Cast<UVerticalBox>(GetWidgetFromName(TEXT("StationRecipeListBox")));if(!StationProgressBar)StationProgressBar=Cast<UProgressBar>(GetWidgetFromName(TEXT("StationProgressBar")));if(!StationStateText)StationStateText=Cast<UTextBlock>(GetWidgetFromName(TEXT("StationStateText")));if(!CloseStationButton)CloseStationButton=Cast<UButton>(GetWidgetFromName(TEXT("CloseStationButton")));
    }
}
void UARPGCraftingStationPanelWidget::RefreshStationUI()
{
    EnsureNativeLayoutOrBindings();if(!Character||!Station||!BuildingUI)return;
    if(StationTitleText){if(Station->StationDefinition)StationTitleText->SetText(DefinitionName(Station->StationDefinition,Station->GetFName()));else if(Station->Definition)StationTitleText->SetText(DefinitionName(Station->Definition,Station->GetFName()));}
    TSubclassOf<UARPGStructureItemRowWidget>ItemRow=BuildingUI->GetStructureItemRowWidgetClass();PopulateItemList(WidgetTree,PlayerItemListBox,Character->Inventory,true,false,ItemRow,this);PopulateItemList(WidgetTree,StationInputListBox,Station->Inventory,false,false,ItemRow,this);PopulateItemList(WidgetTree,StationOutputListBox,Station->OutputInventory,false,true,ItemRow,this);
    if(StationRecipeListBox){StationRecipeListBox->ClearChildren();TSubclassOf<UARPGStationRecipeRowWidget>RowClass=BuildingUI->GetStationRecipeRowWidgetClass();if(!RowClass)RowClass=UARPGStationRecipeRowWidget::StaticClass();if(Station->StationDefinition){for(UARPGRecipeDefinition*Recipe:Station->StationDefinition->Recipes){if(!Recipe)continue;FARPGStationRecipeView V;V.Recipe=Recipe;V.DisplayName=DefinitionName(Recipe,Recipe->DefinitionId);V.InputSummary=RecipeAmountList(Recipe->Inputs);V.OutputSummary=RecipeAmountList(Recipe->Outputs);V.CraftSeconds=Recipe->CraftSeconds;V.bCanQueue=Station->CanQueueRecipe(Character,Recipe);V.FuelSummary=Recipe->bConsumesFuel?FText::Format(NSLOCTEXT("AkumasRPGFramework","FuelReq","Fuel: {0} x{1}"),FText::FromString(Recipe->FuelTag.ToString()),FText::AsNumber(FMath::Max(1,FMath::CeilToInt(Recipe->FuelPerCraft)))):NSLOCTEXT("AkumasRPGFramework","NoFuelReq","No fuel required");UARPGStationRecipeRowWidget*Row=WidgetTree->ConstructWidget<UARPGStationRecipeRowWidget>(RowClass);Row->InitializeStationRecipeRow(this,V);if(UVerticalBoxSlot*S=StationRecipeListBox->AddChildToVerticalBox(Row))S->SetPadding(FMargin(0,0,0,5));}}}
    RefreshStationProgress();BP_OnStationUIRefreshed();
}
void UARPGCraftingStationPanelWidget::RefreshStationProgress(){if(!Station)return;if(StationProgressBar)StationProgressBar->SetPercent(Station->GetCurrentCraftProgress01());if(StationStateText){FString State=TEXT("Idle");if(Station->StationState==EARPGCraftingStationState::Crafting)State=TEXT("Crafting");else if(Station->StationState==EARPGCraftingStationState::Blocked)State=TEXT("Blocked — check fuel, inputs or output space");else if(Station->StationState==EARPGCraftingStationState::Paused)State=TEXT("Paused");if(Station->CraftQueue.Num()>0)State+=FString::Printf(TEXT(" • Queue: %d • %s"),Station->CraftQueue.Num(),*Station->CraftQueue[0].RecipeId.ToString());StationStateText->SetText(FText::FromString(State));}}
void UARPGCraftingStationPanelWidget::HandleItemTransfer(const FARPGStructureItemView&View,int32 Quantity){if(!Character||!Station||!Character->Interaction||Quantity<=0||!View.InstanceId.IsValid())return;if(View.bFromPlayerInventory)Character->Interaction->DepositToStorageInstance(Station,View.InstanceId,Quantity);else if(View.bStationOutput)Character->Interaction->WithdrawStationOutputInstance(Station,View.InstanceId,Quantity);else Character->Interaction->WithdrawFromStorageInstance(Station,View.InstanceId,Quantity);}
void UARPGCraftingStationPanelWidget::QueueRecipe(UARPGRecipeDefinition*Recipe){if(Character&&Character->Interaction&&Station&&Recipe)Character->Interaction->QueueCraft(Station,Recipe,1);}
void UARPGCraftingStationPanelWidget::HandleClose(){if(BuildingUI)BuildingUI->CloseStructureUI();}
