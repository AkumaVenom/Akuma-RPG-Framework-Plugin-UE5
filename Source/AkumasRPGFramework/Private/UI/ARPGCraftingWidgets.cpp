#include "UI/ARPGCraftingWidgets.h"

#include "Actors/ARPGCharacter.h"
#include "Blueprint/WidgetTree.h"
#include "Components/ARPGCraftingComponent.h"
#include "Components/ARPGInventoryComponent.h"
#include "Components/ARPGInventoryUIComponent.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WidgetSwitcher.h"
#include "Data/ARPGItemDefinition.h"
#include "Engine/Texture2D.h"

namespace
{
    void StyleCraftText(UTextBlock* Text, int32 Size, const FLinearColor& Color = FLinearColor::White)
    {
        if (!Text) return;
        FSlateFontInfo Font = Text->GetFont();
        Font.Size = Size;
        Text->SetFont(Font);
        Text->SetColorAndOpacity(FSlateColor(Color));
    }

    UTextBlock* MakeText(UWidgetTree* Tree, const FName Name, const FText& Value, int32 Size = 12,
        const FLinearColor& Color = FLinearColor::White)
    {
        UTextBlock* Text = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
        Text->SetText(Value);
        Text->SetAutoWrapText(true);
        StyleCraftText(Text, Size, Color);
        return Text;
    }

    FText ItemAmountName(const FARPGItemAmount& Amount)
    {
        if (Amount.Item)
            return Amount.Item->DisplayName.IsEmpty() ? FText::FromName(Amount.Item->DefinitionId) : Amount.Item->DisplayName;
        return Amount.ItemId.IsNone() ? NSLOCTEXT("AkumasRPGFramework", "CraftUnknownItem", "Unknown Item") : FText::FromName(Amount.ItemId);
    }

    FText AmountListText(const TArray<FARPGItemAmount>& Amounts)
    {
        if (Amounts.Num() == 0) return NSLOCTEXT("AkumasRPGFramework", "CraftNone", "None");
        FString Out;
        for (const FARPGItemAmount& Amount : Amounts)
        {
            if (Amount.Quantity <= 0) continue;
            if (!Out.IsEmpty()) Out += TEXT("\n");
            Out += FString::Printf(TEXT("%s  x%d"), *ItemAmountName(Amount).ToString(), Amount.Quantity);
        }
        return Out.IsEmpty() ? NSLOCTEXT("AkumasRPGFramework", "CraftNone2", "None") : FText::FromString(Out);
    }

    FText RecipeName(const UARPGRecipeDefinition* Recipe)
    {
        if (!Recipe) return FText::GetEmpty();
        if (!Recipe->DisplayName.IsEmpty()) return Recipe->DisplayName;
        if (!Recipe->DefinitionId.IsNone()) return FText::FromName(Recipe->DefinitionId);
        return FText::FromName(Recipe->GetFName());
    }

    FText ItemName(const UARPGItemDefinition* Item, FName FallbackId)
    {
        if (Item && !Item->DisplayName.IsEmpty()) return Item->DisplayName;
        if (Item && !Item->DefinitionId.IsNone()) return FText::FromName(Item->DefinitionId);
        return FText::FromName(FallbackId);
    }
}

void UARPGCraftingRecipeRowWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    EnsureNativeLayoutOrBindings();
    if (RecipeButton) RecipeButton->OnClicked.AddUniqueDynamic(this, &UARPGCraftingRecipeRowWidget::HandleClicked);
}

void UARPGCraftingRecipeRowWidget::InitializeRecipeRow(UARPGCraftingPanelWidget* InOwnerPanel, const FARPGCraftingRecipeView& InView)
{
    OwnerPanel = InOwnerPanel;
    EnsureNativeLayoutOrBindings();
    if (RecipeButton) RecipeButton->OnClicked.AddUniqueDynamic(this, &UARPGCraftingRecipeRowWidget::HandleClicked);
    SetRecipeView(InView);
}

void UARPGCraftingRecipeRowWidget::SetRecipeView(const FARPGCraftingRecipeView& InView)
{
    View = InView;
    ApplyView();
    BP_OnRecipeRowUpdated(View);
}

void UARPGCraftingRecipeRowWidget::EnsureNativeLayoutOrBindings()
{
    if (!WidgetTree) return;
    if (!WidgetTree->RootWidget)
    {
        RecipeButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("RecipeButton"));
        WidgetTree->RootWidget = RecipeButton;
        UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("RecipeRow"));
        RecipeButton->AddChild(Row);

        USizeBox* IconSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
        IconSize->SetWidthOverride(44.f); IconSize->SetHeightOverride(44.f);
        RecipeIcon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("RecipeIcon"));
        IconSize->SetContent(RecipeIcon);
        if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(IconSize)) S->SetPadding(FMargin(4.f, 4.f, 8.f, 4.f));

        UVerticalBox* Texts = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
        if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(Texts))
        {
            FSlateChildSize Fill; Fill.SizeRule = ESlateSizeRule::Fill; S->SetSize(Fill); S->SetVerticalAlignment(VAlign_Center);
        }
        RecipeNameText = MakeText(WidgetTree, TEXT("RecipeNameText"), FText::GetEmpty(), 13, FLinearColor(0.94f,0.94f,0.96f,1.f));
        RecipeAvailabilityText = MakeText(WidgetTree, TEXT("RecipeAvailabilityText"), FText::GetEmpty(), 10, FLinearColor(0.62f,0.68f,0.76f,1.f));
        Texts->AddChildToVerticalBox(RecipeNameText);
        Texts->AddChildToVerticalBox(RecipeAvailabilityText);
    }
    else
    {
        if (!RecipeButton) RecipeButton = Cast<UButton>(WidgetTree->FindWidget(TEXT("RecipeButton")));
        if (!RecipeIcon) RecipeIcon = Cast<UImage>(WidgetTree->FindWidget(TEXT("RecipeIcon")));
        if (!RecipeNameText) RecipeNameText = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("RecipeNameText")));
        if (!RecipeAvailabilityText) RecipeAvailabilityText = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("RecipeAvailabilityText")));
    }
}

