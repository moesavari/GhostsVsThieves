#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Systems/EGvTCrawlerGhostState.h"
#include "GvTCrawlerGhostAnimInstance.generated.h"

/**
 * C++-driven anim instance: reads replicated ghost state/speed and derives a crawl play rate.
 * AnimBP should stay "dumb": it only blends poses and pipes CrawlPlayRate into the crawl SequencePlayer.
 */
UCLASS(BlueprintType, Blueprintable)
class GHOSTSVSTHIEVES_API UGvTCrawlerGhostAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	/** Replicated/authoritative ghost state (owned by AGvTCrawlerGhost). */
	UPROPERTY(BlueprintReadOnly, Category = "Crawler|Anim")
	EGvTCrawlerGhostState GhostState = EGvTCrawlerGhostState::IdleCeiling;

	/** Current movement speed in UU/s (cm/s). */
	UPROPERTY(BlueprintReadOnly, Category = "Crawler|Anim")
	float Speed = 0.f;

	/** Play-rate to feed into the crawl SequencePlayer. */
	UPROPERTY(BlueprintReadOnly, Category = "Crawler|Anim")
	float CrawlPlayRate = 1.f;

	/** Tuning: Speed (UU/s) that looks correct when CrawlPlayRate == 1.0. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crawler|Anim|Tuning", meta=(ClampMin="1.0", UIMin="50.0", UIMax="1200.0"))
	float RefCrawlSpeed = 350.f;

	/** Tuning: clamp range for CrawlPlayRate. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crawler|Anim|Tuning", meta=(ClampMin="0.0", UIMin="0.0", UIMax="5.0"))
	float MinPlayRate = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crawler|Anim|Tuning", meta=(ClampMin="0.0", UIMin="0.0", UIMax="5.0"))
	float MaxPlayRate = 3.0f;

	/** Optional: smooth the visible Speed/PlayRate to avoid jitter from replication/collisions. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crawler|Anim|Tuning")
	bool bSmoothSpeed = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crawler|Anim|Tuning", meta=(ClampMin="0.0", UIMin="0.0", UIMax="30.0"))
	float SpeedInterp = 8.f;

protected:
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

private:
	float SmoothedSpeed = 0.f;
};
