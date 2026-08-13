#include "UI/ARPGInventoryWidgets.h"

#include "Actors/ARPGCharacter.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WidgetSwitcher.h"
#include "UI/ARPGCraftingWidgets.h"
#include "Components/ARPGInventoryUIComponent.h"
#include "Data/ARPGItemDefinition.h"
#include "Engine/Texture2D.h"
#include "InputCoreTypes.h"

namespace
{
    void StyleInventoryText(UTextBlock* Text, int32 FontSize, const FLinearColor& Color = FLinearColor::White)
    {
        if (!Text) return;
        FSlateFontInfo Font = Text->GetFont();
        Font.Size = FontSize;
        Text->SetFont(Font);
        Text->SetColorAndOpacity(FSlateColor(Color));
    }

    UTextBlock* MakeTextBlock(UWidgetTree* Tree, const FText& Value, int32 FontSize)
    {
        if (!Tree) return nullptr;
        UTextBlock* Text = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
        Text->SetText(Value);
        Text->SetJustification(ETextJustify::Center);
        StyleInventoryText(Text, FontSize);
        return Text;
    }

    FText RarityText(EARPGRarity Rarity)
    {
        switch (Rarity)
        {
            case EARPGRarity::Poor: return NSLOCTEXT("AkumasRPGFramework", "RarityPoor", "Poor");
            case EARPGRarity::Common: return NSLOCTEXT("AkumasRPGFramework", "RarityCommon", "Common");
            case EARPGRarity::Uncommon: return NSLOCTEXT("AkumasRPGFramework", "RarityUncommon", "Uncommon");
            case EARPGRarity::Rare: return NSLOCTEXT("AkumasRPGFramework", "RarityRare", "Rare");
            case EARPGRarity::Epic: return NSLOCTEXT("AkumasRPGFramework", "RarityEpic", "Epic");
            case EARPGRarity::Legendary: return NSLOCTEXT("AkumasRPGFramework", "RarityLegendary", "Legendary");
            case EARPGRarity::Mythic: return NSLOCTEXT("AkumasRPGFramework", "RarityMythic", "Mythic");
            default: return FText::GetEmpty();
        }
    }
}

void UARPGInventoryItemSlotWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    EnsureNativeLayoutOrBindings();
    ApplyViewToStandardFields();
}

void UARPGInventoryItemSlotWidget::InitializeInventorySlot(UARPGInventoryUIComponent* InInventoryUI, EARPGInventoryUISlotSource InSource, int32 InSlotNumber)
{
    InventoryUI = InInventoryUI;
    SlotView.Source = InSource;
    SlotView.SlotNumber = InSlotNumber;
    bDragVisualOnly = false;
    EnsureNativeLayoutOrBindings();
    ApplyViewToStandardFields();
}

void UARPGInventoryItemSlotWidget::InitializeAsDragVisual(const FARPGInventoryUISlotView& InView)
{
    InventoryUI = nullptr;
    SlotView = InView;
    bDragVisualOnly = true;
    SetIsEnabled(false);
    SetRenderOpacity(0.92f);
    EnsureNativeLayoutOrBindings();
    ApplyViewToStandardFields();
}

void UARPGInventoryItemSlotWidget::SetSlotView(const FARPGInventoryUISlotView& InView)
{
    SlotView = InView;
    EnsureNativeLayoutOrBindings();
    ApplyViewToStandardFields();
    BP_OnInventorySlotUpdated(SlotView);
}

FLinearColor UARPGInventoryItemSlotWidget::ResolveRarityColor() const
{
    switch (SlotView.Rarity)
    {
        case EARPGRarity::Poor: return FLinearColor(0.25f, 0.25f, 0.25f, 0.96f);
        case EARPGRarity::Common: return FLinearColor(0.12f, 0.13f, 0.16f, 0.96f);
        case EARPGRarity::Uncommon: return FLinearColor(0.06f, 0.22f, 0.08f, 0.96f);
        case EARPGRarity::Rare: return FLinearColor(0.04f, 0.12f, 0.30f, 0.96f);
        case EARPGRarity::Epic: return FLinearColor(0.20f, 0.06f, 0.30f, 0.96f);
        case EARPGRarity::Legendary: return FLinearColor(0.36f, 0.17f, 0.03f, 0.96f);
        case EARPGRarity::Mythic: return FLinearColor(0.32f, 0.05f, 0.08f, 0.96f);
        default: return FLinearColor(0.12f, 0.13f, 0.16f, 0.96f);
    }
}

