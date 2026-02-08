#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GvTHUDWidget.generated.h"

UCLASS()
class GHOSTSVSTHIEVES_API UGvTHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintImplementableEvent)
	void SetLootValue(int32 Value);

	UFUNCTION()
	void HandleLootChanged(int32 NewLoot);
};
