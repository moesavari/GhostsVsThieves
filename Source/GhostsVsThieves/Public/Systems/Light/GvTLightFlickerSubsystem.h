#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Systems/Light/GvTLightFlickerTypes.h"
#include "GvTLightFlickerSubsystem.generated.h"

class UGvTLightFlickerComponent;

USTRUCT()
struct FGvTLightClusterQueryResult
{
	GENERATED_BODY()

	UPROPERTY()
	bool bFoundAny = false;

	UPROPERTY()
	FVector ClusterCenter = FVector::ZeroVector;

	UPROPERTY()
	int32 NumLights = 0;
};

UCLASS()
class GHOSTSVSTHIEVES_API UGvTLightFlickerSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	void RegisterFlickerComponent(UGvTLightFlickerComponent* Component);
	void UnregisterFlickerComponent(UGvTLightFlickerComponent* Component);

	UFUNCTION(BlueprintCallable, Category = "GvT|LightFlicker")
	void PlayFlickerEvent(const FGvTLightFlickerEvent& Event);

	bool FindNearestLightClusterCenter(
		const FVector& WorldCenter,
		float SearchRadius,
		FGvTLightClusterQueryResult& OutResult) const;

	bool FindNearestLightClusterCenterFromCandidates(
		const FVector& WorldCenter,
		const TArray<FVector>& CandidateLocations,
		float SearchRadius,
		FGvTLightClusterQueryResult& OutResult) const;

	int32 CountLightsInRadius(const FVector& WorldCenter, float SearchRadius) const;

private:
	UPROPERTY(Transient)
	TSet<TObjectPtr<UGvTLightFlickerComponent>> RegisteredComponents;

	void CleanupInvalid();
};