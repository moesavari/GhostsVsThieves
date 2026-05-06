#include "Gameplay/Ghosts/GvTGhostCharacterBase.h"
#include "Net/UnrealNetwork.h"
#include "Components/SkeletalMeshComponent.h"

AGvTGhostCharacterBase::AGvTGhostCharacterBase()
{
	PrimaryActorTick.bCanEverTick = false;

	bReplicates = true;
	SetReplicateMovement(true);
	bAlwaysRelevant = true;
	bOnlyRelevantToOwner = false;
	NetDormancy = DORM_Never;
	NetCullDistanceSquared = FMath::Square(30000.f);
	NetUpdateFrequency = 100.f;
	MinNetUpdateFrequency = 30.f;
}

void AGvTGhostCharacterBase::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		SetGhostPresenceActive(true);
	}
	else
	{
		ApplyGhostPresenceActive();
	}
}

void AGvTGhostCharacterBase::SetGhostPresenceActive(bool bActive)
{
	bGhostPresenceActive = bActive;
	ApplyGhostPresenceActive();

	if (HasAuthority())
	{
		FlushNetDormancy();
		ForceNetUpdate();
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

void AGvTGhostCharacterBase::OnRep_GhostPresenceActive()
{
	ApplyGhostPresenceActive();
}

void AGvTGhostCharacterBase::ApplyGhostPresenceActive()
{
	SetActorHiddenInGame(!bGhostPresenceActive);

	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		MeshComp->SetVisibility(bGhostPresenceActive, true);
		MeshComp->SetHiddenInGame(!bGhostPresenceActive, true);
	}
}

void AGvTGhostCharacterBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AGvTGhostCharacterBase, bGhostPresenceActive);
}