#include "Components/ARPGBuildingUIComponent.h"
#include "Actors/ARPGCharacter.h"
#include "Building/ARPGBuildDoorActor.h"
#include "Building/ARPGBuildLightActor.h"
#include "Building/ARPGBuildWindowActor.h"
#include "Building/ARPGBuildPieceActor.h"
#include "Building/ARPGBuildingComponent.h"
#include "Components/ARPGInteractionComponent.h"
#include "Components/ARPGSettlementUIComponent.h"
#include "Settlement/ARPGBuildBedActor.h"
#include "Settlement/ARPGSettlementHubActor.h"
#include "Components/ARPGInventoryComponent.h"
#include "Components/ARPGInventoryUIComponent.h"
#include "Crafting/ARPGCraftingStationActor.h"
#include "Crafting/ARPGStorageActor.h"
#include "GameFramework/PlayerController.h"
#include "EngineUtils.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "TimerManager.h"
#include "UI/ARPGBuildingWidgets.h"


static bool ARPGWindowOccupiesWindowWallHost(const AARPGBuildWindowActor* Window, const AARPGBuildPieceActor* Host)
{
    if (!Window || !Window->Definition || !Host || !Host->Definition) return false;
    if (!Window->IsConstructionComplete() || !Host->IsConstructionComplete()) return false;
    if (Window->Definition->PieceKind != EARPGBuildPieceKind::Window || Host->Definition->PieceKind != EARPGBuildPieceKind::WindowWall) return false;

    const float Tolerance = FMath::Max(1.f, FMath::Max(Window->Definition->PlacementCollisionClearance, Host->Definition->PlacementCollisionClearance) + 0.5f);
    const float ToleranceSq = FMath::Square(Tolerance);
    TArray<FTransform> Candidates;
    Host->GetSnapTransformsFor(Window->Definition, Candidates);
    for (const FTransform& Candidate : Candidates)
    {
        if (FVector::DistSquared(Candidate.GetLocation(), Window->GetActorLocation()) > ToleranceSq) continue;
        const float YawDelta = FMath::Abs(FMath::FindDeltaAngleDegrees(Candidate.Rotator().Yaw, Window->GetActorRotation().Yaw));
        if (YawDelta <= 1.f) return true;
    }
    return false;
}

static AARPGBuildWindowActor* ARPGFindHostedWindowForWindowWall(UWorld* World, const AARPGBuildPieceActor* Host)
{
    if (!World || !Host || !Host->Definition || Host->Definition->PieceKind != EARPGBuildPieceKind::WindowWall) return nullptr;
    for (TActorIterator<AARPGBuildWindowActor> It(World); It; ++It)
    {
        AARPGBuildWindowActor* Window = *It;
        if (ARPGWindowOccupiesWindowWallHost(Window, Host)) return Window;
    }
    return nullptr;
}


