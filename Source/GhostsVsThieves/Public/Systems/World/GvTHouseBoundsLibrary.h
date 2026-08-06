#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GvTHouseBoundsLibrary.generated.h"

UCLASS()
class GHOSTSVSTHIEVES_API UGvTHouseBoundsLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /** Returns true when Location is inside any valid HouseBounds volume. Fails open when none exist. */
    UFUNCTION(BlueprintPure, Category="GvT|House Bounds", meta=(WorldContext="WorldContextObject"))
    static bool IsLocationInsideHouse(const UObject* WorldContextObject, const FVector& Location, bool& bFoundValidBounds);
};
