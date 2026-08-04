#include "World/Extraction/GvTVanInventoryActor.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Gameplay/Characters/Thieves/GvTThiefCharacter.h"
#include "GvTPlayerController.h"

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
		PC->Client_OpenVanInventory();
	}
}

void AGvTVanInventoryActor::CancelInteract_Implementation(APawn* InstigatorPawn, EGvTInteractionVerb Verb, EGvTInteractionCancelReason Reason)
{
}
