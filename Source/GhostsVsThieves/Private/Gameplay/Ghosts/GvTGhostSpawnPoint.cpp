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

float AGvTGhostSpawnPoint::ScoreForTarget(const AActor* Target, float IdealDistance, float MinDistance, float MaxDistance) const
{
	if (!bEnabled || !Target)
	{
		return -FLT_MAX;
	}

	const float Dist = FVector::Dist(GetActorLocation(), Target->GetActorLocation());
	if (MinDistance > 0.f && Dist < MinDistance)
	{
		return -FLT_MAX;
	}
	if (MaxDistance > 0.f && Dist > MaxDistance)
	{
		return -FLT_MAX;
	}

	const float SafeIdeal = FMath::Max(1.f, IdealDistance);
	const float DistanceScore = 1.f - FMath::Clamp(FMath::Abs(Dist - SafeIdeal) / SafeIdeal, 0.f, 1.f);
	return DistanceScore + FMath::FRandRange(0.f, 0.10f);
}
