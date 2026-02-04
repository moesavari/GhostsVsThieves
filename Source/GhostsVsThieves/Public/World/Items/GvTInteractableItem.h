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

	virtual void Interact_Implementation(APawn* InstigatorPawn) override;
	virtual void Photo_Implementation(APawn* InstigatorPawn) override;

protected:
	UPROPERTY(VisibleAnywhere, Category = "Item")
	UStaticMeshComponent* Mesh;

	UPROPERTY(EditAnywhere, Category = "Item|Rules")
	bool bConsumedOnInteract = true;

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

	UFUNCTION()
	void OnRep_IsConsumed();

	UFUNCTION()
	void OnRep_HasPhoto();

	void ApplyConsumedState(bool bConsumed);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
