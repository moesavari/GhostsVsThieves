#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Gameplay/Interaction/GvTInteractable.h"
#include "GvTVanInventoryActor.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class USoundBase;

UCLASS()
class GHOSTSVSTHIEVES_API AGvTVanInventoryActor : public AActor, public IGvTInteractable
{
	GENERATED_BODY()

public:
	AGvTVanInventoryActor();

	virtual void GetInteractionSpec_Implementation(APawn* InstigatorPawn, EGvTInteractionVerb Verb, FGvTInteractionSpec& OutSpec) const override;
	virtual bool CanInteract_Implementation(APawn* InstigatorPawn, EGvTInteractionVerb Verb) const override;
	virtual void BeginInteract_Implementation(APawn* InstigatorPawn, EGvTInteractionVerb Verb) override;
	virtual void CompleteInteract_Implementation(APawn* InstigatorPawn, EGvTInteractionVerb Verb) override;
	virtual void CancelInteract_Implementation(APawn* InstigatorPawn, EGvTInteractionVerb Verb, EGvTInteractionCancelReason Reason) override;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GvT|Van Inventory")
	TObjectPtr<UBoxComponent> InteractionBounds;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GvT|Van Inventory")
	TObjectPtr<UStaticMeshComponent> PlaceholderMesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GvT|Van Inventory", meta=(ClampMin="0.0"))
	float OpenCastTime = 0.25f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GvT|Van Inventory")
	TObjectPtr<USoundBase> OpenLoopSfx;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GvT|Van Inventory")
	TObjectPtr<USoundBase> OpenEndSfx;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GvT|Van Inventory")
	TObjectPtr<USoundBase> OpenCancelSfx;
};
