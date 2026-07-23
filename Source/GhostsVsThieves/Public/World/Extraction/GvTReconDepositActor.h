#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Gameplay/Interaction/GvTInteractable.h"
#include "GvTReconDepositActor.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class USoundBase;

UCLASS()
class GHOSTSVSTHIEVES_API AGvTReconDepositActor : public AActor, public IGvTInteractable
{
	GENERATED_BODY()

public:
	AGvTReconDepositActor();

	virtual void GetInteractionSpec_Implementation(APawn* InstigatorPawn, EGvTInteractionVerb Verb, FGvTInteractionSpec& OutSpec) const override;
	virtual bool CanInteract_Implementation(APawn* InstigatorPawn, EGvTInteractionVerb Verb) const override;
	virtual void BeginInteract_Implementation(APawn* InstigatorPawn, EGvTInteractionVerb Verb) override;
	virtual void CompleteInteract_Implementation(APawn* InstigatorPawn, EGvTInteractionVerb Verb) override;
	virtual void CancelInteract_Implementation(APawn* InstigatorPawn, EGvTInteractionVerb Verb, EGvTInteractionCancelReason Reason) override;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Recon Deposit")
	TObjectPtr<UBoxComponent> InteractionBounds;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Recon Deposit")
	TObjectPtr<UStaticMeshComponent> PlaceholderMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Recon Deposit|Interaction", meta=(ClampMin="0.0"))
	float DepositCastTime = 0.75f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Recon Deposit|Interaction")
	bool bLockMovementDuringDeposit = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Recon Deposit|Interaction")
	bool bLockLookDuringDeposit = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Recon Deposit|Audio")
	TObjectPtr<USoundBase> DepositLoopSfx = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Recon Deposit|Audio")
	TObjectPtr<USoundBase> DepositEndSfx = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Recon Deposit|Audio")
	TObjectPtr<USoundBase> DepositCancelSfx = nullptr;
};
