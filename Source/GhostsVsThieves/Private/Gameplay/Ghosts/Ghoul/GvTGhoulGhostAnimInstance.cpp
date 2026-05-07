#include "Gameplay/Ghosts/Ghoul/GvTGhoulGhostAnimInstance.h"

#include "Gameplay/Ghosts/Ghoul/GvTGhoulGhostCharacter.h"

void UGvTGhoulGhostAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	const AGvTGhoulGhostCharacter* Ghoul = Cast<AGvTGhoulGhostCharacter>(GetOwningActor());
	if (!Ghoul)
	{
		Speed = 0.f;
		bIsChasing = false;
		bIsPerformingScare = false;
		MovePlayRate = 1.f;
		GhostState = EGvTHauntGhostState::Idle;
		return;
	}

	Speed = Ghoul->GetReplicatedSpeed();
	bIsChasing = Ghoul->IsChasing();
	bIsPerformingScare = Ghoul->IsPerformingScare();
	GhostState = Ghoul->GetGhoulHauntState();

	const float SafeRefSpeed = FMath::Max(1.f, RefMoveSpeed);
	MovePlayRate = FMath::Clamp(Speed / SafeRefSpeed, MinPlayRate, MaxPlayRate);
}