void UARPGCraftingRecipeRowWidget::ApplyView()
{
    EnsureNativeLayoutOrBindings();
    if (RecipeNameText) RecipeNameText->SetText(View.DisplayName);
    if (RecipeAvailabilityText)
    {
        RecipeAvailabilityText->SetText(View.bCanCraft
            ? FText::Format(NSLOCTEXT("AkumasRPGFramework", "CraftAvailableN", "Ready • Max {0}"), FText::AsNumber(View.MaxCraftable))
            : (View.FailureReason.IsEmpty() ? NSLOCTEXT("AkumasRPGFramework", "CraftUnavailable", "Unavailable") : View.FailureReason));
        RecipeAvailabilityText->SetColorAndOpacity(FSlateColor(View.bCanCraft ? FLinearColor(0.38f,0.88f,0.48f,1.f) : FLinearColor(0.94f,0.48f,0.34f,1.f)));
    }
    if (RecipeIcon)
    {
        UTexture2D* Texture = nullptr;
        if (View.Recipe && View.Recipe->Outputs.Num() > 0 && View.Recipe->Outputs[0].Item)
            Texture = View.Recipe->Outputs[0].Item->Icon.LoadSynchronous();
        RecipeIcon->SetBrushFromTexture(Texture, true);
        RecipeIcon->SetOpacity(Texture ? 1.f : 0.12f);
    }
}

void UARPGCraftingRecipeRowWidget::HandleClicked()
{
    if (OwnerPanel && View.Recipe) OwnerPanel->SelectRecipe(View.Recipe);
}

void UARPGRepairItemRowWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    EnsureNativeLayoutOrBindings();
    if (RepairItemButton) RepairItemButton->OnClicked.AddUniqueDynamic(this, &UARPGRepairItemRowWidget::HandleClicked);
}

void UARPGRepairItemRowWidget::InitializeRepairRow(UARPGCraftingPanelWidget* InOwnerPanel, const FARPGRepairItemView& InView)
{
    OwnerPanel = InOwnerPanel;
    EnsureNativeLayoutOrBindings();
    if (RepairItemButton) RepairItemButton->OnClicked.AddUniqueDynamic(this, &UARPGRepairItemRowWidget::HandleClicked);
    SetRepairView(InView);
}

void UARPGRepairItemRowWidget::SetRepairView(const FARPGRepairItemView& InView)
{
    View = InView;
    ApplyView();
    BP_OnRepairRowUpdated(View);
}

void UARPGRepairItemRowWidget::EnsureNativeLayoutOrBindings()
{
    if (!WidgetTree) return;
    if (!WidgetTree->RootWidget)
    {
        RepairItemButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("RepairItemButton"));
        WidgetTree->RootWidget = RepairItemButton;
        UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("RepairItemRow"));
        RepairItemButton->AddChild(Row);

        USizeBox* IconSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
        IconSize->SetWidthOverride(44.f); IconSize->SetHeightOverride(44.f);
        RepairItemIcon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("RepairItemIcon"));
        IconSize->SetContent(RepairItemIcon);
        if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(IconSize)) S->SetPadding(FMargin(4.f,4.f,8.f,4.f));

        UVerticalBox* Texts = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
        if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(Texts)) { FSlateChildSize Fill; Fill.SizeRule=ESlateSizeRule::Fill; S->SetSize(Fill); }
        RepairItemNameText = MakeText(WidgetTree, TEXT("RepairItemNameText"), FText::GetEmpty(), 13);
        RepairDurabilityBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("RepairDurabilityBar"));
        RepairDurabilityText = MakeText(WidgetTree, TEXT("RepairDurabilityText"), FText::GetEmpty(), 10, FLinearColor(0.66f,0.72f,0.82f,1.f));
        Texts->AddChildToVerticalBox(RepairItemNameText);
        if (UVerticalBoxSlot* S = Texts->AddChildToVerticalBox(RepairDurabilityBar)) S->SetPadding(FMargin(0.f,3.f,0.f,2.f));
        Texts->AddChildToVerticalBox(RepairDurabilityText);
    }
    else
    {
        if (!RepairItemButton) RepairItemButton = Cast<UButton>(WidgetTree->FindWidget(TEXT("RepairItemButton")));
        if (!RepairItemIcon) RepairItemIcon = Cast<UImage>(WidgetTree->FindWidget(TEXT("RepairItemIcon")));
        if (!RepairItemNameText) RepairItemNameText = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("RepairItemNameText")));
        if (!RepairDurabilityBar) RepairDurabilityBar = Cast<UProgressBar>(WidgetTree->FindWidget(TEXT("RepairDurabilityBar")));
        if (!RepairDurabilityText) RepairDurabilityText = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("RepairDurabilityText")));
    }
}

