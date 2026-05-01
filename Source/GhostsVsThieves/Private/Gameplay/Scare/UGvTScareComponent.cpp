#include "Gameplay/Scare/UGvTScareComponent.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Actor.h"
#include "TimerManager.h"
#include "Net/UnrealNetwork.h"
#include "Systems/Director/GvTDirectorSubsystem.h"
#include "Gameplay/Ghosts/GvTGhostCharacterBase.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Pawn.h"
#include "Gameplay/Scare/GvTScareTags.h"
#include "Gameplay/Ghosts/Mirror/GvTMirrorActor.h"
#include "Systems/Light/GvTLightFlickerSubsystem.h"
#include "EngineUtils.h"
#include "Gameplay/Characters/Thieves/GvTThiefCharacter.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"

UGvTScareComponent::UGvTScareComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UGvTScareComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UGvTScareComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearLifecycleTimers();
	Super::EndPlay(EndPlayReason);
}

void UGvTScareComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UGvTScareComponent, Panic);
	DOREPLIFETIME(UGvTScareComponent, ScareState);
}

bool UGvTScareComponent::IsServer() const
{
	return GetOwner() && GetOwner()->HasAuthority();
}

float UGvTScareComponent::GetNowServerSeconds() const
{
	return GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
}

//
// =======================
// Panic
// =======================
//

void UGvTScareComponent::AddPanic(float Amount)
{
	Panic = FMath::Clamp(Panic + Amount, 0.f, PanicMax);
	UpdatePanicBand();
}

void UGvTScareComponent::ReducePanic(float Amount)
{
	Panic = FMath::Clamp(Panic - Amount, 0.f, PanicMax);
	UpdatePanicBand();
}

float UGvTScareComponent::GetPanicNormalized() const
{
	return PanicMax > 0.f ? Panic / PanicMax : 0.f;
}

void UGvTScareComponent::UpdatePanicBand()
{
	const float P = GetPanicNormalized();

	if (P < 0.25f) CachedPanicBand = EGvTPanicBand::Calm;
	else if (P < 0.5f) CachedPanicBand = EGvTPanicBand::Unsettled;
	else if (P < 0.75f) CachedPanicBand = EGvTPanicBand::Shaken;
	else if (P < 0.9f) CachedPanicBand = EGvTPanicBand::Terrified;
	else CachedPanicBand = EGvTPanicBand::Critical;
}

EGvTPanicBand UGvTScareComponent::GetPanicBand() const
{
	return CachedPanicBand;
}

//
// =======================
// Lifecycle
// =======================
//

bool UGvTScareComponent::IsScareActive() const
{
	return LifecycleState == EGvTScareLifecycleState::Active;
}

bool UGvTScareComponent::IsScareRecovering() const
{
	return LifecycleState == EGvTScareLifecycleState::Recovering;
}

bool UGvTScareComponent::IsScareBusy() const
{
	return LifecycleState != EGvTScareLifecycleState::Idle;
}

bool UGvTScareComponent::CanStartNewScare() const
{
	return LifecycleState == EGvTScareLifecycleState::Idle;
}

void UGvTScareComponent::BeginLocalScareLifecycle(float ActiveDuration, float RecoveryDuration)
{
	if (!CanStartNewScare() || !GetWorld())
	{
		return;
	}

	ActiveRecoveryDuration = RecoveryDuration;
	LifecycleState = EGvTScareLifecycleState::Active;

	GetWorld()->GetTimerManager().SetTimer(
		TimerHandle_ScareActiveFinish,
		this,
		&UGvTScareComponent::HandleLifecycleActiveFinished,
		ActiveDuration,
		false);
}

void UGvTScareComponent::HandleLifecycleActiveFinished()
{
	EnterRecoveryState();
}

void UGvTScareComponent::EnterRecoveryState()
{
	if (!GetWorld())
	{
		LifecycleState = EGvTScareLifecycleState::Idle;
		return;
	}

	LifecycleState = EGvTScareLifecycleState::Recovering;

	GetWorld()->GetTimerManager().SetTimer(
		TimerHandle_ScareRecoveryFinish,
		this,
		&UGvTScareComponent::HandleLifecycleRecoveryFinished,
		ActiveRecoveryDuration,
		false);
}

void UGvTScareComponent::HandleLifecycleRecoveryFinished()
{
	EndScareLifecycle();
}

void UGvTScareComponent::EndScareLifecycle()
{
	LifecycleState = EGvTScareLifecycleState::Idle;
	ClearLifecycleTimers();
}

