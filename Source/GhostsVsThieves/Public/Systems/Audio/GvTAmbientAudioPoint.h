#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GvTAmbientAudioPoint.generated.h"

class USceneComponent;

UENUM(BlueprintType)
enum class EGvTAmbientZone : uint8
{
	Indoor UMETA(DisplayName = "Indoor"),
	Outdoor UMETA(DisplayName = "Outdoor")
};

UCLASS(BlueprintType)
class GHOSTSVSTHIEVES_API AGvTAmbientAudioPoint : public AActor
{
	GENERATED_BODY()

public:
	AGvTAmbientAudioPoint();

	UFUNCTION(BlueprintPure, Category = "GvT|Ambient")
	bool IsPointEnabled() const { return bEnabled; }

	UFUNCTION(BlueprintPure, Category = "GvT|Ambient")
	EGvTAmbientZone GetAmbientZone() const { return AmbientZone; }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GvT|Ambient")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GvT|Ambient")
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GvT|Ambient")
	EGvTAmbientZone AmbientZone = EGvTAmbientZone::Indoor;
};