void UARPGInventoryItemSlotWidget::EnsureNativeLayoutOrBindings()
{
    if (!WidgetTree) return;

    if (!WidgetTree->RootWidget)
    {
        USizeBox* RootSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("ItemSlotRoot"));
        RootSize->SetWidthOverride(80.f);
        RootSize->SetHeightOverride(80.f);
        WidgetTree->RootWidget = RootSize;

        SlotBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("SlotBorder"));
        SlotBorder->SetPadding(FMargin(4.f));
        RootSize->SetContent(SlotBorder);

        UOverlay* Overlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("ItemSlotOverlay"));
        SlotBorder->SetContent(Overlay);

        ItemIcon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("ItemIcon"));
        if (UOverlaySlot* IconSlot = Overlay->AddChildToOverlay(ItemIcon))
        {
            IconSlot->SetHorizontalAlignment(HAlign_Fill);
            IconSlot->SetVerticalAlignment(VAlign_Fill);
            IconSlot->SetPadding(FMargin(7.f, 7.f, 7.f, 13.f));
        }

        SlotNumberText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("SlotNumberText"));
        StyleInventoryText(SlotNumberText, 11, FLinearColor(0.88f, 0.90f, 0.95f, 1.f));
        if (UOverlaySlot* NumberSlot = Overlay->AddChildToOverlay(SlotNumberText))
        {
            NumberSlot->SetHorizontalAlignment(HAlign_Left);
            NumberSlot->SetVerticalAlignment(VAlign_Top);
            NumberSlot->SetPadding(FMargin(2.f));
        }

        QuantityText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("QuantityText"));
        StyleInventoryText(QuantityText, 12, FLinearColor(1.f, 0.95f, 0.72f, 1.f));
        if (UOverlaySlot* QuantitySlot = Overlay->AddChildToOverlay(QuantityText))
        {
            QuantitySlot->SetHorizontalAlignment(HAlign_Right);
            QuantitySlot->SetVerticalAlignment(VAlign_Bottom);
            QuantitySlot->SetPadding(FMargin(2.f, 2.f, 3.f, 2.f));
        }

        EquippedText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("EquippedText"));
        StyleInventoryText(EquippedText, 10, FLinearColor(0.45f, 0.95f, 0.45f, 1.f));
        if (UOverlaySlot* EquippedSlot = Overlay->AddChildToOverlay(EquippedText))
        {
            EquippedSlot->SetHorizontalAlignment(HAlign_Left);
            EquippedSlot->SetVerticalAlignment(VAlign_Bottom);
            EquippedSlot->SetPadding(FMargin(3.f));
        }

        ItemNameText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ItemNameText"));
        StyleInventoryText(ItemNameText, 9, FLinearColor(0.92f, 0.92f, 0.94f, 1.f));
        ItemNameText->SetJustification(ETextJustify::Center);
        if (UOverlaySlot* NameSlot = Overlay->AddChildToOverlay(ItemNameText))
        {
            NameSlot->SetHorizontalAlignment(HAlign_Fill);
            NameSlot->SetVerticalAlignment(VAlign_Bottom);
            NameSlot->SetPadding(FMargin(3.f, 0.f, 3.f, 1.f));
        }

        CooldownBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("CooldownBar"));
        CooldownBar->SetFillColorAndOpacity(FLinearColor(0.25f, 0.60f, 1.f, 0.92f));
        if (UOverlaySlot* CooldownSlot = Overlay->AddChildToOverlay(CooldownBar))
        {
            CooldownSlot->SetHorizontalAlignment(HAlign_Fill);
            CooldownSlot->SetVerticalAlignment(VAlign_Bottom);
            CooldownSlot->SetPadding(FMargin(4.f, 0.f, 4.f, 1.f));
        }

        DurabilityBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("DurabilityBar"));
        DurabilityBar->SetFillColorAndOpacity(FLinearColor(0.88f, 0.70f, 0.20f, 0.95f));
        if (UOverlaySlot* DurabilitySlot = Overlay->AddChildToOverlay(DurabilityBar))
        {
            DurabilitySlot->SetHorizontalAlignment(HAlign_Fill);
            DurabilitySlot->SetVerticalAlignment(VAlign_Bottom);
            DurabilitySlot->SetPadding(FMargin(5.f, 0.f, 5.f, 6.f));
        }

        BrokenText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("BrokenText"));
        BrokenText->SetText(NSLOCTEXT("AkumasRPGFramework", "InventoryBrokenOverlay", "BROKEN"));
        BrokenText->SetJustification(ETextJustify::Center);
        StyleInventoryText(BrokenText, 10, FLinearColor(1.f, 0.28f, 0.20f, 1.f));
        if (UOverlaySlot* BrokenSlot = Overlay->AddChildToOverlay(BrokenText))
        {
            BrokenSlot->SetHorizontalAlignment(HAlign_Fill);
            BrokenSlot->SetVerticalAlignment(VAlign_Center);
        }
    }
    else
    {
        if (!SlotBorder) SlotBorder = Cast<UBorder>(GetWidgetFromName(TEXT("SlotBorder")));
        if (!ItemIcon) ItemIcon = Cast<UImage>(GetWidgetFromName(TEXT("ItemIcon")));
        if (!QuantityText) QuantityText = Cast<UTextBlock>(GetWidgetFromName(TEXT("QuantityText")));
        if (!SlotNumberText) SlotNumberText = Cast<UTextBlock>(GetWidgetFromName(TEXT("SlotNumberText")));
        if (!ItemNameText) ItemNameText = Cast<UTextBlock>(GetWidgetFromName(TEXT("ItemNameText")));
        if (!EquippedText) EquippedText = Cast<UTextBlock>(GetWidgetFromName(TEXT("EquippedText")));
        if (!CooldownBar) CooldownBar = Cast<UProgressBar>(GetWidgetFromName(TEXT("CooldownBar")));
        if (!DurabilityBar) DurabilityBar = Cast<UProgressBar>(GetWidgetFromName(TEXT("DurabilityBar")));
        if (!BrokenText) BrokenText = Cast<UTextBlock>(GetWidgetFromName(TEXT("BrokenText")));
    }
}

