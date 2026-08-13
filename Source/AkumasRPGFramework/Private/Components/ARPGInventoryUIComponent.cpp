#include "Components/ARPGInventoryUIComponent.h"

#include "Actors/ARPGCharacter.h"
#include "Components/ARPGEquipmentComponent.h"
#include "Components/ARPGInventoryComponent.h"
#include "Components/ARPGItemUseComponent.h"
#include "Items/ARPGItemUseBehavior.h"
#include "Components/ARPGQuickAccessComponent.h"
#include "Data/ARPGItemDefinition.h"
#include "GameFramework/PlayerController.h"

UARPGInventoryUIComponent::UARPGInventoryUIComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(false);
    InventoryWidgetClass = UARPGInventoryPanelWidget::StaticClass();
    QuickAccessWidgetClass = UARPGQuickAccessBarWidget::StaticClass();
    InventorySlotWidgetClass = UARPGInventoryItemSlotWidget::StaticClass();
    QuickAccessSlotWidgetClass = UARPGInventoryItemSlotWidget::StaticClass();
}

void UARPGInventoryUIComponent::BeginPlay()
{
    Super::BeginPlay();
    HandleOwnerControlChanged();
}

bool UARPGInventoryUIComponent::ResolveLocalPlayer(AARPGCharacter*& OutCharacter, APlayerController*& OutPlayerController) const
{
    OutCharacter = Cast<AARPGCharacter>(GetOwner());
    OutPlayerController = nullptr;
    if (!OutCharacter || !OutCharacter->IsLocallyControlled()) return false;

    OutPlayerController = Cast<APlayerController>(OutCharacter->GetController());
    return OutPlayerController && OutPlayerController->IsLocalController();
}

UARPGInventoryComponent* UARPGInventoryUIComponent::GetInventory() const
{
    if (const AARPGCharacter* Character = Cast<AARPGCharacter>(GetOwner())) return Character->Inventory;
    return GetOwner() ? GetOwner()->FindComponentByClass<UARPGInventoryComponent>() : nullptr;
}

UARPGQuickAccessComponent* UARPGInventoryUIComponent::GetQuickAccess() const
{
    if (const AARPGCharacter* Character = Cast<AARPGCharacter>(GetOwner())) return Character->QuickAccess;
    return GetOwner() ? GetOwner()->FindComponentByClass<UARPGQuickAccessComponent>() : nullptr;
}

UARPGItemUseComponent* UARPGInventoryUIComponent::GetItemUse() const
{
    if (const AARPGCharacter* Character = Cast<AARPGCharacter>(GetOwner())) return Character->ItemUse;
    return GetOwner() ? GetOwner()->FindComponentByClass<UARPGItemUseComponent>() : nullptr;
}

void UARPGInventoryUIComponent::HandleOwnerControlChanged()
{
    if (!GetWorld() || GetWorld()->GetNetMode() == NM_DedicatedServer) return;

    AARPGCharacter* Character = nullptr;
    APlayerController* PlayerController = nullptr;
    if (!ResolveLocalPlayer(Character, PlayerController))
    {
        CloseInventoryUIInternal(false);
        if (ActiveQuickAccessWidget)
        {
            ActiveQuickAccessWidget->RemoveFromParent();
            ActiveQuickAccessWidget = nullptr;
        }
        StopCooldownRefreshTimer();
        UnbindRuntimeEvents();
        CachedLocalPlayerController.Reset();
        return;
    }

    CachedLocalPlayerController = PlayerController;
    BindRuntimeEvents();
    if (bAutoCreateQuickAccessHUD && bShowQuickAccessHUD) EnsureQuickAccessUI();
}

void UARPGInventoryUIComponent::BindRuntimeEvents()
{
    if (bEventsBound) return;
    UARPGInventoryComponent* Inventory = GetInventory();
    UARPGQuickAccessComponent* QuickAccess = GetQuickAccess();
    UARPGItemUseComponent* ItemUse = GetItemUse();
    if (!Inventory || !QuickAccess || !ItemUse) return;

    Inventory->OnInventoryChanged.AddUniqueDynamic(this, &UARPGInventoryUIComponent::HandleInventoryChanged);
    QuickAccess->OnQuickAccessChanged.AddUniqueDynamic(this, &UARPGInventoryUIComponent::HandleQuickAccessChanged);
    QuickAccess->OnActiveQuickAccessSlotChanged.AddUniqueDynamic(this, &UARPGInventoryUIComponent::HandleActiveQuickAccessSlotChanged);
    ItemUse->OnItemUseCooldownsChanged.AddUniqueDynamic(this, &UARPGInventoryUIComponent::HandleItemUseCooldownsChanged);
    bEventsBound = true;
}

