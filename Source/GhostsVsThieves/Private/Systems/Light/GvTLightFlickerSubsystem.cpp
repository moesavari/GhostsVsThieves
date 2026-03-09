#include "Systems/Light/GvTLightFlickerSubsystem.h"
#include "Systems/Light/GvTLightFlickerComponent.h"

void UGvTLightFlickerSubsystem::RegisterFlickerComponent(UGvTLightFlickerComponent* Component)
{
	if (IsValid(Component))
	{
		RegisteredComponents.Add(Component);
	}
}

void UGvTLightFlickerSubsystem::UnregisterFlickerComponent(UGvTLightFlickerComponent* Component)
{
	if (Component)
	{
		RegisteredComponents.Remove(Component);
	}
}

void UGvTLightFlickerSubsystem::CleanupInvalid()
{
	for (auto It = RegisteredComponents.CreateIterator(); It; ++It)
	{
		if (!IsValid(*It))
		{
			It.RemoveCurrent();
		}
	}
}

void UGvTLightFlickerSubsystem::PlayFlickerEvent(const FGvTLightFlickerEvent& Event)
{
	CleanupInvalid();

	for (UGvTLightFlickerComponent* Comp : RegisteredComponents)
	{
		if (!IsValid(Comp))
		{
			continue;
		}

		if (Event.bWholeHouse)
		{
			Comp->StartFlicker(Event);
			continue;
		}

		const AActor* Owner = Comp->GetOwner();
		if (!Owner)
		{
			continue;
		}

		const float DistSq = FVector::DistSquared(Owner->GetActorLocation(), Event.WorldCenter);
		if (DistSq <= FMath::Square(Event.Radius))
		{
			Comp->StartFlicker(Event);
		}
	}
}