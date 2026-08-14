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
                if (Last.IsValid())
                {
                    // Logged-in accounts and the local Guest profile both use their stable indexed identity.
                    Character->CharacterId = Last;
                }
                else if (!Accounts->IsLoggedIn())
                {
                    // Legacy Guest saves (pre-v2.15.12) have no GuestCharacterId index. Give the GameMode
                    // world-loader one frame to recover the sole local Guest owner from the world save. This
                    // removes BeginPlay ordering as a source of ownership mismatch. If no legacy world exists,
                    // the second attempt simply registers the freshly generated character as the new Guest.
                    if (!bDeferredGuestIdentityRecoveryOnce && GetWorld())
                    {
                        bDeferredGuestIdentityRecoveryOnce = true;
                        GetWorld()->GetTimerManager().SetTimerForNextTick(this, &UARPGPersistenceComponent::AttemptAutoLoad);
                        return;
                    }
                    Character->EnsureCharacterId();
                    Accounts->RegisterCharacterId(Character->CharacterId);
                }
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
