#include "Gameplay/Ghosts/Ghoul/GvTGhoulGhostCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"

AGvTGhoulGhostCharacter::AGvTGhoulGhostCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	bReplicates = true;
	SetReplicateMovement(true);

	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	bUseControllerRotationYaw = false;

	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->bOrientRotationToMovement = true;
		Move->RotationRate = FRotator(0.f, 720.f, 0.f);
	}
}

void AGvTGhoulGhostCharacter::BeginPlay()
{
	Super::BeginPlay();
}

void AGvTGhoulGhostCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
}