void UARPGInventoryItemSlotWidget::ApplyViewToStandardFields()
{
    if (SlotBorder)
    {
        FLinearColor Color = SlotView.bOccupied ? ResolveRarityColor() : FLinearColor(0.035f, 0.04f, 0.055f, 0.88f);
        if (SlotView.bActive) Color = FLinearColor(0.56f, 0.40f, 0.07f, 0.98f);
        else if (SlotView.bEquipped) Color = FLinearColor(0.07f, 0.30f, 0.12f, 0.96f);
        SlotBorder->SetBrushColor(Color);
    }

    if (SlotNumberText)
    {
        SlotNumberText->SetText(SlotView.Source == EARPGInventoryUISlotSource::QuickAccess
            ? FText::AsNumber(FMath::Max(1, SlotView.SlotNumber))
            : FText::GetEmpty());
        SlotNumberText->SetVisibility(SlotView.Source == EARPGInventoryUISlotSource::QuickAccess ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    }

    if (ItemIcon)
    {
        UTexture2D* Texture = nullptr;
        if (SlotView.ItemDefinition && !SlotView.ItemDefinition->Icon.IsNull()) Texture = SlotView.ItemDefinition->Icon.LoadSynchronous();
        if (Texture)
        {
            ItemIcon->SetBrushFromTexture(Texture, false);
            ItemIcon->SetVisibility(ESlateVisibility::HitTestInvisible);
        }
        else
        {
            ItemIcon->SetVisibility(ESlateVisibility::Hidden);
        }
    }

    if (QuantityText)
    {
        QuantityText->SetText(SlotView.bOccupied && SlotView.Quantity > 1 ? FText::AsNumber(SlotView.Quantity) : FText::GetEmpty());
        QuantityText->SetVisibility(SlotView.bOccupied && SlotView.Quantity > 1 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    }

    if (EquippedText)
    {
        EquippedText->SetText(SlotView.bEquipped ? NSLOCTEXT("AkumasRPGFramework", "InventoryEquippedMark", "E") : FText::GetEmpty());
        EquippedText->SetVisibility(SlotView.bEquipped ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    }

    if (ItemNameText)
    {
        const bool bShowName = SlotView.bOccupied && SlotView.Source == EARPGInventoryUISlotSource::Inventory;
        ItemNameText->SetText(bShowName ? SlotView.DisplayName : FText::GetEmpty());
        ItemNameText->SetVisibility(bShowName ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    }

    if (CooldownBar)
    {
        const bool bCoolingDown = SlotView.Source == EARPGInventoryUISlotSource::QuickAccess && SlotView.CooldownRemaining > KINDA_SMALL_NUMBER;
        CooldownBar->SetPercent(FMath::Clamp(SlotView.CooldownPercent, 0.f, 1.f));
        CooldownBar->SetVisibility(bCoolingDown ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    }

    if (DurabilityBar)
    {
        DurabilityBar->SetPercent(FMath::Clamp(SlotView.DurabilityPercent, 0.f, 1.f));
        DurabilityBar->SetVisibility(SlotView.bOccupied && SlotView.bUsesDurability ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    }
    if (BrokenText)
        BrokenText->SetVisibility(SlotView.bOccupied && SlotView.bBroken ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);

    if (SlotView.bOccupied)
    {
        const FText DurabilityLine = SlotView.bUsesDurability
            ? FText::Format(
                NSLOCTEXT("AkumasRPGFramework", "InventoryTooltipDurability", "\nDurability: {0} / {1} ({2}%){3}"),
                FText::AsNumber(FMath::RoundToInt(SlotView.Durability)),
                FText::AsNumber(FMath::RoundToInt(SlotView.MaxDurability)),
                FText::AsNumber(FMath::RoundToInt(FMath::Clamp(SlotView.DurabilityPercent, 0.f, 1.f) * 100.f)),
                SlotView.bBroken ? NSLOCTEXT("AkumasRPGFramework", "InventoryTooltipBroken", "  BROKEN") : FText::GetEmpty())
            : FText::GetEmpty();
        const FText Tooltip = FText::Format(
            NSLOCTEXT("AkumasRPGFramework", "InventorySlotTooltip", "{0}\n{1}\nQuantity: {2}{3}{4}"),
            SlotView.DisplayName,
            SlotView.Description,
            FText::AsNumber(FMath::Max(0, SlotView.Quantity)),
            DurabilityLine,
            SlotView.bEquipped ? NSLOCTEXT("AkumasRPGFramework", "InventoryTooltipEquipped", "\nEquipped") : FText::GetEmpty());
        SetToolTipText(Tooltip);
    }
    else
    {
        SetToolTipText(FText::GetEmpty());
    }
}

FReply UARPGInventoryItemSlotWidget::NativeOnPreviewMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    // Inventory items are hosted beneath a ScrollBox. Use the tunneling/preview route for
    // inventory presses so selection and drag detection are armed before the ScrollBox can
    // claim the pointer gesture. Quick Access keeps its normal bubbling path.
    if (bDragVisualOnly || !InventoryUI || SlotView.Source != EARPGInventoryUISlotSource::Inventory)
        return Super::NativeOnPreviewMouseButtonDown(InGeometry, InMouseEvent);

    if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton && SlotView.bOccupied)
    {
        InventoryUI->SelectInventorySlot(SlotView.SlotNumber);
        InventoryUI->ActivateInventoryItem(SlotView.ItemInstanceId);
        return FReply::Handled();
    }

    if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
    {
        InventoryUI->SelectInventorySlot(SlotView.SlotNumber);
        if (SlotView.bOccupied)
            return UWidgetBlueprintLibrary::DetectDragIfPressed(InMouseEvent, this, EKeys::LeftMouseButton).NativeReply;
        return FReply::Handled();
    }

    return Super::NativeOnPreviewMouseButtonDown(InGeometry, InMouseEvent);
}

FReply UARPGInventoryItemSlotWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (bDragVisualOnly || !InventoryUI) return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);

    if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton && SlotView.Source == EARPGInventoryUISlotSource::Inventory && SlotView.bOccupied)
    {
        InventoryUI->SelectInventorySlot(SlotView.SlotNumber);
        InventoryUI->ActivateInventoryItem(SlotView.ItemInstanceId);
        return FReply::Handled();
    }

    if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
    {
        if (SlotView.Source == EARPGInventoryUISlotSource::Inventory)
            InventoryUI->SelectInventorySlot(SlotView.SlotNumber);

        if (SlotView.bOccupied)
        {
            bPointerPressed = true;
            return UWidgetBlueprintLibrary::DetectDragIfPressed(InMouseEvent, this, EKeys::LeftMouseButton).NativeReply;
        }
        return FReply::Handled();
    }

    return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply UARPGInventoryItemSlotWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (bDragVisualOnly || !InventoryUI) return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
    if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && bPointerPressed)
    {
        bPointerPressed = false;
        if (SlotView.Source == EARPGInventoryUISlotSource::QuickAccess && SlotView.bOccupied)
            InventoryUI->ActivateQuickAccessSlot(SlotView.SlotNumber);
        else if (SlotView.Source == EARPGInventoryUISlotSource::Inventory)
            InventoryUI->SelectInventorySlot(SlotView.SlotNumber);
        return FReply::Handled();
    }
    return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

void UARPGInventoryItemSlotWidget::NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation)
{
    Super::NativeOnDragDetected(InGeometry, InMouseEvent, OutOperation);
    bPointerPressed = false;
    if (bDragVisualOnly || !InventoryUI || !SlotView.bOccupied) return;

    UARPGInventoryDragDropOperation* Operation = NewObject<UARPGInventoryDragDropOperation>(this);
    if (!Operation) return;
    Operation->Source = SlotView.Source;
    Operation->SourceSlotNumber = SlotView.SlotNumber;
    Operation->ItemInstanceId = SlotView.ItemInstanceId;
    Operation->ItemId = SlotView.ItemId;
    Operation->SourceView = SlotView;
    Operation->InventoryUI = InventoryUI;
    Operation->Pivot = EDragPivot::MouseDown;

    if (APlayerController* PC = GetOwningPlayer())
    {
        if (UARPGInventoryItemSlotWidget* DragVisual = CreateWidget<UARPGInventoryItemSlotWidget>(PC, GetClass()))
        {
            DragVisual->InitializeAsDragVisual(SlotView);
            Operation->DefaultDragVisual = DragVisual;
        }
    }
    OutOperation = Operation;
}

