#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "GvTGhostTypeData.generated.h"

UCLASS(BlueprintType)
class GHOSTSVSTHIEVES_API UGvTGhostTypeData : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GvT|Ghost|Type|Identity")
	FName TypeId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GvT|Ghost|Type|Identity")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GvT|Ghost|Type|Behavior")
	float NoiseInterestMultiplier = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GvT|Ghost|Type|Behavior")
	float ElectricalInterestMultiplier = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GvT|Ghost|Type|Behavior")
	float LightInterestMultiplier = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GvT|Ghost|Type|Behavior")
	float ValuableItemInterestMultiplier = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GvT|Ghost|Type|Behavior")
	float MedicalItemInterestMultiplier = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GvT|Ghost|Type|Behavior")
	float HuntAggressionMultiplier = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GvT|Ghost|Type|Behavior")
	float RoamChanceMultiplier = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GvT|Ghost|Type|Behavior")
	float ScareFrequencyMultiplier = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GvT|Ghost|Type|Behavior")
	float PanicBiasMultiplier = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GvT|Ghost|Type|Behavior")
	float IsolationBiasMultiplier = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GvT|Ghost|Type|Behavior")
	float RecentHauntPressureBiasMultiplier = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GvT|Ghost|Type|Behavior")
	float DarknessBiasMultiplier = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GvT|Ghost|Type|Movement")
	float WalkSpeedMultiplier = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GvT|Ghost|Type|Movement")
	float RunSpeedMultiplier = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GvT|Ghost|Type|Movement")
	float AccelerationMultiplier = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GvT|Ghost|Type|Movement")
	float BrakingMultiplier = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GvT|Ghost|Type|Movement")
	float KillDistanceMultiplier = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GvT|Ghost|Type|Movement")
	float MaxChaseDistanceMultiplier = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GvT|Ghost|Type|Interaction")
	float DoorOpenChance = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GvT|Ghost|Type|Scares")
	FGameplayTagContainer AllowedScareTags;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GvT|Ghost|Type|Scares")
	TMap<FGameplayTag, float> ScareTagMultipliers;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GvT|Ghost|Type|Tags")
	FGameplayTagContainer TypeTags;
};