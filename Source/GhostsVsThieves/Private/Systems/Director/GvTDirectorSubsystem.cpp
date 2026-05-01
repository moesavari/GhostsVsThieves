#include "Systems/Director/GvTDirectorSubsystem.h"
#include "Gameplay/Characters/Thieves/GvTThiefCharacter.h"
#include "Gameplay/Scare/UGvTScareComponent.h"
#include "Gameplay/Scare/GvTScareTags.h"
#include "GvTPlayerState.h"
#include "GvTGameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "World/Doors/GvTDoorActor.h"
#include "Camera/PlayerCameraManager.h"
#include "EngineUtils.h"
#include "Systems/GvTPowerBoxActor.h"
#include "Gameplay/Ghosts/GvTGhostCharacterBase.h"
#include "Gameplay/Ghosts/GvTGhostModelData.h"
#include "Gameplay/Ghosts/GvTGhostTypeData.h"
#include "Gameplay/Ghosts/GvTGhostRuntimeTypes.h"
#include "Systems/Spawn/GvTGhostSpawnPoint.h"
#include "Gameplay/Ghosts/Mirror/GvTMirrorActor.h"

static const TCHAR* GvTNetModeToString(ENetMode NetMode)
{
	switch (NetMode)
	{
		case NM_Standalone:      return TEXT("Standalone");
		case NM_DedicatedServer: return TEXT("DedicatedServer");
		case NM_ListenServer:    return TEXT("ListenServer");
		case NM_Client:          return TEXT("Client");
		default:                 return TEXT("Unknown");
	}
}

static EGvTPanicSource GvTMapScareTagToPanicSource(const FGameplayTag& ScareTag)
{
	if (ScareTag.MatchesTagExact(GvTScareTags::Mirror()))
	{
		return EGvTPanicSource::MirrorScare;
	}
	if (ScareTag.MatchesTagExact(GvTScareTags::GhostScare()))
	{
		return EGvTPanicSource::GhostScare;
	}
	if (ScareTag.MatchesTagExact(GvTScareTags::GhostChase()))
	{
		return EGvTPanicSource::GhostChaseStart;
	}
	if (ScareTag.MatchesTagExact(GvTScareTags::LightChase()))
	{
		return EGvTPanicSource::LightFlicker;
	}
	if (ScareTag.MatchesTagExact(GvTScareTags::RearAudioSting()))
	{
		return EGvTPanicSource::RearAudioSting;
	}
	if (ScareTag.MatchesTagExact(GvTScareTags::GhostScream()))
	{
		return EGvTPanicSource::GhostScream;
	}
	if (ScareTag.MatchesTagExact(GvTScareTags::DoorSlamBehind()))
	{
		return EGvTPanicSource::DoorSlam;
	}

	return EGvTPanicSource::None;
}

static float GvTGetPressureGain01ForScareTag(const FGameplayTag& ScareTag, bool bTriggerLocalFlicker)
{
	if (ScareTag.MatchesTagExact(GvTScareTags::GhostChase()))
	{
		return 0.35f;
	}
	if (ScareTag.MatchesTagExact(GvTScareTags::GhostScare()))
	{
		return 0.22f;
	}
	if (ScareTag.MatchesTagExact(GvTScareTags::Mirror()))
	{
		return 0.18f;
	}
	if (ScareTag.MatchesTagExact(GvTScareTags::LightChase()))
	{
		return 0.14f;
	}
	if (ScareTag.MatchesTagExact(GvTScareTags::RearAudioSting()))
	{
		return 0.10f;
	}
	if (ScareTag.MatchesTagExact(GvTScareTags::GhostScream()))
	{
		return 0.20f;
	}
	if (ScareTag.MatchesTagExact(GvTScareTags::DoorSlamBehind()))
	{
		return 0.16f;
	}

	return bTriggerLocalFlicker ? 0.08f : 0.04f;
}

void UGvTDirectorSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	UE_LOG(LogTemp, Log, TEXT("GvT Director Subsystem Initialized"));

	if (bEnableAutoHaunts)
	{
		StartDirector();
	}
}

void UGvTDirectorSubsystem::Deinitialize()
{
	StopDirector();
	ClearRoundGhosts();

	Super::Deinitialize();
	UE_LOG(LogTemp, Log, TEXT("GvT Director Subsystem Deinitialized"));
}

void UGvTDirectorSubsystem::StartDirector()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (World->GetNetMode() == NM_Client)
	{
		UE_LOG(LogTemp, Log, TEXT("[Director] Skipping auto-haunt loop on client world."));
		return;
	}

	if (TimerHandle_DirectorTick.IsValid())
	{
		return;
	}

	World->GetTimerManager().SetTimer(
		TimerHandle_DirectorTick,
		this,
		&UGvTDirectorSubsystem::TickDirector,
		DirectorTickInterval,
		true
	);

	UE_LOG(LogTemp, Log, TEXT("[Director] Auto-haunt loop started. Interval=%.2f"), DirectorTickInterval);
}

void UGvTDirectorSubsystem::StopDirector()
{
	if (!GetWorld())
	{
		return;
	}

	if (TimerHandle_DirectorTick.IsValid())
	{
		GetWorld()->GetTimerManager().ClearTimer(TimerHandle_DirectorTick);
		TimerHandle_DirectorTick.Invalidate();

		UE_LOG(LogTemp, Log, TEXT("[Director] Auto-haunt loop stopped."));
	} 
}

void UGvTDirectorSubsystem::TickDirector()
{
	if (!bEnableAutoHaunts)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Server / listen-server authority only.
	if (World->GetNetMode() == NM_Client)
	{
		return;
	}

	UpdateHouseTension(DirectorTickInterval);

	const float Now = World->GetTimeSeconds();
	const float CurrentCooldown = GetCurrentGlobalHauntCooldown();

	if ((Now - LastGlobalHauntTime) < CurrentCooldown)
	{
		if (bLogHouseTension)
		{
			UE_LOG(LogTemp, Log,
				TEXT("[DirectorCooldown] Tension=%.2f Cooldown=%.2f Elapsed=%.2f Remaining=%.2f"),
				HouseTension01,
				CurrentCooldown,
				Now - LastGlobalHauntTime,
				CurrentCooldown - (Now - LastGlobalHauntTime));
		}
		return;
	}

	TryDispatchAutoScare();
}

bool UGvTDirectorSubsystem::TryDispatchAutoScare()
{
	AActor* Target = ChooseBestTarget();
	if (!IsValid(Target))
	{
		return false;
	}

	FGvTScareEvent Event;
	const float Roll = FMath::FRand();

	if (Roll < 0.20f)
	{
		Event = MakeRearAudioStingEvent(Target);
	}
	else if (Roll < 0.38f)
	{
		if (APawn* TargetPawn = Cast<APawn>(Target))
		{
			if (AGvTDoorActor* Door = ChooseBestDoorSlamTarget(TargetPawn))
			{
				Event = MakeDoorSlamBehindEvent(Target, Door);
			}
			else
			{
				Event = MakeLightChaseEvent(Target);
			}
		}
		else
		{
			Event = MakeLightChaseEvent(Target);
		}
	}
	else if (Roll < 0.56f)
	{
		Event = MakeLightChaseEvent(Target);
	}
	else if (Roll < 0.72f)
	{
		Event = MakeMirrorEvent(Target);
	}
	else if (Roll < 0.88f)
	{
		AActor* ScreamTarget = Target;
		if (FMath::FRand() <= GhostScreamHighestPanicBiasChance)
		{
			if (AActor* HighestPanicTarget = ChooseHighestPanicTarget())
			{
				ScreamTarget = HighestPanicTarget;
			}
		}

		Event = MakeGhostScreamEvent(ScreamTarget);
	}
	else
	{
		Event = MakeGhostScareEvent(Target);
	}

	UGvTScareComponent* ScareComp = Target->FindComponentByClass<UGvTScareComponent>();

	const bool bDispatched = DispatchScareEvent(Event);
	if (bDispatched && GetWorld())
	{
		if (APawn* TargetPawn = Cast<APawn>(Target))
		{
			RememberTarget(TargetPawn);
		}

		LastGlobalHauntTime = GetWorld()->GetTimeSeconds();
	}

	ApplyHouseTensionImpulse(GetDispatchTensionImpulse(Event));

	return bDispatched;
}

AActor* UGvTDirectorSubsystem::ChooseBestTarget() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	TArray<AActor*> Thieves;
	UGameplayStatics::GetAllActorsOfClass(World, AGvTThiefCharacter::StaticClass(), Thieves);

	struct FTargetCandidate
	{
		APawn* Pawn = nullptr;
		float Score = 0.f;
	};

	TArray<FTargetCandidate> Candidates;
	float BestScore = -FLT_MAX;

	for (AActor* Actor : Thieves)
	{
		APawn* Pawn = Cast<APawn>(Actor);
		if (!Pawn)
		{
			continue;
		}

		const float Score = ScoreTarget(Pawn);
		if (Score <= 0.f)
		{
			continue;
		}

		FTargetCandidate& C = Candidates.AddDefaulted_GetRef();
		C.Pawn = Pawn;
		C.Score = Score;

		BestScore = FMath::Max(BestScore, Score);
	}

	if (Candidates.Num() == 0)
	{
		return nullptr;
	}

	TArray<FTargetCandidate> TopCandidates;
	float TotalWeight = 0.f;

	for (const FTargetCandidate& C : Candidates)
	{
		if (C.Score >= (BestScore - TopScoreWindow))
		{
			TopCandidates.Add(C);
			TotalWeight += C.Score;
		}
	}

	if (TopCandidates.Num() == 0)
	{
		return Candidates[0].Pawn;
	}

	const float Roll = FMath::FRandRange(0.f, TotalWeight);
	float Running = 0.f;

	for (const FTargetCandidate& C : TopCandidates)
	{
		Running += C.Score;
		if (Roll <= Running)
		{
			return C.Pawn;
		}
	}

	return TopCandidates.Last().Pawn;
}