bool UARPGInventoryItemSlotWidget::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
    UARPGInventoryDragDropOperation* Operation = Cast<UARPGInventoryDragDropOperation>(InOperation);
    UARPGInventoryUIComponent* UI = InventoryUI ? InventoryUI.Get() : (Operation ? Operation->InventoryUI.Get() : nullptr);
    if (!Operation || !UI || Operation->bDropHandled) return Super::NativeOnDrop(InGeometry, InDragDropEvent, InOperation);

    bool bAccepted = false;
    if (SlotView.Source == EARPGInventoryUISlotSource::QuickAccess)
    {
        if (Operation->Source == EARPGInventoryUISlotSource::Inventory)
            bAccepted = UI->AssignInventoryItemToQuickAccess(Operation->ItemInstanceId, SlotView.SlotNumber);
        else if (Operation->Source == EARPGInventoryUISlotSource::QuickAccess)
            bAccepted = UI->SwapQuickAccessSlots(Operation->SourceSlotNumber, SlotView.SlotNumber);
    }
    else if (SlotView.Source == EARPGInventoryUISlotSource::Inventory && Operation->Source == EARPGInventoryUISlotSource::QuickAccess)
    {
        bAccepted = UI->ClearQuickAccessSlot(Operation->SourceSlotNumber, UI->bUnequipActiveItemWhenDraggedOff);
    }

    if (bAccepted)
    {
        Operation->bDropHandled = true;
        return true;
    }
    return Super::NativeOnDrop(InGeometry, InDragDropEvent, InOperation);
}

void UARPGInventoryItemSlotWidget::NativeOnDragCancelled(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
    UARPGInventoryDragDropOperation* Operation = Cast<UARPGInventoryDragDropOperation>(InOperation);
    if (Operation && !Operation->bDropHandled && Operation->Source == EARPGInventoryUISlotSource::QuickAccess && Operation->InventoryUI)
    {
        Operation->InventoryUI->ClearQuickAccessSlot(Operation->SourceSlotNumber, Operation->InventoryUI->bUnequipActiveItemWhenDraggedOff);
        Operation->bDropHandled = true;
    }
    Super::NativeOnDragCancelled(InDragDropEvent, InOperation);
}

void UARPGInventoryPanelWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    EnsureNativeLayoutOrBindings();
    if (CloseButton) CloseButton->OnClicked.AddUniqueDynamic(this, &UARPGInventoryPanelWidget::HandleCloseClicked);
    if (PrimaryActionButton) PrimaryActionButton->OnClicked.AddUniqueDynamic(this, &UARPGInventoryPanelWidget::HandlePrimaryActionClicked);
    if (InventoryTabButton) InventoryTabButton->OnClicked.AddUniqueDynamic(this, &UARPGInventoryPanelWidget::HandleInventoryTabClicked);
    if (CraftingTabButton) CraftingTabButton->OnClicked.AddUniqueDynamic(this, &UARPGInventoryPanelWidget::HandleCraftingTabClicked);
}

void UARPGInventoryPanelWidget::InitializeInventoryUI(AARPGCharacter* InCharacter, UARPGInventoryUIComponent* InInventoryUIComponent)
{
    ObservedCharacter = InCharacter;
    InventoryUI = InInventoryUIComponent;
    EnsureNativeLayoutOrBindings();
    if (CloseButton) CloseButton->OnClicked.AddUniqueDynamic(this, &UARPGInventoryPanelWidget::HandleCloseClicked);
    if (PrimaryActionButton) PrimaryActionButton->OnClicked.AddUniqueDynamic(this, &UARPGInventoryPanelWidget::HandlePrimaryActionClicked);
    if (InventoryTabButton) InventoryTabButton->OnClicked.AddUniqueDynamic(this, &UARPGInventoryPanelWidget::HandleInventoryTabClicked);
    if (CraftingTabButton) CraftingTabButton->OnClicked.AddUniqueDynamic(this, &UARPGInventoryPanelWidget::HandleCraftingTabClicked);
    EnsureCraftingPanel();
    RebuildInventoryGrid();
    SetActiveTab(EARPGItemManagementTab::Inventory);
    RefreshInventoryUI();
}

