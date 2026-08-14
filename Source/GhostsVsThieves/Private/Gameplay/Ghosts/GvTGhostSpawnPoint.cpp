#include "Gameplay/Ghosts/GvTGhostSpawnPoint.h"

#include "Components/BillboardComponent.h"
#include "Components/SceneComponent.h"

AGvTGhostSpawnPoint::AGvTGhostSpawnPoint()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;

	// This root must exist in editor and packaged builds. Previously the editor-only
	// billboard was the root in editor while packaged builds created a different
	// root component. Placed instance transforms could therefore be lost during
	// cooking, leaving every spawn point at the world origin.
	USceneComponent* SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(SceneRoot);

#if WITH_EDITORONLY_DATA
	UBillboardComponent* Billboard = CreateDefaultSubobject<UBillboardComponent>(TEXT("Billboard"));
	Billboard->SetupAttachment(SceneRoot);
#endif
}

void AGvTGhostSpawnPoint::BeginPlay()
{
	Super::BeginPlay();

	UE_LOG(LogTemp, Warning,
		TEXT("[GhostSpawnPoint] Ready Point=%s Location=%s Enabled=%s"),
		*GetNameSafe(this),
		*GetActorLocation().ToString(),
		bEnabled ? TEXT("TRUE") : TEXT("FALSE"));
}

bool AGvTGhostSpawnPoint::SupportsHauntTag(FGameplayTag HauntTag) const
{
	if (!bEnabled)
	{
		return false;
	}

	return !HauntTag.IsValid() || SupportedHauntTags.IsEmpty() || SupportedHauntTags.HasTagExact(HauntTag);
}
