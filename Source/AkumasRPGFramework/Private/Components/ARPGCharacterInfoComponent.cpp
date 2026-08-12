#include "Components/ARPGCharacterInfoComponent.h"

#include "Actors/ARPGCharacter.h"
#include "Components/ARPGProgressionComponent.h"
#include "Components/ARPGSpawnEntranceComponent.h"
#include "Components/ARPGStatsComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

UARPGCharacterInfoComponent::UARPGCharacterInfoComponent()
{
    // UWidgetComponent owns a real TickComponent path for screen-space projection/rendering.
    // Keep ticking *possible*, then enable it only while the popup is actually visible.
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = false;
    SetIsReplicatedByDefault(false);

    SetWidgetSpace(EWidgetSpace::Screen);
    SetTickMode(ETickMode::Automatic);
    SetDrawAtDesiredSize(true);
    SetDrawSize(FVector2D(320.f, 96.f));
    SetPivot(FVector2D(0.5f, 1.f));
    SetCollisionEnabled(ECollisionEnabled::NoCollision);
    SetGenerateOverlapEvents(false);
    SetWindowFocusable(false);
    SetTickWhenOffscreen(false);
    SetWidgetClass(UARPGCharacterInfoWidget::StaticClass());
    SetVisibility(false);
}

void UARPGCharacterInfoComponent::BeginPlay()
{
    // Preserve the class authored on the inherited component before any runtime allocation policy.
    // Crucially, do NOT clear WidgetClass before UWidgetComponent::BeginPlay: its native lifecycle
    // must be allowed to initialize normally for reliable screen-space registration.
    CachedConfiguredWidgetClass = GetWidgetClass();

    const bool bDedicatedServer = GetWorld() && GetWorld()->GetNetMode() == NM_DedicatedServer;

    Super::BeginPlay();
    ApplyAutomaticHeight();
    SetVisibility(false);
    RemoveWidgetFromScreen();
    SetComponentTickEnabled(false);

    if (bDedicatedServer || !GetWorld())
    {
        SetWidget(nullptr);
        SetWidgetClass(nullptr);
        return;
    }

    // Lazy mode may retire the instance created by UWidgetComponent registration, but it keeps the
    // configured class and native screen-space lifecycle intact. The proximity timer recreates it.
    if (bLazyCreateWidget && GetUserWidgetObject())
    {
        SetWidget(nullptr);
    }

    if (AARPGCharacter* Character = Cast<AARPGCharacter>(GetOwner()))
    {
        if (Character->Stats) Character->Stats->OnHealthChanged.AddDynamic(this, &UARPGCharacterInfoComponent::HandleHealthChanged);
        if (Character->Progression) Character->Progression->OnLevelChanged.AddDynamic(this, &UARPGCharacterInfoComponent::HandleLevelChanged);
    }

    RefreshCharacterInfoNow();

    const float Interval = FMath::Max(0.05f, ProximityCheckInterval);
    const uint32 StableSeed = GetOwner() ? static_cast<uint32>(GetOwner()->GetUniqueID()) : 0u;
    const float InitialDelay = Interval * (0.15f + (static_cast<float>(StableSeed % 71u) / 100.f));
    GetWorld()->GetTimerManager().SetTimer(ProximityTimer, this, &UARPGCharacterInfoComponent::RefreshCharacterInfoNow, Interval, true, InitialDelay);
}

void UARPGCharacterInfoComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (AARPGCharacter* Character = Cast<AARPGCharacter>(GetOwner()))
    {
        if (Character->Stats) Character->Stats->OnHealthChanged.RemoveDynamic(this, &UARPGCharacterInfoComponent::HandleHealthChanged);
        if (Character->Progression) Character->Progression->OnLevelChanged.RemoveDynamic(this, &UARPGCharacterInfoComponent::HandleLevelChanged);
    }
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(ProximityTimer);
    }
    SetPopupVisibleLocal(false, nullptr);
    ReleaseWidgetInstance();
    Super::EndPlay(EndPlayReason);
}

void UARPGCharacterInfoComponent::ApplyAutomaticHeight()
{
    const AARPGCharacter* Character = Cast<AARPGCharacter>(GetOwner());
    if (!Character) return;

    float RelativeZ = ManualRelativeHeight;
    if (bAutoHeightFromCapsule)
    {
        if (const UCapsuleComponent* Capsule = Character->GetCapsuleComponent())
        {
            RelativeZ = Capsule->GetUnscaledCapsuleHalfHeight() + HeightAboveCapsule;
        }
    }
    SetRelativeLocation(FVector(0.f, 0.f, RelativeZ));
}

