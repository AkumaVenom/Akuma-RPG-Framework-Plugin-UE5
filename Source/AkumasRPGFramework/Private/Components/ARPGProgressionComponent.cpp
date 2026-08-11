#include "Components/ARPGProgressionComponent.h"
#include "Net/UnrealNetwork.h"

UARPGProgressionComponent::UARPGProgressionComponent()
{
    SetIsReplicatedByDefault(true);
}

int64 UARPGProgressionComponent::GetXPRequiredForLevel(int32 InLevel) const
{
    return FMath::Max<int64>(1, FMath::RoundToInt64(BaseXP * FMath::Pow(FMath::Max(1, InLevel), XPExponent)));
}

void UARPGProgressionComponent::AddXP(int64 Amount)
{
    if (!GetOwner() || !GetOwner()->HasAuthority() || Amount <= 0 || Level >= MaxLevel) return;
    const int64 OldXP = XP;
    const int32 OldLevel = Level;
    XP += Amount;
    while (Level < MaxLevel)
    {
        const int64 Needed = GetXPRequiredForLevel(Level);
        if (XP < Needed) break;
        XP -= Needed;
        ++Level;
    }
    if (Level >= MaxLevel) XP = 0;
    OnXPChanged.Broadcast(OldXP, XP);
    if (OldLevel != Level) OnLevelChanged.Broadcast(OldLevel, Level);
}

float UARPGProgressionComponent::GetLevelProgress01() const
{
    if (Level >= MaxLevel) return 1.f;
    return FMath::Clamp(static_cast<float>(XP) / static_cast<float>(GetXPRequiredForLevel(Level)), 0.f, 1.f);
}

void UARPGProgressionComponent::SetProgression(int32 NewLevel, int64 NewXP)
{
    if (!GetOwner() || !GetOwner()->HasAuthority()) return;
    const int32 OldLevel = Level;
    const int64 OldXP = XP;
    Level = FMath::Clamp(NewLevel, 1, MaxLevel);
    XP = FMath::Max<int64>(0, NewXP);

    // SetProgression is used by save restore and is also exposed to authoritative gameplay code. Keep
    // it event-correct so systems such as JRPG stats do not silently miss a direct level change.
    if (OldXP != XP) OnXPChanged.Broadcast(OldXP, XP);
    if (OldLevel != Level) OnLevelChanged.Broadcast(OldLevel, Level);
}

void UARPGProgressionComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(UARPGProgressionComponent, Level);
    DOREPLIFETIME(UARPGProgressionComponent, XP);
}
