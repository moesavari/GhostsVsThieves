#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GvTScareTypes.generated.h"

USTRUCT(BlueprintType)
struct FGvTScareEvent
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	FGameplayTag ScareTag;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	float Intensity01 = 0.5f;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	float Duration = 1.0f;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	FVector WorldHint = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	int32 LocalSeed = 0;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	bool bIsGroupScare = false;
};

USTRUCT(BlueprintType)
struct FGvTScareState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	int32 Seed = 0;

	UPROPERTY(BlueprintReadOnly)
	float NextScareServerTime = 0.f;

	UPROPERTY(BlueprintReadOnly)
	int32 CooldownStacks = 0;

	UPROPERTY(BlueprintReadOnly)
	float SafetyWindowEndTime = 0.f;

	UPROPERTY(BlueprintReadOnly)
	float LastScareServerTime = 0.f;

	UPROPERTY(BlueprintReadOnly)
	uint8 LastPanicTier = 0;
};
