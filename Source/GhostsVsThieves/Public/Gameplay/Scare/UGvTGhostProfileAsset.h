#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "UGvTGhostProfileAsset.generated.h"

UCLASS(BlueprintType)
class GHOSTSVSTHIEVES_API UGvTGhostProfileAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ghost")
	FGameplayTag GhostTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ghost", meta = (ClampMin = "0.1"))
	float FrequencyMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ghost", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SafetyWindowChance = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ghost")
	FVector2D SafetyWindowRangeSeconds = FVector2D(10.f, 25.f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ghost", meta = (ClampMin = "0.0"))
	float DeathRippleAggression = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ghost")
	TMap<FGameplayTag, float> TagMultipliers;
};
