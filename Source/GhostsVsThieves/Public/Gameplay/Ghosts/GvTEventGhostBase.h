#pragma once

#include "CoreMinimal.h"
#include "Gameplay/Ghosts/GvTGhostCharacterBase.h"
#include "GvTEventGhostBase.generated.h"

UCLASS()
class GHOSTSVSTHIEVES_API AGvTEventGhostBase : public AGvTGhostCharacterBase
{
	GENERATED_BODY()

public:

	virtual void BeginGhostEvent(AActor* Target, FGameplayTag EventTag);

	virtual void BeginGhostScare(AActor* Target, FGameplayTag ScareTag) override;
};