AActor* UGvTDirectorSubsystem::ChooseHighestPanicTarget() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	TArray<AActor*> Thieves;
	UGameplayStatics::GetAllActorsOfClass(World, AGvTThiefCharacter::StaticClass(), Thieves);

	APawn* BestPawn = nullptr;
	float BestPanic = -1.f;
	float BestScoreTieBreak = -1.f;

	for (AActor* Actor : Thieves)
	{
		APawn* Pawn = Cast<APawn>(Actor);
		if (!Pawn)
		{
			continue;
		}

		const UGvTScareComponent* ScareComp = Pawn->FindComponentByClass<UGvTScareComponent>();
		if (!ScareComp || ScareComp->IsScareBusy())
		{
			continue;
		}

		const APlayerController* PC = Cast<APlayerController>(Pawn->GetController());
		const AGvTPlayerState* PS = PC ? Cast<AGvTPlayerState>(PC->PlayerState) : nullptr;
		if (!PS)
		{
			continue;
		}

		const float Panic01 = FMath::Clamp(PS->GetPanic01(), 0.f, 1.f);
		const float TieBreakScore = ScoreTarget(Pawn);

		if (Panic01 > BestPanic || (FMath::IsNearlyEqual(Panic01, BestPanic) && TieBreakScore > BestScoreTieBreak))
		{
			BestPawn = Pawn;
			BestPanic = Panic01;
			BestScoreTieBreak = TieBreakScore;
		}
	}

	return BestPawn;
}

bool UGvTDirectorSubsystem::DispatchScareEvent(const FGvTScareEvent& Event)
{
	if (!CanPrimaryGhostUseScareTag(Event.ScareTag))
	{
		UE_LOG(LogTemp, Log,
			TEXT("[Director] Blocked scare %s (not allowed by active ghost type)"),
			*Event.ScareTag.ToString());
		return false;
	}

	AActor* Target = Event.TargetActor;
	if (!IsValid(Target))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Director] Dispatch failed: TargetActor is null."));
		return false;
	}

	UGvTScareComponent* ScareComp = Target->FindComponentByClass<UGvTScareComponent>();
	if (!ScareComp)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Director] Dispatch failed: %s has no GvTScareComponent."), *GetNameSafe(Target));
		return false;
	}

	TriggerRequestedFlicker(Event, ScareComp);

	APawn* Pawn = Cast<APawn>(Target);
	APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	AGvTPlayerState* PS = PC ? Cast<AGvTPlayerState>(PC->PlayerState) : nullptr;

	if (Event.ScareTag.MatchesTagExact(GvTScareTags::DoorSlamBehind()))
	{
		AGvTDoorActor* Door = Cast<AGvTDoorActor>(Event.SourceActor);
		APawn* TargetPawn = Cast<APawn>(Target);

		if (!Door)
		{
			UE_LOG(LogTemp, Warning, TEXT("[Director] DoorSlamBehind failed: SourceActor is not a door."));
			return false;
		}

		if (!TargetPawn)
		{
			UE_LOG(LogTemp, Warning, TEXT("[Director] DoorSlamBehind failed: target is not a pawn."));
			return false;
		}

		const bool bSlammed = Door->TriggerScareSlam(false);
		if (!bSlammed)
		{
			UE_LOG(LogTemp, Warning, TEXT("[Director] DoorSlamBehind failed: %s was not slam-eligible."), *GetNameSafe(Door));
			return false;
		}

		const float PanicRadius = FMath::Max(0.f, DoorSlamPanicRadius);
		const float DistSq = FVector::DistSquared(TargetPawn->GetActorLocation(), Door->GetActorLocation());
		const bool bTargetWithinPanicVicinity = DistSq <= FMath::Square(PanicRadius);

		if (PS)
		{
			FGvTPanicEvent PanicEvent;
			PanicEvent.Source = EGvTPanicSource::DoorSlam;
			PanicEvent.PanicDelta01 = (Event.bAffectsPanic && Event.PanicAmount > 0.f) ? (Event.PanicAmount / 100.f) : 0.f;
			PanicEvent.HauntPressureDelta01 = 0.16f;
			PanicEvent.SourceActor = Door;
			PanicEvent.InstigatorActor = Target;
			PanicEvent.WorldLocation = Door->GetActorLocation();
			PanicEvent.SourceRadius = PanicRadius;
			PanicEvent.bRequiresProximity = true;
			PanicEvent.bRequiresSuccessfulExecution = true;
			PanicEvent.bExecutionSucceeded = bSlammed;
			PanicEvent.CooldownSeconds = 3.0f;

			PS->ApplyPanicEventAuthority(PanicEvent);
		}

		UE_LOG(LogTemp, Warning,
			TEXT("[Director] Dispatch DoorSlamBehind target=%s door=%s withinPanicVicinity=%d dist=%.1f radius=%.1f"),
			*GetNameSafe(Target),
			*GetNameSafe(Door),
			bTargetWithinPanicVicinity ? 1 : 0,
			FMath::Sqrt(DistSq),
			PanicRadius);

		return true;
	}

	if (PS)
	{
		FGvTPanicEvent PanicEvent;
		PanicEvent.Source = GvTMapScareTagToPanicSource(Event.ScareTag);
		PanicEvent.PanicDelta01 =
			(Event.bAffectsPanic && Event.PanicAmount > 0.f)
			? (Event.PanicAmount / 100.f)
			: 0.f;
		PanicEvent.HauntPressureDelta01 = GvTGetPressureGain01ForScareTag(Event.ScareTag, Event.bTriggerLocalFlicker);
		PanicEvent.SourceActor = Event.SourceActor;
		PanicEvent.InstigatorActor = Target;
		PanicEvent.WorldLocation = !Event.WorldHint.IsNearlyZero() ? Event.WorldHint : Target->GetActorLocation();
		PanicEvent.bRequiresProximity = false;
		PanicEvent.bRequiresSuccessfulExecution = true;
		PanicEvent.bExecutionSucceeded = true;

		PS->ApplyPanicEventAuthority(PanicEvent);

		UE_LOG(LogTemp, Log,
			TEXT("[DirectorDispatch] Target=%s Tag=%s Panic=%.2f Pressure=%.2f"),
			*GetNameSafe(Target),
			*Event.ScareTag.ToString(),
			PS->GetPanic01(),
			PS->GetRecentHauntPressure01());
	}
	else
	{
		if (Event.bAffectsPanic && Event.PanicAmount > 0.f)
		{
			ScareComp->AddPanic(Event.PanicAmount);
		}
	}

	if (Event.ScareTag.MatchesTagExact(GvTScareTags::Mirror()))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[Director] Dispatch Mirror to %s Mirror=%s"),
			*GetNameSafe(Target),
			*GetNameSafe(Event.SourceActor));

		ScareComp->RequestMirrorScareFromEvent(Event);
		return true;
	}

	if (Event.ScareTag.MatchesTagExact(GvTScareTags::GhostChase()))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Director] Dispatch GhostChase to %s"), *GetNameSafe(Target));
		ScareComp->RequestGhostChaseFromEvent(Target);
		return true;
	}

	if (Event.ScareTag.MatchesTagExact(GvTScareTags::LightChase()))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Director] Dispatch LightChase to %s"), *GetNameSafe(Target));
		ScareComp->RequestLightChaseFromEvent(Event);
		return true;
	}

	if (Event.ScareTag.MatchesTagExact(GvTScareTags::RearAudioSting()))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Director] Dispatch RearAudioSting to %s"), *GetNameSafe(Target));
		ScareComp->RequestRearAudioStingFromEvent(Event);
		return true;
	}

	if (Event.ScareTag.MatchesTagExact(GvTScareTags::GhostScream()))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Director] Dispatch GhostScream near %s"), *GetNameSafe(Target));
		ScareComp->RequestGhostScreamFromEvent(Event);
		return true;
	}

	if (Event.ScareTag.MatchesTagExact(GvTScareTags::GhostScare()))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[Director] Dispatch GhostScare to %s NetMode=%s LocalRole=%d RemoteRole=%d"),
			*GetNameSafe(Target),
			GvTNetModeToString(Target->GetNetMode()),
			(int32)Target->GetLocalRole(),
			(int32)Target->GetRemoteRole());

		if (AGvTThiefCharacter* ThiefTarget = Cast<AGvTThiefCharacter>(Target))
		{
			ThiefTarget->Client_PlayLocalGhostScare(Event);
		}
		else
		{
			ScareComp->RequestGhostScareFromEvent(Event);
		}
		return true;
	}

	UE_LOG(LogTemp, Warning, TEXT("[Director] Dispatch failed: Unknown scare tag %s"), *Event.ScareTag.ToString());
	return false;
}