static AARPGBuildLightActor* ARPGFindBuildLightFromView(
    UWorld* World,
    const FVector& ViewStart,
    const FVector& ViewEnd,
    ECollisionChannel TraceChannel,
    const AActor* IgnoredActor)
{
    if (!World) return nullptr;

    const FVector Segment = ViewEnd - ViewStart;
    const float SegmentLength = Segment.Size();
    if (SegmentLength <= KINDA_SMALL_NUMBER) return nullptr;
    const FVector Direction = Segment / SegmentLength;

    AARPGBuildLightActor* Best = nullptr;
    float BestScore = TNumericLimits<float>::Max();

    for (TActorIterator<AARPGBuildLightActor> It(World); It; ++It)
    {
        AARPGBuildLightActor* Light = *It;
        if (!Light || !Light->Definition || !Light->IsConstructionComplete() ||
            Light->Definition->PieceKind != EARPGBuildPieceKind::Light)
            continue;

        FVector Center = Light->GetActorLocation();
        FVector Extent = Light->Definition->PlacementBounds;
        if (Light->BuildSkeletalMesh && Light->BuildSkeletalMesh->GetSkeletalMeshAsset())
        {
            Center = Light->BuildSkeletalMesh->Bounds.Origin;
            Extent = Light->BuildSkeletalMesh->Bounds.BoxExtent;
        }
        else if (Light->BuildMesh && Light->BuildMesh->GetStaticMesh())
        {
            Center = Light->BuildMesh->Bounds.Origin;
            Extent = Light->BuildMesh->Bounds.BoxExtent;
        }

        const float Along = FVector::DotProduct(Center - ViewStart, Direction);
        if (Along < 0.f || Along > SegmentLength) continue;
        const FVector Closest = ViewStart + Direction * Along;
        const float LateralSq = FVector::DistSquared(Center, Closest);
        const float VisualAllowance = FMath::Clamp(Extent.GetMax() * 0.35f, 0.f, 80.f);
        const float Radius = FMath::Max(10.f, Light->Definition->LightInteractionRadius + VisualAllowance);
        if (LateralSq > FMath::Square(Radius)) continue;

        // The fixture intentionally has no placement-blocking mesh collision. Prove that the view
        // corridor is not reaching it through an unrelated opaque actor before accepting the semantic
        // interaction candidate. Aim at the nearest point of the fixture's world bounds rather than its
        // centre: wall-mounted art often has its pivot/centre close to the host plane even though the
        // visible lantern/torch projects outward and is plainly reachable from the player's side.
        const FBox VisualBox(Center - Extent, Center + Extent);
        const FVector LOSAimPoint = VisualBox.GetClosestPointTo(ViewStart);
        FCollisionQueryParams Params(SCENE_QUERY_STAT(ARPGBuildLightInteractLOS), false, IgnoredActor);
        FHitResult Obstruction;
        if (World->LineTraceSingleByChannel(Obstruction, ViewStart, LOSAimPoint, TraceChannel, Params))
        {
            if (Obstruction.GetActor() && Obstruction.GetActor() != Light)
            {
                const float HitAlong = FVector::Dist(ViewStart, Obstruction.ImpactPoint);
                const float LightAlong = FVector::Dist(ViewStart, LOSAimPoint);
                if (HitAlong + 2.f < LightAlong) continue;
            }
        }

        const float Score = FMath::Square(Along) + LateralSq * 4.f;
        if (Score < BestScore)
        {
            BestScore = Score;
            Best = Light;
        }
    }

    return Best;
}

UARPGBuildingUIComponent::UARPGBuildingUIComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(false);
    BuildMenuWidgetClass = UARPGBuildMenuWidget::StaticClass();
    BuildPieceRowWidgetClass = UARPGBuildPieceRowWidget::StaticClass();
    PlacementHUDWidgetClass = UARPGBuildPlacementHUDWidget::StaticClass();
    StorageWidgetClass = UARPGStoragePanelWidget::StaticClass();
    CraftingStationWidgetClass = UARPGCraftingStationPanelWidget::StaticClass();
    StructureItemRowWidgetClass = UARPGStructureItemRowWidget::StaticClass();
    StationRecipeRowWidgetClass = UARPGStationRecipeRowWidget::StaticClass();
}

void UARPGBuildingUIComponent::BeginPlay()
{
    Super::BeginPlay();
    BindBuildEvents();
}

void UARPGBuildingUIComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    StopStationProgressTimer();
    UnbindStructureEvents();
    UnbindBuildEvents();
    if (ActiveBuildMenu) ActiveBuildMenu->RemoveFromParent();
    if (ActivePlacementHUD) ActivePlacementHUD->RemoveFromParent();
    if (ActiveStorageWidget) ActiveStorageWidget->RemoveFromParent();
    if (ActiveStationWidget) ActiveStationWidget->RemoveFromParent();
    Super::EndPlay(EndPlayReason);
}

bool UARPGBuildingUIComponent::ResolveLocalPlayer(AARPGCharacter*& OutCharacter, APlayerController*& OutController) const
{
    OutCharacter = Cast<AARPGCharacter>(GetOwner());
    OutController = nullptr;
    if (!OutCharacter || !OutCharacter->IsLocallyControlled() || !GetWorld() || GetWorld()->GetNetMode() == NM_DedicatedServer) return false;
    OutController = Cast<APlayerController>(OutCharacter->GetController());
    return OutController && OutController->IsLocalController();
}