void UGvTScareComponent::ClearLifecycleTimers()
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(TimerHandle_ScareActiveFinish);
		GetWorld()->GetTimerManager().ClearTimer(TimerHandle_ScareRecoveryFinish);	}
}

//
// =======================
// Requests (Wrappers)
// =======================
//

void UGvTScareComponent::RequestMirrorScare(float Intensity01, float LifeSeconds)
{
	FGvTScareEvent Event;
	Event.ScareTag = GvTScareTags::Mirror();
	Event.Intensity01 = Intensity01;
	Event.Duration = LifeSeconds;

	RequestMirrorScareFromEvent(Event);
}

void UGvTScareComponent::RequestGhostChaseScare(AActor* Victim)
{
	// Stubbed  handled by Director
}

void UGvTScareComponent::RequestGhostChaseFromEvent(AActor* Victim)
{
	if (!Victim)
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		if (UGameInstance* GI = World->GetGameInstance())
		{
			if (UGvTDirectorSubsystem* Director = GI->GetSubsystem<UGvTDirectorSubsystem>())
			{
				if (AGvTGhostCharacterBase* Ghost = Director->GetPrimaryGhost())
				{
					if (APawn* VictimPawn = Cast<APawn>(Victim))
					{
						Ghost->BeginGhostChase(VictimPawn);
					}
				}
			}
		}
	}
}

void UGvTScareComponent::RequestGhostScareFromEvent(const FGvTScareEvent& Event)
{
	if (IsServer())
	{
		UE_LOG(LogTemp, Warning, TEXT("[ScareRoute] Server -> Client GhostScare owner=%s target=%s"),
			*GetNameSafe(GetOwner()),
			*GetNameSafe(Event.TargetActor));

		Client_StartGhostScare(Event);
		return;
	}

	PlayLocalGhostScare(Event);
}

void UGvTScareComponent::PlayLocalGhostScare(const FGvTScareEvent& Event)
{
	TryBeginLocalHauntGhostScare(Event);
}

void UGvTScareComponent::Client_StartGhostScare_Implementation(const FGvTScareEvent& Event)
{
	UE_LOG(LogTemp, Warning, TEXT("[ScareRPC] Client_StartGhostScare owner=%s netMode=%d"),
		*GetNameSafe(GetOwner()),
		GetOwner() ? (int32)GetOwner()->GetNetMode() : -1);

	PlayLocalGhostScare(Event);
}

void UGvTScareComponent::RequestLightChaseFromEvent(const FGvTScareEvent& Event)
{
	if (IsServer())
	{
		UE_LOG(LogTemp, Warning, TEXT("[ScareRoute] Server -> Client LightChase owner=%s target=%s"),
			*GetNameSafe(GetOwner()),
			*GetNameSafe(Event.TargetActor));

		Client_StartLightChase(Event);
		return;
	}

	PlayLocalLightChase(Event);
}

void UGvTScareComponent::RequestRearAudioStingFromEvent(const FGvTScareEvent& Event)
{
	if (IsServer())
	{
		UE_LOG(LogTemp, Warning, TEXT("[ScareRoute] Server -> Client RearAudioSting owner=%s target=%s"),
			*GetNameSafe(GetOwner()),
			*GetNameSafe(Event.TargetActor));

		Client_StartRearAudioSting(Event);
		return;
	}

	PlayLocalRearAudioSting(Event);
}

void UGvTScareComponent::RequestGhostScreamFromEvent(const FGvTScareEvent& Event)
{
	if (IsServer())
	{
		UE_LOG(LogTemp, Warning, TEXT("[ScareRoute] Server -> Client GhostScream owner=%s target=%s"),
			*GetNameSafe(GetOwner()),
			*GetNameSafe(Event.TargetActor));

		Client_StartGhostScream(Event);
		return;
	}

	PlayLocalGhostScreamShared(Event);
}

//
// =======================
// Local Playback
// =======================
//

bool UGvTScareComponent::TryBeginLocalHauntGhostScare(const FGvTScareEvent& Event)
{
	UE_LOG(LogTemp, Warning, TEXT("[ClientPayload] HauntGhostScare START owner=%s target=%s tag=%s busy=%d"),
		*GetNameSafe(GetOwner()),
		*GetNameSafe(Event.TargetActor),
		*Event.ScareTag.ToString(),
		IsScareBusy() ? 1 : 0);

	if (IsScareBusy())
	{
		return false;
	}

	const float ActiveSeconds = FMath::Max(0.10f, Event.Duration);
	BeginLocalScareLifecycle(ActiveSeconds, 0.0f);

	return true;
}

