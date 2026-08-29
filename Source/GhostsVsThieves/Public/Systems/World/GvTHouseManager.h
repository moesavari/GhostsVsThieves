#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GvTHouseManager.generated.h"

class USceneComponent;
class UGvTLightFlickerComponent;
class AGvTPowerBoxActor;

/**
 * Logical owner for a modular house. The physical building may remain hundreds
 * of separate meshes; breakers and haunt systems reference this actor instead.
 */
UCLASS(Blueprintable)
class GHOSTSVSTHIEVES_API AGvTHouseManager : public AActor
{
	GENERATED_BODY()

public:
	AGvTHouseManager();

	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category = "GvT|House")
	UGvTLightFlickerComponent* GetLightFlickerComponent() const { return LightFlickerComponent; }

	/** Server-only reroll. Chooses one assigned breaker and hides/disables the rest. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "GvT|House|Power")
	void ChooseRandomActivePowerBox();

	UFUNCTION(BlueprintPure, Category = "GvT|House|Power")
	AGvTPowerBoxActor* GetActivePowerBox() const { return ActivePowerBox; }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GvT|House")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GvT|House")
	TObjectPtr<UGvTLightFlickerComponent> LightFlickerComponent;

	/** The one real breaker selected by the server for this match. */
	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "GvT|House|Power")
	TObjectPtr<AGvTPowerBoxActor> ActivePowerBox;

	/** Disable only for maps that intentionally want every assigned breaker active. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GvT|House|Power")
	bool bChooseOneRandomPowerBox = true;
};