void UARPGRepairItemRowWidget::ApplyView()
{
    EnsureNativeLayoutOrBindings();
    if (RepairItemNameText) RepairItemNameText->SetText(View.DisplayName);
    if (RepairDurabilityBar) RepairDurabilityBar->SetPercent(View.DurabilityPercent);
    if (RepairDurabilityText)
        RepairDurabilityText->SetText(FText::Format(NSLOCTEXT("AkumasRPGFramework", "RepairDurabilityFmt", "{0} / {1}{2}"),
            FText::AsNumber(FMath::RoundToInt(View.CurrentDurability)), FText::AsNumber(FMath::RoundToInt(View.MaxDurability)),
            View.bBroken ? NSLOCTEXT("AkumasRPGFramework", "RepairBrokenSuffix", " • BROKEN") : FText::GetEmpty()));
    if (RepairItemIcon)
    {
        UTexture2D* Texture = View.ItemDefinition ? View.ItemDefinition->Icon.LoadSynchronous() : nullptr;
        RepairItemIcon->SetBrushFromTexture(Texture, true);
        RepairItemIcon->SetOpacity(Texture ? 1.f : 0.12f);
    }
}

void UARPGRepairItemRowWidget::HandleClicked()
{
    if (OwnerPanel && View.ItemInstanceId.IsValid()) OwnerPanel->SelectRepairItem(View.ItemInstanceId);
}

void UARPGCraftingPanelWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    EnsureNativeLayoutOrBindings();
    BindButtons();
}

void UARPGCraftingPanelWidget::InitializeCraftingUI(AARPGCharacter* InCharacter, UARPGInventoryUIComponent* InInventoryUI)
{
    ObservedCharacter = InCharacter;
    InventoryUI = InInventoryUI;
    EnsureNativeLayoutOrBindings();
    BindButtons();
    RefreshCraftingUI();
}

void UARPGCraftingPanelWidget::BindButtons()
{
    if (CraftModeButton) CraftModeButton->OnClicked.AddUniqueDynamic(this, &UARPGCraftingPanelWidget::HandleCraftModeClicked);
    if (RepairModeButton) RepairModeButton->OnClicked.AddUniqueDynamic(this, &UARPGCraftingPanelWidget::HandleRepairModeClicked);
    if (CraftMinusButton) CraftMinusButton->OnClicked.AddUniqueDynamic(this, &UARPGCraftingPanelWidget::HandleCraftMinusClicked);
    if (CraftPlusButton) CraftPlusButton->OnClicked.AddUniqueDynamic(this, &UARPGCraftingPanelWidget::HandleCraftPlusClicked);
    if (CraftMaxButton) CraftMaxButton->OnClicked.AddUniqueDynamic(this, &UARPGCraftingPanelWidget::HandleCraftMaxClicked);
    if (CraftButton) CraftButton->OnClicked.AddUniqueDynamic(this, &UARPGCraftingPanelWidget::HandleCraftClicked);
    if (CancelCraftButton) CancelCraftButton->OnClicked.AddUniqueDynamic(this, &UARPGCraftingPanelWidget::HandleCancelCraftClicked);
    if (RepairButton) RepairButton->OnClicked.AddUniqueDynamic(this, &UARPGCraftingPanelWidget::HandleRepairClicked);
}

