#include "UI/ARPGStatsPanelWidget.h"

#include "Actors/ARPGCharacter.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/ARPGStatsComponent.h"
#include "Components/ARPGStatsUIComponent.h"

namespace
{
    FText FormatNumber(float Value, int32 MaxFractionalDigits = 1)
    {
        FNumberFormattingOptions Options;
        Options.MinimumFractionalDigits = 0;
        Options.MaximumFractionalDigits = FMath::Max(0, MaxFractionalDigits);
        return FText::AsNumber(Value, &Options);
    }

    FText FormatVital(float Current, float Maximum)
    {
        return FText::Format(
            NSLOCTEXT("AkumasRPGFramework", "StatsUIVital", "{0} / {1}"),
            FormatNumber(FMath::Max(0.f, Current), 0),
            FormatNumber(FMath::Max(0.f, Maximum), 0));
    }

    FText FormatPercentPoints(float Value)
    {
        return FText::FromString(FString::Printf(TEXT("%.1f%%"), Value));
    }

    FText FormatPercent01(float Value)
    {
        return FText::FromString(FString::Printf(TEXT("%.1f%%"), FMath::Clamp(Value, 0.f, 1.f) * 100.f));
    }

    FText FormatMultiplier(float Value)
    {
        return FText::FromString(FString::Printf(TEXT("x%.2f"), Value));
    }

    FText FormatAllocation(int32 Value)
    {
        return FText::Format(NSLOCTEXT("AkumasRPGFramework", "StatsUIAllocated", "+{0} allocated"), FText::AsNumber(FMath::Max(0, Value)));
    }

    void StyleText(UTextBlock* Text, int32 FontSize, const FLinearColor& Color = FLinearColor::White)
    {
        if (!Text) return;
        FSlateFontInfo Font = Text->GetFont();
        Font.Size = FontSize;
        Text->SetFont(Font);
        Text->SetColorAndOpacity(FSlateColor(Color));
    }
}

void UARPGStatsPanelWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    EnsureNativeLayoutOrBindings();
    BindStandardButtons();
    ApplySnapshotToStandardFields();
}

void UARPGStatsPanelWidget::InitializeStatsUI(AARPGCharacter* InCharacter, UARPGStatsUIComponent* InStatsUIComponent)
{
    ObservedCharacter = InCharacter;
    OwningStatsUIComponent = InStatsUIComponent;
    EnsureNativeLayoutOrBindings();
    BindStandardButtons();
    RefreshStatsUI();
}

void UARPGStatsPanelWidget::SetStatsUISnapshot(const FARPGStatsUISnapshot& InSnapshot)
{
    StatsUISnapshot = InSnapshot;
    if (InSnapshot.Character) ObservedCharacter = InSnapshot.Character;
    EnsureNativeLayoutOrBindings();
    BindStandardButtons();
    ApplySnapshotToStandardFields();
    BP_OnStatsUIUpdated(StatsUISnapshot);
}

void UARPGStatsPanelWidget::RefreshStatsUI()
{
    if (OwningStatsUIComponent)
    {
        SetStatsUISnapshot(OwningStatsUIComponent->GetStatsUISnapshot());
    }
}

void UARPGStatsPanelWidget::RequestCloseStatsUI()
{
    if (OwningStatsUIComponent)
    {
        OwningStatsUIComponent->CloseStatsUI();
    }
    else
    {
        RemoveFromParent();
    }
}

