#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Systems/Light/GvTLightFlickerTypes.h"
#include "GvTLightFlickerSubsystem.generated.h"

class UGvTLightFlickerComponent;

UCLASS()
class GHOSTSVSTHIEVES_API UGvTLightFlickerSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	void RegisterFlickerComponent(UGvTLightFlickerComponent* Component);
	void UnregisterFlickerComponent(UGvTLightFlickerComponent* Component);

	UFUNCTION(BlueprintCallable, Category = "GvT|LightFlicker")
	void PlayFlickerEvent(const FGvTLightFlickerEvent& Event);

private:
	UPROPERTY(Transient)
	TSet<TObjectPtr<UGvTLightFlickerComponent>> RegisteredComponents;

	void CleanupInvalid();
};