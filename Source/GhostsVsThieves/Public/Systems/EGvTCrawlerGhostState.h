#pragma once

#include "CoreMinimal.h"
#include "EGvTCrawlerGhostState.generated.h"

UENUM(BlueprintType)
enum class EGvTCrawlerGhostState : uint8
{
    IdleCeiling   UMETA(DisplayName = "IdleCeiling"),
    OverheadScare UMETA(DisplayName = "OverheadScare"),
    HauntChase    UMETA(DisplayName = "HauntChase"),
    Vanish        UMETA(DisplayName = "Vanish"),
};