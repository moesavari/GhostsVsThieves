#include "Gameplay/Ghosts/GvTGhostSpawnPoint.h"

#include "Components/BillboardComponent.h"

AGvTGhostSpawnPoint::AGvTGhostSpawnPoint()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;

#if WITH_EDITORONLY_DATA
	UBillboardComponent* Billboard = CreateDefaultSubobject<UBillboardComponent>(TEXT("Billboard"));
	RootComponent = Billboard;
#else
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
#endif
}

bool AGvTGhostSpawnPoint::SupportsHauntTag(FGameplayTag HauntTag) const
{
	if (!bEnabled)
	{
		return false;
	}

	return !HauntTag.IsValid() || SupportedHauntTags.IsEmpty() || SupportedHauntTags.HasTagExact(HauntTag);
}