void UARPGStatsPanelWidget::EnsureNativeLayoutOrBindings()
{
    if (!WidgetTree) return;

    if (!WidgetTree->RootWidget)
    {
        UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("StatsUIRoot"));
        WidgetTree->RootWidget = Root;

        UBorder* ScreenDim = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("StatsUIScreenDim"));
        ScreenDim->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.48f));
        if (UOverlaySlot* DimSlot = Root->AddChildToOverlay(ScreenDim))
        {
            DimSlot->SetHorizontalAlignment(HAlign_Fill);
            DimSlot->SetVerticalAlignment(VAlign_Fill);
        }

        USizeBox* PanelSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("StatsUIPanelSize"));
        PanelSize->SetWidthOverride(860.f);
        PanelSize->SetHeightOverride(720.f);
        if (UOverlaySlot* PanelSlot = Root->AddChildToOverlay(PanelSize))
        {
            PanelSlot->SetHorizontalAlignment(HAlign_Center);
            PanelSlot->SetVerticalAlignment(VAlign_Center);
            PanelSlot->SetPadding(FMargin(24.f));
        }

        UBorder* PanelBackground = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("StatsUIPanelBackground"));
        PanelBackground->SetPadding(FMargin(18.f));
        PanelBackground->SetBrushColor(FLinearColor(0.012f, 0.015f, 0.024f, 0.96f));
        PanelSize->SetContent(PanelBackground);

        UVerticalBox* MainStack = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("StatsUIMainStack"));
        PanelBackground->SetContent(MainStack);

        UHorizontalBox* Header = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("StatsUIHeader"));
        if (UVerticalBoxSlot* HeaderSlot = MainStack->AddChildToVerticalBox(Header))
        {
            HeaderSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));
        }

        USizeBox* NameBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("StatsUINameBox"));
        NameBox->SetWidthOverride(470.f);
        CharacterNameText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("CharacterNameText"));
        StyleText(CharacterNameText, 24, FLinearColor(0.95f, 0.78f, 0.28f, 1.f));
        NameBox->SetContent(CharacterNameText);
        Header->AddChildToHorizontalBox(NameBox);

        USizeBox* LevelBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("StatsUILevelBox"));
        LevelBox->SetWidthOverride(250.f);
        LevelText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("LevelText"));
        StyleText(LevelText, 18, FLinearColor(0.86f, 0.86f, 0.90f, 1.f));
        LevelText->SetJustification(ETextJustify::Right);
        LevelBox->SetContent(LevelText);
        Header->AddChildToHorizontalBox(LevelBox);

        USizeBox* CloseBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("StatsUICloseBox"));
        CloseBox->SetWidthOverride(92.f);
        CloseBox->SetHeightOverride(34.f);
        CloseButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("CloseButton"));
        UTextBlock* CloseLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("CloseButtonText"));
        CloseLabel->SetText(NSLOCTEXT("AkumasRPGFramework", "StatsUIClose", "Close"));
        CloseLabel->SetJustification(ETextJustify::Center);
        StyleText(CloseLabel, 13);
        CloseButton->AddChild(CloseLabel);
        CloseBox->SetContent(CloseButton);
        Header->AddChildToHorizontalBox(CloseBox);

        SystemStateText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("SystemStateText"));
        StyleText(SystemStateText, 12, FLinearColor(0.70f, 0.74f, 0.82f, 1.f));
        if (UVerticalBoxSlot* StateSlot = MainStack->AddChildToVerticalBox(SystemStateText))
        {
            StateSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));
        }

        UHorizontalBox* XPRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("XPRow"));
        if (UVerticalBoxSlot* XPSlot = MainStack->AddChildToVerticalBox(XPRow))
        {
            XPSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 10.f));
        }
        USizeBox* XPTextBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("XPTextBox"));
        XPTextBox->SetWidthOverride(250.f);
        XPText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("XPText"));
        StyleText(XPText, 13);
        XPTextBox->SetContent(XPText);
        XPRow->AddChildToHorizontalBox(XPTextBox);
        USizeBox* XPBarBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("XPBarBox"));
        XPBarBox->SetWidthOverride(530.f);
        XPBarBox->SetHeightOverride(18.f);
        XPBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("XPBar"));
        XPBar->SetFillColorAndOpacity(FLinearColor(0.82f, 0.61f, 0.10f, 1.f));
        XPBarBox->SetContent(XPBar);
        XPRow->AddChildToHorizontalBox(XPBarBox);

        auto AddVitalRow = [&](const TCHAR* RowName, const TCHAR* Label, const TCHAR* TextName, TObjectPtr<UTextBlock>& TextRef,
                               const TCHAR* BarName, TObjectPtr<UProgressBar>& BarRef, const FLinearColor& FillColor)
        {
            UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), FName(RowName));
            if (UVerticalBoxSlot* RowSlot = MainStack->AddChildToVerticalBox(Row)) RowSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 4.f));

            USizeBox* LabelBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
            LabelBox->SetWidthOverride(90.f);
            UTextBlock* LabelText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
            LabelText->SetText(FText::FromString(FString(Label)));
            StyleText(LabelText, 13);
            LabelBox->SetContent(LabelText);
            Row->AddChildToHorizontalBox(LabelBox);

            USizeBox* ValueBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
            ValueBox->SetWidthOverride(180.f);
            TextRef = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), FName(TextName));
            StyleText(TextRef, 13);
            ValueBox->SetContent(TextRef);
            Row->AddChildToHorizontalBox(ValueBox);

            USizeBox* BarBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
            BarBox->SetWidthOverride(510.f);
            BarBox->SetHeightOverride(17.f);
            BarRef = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), FName(BarName));
            BarRef->SetFillColorAndOpacity(FillColor);
            BarBox->SetContent(BarRef);
            Row->AddChildToHorizontalBox(BarBox);
        };

        AddVitalRow(TEXT("HealthRow"), TEXT("Health"), TEXT("HealthText"), HealthText, TEXT("HealthBar"), HealthBar, FLinearColor(0.72f, 0.08f, 0.08f, 1.f));
        AddVitalRow(TEXT("ManaRow"), TEXT("Mana"), TEXT("ManaText"), ManaText, TEXT("ManaBar"), ManaBar, FLinearColor(0.08f, 0.30f, 0.84f, 1.f));
        AddVitalRow(TEXT("StaminaRow"), TEXT("Stamina"), TEXT("StaminaText"), StaminaText, TEXT("StaminaBar"), StaminaBar, FLinearColor(0.16f, 0.68f, 0.18f, 1.f));

        AttributePointsText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("AttributePointsText"));
        StyleText(AttributePointsText, 16, FLinearColor(0.94f, 0.77f, 0.30f, 1.f));
        if (UVerticalBoxSlot* PointSlot = MainStack->AddChildToVerticalBox(AttributePointsText))
        {
            PointSlot->SetPadding(FMargin(0.f, 10.f, 0.f, 7.f));
        }

        UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("StatsUIScroll"));
        if (UVerticalBoxSlot* ScrollSlot = MainStack->AddChildToVerticalBox(Scroll))
        {
            FSlateChildSize FillSize;
            FillSize.SizeRule = ESlateSizeRule::Fill;
            ScrollSlot->SetSize(FillSize);
        }
        UVerticalBox* Body = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("StatsUIBody"));
        Scroll->AddChild(Body);

        auto AddSectionHeader = [&](const TCHAR* Name, const TCHAR* Label)
        {
            UTextBlock* HeaderText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), FName(Name));
            HeaderText->SetText(FText::FromString(FString(Label)));
            StyleText(HeaderText, 18, FLinearColor(0.80f, 0.84f, 0.92f, 1.f));
            if (UVerticalBoxSlot* Slot = Body->AddChildToVerticalBox(HeaderText)) Slot->SetPadding(FMargin(0.f, 8.f, 0.f, 5.f));
        };

        auto AddPrimaryRow = [&](const TCHAR* RowName, const TCHAR* Label, const TCHAR* ValueName, TObjectPtr<UTextBlock>& ValueRef,
                                 const TCHAR* AllocationName, TObjectPtr<UTextBlock>& AllocationRef,
                                 const TCHAR* ButtonName, TObjectPtr<UButton>& ButtonRef)
        {
            UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), FName(RowName));
            if (UVerticalBoxSlot* Slot = Body->AddChildToVerticalBox(Row)) Slot->SetPadding(FMargin(0.f, 1.f));

            USizeBox* LabelBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
            LabelBox->SetWidthOverride(250.f);
            UTextBlock* LabelText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
            LabelText->SetText(FText::FromString(FString(Label)));
            StyleText(LabelText, 14);
            LabelBox->SetContent(LabelText);
            Row->AddChildToHorizontalBox(LabelBox);

            USizeBox* ValueBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
            ValueBox->SetWidthOverride(130.f);
            ValueRef = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), FName(ValueName));
            StyleText(ValueRef, 14, FLinearColor(0.95f, 0.82f, 0.42f, 1.f));
            ValueBox->SetContent(ValueRef);
            Row->AddChildToHorizontalBox(ValueBox);

            USizeBox* AllocationBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
            AllocationBox->SetWidthOverride(220.f);
            AllocationRef = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), FName(AllocationName));
            StyleText(AllocationRef, 12, FLinearColor(0.67f, 0.70f, 0.76f, 1.f));
            AllocationBox->SetContent(AllocationRef);
            Row->AddChildToHorizontalBox(AllocationBox);

            USizeBox* ButtonBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
            ButtonBox->SetWidthOverride(48.f);
            ButtonBox->SetHeightOverride(28.f);
            ButtonRef = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), FName(ButtonName));
            UTextBlock* PlusLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
            PlusLabel->SetText(FText::FromString(TEXT("+")));
            PlusLabel->SetJustification(ETextJustify::Center);
            StyleText(PlusLabel, 16);
            ButtonRef->AddChild(PlusLabel);
            ButtonBox->SetContent(ButtonRef);
            Row->AddChildToHorizontalBox(ButtonBox);
        };

        AddSectionHeader(TEXT("PrimaryStatsHeader"), TEXT("Primary Stats"));
        AddPrimaryRow(TEXT("StrengthRow"), TEXT("Strength"), TEXT("StrengthText"), StrengthText, TEXT("StrengthAllocatedText"), StrengthAllocatedText, TEXT("StrengthPlusButton"), StrengthPlusButton);
        AddPrimaryRow(TEXT("VitalityRow"), TEXT("Vitality"), TEXT("VitalityText"), VitalityText, TEXT("VitalityAllocatedText"), VitalityAllocatedText, TEXT("VitalityPlusButton"), VitalityPlusButton);
        AddPrimaryRow(TEXT("MagicRow"), TEXT("Magic"), TEXT("MagicText"), MagicText, TEXT("MagicAllocatedText"), MagicAllocatedText, TEXT("MagicPlusButton"), MagicPlusButton);
        AddPrimaryRow(TEXT("SpiritRow"), TEXT("Spirit"), TEXT("SpiritText"), SpiritText, TEXT("SpiritAllocatedText"), SpiritAllocatedText, TEXT("SpiritPlusButton"), SpiritPlusButton);
        AddPrimaryRow(TEXT("DexterityRow"), TEXT("Dexterity / Speed"), TEXT("DexterityText"), DexterityText, TEXT("DexterityAllocatedText"), DexterityAllocatedText, TEXT("DexterityPlusButton"), DexterityPlusButton);
        AddPrimaryRow(TEXT("LuckRow"), TEXT("Luck"), TEXT("LuckText"), LuckText, TEXT("LuckAllocatedText"), LuckAllocatedText, TEXT("LuckPlusButton"), LuckPlusButton);

        auto AddDerivedRow = [&](const TCHAR* RowName, const TCHAR* Label, const TCHAR* ValueName, TObjectPtr<UTextBlock>& ValueRef)
        {
            UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), FName(RowName));
            if (UVerticalBoxSlot* Slot = Body->AddChildToVerticalBox(Row)) Slot->SetPadding(FMargin(0.f, 1.f));

            USizeBox* LabelBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
            LabelBox->SetWidthOverride(420.f);
            UTextBlock* LabelText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
            LabelText->SetText(FText::FromString(FString(Label)));
            StyleText(LabelText, 13);
            LabelBox->SetContent(LabelText);
            Row->AddChildToHorizontalBox(LabelBox);

            ValueRef = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), FName(ValueName));
            StyleText(ValueRef, 13, FLinearColor(0.82f, 0.86f, 0.94f, 1.f));
            Row->AddChildToHorizontalBox(ValueRef);
        };

        AddSectionHeader(TEXT("DerivedStatsHeader"), TEXT("Derived Combat Stats"));
        AddDerivedRow(TEXT("MeleeAttackPowerRow"), TEXT("Melee Attack Power"), TEXT("MeleeAttackPowerText"), MeleeAttackPowerText);
        AddDerivedRow(TEXT("RangedAttackPowerRow"), TEXT("Ranged Attack Power"), TEXT("RangedAttackPowerText"), RangedAttackPowerText);
        AddDerivedRow(TEXT("MagicAttackPowerRow"), TEXT("Magic Attack Power"), TEXT("MagicAttackPowerText"), MagicAttackPowerText);
        AddDerivedRow(TEXT("PhysicalDefenseRow"), TEXT("Physical Defense"), TEXT("PhysicalDefenseText"), PhysicalDefenseText);
        AddDerivedRow(TEXT("MagicDefenseRow"), TEXT("Magic Defense"), TEXT("MagicDefenseText"), MagicDefenseText);
        AddDerivedRow(TEXT("AccuracyRow"), TEXT("Accuracy"), TEXT("AccuracyText"), AccuracyText);
        AddDerivedRow(TEXT("EvasionRow"), TEXT("Physical Evasion"), TEXT("EvasionText"), EvasionText);
        AddDerivedRow(TEXT("MagicEvasionRow"), TEXT("Magic Evasion"), TEXT("MagicEvasionText"), MagicEvasionText);
        AddDerivedRow(TEXT("SpeedRow"), TEXT("Speed"), TEXT("SpeedText"), SpeedText);
        AddDerivedRow(TEXT("CriticalChanceRow"), TEXT("Critical Chance"), TEXT("CriticalChanceText"), CriticalChanceText);
        AddDerivedRow(TEXT("CriticalDamageRow"), TEXT("Critical Damage"), TEXT("CriticalDamageText"), CriticalDamageText);
        AddDerivedRow(TEXT("AttackSpeedRow"), TEXT("Attack Speed"), TEXT("AttackSpeedText"), AttackSpeedText);
        AddDerivedRow(TEXT("MovementSpeedRow"), TEXT("Movement Speed"), TEXT("MovementSpeedText"), MovementSpeedText);
    }
    else
    {
#define ARPG_BIND_TEXT(Member, Name) if (!Member) Member = Cast<UTextBlock>(GetWidgetFromName(TEXT(Name)))
#define ARPG_BIND_BAR(Member, Name) if (!Member) Member = Cast<UProgressBar>(GetWidgetFromName(TEXT(Name)))
#define ARPG_BIND_BUTTON(Member, Name) if (!Member) Member = Cast<UButton>(GetWidgetFromName(TEXT(Name)))
        ARPG_BIND_TEXT(CharacterNameText, "CharacterNameText");
        ARPG_BIND_TEXT(LevelText, "LevelText");
        ARPG_BIND_TEXT(SystemStateText, "SystemStateText");
        ARPG_BIND_TEXT(XPText, "XPText");
        ARPG_BIND_BAR(XPBar, "XPBar");
        ARPG_BIND_TEXT(HealthText, "HealthText");
        ARPG_BIND_BAR(HealthBar, "HealthBar");
        ARPG_BIND_TEXT(ManaText, "ManaText");
        ARPG_BIND_BAR(ManaBar, "ManaBar");
        ARPG_BIND_TEXT(StaminaText, "StaminaText");
        ARPG_BIND_BAR(StaminaBar, "StaminaBar");
        ARPG_BIND_TEXT(AttributePointsText, "AttributePointsText");
        ARPG_BIND_TEXT(StrengthText, "StrengthText");
        ARPG_BIND_TEXT(VitalityText, "VitalityText");
        ARPG_BIND_TEXT(MagicText, "MagicText");
        ARPG_BIND_TEXT(SpiritText, "SpiritText");
        ARPG_BIND_TEXT(DexterityText, "DexterityText");
        ARPG_BIND_TEXT(LuckText, "LuckText");
        ARPG_BIND_TEXT(StrengthAllocatedText, "StrengthAllocatedText");
        ARPG_BIND_TEXT(VitalityAllocatedText, "VitalityAllocatedText");
        ARPG_BIND_TEXT(MagicAllocatedText, "MagicAllocatedText");
        ARPG_BIND_TEXT(SpiritAllocatedText, "SpiritAllocatedText");
        ARPG_BIND_TEXT(DexterityAllocatedText, "DexterityAllocatedText");
        ARPG_BIND_TEXT(LuckAllocatedText, "LuckAllocatedText");
        ARPG_BIND_TEXT(MeleeAttackPowerText, "MeleeAttackPowerText");
        ARPG_BIND_TEXT(RangedAttackPowerText, "RangedAttackPowerText");
        ARPG_BIND_TEXT(MagicAttackPowerText, "MagicAttackPowerText");
        ARPG_BIND_TEXT(PhysicalDefenseText, "PhysicalDefenseText");
        ARPG_BIND_TEXT(MagicDefenseText, "MagicDefenseText");
        ARPG_BIND_TEXT(AccuracyText, "AccuracyText");
        ARPG_BIND_TEXT(EvasionText, "EvasionText");
        ARPG_BIND_TEXT(MagicEvasionText, "MagicEvasionText");
        ARPG_BIND_TEXT(SpeedText, "SpeedText");
        ARPG_BIND_TEXT(CriticalChanceText, "CriticalChanceText");
        ARPG_BIND_TEXT(CriticalDamageText, "CriticalDamageText");
        ARPG_BIND_TEXT(AttackSpeedText, "AttackSpeedText");
        ARPG_BIND_TEXT(MovementSpeedText, "MovementSpeedText");
        ARPG_BIND_BUTTON(CloseButton, "CloseButton");
        ARPG_BIND_BUTTON(StrengthPlusButton, "StrengthPlusButton");
        ARPG_BIND_BUTTON(VitalityPlusButton, "VitalityPlusButton");
        ARPG_BIND_BUTTON(MagicPlusButton, "MagicPlusButton");
        ARPG_BIND_BUTTON(SpiritPlusButton, "SpiritPlusButton");
        ARPG_BIND_BUTTON(DexterityPlusButton, "DexterityPlusButton");
        ARPG_BIND_BUTTON(LuckPlusButton, "LuckPlusButton");
#undef ARPG_BIND_TEXT
#undef ARPG_BIND_BAR
#undef ARPG_BIND_BUTTON
    }
}