UARPGBuildingComponent* UARPGBuildingUIComponent::GetBuilding() const
{
    if (const AARPGCharacter* Character = Cast<AARPGCharacter>(GetOwner())) return Character->Building;
    return GetOwner() ? GetOwner()->FindComponentByClass<UARPGBuildingComponent>() : nullptr;
}

UARPGInventoryComponent* UARPGBuildingUIComponent::GetPlayerInventory() const
{
    if (const AARPGCharacter* Character = Cast<AARPGCharacter>(GetOwner())) return Character->Inventory;
    return GetOwner() ? GetOwner()->FindComponentByClass<UARPGInventoryComponent>() : nullptr;
}

void UARPGBuildingUIComponent::BindBuildEvents()
{
    if (bRuntimeEventsBound) return;
    if (UARPGBuildingComponent* Building = GetBuilding())
    {
        Building->OnBuildModeChanged.AddUniqueDynamic(this, &UARPGBuildingUIComponent::HandleBuildModeChanged);
        Building->OnBuildPreviewUpdated.AddUniqueDynamic(this, &UARPGBuildingUIComponent::HandleBuildPreviewUpdated);
        bRuntimeEventsBound = true;
    }
}

void UARPGBuildingUIComponent::UnbindBuildEvents()
{
    if (!bRuntimeEventsBound) return;
    if (UARPGBuildingComponent* Building = GetBuilding())
    {
        Building->OnBuildModeChanged.RemoveDynamic(this, &UARPGBuildingUIComponent::HandleBuildModeChanged);
        Building->OnBuildPreviewUpdated.RemoveDynamic(this, &UARPGBuildingUIComponent::HandleBuildPreviewUpdated);
    }
    bRuntimeEventsBound = false;
}

void UARPGBuildingUIComponent::HandleBuildModeChanged(bool bActive, UARPGBuildPieceDefinition* Piece)
{
    AARPGCharacter* Character = nullptr; APlayerController* Controller = nullptr;
    if (!ResolveLocalPlayer(Character, Controller)) return;
    if (bActive) EnsurePlacementHUD(); else RemovePlacementHUD();
    if (ActivePlacementHUD) ActivePlacementHUD->RefreshPlacementHUD();
}

void UARPGBuildingUIComponent::HandleBuildPreviewUpdated(EARPGPlacementResult Result, FTransform PreviewTransform)
{
    if (ActivePlacementHUD) ActivePlacementHUD->RefreshPlacementHUD();
}

void UARPGBuildingUIComponent::EnsurePlacementHUD()
{
    if (ActivePlacementHUD && ActivePlacementHUD->IsInViewport()) return;
    AARPGCharacter* Character = nullptr; APlayerController* Controller = nullptr;
    if (!ResolveLocalPlayer(Character, Controller)) return;
    TSubclassOf<UARPGBuildPlacementHUDWidget> Class = PlacementHUDWidgetClass;
    if (!Class) Class = UARPGBuildPlacementHUDWidget::StaticClass();
    ActivePlacementHUD = CreateWidget<UARPGBuildPlacementHUDWidget>(Controller, Class);
    if (!ActivePlacementHUD) return;
    ActivePlacementHUD->InitializePlacementHUD(Character);
    if (!ActivePlacementHUD->AddToPlayerScreen(PlacementHUDZOrder)) ActivePlacementHUD->AddToViewport(PlacementHUDZOrder);
    ActivePlacementHUD->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
}

void UARPGBuildingUIComponent::RemovePlacementHUD()
{
    if (ActivePlacementHUD) ActivePlacementHUD->RemoveFromParent();
    ActivePlacementHUD = nullptr;
}

void UARPGBuildingUIComponent::ApplyMenuInputMode(APlayerController* Controller)
{
    if (!Controller || !bManageInputMode) return;
    if (!bCursorStateCaptured) { bPreviousMouseCursor = Controller->bShowMouseCursor; bCursorStateCaptured = true; }
    FInputModeGameAndUI Mode;
    Mode.SetHideCursorDuringCapture(false);
    Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    Controller->SetInputMode(Mode);
    if (bShowMouseCursorWhileMenusOpen) Controller->bShowMouseCursor = true;
}