void UGvTScareComponent::PlayLocalLightChase(const FGvTScareEvent& Event)
{
	UE_LOG(LogTemp, Warning, TEXT("[ClientPayload] LightChase START owner=%s target=%s steps=%d interval=%.2f"),
		*GetNameSafe(GetOwner()),
		*GetNameSafe(Event.TargetActor),
		Event.LightChaseStepCount,
		Event.LightChaseStepInterval);

	BeginLocalScareLifecycle(FMath::Max(0.10f, Event.Duration), LightChaseRecoveryDuration);
	BeginLocalLightChaseSequence(Event);
	BP_PlayScare(Event);
}

void UGvTScareComponent::PlayLocalRearAudioSting(const FGvTScareEvent& Event)
{
	UE_LOG(LogTemp, Warning, TEXT("[ClientPayload] RearAudioSting START owner=%s panic=%.1f"),
		*GetNameSafe(GetOwner()),
		Event.PanicAmount);

	BeginLocalScareLifecycle(0.3f, RearAudioRecoveryDuration);

	const FVector SoundLocation = BuildRearAudioWorldLocation(Event);
	if (RearAudioStingSfx)
	{
		UGameplayStatics::PlaySoundAtLocation(this, RearAudioStingSfx, SoundLocation);
	}
}


void UGvTScareComponent::PlayLocalGhostScreamShared(const FGvTScareEvent& Event)
{
	UE_LOG(LogTemp, Warning, TEXT("[ClientPayload] GhostScream START owner=%s panic=%.1f loc=%s"),
		*GetNameSafe(GetOwner()),
		Event.PanicAmount,
		*Event.WorldHint.ToCompactString());

	BeginLocalScareLifecycle(0.5f, GhostScreamRecoveryDuration);

	const FVector SoundLocation = !Event.WorldHint.IsNearlyZero() ? Event.WorldHint : (GetOwner() ? GetOwner()->GetActorLocation() : FVector::ZeroVector);
	if (GhostScreamSfx)
	{
		UGameplayStatics::PlaySoundAtLocation(this, GhostScreamSfx, SoundLocation);
	}
}

//
// =======================
// RPCs
// =======================
//

void UGvTScareComponent::Client_PlayScare_Implementation(const FGvTScareEvent& Event)
{
	BP_PlayScare(Event);
}

void UGvTScareComponent::Client_PlayMirrorScare_Implementation(float Intensity01, float LifeSeconds)
{
	// Hook for mirror playback if needed
}

void UGvTScareComponent::Client_PlayLightFlicker_Implementation(const FGvTLightFlickerEvent& Event)
{
	PlayLocalLightFlicker(Event);
}

void UGvTScareComponent::Client_StartLightChase_Implementation(const FGvTScareEvent& Event)
{
	UE_LOG(LogTemp, Warning, TEXT("[ScareRPC] Client_StartLightChase owner=%s netMode=%d"),
		*GetNameSafe(GetOwner()),
		GetOwner() ? (int32)GetOwner()->GetNetMode() : -1);

	PlayLocalLightChase(Event);
}

void UGvTScareComponent::Client_StartRearAudioSting_Implementation(const FGvTScareEvent& Event)
{
	UE_LOG(LogTemp, Warning, TEXT("[ScareRPC] Client_StartRearAudioSting owner=%s netMode=%d"),
		*GetNameSafe(GetOwner()),
		GetOwner() ? (int32)GetOwner()->GetNetMode() : -1);

	PlayLocalRearAudioSting(Event);
}

void UGvTScareComponent::Client_StartGhostScream_Implementation(const FGvTScareEvent& Event)
{
	UE_LOG(LogTemp, Warning, TEXT("[ScareRPC] Client_StartGhostScream owner=%s netMode=%d"),
		*GetNameSafe(GetOwner()),
		GetOwner() ? (int32)GetOwner()->GetNetMode() : -1);

	PlayLocalGhostScreamShared(Event);
}

void UGvTScareComponent::OnRep_ScareState() {}
void UGvTScareComponent::OnRep_Panic() {}

void UGvTScareComponent::Server_ReportLightChaseResult_Implementation(bool bSucceeded, float PanicAmount)
{
	if (bSucceeded && PanicAmount > 0.f)
	{
		AddPanic(PanicAmount);
	}
}

void UGvTScareComponent::Test_MirrorScare(float Intensity01, float LifeSeconds)
{
	RequestMirrorScare(Intensity01, LifeSeconds);
}