void UARPGInventoryPanelWidget::EnsureNativeLayoutOrBindings()
{
    if (!WidgetTree) return;
    if (!WidgetTree->RootWidget)
    {
        UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("InventoryUIRoot"));
        WidgetTree->RootWidget = Root;

        UBorder* ScreenDim = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("InventoryUIScreenDim"));
        ScreenDim->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.42f));
        ScreenDim->SetVisibility(ESlateVisibility::HitTestInvisible);
        if (UOverlaySlot* S = Root->AddChildToOverlay(ScreenDim)) { S->SetHorizontalAlignment(HAlign_Fill); S->SetVerticalAlignment(VAlign_Fill); }

        USizeBox* PanelSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("InventoryUIPanelSize"));
        PanelSize->SetWidthOverride(930.f); PanelSize->SetHeightOverride(680.f);
        PanelSize->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
        if (UOverlaySlot* S = Root->AddChildToOverlay(PanelSize))
        {
            S->SetHorizontalAlignment(HAlign_Center); S->SetVerticalAlignment(VAlign_Center); S->SetPadding(FMargin(24.f,24.f,24.f,120.f));
        }

        UBorder* Background = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("InventoryUIPanelBackground"));
        Background->SetPadding(FMargin(16.f)); Background->SetBrushColor(FLinearColor(0.012f,0.015f,0.024f,0.975f)); Background->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
        PanelSize->SetContent(Background);
        UVerticalBox* Main = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("InventoryUIMainStack")); Background->SetContent(Main);

        UHorizontalBox* Header = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("InventoryUIHeader"));
        if (UVerticalBoxSlot* S=Main->AddChildToVerticalBox(Header)) S->SetPadding(FMargin(0.f,0.f,0.f,8.f));
        UTextBlock* Title=WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),TEXT("ItemManagementTitleText")); Title->SetText(NSLOCTEXT("AkumasRPGFramework","ItemManagementTitle","Item Management")); StyleInventoryText(Title,26,FLinearColor(0.95f,0.78f,0.28f,1.f));
        if(UHorizontalBoxSlot* S=Header->AddChildToHorizontalBox(Title)){FSlateChildSize Fill;Fill.SizeRule=ESlateSizeRule::Fill;S->SetSize(Fill);}
        CapacityText=WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),TEXT("CapacityText"));CapacityText->SetJustification(ETextJustify::Right);StyleInventoryText(CapacityText,13,FLinearColor(0.76f,0.80f,0.88f,1.f));
        USizeBox* Cap=WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());Cap->SetWidthOverride(220.f);Cap->SetContent(CapacityText);Header->AddChildToHorizontalBox(Cap);
        CloseButton=WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(),TEXT("CloseButton"));UTextBlock* CloseText=WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());CloseText->SetText(NSLOCTEXT("AkumasRPGFramework","InventoryClose","Close"));CloseText->SetJustification(ETextJustify::Center);StyleInventoryText(CloseText,12);CloseButton->AddChild(CloseText);
        USizeBox* CloseBox=WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());CloseBox->SetWidthOverride(90.f);CloseBox->SetHeightOverride(34.f);CloseBox->SetContent(CloseButton);if(UHorizontalBoxSlot* S=Header->AddChildToHorizontalBox(CloseBox))S->SetPadding(FMargin(8.f,0.f,0.f,0.f));

        // Top-level extensible item-management tabs. Future systems can add switcher pages without replacing the shell.
        UHorizontalBox* Tabs=WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(),TEXT("ItemManagementTabs"));if(UVerticalBoxSlot* S=Main->AddChildToVerticalBox(Tabs))S->SetPadding(FMargin(0.f,0.f,0.f,10.f));
        InventoryTabButton=WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(),TEXT("InventoryTabButton"));InventoryTabButton->AddChild(MakeTextBlock(WidgetTree, NSLOCTEXT("AkumasRPGFramework","InventoryTab","INVENTORY"), 12));
        CraftingTabButton=WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(),TEXT("CraftingTabButton"));CraftingTabButton->AddChild(MakeTextBlock(WidgetTree, NSLOCTEXT("AkumasRPGFramework","CraftingTab","CRAFTING & REPAIR"), 12));
        USizeBox* InvTabSize=WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());InvTabSize->SetWidthOverride(170.f);InvTabSize->SetHeightOverride(36.f);InvTabSize->SetContent(InventoryTabButton);if(UHorizontalBoxSlot* S=Tabs->AddChildToHorizontalBox(InvTabSize))S->SetPadding(FMargin(0.f,0.f,7.f,0.f));
        USizeBox* CraftTabSize=WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());CraftTabSize->SetWidthOverride(190.f);CraftTabSize->SetHeightOverride(36.f);CraftTabSize->SetContent(CraftingTabButton);Tabs->AddChildToHorizontalBox(CraftTabSize);

        MainTabSwitcher=WidgetTree->ConstructWidget<UWidgetSwitcher>(UWidgetSwitcher::StaticClass(),TEXT("MainTabSwitcher"));
        if(UVerticalBoxSlot* S=Main->AddChildToVerticalBox(MainTabSwitcher)){FSlateChildSize Fill;Fill.SizeRule=ESlateSizeRule::Fill;S->SetSize(Fill);}

        // Inventory page preserves the proven drag/drop grid and item context action flow.
        UVerticalBox* InventoryPage=WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(),TEXT("InventoryPage"));MainTabSwitcher->AddChild(InventoryPage);
        UScrollBox* Scroll=WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(),TEXT("InventoryScroll"));if(UVerticalBoxSlot* S=InventoryPage->AddChildToVerticalBox(Scroll)){FSlateChildSize Fill;Fill.SizeRule=ESlateSizeRule::Fill;S->SetSize(Fill);}
        InventoryGrid=WidgetTree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass(),TEXT("InventoryGrid"));InventoryGrid->SetVisibility(ESlateVisibility::SelfHitTestInvisible);Scroll->AddChild(InventoryGrid);
        UBorder* DetailBorder=WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(),TEXT("InventorySelectionBorder"));DetailBorder->SetPadding(FMargin(10.f));DetailBorder->SetBrushColor(FLinearColor(0.025f,0.03f,0.045f,0.96f));if(UVerticalBoxSlot* S=InventoryPage->AddChildToVerticalBox(DetailBorder))S->SetPadding(FMargin(0.f,9.f,0.f,0.f));
        UVerticalBox* Detail=WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());DetailBorder->SetContent(Detail);
        SelectedItemNameText=WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),TEXT("SelectedItemNameText"));StyleInventoryText(SelectedItemNameText,16,FLinearColor(0.94f,0.77f,0.30f,1.f));Detail->AddChildToVerticalBox(SelectedItemNameText);
        SelectedItemDetailsText=WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),TEXT("SelectedItemDetailsText"));SelectedItemDetailsText->SetAutoWrapText(true);StyleInventoryText(SelectedItemDetailsText,11,FLinearColor(0.76f,0.79f,0.86f,1.f));Detail->AddChildToVerticalBox(SelectedItemDetailsText);
        USizeBox* ActionSize=WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(),TEXT("InventoryPrimaryActionSize"));ActionSize->SetWidthOverride(150.f);ActionSize->SetHeightOverride(36.f);if(UVerticalBoxSlot* S=Detail->AddChildToVerticalBox(ActionSize))S->SetPadding(FMargin(0.f,8.f,0.f,0.f));
        PrimaryActionButton=WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(),TEXT("PrimaryActionButton"));PrimaryActionText=WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),TEXT("PrimaryActionText"));PrimaryActionText->SetJustification(ETextJustify::Center);StyleInventoryText(PrimaryActionText,13);PrimaryActionButton->AddChild(PrimaryActionText);PrimaryActionButton->SetVisibility(ESlateVisibility::Collapsed);ActionSize->SetContent(PrimaryActionButton);
        UTextBlock* Help=WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),TEXT("InventoryHelpText"));Help->SetText(NSLOCTEXT("AkumasRPGFramework","InventoryHelpTabs","Select or right-click an item to use/equip it. Drag items to Quick Access. Durable equipment shows its condition directly on the slot. Use the Crafting & Repair tab to create or restore equipment."));Help->SetAutoWrapText(true);StyleInventoryText(Help,10,FLinearColor(0.58f,0.62f,0.70f,1.f));if(UVerticalBoxSlot* S=InventoryPage->AddChildToVerticalBox(Help))S->SetPadding(FMargin(0.f,7.f,0.f,0.f));

        // Crafting page is hosted separately so the entire subsystem can be reskinned/replaced as one Widget Class.
        CraftingPageHost=WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(),TEXT("CraftingPageHost"));CraftingPageHost->SetVisibility(ESlateVisibility::SelfHitTestInvisible);MainTabSwitcher->AddChild(CraftingPageHost);
    }
    else
    {
        if (!InventoryGrid) InventoryGrid=Cast<UUniformGridPanel>(GetWidgetFromName(TEXT("InventoryGrid")));
        if (!CapacityText) CapacityText=Cast<UTextBlock>(GetWidgetFromName(TEXT("CapacityText")));
        if (!SelectedItemNameText) SelectedItemNameText=Cast<UTextBlock>(GetWidgetFromName(TEXT("SelectedItemNameText")));
        if (!SelectedItemDetailsText) SelectedItemDetailsText=Cast<UTextBlock>(GetWidgetFromName(TEXT("SelectedItemDetailsText")));
        if (!CloseButton) CloseButton=Cast<UButton>(GetWidgetFromName(TEXT("CloseButton")));
        if (!PrimaryActionButton) PrimaryActionButton=Cast<UButton>(GetWidgetFromName(TEXT("PrimaryActionButton")));
        if (!PrimaryActionText) PrimaryActionText=Cast<UTextBlock>(GetWidgetFromName(TEXT("PrimaryActionText")));
        if (!InventoryTabButton) InventoryTabButton=Cast<UButton>(GetWidgetFromName(TEXT("InventoryTabButton")));
        if (!CraftingTabButton) CraftingTabButton=Cast<UButton>(GetWidgetFromName(TEXT("CraftingTabButton")));
        if (!MainTabSwitcher) MainTabSwitcher=Cast<UWidgetSwitcher>(GetWidgetFromName(TEXT("MainTabSwitcher")));
        if (!CraftingPageHost) CraftingPageHost=Cast<USizeBox>(GetWidgetFromName(TEXT("CraftingPageHost")));
    }
}