void UARPGStatsPanelWidget::BindStandardButtons()
{
    if (CloseButton) CloseButton->OnClicked.AddUniqueDynamic(this, &UARPGStatsPanelWidget::HandleCloseClicked);
    if (StrengthPlusButton) StrengthPlusButton->OnClicked.AddUniqueDynamic(this, &UARPGStatsPanelWidget::HandleStrengthPlusClicked);
    if (VitalityPlusButton) VitalityPlusButton->OnClicked.AddUniqueDynamic(this, &UARPGStatsPanelWidget::HandleVitalityPlusClicked);
    if (MagicPlusButton) MagicPlusButton->OnClicked.AddUniqueDynamic(this, &UARPGStatsPanelWidget::HandleMagicPlusClicked);
    if (SpiritPlusButton) SpiritPlusButton->OnClicked.AddUniqueDynamic(this, &UARPGStatsPanelWidget::HandleSpiritPlusClicked);
    if (DexterityPlusButton) DexterityPlusButton->OnClicked.AddUniqueDynamic(this, &UARPGStatsPanelWidget::HandleDexterityPlusClicked);
    if (LuckPlusButton) LuckPlusButton->OnClicked.AddUniqueDynamic(this, &UARPGStatsPanelWidget::HandleLuckPlusClicked);
}

