#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "GvTGhostPerceptionComponent.generated.h"

class APawn;

UCLASS(ClassGroup = (GvT), meta = (BlueprintSpawnableComponent))
class GHOSTSVSTHIEVES_API UGvTGhostPerceptionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGvTGhostPerceptionComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Ghost|Perception|Vision")
	float VisionRange = 600.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Ghost|Perception|Vision", meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float VisionHalfAngleDegrees = 45.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Ghost|Perception|Vision")
	float VisionTraceHeight = 60.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Ghost|Perception|Hearing")
	float HearingRadius = 900.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Ghost|Perception|Hearing")
	float NoiseMemorySeconds = 4.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Ghost|Debug")
	bool bDrawPerceptionDebug = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Ghost|Debug")
	float DebugDrawDuration = 0.05f;

	void DrawPerceptionDebug(const APawn* CandidatePawn, bool bCanSee, bool bLOSBlocked, const FVector& TraceStart, const FVector& TraceEnd, const FVector& HitLocation) const;

	bool CanSeePawn(const APawn* CandidatePawn) const;
	APawn* FindBestVisibleVictim(AActor* PreferredTarget = nullptr) const;

	bool TryFindBestNoiseLocation(
		FVector& OutNoiseLocation,
		FGameplayTag& OutNoiseTag,
		float& OutScore) const;

	virtual void TickComponent(
		float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Ghost|Debug")
	bool bDrawDetectionRangesConstantly = true;

	void DrawDetectionRangesDebug() const;

protected:
	bool IsValidVictim(const APawn* CandidatePawn) const;
};