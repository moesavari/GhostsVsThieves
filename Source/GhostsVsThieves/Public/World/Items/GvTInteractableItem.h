#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "Gameplay/Interaction/GvTInteractable.h"
#include "GvTInteractableItem.generated.h"

class UStaticMeshComponent;
class UGvTDirectorSubsystem;

UCLASS()
class GHOSTSVSTHIEVES_API AGvTInteractableItem : public AActor, public IGvTInteractable
{
	GENERATED_BODY()

public:
	AGvTInteractableItem();

	// IGvTInteractable
	virtual void GetInteractionSpec_Implementation(APawn* InstigatorPawn, EGvTInteractionVerb Verb, FGvTInteractionSpec& OutSpec) const override;
	virtual bool CanInteract_Implementation(APawn* InstigatorPawn, EGvTInteractionVerb Verb) const override;
	virtual void BeginInteract_Implementation(APawn* InstigatorPawn, EGvTInteractionVerb Verb) override;
	virtual void CompleteInteract_Implementation(APawn* InstigatorPawn, EGvTInteractionVerb Verb) override;
	virtual void CancelInteract_Implementation(APawn* InstigatorPawn, EGvTInteractionVerb Verb, EGvTInteractionCancelReason Reason) override;

	UFUNCTION(BlueprintPure, Category = "Item|Ghost Reaction")
	bool ShouldUpsetGhostsOnInteract() const { return bUpsetsGhostsOnInteract; }

	UFUNCTION(BlueprintPure, Category = "Item|Ghost Reaction")
	bool IsGhostValuable() const { return bTreatAsValuableForGhosts || BaseValue >= ValuableGhostReactionValueThreshold; }

	UFUNCTION(BlueprintPure, Category = "Item|Ghost Reaction")
	bool IsGhostNoisy() const { return bTreatAsNoisyForGhosts; }

	UFUNCTION(BlueprintPure, Category = "Item|Ghost Reaction")
	bool IsGhostElectrical() const { return bTreatAsElectricalForGhosts; }

	UFUNCTION(BlueprintPure, Category = "Item|Ghost Reaction")
	float GetGhostReactionChanceBonus() const { return GhostReactionChanceBonus; }

	UFUNCTION(BlueprintPure, Category = "Item|Ghost Reaction")
	float GetGhostItemValue01() const;

protected:
	UPROPERTY(VisibleAnywhere, Category = "Item")
	UStaticMeshComponent* Mesh;

	UPROPERTY(EditAnywhere, Category = "Item|Rules")
	bool bConsumedOnInteract = true;


	UPROPERTY(EditAnywhere, Category="Item|Interaction")
	float InteractCastTime = 0.75f;

	UPROPERTY(EditAnywhere, Category="Item|Interaction")
	float PhotoCastTime = 0.25f;

	UPROPERTY(EditAnywhere, Category = "Item|Interaction")
	float ScanCastTime = 0.85f;

	UPROPERTY(EditAnywhere, Category="Item|Interaction")
	bool bLockMoveDuringInteract = true;

	UPROPERTY(EditAnywhere, Category="Item|Interaction")
	bool bLockLookDuringInteract = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GvT|Scan")
	FText DisplayName = FText::FromString(TEXT("Item"));

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GvT|Scan", meta = (ClampMin = "0.0"))
	float ScanMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, Category = "Item|Value")
	int32 BaseValue = 50;

	UPROPERTY(EditAnywhere, Category = "Item|Value")
	float PhotoMultiplier = 1.0f;


	// Interaction with loot should make the house angry. These are used by the Director
	// when the actor does not have explicit tags like Valuable/Noisy/Electrical.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item|Ghost Reaction")
	bool bUpsetsGhostsOnInteract = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item|Ghost Reaction")
	bool bTreatAsValuableForGhosts = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item|Ghost Reaction")
	bool bTreatAsNoisyForGhosts = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item|Ghost Reaction")
	bool bTreatAsElectricalForGhosts = false;

	// Extra chance added on top of panic/pressure/type weighting. Good demo default: stealing should usually wake the house up.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item|Ghost Reaction", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float GhostReactionChanceBonus = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item|Ghost Reaction", meta = (ClampMin = "0"))
	int32 ValuableGhostReactionValueThreshold = 25;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item|Ghost Reaction", meta = (ClampMin = "1"))
	int32 HighValueGhostReactionValue = 250;

	UPROPERTY(EditAnywhere, Category = "Item|Noise")
	float InteractNoiseRadius = 800.f;

	UPROPERTY(EditAnywhere, Category = "Item|Noise")
	float PhotoNoiseRadius = 300.f;

	UPROPERTY(EditAnywhere, Category = "Item|Noise")
	float ScanNoiseRadius = 250.f;

	UPROPERTY(EditAnywhere, Category = "Item|Noise")
	FGameplayTag InteractNoiseTag;

	UPROPERTY(EditAnywhere, Category = "Item|Noise")
	FGameplayTag PhotoNoiseTag;

	UPROPERTY(EditAnywhere, Category = "Item|Noise")
	FGameplayTag ScanNoiseTag;

	UPROPERTY(EditAnywhere, Category = "Item|Audio")
	TObjectPtr<USoundBase> InteractLoopSfx = nullptr;

	UPROPERTY(EditAnywhere, Category = "Item|Audio")
	TObjectPtr<USoundBase> InteractEndSfx = nullptr;

	UPROPERTY(EditAnywhere, Category = "Item|Audio")
	TObjectPtr<USoundBase> InteractCancelSfx = nullptr;

	UPROPERTY(EditAnywhere, Category = "Item|Audio")
	TObjectPtr<USoundBase> ScanLoopSfx = nullptr;

	UPROPERTY(EditAnywhere, Category = "Item|Audio")
	TObjectPtr<USoundBase> ScanEndSfx = nullptr;

	UPROPERTY(EditAnywhere, Category = "Item|Audio")
	TObjectPtr<USoundBase> ScanCancelSfx = nullptr;


	UPROPERTY(ReplicatedUsing = OnRep_IsConsumed)
	bool bIsConsumed = false;

	UPROPERTY(ReplicatedUsing = OnRep_HasPhoto)
	bool bHasBeenPhotographed = false;

	UPROPERTY(ReplicatedUsing = OnRep_HasBeenScanned)
	bool bHasBeenScanned = false;

	UPROPERTY(Replicated)
	int32 AppraisedValue = 0;

	UFUNCTION()
	void OnRep_IsConsumed();

	UFUNCTION()
	void OnRep_HasPhoto();

	UFUNCTION()
	void OnRep_HasBeenScanned();

	void ApplyConsumedState(bool bConsumed);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