void UARPGCraftingPanelWidget::EnsureNativeLayoutOrBindings()
{
    if (!WidgetTree) return;
    if (!WidgetTree->RootWidget)
    {
        UVerticalBox* Root = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("CraftingRoot"));
        WidgetTree->RootWidget = Root;

        UHorizontalBox* ModeBar = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("CraftingModeBar"));
        if (UVerticalBoxSlot* S=Root->AddChildToVerticalBox(ModeBar)) S->SetPadding(FMargin(0.f,0.f,0.f,8.f));
        CraftModeButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("CraftModeButton"));
        CraftModeButton->AddChild(MakeText(WidgetTree, TEXT("CraftModeLabel"), NSLOCTEXT("AkumasRPGFramework","CraftMode","CRAFT"), 12));
        RepairModeButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("RepairModeButton"));
        RepairModeButton->AddChild(MakeText(WidgetTree, TEXT("RepairModeLabel"), NSLOCTEXT("AkumasRPGFramework","RepairMode","REPAIR"), 12));
        USizeBox* CraftModeSize=WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass()); CraftModeSize->SetWidthOverride(130.f); CraftModeSize->SetHeightOverride(34.f); CraftModeSize->SetContent(CraftModeButton);
        USizeBox* RepairModeSize=WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass()); RepairModeSize->SetWidthOverride(130.f); RepairModeSize->SetHeightOverride(34.f); RepairModeSize->SetContent(RepairModeButton);
        if(UHorizontalBoxSlot* S=ModeBar->AddChildToHorizontalBox(CraftModeSize)) S->SetPadding(FMargin(0.f,0.f,6.f,0.f));
        ModeBar->AddChildToHorizontalBox(RepairModeSize);

        CraftingModeSwitcher = WidgetTree->ConstructWidget<UWidgetSwitcher>(UWidgetSwitcher::StaticClass(), TEXT("CraftingModeSwitcher"));
        if (UVerticalBoxSlot* S=Root->AddChildToVerticalBox(CraftingModeSwitcher)) { FSlateChildSize Fill; Fill.SizeRule=ESlateSizeRule::Fill; S->SetSize(Fill); }

        // CRAFT PAGE
        UHorizontalBox* CraftPage = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("CraftPage"));
        CraftingModeSwitcher->AddChild(CraftPage);
        USizeBox* ListSize=WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass()); ListSize->SetWidthOverride(330.f);
        UScrollBox* RecipeScroll=WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("RecipeScroll"));
        RecipeListBox=WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("RecipeListBox"));
        RecipeScroll->AddChild(RecipeListBox); ListSize->SetContent(RecipeScroll);
        if(UHorizontalBoxSlot* S=CraftPage->AddChildToHorizontalBox(ListSize)) S->SetPadding(FMargin(0.f,0.f,12.f,0.f));

        UBorder* CraftDetailsBorder=WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("CraftDetailsBorder"));
        CraftDetailsBorder->SetBrushColor(FLinearColor(0.02f,0.025f,0.038f,0.92f)); CraftDetailsBorder->SetPadding(FMargin(12.f));
        if(UHorizontalBoxSlot* S=CraftPage->AddChildToHorizontalBox(CraftDetailsBorder)) { FSlateChildSize Fill; Fill.SizeRule=ESlateSizeRule::Fill; S->SetSize(Fill); }
        UVerticalBox* Details=WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass()); CraftDetailsBorder->SetContent(Details);
        CraftingTitleText=MakeText(WidgetTree,TEXT("CraftingTitleText"),NSLOCTEXT("AkumasRPGFramework","CraftingHeading","Crafting"),18,FLinearColor(0.95f,0.78f,0.28f,1.f)); Details->AddChildToVerticalBox(CraftingTitleText);
        SelectedRecipeNameText=MakeText(WidgetTree,TEXT("SelectedRecipeNameText"),NSLOCTEXT("AkumasRPGFramework","SelectRecipe","Select a recipe"),16); if(UVerticalBoxSlot* S=Details->AddChildToVerticalBox(SelectedRecipeNameText)) S->SetPadding(FMargin(0.f,10.f,0.f,5.f));
        SelectedRecipeDetailsText=MakeText(WidgetTree,TEXT("SelectedRecipeDetailsText"),FText::GetEmpty(),11,FLinearColor(0.76f,0.80f,0.88f,1.f));
        if(UVerticalBoxSlot* S=Details->AddChildToVerticalBox(SelectedRecipeDetailsText)) { FSlateChildSize Fill; Fill.SizeRule=ESlateSizeRule::Fill; S->SetSize(Fill); }

        UHorizontalBox* QuantityRow=WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(),TEXT("CraftQuantityRow")); if(UVerticalBoxSlot* S=Details->AddChildToVerticalBox(QuantityRow)) S->SetPadding(FMargin(0.f,8.f,0.f,6.f));
        CraftMinusButton=WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(),TEXT("CraftMinusButton")); CraftMinusButton->AddChild(MakeText(WidgetTree,TEXT("CraftMinusLabel"),FText::FromString(TEXT("−")),14));
        CraftQuantityText=MakeText(WidgetTree,TEXT("CraftQuantityText"),FText::AsNumber(1),13);
        CraftPlusButton=WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(),TEXT("CraftPlusButton")); CraftPlusButton->AddChild(MakeText(WidgetTree,TEXT("CraftPlusLabel"),FText::FromString(TEXT("+")),14));
        CraftMaxButton=WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(),TEXT("CraftMaxButton")); CraftMaxButton->AddChild(MakeText(WidgetTree,TEXT("CraftMaxLabel"),NSLOCTEXT("AkumasRPGFramework","CraftMax","MAX"),10));
        auto AddSmall=[&](UWidget* W,float Width){USizeBox* B=WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass()); B->SetWidthOverride(Width);B->SetHeightOverride(32.f);B->SetContent(W); if(UHorizontalBoxSlot* S=QuantityRow->AddChildToHorizontalBox(B))S->SetPadding(FMargin(0.f,0.f,5.f,0.f));};
        AddSmall(CraftMinusButton,42.f); AddSmall(CraftQuantityText,52.f); AddSmall(CraftPlusButton,42.f); AddSmall(CraftMaxButton,62.f);

        UHorizontalBox* ActionRow=WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(),TEXT("CraftActionRow")); Details->AddChildToVerticalBox(ActionRow);
        CraftButton=WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(),TEXT("CraftButton")); CraftButtonText=MakeText(WidgetTree,TEXT("CraftButtonText"),NSLOCTEXT("AkumasRPGFramework","CraftButton","Craft"),12); CraftButton->AddChild(CraftButtonText);
        CancelCraftButton=WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(),TEXT("CancelCraftButton")); CancelCraftButton->AddChild(MakeText(WidgetTree,TEXT("CancelCraftLabel"),NSLOCTEXT("AkumasRPGFramework","CancelCraft","Cancel"),12));
        USizeBox* CraftB=WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());CraftB->SetWidthOverride(150.f);CraftB->SetHeightOverride(36.f);CraftB->SetContent(CraftButton); if(UHorizontalBoxSlot* S=ActionRow->AddChildToHorizontalBox(CraftB))S->SetPadding(FMargin(0.f,0.f,8.f,0.f));
        USizeBox* CancelB=WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());CancelB->SetWidthOverride(100.f);CancelB->SetHeightOverride(36.f);CancelB->SetContent(CancelCraftButton);ActionRow->AddChildToHorizontalBox(CancelB);
        CraftProgressBar=WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(),TEXT("CraftProgressBar")); if(UVerticalBoxSlot* S=Details->AddChildToVerticalBox(CraftProgressBar)) S->SetPadding(FMargin(0.f,9.f,0.f,2.f));
        CraftProgressText=MakeText(WidgetTree,TEXT("CraftProgressText"),FText::GetEmpty(),10,FLinearColor(0.68f,0.74f,0.84f,1.f)); Details->AddChildToVerticalBox(CraftProgressText);

        // REPAIR PAGE
        UHorizontalBox* RepairPage=WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(),TEXT("RepairPage")); CraftingModeSwitcher->AddChild(RepairPage);
        USizeBox* RepairListSize=WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());RepairListSize->SetWidthOverride(330.f);
        UScrollBox* RepairScroll=WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(),TEXT("RepairScroll")); RepairListBox=WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(),TEXT("RepairListBox"));RepairScroll->AddChild(RepairListBox);RepairListSize->SetContent(RepairScroll);
        if(UHorizontalBoxSlot* S=RepairPage->AddChildToHorizontalBox(RepairListSize))S->SetPadding(FMargin(0.f,0.f,12.f,0.f));
        UBorder* RepairDetailsBorder=WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(),TEXT("RepairDetailsBorder"));RepairDetailsBorder->SetBrushColor(FLinearColor(0.02f,0.025f,0.038f,0.92f));RepairDetailsBorder->SetPadding(FMargin(12.f)); if(UHorizontalBoxSlot* S=RepairPage->AddChildToHorizontalBox(RepairDetailsBorder)){FSlateChildSize Fill;Fill.SizeRule=ESlateSizeRule::Fill;S->SetSize(Fill);}
        UVerticalBox* RepairDetails=WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());RepairDetailsBorder->SetContent(RepairDetails);
        SelectedRepairNameText=MakeText(WidgetTree,TEXT("SelectedRepairNameText"),NSLOCTEXT("AkumasRPGFramework","SelectRepair","Select damaged equipment"),16,FLinearColor(0.95f,0.78f,0.28f,1.f));RepairDetails->AddChildToVerticalBox(SelectedRepairNameText);
        SelectedRepairDurabilityBar=WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(),TEXT("SelectedRepairDurabilityBar"));if(UVerticalBoxSlot* S=RepairDetails->AddChildToVerticalBox(SelectedRepairDurabilityBar))S->SetPadding(FMargin(0.f,10.f,0.f,5.f));
        SelectedRepairDetailsText=MakeText(WidgetTree,TEXT("SelectedRepairDetailsText"),FText::GetEmpty(),11,FLinearColor(0.76f,0.80f,0.88f,1.f));if(UVerticalBoxSlot* S=RepairDetails->AddChildToVerticalBox(SelectedRepairDetailsText)){FSlateChildSize Fill;Fill.SizeRule=ESlateSizeRule::Fill;S->SetSize(Fill);}
        RepairButton=WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(),TEXT("RepairButton"));RepairButtonText=MakeText(WidgetTree,TEXT("RepairButtonText"),NSLOCTEXT("AkumasRPGFramework","RepairButton","Repair Item"),12);RepairButton->AddChild(RepairButtonText);
        USizeBox* RepairB=WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());RepairB->SetWidthOverride(160.f);RepairB->SetHeightOverride(36.f);RepairB->SetContent(RepairButton);if(UVerticalBoxSlot* S=RepairDetails->AddChildToVerticalBox(RepairB))S->SetPadding(FMargin(0.f,8.f,0.f,0.f));
    }
    else
    {
#define ARPG_BIND(Type, Member, Name) if(!Member) Member=Cast<Type>(WidgetTree->FindWidget(TEXT(Name)))
        ARPG_BIND(UButton,CraftModeButton,"CraftModeButton"); ARPG_BIND(UButton,RepairModeButton,"RepairModeButton");
        ARPG_BIND(UWidgetSwitcher,CraftingModeSwitcher,"CraftingModeSwitcher"); ARPG_BIND(UVerticalBox,RecipeListBox,"RecipeListBox"); ARPG_BIND(UVerticalBox,RepairListBox,"RepairListBox");
        ARPG_BIND(UTextBlock,CraftingTitleText,"CraftingTitleText"); ARPG_BIND(UTextBlock,SelectedRecipeNameText,"SelectedRecipeNameText"); ARPG_BIND(UTextBlock,SelectedRecipeDetailsText,"SelectedRecipeDetailsText");
        ARPG_BIND(UTextBlock,CraftQuantityText,"CraftQuantityText"); ARPG_BIND(UButton,CraftMinusButton,"CraftMinusButton"); ARPG_BIND(UButton,CraftPlusButton,"CraftPlusButton"); ARPG_BIND(UButton,CraftMaxButton,"CraftMaxButton");
        ARPG_BIND(UButton,CraftButton,"CraftButton"); ARPG_BIND(UButton,CancelCraftButton,"CancelCraftButton"); ARPG_BIND(UTextBlock,CraftButtonText,"CraftButtonText"); ARPG_BIND(UProgressBar,CraftProgressBar,"CraftProgressBar"); ARPG_BIND(UTextBlock,CraftProgressText,"CraftProgressText");
        ARPG_BIND(UTextBlock,SelectedRepairNameText,"SelectedRepairNameText"); ARPG_BIND(UTextBlock,SelectedRepairDetailsText,"SelectedRepairDetailsText"); ARPG_BIND(UProgressBar,SelectedRepairDurabilityBar,"SelectedRepairDurabilityBar"); ARPG_BIND(UButton,RepairButton,"RepairButton"); ARPG_BIND(UTextBlock,RepairButtonText,"RepairButtonText");
#undef ARPG_BIND
    }
}