bool UGvTDirectorSubsystem::DispatchScareEventSimple(
	const FGameplayTag& ScareTag,
	APawn* TargetPawn,
	AActor* SourceActor)
{
	if (!TargetPawn)
	{
		return false;
	}

	FGvTScareEvent Event;

	if (ScareTag.MatchesTagExact(GvTScareTags::Mirror()))
	{
		Event = SourceActor ? MakeMirrorEventForActor(TargetPawn, SourceActor) : MakeMirrorEvent(TargetPawn);
	}
	else if (ScareTag.MatchesTagExact(GvTScareTags::GhostScare()))
	{
		Event = MakeGhostScareEvent(TargetPawn);
	}
	else if (ScareTag.MatchesTagExact(GvTScareTags::GhostChase()))
	{
		Event = MakeGhostChaseEvent(TargetPawn);
	}
	else if (ScareTag.MatchesTagExact(GvTScareTags::LightChase()))
	{
		Event = MakeLightChaseEvent(TargetPawn);
	}
	else if (ScareTag.MatchesTagExact(GvTScareTags::RearAudioSting()))
	{
		Event = MakeRearAudioStingEvent(TargetPawn);
	}
	else if (ScareTag.MatchesTagExact(GvTScareTags::GhostScream()))
	{
		Event = MakeGhostScreamEvent(TargetPawn);
	}
	else if (ScareTag.MatchesTagExact(GvTScareTags::DoorSlamBehind()))
	{
		Event = MakeDoorSlamBehindEvent(TargetPawn, SourceActor);
	}
	else
	{
		Event.TargetActor = TargetPawn;
		Event.ScareTag = ScareTag;
		Event.SourceActor = SourceActor;
		Event.WorldHint = TargetPawn->GetActorLocation();
		Event.PanicAmount = 10.f;
		Event.Intensity01 = 1.0f;
		Event.Duration = 1.5f;
		Event.bAffectsPanic = true;
		Event.bTriggerLocalFlicker = false;
	}

	if (SourceActor && !Event.SourceActor)
	{
		Event.SourceActor = SourceActor;
	}
	if (!Event.TargetActor)
	{
		Event.TargetActor = TargetPawn;
	}
	if (Event.WorldHint.IsNearlyZero())
	{
		Event.WorldHint = TargetPawn->GetActorLocation();
	}

	UE_LOG(LogTemp, Warning, TEXT("[Director] DispatchScareEventSimple built tag=%s target=%s panic=%.1f duration=%.2f"),
		*Event.ScareTag.ToString(),
		*GetNameSafe(Event.TargetActor),
		Event.PanicAmount,
		Event.Duration);

	return DispatchScareEvent(Event);
}

bool UGvTDirectorSubsystem::TriggerRequestedFlicker(const FGvTScareEvent& Event, UGvTScareComponent* TargetScareComp)
{
	if (!TargetScareComp)
	{
		return false;
	}

	if (Event.bTriggerGroupFlicker)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Director] Trigger GROUP flicker"));
		TargetScareComp->Debug_RequestGroupHouseLightFlicker(
			FMath::Clamp(Event.Intensity01, 0.f, 1.f),
			Event.Duration > 0.f ? Event.Duration : 1.5f
		);
		return true;
	}

	if (Event.bTriggerLocalFlicker)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Director] Trigger LOCAL flicker"));
		const FGvTLightFlickerEvent FlickerEvent =
			TargetScareComp->MakeLightFlickerEvent(
				FMath::Clamp(Event.Intensity01, 0.f, 1.f),
				Event.Duration > 0.f ? Event.Duration : 1.5f,
				true);

		TargetScareComp->Client_PlayLightFlicker(FlickerEvent);
		return true;
	}

	return false;
}

FGvTScareEvent UGvTDirectorSubsystem::MakeMirrorEvent(AActor* Target) const
{
	FGvTScareEvent Event;
	Event.ScareTag = GvTScareTags::Mirror();
	Event.TargetActor = Target;
	Event.Intensity01 = 1.0f;
	Event.Duration = MirrorDuration;
	Event.bTriggerLocalFlicker = bMirrorTriggersLocalFlicker;
	Event.bTriggerGroupFlicker = false;
	Event.bAffectsPanic = true;
	Event.PanicAmount = MirrorPanicAmount;
	Event.WorldHint = Target ? Target->GetActorLocation() : FVector::ZeroVector;
	Event.SourceActor = nullptr;
	return Event;
}

FGvTScareEvent UGvTDirectorSubsystem::MakeMirrorEventForActor(AActor* Target, AActor* MirrorActor) const
{
	FGvTScareEvent Event = MakeMirrorEvent(Target);
	Event.SourceActor = MirrorActor;
	Event.WorldHint = MirrorActor ? MirrorActor->GetActorLocation() : FVector::ZeroVector;
	return Event;
}

FGvTScareEvent UGvTDirectorSubsystem::MakeGhostScareEvent(AActor* Target) const
{
	FGvTScareEvent Event;
	Event.ScareTag = GvTScareTags::GhostScare();
	Event.TargetActor = Target;
	Event.Intensity01 = 1.0f;
	Event.Duration = GhostScareDuration;
	Event.bTriggerLocalFlicker = bGhostScareTriggersLocalFlicker;
	Event.bTriggerGroupFlicker = false;
	Event.bAffectsPanic = true;
	Event.PanicAmount = GhostScarePanicAmount;
	return Event;
}

FGvTScareEvent UGvTDirectorSubsystem::MakeGhostChaseEvent(AActor* Target) const
{
	FGvTScareEvent Event;
	Event.ScareTag = GvTScareTags::GhostChase();
	Event.TargetActor = Target;
	Event.Intensity01 = 1.0f;
	Event.Duration = GhostChaseDuration;
	Event.bTriggerLocalFlicker = false;
	Event.bTriggerGroupFlicker = bGhostChaseTriggersGroupFlicker;
	Event.bAffectsPanic = true;
	Event.PanicAmount = GhostChasePanicAmount;
	return Event;
}

FGvTScareEvent UGvTDirectorSubsystem::MakeLightChaseEvent(AActor* Target) const
{
	FGvTScareEvent Event;
	Event.ScareTag = GvTScareTags::LightChase();
	Event.TargetActor = Target;
	Event.Intensity01 = 1.0f;

	Event.LightChaseStepCount = LightChaseStepCount;
	Event.LightChaseStepInterval = LightChaseStepInterval;
	Event.LightChaseStartDistance = LightChaseStartDistance;
	Event.LightChaseEndDistance = LightChaseEndDistance;
	Event.LightChaseFlickerRadius = LightChaseFlickerRadius;
	Event.LightChaseAudioLeadDistance = LightChaseAudioLeadDistance;

	Event.Duration = (LightChaseStepCount * LightChaseStepInterval) + 0.25f;
	Event.bTriggerLocalFlicker = false;
	Event.bTriggerGroupFlicker = false;
	Event.bAffectsPanic = true;
	Event.PanicAmount = LightChasePanicAmount;
	return Event;
}

FGvTScareEvent UGvTDirectorSubsystem::MakeRearAudioStingEvent(AActor* Target) const
{
	FGvTScareEvent Event;
	Event.ScareTag = GvTScareTags::RearAudioSting();
	Event.TargetActor = Target;
	Event.Intensity01 = 1.0f;
	Event.Duration = RearAudioStingDuration;
	Event.bTriggerLocalFlicker = false;
	Event.bTriggerGroupFlicker = false;
	Event.bAffectsPanic = true;
	Event.PanicAmount = RearAudioStingPanicAmount;
	Event.bTwoShotAudio = bRearAudioAllowTwoShot && (FMath::FRand() <= RearAudioTwoShotChance);
	Event.FollowupDelay = 0.18f;
	Event.RearAudioBackOffset = RearAudioBackOffset;
	Event.RearAudioSideOffset = RearAudioSideOffset;
	Event.RearAudioUpOffset = RearAudioUpOffset;
	Event.LocalSeed = FMath::Rand();
	return Event;
}

FGvTScareEvent UGvTDirectorSubsystem::MakeGhostScreamEvent(AActor* Target) const
{
	FGvTScareEvent Event;
	Event.ScareTag = GvTScareTags::GhostScream();
	Event.TargetActor = Target;
	Event.Intensity01 = 1.0f;
	Event.Duration = GhostScreamDuration;
	Event.bTriggerLocalFlicker = false;
	Event.bTriggerGroupFlicker = false;
	Event.bAffectsPanic = true;
	Event.PanicAmount = GhostScreamPanicAmount;
	Event.SharedAudioRadius = GhostScreamAudibleRadius;
	Event.LocalSeed = FMath::Rand();

	const APawn* Pawn = Cast<APawn>(Target);
	const FVector Origin = Pawn ? Pawn->GetActorLocation() : (Target ? Target->GetActorLocation() : FVector::ZeroVector);

	FVector Forward = Target ? Target->GetActorForwardVector() : FVector::ForwardVector;
	FVector Right = Target ? Target->GetActorRightVector() : FVector::RightVector;

	if (Pawn)
	{
		if (const AController* Controller = Pawn->GetController())
		{
			const FRotator ControlRot = Controller->GetControlRotation();
			Forward = FRotationMatrix(ControlRot).GetUnitAxis(EAxis::X);
			Right = FRotationMatrix(ControlRot).GetUnitAxis(EAxis::Y);
		}
	}

	Forward.Z = 0.f;
	Right.Z = 0.f;
	Forward = Forward.GetSafeNormal();
	Right = Right.GetSafeNormal();

	if (Forward.IsNearlyZero())
	{
		Forward = FVector::ForwardVector;
	}
	if (Right.IsNearlyZero())
	{
		Right = FVector::RightVector;
	}

	FRandomStream Stream(Event.LocalSeed);
	const float SideSign = (Stream.FRand() < 0.5f) ? 1.f : -1.f;
	const float Dist = Stream.FRandRange(GhostScreamSpawnDistanceMin, GhostScreamSpawnDistanceMax);

	const FVector Dir = ((-Forward * 0.85f) + (Right * 0.55f * SideSign)).GetSafeNormal();
	Event.WorldHint = Origin + (Dir * Dist) + FVector(0.f, 0.f, 70.f);

	return Event;
}

