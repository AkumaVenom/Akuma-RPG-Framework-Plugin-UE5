#include "UI/ARPGCharacterInfoWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

void UARPGCharacterInfoWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    EnsureNativeLayoutOrBindings();
    ApplySnapshotToStandardFields();
}

void UARPGCharacterInfoWidget::SetCharacterInfo(const FARPGCharacterInfoSnapshot& InSnapshot)
{
    CharacterInfo = InSnapshot;
    EnsureNativeLayoutOrBindings();
    ApplySnapshotToStandardFields();
    BP_OnCharacterInfoUpdated(CharacterInfo);
}

void UARPGCharacterInfoWidget::EnsureNativeLayoutOrBindings()
{
    if (!WidgetTree) return;

    if (!WidgetTree->RootWidget)
    {
        USizeBox* Root = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("NPCInfoRoot"));
        Root->SetWidthOverride(220.f);
        Root->SetHeightOverride(78.f);
        WidgetTree->RootWidget = Root;

        UBorder* Background = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("NPCInfoBackground"));
        Background->SetPadding(FMargin(9.f, 6.f));
        Background->SetBrushColor(FLinearColor(0.012f, 0.012f, 0.018f, 0.78f));
        Root->SetContent(Background);

        UVerticalBox* Stack = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("NPCInfoStack"));
        Background->SetContent(Stack);

        CharacterNameText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("CharacterNameText"));
        CharacterNameText->SetJustification(ETextJustify::Center);
        FSlateFontInfo NameFont = CharacterNameText->GetFont();
        NameFont.Size = 17;
        CharacterNameText->SetFont(NameFont);
        CharacterNameText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
        if (UVerticalBoxSlot* NameSlot = Stack->AddChildToVerticalBox(CharacterNameText))
        {
            NameSlot->SetHorizontalAlignment(HAlign_Fill);
            NameSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 1.f));
        }

        LevelText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("LevelText"));
        LevelText->SetJustification(ETextJustify::Center);
        FSlateFontInfo LevelFont = LevelText->GetFont();
        LevelFont.Size = 12;
        LevelText->SetFont(LevelFont);
        LevelText->SetColorAndOpacity(FSlateColor(FLinearColor(0.82f, 0.82f, 0.86f, 1.f)));
        if (UVerticalBoxSlot* LevelSlot = Stack->AddChildToVerticalBox(LevelText))
        {
            LevelSlot->SetHorizontalAlignment(HAlign_Fill);
            LevelSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 3.f));
        }

        UOverlay* HealthOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("HealthOverlay"));
        if (UVerticalBoxSlot* HealthSlot = Stack->AddChildToVerticalBox(HealthOverlay))
        {
            HealthSlot->SetHorizontalAlignment(HAlign_Fill);
            HealthSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 0.f));
        }

        HealthBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("HealthBar"));
        HealthBar->SetPercent(1.f);
        HealthBar->SetFillColorAndOpacity(FLinearColor(0.12f, 0.72f, 0.22f, 1.f));
        if (UOverlaySlot* BarSlot = HealthOverlay->AddChildToOverlay(HealthBar))
        {
            BarSlot->SetHorizontalAlignment(HAlign_Fill);
            BarSlot->SetVerticalAlignment(VAlign_Fill);
        }

        HealthText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("HealthText"));
        HealthText->SetJustification(ETextJustify::Center);
        FSlateFontInfo HealthFont = HealthText->GetFont();
        HealthFont.Size = 11;
        HealthText->SetFont(HealthFont);
        HealthText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
        if (UOverlaySlot* TextSlot = HealthOverlay->AddChildToOverlay(HealthText))
        {
            TextSlot->SetHorizontalAlignment(HAlign_Fill);
            TextSlot->SetVerticalAlignment(VAlign_Center);
        }
    }
    else
    {
        if (!CharacterNameText) CharacterNameText = Cast<UTextBlock>(GetWidgetFromName(TEXT("CharacterNameText")));
        if (!LevelText) LevelText = Cast<UTextBlock>(GetWidgetFromName(TEXT("LevelText")));
        if (!HealthBar) HealthBar = Cast<UProgressBar>(GetWidgetFromName(TEXT("HealthBar")));
        if (!HealthText) HealthText = Cast<UTextBlock>(GetWidgetFromName(TEXT("HealthText")));
    }
}

void UARPGCharacterInfoWidget::ApplySnapshotToStandardFields()
{
    if (CharacterNameText)
    {
        CharacterNameText->SetText(FText::FromString(CharacterInfo.CharacterName));
    }
    if (LevelText)
    {
        LevelText->SetText(FText::Format(NSLOCTEXT("AkumasRPGFramework", "NPCInfoLevel", "Level {0}"), FText::AsNumber(FMath::Max(1, CharacterInfo.Level))));
    }
    if (HealthBar)
    {
        HealthBar->SetPercent(FMath::Clamp(CharacterInfo.HealthPercent, 0.f, 1.f));
    }
    if (HealthText)
    {
        const int32 CurrentHealth = FMath::Max(0, FMath::RoundToInt(CharacterInfo.Health));
        const int32 MaximumHealth = FMath::Max(0, FMath::RoundToInt(CharacterInfo.MaxHealth));
        HealthText->SetText(FText::Format(NSLOCTEXT("AkumasRPGFramework", "NPCInfoHealth", "{0} / {1}"), FText::AsNumber(CurrentHealth), FText::AsNumber(MaximumHealth)));
    }
}
