#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GvTGhostTypeData.generated.h"

UCLASS(BlueprintType)
class GHOSTSVSTHIEVES_API UGvTGhostTypeData : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GvT|Ghost|Type")
	FName TypeId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GvT|Ghost|Type")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GvT|Ghost|Type")
	float NoiseInterestMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GvT|Ghost|Type")
	float ElectricalInterestMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GvT|Ghost|Type")
	float LightInterestMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GvT|Ghost|Type")
	float ValuableItemInterestMultiplier = 1.0f;
};