FGvTScareEvent UGvTDirectorSubsystem::MakeDoorSlamBehindEvent(AActor* Target, AActor* DoorActor) const
{
	FGvTScareEvent Event;
	Event.ScareTag = GvTScareTags::DoorSlamBehind();
	Event.TargetActor = Target;
	Event.SourceActor = DoorActor;
	Event.WorldHint = DoorActor ? DoorActor->GetActorLocation() : FVector::ZeroVector;
	Event.Intensity01 = 1.0f;
	Event.Duration = DoorSlamBehindDuration;
	Event.bTriggerLocalFlicker = false;
	Event.bTriggerGroupFlicker = false;
	Event.bAffectsPanic = true;
	Event.PanicAmount = DoorSlamBehindPanicAmount;
	Event.LocalSeed = FMath::Rand();
	return Event;
}

float UGvTDirectorSubsystem::GetHauntPressureForPawn(const APawn* Pawn) const
{
	if (!Pawn)
	{
		return 0.f;
	}

	const APlayerController* PC = Cast<APlayerController>(Pawn->GetController());
	const AGvTPlayerState* PS = PC ? Cast<AGvTPlayerState>(PC->PlayerState) : nullptr;
	return PS ? FMath::Clamp(PS->GetRecentHauntPressure01(), 0.f, 1.f) : 0.f;
}

float UGvTDirectorSubsystem::ScoreTarget(APawn* Pawn) const
{
	if (!Pawn)
	{
		return -1.f;
	}

	const UGvTScareComponent* ScareComp = Pawn->FindComponentByClass<UGvTScareComponent>();
	if (!ScareComp)
	{
		return -1.f;
	}

	const APlayerController* PC = Cast<APlayerController>(Pawn->GetController());
	const AGvTPlayerState* PS = PC ? Cast<AGvTPlayerState>(PC->PlayerState) : nullptr;
	if (!PS)
	{
		return -1.f;
	}

	const AGvTGhostCharacterBase* ActiveGhost = GetPrimaryGhost();
	const FGvTGhostRuntimeConfig* Runtime = ActiveGhost ? &ActiveGhost->GetRuntimeConfig() : nullptr;

	float AggressionMult = Runtime ? Runtime->HuntAggressionMultiplier : 1.0f;
	float NoiseMult = Runtime ? Runtime->NoiseInterestMultiplier : 1.0f;
	float PanicMult = Runtime ? Runtime->PanicBiasMultiplier : 1.0f;
	float IsolationMult = Runtime ? Runtime->IsolationBiasMultiplier : 1.0f;
	float PressureMult = Runtime ? Runtime->RecentHauntPressureBiasMultiplier : 1.0f;

	const float Panic01 = FMath::Clamp(PS->GetPanic01(), 0.f, 1.f);
	const float Pressure01 = FMath::Clamp(PS->GetRecentHauntPressure01(), 0.f, 1.f);
	const float Isolation01 = ComputeIsolationScore(Pawn);
	const float Noise01 = 0.f;
	const float RecentTargetPenalty01 = ComputeRecentTargetPenalty01(Pawn);
	const float BusyPenalty01 = ScareComp->IsScareBusy() ? 1.f : 0.f;

	float Score = BaseTargetScore;
	Score += Panic01 * PanicTargetWeight * AggressionMult * PanicMult;
	Score += Isolation01 * IsolationTargetWeight * IsolationMult;
	Score += Noise01 * NearbyNoiseTargetWeight * NoiseMult;
	Score -= Pressure01 * HauntPressurePenaltyWeight * PressureMult;
	Score -= RecentTargetPenalty01 * RecentTargetPenaltyWeight;
	Score -= BusyPenalty01 * BusyTargetPenalty;

	UE_LOG(LogTemp, Log,
		TEXT("[DirectorScore] Target=%s Panic=%.2f Isolation=%.2f Noise=%.2f Pressure=%.2f RecentPenalty=%.2f Busy=%.2f Final=%.2f"),
		*GetNameSafe(Pawn),
		Panic01,
		Isolation01,
		Noise01,
		Pressure01,
		RecentTargetPenalty01,
		BusyPenalty01,
		Score
	);

	return FMath::Max(0.01f, Score);
}

float UGvTDirectorSubsystem::ComputeIsolationScore(const APawn* Pawn) const
{
	if (!Pawn)
	{
		return 0.f;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return 0.f;
	}

	TArray<AActor*> Thieves;
	UGameplayStatics::GetAllActorsOfClass(World, AGvTThiefCharacter::StaticClass(), Thieves);

	float NearestDistSq = TNumericLimits<float>::Max();
	const FVector MyLoc = Pawn->GetActorLocation();

	for (AActor* Actor : Thieves)
	{
		const APawn* OtherPawn = Cast<APawn>(Actor);
		if (!OtherPawn || OtherPawn == Pawn)
		{
			continue;
		}

		const float DistSq = FVector::DistSquared(MyLoc, OtherPawn->GetActorLocation());
		NearestDistSq = FMath::Min(NearestDistSq, DistSq);
	}

	if (NearestDistSq == TNumericLimits<float>::Max())
	{
		return 1.f; // alone? congrats, haunted.
	}

	const float NearestDist = FMath::Sqrt(NearestDistSq);

	// 300uu = not isolated, 2000uu = very isolated
	return FMath::Clamp((NearestDist - 300.f) / 1700.f, 0.f, 1.f);
}

float UGvTDirectorSubsystem::ComputeRecentTargetPenalty01(const APawn* Pawn) const
{
	if (!Pawn)
	{
		return 0.f;
	}

	const UWorld* World = GetWorld();
	if (!World)
	{
		return 0.f;
	}

	const TWeakObjectPtr<APawn> Key(const_cast<APawn*>(Pawn));
	const float* LastTimePtr = LastTargetedTimeSeconds.Find(Key);
	if (!LastTimePtr)
	{
		return 0.f;
	}

	const float Elapsed = World->GetTimeSeconds() - *LastTimePtr;
	if (Elapsed >= RecentTargetMemorySeconds)
	{
		return 0.f;
	}

	return 1.f - FMath::Clamp(Elapsed / RecentTargetMemorySeconds, 0.f, 1.f);
}

void UGvTDirectorSubsystem::RememberTarget(APawn* Pawn)
{
	if (!Pawn || !GetWorld())
	{
		return;
	}

	LastTargetedTimeSeconds.Add(Pawn, GetWorld()->GetTimeSeconds());

	TArray<TWeakObjectPtr<APawn>> KeysToRemove;
	for (const TPair<TWeakObjectPtr<APawn>, float>& Pair : LastTargetedTimeSeconds)
	{
		if (!Pair.Key.IsValid())
		{
			KeysToRemove.Add(Pair.Key);
			continue;
		}

		if ((GetWorld()->GetTimeSeconds() - Pair.Value) > (RecentTargetMemorySeconds * 2.f))
		{
			KeysToRemove.Add(Pair.Key);
		}
	}

	for (const TWeakObjectPtr<APawn>& Key : KeysToRemove)
	{
		LastTargetedTimeSeconds.Remove(Key);
	}
}

float UGvTDirectorSubsystem::ComputeAveragePlayerPanic01() const
{
	const UWorld* World = GetWorld();
	const AGameStateBase* GS = World ? World->GetGameState() : nullptr;
	if (!GS || GS->PlayerArray.Num() == 0)
	{
		return 0.f;
	}

	float Sum = 0.f;
	int32 Count = 0;

	for (APlayerState* PSBase : GS->PlayerArray)
	{
		const AGvTPlayerState* PS = Cast<AGvTPlayerState>(PSBase);
		if (!PS)
		{
			continue;
		}

		Sum += FMath::Clamp(PS->GetPanic01(), 0.f, 1.f);
		Count++;
	}

	return (Count > 0) ? (Sum / float(Count)) : 0.f;
}

float UGvTDirectorSubsystem::ComputeAveragePlayerPressure01() const
{
	const UWorld* World = GetWorld();
	const AGameStateBase* GS = World ? World->GetGameState() : nullptr;
	if (!GS || GS->PlayerArray.Num() == 0)
	{
		return 0.f;
	}

	float Sum = 0.f;
	int32 Count = 0;

	for (APlayerState* PSBase : GS->PlayerArray)
	{
		const AGvTPlayerState* PS = Cast<AGvTPlayerState>(PSBase);
		if (!PS)
		{
			continue;
		}

		Sum += FMath::Clamp(PS->GetRecentHauntPressure01(), 0.f, 1.f);
		Count++;
	}

	return (Count > 0) ? (Sum / float(Count)) : 0.f;
}

