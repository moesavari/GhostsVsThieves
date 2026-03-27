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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Scare|LightChase")
	int32 LightChaseStepCount = 5;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Scare|LightChase", meta = (ClampMin = "0.01"))
	float LightChaseStepInterval = 0.16f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Scare|LightChase", meta = (ClampMin = "0.0"))
	float LightChaseStartDistance = 1800.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Scare|LightChase", meta = (ClampMin = "0.0"))
	float LightChaseEndDistance = 120.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Scare|LightChase", meta = (ClampMin = "0.0"))
	float LightChaseFlickerRadius = 350.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Scare|LightChase", meta = (ClampMin = "0.0"))
	float LightChaseAudioLeadDistance = 80.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Scare|Audio", meta = (ClampMin = "0.0"))
	float SharedAudioRadius = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Scare|Audio")
	bool bTwoShotAudio = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Scare|Audio", meta = (ClampMin = "0.0"))
	float FollowupDelay = 0.18f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Scare|Audio", meta = (ClampMin = "0.0"))
	float RearAudioBackOffset = 180.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Scare|Audio", meta = (ClampMin = "0.0"))
	float RearAudioSideOffset = 110.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Scare|Audio")
	float RearAudioUpOffset = -10.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Scare")
	TObjectPtr<AActor> SourceActor = nullptr;
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