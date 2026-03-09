#include "Systems/Light/GvTLightFlickerComponent.h"
#include "Components/LightComponent.h"
#include "Components/ChildActorComponent.h"
#include "GameFramework/Actor.h"
#include "Systems/Light/GvTLightFlickerSubsystem.h"

UGvTLightFlickerComponent::UGvTLightFlickerComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	SetAutoActivate(true);
}

void UGvTLightFlickerComponent::BeginPlay()
{
	Super::BeginPlay();

	CacheLights();

	UE_LOG(LogTemp, Warning, TEXT("Flicker cached %d lights on %s"), CachedLights.Num(), *GetNameSafe(GetOwner()));

	if (UWorld* World = GetWorld())
	{
		if (UGvTLightFlickerSubsystem* Subsystem = World->GetSubsystem<UGvTLightFlickerSubsystem>())
		{
			Subsystem->RegisterFlickerComponent(this);
		}
	}
}

void UGvTLightFlickerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopFlicker();

	if (UWorld* World = GetWorld())
	{
		if (UGvTLightFlickerSubsystem* Subsystem = World->GetSubsystem<UGvTLightFlickerSubsystem>())
		{
			Subsystem->UnregisterFlickerComponent(this);
		}
	}

	Super::EndPlay(EndPlayReason);
}

void UGvTLightFlickerComponent::GatherLightsFromActor(AActor* Actor, TArray<ULightComponent*>& OutLights) const
{
	if (!IsValid(Actor))
	{
		return;
	}

	TArray<ULightComponent*> LocalLights;
	Actor->GetComponents<ULightComponent>(LocalLights);

	for (ULightComponent* Light : LocalLights)
	{
		if (IsValid(Light))
		{
			OutLights.AddUnique(Light);
		}
	}

	TArray<UChildActorComponent*> ChildActorComps;
	Actor->GetComponents<UChildActorComponent>(ChildActorComps);

	for (UChildActorComponent* ChildComp : ChildActorComps)
	{
		if (!IsValid(ChildComp))
		{
			continue;
		}

		if (AActor* ChildActor = ChildComp->GetChildActor())
		{
			GatherLightsFromActor(ChildActor, OutLights);
		}
	}
}

void UGvTLightFlickerComponent::CacheLights()
{
	CachedLights.Reset();

	AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor))
	{
		return;
	}

	TArray<UChildActorComponent*> ChildActorComps;
	OwnerActor->GetComponents<UChildActorComponent>(ChildActorComps);

	for (UChildActorComponent* ChildComp : ChildActorComps)
	{
		GatherLightsFromChildActorComponent(ChildComp);
	}
}

void UGvTLightFlickerComponent::StartFlicker(const FGvTLightFlickerEvent& Event)
{
	if (CachedLights.Num() == 0)
	{
		CacheLights();
	}

	if (CachedLights.Num() == 0)
	{
		return;
	}

	ActiveEvent = Event;
	FlickerStream.Initialize(Event.Seed ^ GetTypeHash(GetOwner()));
	bIsFlickering = true;

	const float Now = GetWorld() ? GetWorld()->TimeSeconds : 0.f;
	FlickerEndTime = Now + FMath::Max(0.01f, Event.Duration);
	NextPulseTime = Now;

	SetComponentTickEnabled(true);
}

void UGvTLightFlickerComponent::StopFlicker()
{
	if (!bIsFlickering)
	{
		return;
	}

	bIsFlickering = false;
	SetComponentTickEnabled(false);
	RestoreDefaults();
	ApplyCurrentPowerState();
}

void UGvTLightFlickerComponent::RestoreDefaults()
{
	for (FGvTLightDefaultState& State : CachedLights)
	{
		if (!IsValid(State.Light))
		{
			continue;
		}

		State.Light->SetVisibility(State.bVisible);
		State.Light->SetIntensity(State.Intensity);
	}
}