void UARPGStatsPanelWidget::ApplySnapshotToStandardFields()
{
    if (CharacterNameText) CharacterNameText->SetText(FText::FromString(StatsUISnapshot.CharacterName));
    if (LevelText) LevelText->SetText(FText::Format(NSLOCTEXT("AkumasRPGFramework", "StatsUILevel", "Level {0}"), FText::AsNumber(FMath::Max(1, StatsUISnapshot.Level))));
    if (SystemStateText)
    {
        SystemStateText->SetText(StatsUISnapshot.bJRPGStatSystemEnabled
            ? NSLOCTEXT("AkumasRPGFramework", "StatsUIJRPGEnabled", "JRPG Stat System: Enabled")
            : NSLOCTEXT("AkumasRPGFramework", "StatsUIJRPGDisabled", "JRPG Stat System: Disabled (legacy combat values are shown where applicable)"));
    }

    if (XPText)
    {
        XPText->SetText(StatsUISnapshot.bAtMaxLevel
            ? NSLOCTEXT("AkumasRPGFramework", "StatsUIMaxLevel", "XP: MAX LEVEL")
            : FText::Format(NSLOCTEXT("AkumasRPGFramework", "StatsUIXP", "XP: {0} / {1}"), FText::AsNumber(StatsUISnapshot.CurrentXP), FText::AsNumber(StatsUISnapshot.XPRequiredForNextLevel)));
    }
    if (XPBar) XPBar->SetPercent(FMath::Clamp(StatsUISnapshot.XPPercent, 0.f, 1.f));

    if (HealthText) HealthText->SetText(FormatVital(StatsUISnapshot.Health, StatsUISnapshot.MaxHealth));
    if (HealthBar) HealthBar->SetPercent(FMath::Clamp(StatsUISnapshot.HealthPercent, 0.f, 1.f));
    if (ManaText) ManaText->SetText(FormatVital(StatsUISnapshot.Mana, StatsUISnapshot.MaxMana));
    if (ManaBar) ManaBar->SetPercent(FMath::Clamp(StatsUISnapshot.ManaPercent, 0.f, 1.f));
    if (StaminaText) StaminaText->SetText(FormatVital(StatsUISnapshot.Stamina, StatsUISnapshot.MaxStamina));
    if (StaminaBar) StaminaBar->SetPercent(FMath::Clamp(StatsUISnapshot.StaminaPercent, 0.f, 1.f));

    if (AttributePointsText)
    {
        AttributePointsText->SetText(FText::Format(
            NSLOCTEXT("AkumasRPGFramework", "StatsUIPoints", "Attribute Points: {0} available   |   {1} earned"),
            FText::AsNumber(FMath::Max(0, StatsUISnapshot.UnspentAttributePoints)),
            FText::AsNumber(FMath::Max(0, StatsUISnapshot.TotalAttributePointsEarned))));
    }

    if (StrengthText) StrengthText->SetText(FormatNumber(StatsUISnapshot.PrimaryStats.Strength));
    if (VitalityText) VitalityText->SetText(FormatNumber(StatsUISnapshot.PrimaryStats.Vitality));
    if (MagicText) MagicText->SetText(FormatNumber(StatsUISnapshot.PrimaryStats.Magic));
    if (SpiritText) SpiritText->SetText(FormatNumber(StatsUISnapshot.PrimaryStats.Spirit));
    if (DexterityText) DexterityText->SetText(FormatNumber(StatsUISnapshot.PrimaryStats.Dexterity));
    if (LuckText) LuckText->SetText(FormatNumber(StatsUISnapshot.PrimaryStats.Luck));

    if (StrengthAllocatedText) StrengthAllocatedText->SetText(FormatAllocation(StatsUISnapshot.AllocatedPoints.Strength));
    if (VitalityAllocatedText) VitalityAllocatedText->SetText(FormatAllocation(StatsUISnapshot.AllocatedPoints.Vitality));
    if (MagicAllocatedText) MagicAllocatedText->SetText(FormatAllocation(StatsUISnapshot.AllocatedPoints.Magic));
    if (SpiritAllocatedText) SpiritAllocatedText->SetText(FormatAllocation(StatsUISnapshot.AllocatedPoints.Spirit));
    if (DexterityAllocatedText) DexterityAllocatedText->SetText(FormatAllocation(StatsUISnapshot.AllocatedPoints.Dexterity));
    if (LuckAllocatedText) LuckAllocatedText->SetText(FormatAllocation(StatsUISnapshot.AllocatedPoints.Luck));

    if (MeleeAttackPowerText) MeleeAttackPowerText->SetText(FormatNumber(StatsUISnapshot.DerivedStats.MeleeAttackPower));
    if (RangedAttackPowerText) RangedAttackPowerText->SetText(FormatNumber(StatsUISnapshot.DerivedStats.RangedAttackPower));
    if (MagicAttackPowerText) MagicAttackPowerText->SetText(FormatNumber(StatsUISnapshot.DerivedStats.MagicAttackPower));
    if (PhysicalDefenseText) PhysicalDefenseText->SetText(FormatNumber(StatsUISnapshot.DerivedStats.PhysicalDefense));
    if (MagicDefenseText) MagicDefenseText->SetText(FormatNumber(StatsUISnapshot.DerivedStats.MagicDefense));
    if (AccuracyText) AccuracyText->SetText(FormatPercentPoints(StatsUISnapshot.DerivedStats.Accuracy));
    if (EvasionText) EvasionText->SetText(FormatPercentPoints(StatsUISnapshot.DerivedStats.Evasion));
    if (MagicEvasionText) MagicEvasionText->SetText(FormatPercentPoints(StatsUISnapshot.DerivedStats.MagicEvasion));
    if (SpeedText) SpeedText->SetText(FormatNumber(StatsUISnapshot.DerivedStats.Speed));
    if (CriticalChanceText) CriticalChanceText->SetText(FormatPercent01(StatsUISnapshot.DerivedStats.CriticalChance));
    if (CriticalDamageText) CriticalDamageText->SetText(FormatMultiplier(StatsUISnapshot.DerivedStats.CriticalDamageMultiplier));
    if (AttackSpeedText) AttackSpeedText->SetText(FormatMultiplier(StatsUISnapshot.DerivedStats.AttackSpeedMultiplier));
    if (MovementSpeedText) MovementSpeedText->SetText(FormatMultiplier(StatsUISnapshot.DerivedStats.MovementSpeedMultiplier));

    const bool bCanSpend = StatsUISnapshot.bJRPGStatSystemEnabled && StatsUISnapshot.UnspentAttributePoints > 0;
    UARPGStatsComponent* Stats = ObservedCharacter ? ObservedCharacter->Stats : nullptr;
    if (StrengthPlusButton) StrengthPlusButton->SetIsEnabled(bCanSpend && Stats && Stats->CanSpendAttributePoints(EARPGPrimaryStat::Strength, 1));
    if (VitalityPlusButton) VitalityPlusButton->SetIsEnabled(bCanSpend && Stats && Stats->CanSpendAttributePoints(EARPGPrimaryStat::Vitality, 1));
    if (MagicPlusButton) MagicPlusButton->SetIsEnabled(bCanSpend && Stats && Stats->CanSpendAttributePoints(EARPGPrimaryStat::Magic, 1));
    if (SpiritPlusButton) SpiritPlusButton->SetIsEnabled(bCanSpend && Stats && Stats->CanSpendAttributePoints(EARPGPrimaryStat::Spirit, 1));
    if (DexterityPlusButton) DexterityPlusButton->SetIsEnabled(bCanSpend && Stats && Stats->CanSpendAttributePoints(EARPGPrimaryStat::Dexterity, 1));
    if (LuckPlusButton) LuckPlusButton->SetIsEnabled(bCanSpend && Stats && Stats->CanSpendAttributePoints(EARPGPrimaryStat::Luck, 1));
}

void UARPGStatsPanelWidget::SpendPoint(EARPGPrimaryStat Stat)
{
    if (ObservedCharacter && ObservedCharacter->Stats && ObservedCharacter->Stats->SpendAttributePoints(Stat, 1))
    {
        RefreshStatsUI();
    }
}

void UARPGStatsPanelWidget::HandleCloseClicked() { RequestCloseStatsUI(); }
void UARPGStatsPanelWidget::HandleStrengthPlusClicked() { SpendPoint(EARPGPrimaryStat::Strength); }
void UARPGStatsPanelWidget::HandleVitalityPlusClicked() { SpendPoint(EARPGPrimaryStat::Vitality); }
void UARPGStatsPanelWidget::HandleMagicPlusClicked() { SpendPoint(EARPGPrimaryStat::Magic); }
void UARPGStatsPanelWidget::HandleSpiritPlusClicked() { SpendPoint(EARPGPrimaryStat::Spirit); }
void UARPGStatsPanelWidget::HandleDexterityPlusClicked() { SpendPoint(EARPGPrimaryStat::Dexterity); }
void UARPGStatsPanelWidget::HandleLuckPlusClicked() { SpendPoint(EARPGPrimaryStat::Luck); }
