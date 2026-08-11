#include "World/Items/GvTMedicineItem.h"

#include "Gameplay/Characters/Thieves/GvTThiefCharacter.h"
#include "Gameplay/Inventory/GvTInventoryComponent.h"
#include "GvTPlayerState.h"

AGvTMedicineItem::AGvTMedicineItem()
{
	ItemPurpose = EGvTItemPurpose::Consumable;
	bUpsetsGhostsOnInteract = false;
	bTreatAsValuableForGhosts = false;
}

bool AGvTMedicineItem::CanUseMedicine(const AGvTThiefCharacter* Thief) const
{
	if (!IsValid(Thief) || Thief->IsDead() || !IsCarried())
	{
		return false;
	}

	const UGvTInventoryComponent* Inventory = Thief->GetInventoryComponent();
	const AGvTPlayerState* PlayerState = Thief->GetPlayerState<AGvTPlayerState>();
	return Inventory && Inventory->GetSelectedItem() == this && PlayerState && !PlayerState->IsDeadForPanic() && PlayerState->GetPanic01() > KINDA_SMALL_NUMBER;
}

void AGvTMedicineItem::BuildMedicineUseSpec(FGvTInteractionSpec& OutSpec) const
{
	OutSpec = FGvTInteractionSpec{};
	OutSpec.CastTime = MedicineUseDuration;
	OutSpec.bLockMovement = true;
	OutSpec.bLockLook = false;
	OutSpec.bCancelable = true;
	OutSpec.bEmitNoiseOnCancel = false;
	OutSpec.LoopSfx = MedicineUseLoopSfx;
	OutSpec.EndSfx = MedicineUseCompleteSfx;
	OutSpec.CancelSfx = MedicineUseCancelSfx;
}

bool AGvTMedicineItem::CanInteract_Implementation(APawn* InstigatorPawn, EGvTInteractionVerb Verb) const
{
	if (IsCarried())
	{
		return Verb == EGvTInteractionVerb::Interact && CanUseMedicine(Cast<AGvTThiefCharacter>(InstigatorPawn));
	}
	return Super::CanInteract_Implementation(InstigatorPawn, Verb);
}

void AGvTMedicineItem::GetInteractionSpec_Implementation(APawn* InstigatorPawn, EGvTInteractionVerb Verb, FGvTInteractionSpec& OutSpec) const
{
	if (IsCarried() && Verb == EGvTInteractionVerb::Interact)
	{
		BuildMedicineUseSpec(OutSpec);
		return;
	}
	Super::GetInteractionSpec_Implementation(InstigatorPawn, Verb, OutSpec);
}

void AGvTMedicineItem::CompleteInteract_Implementation(APawn* InstigatorPawn, EGvTInteractionVerb Verb)
{
	if (!IsCarried())
	{
		Super::CompleteInteract_Implementation(InstigatorPawn, Verb);
		return;
	}

	AGvTThiefCharacter* Thief = Cast<AGvTThiefCharacter>(InstigatorPawn);
	UGvTInventoryComponent* Inventory = Thief ? Thief->GetInventoryComponent() : nullptr;
	AGvTPlayerState* PlayerState = Thief ? Thief->GetPlayerState<AGvTPlayerState>() : nullptr;
	if (!HasAuthority() || !CanUseMedicine(Thief) || !Inventory || !PlayerState)
	{
		return;
	}

	if (PlayerState->ApplyMedicineAuthority(PanicReduction01) && Inventory->ConsumeSelectedItem(this))
	{
		UE_LOG(LogTemp, Log, TEXT("[Medicine] Player=%s Item=%s Result=CONSUMED Reduction=%.2f"), *GetNameSafe(Thief), *GetNameSafe(this), PanicReduction01);
	}
}
