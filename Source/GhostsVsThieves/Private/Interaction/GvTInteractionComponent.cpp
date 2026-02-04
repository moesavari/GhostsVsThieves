#include "Interaction/GvTInteractionComponent.h"

#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"

#include "Interaction/GvTInteractable.h"

UGvTInteractionComponent::UGvTInteractionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false); // state lives in interactables, not here
}

void UGvTInteractionComponent::TryInteract()
{
	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!OwnerPawn || !OwnerPawn->IsLocallyControlled())
	{
		return;
	}

	Server_TryInteract();
}

void UGvTInteractionComponent::TryPhoto()
{
	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!OwnerPawn || !OwnerPawn->IsLocallyControlled())
	{
		return;
	}

	Server_TryPhoto();
}

void UGvTInteractionComponent::Server_TryInteract_Implementation()
{
	PerformServerTraceAndInteract(false);
}

void UGvTInteractionComponent::Server_TryPhoto_Implementation()
{
	PerformServerTraceAndInteract(true);
}

bool UGvTInteractionComponent::GetViewTrace(FVector& OutStart, FVector& OutEnd) const
{
	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!OwnerPawn)
	{
		return false;
	}

	AController* Controller = OwnerPawn->GetController();
	APlayerController* PC = Cast<APlayerController>(Controller);
	if (!PC)
	{
		return false;
	}

	FVector ViewLoc;
	FRotator ViewRot;
	PC->GetPlayerViewPoint(ViewLoc, ViewRot);

	OutStart = ViewLoc;
	OutEnd = ViewLoc + (ViewRot.Vector() * TraceDistance);
	return true;
}

void UGvTInteractionComponent::PerformServerTraceAndInteract(bool bPhoto)
{
	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!OwnerPawn)
	{
		return;
	}

	FVector Start, End;
	if (!GetViewTrace(Start, End))
	{
		return;
	}

	FCollisionQueryParams Params(SCENE_QUERY_STAT(GvTInteractionTrace), false);
	Params.AddIgnoredActor(OwnerPawn);

	FHitResult Hit;
	const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, Start, End, TraceChannel, Params);

	if (bDebugDraw)
	{
		DrawDebugLine(GetWorld(), Start, End, bHit ? FColor::Green : FColor::Red, false, 1.0f, 0, 1.0f);
		if (bHit)
		{
			DrawDebugSphere(GetWorld(), Hit.ImpactPoint, 10.f, 12, FColor::Yellow, false, 1.0f);
		}
	}

	if (!bHit || !Hit.GetActor())
	{
		return;
	}

	AActor* HitActor = Hit.GetActor();
	if (!HitActor->GetClass()->ImplementsInterface(UGvTInteractable::StaticClass()))
	{
		return;
	}

	if (bPhoto)
	{
		IGvTInteractable::Execute_Photo(HitActor, OwnerPawn);
	}
	else
	{
		IGvTInteractable::Execute_Interact(HitActor, OwnerPawn);
	}
}