void UGvTDirectorSubsystem::UpdateHouseTension(float DeltaSeconds)
{
	const float AvgPanic01 = ComputeAveragePlayerPanic01();
	const float AvgPressure01 = ComputeAveragePlayerPressure01();

	const float TargetTension =
		FMath::Clamp(
			(AvgPanic01 * AvgPanicToTensionWeight) +
			(AvgPressure01 * AvgPressureToTensionWeight),
			0.f,
			1.f);

	HouseTension01 = FMath::FInterpTo(
		HouseTension01,
		TargetTension,
		DeltaSeconds,
		FMath::Max(0.01f, HouseTensionDecayPerSecond * 10.f));

	HouseTension01 = FMath::Clamp(
		HouseTension01 - (HouseTensionDecayPerSecond * DeltaSeconds * 0.25f),
		0.f,
		1.f);

	if (bLogHouseTension)
	{
		const TCHAR* Band =
			(HouseTension01 < 0.25f) ? TEXT("Low") :
			(HouseTension01 < 0.60f) ? TEXT("Mid") :
			TEXT("High");

		UE_LOG(LogTemp, Log,
			TEXT("[HouseTension] AvgPanic=%.2f AvgPressure=%.2f Target=%.2f Final=%.2f Band=%s"),
			AvgPanic01,
			AvgPressure01,
			TargetTension,
			HouseTension01,
			Band);
	}
}

float UGvTDirectorSubsystem::GetDispatchTensionImpulse(const FGvTScareEvent& Event) const
{
	if (Event.ScareTag == GvTScareTags::Mirror())
	{
		return MirrorDispatchTensionImpulse;
	}

	if (Event.ScareTag == GvTScareTags::GhostScare())
	{
		return GhostScareDispatchTensionImpulse;
	}

	if (Event.ScareTag == GvTScareTags::GhostChase())
	{
		return GhostChaseDispatchTensionImpulse;
	}

	if (Event.ScareTag.MatchesTagExact(GvTScareTags::LightChase()))
	{
		return LightChaseDispatchTensionImpulse;
	}

	if (Event.ScareTag == GvTScareTags::RearAudioSting())
	{
		return RearAudioDispatchTensionImpulse;
	}

	if (Event.ScareTag == GvTScareTags::GhostScream())
	{
		return GhostScreamDispatchTensionImpulse;
	}

	if (Event.ScareTag == GvTScareTags::DoorSlamBehind())
	{
		return DoorSlamDispatchTensionImpulse;
	}

	return GenericDispatchTensionImpulse;
}

void UGvTDirectorSubsystem::ApplyHouseTensionImpulse(float Delta01)
{
	HouseTension01 = FMath::Clamp(HouseTension01 + FMath::Max(0.f, Delta01), 0.f, 1.f);

	if (bLogHouseTension)
	{
		UE_LOG(LogTemp, Log,
			TEXT("[HouseTension] Impulse=%.2f NewTension=%.2f"),
			Delta01,
			HouseTension01);
	}
}

float UGvTDirectorSubsystem::GetCurrentGlobalHauntCooldown() const
{
	return FMath::Lerp(GlobalHauntCooldownMax, GlobalHauntCooldownMin, HouseTension01);
}

float UGvTDirectorSubsystem::ScoreDoorForSlam(const APawn* TargetPawn, const AGvTDoorActor* Door) const
{
	if (!TargetPawn || !Door)
	{
		return -1.f;
	}

	if (!Door->IsOpenForScareSlam())
	{
		return -1.f;
	}

	const FVector PawnLoc = TargetPawn->GetActorLocation();
	const FVector DoorLoc = Door->GetActorLocation();

	FVector Forward = TargetPawn->GetActorForwardVector();

	if (const AController* Controller = TargetPawn->GetController())
	{
		Forward = Controller->GetControlRotation().Vector();
	}

	Forward.Z = 0.f;
	Forward = Forward.GetSafeNormal();
	if (Forward.IsNearlyZero())
	{
		Forward = FVector::ForwardVector;
	}

	FVector ToDoor = DoorLoc - PawnLoc;
	ToDoor.Z = 0.f;

	const float Dist = ToDoor.Length();
	if (Dist > DoorSlamSearchRadius || Dist <= KINDA_SMALL_NUMBER)
	{
		return -1.f;
	}

	ToDoor /= Dist;

	const float FrontDot = FVector::DotProduct(Forward, ToDoor);

	// Reject doors too far in front of the player.
	if (FrontDot > DoorSlamMaxFrontDot)
	{
		return -1.f;
	}

	const float BehindScore = FMath::Clamp((-FrontDot + 1.f) * 0.5f, 0.f, 1.f);
	const float DistanceScore = 1.f - FMath::Clamp(Dist / FMath::Max(1.f, DoorSlamSearchRadius), 0.f, 1.f);

	return (BehindScore * DoorSlamBehindWeight) + (DistanceScore * DoorSlamDistanceWeight);
}

AGvTDoorActor* UGvTDirectorSubsystem::ChooseBestDoorSlamTarget(APawn* TargetPawn) const
{
	if (!TargetPawn || !GetWorld())
	{
		return nullptr;
	}

	TArray<AActor*> FoundDoors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AGvTDoorActor::StaticClass(), FoundDoors);

	AGvTDoorActor* BestBehindDoor = nullptr;
	float BestBehindScore = -1.f;

	for (AActor* Actor : FoundDoors)
	{
		AGvTDoorActor* Door = Cast<AGvTDoorActor>(Actor);
		if (!Door || Door->IsLocked())
		{
			continue;
		}

		// No closed-door fallback: DoorSlamBehind is only valid when a readable,
		// already-open door can slam shut.
		const float Score = ScoreDoorForSlam(TargetPawn, Door);
		if (Score > BestBehindScore)
		{
			BestBehindScore = Score;
			BestBehindDoor = Door;
		}
	}

	return BestBehindDoor;
}

AActor* UGvTDirectorSubsystem::FindBestDoorSlamDoor(AActor* Target) const
{
	return ChooseBestDoorSlamTarget(Cast<APawn>(Target));
}

void UGvTDirectorSubsystem::OnPlayerInteractionEvent(AActor* Interactor, AActor* TargetActor)
{
	if (!Interactor)
	{
		return;
	}

	APawn* Pawn = Cast<APawn>(Interactor);
	if (!Pawn)
	{
		return;
	}

	AGvTPlayerState* PS = Pawn->GetPlayerState<AGvTPlayerState>();
	if (!PS)
	{
		return;
	}

	if (IsInteractionReactionOnCooldown(Pawn))
	{
		UE_LOG(LogTemp, Verbose,
			TEXT("[Director] Interaction reaction cooldown active for %s"),
			*GetNameSafe(Pawn));
		return;
	}

	bool bIsElectrical = false;
	bool bIsValuable = false;
	bool bIsNoisy = false;
	float ItemValue01 = 0.f;

	// Current simple actor-tag path
	if (TargetActor)
	{
		if (TargetActor->ActorHasTag(TEXT("Electrical")))
		{
			bIsElectrical = true;
		}

		if (TargetActor->ActorHasTag(TEXT("Valuable")))
		{
			bIsValuable = true;
			ItemValue01 = 1.0f;
		}

		if (TargetActor->ActorHasTag(TEXT("Noisy")))
		{
			bIsNoisy = true;
		}
	}

	const float ReactionChance = ComputeInteractionReactionChance(
		Pawn,
		TargetActor,
		bIsElectrical,
		bIsValuable,
		bIsNoisy,
		ItemValue01);

	UE_LOG(LogTemp, Verbose,
		TEXT("[Director] InteractionEvent Target=%s Source=%s Chance=%.2f Elec=%d Value=%d Noisy=%d Loot=%d Panic=%.2f Pressure=%.2f"),
		*GetNameSafe(Pawn),
		*GetNameSafe(TargetActor),
		ReactionChance,
		bIsElectrical ? 1 : 0,
		bIsValuable ? 1 : 0,
		bIsNoisy ? 1 : 0,
		PS->GetLoot(),
		PS->GetPanic01(),
		PS->GetRecentHauntPressure01());

	if (FMath::FRand() > ReactionChance)
	{
		return;
	}

	TriggerInteractionReaction(Pawn, TargetActor, bIsElectrical, bIsValuable, bIsNoisy, ItemValue01);
	MarkInteractionReactionTriggered(Pawn);
}

void UGvTDirectorSubsystem::TriggerInteractionReaction(
	APawn* Pawn,
	AActor* TargetActor,
	bool bIsElectrical,
	bool bIsValuable,
	bool bIsNoisy,
	float ItemValue01)
{
	if (!Pawn)
	{
		return;
	}

	const UGvTScareComponent* ScareComp = Pawn->FindComponentByClass<UGvTScareComponent>();
	if (!ScareComp || ScareComp->IsScareBusy())
	{
		UE_LOG(LogTemp, Verbose,
			TEXT("[Director] Interaction reaction skipped for %s because scare component is busy"),
			*GetNameSafe(Pawn));
		return;
	}

	const FGameplayTag ChosenScare = ChooseWeightedInteractionScare(
		Pawn,
		TargetActor,
		bIsElectrical,
		bIsValuable,
		bIsNoisy,
		ItemValue01);

	if (!ChosenScare.IsValid())
	{
		UE_LOG(LogTemp, Verbose,
			TEXT("[Director] No weighted scare selected for %s"),
			*GetNameSafe(Pawn));
		return;
	}

	LastInteractionScareByPlayer.Add(Pawn, ChosenScare);

	UE_LOG(LogTemp, Log,
		TEXT("[Director] Weighted interaction scare selected: Target=%s Source=%s Tag=%s"),
		*GetNameSafe(Pawn),
		*GetNameSafe(TargetActor),
		*ChosenScare.ToString());

	DispatchScareEventSimple(ChosenScare, Pawn, TargetActor);
}

AGvTPowerBoxActor* UGvTDirectorSubsystem::FindPowerBoxInWorld()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	for (TActorIterator<AGvTPowerBoxActor> It(World); It; ++It)
	{
		return *It;
	}

	return nullptr;
}