void UARPGBuildingUIComponent::RestoreGameInputMode(APlayerController* Controller)
{
    if (!Controller || !bManageInputMode) return;
    if (bRestoreGameOnlyInputOnClose)
    {
        FInputModeGameOnly Mode;
        Controller->SetInputMode(Mode);
    }
    if (bCursorStateCaptured) Controller->bShowMouseCursor = bPreviousMouseCursor;
    bCursorStateCaptured = false;
}

bool UARPGBuildingUIComponent::OpenBuildMenu()
{
    AARPGCharacter* Character = nullptr; APlayerController* Controller = nullptr;
    if (!ResolveLocalPlayer(Character, Controller)) return false;
    BindBuildEvents();
    CloseStructureUI();
    if (Character->InventoryUI && Character->InventoryUI->IsInventoryUIOpen()) Character->InventoryUI->CloseInventoryUI();
    if (UARPGBuildingComponent* Building = GetBuilding()) if (Building->IsBuildModeActive()) Building->EndBuildMode();
    if (IsBuildMenuOpen()) { ActiveBuildMenu->RefreshBuildMenu(); return true; }

    TSubclassOf<UARPGBuildMenuWidget> Class = BuildMenuWidgetClass;
    if (!Class) Class = UARPGBuildMenuWidget::StaticClass();
    ActiveBuildMenu = CreateWidget<UARPGBuildMenuWidget>(Controller, Class);
    if (!ActiveBuildMenu) return false;
    ActiveBuildMenu->InitializeBuildMenu(Character, this);
    if (!ActiveBuildMenu->AddToPlayerScreen(BuildMenuZOrder)) ActiveBuildMenu->AddToViewport(BuildMenuZOrder);
    CachedPlayerController = Controller;
    ApplyMenuInputMode(Controller);
    return true;
}

bool UARPGBuildingUIComponent::CloseBuildMenu()
{
    const bool bWasOpen = IsBuildMenuOpen();
    if (ActiveBuildMenu) ActiveBuildMenu->RemoveFromParent();
    ActiveBuildMenu = nullptr;
    if (!ActiveStorageWidget && !ActiveStationWidget)
    {
        APlayerController* Controller = CachedPlayerController.Get();
        if (Controller) RestoreGameInputMode(Controller);
    }
    return bWasOpen;
}

bool UARPGBuildingUIComponent::ToggleBuildMenu()
{
    if (IsBuildMenuOpen())
    {
        CloseBuildMenu();
        return true;
    }
    return OpenBuildMenu();
}

bool UARPGBuildingUIComponent::IsBuildMenuOpen() const
{
    return IsValid(ActiveBuildMenu) && ActiveBuildMenu->IsInViewport();
}

bool UARPGBuildingUIComponent::BeginPlacementFromMenu(UARPGBuildPieceDefinition* Piece)
{
    UARPGBuildingComponent* Building = GetBuilding();
    if (!Building || !Piece) return false;
    const bool bStarted = Building->BeginBuildMode(Piece);
    if (bStarted && bCloseBuildMenuWhenPlacementStarts) CloseBuildMenu();
    return bStarted;
}

void UARPGBuildingUIComponent::UnbindStructureEvents()
{
    if (UARPGInventoryComponent* PlayerInventory = GetPlayerInventory()) PlayerInventory->OnInventoryChanged.RemoveDynamic(this, &UARPGBuildingUIComponent::HandlePlayerInventoryChanged);
    if (ActiveStorage && ActiveStorage->Inventory) ActiveStorage->Inventory->OnInventoryChanged.RemoveDynamic(this, &UARPGBuildingUIComponent::HandleStructureInventoryChanged);
    if (ActiveStation)
    {
        if (ActiveStation->Inventory) ActiveStation->Inventory->OnInventoryChanged.RemoveDynamic(this, &UARPGBuildingUIComponent::HandleStructureInventoryChanged);
        if (ActiveStation->OutputInventory) ActiveStation->OutputInventory->OnInventoryChanged.RemoveDynamic(this, &UARPGBuildingUIComponent::HandleStructureInventoryChanged);
        ActiveStation->OnCraftQueueChanged.RemoveDynamic(this, &UARPGBuildingUIComponent::HandleStationQueueChanged);
    }
}

