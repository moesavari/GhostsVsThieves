#include "Gameplay/Inventory/GvTInventoryComponent.h"
#include "Gameplay/Characters/Thieves/GvTThiefCharacter.h"
#include "World/Items/GvTInteractableItem.h"
#include "Net/UnrealNetwork.h"

UGvTInventoryComponent::UGvTInventoryComponent()
{
	SetIsReplicatedByDefault(true);
}

int32 UGvTInventoryComponent::GetUsedCapacity() const
{
	int32 UsedCapacity = 0;
	for (const AGvTInteractableItem* Item : CarriedItems)
	{
		if (IsValid(Item))
		{
			UsedCapacity += Item->GetInventorySpaceCost();
		}
	}
	return UsedCapacity;
}

int32 UGvTInventoryComponent::GetRemainingCapacity() const
{
	return FMath::Max(0, MaxCapacityUnits - GetUsedCapacity());
}

AGvTInteractableItem* UGvTInventoryComponent::GetSelectedItem() const
{
	return CarriedItems.IsValidIndex(SelectedItemIndex) ? CarriedItems[SelectedItemIndex] : nullptr;
}

bool UGvTInventoryComponent::CanAddItem(const AGvTInteractableItem* Item) const
{
	return IsValid(Item) && !CarriedItems.Contains(Item) && Item->GetInventorySpaceCost() <= GetRemainingCapacity();
}

bool UGvTInventoryComponent::TryAddItem(AGvTInteractableItem* Item)
{
	AGvTThiefCharacter* Thief = GetOwnerThief();
	if (!Thief || !Thief->HasAuthority() || !CanAddItem(Item))
	{
		UE_LOG(LogTemp, Log, TEXT("[InventoryPickup] Player=%s Item=%s Result=REJECTED Used=%d Max=%d Cost=%d"), *GetNameSafe(Thief), *GetNameSafe(Item), GetUsedCapacity(), MaxCapacityUnits, IsValid(Item) ? Item->GetInventorySpaceCost() : 0);
		return false;
	}

	CarriedItems.Add(Item);
	if (SelectedItemIndex == INDEX_NONE)
	{
		SelectedItemIndex = 0;
	}

	Item->SetCarriedBy(Thief, false);
	RefreshSelection();

	UE_LOG(LogTemp, Log, TEXT("[InventoryPickup] Player=%s Item=%s Result=SUCCESS Used=%d Max=%d Cost=%d Count=%d"), *GetNameSafe(Thief), *GetNameSafe(Item), GetUsedCapacity(), MaxCapacityUnits, Item->GetInventorySpaceCost(), CarriedItems.Num());
	return true;
}

void UGvTInventoryComponent::SelectNextItem()
{
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		SelectRelativeInternal(1);
	}
	else
	{
		Server_SelectRelative(1);
	}
}

void UGvTInventoryComponent::SelectPreviousItem()
{
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		SelectRelativeInternal(-1);
	}
	else
	{
		Server_SelectRelative(-1);
	}
}

void UGvTInventoryComponent::DropSelectedItem()
{
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		DropSelectedItemInternal();
	}
	else
	{
		Server_DropSelectedItem();
	}
}

void UGvTInventoryComponent::Server_SelectRelative_Implementation(int32 Direction)
{
	SelectRelativeInternal(Direction);
}

void UGvTInventoryComponent::Server_DropSelectedItem_Implementation()
{
	DropSelectedItemInternal();
}

void UGvTInventoryComponent::SelectRelativeInternal(int32 Direction)
{
	if (CarriedItems.Num() <= 0)
	{
		SelectedItemIndex = INDEX_NONE;
		return;
	}

	SelectedItemIndex = (SelectedItemIndex + Direction + CarriedItems.Num()) % CarriedItems.Num();
	RefreshSelection();
	UE_LOG(LogTemp, Log, TEXT("[InventorySelect] Player=%s Index=%d Item=%s"), *GetNameSafe(GetOwner()), SelectedItemIndex, *GetNameSafe(GetSelectedItem()));
}

void UGvTInventoryComponent::RefreshSelection()
{
	for (int32 Index = 0; Index < CarriedItems.Num(); ++Index)
	{
		if (AGvTInteractableItem* Item = CarriedItems[Index])
		{
			Item->SetCarriedBy(GetOwnerThief(), Index == SelectedItemIndex);
		}
	}

	if (GetOwner())
	{
		GetOwner()->ForceNetUpdate();
	}
}

void UGvTInventoryComponent::DropSelectedItemInternal()
{
	AGvTThiefCharacter* Thief = GetOwnerThief();
	AGvTInteractableItem* Item = GetSelectedItem();
	if (!Thief || !Thief->HasAuthority() || !Item)
	{
		return;
	}

	const FVector DropLocation = Thief->GetActorLocation() + Thief->GetActorForwardVector() * DropForwardDistance + FVector::UpVector * DropVerticalOffset;
	CarriedItems.RemoveAt(SelectedItemIndex);

	if (CarriedItems.Num() == 0)
	{
		SelectedItemIndex = INDEX_NONE;
	}
	else
	{
		SelectedItemIndex = FMath::Clamp(SelectedItemIndex, 0, CarriedItems.Num() - 1);
	}

	Item->DropFromInventory(DropLocation, Thief->GetActorRotation());
	RefreshSelection();
	UE_LOG(LogTemp, Log, TEXT("[InventoryDrop] Player=%s Item=%s Used=%d Max=%d Count=%d"), *GetNameSafe(Thief), *GetNameSafe(Item), GetUsedCapacity(), MaxCapacityUnits, CarriedItems.Num());
}

void UGvTInventoryComponent::OnRep_Inventory()
{
	RefreshSelection();
}

AGvTThiefCharacter* UGvTInventoryComponent::GetOwnerThief() const
{
	return Cast<AGvTThiefCharacter>(GetOwner());
}

void UGvTInventoryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UGvTInventoryComponent, CarriedItems);
	DOREPLIFETIME(UGvTInventoryComponent, SelectedItemIndex);
}
