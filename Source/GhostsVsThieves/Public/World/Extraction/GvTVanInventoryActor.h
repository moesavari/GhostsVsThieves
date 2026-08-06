#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Gameplay/Interaction/GvTInteractable.h"
#include "GvTVanInventoryActor.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class USoundBase;
class AGvTInteractableItem;
class AGvTThiefCharacter;

USTRUCT(BlueprintType)
struct FGvTVanItemStack
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="GvT|Van Inventory")
	TSubclassOf<AGvTInteractableItem> ItemClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="GvT|Van Inventory", meta=(ClampMin="0"))
	int32 Quantity = 0;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FGvTVanInventoryChanged);

UCLASS()
class GHOSTSVSTHIEVES_API AGvTVanInventoryActor : public AActor, public IGvTInteractable
{
	GENERATED_BODY()

public:
	AGvTVanInventoryActor();

	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual void GetInteractionSpec_Implementation(APawn* InstigatorPawn, EGvTInteractionVerb Verb, FGvTInteractionSpec& OutSpec) const override;
	virtual bool CanInteract_Implementation(APawn* InstigatorPawn, EGvTInteractionVerb Verb) const override;
	virtual void BeginInteract_Implementation(APawn* InstigatorPawn, EGvTInteractionVerb Verb) override;
	virtual void CompleteInteract_Implementation(APawn* InstigatorPawn, EGvTInteractionVerb Verb) override;
	virtual void CancelInteract_Implementation(APawn* InstigatorPawn, EGvTInteractionVerb Verb, EGvTInteractionCancelReason Reason) override;

	UFUNCTION(BlueprintPure, Category="GvT|Van Inventory")
	TArray<FGvTVanItemStack> GetItemStacks() const { return ItemStacks; }

	UFUNCTION(BlueprintPure, Category="GvT|Van Inventory")
	int32 GetStorageSlotCount() const { return StorageSlotCount; }

	/** Server-only. Validates and transfers one item from a stack to the requesting player. */
	bool TryTakeItem(AGvTThiefCharacter* Thief, int32 StackIndex, FText& OutFailureMessage);

	UPROPERTY(BlueprintAssignable, Category="GvT|Van Inventory")
	FGvTVanInventoryChanged OnVanInventoryChanged;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GvT|Van Inventory")
	TObjectPtr<UBoxComponent> InteractionBounds;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GvT|Van Inventory")
	TObjectPtr<UStaticMeshComponent> PlaceholderMesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GvT|Van Inventory", meta=(ClampMin="0.0"))
	float OpenCastTime = 0.25f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GvT|Van Inventory")
	TObjectPtr<USoundBase> OpenLoopSfx;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GvT|Van Inventory")
	TObjectPtr<USoundBase> OpenEndSfx;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GvT|Van Inventory")
	TObjectPtr<USoundBase> OpenCancelSfx;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="GvT|Van Inventory|Storage")
	TArray<FGvTVanItemStack> StartingItemStacks;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GvT|Van Inventory|Storage", meta=(ClampMin="1"))
	int32 StorageSlotCount = 16;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GvT|Van Inventory|Storage", meta=(ClampMin="100.0"))
	float MaximumTakeDistance = 500.f;

	UPROPERTY(ReplicatedUsing=OnRep_ItemStacks, BlueprintReadOnly, Category="GvT|Van Inventory|Storage")
	TArray<FGvTVanItemStack> ItemStacks;

	UFUNCTION()
	void OnRep_ItemStacks();
};
