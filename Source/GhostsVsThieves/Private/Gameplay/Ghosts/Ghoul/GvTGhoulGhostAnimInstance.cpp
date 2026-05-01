#include "Gameplay/Ghosts/Ghoul/GvTGhoulGhostAnimInstance.h"
#include "GameFramework/Pawn.h"

void UGvTGhoulGhostAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	Speed = 0.f;
	RunPlayRate = 1.f;
	bIsMoving = false;
	GhostState = EGvTGhoulGhostState::Idle;
}

void UGvTGhoulGhostAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	APawn* OwnerPawn = TryGetPawnOwner();
	if (!OwnerPawn)
	{
		Speed = 0.f;
		RunPlayRate = 1.f;
		bIsMoving = false;
		GhostState = EGvTGhoulGhostState::Idle;
		return;
	}

	const FVector Velocity = OwnerPawn->GetVelocity();
	const FVector HorizontalVelocity(Velocity.X, Velocity.Y, 0.f);

	Speed = HorizontalVelocity.Size();
	bIsMoving = Speed > WalkThreshold;

	RunPlayRate = FMath::Clamp(
		Speed / FMath::Max(RunSpeedForPlayRate, 1.f),
		MinRunPlayRate,
		MaxRunPlayRate);

	if (Speed > RunThreshold)
	{
		GhostState = EGvTGhoulGhostState::ChaseRun;
	}
	else if (Speed > WalkThreshold)
	{
		GhostState = EGvTGhoulGhostState::Walk;
	}
	else
	{
		GhostState = EGvTGhoulGhostState::Idle;
	}
}