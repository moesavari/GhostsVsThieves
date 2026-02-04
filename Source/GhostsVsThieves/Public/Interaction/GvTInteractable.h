#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GvTInteractable.generated.h"

UINTERFACE(BlueprintType)
class GHOSTSVSTHIEVES_API UGvTInteractable : public UInterface
{
	GENERATED_BODY()
};

class GHOSTSVSTHIEVES_API IGvTInteractable
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "GvT|Interactable")
	void Interact(APawn* InstigatorPawn);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "GvT|Interactable")
	void Photo(APawn* InstigatorPawn);
};
