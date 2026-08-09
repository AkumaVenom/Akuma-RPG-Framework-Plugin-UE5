#include "Components/ARPGStatsComponent.h"
#include "Net/UnrealNetwork.h"

UARPGStatsComponent::UARPGStatsComponent()
{
    SetIsReplicatedByDefault(true);
}

bool UARPGStatsComponent::ApplyDamage(float Amount)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || Amount <= 0.f || Health <= 0.f) return false;
    const float Old = Health;
    Health = FMath::Clamp(Health - Amount, 0.f, MaxHealth);
    OnHealthChanged.Broadcast(Health, Health - Old);
    if (Health <= 0.f) OnDeath.Broadcast();
    return true;
}

bool UARPGStatsComponent::Heal(float Amount)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || Amount <= 0.f || Health <= 0.f) return false;
    const float Old = Health;
    Health = FMath::Clamp(Health + Amount, 0.f, MaxHealth);
    OnHealthChanged.Broadcast(Health, Health - Old);
    return true;
}

bool UARPGStatsComponent::SpendMana(float Amount)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || Amount < 0.f || Mana < Amount) return false;
    Mana -= Amount;
    return true;
}

void UARPGStatsComponent::RestoreAllVitals()
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;
    const float Old = Health;
    Health = MaxHealth;
    Mana = MaxMana;
    Stamina = MaxStamina;
    OnHealthChanged.Broadcast(Health, Health - Old);
}

float UARPGStatsComponent::GetHealthPercent() const
{
    return MaxHealth > 0.f ? Health / MaxHealth : 0.f;
}

void UARPGStatsComponent::OnRep_Health(float OldHealth)
{
    OnHealthChanged.Broadcast(Health, Health - OldHealth);
    if (OldHealth > 0.f && Health <= 0.f) OnDeath.Broadcast();
}

void UARPGStatsComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(UARPGStatsComponent, Health);
    DOREPLIFETIME(UARPGStatsComponent, MaxHealth);
    DOREPLIFETIME(UARPGStatsComponent, Mana);
    DOREPLIFETIME(UARPGStatsComponent, MaxMana);
    DOREPLIFETIME(UARPGStatsComponent, Stamina);
    DOREPLIFETIME(UARPGStatsComponent, MaxStamina);
    DOREPLIFETIME(UARPGStatsComponent, Armor);
    DOREPLIFETIME(UARPGStatsComponent, AttackPower);
    DOREPLIFETIME(UARPGStatsComponent, SpellPower);
}