bool UARPGCraftingPanelWidget::BuildRecipeView(UARPGRecipeDefinition* Recipe, FARPGCraftingRecipeView& OutView) const
{
    OutView = FARPGCraftingRecipeView();
    if (!Recipe || !ObservedCharacter || !ObservedCharacter->Crafting) return false;
    OutView.Recipe = Recipe;
    OutView.DisplayName = RecipeName(Recipe);
    OutView.Description = Recipe->Description;
    OutView.Category = Recipe->CraftingCategory;
    OutView.InputSummary = AmountListText(Recipe->Inputs);
    OutView.OutputSummary = AmountListText(Recipe->Outputs);
    OutView.CraftSeconds = FMath::Max(0.f, Recipe->CraftSeconds);
    OutView.MaxCraftable = ObservedCharacter->Crafting->GetMaxCraftableCount(Recipe);
    FText Failure;
    OutView.bCanCraft = ObservedCharacter->Crafting->CanCraftRecipe(Recipe, 1, Failure);
    OutView.FailureReason = Failure;
    FString Req;
    if (!Recipe->RequiredSkillId.IsNone()) Req = FString::Printf(TEXT("%s Lv.%d"), *Recipe->RequiredSkillId.ToString(), Recipe->RequiredSkillLevel);
    if (Recipe->RequiredStationTag.IsValid())
    {
        if (!Req.IsEmpty()) Req += TEXT(" • ");
        Req += FString::Printf(TEXT("Station: %s"), *Recipe->RequiredStationTag.ToString());
    }
    OutView.RequirementSummary = Req.IsEmpty() ? NSLOCTEXT("AkumasRPGFramework","NoCraftRequirements","No special requirements") : FText::FromString(Req);
    return true;
}

