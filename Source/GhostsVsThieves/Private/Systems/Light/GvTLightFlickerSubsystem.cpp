#include "Systems/Light/GvTLightFlickerSubsystem.h"
#include "Systems/Light/GvTLightFlickerComponent.h"

void UGvTLightFlickerSubsystem::RegisterFlickerComponent(UGvTLightFlickerComponent* Component)
{
	if (IsValid(Component))
	{
		RegisteredComponents.Add(Component);
	}
}

void UGvTLightFlickerSubsystem::UnregisterFlickerComponent(UGvTLightFlickerComponent* Component)
{
	if (Component)
	{
		RegisteredComponents.Remove(Component);
	}
}

void UGvTLightFlickerSubsystem::CleanupInvalid()
{
	for (auto It = RegisteredComponents.CreateIterator(); It; ++It)
	{
		if (!IsValid(*It))
		{
			It.RemoveCurrent();
		}
	}
}

void UGvTLightFlickerSubsystem::PlayFlickerEvent(const FGvTLightFlickerEvent& Event)
{
	CleanupInvalid();

	for (UGvTLightFlickerComponent* Comp : RegisteredComponents)
	{
		if (!IsValid(Comp))
		{
			continue;
		}

		if (Event.bWholeHouse)
		{
			Comp->StartFlicker(Event);
			continue;
		}

		const AActor* Owner = Comp->GetOwner();
		if (!Owner)
		{
			continue;
		}

		const float DistSq = FVector::DistSquared(Owner->GetActorLocation(), Event.WorldCenter);
		if (DistSq <= FMath::Square(Event.Radius))
		{
			Comp->StartFlicker(Event);
		}
	}
}

bool UGvTLightFlickerSubsystem::FindNearestLightClusterCenter(
	const FVector& WorldCenter,
	float SearchRadius,
	FGvTLightClusterQueryResult& OutResult) const
{
	OutResult = FGvTLightClusterQueryResult{};

	if (SearchRadius <= 0.f)
	{
		return false;
	}

	const float SearchRadiusSq = FMath::Square(SearchRadius);

	TArray<FVector> MatchedLocations;
	MatchedLocations.Reserve(32);

	for (UGvTLightFlickerComponent* Comp : RegisteredComponents)
	{
		if (!IsValid(Comp))
		{
			continue;
		}

		TArray<FVector> LocalLightLocations;
		Comp->GetCachedLightLocations(LocalLightLocations);

		for (const FVector& LightLoc : LocalLightLocations)
		{
			if (FVector::DistSquared(WorldCenter, LightLoc) <= SearchRadiusSq)
			{
				MatchedLocations.Add(LightLoc);
			}
		}
	}

	UE_LOG(LogTemp, Warning,
		TEXT("[LightClusterQuery] Center=%s Radius=%.1f Found=%d"),
		*WorldCenter.ToCompactString(),
		SearchRadius,
		MatchedLocations.Num());

	if (MatchedLocations.Num() == 0)
	{
		return false;
	}

	FVector Sum = FVector::ZeroVector;
	for (const FVector& Loc : MatchedLocations)
	{
		Sum += Loc;
	}

	OutResult.bFoundAny = true;
	OutResult.NumLights = MatchedLocations.Num();
	OutResult.ClusterCenter = Sum / float(MatchedLocations.Num());
	return true;
}

int32 UGvTLightFlickerSubsystem::CountLightsInRadius(const FVector& WorldCenter, float SearchRadius) const
{
	if (SearchRadius <= 0.f)
	{
		return 0;
	}

	const float SearchRadiusSq = FMath::Square(SearchRadius);
	int32 Count = 0;

	for (UGvTLightFlickerComponent* Comp : RegisteredComponents)
	{
		if (!IsValid(Comp))
		{
			continue;
		}

		TArray<FVector> LocalLightLocations;
		Comp->GetCachedLightLocations(LocalLightLocations);

		for (const FVector& LightLoc : LocalLightLocations)
		{
			if (FVector::DistSquared(WorldCenter, LightLoc) <= SearchRadiusSq)
			{
				++Count;
			}
		}
	}

	return Count;
}