void UGvTDirectorSubsystem::AddScareWeight(
	TArray<FGvTWeightedScareChoice>& Choices,
	const FGameplayTag& Tag,
	float Weight)
{
	if (!Tag.IsValid() || FMath::IsNearlyZero(Weight))
	{
		return;
	}

	for (FGvTWeightedScareChoice& Entry : Choices)
	{
		if (Entry.ScareTag == Tag)
		{
			Entry.Weight = FMath::Max(0.f, Entry.Weight + Weight);
			return;
		}
	}

	if (Weight > 0.f)
	{
		FGvTWeightedScareChoice NewEntry;
		NewEntry.ScareTag = Tag;
		NewEntry.Weight = Weight;
		Choices.Add(NewEntry);
	}
}

FGameplayTag UGvTDirectorSubsystem::PickWeightedScare(const TArray<FGvTWeightedScareChoice>& Choices)
{
	float TotalWeight = 0.f;

	for (const FGvTWeightedScareChoice& Entry : Choices)
	{
		if (Entry.Weight > 0.f)
		{
			TotalWeight += Entry.Weight;
		}
	}

	if (TotalWeight <= KINDA_SMALL_NUMBER)
	{
		return FGameplayTag();
	}

	const float Roll = FMath::FRandRange(0.f, TotalWeight);
	float Running = 0.f;

	for (const FGvTWeightedScareChoice& Entry : Choices)
	{
		if (Entry.Weight <= 0.f)
		{
			continue;
		}

		Running += Entry.Weight;
		if (Roll <= Running)
		{
			return Entry.ScareTag;
		}
	}

	return Choices.Num() > 0 ? Choices.Last().ScareTag : FGameplayTag();
}

FGameplayTag UGvTDirectorSubsystem::ChooseWeightedInteractionScare(
	APawn* Pawn,
	AActor* TargetActor,
	bool bIsElectrical,
	bool bIsValuable,
	bool bIsNoisy,
	float ItemValue01) const
{
	if (!Pawn)
	{
		return FGameplayTag();
	}

	const UGvTGhostTypeData* GhostType = GetPrimaryGhostType();
	const UGvTScareComponent* ScareComp = Pawn->FindComponentByClass<UGvTScareComponent>();
	const bool bBusy = ScareComp && ScareComp->IsScareBusy();

	if (bBusy)
	{
		return FGameplayTag();
	}

	const AGvTPlayerState* PS = Pawn->GetPlayerState<AGvTPlayerState>();
	const float Panic = PS ? PS->GetPanic01() : 0.f;
	const float Pressure = PS ? PS->GetRecentHauntPressure01() : 0.f;

	const int32 PanicStage = PS
		? FMath::Clamp(FMath::FloorToInt(Panic * 5.0f), 0, 4)
		: 0;

	TArray<FGvTWeightedScareChoice> Choices;

	// Base pool
	AddScareWeight(Choices, GvTScareTags::RearAudioSting(), 0.70f);
	AddScareWeight(Choices, GvTScareTags::GhostScream(), 1.05f);
	AddScareWeight(Choices, GvTScareTags::LightChase(), 0.80f);
	AddScareWeight(Choices, GvTScareTags::Mirror(), 0.85f);

	// Heavier pool
	AddScareWeight(Choices, GvTScareTags::GhostScare(), 0.10f + (Panic * 0.95f) + (Pressure * 0.40f));
	AddScareWeight(Choices, GvTScareTags::GhostChase(), 0.05f + (Panic * 0.80f) + (Pressure * 0.55f));

	// ---------- Context shaping ----------

	if (bIsElectrical)
	{
		AddScareWeight(Choices, GvTScareTags::LightChase(), 1.75f);
		AddScareWeight(Choices, GvTScareTags::GhostScream(), 0.65f);
		AddScareWeight(Choices, GvTScareTags::Mirror(), 0.10f);
	}

	if (bIsValuable)
	{
		AddScareWeight(Choices, GvTScareTags::GhostScream(), 1.00f);
		AddScareWeight(Choices, GvTScareTags::Mirror(), 0.75f + (ItemValue01 * 0.65f));
		AddScareWeight(Choices, GvTScareTags::RearAudioSting(), 0.35f);
		AddScareWeight(Choices, GvTScareTags::GhostScare(), 0.20f + (ItemValue01 * 0.30f));
	}

	if (bIsNoisy)
	{
		AddScareWeight(Choices, GvTScareTags::GhostScream(), 0.55f);
		AddScareWeight(Choices, GvTScareTags::RearAudioSting(), 0.45f);
		AddScareWeight(Choices, GvTScareTags::GhostChase(), 1.20f);
	}

	const AGvTGhostCharacterBase* ActiveGhost = GetPrimaryGhost();
	const FGvTGhostRuntimeConfig* Runtime = ActiveGhost ? &ActiveGhost->GetRuntimeConfig() : nullptr;

	if (Runtime)
	{
		AddScareWeight(Choices, GvTScareTags::GhostChase(),
			Runtime->HuntAggressionMultiplier * 0.5f);

		AddScareWeight(Choices, GvTScareTags::LightChase(),
			Runtime->ElectricalInterestMultiplier * 0.6f);

		AddScareWeight(Choices, GvTScareTags::GhostScream(),
			Runtime->NoiseInterestMultiplier * 0.35f);

		AddScareWeight(Choices, GvTScareTags::RearAudioSting(),
			Runtime->NoiseInterestMultiplier * 0.30f);

		AddScareWeight(Choices, GvTScareTags::Mirror(),
			Runtime->ValuableItemInterestMultiplier * 0.20f);
	}

	// ---------- Panic-stage shaping ----------

	if (PanicStage <= 0)
	{
		AddScareWeight(Choices, GvTScareTags::GhostScare(), -999.f);
		AddScareWeight(Choices, GvTScareTags::GhostChase(), -999.f);

		AddScareWeight(Choices, GvTScareTags::RearAudioSting(), 0.10f);
		AddScareWeight(Choices, GvTScareTags::GhostScream(), 0.20f);
		AddScareWeight(Choices, GvTScareTags::Mirror(), 0.25f);
	}
	else if (PanicStage == 1)
	{
		AddScareWeight(Choices, GvTScareTags::GhostScare(), -0.20f);
		AddScareWeight(Choices, GvTScareTags::GhostChase(), -0.55f);

		AddScareWeight(Choices, GvTScareTags::Mirror(), 0.45f);
		AddScareWeight(Choices, GvTScareTags::GhostScream(), 0.35f);
		AddScareWeight(Choices, GvTScareTags::RearAudioSting(), -0.10f);
	}
	else if (PanicStage == 2)
	{
		AddScareWeight(Choices, GvTScareTags::GhostScare(), 0.65f);
		AddScareWeight(Choices, GvTScareTags::GhostChase(), 0.80f);
		AddScareWeight(Choices, GvTScareTags::Mirror(), 0.20f);
	}
	else
	{
		AddScareWeight(Choices, GvTScareTags::GhostScare(), 1.00f);
		AddScareWeight(Choices, GvTScareTags::GhostChase(), 1.20f);
		AddScareWeight(Choices, GvTScareTags::RearAudioSting(), -0.20f);
	}

	ApplyAntiRepeatBias(Pawn, Choices);

	for (const FGvTWeightedScareChoice& Entry : Choices)
	{
		if (Entry.ScareTag.IsValid() && Entry.Weight > 0.f)
		{
			UE_LOG(LogTemp, Verbose,
				TEXT("[DirectorWeight] Target=%s Source=%s Tag=%s Weight=%.2f Stage=%d Panic=%.2f Pressure=%.2f Elec=%d Value=%d Noisy=%d"),
				*GetNameSafe(Pawn),
				*GetNameSafe(TargetActor),
				*Entry.ScareTag.ToString(),
				Entry.Weight,
				PanicStage,
				Panic,
				Pressure,
				bIsElectrical ? 1 : 0,
				bIsValuable ? 1 : 0,
				bIsNoisy ? 1 : 0);
		}
	}

	// Filter out invalid scares based on active ghost type
	TArray<FGvTWeightedScareChoice> FilteredChoices;

	for (const FGvTWeightedScareChoice& Entry : Choices)
	{
		if (Entry.Weight <= 0.f || !Entry.ScareTag.IsValid())
		{
			continue;
		}

		if (!CanPrimaryGhostUseScareTag(Entry.ScareTag))
		{
			continue;
		}

		FilteredChoices.Add(Entry);
	}

	if (FilteredChoices.Num() == 0)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[Director] No valid scare choices after ghost-type filtering"));
		return FGameplayTag();
	}

	return PickWeightedScare(FilteredChoices); 
}

bool UGvTDirectorSubsystem::IsInteractionReactionOnCooldown(APawn* Pawn) const
{
	if (!Pawn)
	{
		return true;
	}

	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	const float* LastTime = LastInteractionReactionTime.Find(Pawn);
	if (!LastTime)
	{
		return false;
	}

	const float Now = World->GetTimeSeconds();
	return (Now - *LastTime) < InteractionReactionMinGapSeconds;
}

void UGvTDirectorSubsystem::MarkInteractionReactionTriggered(APawn* Pawn)
{
	if (!Pawn)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	LastInteractionReactionTime.Add(Pawn, World->GetTimeSeconds());
}

