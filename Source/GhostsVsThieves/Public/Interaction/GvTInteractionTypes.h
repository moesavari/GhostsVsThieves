#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GvTInteractionTypes.generated.h"

UENUM(BlueprintType)
enum class EGvTInteractionVerb : uint8
{
	Interact		UMETA(DisplayName="Interact"),
	Photo			UMETA(DisplayName="Photo"),
	Scan			UMETA(DisplayName="Scan"),
};

UENUM(BlueprintType)
enum class EGvTInteractionCancelReason : uint8
{
	UserCanceled	UMETA(DisplayName="User Canceled"),
	Moved			UMETA(DisplayName="Moved"),
	Damaged			UMETA(DisplayName="Damaged"),
	Invalid			UMETA(DisplayName="Invalid"),
	Other			UMETA(DisplayName="Other"),
};

USTRUCT(BlueprintType)
struct FGvTInteractionSpec
{
	GENERATED_BODY()

	// If 0, interaction resolves instantly on the server
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GvT|Interaction")
	float CastTime = 0.f;

	// Lock rules
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GvT|Interaction")
	bool bLockMovement = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GvT|Interaction")
	bool bLockLook = true;

	// Cancel rules
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GvT|Interaction")
	bool bCancelable = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GvT|Interaction")
	bool bEmitNoiseOnCancel = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GvT|Interaction", meta=(EditCondition="bEmitNoiseOnCancel"))
	float CancelNoiseRadius = 600.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GvT|Interaction")
	float CancelNoiseLoudness = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GvT|Interaction")
	FGameplayTag InteractionTag;
};
