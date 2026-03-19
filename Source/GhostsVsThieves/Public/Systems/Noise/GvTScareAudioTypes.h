#pragma once

#include "CoreMinimal.h"
#include "GvTScareAudioTypes.generated.h"

class USoundBase;
class UAudioComponent;

USTRUCT(BlueprintType)
struct FGvTScareAudioSet
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Scare|Audio")
	TObjectPtr<USoundBase> StartSfx = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Scare|Audio")
	TObjectPtr<USoundBase> SustainLoopSfx = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Scare|Audio")
	TObjectPtr<USoundBase> EndSfx = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Scare|Audio", meta = (ClampMin = "0.0"))
	float VolumeMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Scare|Audio", meta = (ClampMin = "0.1"))
	float PitchMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Scare|Audio", meta = (ClampMin = "0.0"))
	float SustainFadeOutSeconds = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Scare|Audio")
	bool bSustainIs2D = false;
};
