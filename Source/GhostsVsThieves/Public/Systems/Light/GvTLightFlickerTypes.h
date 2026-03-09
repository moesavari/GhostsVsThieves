#pragma once

#include "CoreMinimal.h"
#include "GvTLightFlickerTypes.generated.h"

USTRUCT(BlueprintType)
struct FGvTLightFlickerEvent
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|LightFlicker")
	FVector WorldCenter = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|LightFlicker", meta = (ClampMin = "0.0"))
	float Radius = 5000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|LightFlicker", meta = (ClampMin = "0.0"))
	float Duration = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|LightFlicker", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Intensity01 = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|LightFlicker")
	bool bWholeHouse = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|LightFlicker")
	bool bAllowFullOffBursts = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|LightFlicker", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MinDimAlpha = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|LightFlicker", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MaxDimAlpha = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|LightFlicker", meta = (ClampMin = "0.001"))
	float PulseIntervalMin = 0.03f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|LightFlicker", meta = (ClampMin = "0.001"))
	float PulseIntervalMax = 0.10f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|LightFlicker")
	int32 Seed = 1337;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|LightFlicker")
	FName ZoneName = NAME_None;
};