bool UARPGCraftingPanelWidget::BuildRepairView(FGuid ItemInstanceId, FARPGRepairItemView& OutView) const
{
    OutView = FARPGRepairItemView();
    if (!ObservedCharacter || !ObservedCharacter->Inventory || !ObservedCharacter->Crafting || !ItemInstanceId.IsValid()) return false;
    const FARPGInventoryEntry* Entry=ObservedCharacter->Inventory->Items.FindByPredicate([&](const FARPGInventoryEntry& E){return E.InstanceId==ItemInstanceId;});
    if(!Entry) return false;
    UARPGItemDefinition* Definition=ObservedCharacter->Inventory->ResolveItemDefinition(*Entry);
    if(!Definition || !Definition->bUsesDurability || !Definition->bCanBeRepaired) return false;
    OutView.ItemInstanceId=ItemInstanceId;OutView.ItemDefinition=Definition;OutView.DisplayName=ItemName(Definition,Entry->ItemId);OutView.bEquipped=Entry->bEquipped;
    OutView.MaxDurability=FMath::Max(1.f,Definition->MaxDurability);OutView.CurrentDurability=FMath::Clamp(Entry->Durability,0.f,OutView.MaxDurability);OutView.DurabilityPercent=OutView.CurrentDurability/OutView.MaxDurability;OutView.bBroken=OutView.CurrentDurability<=KINDA_SMALL_NUMBER;
    FText Failure;OutView.bCanRepair=ObservedCharacter->Crafting->CanRepairItem(ItemInstanceId,Failure);OutView.FailureReason=Failure;
    TArray<FARPGItemAmount> Cost;ObservedCharacter->Crafting->GetRepairCost(ItemInstanceId,Cost);OutView.RepairCostSummary=AmountListText(Cost);
    return true;
}

void UARPGCraftingPanelWidget::RebuildRecipeList()
{
    RuntimeRecipeRows.Reset();
    if(!RecipeListBox || !ObservedCharacter || !ObservedCharacter->Crafting) return;
    RecipeListBox->ClearChildren();
    TSubclassOf<UARPGCraftingRecipeRowWidget> RowClass = UARPGCraftingRecipeRowWidget::StaticClass();
    if (InventoryUI && InventoryUI->CraftingRecipeRowWidgetClass) RowClass = InventoryUI->CraftingRecipeRowWidgetClass;
    for(UARPGRecipeDefinition* Recipe:ObservedCharacter->Crafting->PlayerRecipes)
    {
        if(!Recipe || !ObservedCharacter->Crafting->IsRecipeAvailableToPlayer(Recipe)) continue;
        FARPGCraftingRecipeView View;if(!BuildRecipeView(Recipe,View)) continue;
        UARPGCraftingRecipeRowWidget* Row=CreateWidget<UARPGCraftingRecipeRowWidget>(GetOwningPlayer(),RowClass);if(!Row) continue;
        Row->InitializeRecipeRow(this,View);RecipeListBox->AddChildToVerticalBox(Row);RuntimeRecipeRows.Add(Row);
    }
}

