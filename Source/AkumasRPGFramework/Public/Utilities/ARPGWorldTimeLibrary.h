#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "World/ARPGDayNightCycle.h"
#include "ARPGWorldTimeLibrary.generated.h"

/** Global green Blueprint nodes for the active ARPG Day Night Cycle actor in the current world. */
UCLASS()
class AKUMASRPGFRAMEWORK_API UARPGWorldTimeLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintPure, Category="ARPG|World Time", meta=(WorldContext="WorldContextObject", DisplayName="Get Day Night Cycle"))
    static AARPGDayNightCycle* GetDayNightCycle(const UObject* WorldContextObject);

    UFUNCTION(BlueprintPure, Category="ARPG|World Time", meta=(WorldContext="WorldContextObject", DisplayName="Is Day"))
    static bool IsDay(const UObject* WorldContextObject);

    UFUNCTION(BlueprintPure, Category="ARPG|World Time", meta=(WorldContext="WorldContextObject", DisplayName="Is Night"))
    static bool IsNight(const UObject* WorldContextObject);

    UFUNCTION(BlueprintPure, Category="ARPG|World Time", meta=(WorldContext="WorldContextObject", DisplayName="Get World Hour"))
    static float GetWorldHour(const UObject* WorldContextObject);

    UFUNCTION(BlueprintPure, Category="ARPG|World Time", meta=(WorldContext="WorldContextObject", DisplayName="Get World Date Time"))
    static FDateTime GetWorldDateTime(const UObject* WorldContextObject);

    UFUNCTION(BlueprintPure, Category="ARPG|World Time", meta=(WorldContext="WorldContextObject", DisplayName="Get Day Night Phase"))
    static EARPGDayNightPhase GetDayNightPhase(const UObject* WorldContextObject);

    UFUNCTION(BlueprintPure, Category="ARPG|World Time", meta=(WorldContext="WorldContextObject", DisplayName="Get Daylight Amount"))
    static float GetDaylightAmount(const UObject* WorldContextObject);
};