float UGvTDirectorSubsystem::ComputeInteractionReactionChance(
	APawn* Pawn,
	AActor* TargetActor,
	bool bIsElectrical,
	bool bIsValuable,
	bool bIsNoisy,
	float ItemValue01) const
{
	if (!Pawn)
	{
		return 0.f;
	}

	const AGvTPlayerState* PS = Pawn->GetPlayerState<AGvTPlayerState>();
	if (!PS)
	{
		return 0.f;
	}

	const float Panic = PS->GetPanic01();
	const float Pressure = PS->GetRecentHauntPressure01();

	const float LootPressure01 = FMath::Clamp(
		float(PS->GetLoot()) / FMath::Max(1.f, LootPressureMaxLootValue),
		0.f,
		1.f);

	float ReactionChance = BaseInteractionReactionChance;

	ReactionChance += Panic * 0.20f;
	ReactionChance += Pressure * 0.18f;
	ReactionChance += LootPressure01 * (LootPressureChanceBonus + 0.10f);

	if (bIsElectrical) ReactionChance += 0.15f;
	if (bIsValuable)   ReactionChance += 0.25f;
	if (bIsNoisy)      ReactionChance += 0.12f;

	ReactionChance += FMath::Clamp(ItemValue01, 0.f, 1.f) * 0.15f;

	if (PS->GetLoot() >= 300)
	{
		ReactionChance += 0.08f;
	}
	if (PS->GetLoot() >= 500)
	{
		ReactionChance += 0.10f;
	}
	if (PS->GetLoot() >= 800)
	{
		ReactionChance += 0.10f;
	}

	return FMath::Clamp(ReactionChance, 0.f, 0.95f);
}

void UGvTDirectorSubsystem::ApplyAntiRepeatBias(
	APawn* Pawn,
	TArray<FGvTWeightedScareChoice>& Choices) const
{
	if (!Pawn)
	{
		return;
	}

	const FGameplayTag* LastTag = LastInteractionScareByPlayer.Find(Pawn);
	if (!LastTag || !LastTag->IsValid())
	{
		return;
	}

	for (FGvTWeightedScareChoice& Entry : Choices)
	{
		if (Entry.ScareTag == *LastTag)
		{
			Entry.Weight *= AntiRepeatWeightMultiplier;
			break;
		}
	}
}

void UGvTDirectorSubsystem::InitializeRoundGhosts()
{
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client)
	{
		return;
	}

	ClearRoundGhosts();

	if (AvailableGhostModels.Num() == 0 || AvailableGhostTypes.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Director] Cannot initialize round ghosts. Missing model/type assets."));
		return;
	}

	const int32 GhostCount = FMath::Max(1, NumGhostsThisRound);

	for (int32 Index = 0; Index < GhostCount; ++Index)
	{
		FGvTGhostRoundSpawnSpec SpawnSpec;
		if (!BuildRoundGhostSpawnSpec(Index, SpawnSpec))
		{
			UE_LOG(LogTemp, Warning, TEXT("[Director] Failed to build round ghost spawn spec for slot %d"), Index);
			continue;
		}

		FGvTGhostRuntimeConfig RuntimeConfig;
		if (!BuildRuntimeConfig(SpawnSpec.ModelData, SpawnSpec.TypeData, RuntimeConfig))
		{
			UE_LOG(LogTemp, Warning, TEXT("[Director] Failed to build runtime config for %s"), *SpawnSpec.GhostId.ToString());
			continue;
		}

		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

		AGvTGhostCharacterBase* Ghost = World->SpawnActor<AGvTGhostCharacterBase>(
			SpawnSpec.ModelData->GhostActorClass,
			SpawnSpec.SpawnTransform,
			Params);

		if (!Ghost)
		{
			UE_LOG(LogTemp, Warning, TEXT("[Director] Failed to spawn ghost for slot %d"), Index);
			continue;
		}

		Ghost->InitializeGhost(SpawnSpec.GhostId, SpawnSpec.ModelData, SpawnSpec.TypeData, RuntimeConfig);
		Ghost->SetGhostPresenceActive(false);
		ActiveGhosts.Add(Ghost);

		UE_LOG(LogTemp, Log,
			TEXT("[Director] Spawned dormant %s using Model=%s Type=%s Spawn=%s"),
			*SpawnSpec.GhostId.ToString(),
			*GetNameSafe(SpawnSpec.ModelData),
			*GetNameSafe(SpawnSpec.TypeData),
			*SpawnSpec.SpawnTransform.GetLocation().ToCompactString());
	}
}

void UGvTDirectorSubsystem::ClearRoundGhosts()
{
	for (AGvTGhostCharacterBase* Ghost : ActiveGhosts)
	{
		if (IsValid(Ghost))
		{
			Ghost->Destroy();
		}
	}

	ActiveGhosts.Reset();
}

bool UGvTDirectorSubsystem::BuildRuntimeConfig(
	const UGvTGhostModelData* ModelData,
	const UGvTGhostTypeData* TypeData,
	FGvTGhostRuntimeConfig& OutConfig) const
{
	if (!ModelData || !TypeData)
	{
		return false;
	}

	OutConfig.WalkSpeed = ModelData->BaseWalkSpeed * TypeData->WalkSpeedMultiplier;
	OutConfig.RunSpeed = ModelData->BaseRunSpeed * TypeData->RunSpeedMultiplier;
	OutConfig.Acceleration = ModelData->BaseAcceleration * TypeData->AccelerationMultiplier;
	OutConfig.BrakingDecel = ModelData->BaseBrakingDecel * TypeData->BrakingMultiplier;

	OutConfig.NoiseInterestMultiplier = TypeData->NoiseInterestMultiplier;
	OutConfig.ElectricalInterestMultiplier = TypeData->ElectricalInterestMultiplier;
	OutConfig.LightInterestMultiplier = TypeData->LightInterestMultiplier;
	OutConfig.ValuableItemInterestMultiplier = TypeData->ValuableItemInterestMultiplier;
	OutConfig.MedicalItemInterestMultiplier = TypeData->MedicalItemInterestMultiplier;

	OutConfig.HuntAggressionMultiplier = TypeData->HuntAggressionMultiplier;
	OutConfig.RoamChanceMultiplier = TypeData->RoamChanceMultiplier;
	OutConfig.ScareFrequencyMultiplier = TypeData->ScareFrequencyMultiplier;

	OutConfig.PanicBiasMultiplier = TypeData->PanicBiasMultiplier;
	OutConfig.IsolationBiasMultiplier = TypeData->IsolationBiasMultiplier;
	OutConfig.RecentHauntPressureBiasMultiplier = TypeData->RecentHauntPressureBiasMultiplier;
	OutConfig.DarknessBiasMultiplier = TypeData->DarknessBiasMultiplier;

	OutConfig.KillDistanceMultiplier = TypeData->KillDistanceMultiplier;
	OutConfig.MaxChaseDistanceMultiplier = TypeData->MaxChaseDistanceMultiplier;
	OutConfig.DoorOpenChance = TypeData->DoorOpenChance;

	OutConfig.AllowedScareTags = TypeData->AllowedScareTags;
	OutConfig.TypeTags = TypeData->TypeTags;

	return true;
}

AGvTGhostCharacterBase* UGvTDirectorSubsystem::GetPrimaryGhost() const
{
	for (AGvTGhostCharacterBase* Ghost : ActiveGhosts)
	{
		if (IsValid(Ghost))
		{
			return Ghost;
		}
	}

	return nullptr;
}

AGvTGhostCharacterBase* UGvTDirectorSubsystem::GetGhostById(FName GhostId) const
{
	if (GhostId.IsNone())
	{
		return nullptr;
	}

	for (AGvTGhostCharacterBase* Ghost : ActiveGhosts)
	{
		if (IsValid(Ghost) && Ghost->GetGhostId() == GhostId)
		{
			return Ghost;
		}
	}

	return nullptr;
}

const UGvTGhostModelData* UGvTDirectorSubsystem::GetPrimaryGhostModel() const
{
	if (const AGvTGhostCharacterBase* Ghost = GetPrimaryGhost())
	{
		return Ghost->GetGhostModelData();
	}

	return nullptr;
}

const UGvTGhostTypeData* UGvTDirectorSubsystem::GetPrimaryGhostType() const
{
	if (const AGvTGhostCharacterBase* Ghost = GetPrimaryGhost())
	{
		return Ghost->GetGhostTypeData();
	}

	return nullptr;
}

bool UGvTDirectorSubsystem::CanPrimaryGhostUseScareTag(FGameplayTag ScareTag) const
{
	if (const AGvTGhostCharacterBase* Ghost = GetPrimaryGhost())
	{
		return Ghost->CanUseScareTag(ScareTag);
	}

	// No active round ghost yet: allow for legacy fallback.
	return true;
}

void UGvTDirectorSubsystem::StartRoundGhostSetup()
{
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client)
	{
		return;
	}

	ClearRoundGhosts();
	InitializeRoundGhosts();

	if (AGvTGhostCharacterBase* Ghost = GetPrimaryGhost())
	{
		UE_LOG(LogTemp, Log,
			TEXT("[Director] PrimaryGhost=%s Model=%s Type=%s"),
			*GetNameSafe(Ghost),
			*GetNameSafe(Ghost->GetGhostModelData()),
			*GetNameSafe(Ghost->GetGhostTypeData()));
	}
}

void UGvTDirectorSubsystem::EndRoundGhostSetup()
{
	ClearRoundGhosts();
}