void UARPGBuildingUIComponent::BindStructureEvents()
{
    UnbindStructureEvents();
    if (UARPGInventoryComponent* PlayerInventory = GetPlayerInventory()) PlayerInventory->OnInventoryChanged.AddUniqueDynamic(this, &UARPGBuildingUIComponent::HandlePlayerInventoryChanged);
    if (ActiveStorage && ActiveStorage->Inventory) ActiveStorage->Inventory->OnInventoryChanged.AddUniqueDynamic(this, &UARPGBuildingUIComponent::HandleStructureInventoryChanged);
    if (ActiveStation)
    {
        if (ActiveStation->Inventory) ActiveStation->Inventory->OnInventoryChanged.AddUniqueDynamic(this, &UARPGBuildingUIComponent::HandleStructureInventoryChanged);
        if (ActiveStation->OutputInventory) ActiveStation->OutputInventory->OnInventoryChanged.AddUniqueDynamic(this, &UARPGBuildingUIComponent::HandleStructureInventoryChanged);
        ActiveStation->OnCraftQueueChanged.AddUniqueDynamic(this, &UARPGBuildingUIComponent::HandleStationQueueChanged);
    }
}

bool UARPGBuildingUIComponent::OpenStorageUI(AARPGStorageActor* Storage)
{
    AARPGCharacter* Character = nullptr; APlayerController* Controller = nullptr;
    if (!ResolveLocalPlayer(Character, Controller) || !Storage || !Storage->IsConstructionComplete() || !Storage->CanActorAccess(Character)) return false;
    if (FVector::DistSquared(Character->GetActorLocation(), Storage->GetActorLocation()) > FMath::Square(StructureInteractionDistance)) return false;
    if (Cast<AARPGCraftingStationActor>(Storage)) return OpenCraftingStationUI(Cast<AARPGCraftingStationActor>(Storage));
    CloseBuildMenu(); CloseStructureUI();
    if (Character->InventoryUI && Character->InventoryUI->IsInventoryUIOpen()) Character->InventoryUI->CloseInventoryUI();
    if (UARPGBuildingComponent* Building = GetBuilding()) if (Building->IsBuildModeActive()) Building->EndBuildMode();

    ActiveStorage = Storage;
    TSubclassOf<UARPGStoragePanelWidget> Class = StorageWidgetClass;
    if (!Class) Class = UARPGStoragePanelWidget::StaticClass();
    ActiveStorageWidget = CreateWidget<UARPGStoragePanelWidget>(Controller, Class);
    if (!ActiveStorageWidget) { ActiveStorage = nullptr; return false; }
    ActiveStorageWidget->InitializeStorageUI(Character, Storage, this);
    if (!ActiveStorageWidget->AddToPlayerScreen(StructureUIZOrder)) ActiveStorageWidget->AddToViewport(StructureUIZOrder);
    CachedPlayerController = Controller;
    BindStructureEvents();
    ApplyMenuInputMode(Controller);
    return true;
}

bool UARPGBuildingUIComponent::OpenCraftingStationUI(AARPGCraftingStationActor* Station)
{
    AARPGCharacter* Character = nullptr; APlayerController* Controller = nullptr;
    if (!ResolveLocalPlayer(Character, Controller) || !Station || !Station->IsConstructionComplete() || !Station->CanActorAccess(Character)) return false;
    if (FVector::DistSquared(Character->GetActorLocation(), Station->GetActorLocation()) > FMath::Square(StructureInteractionDistance)) return false;
    CloseBuildMenu(); CloseStructureUI();
    if (Character->InventoryUI && Character->InventoryUI->IsInventoryUIOpen()) Character->InventoryUI->CloseInventoryUI();
    if (UARPGBuildingComponent* Building = GetBuilding()) if (Building->IsBuildModeActive()) Building->EndBuildMode();

    ActiveStation = Station;
    ActiveStorage = Station;
    TSubclassOf<UARPGCraftingStationPanelWidget> Class = CraftingStationWidgetClass;
    if (!Class) Class = UARPGCraftingStationPanelWidget::StaticClass();
    ActiveStationWidget = CreateWidget<UARPGCraftingStationPanelWidget>(Controller, Class);
    if (!ActiveStationWidget) { ActiveStation = nullptr; ActiveStorage = nullptr; return false; }
    ActiveStationWidget->InitializeStationUI(Character, Station, this);
    if (!ActiveStationWidget->AddToPlayerScreen(StructureUIZOrder)) ActiveStationWidget->AddToViewport(StructureUIZOrder);
    CachedPlayerController = Controller;
    BindStructureEvents();
    StartStationProgressTimer();
    ApplyMenuInputMode(Controller);
    return true;
}

