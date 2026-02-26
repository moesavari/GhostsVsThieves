#include "Gameplay/Ghosts/Crawler/GvTCrawlerGhostAnimInstance.h"
#include "Gameplay/Ghosts/Crawler/AGvTCrawlerGhost.h"

void UGvTCrawlerGhostAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	const AGvTCrawlerGhost* Ghost = Cast<AGvTCrawlerGhost>(GetOwningActor());
	if (!Ghost)
	{
		GhostState = EGvTCrawlerGhostState::IdleCeiling;
		Speed = 0.f;
		CrawlPlayRate = 1.f;
		SmoothedSpeed = 0.f;
		return;
	}

	GhostState = Ghost->GetState();

	const float TargetSpeed = FMath::Max(0.f, Ghost->GetReplicatedSpeed());
	if (bSmoothSpeed)
	{
		SmoothedSpeed = FMath::FInterpTo(SmoothedSpeed, TargetSpeed, DeltaSeconds, SpeedInterp);
		Speed = SmoothedSpeed;
	}
	else
	{
		Speed = TargetSpeed;
		SmoothedSpeed = TargetSpeed;
	}

	// Derive play rate from speed. Keep non-chase states stable.
	if (GhostState == EGvTCrawlerGhostState::HauntChase)
	{
		const float SafeRef = FMath::Max(1.f, RefCrawlSpeed);
		const float Raw = Speed / SafeRef;
		CrawlPlayRate = FMath::Clamp(Raw, MinPlayRate, MaxPlayRate);
	}
	else
	{
		CrawlPlayRate = 1.f;
	}
}
