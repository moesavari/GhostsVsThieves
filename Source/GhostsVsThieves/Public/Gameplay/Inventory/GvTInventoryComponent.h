#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GvTInventoryComponent.generated.h"

class AGvTInteractableItem;
class AGvTThiefCharacter;

UCLASS(ClassGroup=(GvT), meta=(BlueprintSpawnableComponent))
class GHOSTSVSTHIEVES_API UGvTInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGvTInventoryComponent();

	UFUNCTION(BlueprintPure, Category="GvT|Inventory")
	int32 GetUsedCapacity() const;

	UFUNCTION(BlueprintPure, Category="GvT|Inventory")
	int32 GetRemainingCapacity() const;

	UFUNCTION(BlueprintPure, Category="GvT|Inventory")
	AGvTInteractableItem* GetSelectedItem() const;

	/** Snapshot used by the van inventory HUD. Do not mutate the returned array. */
	UFUNCTION(BlueprintPure, Category="GvT|Inventory|UI")
	TArray<AGvTInteractableItem*> GetCarriedItems() const;

	UFUNCTION(BlueprintPure, Category="GvT|Inventory|UI")
	int32 GetMaxCapacity() const { return MaxCapacityUnits; }

	UFUNCTION(BlueprintPure, Category="GvT|Inventory|UI")
	int32 GetSelectedItemIndex() const { return SelectedItemIndex; }

	UFUNCTION(BlueprintPure, Category="GvT|Inventory")
	bool CanAddItem(const AGvTInteractableItem* Item) const;

	/** Server-only. Called by an item after its interaction completes. */
	bool TryAddItem(AGvTInteractableItem* Item);

	UFUNCTION(BlueprintCallable, Category="GvT|Inventory")
	void SelectNextItem();

	UFUNCTION(BlueprintCallable, Category="GvT|Inventory")
	void SelectPreviousItem();

	UFUNCTION(BlueprintCallable, Category="GvT|Inventory")
	void DropSelectedItem();

	/** Server-only. Removes the selected item without dropping it into the world. */
	bool TryRemoveSelectedItemForDeposit(AGvTInteractableItem*& OutItem);

	UFUNCTION(BlueprintPure, Category="GvT|Inventory")
	bool ContainsStolenLoot() const;

	/** Server-only. Drops every carried item around the owner. */
	void DropAllItemsOnDeath();

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GvT|Inventory", meta=(ClampMin="1"))
	int32 MaxCapacityUnits = 4;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GvT|Inventory")
	float DropForwardDistance = 110.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GvT|Inventory")
	float DropVerticalOffset = 20.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GvT|Inventory|Drop", meta=(ClampMin="0.0"))
	float DropSweepPadding = 8.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GvT|Inventory|Drop", meta=(ClampMin="0.1", ClampMax="1.0"))
	float DropCollisionExtentScale = 0.90f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GvT|Inventory|Drop", meta=(ClampMin="0.0"))
	float MinimumValidDropDistance = 25.f;

	UPROPERTY(ReplicatedUsing=OnRep_Inventory)
	TArray<TObjectPtr<AGvTInteractableItem>> CarriedItems;

	UPROPERTY(ReplicatedUsing=OnRep_Inventory)
	int32 SelectedItemIndex = INDEX_NONE;

	UFUNCTION()
	void OnRep_Inventory();

	UFUNCTION(Server, Reliable)
	void Server_SelectRelative(int32 Direction);

	UFUNCTION(Server, Reliable)
	void Server_DropSelectedItem();

private:
	AGvTThiefCharacter* GetOwnerThief() const;
	void SelectRelativeInternal(int32 Direction);
	void RefreshSelection();
	void DropSelectedItemInternal();
	bool FindSafeDropTransform(const AGvTInteractableItem* Item, FVector& OutLocation, FRotator& OutRotation) const;
	void RemoveItemAtIndex(int32 ItemIndex);
};
