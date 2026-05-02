#include "Gameplay/Ghosts/GvTGhostCharacterBase.h"

#include "Components/SkeletalMeshComponent.h"

AGvTGhostCharacterBase::AGvTGhostCharacterBase()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
}

void AGvTGhostCharacterBase::BeginPlay()
{
	Super::BeginPlay();

	if (bStartHidden)
	{
		SetGhostPresenceActive(false);
	}
}

void AGvTGhostCharacterBase::SetGhostPresenceActive(bool bActive)
{
	bGhostPresenceActive = bActive;

	SetActorHiddenInGame(!bActive);

	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		MeshComp->SetVisibility(bActive, true);
		MeshComp->SetHiddenInGame(!bActive, true);
	}
}

void AGvTGhostCharacterBase::BeginGhostScare(AActor* Target, FGameplayTag ScareTag)
{
	UE_LOG(LogTemp, Warning,
		TEXT("[GhostBase] BeginGhostScare Target=%s Tag=%s"),
		*GetNameSafe(Target),
		*ScareTag.ToString());
}

void AGvTGhostCharacterBase::BeginGhostEvent(AActor* Target, FGameplayTag EventTag)
{
	UE_LOG(LogTemp, Warning,
		TEXT("[GhostBase] BeginGhostEvent Target=%s Tag=%s"),
		*GetNameSafe(Target),
		*EventTag.ToString());
}

void AGvTGhostCharacterBase::BeginGhostHaunt(AActor* Target, FGameplayTag HauntTag)
{
	UE_LOG(LogTemp, Warning,
		TEXT("[GhostBase] BeginGhostHaunt Target=%s Tag=%s"),
		*GetNameSafe(Target),
		*HauntTag.ToString());
}