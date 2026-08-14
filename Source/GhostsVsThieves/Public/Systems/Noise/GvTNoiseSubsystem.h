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

	UPROPERTY(BlueprintReadOnly)
	int64 EventId = 0;
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

	UFUNCTION(BlueprintPure)
	bool TryGetBestRecentNoiseNearLocation(
		const FVector& ListenerLocation,
		float HearingRadius,
		float MemorySeconds,
		FVector& OutNoiseLocation,
		FGameplayTag& OutNoiseTag,
		float& OutScore,
		int64& OutEventId) const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Noise|Debug")
	bool bDebugGhostHearingOnScreen = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Noise|Debug")
	float DebugGhostHearingMessageTime = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Noise|Debug")
	float GhostHearingLoudnessRadiusMultiplier = 1.0f;

private:
	UPROPERTY()
	TArray<FGvTNoiseEvent> RecentEvents;

	UPROPERTY(EditDefaultsOnly, Category = "GvT|Noise")
	int32 MaxRecentEvents = 64;

	int64 NextEventId = 1;
};