void UGvTLightFlickerComponent::ApplyPulse()
{
	const float Intensity01 = FMath::Clamp(ActiveEvent.Intensity01, 0.f, 1.f);

	for (FGvTLightDefaultState& State : CachedLights)
	{
		if (ActiveEvent.ZoneName != NAME_None && State.ZoneName != ActiveEvent.ZoneName)
		{
			continue;
		}

		if (!IsValid(State.Light))
		{
			continue;
		}

		bool bTurnOff = false;
		if (ActiveEvent.bAllowFullOffBursts)
		{
			const float OffChance = FMath::Lerp(0.05f, 0.35f, Intensity01);
			bTurnOff = FlickerStream.FRand() < OffChance;
		}

		if (bTurnOff)
		{
			State.Light->SetVisibility(false);
			continue;
		}

		State.Light->SetVisibility(State.bVisible);

		const float Alpha = FlickerStream.FRandRange(
			FMath::Min(ActiveEvent.MinDimAlpha, ActiveEvent.MaxDimAlpha),
			FMath::Max(ActiveEvent.MinDimAlpha, ActiveEvent.MaxDimAlpha));

		const float BoostedAlpha = FMath::Lerp(1.0f, Alpha, Intensity01);
		State.Light->SetIntensity(State.Intensity * BoostedAlpha);
	}
}

void UGvTLightFlickerComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bIsFlickering || !GetWorld())
	{
		return;
	}

	const float Now = GetWorld()->TimeSeconds;
	if (Now >= FlickerEndTime)
	{
		StopFlicker();
		return;
	}

	if (Now >= NextPulseTime)
	{
		ApplyPulse();

		NextPulseTime = Now + FlickerStream.FRandRange(
			FMath::Max(0.001f, ActiveEvent.PulseIntervalMin),
			FMath::Max(ActiveEvent.PulseIntervalMin, ActiveEvent.PulseIntervalMax));
	}
}

bool UGvTLightFlickerComponent::HasTag(const TArray<FName>& Tags, FName TagName) const
{
	for (const FName& Tag : Tags)
	{
		if (Tag == TagName)
		{
			return true;
		}
	}
	return false;
}

FName UGvTLightFlickerComponent::ResolveZoneFromTags(const TArray<FName>& Tags) const
{
	for (const FName& Tag : Tags)
	{
		const FString TagStr = Tag.ToString();
		if (TagStr.StartsWith(TEXT("LightZone.")))
		{
			return FName(*TagStr.RightChop(10));
		}
	}
	return NAME_None;
}

void UGvTLightFlickerComponent::GatherLightsFromChildActorComponent(UChildActorComponent* ChildComp)
{
	if (!IsValid(ChildComp))
	{
		return;
	}

	AActor* ChildActor = ChildComp->GetChildActor();
	if (!IsValid(ChildActor))
	{
		return;
	}

	const FName ZoneName = ResolveZoneFromTags(ChildComp->ComponentTags);
	const bool bIgnoreHousePower = HasTag(ChildComp->ComponentTags, FName(TEXT("Power.Ignore")));

	TArray<ULightComponent*> ChildLights;
	ChildActor->GetComponents<ULightComponent>(ChildLights);

	for (ULightComponent* Light : ChildLights)
	{
		if (!IsValid(Light))
		{
			continue;
		}

		FGvTLightDefaultState State;
		State.Light = Light;
		State.Intensity = Light->Intensity;
		State.bVisible = Light->IsVisible();
		State.ZoneName = ZoneName;
		State.bAffectedByHousePower = !bIgnoreHousePower;
		CachedLights.Add(State);
	}
}

void UGvTLightFlickerComponent::SetHousePowerEnabled(bool bEnabled)
{
	bHousePowerEnabled = bEnabled;
	ApplyCurrentPowerState();
}

void UGvTLightFlickerComponent::SetZonePowerEnabled(FName ZoneName, bool bEnabled)
{
	if (ZoneName == NAME_None)
	{
		return;
	}

	ZonePowerOverrides.FindOrAdd(ZoneName) = bEnabled;
	ApplyCurrentPowerState();
}

void UGvTLightFlickerComponent::ApplyCurrentPowerState()
{
	for (FGvTLightDefaultState& State : CachedLights)
	{
		if (!IsValid(State.Light))
		{
			continue;
		}

		if (!State.bAffectedByHousePower)
		{
			State.Light->SetVisibility(State.bVisible);
			State.Light->SetIntensity(State.Intensity);
			continue;
		}

		bool bZonePowered = true;
		if (const bool* Found = ZonePowerOverrides.Find(State.ZoneName))
		{
			bZonePowered = *Found;
		}

		const bool bPowered = bHousePowerEnabled && bZonePowered;

		if (bPowered)
		{
			State.Light->SetVisibility(State.bVisible);
			State.Light->SetIntensity(State.Intensity);
		}
		else
		{
			State.Light->SetVisibility(false);
		}
	}
}