void UARPGInventoryUIComponent::UnbindRuntimeEvents()
{
    if (!bEventsBound) return;
    if (UARPGInventoryComponent* Inventory = GetInventory())
        Inventory->OnInventoryChanged.RemoveDynamic(this, &UARPGInventoryUIComponent::HandleInventoryChanged);
    if (UARPGQuickAccessComponent* QuickAccess = GetQuickAccess())
    {
        QuickAccess->OnQuickAccessChanged.RemoveDynamic(this, &UARPGInventoryUIComponent::HandleQuickAccessChanged);
        QuickAccess->OnActiveQuickAccessSlotChanged.RemoveDynamic(this, &UARPGInventoryUIComponent::HandleActiveQuickAccessSlotChanged);
    }
    if (UARPGItemUseComponent* ItemUse = GetItemUse())
        ItemUse->OnItemUseCooldownsChanged.RemoveDynamic(this, &UARPGInventoryUIComponent::HandleItemUseCooldownsChanged);
    bEventsBound = false;
}

bool UARPGInventoryUIComponent::OpenInventoryUI()
{
    if (!GetWorld() || GetWorld()->GetNetMode() == NM_DedicatedServer) return false;

    AARPGCharacter* Character = nullptr;
    APlayerController* PlayerController = nullptr;
    if (!ResolveLocalPlayer(Character, PlayerController)) return false;
    BindRuntimeEvents();

    if (IsInventoryUIOpen())
    {
        RefreshInventoryUI();
        return true;
    }

    TSubclassOf<UARPGInventoryPanelWidget> ResolvedClass = InventoryWidgetClass;
    if (!ResolvedClass) ResolvedClass = UARPGInventoryPanelWidget::StaticClass();

    ActiveInventoryWidget = CreateWidget<UARPGInventoryPanelWidget>(PlayerController, ResolvedClass);
    if (!ActiveInventoryWidget) return false;

    ActiveInventoryWidget->InitializeInventoryUI(Character, this);
    if (!ActiveInventoryWidget->AddToPlayerScreen(InventoryZOrder))
        ActiveInventoryWidget->AddToViewport(InventoryZOrder);

    CachedLocalPlayerController = PlayerController;
    EnsureQuickAccessUI();
    ApplyOpenInputMode(PlayerController);
    RefreshInventoryUI();
    RefreshQuickAccessUI();
    return true;
}

bool UARPGInventoryUIComponent::CloseInventoryUI()
{
    return CloseInventoryUIInternal(true);
}

bool UARPGInventoryUIComponent::CloseInventoryUIInternal(bool bRestoreInputMode)
{
    const bool bHadWidget = IsValid(ActiveInventoryWidget);
    if (ActiveInventoryWidget)
    {
        ActiveInventoryWidget->RemoveFromParent();
        ActiveInventoryWidget = nullptr;
    }

    APlayerController* PlayerController = CachedLocalPlayerController.Get();
    if (!PlayerController)
    {
        AARPGCharacter* Character = nullptr;
        ResolveLocalPlayer(Character, PlayerController);
    }

    if (bRestoreInputMode && PlayerController) RestoreClosedInputMode(PlayerController);
    return bHadWidget;
}

bool UARPGInventoryUIComponent::ToggleInventoryUI()
{
    return IsInventoryUIOpen() ? CloseInventoryUI() : OpenInventoryUI();
}

bool UARPGInventoryUIComponent::IsInventoryUIOpen() const
{
    return IsValid(ActiveInventoryWidget) && ActiveInventoryWidget->IsInViewport();
}

