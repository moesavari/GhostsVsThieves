#include "World/Extraction/GvTReconDepositActor.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Gameplay/Characters/Thieves/GvTThiefCharacter.h"
#include "Gameplay/Inventory/GvTInventoryComponent.h"
#include "World/Items/GvTInteractableItem.h"
#include "GvTGameModeBase.h"
#include "GvTGameStateBase.h"
#include "GvTPlayerController.h"

AGvTReconDepositActor::AGvTReconDepositActor()
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

void AGvTReconDepositActor::GetInteractionSpec_Implementation(APawn* InstigatorPawn, EGvTInteractionVerb Verb, FGvTInteractionSpec& OutSpec) const
{
	OutSpec = FGvTInteractionSpec{};
	if (Verb != EGvTInteractionVerb::Interact)
	{
		return;
	}

	OutSpec.CastTime = DepositCastTime;
	OutSpec.bLockMovement = bLockMovementDuringDeposit;
	OutSpec.bLockLook = bLockLookDuringDeposit;
	OutSpec.bCancelable = true;
	OutSpec.bEmitNoiseOnCancel = false;
	OutSpec.LoopSfx = DepositLoopSfx;
	OutSpec.EndSfx = DepositEndSfx;
	OutSpec.CancelSfx = DepositCancelSfx;
}

bool AGvTReconDepositActor::CanInteract_Implementation(APawn* InstigatorPawn, EGvTInteractionVerb Verb) const
{
	if (Verb != EGvTInteractionVerb::Interact)
	{
		return false;
	}

	const AGvTThiefCharacter* Thief = Cast<AGvTThiefCharacter>(InstigatorPawn);
	const UGvTInventoryComponent* Inventory = Thief ? Thief->GetInventoryComponent() : nullptr;
	const AGvTInteractableItem* Item = Inventory ? Inventory->GetSelectedItem() : nullptr;
	return IsValid(Item) && Item->IsStolenLoot();
}

void AGvTReconDepositActor::BeginInteract_Implementation(APawn* InstigatorPawn, EGvTInteractionVerb Verb)
{
}

void AGvTReconDepositActor::CompleteInteract_Implementation(APawn* InstigatorPawn, EGvTInteractionVerb Verb)
{
	if (!HasAuthority() || Verb != EGvTInteractionVerb::Interact)
	{
		return;
	}

	AGvTThiefCharacter* Thief = Cast<AGvTThiefCharacter>(InstigatorPawn);
	UGvTInventoryComponent* Inventory = Thief ? Thief->GetInventoryComponent() : nullptr;
	AGvTInteractableItem* SelectedItem = Inventory ? Inventory->GetSelectedItem() : nullptr;
	if (!Inventory || !SelectedItem || !SelectedItem->IsStolenLoot())
	{
		UE_LOG(LogTemp, Log, TEXT("[ReconDeposit] Player=%s Result=REJECTED Reason=NotStolenLoot"), *GetNameSafe(Thief));
		if (AGvTPlayerController* PC = Thief ? Cast<AGvTPlayerController>(Thief->GetController()) : nullptr)
		{
			PC->Client_ShowHUDMessage(FText::FromString(TEXT("Select stolen loot before depositing.")), false);
		}
		return;
	}

	const int32 SecuredValue = SelectedItem->GetSecuredLootValue();
	const bool bWasMainObjective = SelectedItem->IsMainObjective();
	const FText ItemDisplayName = SelectedItem->GetInventoryDisplayName();
	const FString ItemName = GetNameSafe(SelectedItem);
	AGvTInteractableItem* RemovedItem = nullptr;
	if (!Inventory->TryRemoveSelectedItemForDeposit(RemovedItem) || RemovedItem != SelectedItem)
	{
		UE_LOG(LogTemp, Log, TEXT("[ReconDeposit] Player=%s Item=%s Result=REJECTED Reason=InventoryChanged"), *GetNameSafe(Thief), *ItemName);
		if (AGvTPlayerController* PC = Thief ? Cast<AGvTPlayerController>(Thief->GetController()) : nullptr)
		{
			PC->Client_ShowHUDMessage(FText::FromString(TEXT("Deposit failed. Your inventory changed.")), false);
		}
		return;
	}

	if (AGvTGameModeBase* GM = GetWorld()->GetAuthGameMode<AGvTGameModeBase>())
	{
		GM->NotifyLootDeposited(RemovedItem, SecuredValue);
	}
	RemovedItem->Destroy();

	const AGvTGameStateBase* GS = GetWorld()->GetGameState<AGvTGameStateBase>();
	UE_LOG(LogTemp, Log, TEXT("[ReconDeposit] Player=%s Item=%s Value=%d TeamSecuredTotal=%d Result=SUCCESS"), *GetNameSafe(Thief), *ItemName, SecuredValue, GS ? GS->GetTeamSecuredLoot() : 0);
	if (AGvTPlayerController* PC = Thief ? Cast<AGvTPlayerController>(Thief->GetController()) : nullptr)
	{
		if (bWasMainObjective)
		{
			PC->Client_ShowOnboardingPrompt(EGvTOnboardingPrompt::ObjectiveSecured);
		}
		else
		{
			PC->Client_ShowHUDMessage(
				FText::Format(
					FText::FromString(TEXT("Loot secured: {0}")),
					FText::AsNumber(SecuredValue)),
				true);
		}
	}
}

void AGvTReconDepositActor::CancelInteract_Implementation(APawn* InstigatorPawn, EGvTInteractionVerb Verb, EGvTInteractionCancelReason Reason)
{
}
