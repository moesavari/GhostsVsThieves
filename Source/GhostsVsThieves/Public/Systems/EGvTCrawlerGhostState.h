#pragma once

#include "CoreMinimal.h"
#include "EGvTCrawlerGhostState.generated.h"

UENUM(BlueprintType)
enum class EGvTCrawlerGhostState : uint8
{
    IdleCeiling UMETA(DisplayName = "IdleCeiling"),
    DropScare   UMETA(DisplayName = "DropScare"),
    HauntChase  UMETA(DisplayName = "HauntChase"),
    Vanish      UMETA(DisplayName = "Vanish"),
};