bool UGvTDirectorSubsystem::BuildRoundGhostSpawnSpec(int32 GhostIndex, FGvTGhostRoundSpawnSpec& OutSpec) const
{
	OutSpec = FGvTGhostRoundSpawnSpec();

	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	if (AvailableGhostModels.Num() == 0 || AvailableGhostTypes.Num() == 0)
	{
		return false;
	}

	UGvTGhostModelData* ModelData = AvailableGhostModels.IsValidIndex(GhostIndex)
		? AvailableGhostModels[GhostIndex]
		: AvailableGhostModels[FMath::RandRange(0, AvailableGhostModels.Num() - 1)];

	UGvTGhostTypeData* TypeData = AvailableGhostTypes.IsValidIndex(GhostIndex)
		? AvailableGhostTypes[GhostIndex]
		: AvailableGhostTypes[FMath::RandRange(0, AvailableGhostTypes.Num() - 1)];

	if (!ModelData || !TypeData || !ModelData->GhostActorClass)
	{
		return false;
	}

	TArray<AActor*> FoundActors;
	UGameplayStatics::GetAllActorsOfClass(World, AGvTGhostSpawnPoint::StaticClass(), FoundActors);

	TArray<AGvTGhostSpawnPoint*> ValidSpawnPoints;
	for (AActor* Actor : FoundActors)
	{
		AGvTGhostSpawnPoint* SpawnPoint = Cast<AGvTGhostSpawnPoint>(Actor);
		if (!SpawnPoint)
		{
			continue;
		}

		ValidSpawnPoints.Add(SpawnPoint);
	}

	if (ValidSpawnPoints.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Director] No ghost spawn points found in world."));
		return false;
	}

	AGvTGhostSpawnPoint* ChosenSpawn = ValidSpawnPoints[FMath::RandRange(0, ValidSpawnPoints.Num() - 1)];
	if (!ChosenSpawn)
	{
		return false;
	}

	OutSpec.GhostId = FName(*FString::Printf(TEXT("Ghost_%02d"), GhostIndex + 1));
	OutSpec.ModelData = ModelData;
	OutSpec.TypeData = TypeData;
	OutSpec.SpawnTransform = ChosenSpawn->GetActorTransform();

	return true;
}

void UGvTDirectorSubsystem::ConfigureRoundGhostPool(
	const TArray<UGvTGhostModelData*>& InModels,
	const TArray<UGvTGhostTypeData*>& InTypes,
	int32 InNumGhosts)
{
	AvailableGhostModels.Reset();
	for (UGvTGhostModelData* Model : InModels)
	{
		if (IsValid(Model))
		{
			AvailableGhostModels.Add(Model);
		}
	}

	AvailableGhostTypes.Reset();
	for (UGvTGhostTypeData* Type : InTypes)
	{
		if (IsValid(Type))
		{
			AvailableGhostTypes.Add(Type);
		}
	}

	NumGhostsThisRound = FMath::Max(1, InNumGhosts);

	UE_LOG(LogTemp, Log,
		TEXT("[Director] ConfigureRoundGhostPool Models=%d Types=%d NumGhosts=%d"),
		AvailableGhostModels.Num(),
		AvailableGhostTypes.Num(),
		NumGhostsThisRound);
}

bool UGvTDirectorSubsystem::DispatchGhostHunt(const FGvTGhostHuntRequest& Request)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	AGvTGhostCharacterBase* Ghost = GetPrimaryGhost();
	if (!Ghost)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Director] DispatchGhostHunt failed: no primary ghost."));
		return false;
	}

	APawn* TargetPawn = Cast<APawn>(Request.TargetActor);
	if (!TargetPawn)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Director] DispatchGhostHunt failed: invalid target."));
		return false;
	}

	UE_LOG(LogTemp, Warning,
		TEXT("[Director] DispatchGhostHunt Ghost=%s Target=%s Intensity=%.2f MaxGhosts=%d Multi=%d"),
		*GetNameSafe(Ghost),
		*GetNameSafe(TargetPawn),
		Request.Intensity01,
		Request.MaxGhostsToDispatch,
		Request.bAllowMultipleGhosts ? 1 : 0);

	Ghost->BeginGhostChase(TargetPawn);
	RememberTarget(TargetPawn);
	LastGlobalHauntTime = World->GetTimeSeconds();

	return true;
}

bool UGvTDirectorSubsystem::DispatchGhostEvent(const FGvTGhostEventRequest& Request)
{
	AActor* Target = Request.TargetActor;
	if (!Target)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Director] DispatchGhostEvent failed: no target."));
		return false;
	}

	FGvTScareEvent Event;

	if (Request.RequestedEventTag.MatchesTagExact(GvTScareTags::Mirror()))
	{
		Event = Request.SourceActor ? MakeMirrorEventForActor(Target, Request.SourceActor) : MakeMirrorEvent(Target);
	}
	else if (Request.RequestedEventTag.MatchesTagExact(GvTScareTags::GhostScare()))
	{
		Event = MakeGhostScareEvent(Target);
	}
	else if (Request.RequestedEventTag.MatchesTagExact(GvTScareTags::GhostChase()))
	{
		Event = MakeGhostChaseEvent(Target);
	}
	else if (Request.RequestedEventTag.MatchesTagExact(GvTScareTags::LightChase()))
	{
		Event = MakeLightChaseEvent(Target);
	}
	else if (Request.RequestedEventTag.MatchesTagExact(GvTScareTags::RearAudioSting()))
	{
		Event = MakeRearAudioStingEvent(Target);
	}
	else if (Request.RequestedEventTag.MatchesTagExact(GvTScareTags::GhostScream()))
	{
		Event = MakeGhostScreamEvent(Target);
	}
	else if (Request.RequestedEventTag.MatchesTagExact(GvTScareTags::DoorSlamBehind()))
	{
		Event = MakeDoorSlamBehindEvent(Target, Request.SourceActor);
	}
	else
	{
		Event.TargetActor = Target;
		Event.ScareTag = Request.RequestedEventTag;
		Event.Intensity01 = Request.Intensity01;
		Event.Duration = 1.5f;
		Event.PanicAmount = 10.f;
		Event.bAffectsPanic = true;
	}

	if (Request.Intensity01 > 0.f)
	{
		Event.Intensity01 = Request.Intensity01;
	}
	if (Request.SourceActor && !Event.SourceActor)
	{
		Event.SourceActor = Request.SourceActor;
	}
	if (!Event.TargetActor)
	{
		Event.TargetActor = Target;
	}
	if (!Request.WorldLocation.IsNearlyZero())
	{
		Event.WorldHint = Request.WorldLocation;
	}
	else if (Event.WorldHint.IsNearlyZero())
	{
		Event.WorldHint = Target->GetActorLocation();
	}

	UE_LOG(LogTemp, Warning,
		TEXT("[Director] DispatchGhostEvent Tag=%s Target=%s Intensity=%.2f Panic=%.1f Duration=%.2f"),
		*Event.ScareTag.ToString(),
		*GetNameSafe(Target),
		Event.Intensity01,
		Event.PanicAmount,
		Event.Duration);

	return DispatchScareEvent(Event);
}

AGvTMirrorActor* UGvTDirectorSubsystem::ChooseMirrorForTarget(APawn* TargetPawn) const
{
	if (!TargetPawn || !GetWorld())
	{
		return nullptr;
	}

	TArray<AActor*> FoundActors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AGvTMirrorActor::StaticClass(), FoundActors);

	AGvTMirrorActor* BestMirror = nullptr;
	float BestScore = -FLT_MAX;

	for (AActor* Actor : FoundActors)
	{
		AGvTMirrorActor* Mirror = Cast<AGvTMirrorActor>(Actor);
		if (!Mirror)
		{
			continue;
		}

		if (!IsMirrorValidForTarget(Mirror, TargetPawn))
		{
			continue;
		}

		const float Score = ScoreMirrorForTarget(Mirror, TargetPawn);
		if (Score > BestScore)
		{
			BestScore = Score;
			BestMirror = Mirror;
		}
	}

	UE_LOG(LogTemp, Warning,
		TEXT("[Director] ChooseMirrorForTarget Target=%s Chosen=%s Score=%.2f"),
		*GetNameSafe(TargetPawn),
		*GetNameSafe(BestMirror),
		BestScore);

	return BestMirror;
}

bool UGvTDirectorSubsystem::IsMirrorValidForTarget(const AGvTMirrorActor* Mirror, const APawn* TargetPawn) const
{
	if (!Mirror || !TargetPawn)
	{
		return false;
	}

	const float DistSq = FVector::DistSquared(Mirror->GetActorLocation(), TargetPawn->GetActorLocation());
	if (DistSq > FMath::Square(MirrorMaxDistance))
	{
		return false;
	}

	return true;
}

float UGvTDirectorSubsystem::ScoreMirrorForTarget(const AGvTMirrorActor* Mirror, const APawn* TargetPawn) const
{
	if (!Mirror || !TargetPawn)
	{
		return -FLT_MAX;
	}

	const FVector ToMirror = (Mirror->GetActorLocation() - TargetPawn->GetActorLocation());
	const float Distance = ToMirror.Size();
	const FVector ToMirrorDir = ToMirror.GetSafeNormal();

	const FVector Forward = TargetPawn->GetActorForwardVector();
	const float FacingDot = FVector::DotProduct(Forward, ToMirrorDir);

	const float DistanceScore = 1.f - FMath::Clamp(Distance / MirrorMaxDistance, 0.f, 1.f);
	const float FacingScore = FMath::GetMappedRangeValueClamped(
		FVector2D(-1.f, 1.f),
		FVector2D(0.f, 1.f),
		FacingDot);

	float Score = 0.f;
	Score += DistanceScore * 0.7f;
	Score += FacingScore * 0.3f;

	return Score;
}
