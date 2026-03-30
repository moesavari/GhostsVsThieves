#include "Systems/Director/GvTDirectorSubsystem.h"
#include "Gameplay/Scare/UGvTScareComponent.h"
#include "Gameplay/Scare/GvTScareTags.h"
#include "Gameplay/Characters/Thieves/GvTThiefCharacter.h"
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
#include "GvTPlayerState.h"
#include "EngineUtils.h"
#include "Systems/GvTPowerBoxActor.h"

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
	if (ScareTag.MatchesTagExact(GvTScareTags::CrawlerOverhead()))
	{
		return EGvTPanicSource::CrawlerOverhead;
	}
	if (ScareTag.MatchesTagExact(GvTScareTags::CrawlerChase()))
	{
		return EGvTPanicSource::CrawlerChaseStart;
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
	if (ScareTag.MatchesTagExact(GvTScareTags::CrawlerChase()))
	{
		return 0.35f;
	}
	if (ScareTag.MatchesTagExact(GvTScareTags::CrawlerOverhead()))
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
		Event = MakeCrawlerOverheadEvent(Target);
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

		const bool bSlammed = Door->TriggerScareSlam();
		if (!bSlammed)
		{
			UE_LOG(LogTemp, Warning, TEXT("[Director] DoorSlamBehind failed: %s was not slam-eligible."), *GetNameSafe(Door));
			return false;
		}

		// Panic only if the target is actually close enough to the slammed door.
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
			(Event.bAffectsPanic && Event.PanicAmount > 0.f && !Event.ScareTag.MatchesTagExact(GvTScareTags::LightChase()))
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
		UE_LOG(LogTemp, Warning, TEXT("[Director] Dispatch Mirror to %s"), *GetNameSafe(Target));
		ScareComp->RequestMirrorScare(Event.Intensity01, Event.Duration > 0.f ? Event.Duration : 1.5f);
		return true;
	}

	if (Event.ScareTag.MatchesTagExact(GvTScareTags::CrawlerChase()))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Director] Dispatch Crawler Chase to %s"), *GetNameSafe(Target));
		ScareComp->RequestCrawlerChaseFromEvent(Target);
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

	if (Event.ScareTag.MatchesTagExact(GvTScareTags::CrawlerOverhead()))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[Director] Dispatch CrawlerOverhead to %s NetMode=%s LocalRole=%d RemoteRole=%d"),
			*GetNameSafe(Target),
			GvTNetModeToString(Target->GetNetMode()),
			(int32)Target->GetLocalRole(),
			(int32)Target->GetRemoteRole());

		ScareComp->RequestCrawlerOverheadFromEvent(Event);
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
	Event.TargetActor = TargetPawn;
	Event.ScareTag = ScareTag;
	Event.SourceActor = SourceActor;
	Event.WorldHint = TargetPawn->GetActorLocation();
	Event.PanicAmount = 10.f;
	Event.Intensity01 = 1.0f;
	Event.Duration = 1.5f;
	Event.bAffectsPanic = true;
	Event.bTriggerLocalFlicker = false;

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
		TargetScareComp->Debug_RequestLocalHouseLightFlicker(
			FMath::Clamp(Event.Intensity01, 0.f, 1.f),
			Event.Duration > 0.f ? Event.Duration : 1.5f
		);
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
	return Event;
}

FGvTScareEvent UGvTDirectorSubsystem::MakeCrawlerOverheadEvent(AActor* Target) const
{
	FGvTScareEvent Event;
	Event.ScareTag = GvTScareTags::CrawlerOverhead();
	Event.TargetActor = Target;
	Event.Intensity01 = 1.0f;
	Event.Duration = CrawlerOverheadDuration;
	Event.bTriggerLocalFlicker = bCrawlerOverheadTriggersLocalFlicker;
	Event.bTriggerGroupFlicker = false;
	Event.bAffectsPanic = true;
	Event.PanicAmount = CrawlerOverheadPanicAmount;
	return Event;
}

