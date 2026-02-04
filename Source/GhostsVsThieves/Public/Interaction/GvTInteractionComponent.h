#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GvTInteractionComponent.generated.h"

UCLASS(ClassGroup = (GvT), meta = (BlueprintSpawnableComponent))
class GHOSTSVSTHIEVES_API UGvTInteractionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGvTInteractionComponent();

	UFUNCTION(BlueprintCallable, Category = "GvT|Interaction")
	void TryInteract();

	UFUNCTION(BlueprintCallable, Category = "GvT|Interaction")
	void TryPhoto();

	UFUNCTION(BlueprintCallable, Category = "GvT|Interaction")
	void SetDebugDraw(bool bEnabled) { bDebugDraw = bEnabled; }

protected:
	UPROPERTY(EditDefaultsOnly, Category = "GvT|Interaction")
	float TraceDistance = 350.f;

	UPROPERTY(EditDefaultsOnly, Category = "GvT|Interaction")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

	UPROPERTY(EditDefaultsOnly, Category = "GvT|Interaction")
	bool bDebugDraw = false;

	UFUNCTION(Server, Reliable)
	void Server_TryInteract();

	UFUNCTION(Server, Reliable)
	void Server_TryPhoto();

	void PerformServerTraceAndInteract(bool bPhoto);

	bool GetViewTrace(FVector& OutStart, FVector& OutEnd) const;
};
