#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Gameplay/Interaction/GvTInteractable.h"
#include "GvTExtractionDepartureActor.generated.h"

class UBoxComponent;
class USceneComponent;
class UStaticMeshComponent;
class USoundBase;

UCLASS()
class GHOSTSVSTHIEVES_API AGvTExtractionDepartureActor : public AActor, public IGvTInteractable
{
	GENERATED_BODY()

public:
	AGvTExtractionDepartureActor();
	virtual void GetInteractionSpec_Implementation(APawn* InstigatorPawn, EGvTInteractionVerb Verb, FGvTInteractionSpec& OutSpec) const override;
	virtual bool CanInteract_Implementation(APawn* InstigatorPawn, EGvTInteractionVerb Verb) const override;
	virtual void BeginInteract_Implementation(APawn* InstigatorPawn, EGvTInteractionVerb Verb) override;
	virtual void CompleteInteract_Implementation(APawn* InstigatorPawn, EGvTInteractionVerb Verb) override;
	virtual void CancelInteract_Implementation(APawn* InstigatorPawn, EGvTInteractionVerb Verb, EGvTInteractionCancelReason Reason) override;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Extraction")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Extraction")
	TObjectPtr<UBoxComponent> InteractionBounds;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Extraction")
	TObjectPtr<UStaticMeshComponent> PlaceholderMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Extraction|Interaction", meta=(ClampMin="0.0"))
	float DepartureCastTime = 3.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Extraction|Audio")
	TObjectPtr<USoundBase> DepartureLoopSfx = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Extraction|Audio")
	TObjectPtr<USoundBase> DepartureEndSfx = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Extraction|Audio")
	TObjectPtr<USoundBase> DepartureCancelSfx = nullptr;
};