void UARPGInventoryPanelWidget::RebuildInventoryGrid()
{
    if (!InventoryUI || !InventoryGrid || !WidgetTree) return;
    InventoryGrid->ClearChildren();
    RuntimeSlots.Reset();

    const int32 Columns = FMath::Clamp(InventoryUI->InventoryColumns, 1, 12);
    const int32 SlotCount = InventoryUI->GetInventoryDisplaySlotCount();
    TSubclassOf<UARPGInventoryItemSlotWidget> SlotClass = InventoryUI->InventorySlotWidgetClass;
    if (!SlotClass) SlotClass = UARPGInventoryItemSlotWidget::StaticClass();

    for (int32 SlotNumber = 1; SlotNumber <= SlotCount; ++SlotNumber)
    {
        USizeBox* SlotSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), FName(*FString::Printf(TEXT("InventorySlotSize_%d"), SlotNumber)));
        SlotSize->SetWidthOverride(FMath::Clamp(InventoryUI->InventorySlotSize, 48.f, 160.f));
        SlotSize->SetHeightOverride(FMath::Clamp(InventoryUI->InventorySlotSize, 48.f, 160.f));
        SlotSize->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

        UARPGInventoryItemSlotWidget* SlotWidget = WidgetTree->ConstructWidget<UARPGInventoryItemSlotWidget>(SlotClass, FName(*FString::Printf(TEXT("InventorySlot_%d"), SlotNumber)));
        SlotWidget->InitializeInventorySlot(InventoryUI, EARPGInventoryUISlotSource::Inventory, SlotNumber);
        SlotSize->SetContent(SlotWidget);

        const int32 Index = SlotNumber - 1;
        if (UUniformGridSlot* GridSlot = InventoryGrid->AddChildToUniformGrid(SlotSize, Index / Columns, Index % Columns))
            GridSlot->SetHorizontalAlignment(HAlign_Center);
        RuntimeSlots.Add(SlotWidget);
    }
}

void UARPGInventoryPanelWidget::RefreshInventoryUI()
{
    if (!InventoryUI) return;
    EnsureNativeLayoutOrBindings();
    if (RuntimeSlots.Num() != InventoryUI->GetInventoryDisplaySlotCount()) RebuildInventoryGrid();

    int32 UsedSlots = 0;
    for (int32 Index = 0; Index < RuntimeSlots.Num(); ++Index)
    {
        FARPGInventoryUISlotView View;
        if (InventoryUI->GetInventorySlotView(Index + 1, View))
        {
            RuntimeSlots[Index]->SetSlotView(View);
            if (View.bOccupied) ++UsedSlots;
        }
    }

    if (CapacityText)
    {
        CapacityText->SetText(FText::Format(NSLOCTEXT("AkumasRPGFramework", "InventoryCapacity", "Slots: {0} / {1}"),
            FText::AsNumber(UsedSlots), FText::AsNumber(InventoryUI->GetInventoryDisplaySlotCount())));
    }

    if (SelectedSlotView.SlotNumber > 0)
    {
        FARPGInventoryUISlotView RefreshedSelection;
        if (InventoryUI->GetInventorySlotView(SelectedSlotView.SlotNumber, RefreshedSelection)) SelectedSlotView = RefreshedSelection;
    }
    UpdateSelectionText();
    if (CraftingPanel) CraftingPanel->RefreshCraftingProgress();
    ApplyActiveTab();
    BP_OnInventoryUIRefreshed();
}