void UGvTScareComponent::Debug_RequestGroupHouseLightFlicker(float Intensity01, float Duration)
{
	// Group flicker is intentionally different from local flicker:
	// - Local flicker affects only this machine.
	// - Group flicker requests the server, then sends a client RPC to every thief.
	if (!IsServer())
	{
		UE_LOG(LogTemp, Warning, TEXT("[ScareRoute] GroupLightFlicker request from client owner=%s intensity=%.2f duration=%.2f"),
			*GetNameSafe(GetOwner()), Intensity01, Duration);

		Server_RequestGroupHouseLightFlicker(Intensity01, Duration);
		return;
	}

	const FGvTLightFlickerEvent Event = MakeLightFlickerEvent(Intensity01, Duration, true);

	int32 SentCount = 0;
	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ScareRoute] GroupLightFlicker failed: no world owner=%s"), *GetNameSafe(GetOwner()));
		return;
	}

	for (TActorIterator<AGvTThiefCharacter> It(World); It; ++It)
	{
		AGvTThiefCharacter* Thief = *It;
		if (!IsValid(Thief))
		{
			continue;
		}

		UGvTScareComponent* TargetScareComp = Thief->FindComponentByClass<UGvTScareComponent>();
		if (!TargetScareComp)
		{
			UE_LOG(LogTemp, Warning, TEXT("[ScareRoute] GroupLightFlicker skipped %s: no scare component"), *GetNameSafe(Thief));
			continue;
		}

		TargetScareComp->Client_PlayLightFlicker(Event);
		++SentCount;
	}

	UE_LOG(LogTemp, Warning, TEXT("[ScareRoute] GroupLightFlicker broadcast sent=%d requestedBy=%s intensity=%.2f duration=%.2f"),
		SentCount, *GetNameSafe(GetOwner()), Event.Intensity01, Event.Duration);
}

void UGvTScareComponent::Server_RequestGroupHouseLightFlicker_Implementation(float Intensity01, float Duration)
{
	Debug_RequestGroupHouseLightFlicker(Intensity01, Duration);
}

void UGvTScareComponent::Debug_RequestLocalHouseLightFlicker(float Intensity01, float Duration)
{
	// Local means this player/client only. It can still flicker the whole house locally,
	// but it must not send RPCs to other players.
	const FGvTLightFlickerEvent Event = MakeLightFlickerEvent(Intensity01, Duration, true);

	UE_LOG(LogTemp, Warning, TEXT("[ScareRoute] LocalLightFlicker local-only owner=%s intensity=%.2f duration=%.2f"),
		*GetNameSafe(GetOwner()), Event.Intensity01, Event.Duration);

	PlayLocalLightFlicker(Event);
}

void UGvTScareComponent::Debug_RequestLightChase(float Intensity01)
{
	FGvTScareEvent Event;
	Event.Intensity01 = Intensity01;
	RequestLightChaseFromEvent(Event);
}

FGvTLightFlickerEvent UGvTScareComponent::MakeLightFlickerEvent(float Intensity01, float Duration, bool bWholeHouse) const
{
	FGvTLightFlickerEvent Event;
	Event.Intensity01 = Intensity01;
	Event.Duration = Duration;
	Event.bWholeHouse = bWholeHouse;
	return Event;
}

void UGvTScareComponent::PlayLocalLightFlicker(const FGvTLightFlickerEvent& Event) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ClientPayload] LightFlicker failed: no world owner=%s"), *GetNameSafe(GetOwner()));
		return;
	}

	UGvTLightFlickerSubsystem* FlickerSubsystem = World->GetSubsystem<UGvTLightFlickerSubsystem>();
	if (!FlickerSubsystem)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ClientPayload] LightFlicker failed: no subsystem owner=%s"), *GetNameSafe(GetOwner()));
		return;
	}

	const int32 LightsInRadius = Event.bWholeHouse ? -1 : FlickerSubsystem->CountLightsInRadius(Event.WorldCenter, Event.Radius);
	UE_LOG(LogTemp, Warning, TEXT("[ClientPayload] LightFlicker START owner=%s wholeHouse=%d center=%s radius=%.1f lights=%d duration=%.2f intensity=%.2f"),
		*GetNameSafe(GetOwner()),
		Event.bWholeHouse ? 1 : 0,
		*Event.WorldCenter.ToCompactString(),
		Event.Radius,
		LightsInRadius,
		Event.Duration,
		Event.Intensity01);

	FlickerSubsystem->PlayFlickerEvent(Event);
}

