#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Systems/EGvTCrawlerGhostState.h"
#include "GvTCrawlerGhostAnimInstance.generated.h"

UCLASS()
class GHOSTSVSTHIEVES_API UGvTCrawlerGhostAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

	UFUNCTION(BlueprintCallable, Category = "Ghost")
	void ForceGhostState(EGvTCrawlerGhostState NewState, float DurationSeconds);

	UFUNCTION(BlueprintCallable, Category = "Ghost")
	void PlayOverheadScarePose(float DurationSeconds);

	UPROPERTY(BlueprintReadOnly, Category = "Ghost")
	EGvTCrawlerGhostState GhostState = EGvTCrawlerGhostState::IdleCeiling;

	UPROPERTY(BlueprintReadOnly, Category = "Ghost")
	float Speed = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Ghost")
	float CrawlPlayRate = 1.f;

	UPROPERTY(BlueprintReadOnly, Category = "Ghost")
	bool bIsMoving = false;

	UPROPERTY(BlueprintReadOnly, Category = "Ghost")
	bool bHasForcedGhostState = false;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Ghost")
	float MoveThreshold = 3.f;

	UPROPERTY(EditDefaultsOnly, Category = "Ghost")
	float CrawlSpeedForPlayRate = 150.f;

	UPROPERTY(EditDefaultsOnly, Category = "Ghost")
	float MinCrawlPlayRate = 0.75f;

	UPROPERTY(EditDefaultsOnly, Category = "Ghost")
	float MaxCrawlPlayRate = 2.0f;

private:
	UPROPERTY(Transient)
	float ForcedOverheadScareRemaining = 0.f;

private:
	EGvTCrawlerGhostState ForcedGhostState = EGvTCrawlerGhostState::IdleCeiling;
	float ForcedGhostStateTimeRemaining = 0.f;
};