bool UARPGCharacterInfoComponent::FindNearestLocalViewer(ULocalPlayer*& OutLocalPlayer, APawn*& OutPawn, float& OutDistanceSquared) const
{
    OutLocalPlayer = nullptr;
    OutPawn = nullptr;
    OutDistanceSquared = TNumericLimits<float>::Max();

    const AActor* Owner = GetOwner();
    const UWorld* World = GetWorld();
    const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
    if (!Owner || !World || !GameInstance) return false;

    for (ULocalPlayer* LocalPlayer : GameInstance->GetLocalPlayers())
    {
        if (!LocalPlayer) continue;
        APlayerController* PC = LocalPlayer->GetPlayerController(GetWorld());
        APawn* Pawn = PC ? PC->GetPawn() : nullptr;
        if (!Pawn) continue;

        const float DistanceSquared = FVector::DistSquared(Pawn->GetActorLocation(), Owner->GetActorLocation());
        if (DistanceSquared < OutDistanceSquared)
        {
            OutDistanceSquared = DistanceSquared;
            OutLocalPlayer = LocalPlayer;
            OutPawn = Pawn;
        }
    }

    return OutLocalPlayer != nullptr && OutPawn != nullptr;
}

bool UARPGCharacterInfoComponent::ShouldShowForViewer(ULocalPlayer* LocalPlayer, APawn* ViewerPawn, float DistanceSquared) const
{
    const AARPGCharacter* Character = Cast<AARPGCharacter>(GetOwner());
    if (!bEnableInfoPopup || !Character || !LocalPlayer || !ViewerPawn || Character->IsHidden()) return false;

    if (bHideLocalPlayerSelf && ViewerPawn == Character) return false;

    const bool bPlayerControlled = Character->IsPlayerControlled() || Character->GetPlayerState() != nullptr;
    if (bPlayerControlled && !bShowOnPlayerControlledCharacters) return false;
    if (!bPlayerControlled && !bShowOnAICharacters) return false;

    if (bHideWhenDead && Character->Stats && Character->Stats->Health <= 0.f) return false;

    if (bHideDuringSpawnEntrance)
    {
        if (const UARPGSpawnEntranceComponent* Entrance = Character->FindComponentByClass<UARPGSpawnEntranceComponent>())
        {
            if (Entrance->IsGroundRiseActive()) return false;
        }
    }

    const float SafeShowDistance = FMath::Max(0.f, ShowDistance);
    const float SafeHideDistance = FMath::Max(SafeShowDistance, HideDistance);
    const float ActiveDistance = bPopupVisible ? SafeHideDistance : SafeShowDistance;
    if (DistanceSquared > FMath::Square(ActiveDistance)) return false;

    if (bRequireLineOfSight)
    {
        APlayerController* PC = LocalPlayer->GetPlayerController(GetWorld());
        if (!PC || !PC->LineOfSightTo(Character)) return false;
    }

    return true;
}

FARPGCharacterInfoSnapshot UARPGCharacterInfoComponent::GetCharacterInfoSnapshot() const
{
    FARPGCharacterInfoSnapshot Snapshot;
    AARPGCharacter* Character = Cast<AARPGCharacter>(GetOwner());
    if (!Character) return Snapshot;

    Snapshot.Character = Character;
    Snapshot.CharacterName = Character->RPGCharacterName;

    if (Character->Stats)
    {
        Snapshot.Level = FMath::Max(1, Character->Stats->GetEffectiveLevel());
        Snapshot.Health = Character->Stats->Health;
        Snapshot.MaxHealth = Character->Stats->MaxHealth;
        Snapshot.HealthPercent = Character->Stats->GetHealthPercent();
        Snapshot.bAlive = Character->Stats->Health > 0.f;
    }
    else
    {
        Snapshot.Level = Character->Progression ? FMath::Max(1, Character->Progression->Level) : 1;
        Snapshot.Health = 0.f;
        Snapshot.MaxHealth = 0.f;
        Snapshot.HealthPercent = 0.f;
        Snapshot.bAlive = true;
    }

    return Snapshot;
}

void UARPGCharacterInfoComponent::EnsureWidgetCreated(ULocalPlayer* LocalPlayer)
{
    TSubclassOf<UUserWidget> DesiredClass = CachedConfiguredWidgetClass;
    if (!DesiredClass)
    {
        DesiredClass = UARPGCharacterInfoWidget::StaticClass();
        CachedConfiguredWidgetClass = DesiredClass;
    }

    if (LocalPlayer && GetOwnerPlayer() != LocalPlayer)
    {
        SetOwnerPlayer(LocalPlayer);
    }

    if (GetWidgetClass() != DesiredClass)
    {
        SetWidgetClass(DesiredClass);
    }

    if (!GetUserWidgetObject())
    {
        InitWidget();
    }
}

void UARPGCharacterInfoComponent::ReleaseWidgetInstance()
{
    if (!GetUserWidgetObject()) return;

    // Preserve WidgetClass. Clearing it after BeginPlay can break the native screen-space widget
    // lifecycle and also discards the class selected on the inherited CharacterInfo component.
    RemoveWidgetFromScreen();
    SetWidget(nullptr);
}