void UARPGCraftingPanelWidget::RebuildRepairList()
{
    RuntimeRepairRows.Reset();
    if(!RepairListBox || !ObservedCharacter || !ObservedCharacter->Inventory) return;
    RepairListBox->ClearChildren();
    TSubclassOf<UARPGRepairItemRowWidget> RowClass = UARPGRepairItemRowWidget::StaticClass();
    if (InventoryUI && InventoryUI->RepairItemRowWidgetClass) RowClass = InventoryUI->RepairItemRowWidgetClass;
    for(const FARPGInventoryEntry& Entry:ObservedCharacter->Inventory->Items)
    {
        FARPGRepairItemView View;if(!BuildRepairView(Entry.InstanceId,View)) continue;
        if(View.CurrentDurability>=View.MaxDurability-KINDA_SMALL_NUMBER) continue;
        UARPGRepairItemRowWidget* Row=CreateWidget<UARPGRepairItemRowWidget>(GetOwningPlayer(),RowClass);if(!Row) continue;
        Row->InitializeRepairRow(this,View);RepairListBox->AddChildToVerticalBox(Row);RuntimeRepairRows.Add(Row);
    }
}

void UARPGCraftingPanelWidget::RefreshSelectedRecipe()
{
    if(SelectedRecipe && !BuildRecipeView(SelectedRecipe,SelectedRecipeView)) SelectedRecipe=nullptr;
    if(!SelectedRecipe && ObservedCharacter && ObservedCharacter->Crafting)
    {
        for(UARPGRecipeDefinition* Recipe:ObservedCharacter->Crafting->PlayerRecipes) if(Recipe && ObservedCharacter->Crafting->IsRecipeAvailableToPlayer(Recipe)){SelectedRecipe=Recipe;BuildRecipeView(Recipe,SelectedRecipeView);break;}
    }
    if(!SelectedRecipe)
    {
        SelectedRecipeView=FARPGCraftingRecipeView(); if(SelectedRecipeNameText)SelectedRecipeNameText->SetText(NSLOCTEXT("AkumasRPGFramework","NoRecipeSelected","Select a recipe")); if(SelectedRecipeDetailsText)SelectedRecipeDetailsText->SetText(FText::GetEmpty()); if(CraftButton)CraftButton->SetIsEnabled(false); return;
    }
    const int32 Max=FMath::Max(1,SelectedRecipeView.MaxCraftable);CraftQuantity=FMath::Clamp(CraftQuantity,1,Max);
    if(SelectedRecipeNameText)SelectedRecipeNameText->SetText(SelectedRecipeView.DisplayName);
    if(SelectedRecipeDetailsText)
    {
        const FText Details=FText::Format(NSLOCTEXT("AkumasRPGFramework","RecipeDetailFmt","{0}\n\nINGREDIENTS\n{1}\n\nCREATES\n{2}\n\nREQUIREMENTS\n{3}\n\nCraft time: {4}s\nMax craftable now: {5}{6}"),
            SelectedRecipeView.Description,SelectedRecipeView.InputSummary,SelectedRecipeView.OutputSummary,SelectedRecipeView.RequirementSummary,FText::AsNumber(SelectedRecipeView.CraftSeconds),FText::AsNumber(SelectedRecipeView.MaxCraftable),
            SelectedRecipeView.FailureReason.IsEmpty()?FText::GetEmpty():FText::Format(NSLOCTEXT("AkumasRPGFramework","RecipeFailSuffix","\n\n{0}"),SelectedRecipeView.FailureReason));
        SelectedRecipeDetailsText->SetText(Details);
    }
    if(CraftQuantityText)CraftQuantityText->SetText(FText::AsNumber(CraftQuantity));
    const bool bBusy=ObservedCharacter && ObservedCharacter->Crafting && ObservedCharacter->Crafting->IsCrafting();
    if(CraftButton){FText Reason;const bool bCan=!bBusy && ObservedCharacter->Crafting->CanCraftRecipe(SelectedRecipe,CraftQuantity,Reason);CraftButton->SetIsEnabled(bCan);}
    BP_OnCraftingRecipeSelected(SelectedRecipeView);
}

void UARPGCraftingPanelWidget::RefreshSelectedRepair()
{
    if(SelectedRepairItemId.IsValid() && !BuildRepairView(SelectedRepairItemId,SelectedRepairView)) SelectedRepairItemId.Invalidate();
    if(!SelectedRepairItemId.IsValid() && ObservedCharacter && ObservedCharacter->Inventory)
    {
        for(const FARPGInventoryEntry& Entry:ObservedCharacter->Inventory->Items){FARPGRepairItemView V;if(BuildRepairView(Entry.InstanceId,V)&&V.CurrentDurability<V.MaxDurability-KINDA_SMALL_NUMBER){SelectedRepairItemId=Entry.InstanceId;SelectedRepairView=V;break;}}
    }
    if(!SelectedRepairItemId.IsValid())
    {
        SelectedRepairView=FARPGRepairItemView();if(SelectedRepairNameText)SelectedRepairNameText->SetText(NSLOCTEXT("AkumasRPGFramework","NoRepairSelected","No damaged repairable equipment"));if(SelectedRepairDetailsText)SelectedRepairDetailsText->SetText(FText::GetEmpty());if(SelectedRepairDurabilityBar)SelectedRepairDurabilityBar->SetPercent(0.f);if(RepairButton)RepairButton->SetIsEnabled(false);return;
    }
    if(SelectedRepairNameText)SelectedRepairNameText->SetText(SelectedRepairView.DisplayName);if(SelectedRepairDurabilityBar)SelectedRepairDurabilityBar->SetPercent(SelectedRepairView.DurabilityPercent);
    if(SelectedRepairDetailsText)SelectedRepairDetailsText->SetText(FText::Format(NSLOCTEXT("AkumasRPGFramework","RepairDetailFmt","Durability: {0} / {1} ({2}%)\nState: {3}\n\nREPAIR MATERIALS\n{4}{5}"),FText::AsNumber(FMath::RoundToInt(SelectedRepairView.CurrentDurability)),FText::AsNumber(FMath::RoundToInt(SelectedRepairView.MaxDurability)),FText::AsNumber(FMath::RoundToInt(SelectedRepairView.DurabilityPercent*100.f)),SelectedRepairView.bBroken?NSLOCTEXT("AkumasRPGFramework","BrokenState","BROKEN"):NSLOCTEXT("AkumasRPGFramework","DamagedState","Damaged"),SelectedRepairView.RepairCostSummary,SelectedRepairView.FailureReason.IsEmpty()?FText::GetEmpty():FText::Format(NSLOCTEXT("AkumasRPGFramework","RepairFailSuffix","\n\n{0}"),SelectedRepairView.FailureReason)));
    if(RepairButton)RepairButton->SetIsEnabled(SelectedRepairView.bCanRepair);BP_OnRepairItemSelected(SelectedRepairView);
}

