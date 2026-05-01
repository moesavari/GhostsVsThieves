#include "Gameplay/Ghosts/Crawler/GvTCrawlerGhostAnimInstance.h"
#include "GameFramework/Pawn.h"

void UGvTCrawlerGhostAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	Speed = 0.f;
	CrawlPlayRate = 1.f;
	bIsMoving = false;
	bHasForcedGhostState = false;
	ForcedGhostState = EGvTCrawlerGhostState::IdleCeiling;
	ForcedGhostStateTimeRemaining = 0.f;
	GhostState = EGvTCrawlerGhostState::IdleCeiling;
}

void UGvTCrawlerGhostAnimInstance::ForceGhostState(EGvTCrawlerGhostState NewState, float DurationSeconds)
{
	ForcedGhostState = NewState;
	ForcedGhostStateTimeRemaining = FMath::Max(0.01f, DurationSeconds);
	bHasForcedGhostState = true;
	GhostState = ForcedGhostState;
}

void UGvTCrawlerGhostAnimInstance::PlayOverheadScarePose(float DurationSeconds)
{
	ForceGhostState(EGvTCrawlerGhostState::OverheadScare, DurationSeconds);
}

void UGvTCrawlerGhostAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	if (ForcedOverheadScareRemaining > 0.f)
	{
		ForcedOverheadScareRemaining = FMath::Max(0.f, ForcedOverheadScareRemaining - DeltaSeconds);
		GhostState = EGvTCrawlerGhostState::OverheadScare;
		Speed = 0.f;
		bIsMoving = false;
		CrawlPlayRate = 1.f;
		return;
	}

	APawn* OwnerPawn = TryGetPawnOwner();
	if (!OwnerPawn)
	{
		Speed = 0.f;
		CrawlPlayRate = 1.f;
		bIsMoving = false;

		if (bHasForcedGhostState)
		{
			ForcedGhostStateTimeRemaining -= DeltaSeconds;
			if (ForcedGhostStateTimeRemaining > 0.f)
			{
				GhostState = ForcedGhostState;
				return;
			}

			bHasForcedGhostState = false;
			ForcedGhostStateTimeRemaining = 0.f;
		}

		GhostState = EGvTCrawlerGhostState::IdleCeiling;
		return;
	}

	const FVector Velocity = OwnerPawn->GetVelocity();
	const FVector HorizontalVelocity(Velocity.X, Velocity.Y, 0.f);

	Speed = HorizontalVelocity.Size();
	bIsMoving = Speed > MoveThreshold;

	// Since crawler now uses plain movement, just drive play rate from speed.
	CrawlPlayRate = FMath::Clamp(
		Speed / FMath::Max(CrawlSpeedForPlayRate, 1.f),
		MinCrawlPlayRate,
		MaxCrawlPlayRate);

	if (bHasForcedGhostState)
	{
		ForcedGhostStateTimeRemaining -= DeltaSeconds;
		if (ForcedGhostStateTimeRemaining > 0.f)
		{
			GhostState = ForcedGhostState;
			return;
		}

		bHasForcedGhostState = false;
		ForcedGhostStateTimeRemaining = 0.f;
	}

	// Keep enum blend graph alive with simple logic.
	GhostState = bIsMoving
		? EGvTCrawlerGhostState::HauntChase
		: EGvTCrawlerGhostState::IdleCeiling;
}