bool UARPGBuildingUIComponent::CloseStructureUI()
{
    const bool bHad = IsValid(ActiveStorageWidget) || IsValid(ActiveStationWidget);
    StopStationProgressTimer();
    UnbindStructureEvents();
    if (ActiveStorageWidget) ActiveStorageWidget->RemoveFromParent();
    if (ActiveStationWidget) ActiveStationWidget->RemoveFromParent();
    ActiveStorageWidget = nullptr;
    ActiveStationWidget = nullptr;
    ActiveStorage = nullptr;
    ActiveStation = nullptr;
    if (!IsBuildMenuOpen())
    {
        if (APlayerController* Controller = CachedPlayerController.Get()) RestoreGameInputMode(Controller);
    }
    return bHad;
}

bool UARPGBuildingUIComponent::InteractWithBuiltStructureFromView()
{
    AARPGCharacter* Character = nullptr; APlayerController* Controller = nullptr;
    if (!ResolveLocalPlayer(Character, Controller) || !GetWorld()) return false;
    FVector Start; FRotator Rotation; Controller->GetPlayerViewPoint(Start, Rotation);
    const FVector End = Start + Rotation.Vector() * StructureInteractionDistance;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(ARPGStructureInteract), false, Character);

    auto HandleActor = [&](AActor* Actor) -> bool
    {
        if (AARPGBuildDoorActor* Door = Cast<AARPGBuildDoorActor>(Actor))
        {
            if (Character->Interaction) { Character->Interaction->ToggleBuiltDoor(Door); return true; }
            return false;
        }
        if (AARPGBuildWindowActor* Window = Cast<AARPGBuildWindowActor>(Actor))
        {
            if (Character->Interaction) { Character->Interaction->ToggleBuiltWindow(Window); return true; }
            return false;
        }
        if (AARPGBuildLightActor* Light = Cast<AARPGBuildLightActor>(Actor))
        {
            if (Character->Interaction) { Character->Interaction->ToggleBuiltLight(Light); return true; }
            return false;
        }
        // Settlement Hub derives Storage, so resolve it before the generic container branch. Beds and
        // Hubs use the character's dedicated local Settlement UI component; authority remains in the
        // existing player-owned Interaction component.
        if (AARPGSettlementHubActor* Hub = Cast<AARPGSettlementHubActor>(Actor))
            return Character->SettlementUI ? Character->SettlementUI->OpenSettlementPanel(Hub) : false;
        if (AARPGBuildBedActor* Bed = Cast<AARPGBuildBedActor>(Actor))
            return Character->SettlementUI ? Character->SettlementUI->OpenBedPanel(Bed) : false;
        if (AARPGCraftingStationActor* Station = Cast<AARPGCraftingStationActor>(Actor)) return OpenCraftingStationUI(Station);
        if (AARPGStorageActor* Storage = Cast<AARPGStorageActor>(Actor)) return OpenStorageUI(Storage);
        return false;
    };

    auto HandleHitOrHostedWindow = [&](AActor* HitActor) -> bool
    {
        if (!HitActor) return false;
        if (HandleActor(HitActor)) return true;

        // WindowWall art frequently uses conservative/simple collision that covers the visual aperture.
        // In that case the view trace correctly hits the structural host before it can ever reach the
        // hosted Window collider. Resolve only the Window occupying THIS exact native host socket; do
        // not multi-trace through unrelated walls or arbitrary blockers.
        if (AARPGBuildPieceActor* Host = Cast<AARPGBuildPieceActor>(HitActor))
        {
            if (Host->Definition && Host->Definition->PieceKind == EARPGBuildPieceKind::WindowWall)
                if (AARPGBuildWindowActor* HostedWindow = ARPGFindHostedWindowForWindowWall(GetWorld(), Host))
                    return HandleActor(HostedWindow);
        }
        return false;
    };

    // Buildable lights deliberately do not block placement or Pawn movement, so they are acquired
    // semantically from the same view ray before the normal blocking structure trace.
    if (AARPGBuildLightActor* Light = ARPGFindBuildLightFromView(GetWorld(), Start, End, StructureInteractionTraceChannel, Character))
        if (HandleActor(Light)) return true;

    FHitResult Hit;
    if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, StructureInteractionTraceChannel, Params) && HandleHitOrHostedWindow(Hit.GetActor())) return true;

    // Native Windows keep a dedicated Visibility-only interaction target while open. If a project has
    // changed StructureInteractionTraceChannel, retain plug-and-play Window toggling through this narrow fallback.
    if (StructureInteractionTraceChannel != ECC_Visibility)
    {
        FHitResult VisibilityHit;
        if (GetWorld()->LineTraceSingleByChannel(VisibilityHit, Start, End, ECC_Visibility, Params) && HandleHitOrHostedWindow(VisibilityHit.GetActor()))
            return true;
    }
    return false;
}

