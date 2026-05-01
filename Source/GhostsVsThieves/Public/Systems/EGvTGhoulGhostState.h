#pragma once

#include "CoreMinimal.h"
#include "EGvTGhoulGhostState.generated.h"

UENUM(BlueprintType)
enum class EGvTGhoulGhostState : uint8
{
	Idle		UMETA(DisplayName = "Idle"),
	Walk		UMETA(DisplayName = "Walk"),
	ChaseRun	UMETA(DisplayName = "ChaseRun"),
	Scream		UMETA(DisplayName = "Scream"),
	StandUp		UMETA(DisplayName = "StandUp"),
	Vanish		UMETA(DisplayName = "Vanish"),
};