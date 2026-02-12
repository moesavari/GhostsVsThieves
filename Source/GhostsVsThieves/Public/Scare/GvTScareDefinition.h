#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "GvTScareDefinition.generated.h"

UCLASS(BlueprintType)
class GHOSTSVSTHIEVES_API UGvTScareDefinition : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scare")
	FGameplayTag ScareTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scare", meta = (ClampMin = "0.0"))
	float BaseWeight = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scare", meta = (ClampMin = "0", ClampMax = "4"))
	uint8 MinPanicTier = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scare", meta = (ClampMin = "0", ClampMax = "4"))
	uint8 MaxPanicTier = 4;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scare", meta = (ClampMin = "0.0"))
	float MinCooldownAfter = 6.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scare", meta = (ClampMin = "0.0"))
	float MaxCooldownAfter = 18.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scare", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float IntensityMin01 = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scare", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float IntensityMax01 = 0.8f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scare")
	FGameplayTagContainer RequiredContexts;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scare")
	FGameplayTagContainer BlockedContexts;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scare")
	bool bAllowDuringSafetyWindow = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scare", meta = (ClampMin = "0.0"))
	float RepeatPenaltySeconds = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scare", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float RepeatPenaltyMultiplier = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scare")
	bool bIsGroupEligible = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scare")
	FGameplayTagContainer BoostContexts;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scare", meta = (ClampMin = "0.0"))
	float BoostMultiplier = 1.0f;
};
