#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "Interaction/GvTInteractable.h"
#include "GvTInteractableItem.generated.h"

class UStaticMeshComponent;

UCLASS()
class GHOSTSVSTHIEVES_API AGvTInteractableItem : public AActor, public IGvTInteractable
{
	GENERATED_BODY()

public:
	AGvTInteractableItem();

	// IGvTInteractable
	virtual void GetInteractionSpec_Implementation(APawn* InstigatorPawn, EGvTInteractionVerb Verb, FGvTInteractionSpec& OutSpec) const override;
	virtual bool CanInteract_Implementation(APawn* InstigatorPawn, EGvTInteractionVerb Verb) const override;
	virtual void BeginInteract_Implementation(APawn* InstigatorPawn, EGvTInteractionVerb Verb) override;
	virtual void CompleteInteract_Implementation(APawn* InstigatorPawn, EGvTInteractionVerb Verb) override;
	virtual void CancelInteract_Implementation(APawn* InstigatorPawn, EGvTInteractionVerb Verb, EGvTInteractionCancelReason Reason) override;

protected:
	UPROPERTY(VisibleAnywhere, Category = "Item")
	UStaticMeshComponent* Mesh;

	UPROPERTY(EditAnywhere, Category = "Item|Rules")
	bool bConsumedOnInteract = true;


	UPROPERTY(EditAnywhere, Category="Item|Interaction")
	float InteractCastTime = 0.75f;

	UPROPERTY(EditAnywhere, Category="Item|Interaction")
	float PhotoCastTime = 0.25f;

	UPROPERTY(EditAnywhere, Category="Item|Interaction")
	bool bLockMoveDuringInteract = true;

	UPROPERTY(EditAnywhere, Category="Item|Interaction")
	bool bLockLookDuringInteract = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GvT|Scan")
	FText DisplayName = FText::FromString(TEXT("Item"));

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GvT|Scan", meta = (ClampMin = "0.0"))
	float ScanMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, Category = "Item|Value")
	int32 BaseValue = 50;

	UPROPERTY(EditAnywhere, Category = "Item|Value")
	float PhotoMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, Category = "Item|Noise")
	float InteractNoiseRadius = 800.f;

	UPROPERTY(EditAnywhere, Category = "Item|Noise")
	float PhotoNoiseRadius = 300.f;

	UPROPERTY(EditAnywhere, Category = "Item|Noise")
	FGameplayTag InteractNoiseTag;

	UPROPERTY(EditAnywhere, Category = "Item|Noise")
	FGameplayTag PhotoNoiseTag;

	UPROPERTY(ReplicatedUsing = OnRep_IsConsumed)
	bool bIsConsumed = false;

	UPROPERTY(ReplicatedUsing = OnRep_HasPhoto)
	bool bHasBeenPhotographed = false;

	UPROPERTY(Replicated)
	int32 AppraisedValue = 0;

	UFUNCTION()
	void OnRep_IsConsumed();

	UFUNCTION()
	void OnRep_HasPhoto();

	void ApplyConsumedState(bool bConsumed);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
