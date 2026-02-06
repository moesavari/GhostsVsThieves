#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Interaction/GvTInteractionTypes.h"
#include "GvTInteractable.generated.h"

UINTERFACE(BlueprintType)
class GHOSTSVSTHIEVES_API UGvTInteractable : public UInterface
{
	GENERATED_BODY()
};

class GHOSTSVSTHIEVES_API IGvTInteractable
{
	GENERATED_BODY()

public:
	// Return the spec for this verb (cast time, lock rules, cancel noise rules).
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="GvT|Interactable")
	void GetInteractionSpec(APawn* InstigatorPawn, EGvTInteractionVerb Verb, FGvTInteractionSpec& OutSpec) const;

	// Optional validation gate. If not implemented in BP, assume true.
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="GvT|Interactable")
	bool CanInteract(APawn* InstigatorPawn, EGvTInteractionVerb Verb) const;

	// Called when a cast-time interaction begins (server). For instant interactions, this may be skipped.
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="GvT|Interactable")
	void BeginInteract(APawn* InstigatorPawn, EGvTInteractionVerb Verb);

	// Called when interaction completes successfully (server).
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="GvT|Interactable")
	void CompleteInteract(APawn* InstigatorPawn, EGvTInteractionVerb Verb);

	// Called when interaction is canceled (server).
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="GvT|Interactable")
	void CancelInteract(APawn* InstigatorPawn, EGvTInteractionVerb Verb, EGvTInteractionCancelReason Reason);
};
