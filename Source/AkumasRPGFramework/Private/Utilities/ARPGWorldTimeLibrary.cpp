#include "Utilities/ARPGWorldTimeLibrary.h"

#include "Engine/Engine.h"
#include "EngineUtils.h"

AARPGDayNightCycle* UARPGWorldTimeLibrary::GetDayNightCycle(const UObject* WorldContextObject)
{
    if (!GEngine || !WorldContextObject) return nullptr;
    UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull);
    if (!World) return nullptr;

    for (TActorIterator<AARPGDayNightCycle> It(World); It; ++It)
    {
        return *It;
    }
    return nullptr;
}

bool UARPGWorldTimeLibrary::IsDay(const UObject* WorldContextObject)
{
    if (const AARPGDayNightCycle* Cycle = GetDayNightCycle(WorldContextObject)) return Cycle->IsDay();
    const int32 Hour = FDateTime::Now().GetHour();
    return Hour >= 6 && Hour < 18;
}

bool UARPGWorldTimeLibrary::IsNight(const UObject* WorldContextObject)
{
    if (const AARPGDayNightCycle* Cycle = GetDayNightCycle(WorldContextObject)) return Cycle->IsNight();
    return !IsDay(WorldContextObject);
}

float UARPGWorldTimeLibrary::GetWorldHour(const UObject* WorldContextObject)
{
    if (const AARPGDayNightCycle* Cycle = GetDayNightCycle(WorldContextObject)) return Cycle->GetWorldHour();
    const FDateTime Now = FDateTime::Now();
    return static_cast<float>(Now.GetHour()) + static_cast<float>(Now.GetMinute()) / 60.f + static_cast<float>(Now.GetSecond()) / 3600.f;
}

FDateTime UARPGWorldTimeLibrary::GetWorldDateTime(const UObject* WorldContextObject)
{
    if (const AARPGDayNightCycle* Cycle = GetDayNightCycle(WorldContextObject)) return Cycle->GetWorldDateTime();
    return FDateTime::Now();
}

EARPGDayNightPhase UARPGWorldTimeLibrary::GetDayNightPhase(const UObject* WorldContextObject)
{
    if (const AARPGDayNightCycle* Cycle = GetDayNightCycle(WorldContextObject)) return Cycle->GetDayNightPhase();
    const float Hour = GetWorldHour(WorldContextObject);
    if (Hour >= 5.f && Hour < 6.f) return EARPGDayNightPhase::Dawn;
    if (Hour >= 6.f && Hour < 17.f) return EARPGDayNightPhase::Day;
    if (Hour >= 17.f && Hour < 18.f) return EARPGDayNightPhase::Dusk;
    return EARPGDayNightPhase::Night;
}

float UARPGWorldTimeLibrary::GetDaylightAmount(const UObject* WorldContextObject)
{
    if (const AARPGDayNightCycle* Cycle = GetDayNightCycle(WorldContextObject)) return Cycle->GetDaylightAmount();
    const float Hour = GetWorldHour(WorldContextObject);
    if (Hour <= 6.f || Hour >= 18.f) return 0.f;
    const float Alpha = (Hour - 6.f) / 12.f;
    return FMath::SmoothStep(0.f, 0.20f, FMath::Max(0.f, FMath::Sin(Alpha * PI)));
}