bool UARPGBuildingUIComponent::DemolishBuiltStructureFromView()
{
    AARPGCharacter* Character = nullptr; APlayerController* Controller = nullptr;
    if (!ResolveLocalPlayer(Character, Controller) || !GetWorld() || !Character->Interaction) return false;
    FVector Start; FRotator Rotation; Controller->GetPlayerViewPoint(Start, Rotation);
    const FVector End = Start + Rotation.Vector() * StructureInteractionDistance;

    // Buildable lights intentionally keep their imported fixture mesh collision disabled so they can
    // never become hidden Wall/Stair/Floor placement blockers. Reuse the same bounded semantic view
    // acquisition as interaction so those non-blocking fixtures remain fully demolishable.
    if (AARPGBuildLightActor* Light = ARPGFindBuildLightFromView(
        GetWorld(), Start, End, StructureInteractionTraceChannel, Character))
    {
        if (Light->bRuntimePlaced && Light->CanActorModify(Character))
        {
            Character->Interaction->DemolishBuilding(Light);
            return true;
        }
    }

    FHitResult Hit;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(ARPGStructureDemolish), false, Character);
    if (!GetWorld()->LineTraceSingleByChannel(Hit, Start, End, StructureInteractionTraceChannel, Params)) return false;
    AARPGBuildPieceActor* BuildingActor = Cast<AARPGBuildPieceActor>(Hit.GetActor());
    if (!BuildingActor || !BuildingActor->bRuntimePlaced || !BuildingActor->CanActorModify(Character)) return false;
    Character->Interaction->DemolishBuilding(BuildingActor);
    return true;
}

void UARPGBuildingUIComponent::HandlePlayerInventoryChanged() { RefreshOpenStructureUI(); if (ActiveBuildMenu) ActiveBuildMenu->RefreshBuildMenu(); }
void UARPGBuildingUIComponent::HandleStructureInventoryChanged() { RefreshOpenStructureUI(); }
void UARPGBuildingUIComponent::HandleStationQueueChanged() { if (ActiveStationWidget) ActiveStationWidget->RefreshStationUI(); }

void UARPGBuildingUIComponent::RefreshOpenStructureUI()
{
    if (ActiveStorageWidget) ActiveStorageWidget->RefreshStorageUI();
    if (ActiveStationWidget) ActiveStationWidget->RefreshStationUI();
}

void UARPGBuildingUIComponent::StartStationProgressTimer()
{
    StopStationProgressTimer();
    if (GetWorld()) GetWorld()->GetTimerManager().SetTimer(StationProgressTimer, this, &UARPGBuildingUIComponent::RefreshStationProgress, 0.1f, true);
}
void UARPGBuildingUIComponent::StopStationProgressTimer()
{
    if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(StationProgressTimer);
}
void UARPGBuildingUIComponent::RefreshStationProgress()
{
    if (ActiveStationWidget) ActiveStationWidget->RefreshStationProgress(); else StopStationProgressTimer();
}
