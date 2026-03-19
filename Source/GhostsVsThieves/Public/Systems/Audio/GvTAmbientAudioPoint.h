#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GvTAmbientAudioPoint.generated.h"

class USceneComponent;

UCLASS(BlueprintType)
class GHOSTSVSTHIEVES_API AGvTAmbientAudioPoint : public AActor
{
	GENERATED_BODY()

public:
	AGvTAmbientAudioPoint();

	UFUNCTION(BlueprintPure, Category = "GvT|Ambient")
	bool IsPointEnabled() const { return bEnabled; }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GvT|Ambient")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GvT|Ambient")
	bool bEnabled = true;
};