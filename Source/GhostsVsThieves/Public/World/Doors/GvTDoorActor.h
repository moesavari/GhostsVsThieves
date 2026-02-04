#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "Interaction/GvTInteractable.h"
#include "GvTDoorActor.generated.h"

class USceneComponent;
class UStaticMeshComponent;

UCLASS()
class GHOSTSVSTHIEVES_API AGvTDoorActor : public AActor, public IGvTInteractable
{
	GENERATED_BODY()

public:
	AGvTDoorActor();

	virtual void Interact_Implementation(APawn* InstigatorPawn) override;
	virtual void Photo_Implementation(APawn* InstigatorPawn) override {}

protected:
	UPROPERTY(VisibleAnywhere, Category = "Door")
	USceneComponent* Root;

	UPROPERTY(VisibleAnywhere, Category = "Door")
	USceneComponent* Hinge;

	UPROPERTY(VisibleAnywhere, Category = "Door")
	UStaticMeshComponent* DoorMesh;

	UPROPERTY(EditAnywhere, Category = "Door|Rotation")
	float ClosedYaw = 0.f;

	UPROPERTY(EditAnywhere, Category = "Door|Rotation")
	float OpenYaw = 90.f;

	UPROPERTY(EditAnywhere, Category = "Door|Rotation")
	bool bInvertDirection = false;

	UPROPERTY(EditAnywhere, Category = "Door|Noise")
	float DoorNoiseRadius = 1200.f;

	UPROPERTY(EditAnywhere, Category = "Door|Noise")
	FGameplayTag DoorNoiseTag;

	UPROPERTY(ReplicatedUsing = OnRep_IsOpen)
	bool bIsOpen = false;

	UFUNCTION()
	void OnRep_IsOpen();

	void ApplyDoorState(bool bOpen);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
