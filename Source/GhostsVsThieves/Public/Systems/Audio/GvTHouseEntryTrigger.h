#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GvTHouseEntryTrigger.generated.h"

class UBoxComponent;
class UPrimitiveComponent;
class AGvTHouseVoiceDirector;
struct FHitResult;

/** Place one at each usable entrance and point them all at the same House Voice Director. */
UCLASS()
class GHOSTSVSTHIEVES_API AGvTHouseEntryTrigger : public AActor
{
	GENERATED_BODY()

public:
	AGvTHouseEntryTrigger();

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GvT|House Voice")
	TObjectPtr<UBoxComponent> EntryTrigger;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "GvT|House Voice")
	TObjectPtr<AGvTHouseVoiceDirector> HouseVoiceDirector;

	UFUNCTION()
	void HandleEntryOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
};
