#pragma once

#include "CoreMinimal.h"
#include "World/Items/GvTInteractableItem.h"
#include "GvTFlashlightItem.generated.h"

class USpotLightComponent;

UCLASS()
class GHOSTSVSTHIEVES_API AGvTFlashlightItem : public AGvTInteractableItem
{
	GENERATED_BODY()

public:
	AGvTFlashlightItem();

	/** Toggles the beam only while this item is the carrier's selected item. */
	UFUNCTION(BlueprintCallable, Category = "Flashlight")
	void ToggleFlashlight();

	UFUNCTION(BlueprintPure, Category = "Flashlight")
	bool IsFlashlightOn() const { return bIsFlashlightOn; }

protected:
	virtual void ApplyCarryState() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Flashlight")
	TObjectPtr<USpotLightComponent> FlashlightBeam;

	UPROPERTY(ReplicatedUsing = OnRep_FlashlightOn, BlueprintReadOnly, Category = "Flashlight")
	bool bIsFlashlightOn = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flashlight", meta = (ClampMin = "0.0"))
	float BeamIntensity = 12000.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flashlight", meta = (ClampMin = "0.0"))
	float BeamRange = 2500.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flashlight", meta = (ClampMin = "0.0", ClampMax = "80.0"))
	float BeamInnerConeAngle = 14.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flashlight", meta = (ClampMin = "0.0", ClampMax = "80.0"))
	float BeamOuterConeAngle = 28.f;

	UFUNCTION(Server, Reliable)
	void Server_SetFlashlightOn(bool bNewOn);

	UFUNCTION()
	void OnRep_FlashlightOn();

private:
	void SetFlashlightOnAuthoritative(bool bNewOn);
	void ApplyFlashlightState();
};