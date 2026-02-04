#include "World/Items/GvTInteractableItem.h"

#include "Net/UnrealNetwork.h"
#include "Components/StaticMeshComponent.h"
#include "Systems/Noise/GvTNoiseEmitterComponent.h"
#include "GvTPlayerState.h"
#include "GameFramework/PlayerController.h"

AGvTInteractableItem::AGvTInteractableItem()
{
	bReplicates = true;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	Mesh->SetMobility(EComponentMobility::Movable);

	InteractNoiseTag = FGameplayTag::RequestGameplayTag(TEXT("Noise.Interact"));
	PhotoNoiseTag = FGameplayTag::RequestGameplayTag(TEXT("Noise.Photo"));
}

void AGvTInteractableItem::Interact_Implementation(APawn* InstigatorPawn)
{
	if (!HasAuthority() || bIsConsumed)
		return;

	if (InstigatorPawn)
	{
		if (UGvTNoiseEmitterComponent* Noise = InstigatorPawn->FindComponentByClass<UGvTNoiseEmitterComponent>())
		{
			Noise->EmitNoise(InteractNoiseTag, InteractNoiseRadius, 1.0f);
		}
	}

	if (bConsumedOnInteract)
	{
		bIsConsumed = true;
		ApplyConsumedState(true);
	}
}

void AGvTInteractableItem::Photo_Implementation(APawn* InstigatorPawn)
{
	if (!HasAuthority() || bHasBeenPhotographed || bIsConsumed)
		return;

	bHasBeenPhotographed = true;

	if (InstigatorPawn)
	{
		if (UGvTNoiseEmitterComponent* Noise = InstigatorPawn->FindComponentByClass<UGvTNoiseEmitterComponent>())
		{
			Noise->EmitNoise(PhotoNoiseTag, PhotoNoiseRadius, 1.0f);
		}

		APlayerController* PC = Cast<APlayerController>(InstigatorPawn->GetController());
		if (PC)
		{
			AGvTPlayerState* PS = PC->GetPlayerState<AGvTPlayerState>();
			if (PS)
			{
				const int32 PhotoScore = FMath::RoundToInt(static_cast<float>(BaseValue) * PhotoMultiplier);
				PS->AddLoot(PhotoScore);
			}
		}
	}
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

void AGvTInteractableItem::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AGvTInteractableItem, bIsConsumed);
	DOREPLIFETIME(AGvTInteractableItem, bHasBeenPhotographed);
}
