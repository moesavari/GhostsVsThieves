#include "Systems/GvTPowerBoxActor.h"
#include "Components/PointLightComponent.h"
#include "Net/UnrealNetwork.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/BoxComponent.h"
#include "Gameplay/Interaction/GvTInteractable.h"
#include "Systems/Noise/GvTNoiseEmitterComponent.h"
#include "Systems/Light/GvTLightFlickerComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Gameplay/Characters/Thieves/GvTThiefCharacter.h"
#include "GvTPlayerState.h"
#include "Systems/Director/GvTDirectorSubsystem.h"
#include "Gameplay/Scare/GvTScareTags.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"
#include "Engine/GameInstance.h"

static void GvTApplyPowerStatePanicToPlayers(
	UWorld* World,
	AActor* SourceActor,
	EGvTHousePowerState NewPowerState,
	EGvTPowerChangeCause Cause)
{
	if (!World)
	{
		return;
	}

	// Only haunted / scripted house behavior should apply panic.
	if (Cause != EGvTPowerChangeCause::GhostEvent &&
		Cause != EGvTPowerChangeCause::SystemScript)
	{
		return;
	}

	TArray<AActor*> Thieves;
	UGameplayStatics::GetAllActorsOfClass(World, AGvTThiefCharacter::StaticClass(), Thieves);

	for (AActor* Actor : Thieves)
	{
		APawn* Pawn = Cast<APawn>(Actor);
		if (!Pawn)
		{
			continue;
		}

		AGvTPlayerState* PS = Pawn->GetPlayerState<AGvTPlayerState>();
		if (!PS)
		{
			continue;
		}

		FGvTPanicEvent PanicEvent;
		PanicEvent.SourceActor = SourceActor;
		PanicEvent.InstigatorActor = SourceActor;
		PanicEvent.WorldLocation = SourceActor ? SourceActor->GetActorLocation() : FVector::ZeroVector;
		PanicEvent.bRequiresProximity = false;
		PanicEvent.bRequiresSuccessfulExecution = true;
		PanicEvent.bExecutionSucceeded = true;

		switch (NewPowerState)
		{
		case EGvTHousePowerState::Off:
		case EGvTHousePowerState::Blown:
			PanicEvent.Source = EGvTPanicSource::PowerOutage;
			PanicEvent.PanicDelta01 = 0.10f;
			PanicEvent.HauntPressureDelta01 = 0.12f;
			PanicEvent.CooldownSeconds = 15.0f;
			break;

		case EGvTHousePowerState::On:
			PanicEvent.Source = EGvTPanicSource::PowerRestore;
			PanicEvent.PanicDelta01 = -0.02f;
			PanicEvent.HauntPressureDelta01 = -0.04f;
			PanicEvent.CooldownSeconds = 2.0f;
			break;

		default:
			continue;
		}

		PS->ApplyPanicEventAuthority(PanicEvent);
	}
}
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

	if (HasAuthority() && GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(TimerHandle_RandomFailure);
		if (PowerState == EGvTHousePowerState::On)
		{
			PowerTurnedOnTime = GetWorld()->GetTimeSeconds();
			ScheduleRandomFailureCheck();
		}
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

void AGvTPowerBoxActor::ScheduleRandomFailureCheck()
{
	if (!HasAuthority() || !GetWorld() || PowerState != EGvTHousePowerState::On)
	{
		return;
	}

	const float MinInterval = FMath::Max(0.1f, FailureCheckIntervalMin);
	const float MaxInterval = FMath::Max(MinInterval, FailureCheckIntervalMax);
	const float PoweredFor = GetWorld()->GetTimeSeconds() - PowerTurnedOnTime;
	const float ProtectionRemaining = FMath::Max(0.f, FailureProtectionSeconds - PoweredFor);
	const float Delay = ProtectionRemaining + FMath::FRandRange(MinInterval, MaxInterval);
	GetWorld()->GetTimerManager().SetTimer(TimerHandle_RandomFailure, this, &AGvTPowerBoxActor::RunRandomFailureCheck, Delay, false);
}

float AGvTPowerBoxActor::GetRandomFailureChance() const
{
	float Activity = 0.f;
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UGvTDirectorSubsystem* Director = GI->GetSubsystem<UGvTDirectorSubsystem>())
		{
			Activity = Director->GetHouseActivity01();
		}
	}

	if (Activity >= 0.90f) return FailureChanceHostile;
	if (Activity >= 0.70f) return FMath::Lerp(FailureChanceAwake, FailureChanceHostile, (Activity - 0.70f) / 0.20f);
	if (Activity >= 0.50f) return FailureChanceAwake;
	if (Activity >= 0.25f) return FailureChanceStirring;
	return FailureChanceDormant;
}

