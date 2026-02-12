#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GameplayTagContainer.h"
#include "GvTNoiseSubsystem.generated.h"

USTRUCT(BlueprintType)
struct FGvTNoiseEvent
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FVector Location = FVector::ZeroVector;

    UPROPERTY(BlueprintReadOnly)
    float Radius = 0.f;

    UPROPERTY(BlueprintReadOnly)
    float Loudness = 0.f;

    UPROPERTY(BlueprintReadOnly)
    FGameplayTag NoiseTag;

    UPROPERTY(BlueprintReadOnly)
    float TimeSeconds = 0.f;
};

UCLASS()
class GHOSTSVSTHIEVES_API UGvTNoiseSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	UFUNCTION(BlueprintCallable)
	void EmitNoise(const FGvTNoiseEvent& NoiseEvent);

	UFUNCTION(BlueprintPure)
	int32 GetRecentTagCount(FGameplayTag Tag, float WindowSeconds = 25.f) const;

	UFUNCTION(BlueprintPure)
	float GetRecentTagLoudnessSum(FGameplayTag Tag, float WindowSeconds = 25.f) const;

	UFUNCTION(BlueprintPure)
	int32 GetRecentAnyCount(const FGameplayTagContainer& AnyTags, float WindowSeconds = 25.f) const;

	UPROPERTY() 
	float ContextMult = 1.f;

private:
	UPROPERTY()
	TArray<FGvTNoiseEvent> RecentEvents;

	UPROPERTY(EditDefaultsOnly, Category = "GvT|Noise")
	int32 MaxRecentEvents = 64;
};