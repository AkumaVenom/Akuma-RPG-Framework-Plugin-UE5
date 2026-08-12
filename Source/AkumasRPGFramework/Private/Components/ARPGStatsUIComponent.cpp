#include "Components/ARPGStatsUIComponent.h"

#include "Actors/ARPGCharacter.h"
#include "Components/ARPGProgressionComponent.h"
#include "Components/ARPGStatsComponent.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/PlayerController.h"

UARPGStatsUIComponent::UARPGStatsUIComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(false);
    StatsWidgetClass = UARPGStatsPanelWidget::StaticClass();
}

bool UARPGStatsUIComponent::ResolveLocalPlayer(AARPGCharacter*& OutCharacter, APlayerController*& OutPlayerController) const
{
    OutCharacter = Cast<AARPGCharacter>(GetOwner());
    OutPlayerController = nullptr;
    if (!OutCharacter || !OutCharacter->IsLocallyControlled()) return false;

    OutPlayerController = Cast<APlayerController>(OutCharacter->GetController());
    return OutPlayerController && OutPlayerController->IsLocalController();
}

bool UARPGStatsUIComponent::OpenStatsUI()
{
    if (!GetWorld() || GetWorld()->GetNetMode() == NM_DedicatedServer) return false;

    AARPGCharacter* Character = nullptr;
    APlayerController* PlayerController = nullptr;
    if (!ResolveLocalPlayer(Character, PlayerController)) return false;

    if (IsStatsUIOpen())
    {
        RefreshStatsUI();
        return true;
    }

    TSubclassOf<UARPGStatsPanelWidget> ResolvedClass = StatsWidgetClass;
    if (!ResolvedClass) ResolvedClass = UARPGStatsPanelWidget::StaticClass();

    ActiveStatsWidget = CreateWidget<UARPGStatsPanelWidget>(PlayerController, ResolvedClass);
    if (!ActiveStatsWidget) return false;

    ActiveStatsWidget->InitializeStatsUI(Character, this);
    if (!ActiveStatsWidget->AddToPlayerScreen(ZOrder))
    {
        ActiveStatsWidget->AddToViewport(ZOrder);
    }

    CachedLocalPlayerController = PlayerController;
    ApplyOpenInputMode(PlayerController);
    StartRefreshTimer();
    RefreshStatsUI();
    return true;
}

bool UARPGStatsUIComponent::CloseStatsUI()
{
    return CloseStatsUIInternal(true);
}

bool UARPGStatsUIComponent::CloseStatsUIInternal(bool bRestoreInputMode)
{
    const bool bHadWidget = IsValid(ActiveStatsWidget);
    StopRefreshTimer();

    if (ActiveStatsWidget)
    {
        ActiveStatsWidget->RemoveFromParent();
        ActiveStatsWidget = nullptr;
    }

    APlayerController* PlayerController = CachedLocalPlayerController.Get();
    if (!PlayerController)
    {
        AARPGCharacter* Character = nullptr;
        ResolveLocalPlayer(Character, PlayerController);
    }

    if (bRestoreInputMode && PlayerController)
    {
        RestoreClosedInputMode(PlayerController);
    }

    CachedLocalPlayerController.Reset();
    return bHadWidget;
}

bool UARPGStatsUIComponent::ToggleStatsUI()
{
    return IsStatsUIOpen() ? CloseStatsUI() : OpenStatsUI();
}

bool UARPGStatsUIComponent::IsStatsUIOpen() const
{
    return IsValid(ActiveStatsWidget) && ActiveStatsWidget->IsInViewport();
}

void UARPGStatsUIComponent::RefreshStatsUI()
{
    if (!ActiveStatsWidget) return;
    if (!ActiveStatsWidget->IsInViewport())
    {
        CloseStatsUIInternal(true);
        return;
    }

    ActiveStatsWidget->SetStatsUISnapshot(GetStatsUISnapshot());
}

