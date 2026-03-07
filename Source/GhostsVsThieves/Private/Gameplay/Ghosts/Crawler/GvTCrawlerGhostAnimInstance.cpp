#include "Gameplay/Ghosts/Crawler/GvTCrawlerGhostAnimInstance.h"
#include "Gameplay/Ghosts/Crawler/GvTCrawlerGhostCharacter.h"

void UGvTCrawlerGhostAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	const AGvTCrawlerGhostCharacter* CharGhost = Cast<AGvTCrawlerGhostCharacter>(GetOwningActor());
	if (!CharGhost)
	{
		GhostState = EGvTCrawlerGhostState::IdleCeiling;
		Speed = 0.f;
		SmoothedSpeed = 0.f;
		CrawlPlayRate = 1.f;
		return;
	}

	GhostState = CharGhost->GetState();

	const float TargetSpeed = FMath::Max(0.f, CharGhost->GetReplicatedSpeed());

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

	// Fallback
	//GhostState = EGvTCrawlerGhostState::IdleCeiling;
	//Speed = 0.f;
	CrawlPlayRate = 1.f;
	SmoothedSpeed = 0.f;
}