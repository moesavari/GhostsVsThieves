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

void AGvTDoorActor::Interact_Implementation(APawn* InstigatorPawn)
{
	if (!HasAuthority())
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
