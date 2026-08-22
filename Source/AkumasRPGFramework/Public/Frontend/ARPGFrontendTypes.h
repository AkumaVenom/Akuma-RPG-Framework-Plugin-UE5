#pragma once

#include "CoreMinimal.h"
#include "ARPGFrontendTypes.generated.h"

UENUM(BlueprintType)
enum class EARPGFrontendPlayMode : uint8
{
    None UMETA(DisplayName="None"),
    SinglePlayer UMETA(DisplayName="Single Player"),
    ListenHost UMETA(DisplayName="Host & Play"),
    DirectIPClient UMETA(DisplayName="Join by IP")
};

UENUM(BlueprintType)
enum class EARPGNetworkConnectionState : uint8
{
    Idle UMETA(DisplayName="Idle"),
    OpeningSinglePlayer UMETA(DisplayName="Opening Single Player"),
    StartingListenHost UMETA(DisplayName="Starting Listen Host"),
    Connecting UMETA(DisplayName="Connecting"),
    AwaitingProfileHandshake UMETA(DisplayName="Awaiting Profile Handshake"),
    Connected UMETA(DisplayName="Connected"),
    Failed UMETA(DisplayName="Failed"),
    ReturningToMenu UMETA(DisplayName="Returning To Menu")
};

USTRUCT(BlueprintType)
struct AKUMASRPGFRAMEWORK_API FARPGFrontendSessionSettings
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ARPG|Frontend") FName GameplayMap = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ARPG|Frontend") FName MainMenuMap = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ARPG|Frontend", meta=(ClampMin="1", ClampMax="65535")) int32 ListenPort = 7777;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ARPG|Frontend") bool bLAN = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ARPG|Frontend") FString JoinAddress;
};
