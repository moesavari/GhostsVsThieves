#include "World/Extraction/GvTVanInventoryActor.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Gameplay/Characters/Thieves/GvTThiefCharacter.h"
#include "Gameplay/Inventory/GvTInventoryComponent.h"
#include "World/Items/GvTInteractableItem.h"
#include "GvTPlayerController.h"
#include "Net/UnrealNetwork.h"

AGvTVanInventoryActor::AGvTVanInventoryActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	InteractionBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractionBounds"));
	SetRootComponent(InteractionBounds);
	InteractionBounds->SetBoxExtent(FVector(100.f, 100.f, 75.f));
	InteractionBounds->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionBounds->SetCollisionObjectType(ECC_WorldDynamic);
	InteractionBounds->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractionBounds->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	PlaceholderMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlaceholderMesh"));
	PlaceholderMesh->SetupAttachment(InteractionBounds);
	PlaceholderMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AGvTVanInventoryActor::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		ItemStacks.Reset();
		for (const FGvTVanItemStack& StartingStack : StartingItemStacks)
		{
			if (!StartingStack.ItemClass || StartingStack.Quantity <= 0)
			{
				continue;
			}

			FGvTVanItemStack* ExistingStack = ItemStacks.FindByPredicate([&StartingStack](const FGvTVanItemStack& Stack)
			{
				return Stack.ItemClass == StartingStack.ItemClass;
			});
			if (ExistingStack)
			{
				ExistingStack->Quantity += StartingStack.Quantity;
			}
			else
			{
				ItemStacks.Add(StartingStack);
			}
		}
		ForceNetUpdate();
	}
}

void AGvTVanInventoryActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AGvTVanInventoryActor, ItemStacks);
}

void AGvTVanInventoryActor::GetInteractionSpec_Implementation(APawn* InstigatorPawn, EGvTInteractionVerb Verb, FGvTInteractionSpec& OutSpec) const
{
	OutSpec = FGvTInteractionSpec{};
	if (Verb != EGvTInteractionVerb::Interact) return;
	OutSpec.CastTime = OpenCastTime;
	OutSpec.bLockMovement = true;
	OutSpec.bLockLook = false;
	OutSpec.bCancelable = true;
	OutSpec.bEmitNoiseOnCancel = false;
	OutSpec.LoopSfx = OpenLoopSfx;
	OutSpec.EndSfx = OpenEndSfx;
	OutSpec.CancelSfx = OpenCancelSfx;
}

bool AGvTVanInventoryActor::CanInteract_Implementation(APawn* InstigatorPawn, EGvTInteractionVerb Verb) const
{
	const AGvTThiefCharacter* Thief = Cast<AGvTThiefCharacter>(InstigatorPawn);
	return Verb == EGvTInteractionVerb::Interact && IsValid(Thief) && !Thief->IsDead();
}

void AGvTVanInventoryActor::BeginInteract_Implementation(APawn* InstigatorPawn, EGvTInteractionVerb Verb)
{
}

void AGvTVanInventoryActor::CompleteInteract_Implementation(APawn* InstigatorPawn, EGvTInteractionVerb Verb)
{
	if (!HasAuthority() || Verb != EGvTInteractionVerb::Interact) return;
	if (AGvTPlayerController* PC = InstigatorPawn ? Cast<AGvTPlayerController>(InstigatorPawn->GetController()) : nullptr)
	{
		PC->Client_OpenVanInventory(this);
	}
}

void AGvTVanInventoryActor::CancelInteract_Implementation(APawn* InstigatorPawn, EGvTInteractionVerb Verb, EGvTInteractionCancelReason Reason)
{
}

bool AGvTVanInventoryActor::TryTakeItem(AGvTThiefCharacter* Thief, int32 StackIndex, FText& OutFailureMessage)
{
	OutFailureMessage = FText::GetEmpty();
	if (!HasAuthority() || !IsValid(Thief) || Thief->IsDead())
	{
		OutFailureMessage = NSLOCTEXT("GvTVanInventory", "InvalidPlayer", "Unable to take that item.");
		return false;
	}

	if (FVector::DistSquared(Thief->GetActorLocation(), GetActorLocation()) > FMath::Square(MaximumTakeDistance))
	{
		OutFailureMessage = NSLOCTEXT("GvTVanInventory", "TooFar", "Move closer to the van inventory.");
		return false;
	}

	if (!ItemStacks.IsValidIndex(StackIndex) || !ItemStacks[StackIndex].ItemClass || ItemStacks[StackIndex].Quantity <= 0)
	{
		OutFailureMessage = NSLOCTEXT("GvTVanInventory", "NoLongerAvailable", "That item is no longer available.");
		return false;
	}

	UGvTInventoryComponent* Inventory = Thief->GetInventoryComponent();
	const AGvTInteractableItem* ItemDefaults = ItemStacks[StackIndex].ItemClass->GetDefaultObject<AGvTInteractableItem>();
	const int32 RequiredCapacity = ItemDefaults ? ItemDefaults->GetInventorySpaceCost() : 1;
	if (!Inventory || RequiredCapacity > Inventory->GetRemainingCapacity())
	{
		OutFailureMessage = FText::Format(NSLOCTEXT("GvTVanInventory", "InventoryFull", "Inventory full - requires {0} available slots."), FText::AsNumber(RequiredCapacity));
		return false;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = Thief;
	SpawnParams.Instigator = Thief;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AGvTInteractableItem* SpawnedItem = GetWorld()->SpawnActor<AGvTInteractableItem>(ItemStacks[StackIndex].ItemClass, GetActorTransform(), SpawnParams);
	if (!SpawnedItem || !Inventory->TryAddItem(SpawnedItem))
	{
		if (SpawnedItem)
		{
			SpawnedItem->Destroy();
		}
		OutFailureMessage = NSLOCTEXT("GvTVanInventory", "TransferFailed", "Unable to transfer that item.");
		return false;
	}

	--ItemStacks[StackIndex].Quantity;
	// Keep the slot and class stable at zero. A delayed click can then only fail;
	// it can never shift to and accidentally take a different stack.
	OnRep_ItemStacks();
	ForceNetUpdate();
	return true;
}

void AGvTVanInventoryActor::OnRep_ItemStacks()
{
	OnVanInventoryChanged.Broadcast();
}

FGvTVanItemHoverInfo AGvTVanInventoryActor::GetItemHoverInfo(int32 StackIndex) const
{
	FGvTVanItemHoverInfo Result;
	if (!ItemStacks.IsValidIndex(StackIndex))
	{
		return Result;
	}

	const FGvTVanItemStack& Stack = ItemStacks[StackIndex];
	const AGvTInteractableItem* ItemDefaults = Stack.ItemClass ? Stack.ItemClass->GetDefaultObject<AGvTInteractableItem>() : nullptr;
	if (!ItemDefaults)
	{
		return Result;
	}

	Result.bIsValid = true;
	Result.DisplayName = ItemDefaults->GetInventoryDisplayName();
	Result.Description = ItemDefaults->GetInventoryDescription();
	Result.Icon = ItemDefaults->GetInventoryIcon();
	Result.Quantity = FMath::Max(0, Stack.Quantity);
	Result.InventorySpaceCost = ItemDefaults->GetInventorySpaceCost();
	return Result;
}