void UARPGCraftingPanelWidget::RefreshCraftingUI()
{
    EnsureNativeLayoutOrBindings();
    RebuildRecipeList();
    RebuildRepairList();
    RefreshSelectedRecipe();
    RefreshSelectedRepair();
    ApplyModeVisibility();
    RefreshCraftingProgress();
    BP_OnCraftingUIRefreshed();
}

void UARPGCraftingPanelWidget::RefreshCraftingProgress()
{
    if (!ObservedCharacter || !ObservedCharacter->Crafting) return;
    const bool bCrafting = ObservedCharacter->Crafting->IsCrafting();
    if (CraftProgressBar) CraftProgressBar->SetPercent(bCrafting ? ObservedCharacter->Crafting->GetCraftProgress01() : 0.f);
    if (CraftProgressText)
        CraftProgressText->SetText(bCrafting
            ? FText::Format(NSLOCTEXT("AkumasRPGFramework","CraftProgressFmt","Crafting {0} • {1}s remaining • {2} craft(s) left"), RecipeName(ObservedCharacter->Crafting->ActiveRecipe), FText::AsNumber(ObservedCharacter->Crafting->GetCraftSecondsRemaining()), FText::AsNumber(ObservedCharacter->Crafting->ActiveRemainingCount))
            : NSLOCTEXT("AkumasRPGFramework","CraftIdle","Ready"));
    if (CancelCraftButton) CancelCraftButton->SetIsEnabled(bCrafting);
    if (CraftButton && bCrafting) CraftButton->SetIsEnabled(false);
}

void UARPGCraftingPanelWidget::SetMode(EARPGCraftingUIMode NewMode){Mode=NewMode;ApplyModeVisibility();}
void UARPGCraftingPanelWidget::ApplyModeVisibility(){if(CraftingModeSwitcher)CraftingModeSwitcher->SetActiveWidgetIndex(Mode==EARPGCraftingUIMode::Craft?0:1);if(CraftingTitleText)CraftingTitleText->SetText(Mode==EARPGCraftingUIMode::Craft?NSLOCTEXT("AkumasRPGFramework","CraftHeading2","Crafting"):NSLOCTEXT("AkumasRPGFramework","RepairHeading2","Equipment Repair"));}
void UARPGCraftingPanelWidget::SelectRecipe(UARPGRecipeDefinition* Recipe){SelectedRecipe=Recipe;CraftQuantity=1;RefreshSelectedRecipe();}
void UARPGCraftingPanelWidget::SelectRepairItem(FGuid ItemInstanceId){SelectedRepairItemId=ItemInstanceId;RefreshSelectedRepair();}
void UARPGCraftingPanelWidget::HandleCraftModeClicked(){SetMode(EARPGCraftingUIMode::Craft);}
void UARPGCraftingPanelWidget::HandleRepairModeClicked(){SetMode(EARPGCraftingUIMode::Repair);}
void UARPGCraftingPanelWidget::HandleCraftMinusClicked(){CraftQuantity=FMath::Max(1,CraftQuantity-1);RefreshSelectedRecipe();}
void UARPGCraftingPanelWidget::HandleCraftPlusClicked(){if(SelectedRecipe){const int32 Max=FMath::Max(1,SelectedRecipeView.MaxCraftable);CraftQuantity=FMath::Min(Max,CraftQuantity+1);}RefreshSelectedRecipe();}
void UARPGCraftingPanelWidget::HandleCraftMaxClicked(){if(SelectedRecipe)CraftQuantity=FMath::Max(1,SelectedRecipeView.MaxCraftable);RefreshSelectedRecipe();}
void UARPGCraftingPanelWidget::HandleCraftClicked(){if(ObservedCharacter&&ObservedCharacter->Crafting&&SelectedRecipe)ObservedCharacter->Crafting->CraftRecipe(SelectedRecipe,CraftQuantity);RefreshCraftingUI();}
void UARPGCraftingPanelWidget::HandleCancelCraftClicked(){if(ObservedCharacter&&ObservedCharacter->Crafting)ObservedCharacter->Crafting->CancelCrafting();RefreshCraftingUI();}
void UARPGCraftingPanelWidget::HandleRepairClicked(){if(ObservedCharacter&&ObservedCharacter->Crafting&&SelectedRepairItemId.IsValid())ObservedCharacter->Crafting->RepairItem(SelectedRepairItemId);RefreshCraftingUI();}
