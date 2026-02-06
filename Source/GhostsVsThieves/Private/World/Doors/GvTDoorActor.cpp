#include "World/Doors/GvTDoorActor.h"

#include "Net/UnrealNetwork.h"
#include "Components/StaticMeshComponent.h"
#include "Systems/Noise/GvTNoiseEmitterComponent.h"

AGvTDoorActor::AGvTDoorActor()
{
	bReplicates = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Hinge = CreateDefaultSubobject<USceneComponent>(TEXT("Hinge"));
	Hinge->SetupAttachment(Root);

	DoorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DoorMesh"));
	DoorMesh->SetupAttachment(Hinge);
	DoorMesh->SetMobility(EComponentMobility::Movable);

	DoorNoiseTag = FGameplayTag::RequestGameplayTag(TEXT("Noise.Interact"));
}

void AGvTDoorActor::GetInteractionSpec_Implementation(APawn* InstigatorPawn, EGvTInteractionVerb Verb, FGvTInteractionSpec& OutSpec) const
{
	OutSpec = FGvTInteractionSpec{};
	OutSpec.CastTime = 0.f;          // doors are instant (per MVP build order)
	OutSpec.bLockMovement = false;
	OutSpec.bLockLook = false;
	OutSpec.bCancelable = false;
	OutSpec.bEmitNoiseOnCancel = false;
	OutSpec.CancelNoiseRadius = 0.f;
	OutSpec.CancelNoiseLoudness = 0.f;
	OutSpec.InteractionTag = DoorNoiseTag;
}

bool AGvTDoorActor::CanInteract_Implementation(APawn* InstigatorPawn, EGvTInteractionVerb Verb) const
{
	// Doors ignore Photo verb for now, but you could support it later.
	return Verb == EGvTInteractionVerb::Interact;
}

void AGvTDoorActor::BeginInteract_Implementation(APawn* InstigatorPawn, EGvTInteractionVerb Verb)
{
	// No-op for instant door.
}

void AGvTDoorActor::CompleteInteract_Implementation(APawn* InstigatorPawn, EGvTInteractionVerb Verb)
{
	if (!HasAuthority())
		return;

	if (Verb != EGvTInteractionVerb::Interact)
		return;

	bIsOpen = !bIsOpen;
	ApplyDoorState(bIsOpen);

	if (InstigatorPawn)
	{
		if (UGvTNoiseEmitterComponent* Noise = InstigatorPawn->FindComponentByClass<UGvTNoiseEmitterComponent>())
		{
			Noise->EmitNoise(DoorNoiseTag, DoorNoiseRadius, 1.0f);
		}
	}
}

void AGvTDoorActor::CancelInteract_Implementation(APawn* InstigatorPawn, EGvTInteractionVerb Verb, EGvTInteractionCancelReason Reason)
{
	// No-op
}

void AGvTDoorActor::OnRep_IsOpen()
{
	ApplyDoorState(bIsOpen);
}

void AGvTDoorActor::ApplyDoorState(bool bOpen)
{
	const float SignedOpen = bInvertDirection ? -OpenYaw : OpenYaw;
	const float TargetYaw = bOpen ? SignedOpen : ClosedYaw;
	Hinge->SetRelativeRotation(FRotator(0.f, TargetYaw, 0.f));
}

void AGvTDoorActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AGvTDoorActor, bIsOpen);
}