void UARPGInventoryPanelWidget::SetSelectedSlotView(const FARPGInventoryUISlotView& InView)
{
    SelectedSlotView = InView;
    UpdateSelectionText();
    BP_OnInventorySelectionChanged(SelectedSlotView);
}

void UARPGInventoryPanelWidget::UpdateSelectionText()
{
    if (SelectedItemNameText)
        SelectedItemNameText->SetText(SelectedSlotView.bOccupied ? SelectedSlotView.DisplayName : NSLOCTEXT("AkumasRPGFramework", "InventoryNoSelection", "Select an item"));

    if (PrimaryActionButton)
    {
        const UARPGItemDefinition* Definition = SelectedSlotView.ItemDefinition;
        const bool bCanEquip = SelectedSlotView.bOccupied && Definition && Definition->bEquippable && Definition->EquipmentSlot.IsValid();
        const bool bBrokenEquip = bCanEquip && SelectedSlotView.bUsesDurability && SelectedSlotView.bBroken;
        const bool bCanUse = SelectedSlotView.bOccupied && Definition && Definition->bUsable;
        const bool bPreferUse = bCanUse && Definition->QuickAccessAction == EARPGQuickAccessAction::Use;
        const bool bActionIsUse = bCanUse && (bPreferUse || !bCanEquip);
        PrimaryActionButton->SetVisibility((bCanEquip || bCanUse) ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
        const bool bUseAvailableNow = !bActionIsUse || (InventoryUI && InventoryUI->CanUseInventoryItemNow(SelectedSlotView.ItemInstanceId));
        PrimaryActionButton->SetIsEnabled(!bBrokenEquip && (!bActionIsUse || (SelectedSlotView.CooldownRemaining <= KINDA_SMALL_NUMBER && bUseAvailableNow)));
        if (PrimaryActionText)
        {
            if (bActionIsUse)
                PrimaryActionText->SetText(SelectedSlotView.CooldownRemaining > KINDA_SMALL_NUMBER
                    ? FText::Format(NSLOCTEXT("AkumasRPGFramework", "InventoryUseCooldownAction", "Use ({0}s)"), FText::AsNumber(FMath::CeilToInt(SelectedSlotView.CooldownRemaining)))
                    : NSLOCTEXT("AkumasRPGFramework", "InventoryUseAction", "Use"));
            else if (bCanEquip)
                PrimaryActionText->SetText(bBrokenEquip ? NSLOCTEXT("AkumasRPGFramework", "InventoryBrokenRepairAction", "Broken - Repair") : (SelectedSlotView.bEquipped ? NSLOCTEXT("AkumasRPGFramework", "InventoryUnequipAction", "Unequip") : NSLOCTEXT("AkumasRPGFramework", "InventoryEquipAction", "Equip")));
        }
    }

    if (SelectedItemDetailsText)
    {
        if (!SelectedSlotView.bOccupied)
        {
            SelectedItemDetailsText->SetText(NSLOCTEXT("AkumasRPGFramework", "InventoryNoSelectionDetail", "Item details will appear here."));
            return;
        }

        const FText DurabilityDetail = SelectedSlotView.bUsesDurability
            ? FText::Format(
                NSLOCTEXT("AkumasRPGFramework", "InventorySelectedDurability", "   Durability: {0} / {1} ({2}%){3}"),
                FText::AsNumber(FMath::RoundToInt(SelectedSlotView.Durability)),
                FText::AsNumber(FMath::RoundToInt(SelectedSlotView.MaxDurability)),
                FText::AsNumber(FMath::RoundToInt(FMath::Clamp(SelectedSlotView.DurabilityPercent, 0.f, 1.f) * 100.f)),
                SelectedSlotView.bBroken ? NSLOCTEXT("AkumasRPGFramework", "InventorySelectedBroken", "   BROKEN") : FText::GetEmpty())
            : FText::GetEmpty();
        SelectedItemDetailsText->SetText(FText::Format(
            NSLOCTEXT("AkumasRPGFramework", "InventorySelectedDetail", "{0}\nRarity: {1}   Quantity: {2}{3}{4}{5}"),
            SelectedSlotView.Description,
            RarityText(SelectedSlotView.Rarity),
            FText::AsNumber(FMath::Max(0, SelectedSlotView.Quantity)),
            DurabilityDetail,
            SelectedSlotView.bBound ? NSLOCTEXT("AkumasRPGFramework", "InventoryBoundDetail", "   Bound") : FText::GetEmpty(),
            SelectedSlotView.bEquipped ? NSLOCTEXT("AkumasRPGFramework", "InventoryEquippedDetail", "   Equipped") : FText::GetEmpty()));
    }
}

void UARPGInventoryPanelWidget::HandlePrimaryActionClicked()
{
    if (!InventoryUI || !SelectedSlotView.bOccupied || !SelectedSlotView.ItemInstanceId.IsValid()) return;
    InventoryUI->ActivateInventoryItem(SelectedSlotView.ItemInstanceId);
    RefreshInventoryUI();
}

void UARPGInventoryPanelWidget::EnsureCraftingPanel()
{
    if (CraftingPanel || !CraftingPageHost || !InventoryUI || !ObservedCharacter || !GetOwningPlayer()) return;
    TSubclassOf<UARPGCraftingPanelWidget> ResolvedClass = InventoryUI->CraftingWidgetClass;
    if (!ResolvedClass) ResolvedClass = UARPGCraftingPanelWidget::StaticClass();
    CraftingPanel = CreateWidget<UARPGCraftingPanelWidget>(GetOwningPlayer(), ResolvedClass);
    if (!CraftingPanel) return;
    CraftingPanel->InitializeCraftingUI(ObservedCharacter, InventoryUI);
    CraftingPageHost->SetContent(CraftingPanel);
}

void UARPGInventoryPanelWidget::SetActiveTab(EARPGItemManagementTab NewTab)
{
    ActiveTab = NewTab;
    EnsureCraftingPanel();
    ApplyActiveTab();
    if (ActiveTab == EARPGItemManagementTab::Crafting) RefreshCraftingUI();
    BP_OnItemManagementTabChanged(ActiveTab);
}

void UARPGInventoryPanelWidget::ApplyActiveTab()
{
    if (MainTabSwitcher) MainTabSwitcher->SetActiveWidgetIndex(ActiveTab == EARPGItemManagementTab::Inventory ? 0 : 1);
    if (InventoryTabButton) InventoryTabButton->SetIsEnabled(ActiveTab != EARPGItemManagementTab::Inventory);
    if (CraftingTabButton) CraftingTabButton->SetIsEnabled(ActiveTab != EARPGItemManagementTab::Crafting);
}

void UARPGInventoryPanelWidget::RefreshCraftingUI()
{
    EnsureCraftingPanel();
    if (CraftingPanel) CraftingPanel->RefreshCraftingUI();
}

void UARPGInventoryPanelWidget::HandleInventoryTabClicked() { SetActiveTab(EARPGItemManagementTab::Inventory); }
void UARPGInventoryPanelWidget::HandleCraftingTabClicked() { SetActiveTab(EARPGItemManagementTab::Crafting); }

void UARPGInventoryPanelWidget::RequestCloseInventoryUI()
{
    if (InventoryUI) InventoryUI->CloseInventoryUI();
    else RemoveFromParent();
}

void UARPGInventoryPanelWidget::HandleCloseClicked()
{
    RequestCloseInventoryUI();
}

bool UARPGInventoryPanelWidget::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
    UARPGInventoryDragDropOperation* Operation = Cast<UARPGInventoryDragDropOperation>(InOperation);
    if (Operation && InventoryUI && !Operation->bDropHandled && Operation->Source == EARPGInventoryUISlotSource::QuickAccess)
    {
        if (InventoryUI->ClearQuickAccessSlot(Operation->SourceSlotNumber, InventoryUI->bUnequipActiveItemWhenDraggedOff))
        {
            Operation->bDropHandled = true;
            return true;
        }
    }
    return Super::NativeOnDrop(InGeometry, InDragDropEvent, InOperation);
}

void UARPGQuickAccessBarWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    // The hotbar is a full-screen viewport widget layered above Inventory. The top-level
    // UUserWidget itself must not be a full-screen hit target, otherwise it blocks every
    // lower-Z Inventory control. SelfHitTestInvisible keeps the hotbar's child slots fully
    // interactive while allowing pointer hits to pass through everywhere else.
    SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    EnsureNativeLayoutOrBindings();
}

void UARPGQuickAccessBarWidget::InitializeQuickAccessUI(AARPGCharacter* InCharacter, UARPGInventoryUIComponent* InInventoryUIComponent)
{
    ObservedCharacter = InCharacter;
    InventoryUI = InInventoryUIComponent;
    EnsureNativeLayoutOrBindings();
    RebuildQuickAccessSlots();
    RefreshQuickAccessUI();
}

void UARPGQuickAccessBarWidget::EnsureNativeLayoutOrBindings()
{
    if (!WidgetTree) return;
    if (!WidgetTree->RootWidget)
    {
        UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("QuickAccessRoot"));
        Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
        WidgetTree->RootWidget = Root;

        UBorder* BarBackground = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("QuickAccessBackground"));
        BarBackground->SetPadding(FMargin(7.f));
        BarBackground->SetBrushColor(FLinearColor(0.008f, 0.010f, 0.016f, 0.92f));
        UCanvasPanelSlot* CanvasSlot = Root->AddChildToCanvas(BarBackground);
        if (CanvasSlot)
        {
            CanvasSlot->SetAnchors(FAnchors(0.5f, 1.f));
            CanvasSlot->SetAlignment(FVector2D(0.5f, 1.f));
            CanvasSlot->SetPosition(FVector2D(0.f, -24.f));
            CanvasSlot->SetAutoSize(true);
        }

        QuickAccessBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("QuickAccessBox"));
        BarBackground->SetContent(QuickAccessBox);
    }
    else
    {
        if (!QuickAccessBox) QuickAccessBox = Cast<UHorizontalBox>(GetWidgetFromName(TEXT("QuickAccessBox")));
    }
}

void UARPGQuickAccessBarWidget::RebuildQuickAccessSlots()
{
    if (!InventoryUI || !QuickAccessBox || !WidgetTree) return;
    QuickAccessBox->ClearChildren();
    RuntimeSlots.Reset();

    const int32 SlotCount = InventoryUI->GetQuickAccessDisplaySlotCount();
    TSubclassOf<UARPGInventoryItemSlotWidget> SlotClass = InventoryUI->QuickAccessSlotWidgetClass;
    if (!SlotClass) SlotClass = UARPGInventoryItemSlotWidget::StaticClass();

    for (int32 SlotNumber = 1; SlotNumber <= SlotCount; ++SlotNumber)
    {
        USizeBox* SlotSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), FName(*FString::Printf(TEXT("QuickAccessSlotSize_%d"), SlotNumber)));
        const float Size = FMath::Clamp(InventoryUI->QuickAccessSlotSize, 48.f, 160.f);
        SlotSize->SetWidthOverride(Size);
        SlotSize->SetHeightOverride(Size);

        UARPGInventoryItemSlotWidget* SlotWidget = WidgetTree->ConstructWidget<UARPGInventoryItemSlotWidget>(SlotClass, FName(*FString::Printf(TEXT("QuickAccessSlot_%d"), SlotNumber)));
        SlotWidget->InitializeInventorySlot(InventoryUI, EARPGInventoryUISlotSource::QuickAccess, SlotNumber);
        SlotSize->SetContent(SlotWidget);
        if (UHorizontalBoxSlot* BoxSlot = QuickAccessBox->AddChildToHorizontalBox(SlotSize)) BoxSlot->SetPadding(FMargin(3.f));
        RuntimeSlots.Add(SlotWidget);
    }
}

void UARPGQuickAccessBarWidget::RefreshQuickAccessUI()
{
    if (!InventoryUI) return;
    EnsureNativeLayoutOrBindings();
    if (RuntimeSlots.Num() != InventoryUI->GetQuickAccessDisplaySlotCount()) RebuildQuickAccessSlots();

    for (int32 Index = 0; Index < RuntimeSlots.Num(); ++Index)
    {
        FARPGInventoryUISlotView View;
        if (InventoryUI->GetQuickAccessSlotView(Index + 1, View)) RuntimeSlots[Index]->SetSlotView(View);
    }
    BP_OnQuickAccessUIRefreshed();
}
