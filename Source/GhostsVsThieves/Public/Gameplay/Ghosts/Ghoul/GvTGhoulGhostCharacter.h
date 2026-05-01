#pragma once

#include "CoreMinimal.h"
#include "Gameplay/Ghosts/GvTGhostCharacterBase.h"
#include "GvTGhoulGhostCharacter.generated.h"

UCLASS(BlueprintType)
class GHOSTSVSTHIEVES_API AGvTGhoulGhostCharacter : public AGvTGhostCharacterBase
{
	GENERATED_BODY()

public:
	AGvTGhoulGhostCharacter();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
};