void UGvTScareComponent::BeginLocalLightChaseSequence(const FGvTScareEvent& Event)
{
	if (!GetWorld())
	{
		return;
	}

	EndLocalLightChaseSequence();

	ActiveLightChaseEvent = Event;
	ActiveLightChaseStepLocations = BuildLightChaseStepLocations(Event);
	ActiveLightChaseStepIndex = 0;
	bActiveLightChaseAffectedAnyLights = false;
	bLightChaseSequenceActive = ActiveLightChaseStepLocations.Num() > 0;

	UE_LOG(LogTemp, Warning, TEXT("[ClientPayload] LightChase sequence built owner=%s steps=%d"),
		*GetNameSafe(GetOwner()),
		ActiveLightChaseStepLocations.Num());

	if (!bLightChaseSequenceActive)
	{
		Server_ReportLightChaseResult(false, 0.f);
		return;
	}

	AdvanceLocalLightChaseSequence();
}

void UGvTScareComponent::AdvanceLocalLightChaseSequence()
{
	if (!bLightChaseSequenceActive || !GetWorld())
	{
		return;
	}

	if (!ActiveLightChaseStepLocations.IsValidIndex(ActiveLightChaseStepIndex))
	{
		const bool bSucceeded = bActiveLightChaseAffectedAnyLights;
		UE_LOG(LogTemp, Warning, TEXT("[ClientPayload] LightChase END owner=%s success=%d"),
			*GetNameSafe(GetOwner()),
			bSucceeded ? 1 : 0);

		EndLocalLightChaseSequence();
		Server_ReportLightChaseResult(bSucceeded, bSucceeded ? ActiveLightChaseEvent.PanicAmount : 0.f);
		return;
	}

	const bool bIsFinalStep = ActiveLightChaseStepIndex == ActiveLightChaseStepLocations.Num() - 1;
	const FVector StepLocation = ActiveLightChaseStepLocations[ActiveLightChaseStepIndex];
	PlayLightChaseStepEffects(StepLocation, bIsFinalStep);

	++ActiveLightChaseStepIndex;

	GetWorld()->GetTimerManager().SetTimer(
		TimerHandle_LightChaseStep,
		this,
		&UGvTScareComponent::AdvanceLocalLightChaseSequence,
		FMath::Max(0.01f, ActiveLightChaseEvent.LightChaseStepInterval),
		false);
}

void UGvTScareComponent::EndLocalLightChaseSequence()
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(TimerHandle_LightChaseStep);
	}

	bLightChaseSequenceActive = false;
	ActiveLightChaseStepLocations.Reset();
	ActiveLightChaseStepIndex = 0;
}

TArray<FVector> UGvTScareComponent::BuildLightChaseStepLocations(const FGvTScareEvent& Event) const
{
	TArray<FVector> StepLocations;

	const AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return StepLocations;
	}

	const int32 StepCount = FMath::Max(2, Event.LightChaseStepCount);
	const FVector OwnerLocation = OwnerActor->GetActorLocation();
	const FVector Forward = OwnerActor->GetActorForwardVector().GetSafeNormal();

	StepLocations.Reserve(StepCount);
	for (int32 Index = 0; Index < StepCount; ++Index)
	{
		const float Alpha = StepCount > 1 ? float(Index) / float(StepCount - 1) : 1.f;
		const float Distance = FMath::Lerp(Event.LightChaseStartDistance, Event.LightChaseEndDistance, Alpha);
		StepLocations.Add(ResolveLightChaseStepLocation(OwnerLocation + Forward * Distance));
	}

	CollapseLightChaseStepLocations(StepLocations);
	return StepLocations;
}

FVector UGvTScareComponent::ResolveLightChaseStepLocation(const FVector& RawStepLocation) const
{
	if (const UWorld* World = GetWorld())
	{
		if (const UGvTLightFlickerSubsystem* FlickerSubsystem = World->GetSubsystem<UGvTLightFlickerSubsystem>())
		{
			FGvTLightClusterQueryResult Result;
			if (FlickerSubsystem->FindNearestLightClusterCenter(RawStepLocation, ActiveLightChaseEvent.LightChaseFlickerRadius, Result))
			{
				return Result.ClusterCenter;
			}
		}
	}

	return RawStepLocation;
}