bool UARPGInventoryUIComponent::EnsureQuickAccessUI()
{
    if (!GetWorld() || GetWorld()->GetNetMode() == NM_DedicatedServer) return false;

    AARPGCharacter* Character = nullptr;
    APlayerController* PlayerController = nullptr;
    if (!ResolveLocalPlayer(Character, PlayerController)) return false;
    BindRuntimeEvents();

    if (ActiveQuickAccessWidget && ActiveQuickAccessWidget->IsInViewport())
    {
        ActiveQuickAccessWidget->SetVisibility(bShowQuickAccessHUD ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
        RefreshQuickAccessUI();
        return true;
    }

    TSubclassOf<UARPGQuickAccessBarWidget> ResolvedClass = QuickAccessWidgetClass;
    if (!ResolvedClass) ResolvedClass = UARPGQuickAccessBarWidget::StaticClass();

    ActiveQuickAccessWidget = CreateWidget<UARPGQuickAccessBarWidget>(PlayerController, ResolvedClass);
    if (!ActiveQuickAccessWidget) return false;

    ActiveQuickAccessWidget->InitializeQuickAccessUI(Character, this);
    if (!ActiveQuickAccessWidget->AddToPlayerScreen(QuickAccessZOrder))
        ActiveQuickAccessWidget->AddToViewport(QuickAccessZOrder);
    ActiveQuickAccessWidget->SetVisibility(bShowQuickAccessHUD ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
    CachedLocalPlayerController = PlayerController;
    RefreshQuickAccessUI();
    return true;
}

void UARPGInventoryUIComponent::SetQuickAccessHUDVisible(bool bShouldShow)
{
    bShowQuickAccessHUD = bShouldShow;
    if (bShouldShow)
    {
        EnsureQuickAccessUI();
        if (ActiveQuickAccessWidget) ActiveQuickAccessWidget->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    }
    else if (ActiveQuickAccessWidget)
    {
        ActiveQuickAccessWidget->SetVisibility(ESlateVisibility::Collapsed);
        StopCooldownRefreshTimer();
    }
}

void UARPGInventoryUIComponent::RefreshInventoryUI()
{
    if (ActiveInventoryWidget)
    {
        if (!ActiveInventoryWidget->IsInViewport())
        {
            CloseInventoryUIInternal(true);
            return;
        }
        ActiveInventoryWidget->RefreshInventoryUI();
    }
    UpdateCooldownRefreshTimer();
}

void UARPGInventoryUIComponent::RefreshQuickAccessUI()
{
    if (ActiveQuickAccessWidget && ActiveQuickAccessWidget->IsInViewport() && bShowQuickAccessHUD)
        ActiveQuickAccessWidget->RefreshQuickAccessUI();
    UpdateCooldownRefreshTimer();
}

int32 UARPGInventoryUIComponent::GetInventoryDisplaySlotCount() const
{
    if (const UARPGInventoryComponent* Inventory = GetInventory()) return FMath::Max(1, Inventory->MaxSlots);
    return 0;
}

int32 UARPGInventoryUIComponent::GetQuickAccessDisplaySlotCount() const
{
    if (const UARPGQuickAccessComponent* QuickAccess = GetQuickAccess()) return FMath::Max(1, QuickAccess->MaxQuickAccessSlots);
    return 0;
}

bool UARPGInventoryUIComponent::GetInventorySlotView(int32 SlotNumber, FARPGInventoryUISlotView& OutView) const
{
    OutView = FARPGInventoryUISlotView();
    OutView.Source = EARPGInventoryUISlotSource::Inventory;
    OutView.SlotNumber = SlotNumber;

    const UARPGInventoryComponent* Inventory = GetInventory();
    if (!Inventory || SlotNumber < 1 || SlotNumber > FMath::Max(1, Inventory->MaxSlots)) return false;

    const int32 Index = SlotNumber - 1;
    if (!Inventory->Items.IsValidIndex(Index)) return true;

    const FARPGInventoryEntry& Entry = Inventory->Items[Index];
    if (!Entry.InstanceId.IsValid() || Entry.Quantity <= 0) return true;

    OutView.bOccupied = true;
    OutView.bOwned = true;
    OutView.bEquipped = Entry.bEquipped;
    OutView.bBound = Entry.bBound;
    OutView.ItemId = Entry.ItemId;
    OutView.ItemInstanceId = Entry.InstanceId;
    OutView.Quantity = FMath::Max(0, Entry.Quantity);
    OutView.Durability = Entry.Durability;
    OutView.EquipmentSlot = Entry.EquipmentSlot;
    OutView.ItemDefinition = Inventory->ResolveItemDefinition(Entry);
    if (OutView.ItemDefinition)
    {
        OutView.DisplayName = OutView.ItemDefinition->DisplayName.IsEmpty() ? FText::FromName(Entry.ItemId) : OutView.ItemDefinition->DisplayName;
        OutView.Description = OutView.ItemDefinition->Description;
        OutView.Rarity = OutView.ItemDefinition->Rarity;
        if (OutView.ItemDefinition->bUsable)
        {
            if (const UARPGItemUseComponent* ItemUse = GetItemUse())
            {
                OutView.CooldownRemaining = ItemUse->GetCooldownRemaining(Entry.ItemId);
                const float CooldownTotal = FMath::Max(0.f, OutView.ItemDefinition->UseCooldownSeconds);
                OutView.CooldownPercent = CooldownTotal > KINDA_SMALL_NUMBER
                    ? FMath::Clamp(OutView.CooldownRemaining / CooldownTotal, 0.f, 1.f)
                    : 0.f;
            }
        }
    }
    else
    {
        OutView.DisplayName = FText::FromName(Entry.ItemId);
    }
    return true;
}

bool UARPGInventoryUIComponent::GetQuickAccessSlotView(int32 SlotNumber, FARPGInventoryUISlotView& OutView) const
{
    OutView = FARPGInventoryUISlotView();
    OutView.Source = EARPGInventoryUISlotSource::QuickAccess;
    OutView.SlotNumber = SlotNumber;

    const UARPGQuickAccessComponent* QuickAccess = GetQuickAccess();
    const UARPGInventoryComponent* Inventory = GetInventory();
    if (!QuickAccess || SlotNumber < 1 || SlotNumber > FMath::Max(1, QuickAccess->MaxQuickAccessSlots)) return false;

    FARPGQuickAccessSlotView QuickView;
    if (!QuickAccess->GetSlotView(SlotNumber, QuickView)) return true;

    OutView.bOccupied = QuickView.bAssigned;
    OutView.bOwned = QuickView.bOwned;
    OutView.bActive = QuickView.bActive;
    OutView.ItemId = QuickView.ItemId;
    OutView.ItemInstanceId = QuickView.ItemInstanceId;
    OutView.Quantity = QuickView.Quantity;
    OutView.ItemDefinition = QuickView.ItemDefinition;
    OutView.ResolvedQuickAccessAction = QuickView.ResolvedAction;
    OutView.CooldownRemaining = FMath::Max(0.f, QuickView.CooldownRemaining);

    if (QuickView.ItemDefinition)
    {
        OutView.DisplayName = QuickView.ItemDefinition->DisplayName.IsEmpty() ? FText::FromName(QuickView.ItemId) : QuickView.ItemDefinition->DisplayName;
        OutView.Description = QuickView.ItemDefinition->Description;
        OutView.Rarity = QuickView.ItemDefinition->Rarity;
        const float CooldownTotal = FMath::Max(0.f, QuickView.ItemDefinition->UseCooldownSeconds);
        OutView.CooldownPercent = CooldownTotal > KINDA_SMALL_NUMBER
            ? FMath::Clamp(OutView.CooldownRemaining / CooldownTotal, 0.f, 1.f)
            : 0.f;
    }
    else if (!QuickView.ItemId.IsNone())
    {
        OutView.DisplayName = FText::FromName(QuickView.ItemId);
    }

    if (Inventory && QuickView.ItemInstanceId.IsValid())
    {
        const FARPGInventoryEntry* Entry = Inventory->Items.FindByPredicate([&](const FARPGInventoryEntry& Candidate)
        {
            return Candidate.InstanceId == QuickView.ItemInstanceId && Candidate.Quantity > 0;
        });
        if (Entry)
        {
            OutView.bEquipped = Entry->bEquipped;
            OutView.bBound = Entry->bBound;
            OutView.Durability = Entry->Durability;
            OutView.EquipmentSlot = Entry->EquipmentSlot;
        }
    }
    return true;
}

bool UARPGInventoryUIComponent::AssignInventoryItemToQuickAccess(FGuid ItemInstanceId, int32 TargetQuickAccessSlot)
{
    UARPGQuickAccessComponent* QuickAccess = GetQuickAccess();
    if (!QuickAccess || !ItemInstanceId.IsValid()) return false;
    const bool bAccepted = QuickAccess->AssignItemToSlot(TargetQuickAccessSlot, ItemInstanceId);
    if (bAccepted) RefreshQuickAccessUI();
    return bAccepted;
}

bool UARPGInventoryUIComponent::SwapQuickAccessSlots(int32 FirstSlotNumber, int32 SecondSlotNumber)
{
    UARPGQuickAccessComponent* QuickAccess = GetQuickAccess();
    if (!QuickAccess) return false;
    if (FirstSlotNumber == SecondSlotNumber) return true;
    const bool bAccepted = QuickAccess->SwapSlots(FirstSlotNumber, SecondSlotNumber);
    if (bAccepted) RefreshQuickAccessUI();
    return bAccepted;
}

bool UARPGInventoryUIComponent::ClearQuickAccessSlot(int32 SlotNumber, bool bUnequipIfActive)
{
    UARPGQuickAccessComponent* QuickAccess = GetQuickAccess();
    if (!QuickAccess) return false;
    const bool bAccepted = bUnequipIfActive ? QuickAccess->ClearSlotAndUnequipActive(SlotNumber) : QuickAccess->ClearSlot(SlotNumber);
    if (bAccepted)
    {
        RefreshInventoryUI();
        RefreshQuickAccessUI();
    }
    return bAccepted;
}

bool UARPGInventoryUIComponent::ActivateQuickAccessSlot(int32 SlotNumber)
{
    UARPGQuickAccessComponent* QuickAccess = GetQuickAccess();
    if (!QuickAccess) return false;
    const bool bAccepted = QuickAccess->ActivateSlot(SlotNumber);
    if (bAccepted) RefreshQuickAccessUI();
    return bAccepted;
}

bool UARPGInventoryUIComponent::CanUseInventoryItemNow(FGuid ItemInstanceId) const
{
    const AARPGCharacter* Character = Cast<AARPGCharacter>(GetOwner());
    return Character && Character->ItemUse && Character->ItemUse->CanUseItemNow(ItemInstanceId);
}

bool UARPGInventoryUIComponent::UseInventoryItem(FGuid ItemInstanceId)
{
    AARPGCharacter* Character = Cast<AARPGCharacter>(GetOwner());
    if (!Character || !Character->Inventory || !Character->ItemUse || !ItemInstanceId.IsValid()) return false;
    UARPGItemDefinition* Definition = Character->Inventory->GetItemDefinitionForInstance(ItemInstanceId);
    if (!Definition || !Definition->bUsable || !Character->ItemUse->CanUseItemNow(ItemInstanceId)) return false;

    const bool bAccepted = Character->ItemUse->UseItem(ItemInstanceId, EARPGItemUseSource::InventoryUI, 0);
    if (bAccepted)
    {
        RefreshInventoryUI();
        RefreshQuickAccessUI();
    }
    return bAccepted;
}

bool UARPGInventoryUIComponent::ActivateInventoryItem(FGuid ItemInstanceId)
{
    AARPGCharacter* Character = Cast<AARPGCharacter>(GetOwner());
    if (!Character || !Character->Inventory || !ItemInstanceId.IsValid()) return false;

    UARPGItemDefinition* Definition = Character->Inventory->GetItemDefinitionForInstance(ItemInstanceId);
    if (!Definition) return false;

    const bool bPreferUse = Definition->bUsable && Definition->QuickAccessAction == EARPGQuickAccessAction::Use;
    if (bPreferUse) return UseInventoryItem(ItemInstanceId);

    if (Definition->bEquippable && Definition->EquipmentSlot.IsValid())
    {
        if (!Character->Equipment) return false;
        const bool bAccepted = Character->Inventory->IsItemInstanceEquipped(ItemInstanceId)
            ? Character->Equipment->UnequipItem(ItemInstanceId)
            : Character->Equipment->EquipItem(ItemInstanceId);
        if (bAccepted)
        {
            RefreshInventoryUI();
            RefreshQuickAccessUI();
        }
        return bAccepted;
    }

    if (Definition->bUsable) return UseInventoryItem(ItemInstanceId);
    return false;
}

bool UARPGInventoryUIComponent::ToggleEquipInventoryItem(FGuid ItemInstanceId)
{
    // Kept for Blueprint compatibility. v2.13 makes this the same context-sensitive primary action
    // the native Inventory UI has always intended: equipment toggles, usable items use directly.
    return ActivateInventoryItem(ItemInstanceId);
}

void UARPGInventoryUIComponent::SelectInventorySlot(int32 SlotNumber)
{
    if (!ActiveInventoryWidget) return;
    FARPGInventoryUISlotView View;
    if (GetInventorySlotView(SlotNumber, View)) ActiveInventoryWidget->SetSelectedSlotView(View);
}

void UARPGInventoryUIComponent::HandleInventoryChanged()
{
    RefreshInventoryUI();
    RefreshQuickAccessUI();
}

void UARPGInventoryUIComponent::HandleQuickAccessChanged()
{
    RefreshQuickAccessUI();
}

void UARPGInventoryUIComponent::HandleActiveQuickAccessSlotChanged(int32 SlotNumber, FName ItemId, FGuid ItemInstanceId)
{
    RefreshQuickAccessUI();
}

void UARPGInventoryUIComponent::HandleItemUseCooldownsChanged()
{
    RefreshInventoryUI();
    RefreshQuickAccessUI();
}

void UARPGInventoryUIComponent::UpdateCooldownRefreshTimer()
{
    if (!GetWorld()) return;

    bool bNeedsCooldownRefresh = false;
    if (ActiveQuickAccessWidget && ActiveQuickAccessWidget->IsInViewport() && bShowQuickAccessHUD)
    {
        const int32 SlotCount = GetQuickAccessDisplaySlotCount();
        for (int32 SlotNumber = 1; SlotNumber <= SlotCount; ++SlotNumber)
        {
            FARPGInventoryUISlotView View;
            if (GetQuickAccessSlotView(SlotNumber, View) && View.CooldownRemaining > KINDA_SMALL_NUMBER)
            {
                bNeedsCooldownRefresh = true;
                break;
            }
        }
    }

    if (!bNeedsCooldownRefresh && IsInventoryUIOpen())
    {
        const int32 SlotCount = GetInventoryDisplaySlotCount();
        for (int32 SlotNumber = 1; SlotNumber <= SlotCount; ++SlotNumber)
        {
            FARPGInventoryUISlotView View;
            if (GetInventorySlotView(SlotNumber, View) && View.CooldownRemaining > KINDA_SMALL_NUMBER)
            {
                bNeedsCooldownRefresh = true;
                break;
            }
        }
    }

    FTimerManager& TimerManager = GetWorld()->GetTimerManager();
    if (bNeedsCooldownRefresh)
    {
        if (!TimerManager.IsTimerActive(CooldownRefreshTimer))
        {
            const float Interval = FMath::Clamp(CooldownRefreshInterval, 0.05f, 1.f);
            TimerManager.SetTimer(CooldownRefreshTimer, this, &UARPGInventoryUIComponent::HandleCooldownRefreshTick, Interval, true, Interval);
        }
    }
    else
    {
        StopCooldownRefreshTimer();
    }
}

void UARPGInventoryUIComponent::HandleCooldownRefreshTick()
{
    if (ActiveInventoryWidget && ActiveInventoryWidget->IsInViewport()) ActiveInventoryWidget->RefreshInventoryUI();
    if (ActiveQuickAccessWidget && ActiveQuickAccessWidget->IsInViewport() && bShowQuickAccessHUD) ActiveQuickAccessWidget->RefreshQuickAccessUI();
    UpdateCooldownRefreshTimer();
}

void UARPGInventoryUIComponent::StopCooldownRefreshTimer()
{
    if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(CooldownRefreshTimer);
}

void UARPGInventoryUIComponent::ApplyOpenInputMode(APlayerController* PlayerController)
{
    if (!bManageInputMode || !PlayerController) return;
    bPreviousMouseCursor = PlayerController->bShowMouseCursor;

    FInputModeGameAndUI InputMode;
    InputMode.SetHideCursorDuringCapture(false);
    InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    PlayerController->SetInputMode(InputMode);
    PlayerController->bShowMouseCursor = bShowMouseCursorWhileOpen;
}

void UARPGInventoryUIComponent::RestoreClosedInputMode(APlayerController* PlayerController)
{
    if (!bManageInputMode || !PlayerController) return;
    if (bRestoreGameOnlyInputOnClose)
    {
        FInputModeGameOnly InputMode;
        PlayerController->SetInputMode(InputMode);
    }
    PlayerController->bShowMouseCursor = bPreviousMouseCursor;
}

void UARPGInventoryUIComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    CloseInventoryUIInternal(false);
    if (ActiveQuickAccessWidget)
    {
        ActiveQuickAccessWidget->RemoveFromParent();
        ActiveQuickAccessWidget = nullptr;
    }
    StopCooldownRefreshTimer();
    UnbindRuntimeEvents();
    CachedLocalPlayerController.Reset();
    Super::EndPlay(EndPlayReason);
}
