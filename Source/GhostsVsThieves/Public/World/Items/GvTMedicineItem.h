#pragma once

#include "CoreMinimal.h"
#include "World/Items/GvTInteractableItem.h"
#include "GvTMedicineItem.generated.h"

class AGvTThiefCharacter;

UCLASS()
class GHOSTSVSTHIEVES_API AGvTMedicineItem : public AGvTInteractableItem
{
	GENERATED_BODY()

public:
	AGvTMedicineItem();

	bool CanUseMedicine(const AGvTThiefCharacter* Thief) const;
	void BuildMedicineUseSpec(FGvTInteractionSpec& OutSpec) const;

	virtual bool CanInteract_Implementation(APawn* InstigatorPawn, EGvTInteractionVerb Verb) const override;
	virtual void GetInteractionSpec_Implementation(APawn* InstigatorPawn, EGvTInteractionVerb Verb, FGvTInteractionSpec& OutSpec) const override;
	virtual void CompleteInteract_Implementation(APawn* InstigatorPawn, EGvTInteractionVerb Verb) override;

protected:
	UPROPERTY(EditAnywhere, Category = "Medicine", meta = (ClampMin = "0.1"))
	float MedicineUseDuration = 2.f;

	UPROPERTY(EditAnywhere, Category = "Medicine", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float PanicReduction01 = 0.25f;

	UPROPERTY(EditAnywhere, Category = "Medicine|Audio")
	TObjectPtr<USoundBase> MedicineUseLoopSfx = nullptr;

	UPROPERTY(EditAnywhere, Category = "Medicine|Audio")
	TObjectPtr<USoundBase> MedicineUseCompleteSfx = nullptr;

	UPROPERTY(EditAnywhere, Category = "Medicine|Audio")
	TObjectPtr<USoundBase> MedicineUseCancelSfx = nullptr;
};
