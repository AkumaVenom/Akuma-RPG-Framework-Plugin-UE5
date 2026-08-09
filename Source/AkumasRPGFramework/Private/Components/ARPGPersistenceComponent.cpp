#include "Components/ARPGPersistenceComponent.h"
#include "Subsystems/ARPGSaveSubsystem.h"
#include "Subsystems/ARPGAccountSubsystem.h"
#include "Actors/ARPGCharacter.h"
#include "Engine/GameInstance.h"
#include "TimerManager.h"

UARPGPersistenceComponent::UARPGPersistenceComponent() { PrimaryComponentTick.bCanEverTick = false; }

void UARPGPersistenceComponent::BeginPlay()
{
    Super::BeginPlay();
    if (!GetOwner() || (GetOwner()->GetNetMode() == NM_Client && !GetOwner()->HasAuthority())) return;
    if (bAutoLoadOnBeginPlay && GetWorld()) GetWorld()->GetTimerManager().SetTimerForNextTick(this, &UARPGPersistenceComponent::AttemptAutoLoad);
    if (bAutoSave && AutoSaveIntervalSeconds > 0.f && GetWorld()) GetWorld()->GetTimerManager().SetTimer(AutoSaveTimer, this, &UARPGPersistenceComponent::HandleAutoSave, AutoSaveIntervalSeconds, true);
}

void UARPGPersistenceComponent::AttemptAutoLoad()
{
    if (AARPGCharacter* Character = Cast<AARPGCharacter>(GetOwner()))
    {
        if (UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr)
        {
            if (UARPGAccountSubsystem* Accounts = GI->GetSubsystem<UARPGAccountSubsystem>())
            {
                const FGuid Last = Accounts->GetLastCharacterId();
                if (Last.IsValid() && Accounts->IsLoggedIn()) Character->CharacterId = Last;
            }
        }
    }
    LoadNow();
}

void UARPGPersistenceComponent::HandleAutoSave() { SaveNow(); }

bool UARPGPersistenceComponent::SaveNow()
{
    UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr; UARPGSaveSubsystem* Saves = GI ? GI->GetSubsystem<UARPGSaveSubsystem>() : nullptr;
    return Saves && GetOwner() ? Saves->SaveCharacter(GetOwner()) : false;
}

bool UARPGPersistenceComponent::LoadNow()
{
    UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr; UARPGSaveSubsystem* Saves = GI ? GI->GetSubsystem<UARPGSaveSubsystem>() : nullptr;
    return Saves && GetOwner() ? Saves->LoadCharacter(GetOwner()) : false;
}

void UARPGPersistenceComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (bSaveOnEndPlay && EndPlayReason != EEndPlayReason::Destroyed) SaveNow();
    Super::EndPlay(EndPlayReason);
}
