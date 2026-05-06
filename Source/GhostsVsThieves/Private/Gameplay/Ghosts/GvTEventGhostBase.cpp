#include "Gameplay/Ghosts/GvTEventGhostBase.h"

void AGvTEventGhostBase::BeginGhostEvent(AActor* Target, FGameplayTag EventTag)
{
	UE_LOG(LogTemp, Warning,
		TEXT("[EventGhost] BeginGhostEvent Target=%s Tag=%s"),
		*GetNameSafe(Target),
		*EventTag.ToString());
}

void AGvTEventGhostBase::BeginGhostScare(AActor* Target, FGameplayTag ScareTag)
{
	UE_LOG(LogTemp, Warning,
		TEXT("[EventGhost] BeginGhostScare Target=%s Tag=%s"),
		*GetNameSafe(Target),
		*ScareTag.ToString());
}