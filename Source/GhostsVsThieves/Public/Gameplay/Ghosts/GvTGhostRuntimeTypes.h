#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GvTGhostRuntimeTypes.generated.h"

class UGvTGhostModelData;
class UGvTGhostTypeData;

UENUM(BlueprintType)
enum class EGvTGhostBehaviorMode : uint8
{
	Idle,
	Roam,
	Investigate,
	Hunt,
	Chase,
	Scare,
	Recover
};

USTRUCT(BlueprintType)
struct FGvTGhostRuntimeConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GvT|Ghost|Runtime")
	float WalkSpeed = 140.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GvT|Ghost|Runtime")
	float RunSpeed = 350.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GvT|Ghost|Runtime")
	float Acceleration = 2048.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GvT|Ghost|Runtime")
	float BrakingDecel = 2048.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GvT|Ghost|Runtime")
	float NoiseInterestMultiplier = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GvT|Ghost|Runtime")
	float ElectricalInterestMultiplier = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GvT|Ghost|Runtime")
	float HuntAggressionMultiplier = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GvT|Ghost|Runtime")
	float RoamChanceMultiplier = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GvT|Ghost|Runtime")
	float ScareFrequencyMultiplier = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GvT|Ghost|Runtime")
	float LightInterestMultiplier = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GvT|Ghost|Runtime")
	float ValuableItemInterestMultiplier = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GvT|Ghost|Runtime")
	float MedicalItemInterestMultiplier = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GvT|Ghost|Runtime")
	float PanicBiasMultiplier = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GvT|Ghost|Runtime")
	float IsolationBiasMultiplier = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GvT|Ghost|Runtime")
	float RecentHauntPressureBiasMultiplier = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GvT|Ghost|Runtime")
	float DarknessBiasMultiplier = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GvT|Ghost|Runtime")
	float KillDistanceMultiplier = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GvT|Ghost|Runtime")
	float MaxChaseDistanceMultiplier = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GvT|Ghost|Runtime")
	float DoorOpenChance = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GvT|Ghost|Runtime")
	FGameplayTagContainer TypeTags;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GvT|Ghost|Runtime")
	FGameplayTagContainer AllowedScareTags;
};

USTRUCT(BlueprintType)
struct FGvTGhostRoundSpawnSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ghost")
	FName GhostId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ghost")
	TObjectPtr<UGvTGhostModelData> ModelData = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ghost")
	TObjectPtr<UGvTGhostTypeData> TypeData = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ghost")
	FTransform SpawnTransform = FTransform::Identity;
};