void UARPGCharacterInfoComponent::ApplySnapshotToWidget(const FARPGCharacterInfoSnapshot& Snapshot)
{
    UUserWidget* UserWidget = GetUserWidgetObject();
    if (!UserWidget) return;

    if (UARPGCharacterInfoWidget* TypedWidget = Cast<UARPGCharacterInfoWidget>(UserWidget))
    {
        TypedWidget->SetCharacterInfo(Snapshot);
        return;
    }

    // Zero-graph fallback for an ordinary project UserWidget: if it contains standard (or remapped)
    // child names, populate them directly. Projects that need more logic can reparent to
    // UARPGCharacterInfoWidget and consume On ARPG Character Info Updated instead.
    if (!NameTextWidgetName.IsNone())
    {
        if (UTextBlock* Text = Cast<UTextBlock>(UserWidget->GetWidgetFromName(NameTextWidgetName)))
        {
            Text->SetText(FText::FromString(Snapshot.CharacterName));
        }
    }
    if (!LevelTextWidgetName.IsNone())
    {
        if (UTextBlock* Text = Cast<UTextBlock>(UserWidget->GetWidgetFromName(LevelTextWidgetName)))
        {
            Text->SetText(FText::Format(NSLOCTEXT("AkumasRPGFramework", "NPCInfoLevel", "Level {0}"), FText::AsNumber(FMath::Max(1, Snapshot.Level))));
        }
    }
    if (!HealthBarWidgetName.IsNone())
    {
        if (UProgressBar* Bar = Cast<UProgressBar>(UserWidget->GetWidgetFromName(HealthBarWidgetName)))
        {
            Bar->SetPercent(FMath::Clamp(Snapshot.HealthPercent, 0.f, 1.f));
        }
    }
    if (!HealthTextWidgetName.IsNone())
    {
        if (UTextBlock* Text = Cast<UTextBlock>(UserWidget->GetWidgetFromName(HealthTextWidgetName)))
        {
            Text->SetText(FText::Format(NSLOCTEXT("AkumasRPGFramework", "NPCInfoHealth", "{0} / {1}"),
                FText::AsNumber(FMath::Max(0, FMath::RoundToInt(Snapshot.Health))),
                FText::AsNumber(FMath::Max(0, FMath::RoundToInt(Snapshot.MaxHealth)))));
        }
    }
}

void UARPGCharacterInfoComponent::SetPopupVisibleLocal(bool bShouldBeVisible, ULocalPlayer* LocalPlayer)
{
    if (bShouldBeVisible)
    {
        // Screen-space UWidgetComponent requires its native component tick path to project/register
        // the widget. Enable it before initialization/visibility, then let Unreal manage projection.
        SetComponentTickEnabled(true);
        SetTickMode(ETickMode::Automatic);
        EnsureWidgetCreated(LocalPlayer);
        if (!GetUserWidgetObject())
        {
            bPopupVisible = false;
            SetVisibility(false);
            RemoveWidgetFromScreen();
            SetComponentTickEnabled(false);
            return;
        }

        bPopupVisible = true;
        HiddenSinceTime = -1.f;
        ApplySnapshotToWidget(GetCharacterInfoSnapshot());
        SetVisibility(true);
        RequestRenderUpdate();
        return;
    }

    if (bPopupVisible && GetWorld())
    {
        HiddenSinceTime = GetWorld()->GetTimeSeconds();
    }
    else if (HiddenSinceTime < 0.f && GetWorld())
    {
        HiddenSinceTime = GetWorld()->GetTimeSeconds();
    }

    bPopupVisible = false;
    SetVisibility(false);
    RemoveWidgetFromScreen();
    SetComponentTickEnabled(false);
}

void UARPGCharacterInfoComponent::RefreshCharacterInfoNow()
{
    if (!GetWorld() || GetWorld()->GetNetMode() == NM_DedicatedServer)
    {
        SetVisibility(false);
        return;
    }

    ApplyAutomaticHeight();

    ULocalPlayer* LocalPlayer = nullptr;
    APawn* ViewerPawn = nullptr;
    float DistanceSquared = TNumericLimits<float>::Max();
    const bool bHasViewer = FindNearestLocalViewer(LocalPlayer, ViewerPawn, DistanceSquared);
    const bool bShouldShow = bHasViewer && ShouldShowForViewer(LocalPlayer, ViewerPawn, DistanceSquared);

    if (bShouldShow)
    {
        SetPopupVisibleLocal(true, LocalPlayer);
        return;
    }

    SetPopupVisibleLocal(false, LocalPlayer);

    if (bReleaseWidgetWhenFar && GetUserWidgetObject() && HiddenSinceTime >= 0.f)
    {
        const float HiddenFor = GetWorld()->GetTimeSeconds() - HiddenSinceTime;
        if (HiddenFor >= FMath::Max(0.f, FarWidgetReleaseDelay))
        {
            ReleaseWidgetInstance();
        }
    }
}

void UARPGCharacterInfoComponent::HandleHealthChanged(float NewHealth, float Delta)
{
    (void)NewHealth;
    (void)Delta;
    RefreshCharacterInfoNow();
}

void UARPGCharacterInfoComponent::HandleLevelChanged(int32 OldLevel, int32 NewLevel)
{
    (void)OldLevel;
    (void)NewLevel;
    if (bPopupVisible) ApplySnapshotToWidget(GetCharacterInfoSnapshot());
}