void UGvTScareComponent::CollapseLightChaseStepLocations(TArray<FVector>& StepLocations) const
{
	TArray<FVector> Collapsed;
	Collapsed.Reserve(StepLocations.Num());

	for (const FVector& Location : StepLocations)
	{
		if (Collapsed.Num() == 0 || FVector::DistSquared(Collapsed.Last(), Location) > FMath::Square(75.f))
		{
			Collapsed.Add(Location);
		}
	}

	StepLocations = MoveTemp(Collapsed);
}

void UGvTScareComponent::PlayLightChaseStepEffects(const FVector& StepLocation, bool bIsFinalStep)
{
	FGvTLightFlickerEvent FlickerEvent = MakeLightFlickerEvent(
		bIsFinalStep ? 1.0f : 0.85f,
		FMath::Max(0.10f, ActiveLightChaseEvent.LightChaseStepInterval * 1.35f),
		false);

	FlickerEvent.WorldCenter = StepLocation;
	FlickerEvent.Radius = FMath::Max(150.f, ActiveLightChaseEvent.LightChaseFlickerRadius);
	FlickerEvent.Seed = ActiveLightChaseEvent.LocalSeed + ActiveLightChaseStepIndex * 37 + 1337;

	if (const UWorld* World = GetWorld())
	{
		if (const UGvTLightFlickerSubsystem* FlickerSubsystem = World->GetSubsystem<UGvTLightFlickerSubsystem>())
		{
			const int32 LightsInRadius = FlickerSubsystem->CountLightsInRadius(FlickerEvent.WorldCenter, FlickerEvent.Radius);
			bActiveLightChaseAffectedAnyLights |= LightsInRadius > 0;

			UE_LOG(LogTemp, Warning, TEXT("[ClientPayload] LightChase STEP owner=%s index=%d final=%d center=%s lights=%d"),
				*GetNameSafe(GetOwner()),
				ActiveLightChaseStepIndex,
				bIsFinalStep ? 1 : 0,
				*FlickerEvent.WorldCenter.ToCompactString(),
				LightsInRadius);
		}
	}

	PlayLocalLightFlicker(FlickerEvent);
	PlayLocalLightChaseSoundAtLocation(StepLocation, bIsFinalStep, 1.f);
}


void UGvTScareComponent::PlayLocalLightChaseSoundAtLocation(const FVector& WorldLocation, bool bIsFinalStep, float StepAlpha) const
{
	const TObjectPtr<USoundBase> Chosen = bIsFinalStep ? LightChaseFinalSfx : LightChaseStepSfx;
	if (Chosen)
	{
		UGameplayStatics::PlaySoundAtLocation(this, Chosen, WorldLocation);
	}
}

FVector UGvTScareComponent::BuildRearAudioWorldLocation(const FGvTScareEvent& Event) const
{
	return GetOwner() ? GetOwner()->GetActorLocation() : FVector::ZeroVector;
}

void UGvTScareComponent::Server_ApplyDeathRipple(const FVector& DeathLocation, float Radius, float BaseIntensity01)
{
}

void UGvTScareComponent::RequestMirrorScareFromEvent(const FGvTScareEvent& Event)
{
	if (IsServer())
	{
		Client_StartMirrorScare(Event);
		return;
	}

	PlayLocalMirrorScare(Event);
}

void UGvTScareComponent::Client_StartMirrorScare_Implementation(const FGvTScareEvent& Event)
{
	PlayLocalMirrorScare(Event);
}

void UGvTScareComponent::PlayLocalMirrorScare(const FGvTScareEvent& Event)
{
	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (OwnerPawn && !OwnerPawn->IsLocallyControlled())
	{
		return;
	}

	const float LifeSeconds = Event.Duration > 0.f ? Event.Duration : 1.5f;
	BeginLocalScareLifecycle(LifeSeconds, MirrorRecoveryDuration);

	AGvTMirrorActor* Mirror = Cast<AGvTMirrorActor>(Event.SourceActor);
	if (!Mirror)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[Scare] Mirror scare failed: SourceActor is not a mirror. Tag=%s Source=%s"),
			*Event.ScareTag.ToString(),
			*GetNameSafe(Event.SourceActor));
		return;
	}

	Mirror->TriggerReflectLocal(Event.Intensity01, LifeSeconds);
}

void UGvTScareComponent::Server_RequestMirrorActorScare_Implementation(AGvTMirrorActor* Mirror, float Intensity01, float LifeSeconds)
{
	if (!Mirror)
	{
		return;
	}

	BeginLocalScareLifecycle(LifeSeconds, MirrorRecoveryDuration);
	Client_PlayMirrorScare(Intensity01, LifeSeconds);
}