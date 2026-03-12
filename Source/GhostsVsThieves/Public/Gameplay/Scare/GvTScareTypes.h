#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GameFramework/Actor.h"
#include "GvTScareTypes.generated.h"

USTRUCT(BlueprintType)
struct FGvTScareEvent
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Scare")
	FGameplayTag ScareTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Scare")
	float Intensity01 = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Scare")
	float Duration = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Scare")
	FVector WorldHint = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Scare")
	int32 LocalSeed = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Scare")
	bool bIsGroupScare = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Scare")
	TObjectPtr<AActor> TargetActor = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Scare")
	bool bTriggerLocalFlicker = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Scare")
	bool bTriggerGroupFlicker = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Scare")
	bool bAffectsPanic = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Scare")
	float PanicAmount = 0.f;
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