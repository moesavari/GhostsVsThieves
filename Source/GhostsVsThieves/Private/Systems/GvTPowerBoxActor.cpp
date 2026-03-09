#include "Systems/GvTPowerBoxActor.h"
#include "Components/PointLightComponent.h"
#include "Net/UnrealNetwork.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/BoxComponent.h"
#include "Gameplay/Interaction/GvTInteractable.h"
#include "Systems/Noise/GvTNoiseEmitterComponent.h"
#include "Systems/Light/GvTLightFlickerComponent.h"

AGvTPowerBoxActor::AGvTPowerBoxActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	PowerBoxMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PowerBoxMesh"));
	PowerBoxMesh->SetupAttachment(Root);
	PowerBoxMesh->SetIsReplicated(false);

	InteractionBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractionBounds"));
	InteractionBounds->SetupAttachment(Root);
	InteractionBounds->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionBounds->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractionBounds->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	InteractionBounds->SetBoxExtent(FVector(24.f, 16.f, 40.f));

	PowerInteractNoiseTag = FGameplayTag::RequestGameplayTag(TEXT("Noise.Interact"));

	OnIndicatorLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("OnIndicatorLight"));
	InitializeIndicatorLights(OnIndicatorLight, FColor::Green);

	OffIndicatorLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("OffIndicatorLight"));
	InitializeIndicatorLights(OffIndicatorLight, FColor::Yellow);

	BlownIndicatorLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("BlownIndicatorLight"));
	InitializeIndicatorLights(BlownIndicatorLight, FColor::Red);
}

void AGvTPowerBoxActor::BeginPlay()
{
	Super::BeginPlay();
	ApplyPowerState();
}

void AGvTPowerBoxActor::InitializeIndicatorLights(TObjectPtr<UPointLightComponent> IndicatorLight, FColor Color)
{
	if (!IndicatorLight || !PowerBoxMesh)
	{
		return;
	}

	IndicatorLight->SetupAttachment(PowerBoxMesh);
	IndicatorLight->SetIntensity(5000.f);
	IndicatorLight->SetAttenuationRadius(120.f);
	IndicatorLight->SetLightColor(Color);
	IndicatorLight->SetCastShadows(false);
	IndicatorLight->SetVisibility(false);
}

void AGvTPowerBoxActor::OnRep_PowerState()
{
	ApplyPowerState();
}

void AGvTPowerBoxActor::ApplyPowerState()
{
	if (OnIndicatorLight)
	{
		OnIndicatorLight->SetVisibility(PowerState == EGvTHousePowerState::On);
	}

	if (OffIndicatorLight)
	{
		OffIndicatorLight->SetVisibility(PowerState == EGvTHousePowerState::Off);
	}

	if (BlownIndicatorLight)
	{
		BlownIndicatorLight->SetVisibility(PowerState == EGvTHousePowerState::Blown);
	}

	if (!IsValid(HouseActor))
	{
		UE_LOG(LogTemp, Warning, TEXT("PowerBox %s has no HouseActor assigned."), *GetName());
		return;
	}

	UGvTLightFlickerComponent* Flicker = HouseActor->FindComponentByClass<UGvTLightFlickerComponent>();
	if (!IsValid(Flicker))
	{
		UE_LOG(LogTemp, Warning, TEXT("PowerBox %s could not find UGvTLightFlickerComponent on HouseActor %s."),
			*GetName(),
			*GetNameSafe(HouseActor));
		return;
	}

	switch (PowerState)
	{
	case EGvTHousePowerState::On:
		Flicker->SetHousePowerEnabled(true);
		break;

	case EGvTHousePowerState::Off:
		Flicker->SetHousePowerEnabled(false);
		break;

	case EGvTHousePowerState::Blown:
		Flicker->SetHousePowerEnabled(false);
		break;

	default:
		break;
	}

	UE_LOG(LogTemp, Warning, TEXT("PowerBox %s applied power state: %d"), *GetName(), static_cast<int32>(PowerState));
}

void AGvTPowerBoxActor::Server_SetPowerState_Implementation(EGvTHousePowerState NewState)
{
	PowerState = NewState;
	ApplyPowerState();
}

void AGvTPowerBoxActor::TogglePower()
{
	if (!HasAuthority())
	{
		Server_SetPowerState(PowerState == EGvTHousePowerState::On
			? EGvTHousePowerState::Off
			: EGvTHousePowerState::On);
		return;
	}

	if (PowerState == EGvTHousePowerState::Blown)
	{
		UE_LOG(LogTemp, Warning, TEXT("PowerBox %s is blown and cannot be toggled normally."), *GetName());
		return;
	}

	PowerState = (PowerState == EGvTHousePowerState::On)
		? EGvTHousePowerState::Off
		: EGvTHousePowerState::On;

	ApplyPowerState();
}

void AGvTPowerBoxActor::BlowPowerBox()
{
	if (!HasAuthority())
	{
		Server_SetPowerState(EGvTHousePowerState::Blown);
		return;
	}

	PowerState = EGvTHousePowerState::Blown;
	ApplyPowerState();
}

void AGvTPowerBoxActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AGvTPowerBoxActor, PowerState);
}

void AGvTPowerBoxActor::HandlePlayerInteract(APawn* InstigatorPawn)
{
	if (PowerState == EGvTHousePowerState::Blown)
	{
		UE_LOG(LogTemp, Warning, TEXT("PowerBox %s is blown and cannot be toggled normally."), *GetName());
		return;
	}

	TogglePower();
}

void AGvTPowerBoxActor::GetInteractionSpec_Implementation(APawn* InstigatorPawn, EGvTInteractionVerb Verb, FGvTInteractionSpec& OutSpec) const
{
	OutSpec = FGvTInteractionSpec{};
	OutSpec.CastTime = 0.f;
	OutSpec.bLockMovement = false;
	OutSpec.bLockLook = false;
	OutSpec.bCancelable = false;
	OutSpec.bEmitNoiseOnCancel = false;
	OutSpec.CancelNoiseRadius = 0.f;
	OutSpec.CancelNoiseLoudness = 0.f;
	OutSpec.InteractionTag = PowerInteractNoiseTag;
}

bool AGvTPowerBoxActor::CanInteract_Implementation(APawn* InstigatorPawn, EGvTInteractionVerb Verb) const
{
	return Verb == EGvTInteractionVerb::Interact;
}

void AGvTPowerBoxActor::BeginInteract_Implementation(APawn* InstigatorPawn, EGvTInteractionVerb Verb)
{
	// No-op for instant interaction.
}

void AGvTPowerBoxActor::CancelInteract_Implementation(APawn* InstigatorPawn, EGvTInteractionVerb Verb, EGvTInteractionCancelReason Reason)
{
	// No-op for instant interaction.
}

void AGvTPowerBoxActor::CompleteInteract_Implementation(APawn* InstigatorPawn, EGvTInteractionVerb Verb)
{
	if (!HasAuthority())
	{
		return;
	}

	if (Verb != EGvTInteractionVerb::Interact)
	{
		return;
	}

	HandlePlayerInteract(InstigatorPawn);
}