FARPGStatsUISnapshot UARPGStatsUIComponent::GetStatsUISnapshot() const
{
    FARPGStatsUISnapshot Snapshot;
    AARPGCharacter* Character = Cast<AARPGCharacter>(GetOwner());
    if (!Character) return Snapshot;

    Snapshot.Character = Character;
    Snapshot.CharacterName = Character->RPGCharacterName;

    const UARPGStatsComponent* Stats = Character->Stats;
    const UARPGProgressionComponent* Progression = Character->Progression;

    if (Stats)
    {
        Snapshot.bJRPGStatSystemEnabled = Stats->bEnableJRPGStatSystem;
        Snapshot.Level = Stats->GetEffectiveLevel();
        Snapshot.Health = Stats->Health;
        Snapshot.MaxHealth = Stats->MaxHealth;
        Snapshot.HealthPercent = Stats->MaxHealth > 0.f ? FMath::Clamp(Stats->Health / Stats->MaxHealth, 0.f, 1.f) : 0.f;
        Snapshot.Mana = Stats->Mana;
        Snapshot.MaxMana = Stats->MaxMana;
        Snapshot.ManaPercent = Stats->MaxMana > 0.f ? FMath::Clamp(Stats->Mana / Stats->MaxMana, 0.f, 1.f) : 0.f;
        Snapshot.Stamina = Stats->Stamina;
        Snapshot.MaxStamina = Stats->MaxStamina;
        Snapshot.StaminaPercent = Stats->MaxStamina > 0.f ? FMath::Clamp(Stats->Stamina / Stats->MaxStamina, 0.f, 1.f) : 0.f;
        Snapshot.PrimaryStats = Stats->PrimaryStats;
        Snapshot.AllocatedPoints = Stats->StatProgression.AllocatedPoints;
        Snapshot.UnspentAttributePoints = Stats->StatProgression.UnspentAttributePoints;
        Snapshot.TotalAttributePointsEarned = Stats->StatProgression.TotalAttributePointsEarned;

        Snapshot.DerivedStats = Stats->DerivedStats;
        // Getter-backed values make the panel truthful for legacy characters too.
        Snapshot.DerivedStats.MeleeAttackPower = Stats->GetMeleeAttackPower();
        Snapshot.DerivedStats.RangedAttackPower = Stats->GetRangedAttackPower();
        Snapshot.DerivedStats.MagicAttackPower = Stats->GetMagicAttackPower();
        Snapshot.DerivedStats.PhysicalDefense = Stats->GetPhysicalDefense();
        Snapshot.DerivedStats.MagicDefense = Stats->GetMagicDefense();
        Snapshot.DerivedStats.Accuracy = Stats->GetAccuracy();
        Snapshot.DerivedStats.Evasion = Stats->GetEvasion();
        Snapshot.DerivedStats.MagicEvasion = Stats->GetMagicEvasion();
        Snapshot.DerivedStats.Speed = Stats->GetDerivedStatValue(EARPGDerivedStat::Speed);
        Snapshot.DerivedStats.CriticalChance = Stats->GetCriticalChanceBonus();
        Snapshot.DerivedStats.CriticalDamageMultiplier = Stats->GetCriticalDamageMultiplier();
        Snapshot.DerivedStats.AttackSpeedMultiplier = Stats->GetAttackSpeedMultiplier();
        Snapshot.DerivedStats.MovementSpeedMultiplier = Stats->GetMovementSpeedMultiplier();

        Snapshot.LegacyAttackPower = Stats->AttackPower;
        Snapshot.LegacySpellPower = Stats->SpellPower;
        Snapshot.LegacyArmor = Stats->Armor;
    }

    if (Progression)
    {
        if (!Stats) Snapshot.Level = Progression->Level;
        Snapshot.CurrentXP = Progression->XP;
        Snapshot.bAtMaxLevel = Progression->Level >= Progression->MaxLevel;
        Snapshot.XPRequiredForNextLevel = Snapshot.bAtMaxLevel ? 0 : Progression->GetXPRequiredForLevel(Progression->Level);
        Snapshot.XPPercent = Progression->GetLevelProgress01();
    }

    Snapshot.Level = FMath::Max(1, Snapshot.Level);
    return Snapshot;
}

void UARPGStatsUIComponent::StartRefreshTimer()
{
    StopRefreshTimer();
    if (!GetWorld()) return;
    const float Interval = FMath::Clamp(RefreshInterval, 0.05f, 2.f);
    GetWorld()->GetTimerManager().SetTimer(RefreshTimerHandle, this, &UARPGStatsUIComponent::RefreshStatsUI, Interval, true, Interval);
}

void UARPGStatsUIComponent::StopRefreshTimer()
{
    if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(RefreshTimerHandle);
}

void UARPGStatsUIComponent::ApplyOpenInputMode(APlayerController* PlayerController)
{
    if (!bManageInputMode || !PlayerController) return;
    bPreviousMouseCursor = PlayerController->bShowMouseCursor;

    FInputModeGameAndUI InputMode;
    InputMode.SetHideCursorDuringCapture(false);
    InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    PlayerController->SetInputMode(InputMode);
    PlayerController->bShowMouseCursor = bShowMouseCursorWhileOpen;
}

void UARPGStatsUIComponent::RestoreClosedInputMode(APlayerController* PlayerController)
{
    if (!bManageInputMode || !PlayerController) return;
    if (bRestoreGameOnlyInputOnClose)
    {
        FInputModeGameOnly InputMode;
        PlayerController->SetInputMode(InputMode);
    }
    PlayerController->bShowMouseCursor = bPreviousMouseCursor;
}

void UARPGStatsUIComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    CloseStatsUIInternal(false);
    Super::EndPlay(EndPlayReason);
}