FGvTScareEvent UGvTDirectorSubsystem::MakeCrawlerChaseEvent(AActor* Target) const
{
	FGvTScareEvent Event;
	Event.ScareTag = GvTScareTags::CrawlerChase();
	Event.TargetActor = Target;
	Event.Intensity01 = 1.0f;
	Event.Duration = CrawlerChaseDuration;
	Event.bTriggerLocalFlicker = false;
	Event.bTriggerGroupFlicker = bCrawlerChaseTriggersGroupFlicker;
	Event.bAffectsPanic = true;
	Event.PanicAmount = CrawlerChasePanicAmount;
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

	const float Panic01 = FMath::Clamp(PS->GetPanic01(), 0.f, 1.f);
	const float Pressure01 = FMath::Clamp(PS->GetRecentHauntPressure01(), 0.f, 1.f);
	const float Isolation01 = ComputeIsolationScore(Pawn);
	const float Noise01 = 0.f;
	const float RecentTargetPenalty01 = ComputeRecentTargetPenalty01(Pawn);
	const float BusyPenalty01 = ScareComp->IsScareBusy() ? 1.f : 0.f;

	float Score = BaseTargetScore;
	Score += Panic01 * PanicTargetWeight;
	Score += Isolation01 * IsolationTargetWeight;
	Score += Noise01 * NearbyNoiseTargetWeight;
	Score -= Pressure01 * HauntPressurePenaltyWeight;
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

	if (Event.ScareTag == GvTScareTags::CrawlerOverhead())
	{
		return CrawlerOverheadDispatchTensionImpulse;
	}

	if (Event.ScareTag == GvTScareTags::CrawlerChase())
	{
		return CrawlerChaseDispatchTensionImpulse;
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

	AGvTDoorActor* BestDoor = nullptr;
	float BestScore = -1.f;

	for (AActor* Actor : FoundDoors)
	{
		AGvTDoorActor* Door = Cast<AGvTDoorActor>(Actor);
		if (!Door)
		{
			continue;
		}

		const float Score = ScoreDoorForSlam(TargetPawn, Door);
		if (Score > BestScore)
		{
			BestScore = Score;
			BestDoor = Door;
		}
	}

	return BestDoor;
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

	const float Panic = PS->GetPanic01();
	const float Pressure = PS->GetRecentHauntPressure01();

	bool bIsElectrical = false;
	bool bIsValuable = false;
	bool bIsNoisy = false;
	float ItemValue01 = 0.f;

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

	float ReactionChance = 0.10f;
	ReactionChance += Panic * 0.35f;
	ReactionChance += Pressure * 0.25f;

	if (bIsElectrical) ReactionChance += 0.35f;
	if (bIsValuable)   ReactionChance += 0.20f;
	if (bIsNoisy)      ReactionChance += 0.25f;

	ReactionChance = FMath::Clamp(ReactionChance, 0.f, 0.95f);

	if (FMath::FRand() > ReactionChance)
	{
		return;
	}

	TriggerInteractionReaction(Pawn, TargetActor, bIsElectrical, bIsValuable, bIsNoisy, ItemValue01);
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

	AGvTPlayerState* PS = Pawn->GetPlayerState<AGvTPlayerState>();
	if (!PS)
	{
		return;
	}

	const float Panic = PS->GetPanic01();
	FGameplayTag ChosenScare;

	if (bIsElectrical)
	{
		const float Roll = FMath::FRand();

		if (Roll < 0.7f)
		{
			if (AGvTPowerBoxActor* Power = FindPowerBoxInWorld())
			{
				Power->ForcePowerStateFromGhost(EGvTHousePowerState::Off);
			}

			ChosenScare = GvTScareTags::LightChase();
		}
		else
		{
			ChosenScare = GvTScareTags::GhostScream();
		}
	}
	else if (bIsValuable)
	{
		const float Roll = FMath::FRand();

		if (Roll < 0.5f)
		{
			ChosenScare = GvTScareTags::GhostScream();
		}
		else
		{
			ChosenScare = GvTScareTags::RearAudioSting();
		}
	}
	else if (bIsNoisy)
	{
		if (Panic > 0.5f)
		{
			ChosenScare = GvTScareTags::CrawlerChase();
		}
		else
		{
			ChosenScare = GvTScareTags::RearAudioSting();
		}
	}
	else
	{
		const float Roll = FMath::FRand();

		if (Panic < 0.3f)
		{
			ChosenScare = (Roll < 0.5f)
				? GvTScareTags::RearAudioSting()
				: GvTScareTags::LightChase();
		}
		else if (Panic < 0.7f)
		{
			if (Roll < 0.4f)
			{
				ChosenScare = GvTScareTags::Mirror();
			}
			else if (Roll < 0.8f)
			{
				ChosenScare = GvTScareTags::GhostScream();
			}
			else
			{
				ChosenScare = GvTScareTags::LightChase();
			}
		}
		else
		{
			ChosenScare = (Roll < 0.5f)
				? GvTScareTags::CrawlerOverhead()
				: GvTScareTags::CrawlerChase();
		}
	}

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