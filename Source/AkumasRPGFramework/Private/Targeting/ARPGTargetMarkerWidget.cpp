#include "Targeting/ARPGTargetMarkerWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/TextBlock.h"
#include "Materials/MaterialInterface.h"
#include "Engine/Texture2D.h"

void UARPGTargetMarkerWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    EnsureNativeLayout();
    ApplyMarkerArt();
}

void UARPGTargetMarkerWidget::EnsureNativeLayout()
{
    if (!WidgetTree) return;

    if (!WidgetTree->RootWidget)
    {
        UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("TargetMarkerRoot"));
        WidgetTree->RootWidget = Root;

        MarkerImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("TargetMarkerImage"));
        if (UOverlaySlot* ImageSlot = Root->AddChildToOverlay(MarkerImage))
        {
            ImageSlot->SetHorizontalAlignment(HAlign_Fill);
            ImageSlot->SetVerticalAlignment(VAlign_Fill);
        }

        FallbackReticle = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TargetMarkerFallback"));
        FallbackReticle->SetText(FText::FromString(TEXT("◇")));
        FSlateFontInfo Font = FallbackReticle->GetFont();
        Font.Size = 38;
        FallbackReticle->SetFont(Font);
        FallbackReticle->SetJustification(ETextJustify::Center);
        if (UOverlaySlot* TextSlot = Root->AddChildToOverlay(FallbackReticle))
        {
            TextSlot->SetHorizontalAlignment(HAlign_Center);
            TextSlot->SetVerticalAlignment(VAlign_Center);
        }
    }
    else
    {
        if (!MarkerImage) MarkerImage = Cast<UImage>(WidgetTree->FindWidget(TEXT("TargetMarkerImage")));
        if (!FallbackReticle) FallbackReticle = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("TargetMarkerFallback")));
    }
}

void UARPGTargetMarkerWidget::ConfigureMarker(UTexture2D* Texture, UMaterialInterface* Material, FLinearColor InColor,
                                               bool bInAnimateAcquire, float InAcquireDuration,
                                               bool bInPulseWhileLocked, float InPulseSpeed, float InPulseScale,
                                               float InReleaseDuration)
{
    ActiveTexture = Texture;
    ActiveMaterial = Material;
    MarkerColor = InColor;
    bAnimateAcquire = bInAnimateAcquire;
    AcquireDuration = FMath::Max(0.01f, InAcquireDuration);
    bPulseWhileLocked = bInPulseWhileLocked;
    PulseSpeed = FMath::Max(0.f, InPulseSpeed);
    PulseScale = FMath::Max(0.f, InPulseScale);
    ReleaseDuration = FMath::Max(0.01f, InReleaseDuration);
    EnsureNativeLayout();
    ApplyMarkerArt();
    BP_OnMarkerConfigured(MarkerColor);
    PlayAcquireAnimation();
}

void UARPGTargetMarkerWidget::ApplyMarkerArt()
{
    EnsureNativeLayout();
    const bool bHasArt = ActiveMaterial != nullptr || ActiveTexture != nullptr;

    if (MarkerImage)
    {
        if (ActiveMaterial) MarkerImage->SetBrushFromMaterial(ActiveMaterial);
        else if (ActiveTexture) MarkerImage->SetBrushFromTexture(ActiveTexture, false);
        MarkerImage->SetColorAndOpacity(MarkerColor);
        MarkerImage->SetVisibility(bHasArt ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    }

    if (FallbackReticle)
    {
        FallbackReticle->SetColorAndOpacity(FSlateColor(MarkerColor));
        FallbackReticle->SetVisibility(bHasArt ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
    }
}

void UARPGTargetMarkerWidget::PlayAcquireAnimation()
{
    BP_OnTargetAcquired();
    bReleasing = false;
    bReleaseFinished = false;
    AnimationElapsed = 0.f;
    SetVisibility(ESlateVisibility::HitTestInvisible);
    if (bAnimateAcquire)
    {
        SetRenderOpacity(0.f);
        SetRenderScale(FVector2D(0.62f, 0.62f));
    }
    else
    {
        SetRenderOpacity(1.f);
        SetRenderScale(FVector2D(1.f, 1.f));
    }
}

void UARPGTargetMarkerWidget::PlayReleaseAnimation()
{
    if (bReleasing) return;
    BP_OnTargetReleased();
    bReleasing = true;
    bReleaseFinished = false;
    AnimationElapsed = 0.f;
}

void UARPGTargetMarkerWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    AnimationElapsed += FMath::Max(0.f, InDeltaTime);

    if (bReleasing)
    {
        const float Alpha = FMath::Clamp(AnimationElapsed / ReleaseDuration, 0.f, 1.f);
        const float Ease = FMath::InterpEaseIn(0.f, 1.f, Alpha, 2.f);
        SetRenderOpacity(1.f - Ease);
        const float Scale = FMath::Lerp(1.f, 0.78f, Ease);
        SetRenderScale(FVector2D(Scale, Scale));
        if (Alpha >= 1.f)
        {
            bReleaseFinished = true;
            SetVisibility(ESlateVisibility::Collapsed);
        }
        return;
    }

    if (bAnimateAcquire && AnimationElapsed < AcquireDuration)
    {
        const float Alpha = FMath::Clamp(AnimationElapsed / AcquireDuration, 0.f, 1.f);
        const float Ease = FMath::InterpEaseOut(0.f, 1.f, Alpha, 3.f);
        SetRenderOpacity(Ease);
        // A small overshoot gives the lock-on a crisp UI confirmation without requiring a WidgetAnimation asset.
        const float Overshoot = FMath::Sin(Alpha * PI) * 0.12f;
        const float Scale = FMath::Lerp(0.62f, 1.f, Ease) + Overshoot;
        SetRenderScale(FVector2D(Scale, Scale));
        return;
    }

    SetRenderOpacity(1.f);
    float Scale = 1.f;
    if (bPulseWhileLocked && PulseSpeed > 0.f && PulseScale > 0.f)
        Scale += FMath::Sin(AnimationElapsed * PulseSpeed * 2.f * PI) * PulseScale;
    SetRenderScale(FVector2D(Scale, Scale));
}
