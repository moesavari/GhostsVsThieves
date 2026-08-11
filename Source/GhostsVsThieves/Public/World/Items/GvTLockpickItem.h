#pragma once

#include "CoreMinimal.h"
#include "World/Items/GvTInteractableItem.h"
#include "GvTLockpickItem.generated.h"

/** Marker/tool item. Basic lockpicks are reusable and are never consumed. */
UCLASS()
class GHOSTSVSTHIEVES_API AGvTLockpickItem : public AGvTInteractableItem
{
	GENERATED_BODY()

public:
	AGvTLockpickItem();
};
