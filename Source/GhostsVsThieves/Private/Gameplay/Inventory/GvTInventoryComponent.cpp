#include "Gameplay/Inventory/GvTInventoryComponent.h"
#include "Gameplay/Characters/Thieves/GvTThiefCharacter.h"
#include "World/Items/GvTInteractableItem.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"

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

	FVector DropLocation = FVector::ZeroVector;
	FRotator DropRotation = FRotator::ZeroRotator;
	if (!FindSafeDropTransform(Item, DropLocation, DropRotation))
	{
		UE_LOG(LogTemp, Log, TEXT("[InventoryDrop] Player=%s Item=%s Result=REJECTED Reason=NoSafeDropLocation"), *GetNameSafe(Thief), *GetNameSafe(Item));
		return;
	}

	const int32 RemovedIndex = SelectedItemIndex;
	RemoveItemAtIndex(RemovedIndex);
	Item->DropFromInventory(DropLocation, DropRotation);
	RefreshSelection();
	UE_LOG(LogTemp, Log, TEXT("[InventoryDrop] Player=%s Item=%s Result=SUCCESS Used=%d Max=%d Count=%d Location=%s"), *GetNameSafe(Thief), *GetNameSafe(Item), GetUsedCapacity(), MaxCapacityUnits, CarriedItems.Num(), *DropLocation.ToCompactString());
}

bool UGvTInventoryComponent::TryRemoveSelectedItemForDeposit(AGvTInteractableItem*& OutItem)
{
	OutItem = nullptr;

	AGvTThiefCharacter* Thief = GetOwnerThief();
	AGvTInteractableItem* Item = GetSelectedItem();
	if (!Thief || !Thief->HasAuthority() || !Item)
	{
		return false;
	}

	const int32 RemovedIndex = SelectedItemIndex;
	RemoveItemAtIndex(RemovedIndex);
	RefreshSelection();
	OutItem = Item;

	UE_LOG(LogTemp, Log, TEXT("[InventoryDeposit] Player=%s Item=%s Result=REMOVED Used=%d Max=%d Count=%d"), *GetNameSafe(Thief), *GetNameSafe(Item), GetUsedCapacity(), MaxCapacityUnits, CarriedItems.Num());
	return true;
}

bool UGvTInventoryComponent::FindSafeDropTransform(const AGvTInteractableItem* Item, FVector& OutLocation, FRotator& OutRotation) const
{
	const AGvTThiefCharacter* Thief = GetOwnerThief();
	UWorld* World = GetWorld();
	if (!Thief || !World || !Item)
	{
		return false;
	}

	OutRotation = Thief->GetActorRotation();

	const FVector Forward = Thief->GetActorForwardVector().GetSafeNormal();
	const FVector Start = Thief->GetActorLocation() + FVector::UpVector * DropVerticalOffset;
	const FVector DesiredEnd = Start + Forward * DropForwardDistance;
	const FVector RawExtent = Item->GetDropCollisionExtent();
	const FVector SweepExtent = FVector(
		FMath::Max(4.f, RawExtent.X * DropCollisionExtentScale),
		FMath::Max(4.f, RawExtent.Y * DropCollisionExtentScale),
		FMath::Max(4.f, RawExtent.Z * DropCollisionExtentScale));

	FCollisionObjectQueryParams ObjectQuery;
	ObjectQuery.AddObjectTypesToQuery(ECC_WorldStatic);
	ObjectQuery.AddObjectTypesToQuery(ECC_WorldDynamic);
	ObjectQuery.AddObjectTypesToQuery(ECC_PhysicsBody);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(GvTSafeInventoryDrop), false);
	QueryParams.AddIgnoredActor(Thief);
	QueryParams.AddIgnoredActor(Item);

	FHitResult Hit;
	const FCollisionShape Shape = FCollisionShape::MakeBox(SweepExtent);
	const bool bBlocked = World->SweepSingleByObjectType(Hit, Start, DesiredEnd, OutRotation.Quaternion(), ObjectQuery, Shape, QueryParams);

	if (bBlocked && Hit.bStartPenetrating)
	{
		return false;
	}

	OutLocation = bBlocked ? Hit.Location - Forward * DropSweepPadding : DesiredEnd;
	if (FVector::DistSquared(Start, OutLocation) < FMath::Square(MinimumValidDropDistance))
	{
		return false;
	}

	const bool bOverlapping = World->OverlapBlockingTestByChannel(OutLocation, OutRotation.Quaternion(), ECC_WorldStatic, Shape, QueryParams);

	if (bOverlapping)
	{
		return false;
	}

	const bool bOverlappingDynamic = World->OverlapBlockingTestByChannel(OutLocation, OutRotation.Quaternion(), ECC_WorldDynamic, Shape, QueryParams);
	return !bOverlappingDynamic;
}

void UGvTInventoryComponent::RemoveItemAtIndex(int32 ItemIndex)
{
	if (!CarriedItems.IsValidIndex(ItemIndex))
	{
		return;
	}

	CarriedItems.RemoveAt(ItemIndex);
	if (CarriedItems.Num() == 0)
	{
		SelectedItemIndex = INDEX_NONE;
	}
	else
	{
		SelectedItemIndex = FMath::Clamp(ItemIndex, 0, CarriedItems.Num() - 1);
	}
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
