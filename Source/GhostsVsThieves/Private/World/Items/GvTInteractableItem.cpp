#include "World/Items/GvTInteractableItem.h"

#include "Net/UnrealNetwork.h"
#include "Components/StaticMeshComponent.h"
#include "Systems/Noise/GvTNoiseEmitterComponent.h"
#include "GvTPlayerState.h"
#include "GvTPlayerController.h"
#include "GameFramework/PlayerController.h"

AGvTInteractableItem::AGvTInteractableItem()
{
	bReplicates = true;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	Mesh->SetMobility(EComponentMobility::Movable);

	InteractNoiseTag = FGameplayTag::RequestGameplayTag(TEXT("Noise.Interact"));
	PhotoNoiseTag = FGameplayTag::RequestGameplayTag(TEXT("Noise.Photo"));
	ScanNoiseTag = FGameplayTag::RequestGameplayTag(TEXT("Noise.Scan"));
}

void AGvTInteractableItem::GetInteractionSpec_Implementation(APawn* InstigatorPawn, EGvTInteractionVerb Verb, FGvTInteractionSpec& OutSpec) const
{
	OutSpec = FGvTInteractionSpec{};

	if (Verb == EGvTInteractionVerb::Scan)
	{
		OutSpec.CastTime = ScanCastTime;
		OutSpec.bLockMovement = true;
		OutSpec.bLockLook = true;
		OutSpec.bCancelable = true;

		OutSpec.bEmitNoiseOnCancel = true;
		OutSpec.CancelNoiseRadius = ScanNoiseRadius * 0.75f;
		OutSpec.CancelNoiseLoudness = 1.0f;
		OutSpec.InteractionTag = ScanNoiseTag;

		OutSpec.LoopSfx = ScanLoopSfx;
		OutSpec.EndSfx = ScanEndSfx;
		OutSpec.CancelSfx = ScanCancelSfx;
	}
	else if (Verb == EGvTInteractionVerb::Photo)
	{
		OutSpec.CastTime = PhotoCastTime;
		OutSpec.bLockMovement = true;
		OutSpec.bLockLook = true;
		OutSpec.bCancelable = true;

		OutSpec.bEmitNoiseOnCancel = true;
		OutSpec.CancelNoiseRadius = PhotoNoiseRadius * 0.75f;
		OutSpec.CancelNoiseLoudness = 1.0f;
		OutSpec.InteractionTag = PhotoNoiseTag;
	}
	else
	{
		OutSpec.CastTime = InteractCastTime;
		OutSpec.bLockMovement = bLockMoveDuringInteract;
		OutSpec.bLockLook = bLockLookDuringInteract;
		OutSpec.bCancelable = true;

		OutSpec.bEmitNoiseOnCancel = true;
		OutSpec.CancelNoiseRadius = InteractNoiseRadius * 0.75f;
		OutSpec.CancelNoiseLoudness = 1.0f;
		OutSpec.InteractionTag = InteractNoiseTag;

		OutSpec.LoopSfx = InteractLoopSfx;
		OutSpec.EndSfx = InteractEndSfx;
		OutSpec.CancelSfx = InteractCancelSfx;
	}
}

bool AGvTInteractableItem::CanInteract_Implementation(APawn* InstigatorPawn, EGvTInteractionVerb Verb) const
{
	if (bIsConsumed)
	{
		return false;
	}
	if (Verb == EGvTInteractionVerb::Scan)
	{
		return !bHasBeenScanned && !bIsConsumed;
	}
	return true;
}

void AGvTInteractableItem::BeginInteract_Implementation(APawn* InstigatorPawn, EGvTInteractionVerb Verb)
{
	// Optional: play a local SFX/FX via multicast later.
}

void AGvTInteractableItem::CompleteInteract_Implementation(APawn* InstigatorPawn, EGvTInteractionVerb Verb)
{
	if (!HasAuthority())
	{
		return;
	}

	if (Verb == EGvTInteractionVerb::Scan)
	{
		if (bHasBeenScanned || bIsConsumed)
			return;

		bHasBeenScanned = true;

		if (InstigatorPawn)
		{
			if (UGvTNoiseEmitterComponent* Noise = InstigatorPawn->FindComponentByClass<UGvTNoiseEmitterComponent>())
			{
				Noise->EmitNoise(ScanNoiseTag, ScanNoiseRadius, 1.0f);
			}
		}

		AppraisedValue = FMath::RoundToInt(float(BaseValue) * ScanMultiplier);

		if (AGvTPlayerController* PC = InstigatorPawn ? Cast<AGvTPlayerController>(InstigatorPawn->GetController()) : nullptr)
		{
			PC->Client_ShowScanResult(this, DisplayName, AppraisedValue);
		}

		return;
	}

	// Verb == Interact
	if (bIsConsumed)
		return;

	if (InstigatorPawn)
	{
		if (UGvTNoiseEmitterComponent* Noise = InstigatorPawn->FindComponentByClass<UGvTNoiseEmitterComponent>())
		{
			Noise->EmitNoise(InteractNoiseTag, InteractNoiseRadius, 1.f);
		}
	}

	if (InstigatorPawn)
	{
		APlayerController* PC = Cast<APlayerController>(InstigatorPawn->GetController());
		if (PC)
		{
			AGvTPlayerState* PS = PC->GetPlayerState<AGvTPlayerState>();
			if (PS)
			{
				const int32 FinalValue = bHasBeenPhotographed
					? FMath::Max(AppraisedValue, 0)
					: BaseValue;

				PS->AddLoot(FinalValue);
			}
		}
	}

	if (bConsumedOnInteract)
	{
		bIsConsumed = true;
		ApplyConsumedState(true);
	}
}

void AGvTInteractableItem::CancelInteract_Implementation(APawn* InstigatorPawn, EGvTInteractionVerb Verb, EGvTInteractionCancelReason Reason)
{
	// Optional: stop SFX/FX. Noise-on-cancel is handled by InteractionComponent.
}

void AGvTInteractableItem::ApplyConsumedState(bool bConsumed)
{
	SetActorEnableCollision(!bConsumed);
	SetActorHiddenInGame(bConsumed);
}

void AGvTInteractableItem::OnRep_IsConsumed()
{
	ApplyConsumedState(bIsConsumed);
}

void AGvTInteractableItem::OnRep_HasPhoto()
{
	// Placeholder
}

void AGvTInteractableItem::OnRep_HasBeenScanned()
{
	// Optional: update material/outline/UI later
}

void AGvTInteractableItem::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AGvTInteractableItem, bIsConsumed);
	DOREPLIFETIME(AGvTInteractableItem, bHasBeenPhotographed);
	DOREPLIFETIME(AGvTInteractableItem, AppraisedValue);
	DOREPLIFETIME(AGvTInteractableItem, bHasBeenScanned);
}