void AGvTPowerBoxActor::RunRandomFailureCheck()
{
	if (!HasAuthority() || PowerState != EGvTHousePowerState::On)
	{
		return;
	}

	const float FailureChance = FMath::Clamp(GetRandomFailureChance(), 0.f, 1.f);
	const float Roll = FMath::FRand();
	UE_LOG(LogTemp, Log, TEXT("[PowerFailure] Breaker=%s Chance=%.3f Roll=%.3f"), *GetNameSafe(this), FailureChance, Roll);

	if (Roll <= FailureChance)
	{
		BlowPowerBox();
		return;
	}

	ScheduleRandomFailureCheck();
}

void AGvTPowerBoxActor::Multicast_PlayPowerStateAudio_Implementation(EGvTHousePowerState NewState)
{
	USoundBase* Sound = NewState == EGvTHousePowerState::On ? PowerOnSound : NewState == EGvTHousePowerState::Blown ? PowerBlownSound : PowerOffSound;
	if (Sound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, Sound, GetActorLocation());
	}
}

void AGvTPowerBoxActor::Server_SetPowerState_Implementation(EGvTHousePowerState NewState)
{
	if (PowerState == NewState)
	{
		return;
	}

	PowerState = NewState;
	ApplyPowerState();
	Multicast_PlayPowerStateAudio(PowerState);
}

void AGvTPowerBoxActor::TogglePower()
{
	if (!HasAuthority())
	{
		Server_TogglePower();
		return;
	}

	if (PowerState == EGvTHousePowerState::Blown)
	{
		return;
	}

	PowerState = (PowerState == EGvTHousePowerState::On)
		? EGvTHousePowerState::Off
		: EGvTHousePowerState::On;

	ApplyPowerState();
	Multicast_PlayPowerStateAudio(PowerState);

	UE_LOG(LogTemp, Log,
		TEXT("[Power] Player toggled power -> %d"),
		(int32)PowerState);
}

void AGvTPowerBoxActor::Server_TogglePower_Implementation()
{
	TogglePower();
}

void AGvTPowerBoxActor::BlowPowerBox()
{
	if (!HasAuthority())
	{
		Server_SetPowerState(EGvTHousePowerState::Blown);
		return;
	}

	if (PowerState == EGvTHousePowerState::Blown)
	{
		return;
	}

	PowerState = EGvTHousePowerState::Blown;
	ApplyPowerState();
	Multicast_PlayPowerStateAudio(PowerState);
	GvTApplyPowerStatePanicToPlayers(GetWorld(), this, PowerState, EGvTPowerChangeCause::GhostEvent);
}

void AGvTPowerBoxActor::Server_ForcePowerStateFromGhost_Implementation(EGvTHousePowerState NewState)
{
	ForcePowerStateFromGhost(NewState);
}

void AGvTPowerBoxActor::ForcePowerStateFromGhost(EGvTHousePowerState NewState)
{
	if (!HasAuthority())
	{
		Server_ForcePowerStateFromGhost(NewState);
		return;
	}

	if (PowerState == NewState)
	{
		return;
	}

	PowerState = NewState;
	ApplyPowerState();
	Multicast_PlayPowerStateAudio(PowerState);

	GvTApplyPowerStatePanicToPlayers(GetWorld(), this, PowerState, EGvTPowerChangeCause::GhostEvent);

	UE_LOG(LogTemp, Log,
		TEXT("[Power] Ghost forced power state -> %d"),
		(int32)PowerState);
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

	const EGvTHousePowerState OldState = PowerState;

	TogglePower();

	const bool bTurnedPowerOn =
		OldState == EGvTHousePowerState::Off &&
		PowerState == EGvTHousePowerState::On;

	const bool bCanScare =
		!bBreakerScareOnlyWhenTurningPowerOn || bTurnedPowerOn;

	if (bCanScare && InstigatorPawn && FMath::FRand() <= BreakerGhostReactionChance)
	{
		if (UGameInstance* GI = GetGameInstance())
		{
			if (UGvTDirectorSubsystem* Director = GI->GetSubsystem<UGvTDirectorSubsystem>())
			{
				Director->DispatchScareEventSimple(
					GvTScareTags::RearAudioSting(),
					InstigatorPawn,
					this);
			}
		}
	}
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
	return Verb == EGvTInteractionVerb::Interact && PowerState != EGvTHousePowerState::Blown;
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