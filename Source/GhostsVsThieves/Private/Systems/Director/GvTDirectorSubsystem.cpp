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
#include "EngineUtils.h"
#include "UObject/UObjectGlobals.h"
#include "Systems/World/GvTHouseBoundsLibrary.h"
#include "NavigationSystem.h"
#include "Systems/GvTPowerBoxActor.h"
#include "Systems/Light/GvTLightFlickerSubsystem.h"
#include "Gameplay/Ghosts/GvTGhostCharacterBase.h"
#include "Gameplay/Ghosts/GvTHauntGhostBase.h"
#include "Gameplay/Ghosts/GvTGhostSpawnPoint.h"
#include "Gameplay/Ghosts/GvTGhostModelData.h"
#include "Gameplay/Ghosts/GvTGhostTypeData.h"
#include "Gameplay/Ghosts/GvTEventGhostBase.h"
#include "World/Items/GvTInteractableItem.h"
#include "Gameplay/Characters/Thieves/GvTThiefPerceptionComponent.h"
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
	if (ScareTag.MatchesTagExact(GvTScareTags::RearAudioSting()) ||
		ScareTag.MatchesTagExact(GvTScareTags::GhostScare_AudioRear()))
	{
		return EGvTPanicSource::RearAudioSting;
	}
	if (ScareTag.MatchesTagExact(GvTScareTags::GhostScream()) ||
		ScareTag.MatchesTagExact(GvTScareTags::GhostScare_Scream()))
	{
		return EGvTPanicSource::GhostScream;
	}
	if (ScareTag.MatchesTagExact(GvTScareTags::DoorSlamBehind()))
	{
		return EGvTPanicSource::DoorSlam;
	}
	if (ScareTag.MatchesTagExact(GvTScareTags::GhostScare_Close()))
	{
		return EGvTPanicSource::GhostScare;
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
	if (ScareTag.MatchesTagExact(GvTScareTags::RearAudioSting()) ||
		ScareTag.MatchesTagExact(GvTScareTags::GhostScare_AudioRear()))
	{
		return 0.10f;
	}
	if (ScareTag.MatchesTagExact(GvTScareTags::GhostScream()) ||
		ScareTag.MatchesTagExact(GvTScareTags::GhostScare_Scream()))
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

	PostLoadMapHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(
		this,
		&UGvTDirectorSubsystem::HandlePostLoadMap);

	ResetTransientMatchState(GetWorld());

	UE_LOG(LogTemp, Log, TEXT("GvT Director Subsystem Initialized"));

	if (bEnableAutoHaunts)
	{
		StartDirector();
	}
}

void UGvTDirectorSubsystem::Deinitialize()
{
	if (PostLoadMapHandle.IsValid())
	{
		FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapHandle);
		PostLoadMapHandle.Reset();
	}

	StopDirector();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TimerHandle_TheftReaction);
		World->GetTimerManager().ClearTimer(TimerHandle_ObjectiveCursedHaunt);
	}

	Super::Deinitialize();
	UE_LOG(LogTemp, Log, TEXT("GvT Director Subsystem Deinitialized"));
}

void UGvTDirectorSubsystem::HandlePostLoadMap(UWorld* LoadedWorld)
{
	if (!LoadedWorld || LoadedWorld->GetGameInstance() != GetGameInstance())
	{
		return;
	}

	ResetTransientMatchState(LoadedWorld);

	if (bEnableAutoHaunts)
	{
		StartDirector();
	}

	UE_LOG(LogTemp, Log, TEXT("[DirectorLifecycle] Reset transient Director state for map=%s"), *GetNameSafe(LoadedWorld));
}

void UGvTDirectorSubsystem::ResetTransientMatchState(UWorld* LoadedWorld)
{
	if (LoadedWorld)
	{
		LoadedWorld->GetTimerManager().ClearTimer(TimerHandle_DirectorTick);
		LoadedWorld->GetTimerManager().ClearTimer(TimerHandle_TheftReaction);
		LoadedWorld->GetTimerManager().ClearTimer(TimerHandle_ObjectiveCursedHaunt);
	}

	TimerHandle_DirectorTick.Invalidate();
	TimerHandle_TheftReaction.Invalidate();
	TimerHandle_ObjectiveCursedHaunt.Invalidate();

	Heat = 0.f;
	HouseActivity01 = 0.f;
	TheftActivity01 = 0.f;
	TimeActivity01 = 0.f;
	HouseTension01 = 0.f;
	bHouseActivityStarted = false;

	LastGlobalHauntTime = -1000.f;
	LastHauntEndTime = -1000.f;
	LastLightingResponseTime = -1000.f;
	bHauntSpawnInProgress = false;
	bWasHauntActiveLastTick = false;

	LastTargetedTimeSeconds.Reset();
	LastScareDispatchTimes.Reset();
	ActiveHauntGhostByTarget.Reset();
	LastManualHauntRequestTimeByTarget.Reset();

	PendingTheftPawn.Reset();
	PendingTheftSource.Reset();
	bPendingTheftElectrical = false;
	bPendingTheftValuable = false;
	bPendingTheftNoisy = false;
	PendingTheftValue01 = 0.f;
	PendingTheftCount = 0;
	PendingObjectiveCarrier.Reset();
	PendingObjectiveItem.Reset();
	bObjectiveHasTriggeredCursedHaunt = false;
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
	UpdateHouseActivity(DirectorTickInterval);

	const float Now = World->GetTimeSeconds();
	const bool bHauntActiveNow = IsAnyHauntActive();
	if (bWasHauntActiveLastTick && !bHauntActiveNow)
	{
		LastHauntEndTime = Now;
		UE_LOG(LogTemp, Log, TEXT("[DirectorRecovery] Haunt ended. Ordinary haunts blocked for %.1f seconds."), PostHauntRecoveryDuration);
	}
	bWasHauntActiveLastTick = bHauntActiveNow;
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
	const bool bHauntActive = IsAnyHauntActive();
	const bool bPostHauntRecovery = IsInPostHauntRecovery();

	APawn* TargetPawn = Cast<APawn>(ChooseBestTarget());
	if (!IsPawnEligibleForDirector(TargetPawn))
	{
		return false;
	}

	const float Panic01 = GetPanicForPawn(TargetPawn);

	const bool bHauntUnlocked = Panic01 >= HauntChasePanicThreshold01;
	if (!bHauntActive && !bPostHauntRecovery && bHauntUnlocked)
	{
		AActor* ChaseTarget = TargetPawn;
		if (FMath::FRand() <= GhostScreamHighestPanicBiasChance)
		{
			if (AActor* HighestPanicTarget = ChooseHighestPanicTarget())
			{
				APawn* HighestPanicPawn = Cast<APawn>(HighestPanicTarget);

				if (IsPawnEligibleForDirector(HighestPanicPawn) &&
					GetPanicForPawn(HighestPanicPawn) >= HauntChasePanicThreshold01)
				{
					ChaseTarget = HighestPanicPawn;
				}
			}
		}

		const float PanicAlpha = FMath::Clamp((Panic01 - HauntChasePanicThreshold01) / FMath::Max(0.01f, 1.0f - HauntChasePanicThreshold01), 0.0f, 1.0f);
		const float ChaseChance = FMath::Lerp(HauntChaseChanceAtThreshold01, HauntChaseChanceAtMaxPanic01, PanicAlpha);
		const float ChaseRoll = FMath::FRand();

		if (ChaseRoll <= ChaseChance)
		{
			const FGvTScareEvent ChaseEvent = MakeCrawlerChaseEvent(ChaseTarget);
			const bool bDispatched = DispatchScareEvent(ChaseEvent);

			if (bDispatched && GetWorld())
			{
				if (APawn* RememberPawn = Cast<APawn>(ChaseEvent.TargetActor))
				{
					RememberTarget(RememberPawn);
				}

				LastGlobalHauntTime = GetWorld()->GetTimeSeconds();
			}

			if(bDispatched)
				ApplyHouseTensionImpulse(GetDispatchTensionImpulse(ChaseEvent));

			UE_LOG(LogTemp, Log,
				TEXT("[DirectorPanicHaunt] ForcedPriorityChase Target=%s Panic=%.2f Chance=%.2f Roll=%.2f Dispatched=%d"),
				*GetNameSafe(ChaseEvent.TargetActor),
				Panic01,
				ChaseChance,
				ChaseRoll,
				bDispatched ? 1 : 0);

			return bDispatched;
		}
	}

	TArray<FGvTScareEvent> EligibleEvents;
	EligibleEvents.Reserve(12);

	if (Panic01 >= GhostScareMinPanicThreshold01 || HouseActivity01 >= GhostScareActivityUnlock01)
	{
		EligibleEvents.Add(MakeRearAudioStingEvent(TargetPawn));
		EligibleEvents.Add(MakeGhostScreamEvent(TargetPawn));

		if (!bHauntActive && !bPostHauntRecovery && FMath::FRand() <= CloseGhostScareSelectionChance)
		{
			EligibleEvents.Add(MakeCloseGhostScareEvent(TargetPawn));
		}
	}

	EligibleEvents.Add(MakeLightChaseEvent(TargetPawn));

	if (AGvTDoorActor* Door = ChooseBestDoorSlamTarget(TargetPawn))
	{
		EligibleEvents.Add(MakeDoorSlamBehindEvent(TargetPawn, Door));
	}

	if (Panic01 >= MirrorEventPanicThreshold01)
	{
		const float MirrorAlpha = FMath::Clamp((Panic01 - MirrorEventPanicThreshold01) / FMath::Max(0.01f, 1.0f - MirrorEventPanicThreshold01), 0.0f, 1.0f);
		const float MirrorChance = FMath::Lerp(MirrorChanceAtThreshold01, MirrorChanceAtMaxPanic01, MirrorAlpha);
		const int32 MirrorWeight = FMath::Clamp(FMath::RoundToInt(MirrorChance * 8.0f), 1, 6);

		for (int32 WeightIndex = 0; WeightIndex < MirrorWeight; ++WeightIndex)
		{
			EligibleEvents.Add(MakeMirrorEvent(TargetPawn));
		}
	}

	// If the priority chase roll missed, still keep haunt/chase in the fallback pool above 60%.
	// This prevents a failed priority roll from making high panic feel strangely safe.
	if (!bHauntActive && !bPostHauntRecovery && bHauntUnlocked)
	{
		EligibleEvents.Add(MakeCrawlerChaseEvent(TargetPawn));
	}

	if (bPostHauntRecovery && bLogHouseTension)
	{
		const float Remaining = FMath::Max(0.0f, PostHauntRecoveryDuration - (GetWorld()->GetTimeSeconds() - LastHauntEndTime));
		UE_LOG(LogTemp, Log, TEXT("[DirectorRecovery] Environmental pressure only. Remaining=%.1f Panic=%.2f Activity=%.2f"), Remaining, Panic01, HouseActivity01);
	}

	if (EligibleEvents.Num() == 0)
	{
		UE_LOG(LogTemp, Log, TEXT("[DirectorThreshold] No eligible auto scares for Target=%s Panic=%.2f"), *GetNameSafe(TargetPawn), Panic01);
		return false;
	}

	while (!EligibleEvents.IsEmpty())
	{
		const int32 Index = FMath::RandRange(0, EligibleEvents.Num() - 1);
		const FGvTScareEvent Event = EligibleEvents[Index];
		EligibleEvents.RemoveAtSwap(Index);

		const bool bDispatched = DispatchScareEvent(Event);
		UE_LOG(LogTemp, Log, TEXT("[DirectorThreshold] AutoScare Target=%s Tag=%s Panic=%.2f Activity=%.2f Dispatched=%d"), *GetNameSafe(Event.TargetActor), *Event.ScareTag.ToString(), Panic01, HouseActivity01, bDispatched ? 1 : 0);

		if (!bDispatched)
		{
			continue;
		}

		if (APawn* RememberPawn = Cast<APawn>(Event.TargetActor))
		{
			RememberTarget(RememberPawn);
		}

		LastGlobalHauntTime = GetWorld()->GetTimeSeconds();
		ApplyHouseTensionImpulse(GetDispatchTensionImpulse(Event));
		return true;
	}

	return false;
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
		if (!IsPawnEligibleForDirector(Pawn))
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
		if (!IsPawnEligibleForDirector(Pawn))
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

void UGvTDirectorSubsystem::ApplyPanicEventToPlayers(const FGvTPanicEvent& PanicEvent, AActor* ExcludedActor) const
{
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client)
	{
		return;
	}

	TArray<AActor*> Thieves;
	UGameplayStatics::GetAllActorsOfClass(World, AGvTThiefCharacter::StaticClass(), Thieves);

	for (AActor* Actor : Thieves)
	{
		APawn* Pawn = Cast<APawn>(Actor);
		if (!IsPawnEligibleForDirector(Pawn) || Pawn == ExcludedActor)
		{
			continue;
		}

		AGvTPlayerState* PlayerState = Pawn->GetPlayerState<AGvTPlayerState>();
		if (!PlayerState)
		{
			continue;
		}

		PlayerState->ApplyPanicEventAuthority(PanicEvent);
	}
}

void UGvTDirectorSubsystem::ApplyDoorSlamPanicToNearbyPlayers(AGvTDoorActor* Door, AActor* InstigatorActor, bool bSlamSucceeded) const
{
	if (!IsValid(Door))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[DoorSlamPanic] Skipped: invalid door."));

		return;
	}

	if (!bSlamSucceeded)
	{
		UE_LOG(LogTemp, Verbose,
			TEXT("[DoorSlamPanic] Skipped: door did not successfully slam. Door=%s"),
			*GetNameSafe(Door));

		return;
	}

	FGvTPanicEvent PanicEvent;
	PanicEvent.Source = EGvTPanicSource::DoorSlam;
	PanicEvent.PanicDelta01 = ScalePanicAmount(DoorSlamBehindPanicAmount) / 100.f;

	PanicEvent.HauntPressureDelta01 = FMath::Clamp(DoorSlamPressureAmount01, 0.f, 1.f);

	PanicEvent.SourceActor = Door;
	PanicEvent.InstigatorActor = InstigatorActor;
	PanicEvent.WorldLocation = Door->GetActorLocation();

	PanicEvent.SourceRadius = FMath::Max(0.f, DoorSlamPanicRadius);

	PanicEvent.bRequiresProximity = true;
	PanicEvent.bRequiresSuccessfulExecution = true;
	PanicEvent.bExecutionSucceeded = true;

	PanicEvent.CooldownSeconds = FMath::Max(0.f, DoorSlamPanicCooldown);

	ApplyPanicEventToPlayers(PanicEvent);

	UE_LOG(LogTemp, Warning,
		TEXT("[DoorSlamPanic] Applied proximity event. Door=%s Instigator=%s Radius=%.1f Panic=%.2f Pressure=%.2f"),
		*GetNameSafe(Door),
		*GetNameSafe(InstigatorActor),
		PanicEvent.SourceRadius,
		PanicEvent.PanicDelta01,
		PanicEvent.HauntPressureDelta01);
}

void UGvTDirectorSubsystem::ApplyGlobalScarePanicToOtherPlayers(const FGvTScareEvent& Event, AActor* PrimaryTarget) const
{
	const bool bGhostScream = Event.ScareTag.MatchesTagExact(GvTScareTags::GhostScream()) ||
		Event.ScareTag.MatchesTagExact(GvTScareTags::GhostScare_Scream());
	const bool bLightChase = Event.ScareTag.MatchesTagExact(GvTScareTags::LightChase());
	const bool bGlobalPresentation = Event.bTriggerGroupFlicker || bGhostScream || bLightChase;

	if (!bGlobalPresentation || !Event.bAffectsPanic || Event.PanicAmount <= 0.f)
	{
		return;
	}

	FGvTPanicEvent PanicEvent;
	PanicEvent.Source = GvTMapScareTagToPanicSource(Event.ScareTag);
	PanicEvent.PanicDelta01 = (Event.PanicAmount / 100.f) * GetRecoveryPanicMultiplier(Event.ScareTag);
	PanicEvent.HauntPressureDelta01 = GvTGetPressureGain01ForScareTag(Event.ScareTag, Event.bTriggerLocalFlicker);
	PanicEvent.SourceActor = Event.SourceActor;
	PanicEvent.InstigatorActor = PrimaryTarget;
	PanicEvent.WorldLocation = !Event.WorldHint.IsNearlyZero()
		? Event.WorldHint
		: (PrimaryTarget ? PrimaryTarget->GetActorLocation() : FVector::ZeroVector);
	PanicEvent.bRequiresSuccessfulExecution = true;
	PanicEvent.bExecutionSucceeded = true;

	// A scream is shared only with players who can actually hear it.
	if (bGhostScream && Event.SharedAudioRadius > 0.f)
	{
		PanicEvent.bRequiresProximity = true;
		PanicEvent.SourceRadius = Event.SharedAudioRadius;
	}

	ApplyPanicEventToPlayers(PanicEvent, PrimaryTarget);
}

void UGvTDirectorSubsystem::ApplyHauntStartPanicToAllPlayers(AActor* HauntSource, AActor* InstigatorActor) const
{
	if (HauntStartPanicAmount <= 0.f && HauntStartPressureAmount01 <= 0.f)
	{
		return;
	}

	FGvTPanicEvent PanicEvent;
	PanicEvent.Source = EGvTPanicSource::GhostHauntStart;
	PanicEvent.PanicDelta01 = ScalePanicAmount(HauntStartPanicAmount) / 100.f;
	PanicEvent.HauntPressureDelta01 = HauntStartPressureAmount01;
	PanicEvent.SourceActor = HauntSource;
	PanicEvent.InstigatorActor = InstigatorActor;
	PanicEvent.WorldLocation = HauntSource ? HauntSource->GetActorLocation() : FVector::ZeroVector;
	PanicEvent.bRequiresProximity = false;
	PanicEvent.bRequiresSuccessfulExecution = true;
	PanicEvent.bExecutionSucceeded = IsValid(HauntSource);
	PanicEvent.CooldownSeconds = 0.f;

	ApplyPanicEventToPlayers(PanicEvent);
}

bool UGvTDirectorSubsystem::DispatchScareEvent(const FGvTScareEvent& Event)
{
	AActor* Target = Event.TargetActor;
	if (!IsValid(Target))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Director] Dispatch failed: TargetActor is null."));
		return false;
	}

	if (APawn* TargetPawn = Cast<APawn>(Target))
	{
		if (!IsPawnEligibleForDirector(TargetPawn))
		{
			UE_LOG(LogTemp, Log,
				TEXT("[Director] Dispatch skipped: target is dead or ineligible. Target=%s Tag=%s"),
				*GetNameSafe(TargetPawn),
				*Event.ScareTag.ToString());

			return false;
		}
	}

	const bool bIsMirrorEvent = Event.ScareTag.MatchesTagExact(GvTScareTags::Mirror()) || Event.ScareTag.MatchesTagExact(GvTScareTags::GhostEvent_Mirror());
	const bool bIsCloseGhostScare = Event.ScareTag.MatchesTagExact(GvTScareTags::GhostScare_Close());
	const bool bIsHauntEvent =
		Event.ScareTag.MatchesTagExact(GvTScareTags::CrawlerChase()) ||
		Event.ScareTag.MatchesTagExact(GvTScareTags::GhostHaunt_Chase());

	if (bIsCloseGhostScare && IsAnyHauntActive())
	{
		UE_LOG(LogTemp, Log, TEXT("[DirectorHauntLifecycle] Close scare blocked during active haunt. Target=%s"), *GetNameSafe(Target));
		return false;
	}

	if (bIsCloseGhostScare && IsInPostHauntRecovery())
	{
		UE_LOG(LogTemp, Log, TEXT("[DirectorRecovery] Close scare blocked for full post-haunt recovery. Target=%s Remaining=%.1f"), *GetNameSafe(Target), FMath::Max(0.f, PostHauntRecoveryDuration - GetPostHauntRecoveryElapsed()));
		return false;
	}

	const bool bForcedHaunt = bIsHauntEvent && Event.bIgnorePanicThreshold;
	if (!bForcedHaunt && IsScareTagOnCooldown(Event.ScareTag))
	{
		UE_LOG(LogTemp, Log, TEXT("[DirectorCooldown] Per-scare cooldown blocked Tag=%s Target=%s"), *Event.ScareTag.ToString(), *GetNameSafe(Target));
		return false;
	}

	if (IsInPostHauntRecovery() && GetPostHauntRecoveryElapsed() < PostHauntStrongScareBlockDuration && IsStrongRecoveryScare(Event.ScareTag))
	{
		UE_LOG(LogTemp, Log, TEXT("[DirectorRecovery] Strong scare blocked during early recovery. Tag=%s Elapsed=%.1f"), *Event.ScareTag.ToString(), GetPostHauntRecoveryElapsed());
		return false;
	}

	if (!Event.bIgnorePanicThreshold || bIsMirrorEvent)
	{
		if (APawn* TargetPawnForThreshold = Cast<APawn>(Target))
		{
			const float Panic01 = GetPanicForPawn(TargetPawnForThreshold);

			if (!CanScareTagRunAtPanic(
				Event.ScareTag,
				Panic01))
			{
				UE_LOG(LogTemp, Log,
					TEXT("[DirectorThreshold] Blocked Tag=%s Target=%s Panic=%.2f MirrorMin=%.2f ChaseMin=%.2f"),
					*Event.ScareTag.ToString(),
					*GetNameSafe(Target),
					Panic01,
					MirrorEventPanicThreshold01,
					HauntChasePanicThreshold01);

				return false;
			}
		}
	}

	UGvTScareComponent* ScareComp = Target->FindComponentByClass<UGvTScareComponent>();
	if (!ScareComp)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Director] Dispatch failed: %s has no GvTScareComponent."), *GetNameSafe(Target));
		return false;
	}

	AGvTMirrorActor* SelectedMirror = nullptr;
	if (bIsMirrorEvent)
	{
		APawn* TargetPawn = Cast<APawn>(Target);
		SelectedMirror = FindEligibleMirrorForTarget(TargetPawn);
		AGvTThiefCharacter* Thief = Cast<AGvTThiefCharacter>(Target);
		UGvTThiefPerceptionComponent* Perception = Thief ? Thief->FindComponentByClass<UGvTThiefPerceptionComponent>() : nullptr;

		if (!SelectedMirror || !Perception)
		{
			UE_LOG(LogTemp, Log, TEXT("[MirrorDispatch] Rejected: no visible eligible mirror. Target=%s"), *GetNameSafe(Target));
			return false;
		}

		Perception->PlayMirrorScareFromDirector(SelectedMirror, Event.Intensity01, Event.Duration);
		UE_LOG(LogTemp, Log, TEXT("[MirrorDispatch] Confirmed Mirror=%s Target=%s"), *GetNameSafe(SelectedMirror), *GetNameSafe(Target));
	}

	const bool bIsLightChaseEvent = Event.ScareTag.MatchesTagExact(GvTScareTags::LightChase());
	if (bIsLightChaseEvent && !IsLightingScareAvailable(Event))
	{
		UE_LOG(LogTemp, Log, TEXT("[DirectorPower] Lighting scare rejected because house power/lights are unavailable. Tag=%s"), *Event.ScareTag.ToString());
		return false;
	}

	if (bIsLightChaseEvent && GetWorld())
	{
		const float Now = GetWorld()->GetTimeSeconds();
		if ((Now - LastLightingResponseTime) < LightingResponseCooldown)
		{
			UE_LOG(LogTemp, Log, TEXT("[Director] LightChase cooldown active. Remaining=%.2f"), LightingResponseCooldown - (Now - LastLightingResponseTime));
			return false;
		}
	}

	if (bIsHauntEvent && IsInPostHauntRecovery() && !Event.bIgnorePanicThreshold)
	{
		UE_LOG(LogTemp, Log, TEXT("[DirectorRecovery] Ordinary haunt blocked during recovery. Tag=%s Target=%s"), *Event.ScareTag.ToString(), *GetNameSafe(Target));
		return false;
	}

	if (IsAnyHauntActive() && bIsHauntEvent)
	{
		UE_LOG(LogTemp, Log,
			TEXT("[Director] Dispatch blocked during active haunt. Tag=%s Target=%s"),
			*Event.ScareTag.ToString(),
			*GetNameSafe(Target));

		return false;
	}

	if (bIsHauntEvent)
	{
		APawn* TargetPawn = Cast<APawn>(Target);
		if (!TargetPawn)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[Director] GhostHaunt failed: target is not a pawn."));

			return false;
		}

		UE_LOG(LogTemp, Warning,
			TEXT("[Director] Dispatch GhostHaunt chase to %s"),
			*GetNameSafe(TargetPawn));

		AGvTGhostCharacterBase* Ghost =
			SpawnHauntGhostForTarget(
				TargetPawn,
				GvTScareTags::GhostHaunt_Chase());

		if (!IsValid(Ghost))
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[Director] GhostHaunt dispatch rejected or spawn failed. Target=%s"),
				*GetNameSafe(TargetPawn));

			return false;
		}

		TriggerRequestedFlicker(Event, ScareComp);
		RememberDispatchedScare(Event.ScareTag);

		return true;
	}

	RememberDispatchedScare(Event.ScareTag);

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

		const bool bTryCreak = FMath::FRand() <= DoorCreakSelectionChance && Door->CanTriggerScareCreak();
		const bool bDoorScarePlayed = bTryCreak ? Door->TriggerScareCreak() : Door->TriggerScareSlam();
		if (!bDoorScarePlayed)
		{
			UE_LOG(LogTemp, Warning, TEXT("[Director] Door scare failed: %s was not eligible."), *GetNameSafe(Door));
			return false;
		}

		ApplyDoorSlamPanicToNearbyPlayers(Door, Target, bDoorScarePlayed);

		const float PanicRadius = FMath::Max(0.f, DoorSlamPanicRadius);

		const float TargetDistance = FVector::Dist(TargetPawn->GetActorLocation(), Door->GetActorLocation());

		const bool bTargetWithinPanicVicinity = TargetDistance <= PanicRadius;

		UE_LOG(LogTemp, Warning,
			TEXT("[Director] Dispatch DoorScare mode=%s target=%s door=%s withinPanicVicinity=%d dist=%.1f radius=%.1f"),
			bTryCreak ? TEXT("Creak") : TEXT("Slam"),
			*GetNameSafe(Target),
			*GetNameSafe(Door),
			bTargetWithinPanicVicinity ? 1 : 0,
			TargetDistance,
			PanicRadius);

		return true;
	}

	if (PS && !bIsLightChaseEvent)
	{
		FGvTPanicEvent PanicEvent;
		PanicEvent.Source = GvTMapScareTagToPanicSource(Event.ScareTag);
		PanicEvent.PanicDelta01 =
			(Event.bAffectsPanic && Event.PanicAmount > 0.f)
			? (ScalePanicAmount(Event.PanicAmount) / 100.f) * GetRecoveryPanicMultiplier(Event.ScareTag)
			: 0.f;
		PanicEvent.HauntPressureDelta01 = GvTGetPressureGain01ForScareTag(Event.ScareTag, Event.bTriggerLocalFlicker);
		PanicEvent.SourceActor = SelectedMirror ? SelectedMirror : Event.SourceActor;
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
	else if (!PS && !bIsLightChaseEvent)
	{
		if (Event.bAffectsPanic && Event.PanicAmount > 0.f)
		{
			ScareComp->AddPanic(ScalePanicAmount(Event.PanicAmount) * GetRecoveryPanicMultiplier(Event.ScareTag));
		}
	}

	if (!bIsLightChaseEvent)
	{
		ApplyGlobalScarePanicToOtherPlayers(Event, Target);
	}

	if (bIsMirrorEvent)
	{
		return true;
	}

	if (Event.ScareTag.MatchesTagExact(GvTScareTags::CrawlerOverhead()))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Director] CrawlerOverhead deprecated; routing to RearAudioSting for %s"), *GetNameSafe(Target));
		ScareComp->RequestRearAudioStingFromEvent(MakeRearAudioStingEvent(Target));
		return true;
	}

	if (Event.ScareTag.MatchesTagExact(GvTScareTags::LightChase()))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Director] Dispatch LightChase to %s"), *GetNameSafe(Target));
		LastLightingResponseTime = GetWorld() ? GetWorld()->GetTimeSeconds() : LastLightingResponseTime;
		ScareComp->RequestLightChaseFromEvent(Event);
		return true;
	}

	if (Event.ScareTag.MatchesTagExact(GvTScareTags::GhostScream()) ||
		Event.ScareTag.MatchesTagExact(GvTScareTags::GhostScare_Scream()))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Director] Dispatch GhostScream to %s"), *GetNameSafe(Target));
		ScareComp->RequestGhostScreamFromEvent(Event);
		return true;
	}

	if (Event.ScareTag.MatchesTagExact(GvTScareTags::RearAudioSting()) ||
		Event.ScareTag.MatchesTagExact(GvTScareTags::GhostScare_AudioRear()))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Director] Dispatch RearAudioSting to %s"), *GetNameSafe(Target));
		ScareComp->RequestRearAudioStingFromEvent(Event);
		return true;
	}

	if (Event.ScareTag.MatchesTagExact(GvTScareTags::GhostScare_Close()))
	{
		if (AGvTThiefCharacter* Thief = Cast<AGvTThiefCharacter>(Target))
		{
			Thief->Client_PlayGhostScare(GvTScareTags::GhostScare_Close());
			return true;
		}

		return false;
	}

	UE_LOG(LogTemp, Warning, TEXT("[Director] Dispatch failed: Unknown scare tag %s"), *Event.ScareTag.ToString());
	return false;
}

bool UGvTDirectorSubsystem::DispatchScareEventSimple(
	const FGameplayTag& ScareTag,
	APawn* TargetPawn,
	AActor* SourceActor,
	bool bIgnorePanicThreshold)
{
	if (!IsPawnEligibleForDirector(TargetPawn))
	{
		return false;
	}

	FGvTScareEvent Event;

	if (ScareTag.MatchesTagExact(GvTScareTags::GhostScare_Close()))
	{
		Event = MakeCloseGhostScareEvent(TargetPawn);
	}
	else if (ScareTag.MatchesTagExact(GvTScareTags::GhostScare_AudioRear()) ||
		ScareTag.MatchesTagExact(GvTScareTags::RearAudioSting()))
	{
		Event = MakeRearAudioStingEvent(TargetPawn);
		Event.ScareTag = GvTScareTags::GhostScare_AudioRear();
	}
	else if (ScareTag.MatchesTagExact(GvTScareTags::GhostScare_Scream()) ||
		ScareTag.MatchesTagExact(GvTScareTags::GhostScream()))
	{
		Event = MakeGhostScreamEvent(TargetPawn);
		Event.ScareTag = GvTScareTags::GhostScare_Scream();
	}
	else if (ScareTag.MatchesTagExact(GvTScareTags::CrawlerChase()) ||
		ScareTag.MatchesTagExact(GvTScareTags::GhostHaunt_Chase()))
	{
		Event = MakeCrawlerChaseEvent(TargetPawn);
	}
	else if (ScareTag.MatchesTagExact(GvTScareTags::CrawlerOverhead()))
	{
		Event = MakeCrawlerOverheadEvent(TargetPawn);
	}
	else if (ScareTag.MatchesTagExact(GvTScareTags::LightChase()))
	{
		Event = MakeLightChaseEvent(TargetPawn);
	}
	else if (ScareTag.MatchesTagExact(GvTScareTags::Mirror()) || ScareTag.MatchesTagExact(GvTScareTags::GhostEvent_Mirror()))
	{
		Event = MakeMirrorEvent(TargetPawn);
	}
	else
	{
		Event.TargetActor = TargetPawn;
		Event.ScareTag = ScareTag;
		Event.WorldHint = TargetPawn->GetActorLocation();
		Event.PanicAmount = 10.f;
		Event.Intensity01 = 1.0f;
		Event.Duration = 1.5f;
		Event.bAffectsPanic = true;
		Event.bTriggerLocalFlicker = false;
	}

	Event.TargetActor = TargetPawn;
	Event.SourceActor = SourceActor;
	Event.bIgnorePanicThreshold = bIgnorePanicThreshold;

	return DispatchScareEvent(Event);
}

TSubclassOf<AGvTGhostCharacterBase> UGvTDirectorSubsystem::ChooseHauntGhostClass() const
{
	TArray<TSubclassOf<AGvTGhostCharacterBase>> Candidates;

	for (const UGvTGhostModelData* Model : GhostModels)
	{
		if (Model && Model->bCanHaunt && *Model->GhostClass)
		{
			Candidates.Add(Model->GhostClass);
		}
	}

	for (TSubclassOf<AGvTGhostCharacterBase> GhostClass : HauntGhostClasses)
	{
		if (*GhostClass)
		{
			Candidates.Add(GhostClass);
		}
	}

	if (Candidates.Num() == 0)
	{
		return nullptr;
	}

	return Candidates[FMath::RandRange(0, Candidates.Num() - 1)];
}

bool UGvTDirectorSubsystem::ChooseHauntSpawnTransform(APawn* TargetPawn, FGameplayTag HauntTag, FTransform& OutSpawnTransform) const
{
	if (!TargetPawn)
	{
		return false;
	}

	const FVector TargetLoc = TargetPawn->GetActorLocation();

	TArray<AGvTGhostSpawnPoint*> EligibleSpawnPoints;
	int32 DiscoveredSpawnPointCount = 0;

	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<AGvTGhostSpawnPoint> It(World); It; ++It)
		{
			AGvTGhostSpawnPoint* Point = *It;
			++DiscoveredSpawnPointCount;
			if (!IsValid(Point) || !Point->SupportsHauntTag(HauntTag))
			{
				continue;
			}

			const FVector PointLocation = Point->GetActorLocation();
			if (PointLocation.IsNearlyZero(10.f))
			{
				UE_LOG(LogTemp, Error,
					TEXT("[GhostSpawn] Rejecting spawn point at world origin. Point=%s Location=%s Tag=%s"),
					*GetNameSafe(Point),
					*PointLocation.ToString(),
					*HauntTag.ToString());
				continue;
			}

			EligibleSpawnPoints.Add(Point);
		}
	}

	UE_LOG(LogTemp, Warning,
		TEXT("[GhostSpawn] Discovered=%d Eligible=%d Target=%s TargetLocation=%s Tag=%s"),
		DiscoveredSpawnPointCount,
		EligibleSpawnPoints.Num(),
		*GetNameSafe(TargetPawn),
		*TargetLoc.ToString(),
		*HauntTag.ToString());

	AGvTGhostSpawnPoint* SpawnPoint = nullptr;

	if (!EligibleSpawnPoints.IsEmpty())
	{
		SpawnPoint = EligibleSpawnPoints[FMath::RandRange(0, EligibleSpawnPoints.Num() - 1)];
	}

	if (!IsValid(SpawnPoint))
	{
		UE_LOG(LogTemp, Error,
			TEXT("[GhostSpawn] FAILED: No valid enabled GvTGhostSpawnPoint supports haunt tag=%s. Discovered=%d Eligible=%d. Haunt cancelled; world-origin fallback is forbidden."),
			*HauntTag.ToString(),
			DiscoveredSpawnPointCount,
			EligibleSpawnPoints.Num());
		return false;
	}

	// Designers place spawn points on the floor. Character actor locations are capsule centers,
	// so add a small Z offset and never nav-project the spawn to another floor or the roof.
	const FVector SpawnLoc = SpawnPoint->GetActorLocation() + FVector(0.f, 0.f, HauntSpawnPointZOffset);
	const FRotator SpawnRot = (TargetLoc - SpawnLoc).Rotation();
	OutSpawnTransform = FTransform(SpawnRot, SpawnLoc);

	UE_LOG(LogTemp, Warning,
		TEXT("[GhostSpawn] Selected=%s Eligible=%d Raw=%s Final=%s Target=%s Tag=%s"),
		*GetNameSafe(SpawnPoint),
		EligibleSpawnPoints.Num(),
		*SpawnPoint->GetActorLocation().ToString(),
		*SpawnLoc.ToString(),
		*TargetLoc.ToString(),
		*HauntTag.ToString());

	return true;
}

AGvTGhostCharacterBase* UGvTDirectorSubsystem::SpawnHauntGhostForTarget(APawn* TargetPawn, FGameplayTag HauntTag, TSubclassOf<AGvTGhostCharacterBase> FallbackGhostClass)
{
	UWorld* World = GetWorld();
	if (!World || !IsPawnEligibleForDirector(TargetPawn))
	{
		return nullptr;
	}

	if (IsAnyHauntActive())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[GhostHaunt] Spawn skipped: another haunt ghost is already active. Existing=%s RequestedTarget=%s"),
			*GetNameSafe(FindActiveHauntGhost()),
			*GetNameSafe(TargetPawn));

		return nullptr;
	}

	const TWeakObjectPtr<APawn> TargetKey(TargetPawn);
	const float Now = World->GetTimeSeconds();
	if (const float* LastRequestTime = LastManualHauntRequestTimeByTarget.Find(TargetKey))
	{
		const float Elapsed = Now - *LastRequestTime;
		if (Elapsed < ManualHauntRequestCooldown)
		{
			if (const TWeakObjectPtr<AGvTGhostCharacterBase>* ExistingPtr = ActiveHauntGhostByTarget.Find(TargetKey))
			{
				if (ExistingPtr->IsValid())
				{
					UE_LOG(LogTemp, Warning,
						TEXT("[GhostHaunt] Request ignored by cooldown. Target=%s Existing=%s Remaining=%.2f"),
						*GetNameSafe(TargetPawn),
						*GetNameSafe(ExistingPtr->Get()),
						ManualHauntRequestCooldown - Elapsed);
					return ExistingPtr->Get();
				}
			}
		}
	}

	LastManualHauntRequestTimeByTarget.Add(TargetKey, Now);

	TSubclassOf<AGvTGhostCharacterBase> SpawnClass = ChooseHauntGhostClass();

	if (!SpawnClass && *FallbackGhostClass)
	{
		SpawnClass = FallbackGhostClass;
	}

	if (!SpawnClass && *DefaultHauntGhostClass)
	{
		SpawnClass = DefaultHauntGhostClass;
	}

	if (!SpawnClass)
	{
		UE_LOG(LogTemp, Error,
			TEXT("[GhostHaunt] FAILED: No haunt ghost class configured. Add BP_GvTGhoulGhost or BP_GvTCrawlerGhost to Director DefaultHauntGhostClass / HauntGhostClasses."));
		return nullptr;
	}

	FTransform SpawnTransform;
	if (!ChooseHauntSpawnTransform(TargetPawn, HauntTag, SpawnTransform))
	{
		return nullptr;
	}

	FActorSpawnParameters Params;
	Params.Owner = TargetPawn;
	Params.Instigator = TargetPawn;
	// The placed spawn point is authoritative. Collision adjustment must never relocate
	// the ghost away from it (including vertically onto the roof).
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	SetHauntExitDoorsLocked(true);
	bHauntSpawnInProgress = true;

	AGvTGhostCharacterBase* Ghost =
		World->SpawnActor<AGvTGhostCharacterBase>(
			SpawnClass,
			SpawnTransform,
			Params);

	bHauntSpawnInProgress = false;

	if (!IsValid(Ghost))
	{
		SetHauntExitDoorsLocked(false);
		UE_LOG(LogTemp, Warning,
			TEXT("[GhostHaunt] Failed to spawn haunt ghost class=%s."),
			*GetNameSafe(SpawnClass));

		return nullptr;
	}

	UE_LOG(LogTemp, Warning,
		TEXT("[GhostSpawn] Spawned=%s Class=%s Requested=%s Actual=%s Target=%s Tag=%s"),
		*GetNameSafe(Ghost),
		*GetNameSafe(SpawnClass),
		*SpawnTransform.GetLocation().ToString(),
		*Ghost->GetActorLocation().ToString(),
		*GetNameSafe(TargetPawn),
		*HauntTag.ToString());

	if (!Ghost->GetActorLocation().Equals(SpawnTransform.GetLocation(), 5.f))
	{
		UE_LOG(LogTemp, Error,
			TEXT("[GhostSpawn] Spawn transform mismatch. Ghost=%s Requested=%s Actual=%s"),
			*GetNameSafe(Ghost),
			*SpawnTransform.GetLocation().ToString(),
			*Ghost->GetActorLocation().ToString());
	}

	ActiveHauntGhostByTarget.Add(TargetKey, Ghost);

	const FVector LocationBeforeBeginHaunt = Ghost->GetActorLocation();
	Ghost->BeginGhostHaunt(TargetPawn, HauntTag);
	if (const AGvTGameStateBase* GS = World->GetGameState<AGvTGameStateBase>(); GS && GS->IsMainObjectiveSecured())
	{
		if (AGvTHauntGhostBase* HauntGhost = Cast<AGvTHauntGhostBase>(Ghost))
		{
			HauntGhost->ActivateCursedHaunt(nullptr, false);
		}
	}

	if (!Ghost->GetActorLocation().Equals(LocationBeforeBeginHaunt, 5.f))
	{
		UE_LOG(LogTemp, Error,
			TEXT("[GhostSpawn] BeginGhostHaunt moved ghost unexpectedly. Ghost=%s Before=%s After=%s"),
			*GetNameSafe(Ghost),
			*LocationBeforeBeginHaunt.ToString(),
			*Ghost->GetActorLocation().ToString());
	}

	UE_LOG(LogTemp, Warning,
		TEXT("[GhostHaunt] Started single world haunt tag=%s Ghost=%s Target=%s Location=%s"),
		*HauntTag.ToString(),
		*GetNameSafe(Ghost),
		*GetNameSafe(TargetPawn),
		*Ghost->GetActorLocation().ToString());

	ApplyHauntStartPanicToAllPlayers(Ghost, TargetPawn);

	return Ghost;
}

bool UGvTDirectorSubsystem::TriggerRequestedFlicker(const FGvTScareEvent& Event, UGvTScareComponent* TargetScareComp)
{
	if (!TargetScareComp)
	{
		return false;
	}

	if ((Event.bTriggerGroupFlicker || Event.bTriggerLocalFlicker) && !IsLightingScareAvailable(Event))
	{
		return false;
	}

	if (Event.bTriggerGroupFlicker)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Director] Trigger GROUP flicker"));
		TargetScareComp->RequestGroupHouseLightFlicker(
			FMath::Clamp(Event.Intensity01, 0.f, 1.f),
			Event.Duration > 0.f ? Event.Duration : 1.5f
		);
		return true;
	}

	if (Event.bTriggerLocalFlicker)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Director] Trigger LOCAL flicker"));
		TargetScareComp->RequestLocalHouseLightFlicker(
			FMath::Clamp(Event.Intensity01, 0.f, 1.f),
			Event.Duration > 0.f ? Event.Duration : 1.5f
		);
		return true;
	}

	return false;
}

bool UGvTDirectorSubsystem::IsLightingScareAvailable(const FGvTScareEvent& Event) const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	AGvTPowerBoxActor* PowerBox = const_cast<UGvTDirectorSubsystem*>(this)->FindPowerBoxInWorld();
	if (!IsValid(PowerBox) || !PowerBox->IsPowerAvailableForScares())
	{
		return false;
	}

	const UGvTLightFlickerSubsystem* FlickerSubsystem = World->GetSubsystem<UGvTLightFlickerSubsystem>();
	if (!FlickerSubsystem)
	{
		return false;
	}

	if (Event.bTriggerGroupFlicker)
	{
		return FlickerSubsystem->CountLightsInRadius(FVector::ZeroVector, 1000000.f) > 0;
	}

	const FVector Center = !Event.WorldHint.IsNearlyZero() ? Event.WorldHint : Event.TargetActor ? Event.TargetActor->GetActorLocation() : FVector::ZeroVector;
	const float Radius = Event.ScareTag.MatchesTagExact(GvTScareTags::LightChase()) ? FMath::Max(500.f, LightChaseStartDistance + LightChaseFlickerRadius) : 8000.f;
	return FlickerSubsystem->CountLightsInRadius(Center, Radius) > 0;
}

float UGvTDirectorSubsystem::GetPanicMultiplier() const
{
	float ActivityGrowth = 0.f;
	if (HouseActivity01 >= 0.90f) ActivityGrowth = 1.15f;
	else if (HouseActivity01 >= 0.70f) ActivityGrowth = FMath::Lerp(0.75f, 1.15f, (HouseActivity01 - 0.70f) / 0.20f);
	else if (HouseActivity01 >= 0.50f) ActivityGrowth = FMath::Lerp(0.45f, 0.75f, (HouseActivity01 - 0.50f) / 0.20f);
	else if (HouseActivity01 >= 0.25f) ActivityGrowth = FMath::Lerp(0.20f, 0.45f, (HouseActivity01 - 0.25f) / 0.25f);

	const float MinutesSinceFirstTheft = TimeActivityPerSecond > KINDA_SMALL_NUMBER ? (TimeActivity01 / TimeActivityPerSecond) / 60.f : 0.f;
	const float TimeGrowth = FMath::Min(MaximumPanicTimeGrowth, MinutesSinceFirstTheft * PanicTimeGrowthPerMinute);
	return FMath::Clamp(1.f + ActivityGrowth + TimeGrowth, 1.f, MaximumPanicMultiplier);
}

float UGvTDirectorSubsystem::ScalePanicAmount(float BaseAmount) const
{
	return FMath::Max(0.f, BaseAmount) * GetPanicMultiplier();
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

float UGvTDirectorSubsystem::GetPanicForPawn(const APawn* Pawn) const
{
	if (!IsPawnEligibleForDirector(Pawn))
	{
		return 0.f;
	}

	const APlayerController* PC = Cast<APlayerController>(Pawn->GetController());
	const AGvTPlayerState* PS = PC ? Cast<AGvTPlayerState>(PC->PlayerState) : nullptr;
	if (!PS)
	{
		PS = Pawn->GetPlayerState<AGvTPlayerState>();
	}

	return PS ? FMath::Clamp(PS->GetPanic01(), 0.f, 1.f) : 0.f;
}

bool UGvTDirectorSubsystem::IsGhostScareTag(const FGameplayTag& ScareTag) const
{
	return ScareTag.MatchesTagExact(GvTScareTags::GhostScare_Close())
		|| ScareTag.MatchesTagExact(GvTScareTags::GhostScare_Scream())
		|| ScareTag.MatchesTagExact(GvTScareTags::GhostScare_AudioRear())
		|| ScareTag.MatchesTagExact(GvTScareTags::CrawlerOverhead());
}

bool UGvTDirectorSubsystem::HasActiveHaunt() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	for (TActorIterator<AGvTHauntGhostBase> It(World); It; ++It)
	{
		const AGvTHauntGhostBase* Ghost = *It;
		if (IsValid(Ghost))
		{
			const EGvTHauntGhostState State = Ghost->GetHauntState();
			if (State == EGvTHauntGhostState::SpawnIntro
				|| State == EGvTHauntGhostState::Roaming
				|| State == EGvTHauntGhostState::Investigating
				|| State == EGvTHauntGhostState::Chasing
				|| State == EGvTHauntGhostState::Searching
				|| State == EGvTHauntGhostState::Recovering)
			{
				return true;
			}
		}
	}

	return false;
}

bool UGvTDirectorSubsystem::CanScareTagRunAtPanic(const FGameplayTag& ScareTag, float Panic01) const
{
	if (ScareTag.MatchesTagExact(GvTScareTags::Mirror()) ||
		ScareTag.MatchesTagExact(GvTScareTags::GhostEvent_Mirror()))
	{
		return Panic01 >= MirrorEventPanicThreshold01;
	}

	if (ScareTag.MatchesTagExact(GvTScareTags::CrawlerChase()) ||
		ScareTag.MatchesTagExact(GvTScareTags::GhostHaunt_Chase()))
	{
		return Panic01 >= HauntChasePanicThreshold01;
	}

	if (IsGhostScareTag(ScareTag))
	{
		return (Panic01 >= GhostScareMinPanicThreshold01 || HouseActivity01 >= GhostScareActivityUnlock01) && Panic01 <= 1.0f;
	}

	return true;
}

float UGvTDirectorSubsystem::GetHauntPressureForPawn(const APawn* Pawn) const
{
	if (!IsPawnEligibleForDirector(Pawn))
	{
		return 0.f;
	}

	const APlayerController* PC = Cast<APlayerController>(Pawn->GetController());
	const AGvTPlayerState* PS = PC ? Cast<AGvTPlayerState>(PC->PlayerState) : nullptr;
	return PS ? FMath::Clamp(PS->GetRecentHauntPressure01(), 0.f, 1.f) : 0.f;
}

float UGvTDirectorSubsystem::ScoreTarget(APawn* Pawn) const
{
	if (!IsPawnEligibleForDirector(Pawn))
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
	if (!IsPawnEligibleForDirector(Pawn))
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
		if (!IsPawnEligibleForDirector(OtherPawn) || OtherPawn == Pawn)
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
	if (!GetWorld() || !IsPawnEligibleForDirector(Pawn))
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
		if (!PS || PS->IsDeadForPanic())
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
		if (!PS || PS->IsDeadForPanic())
		{
			continue;
		}

		Sum += FMath::Clamp(
			PS->GetRecentHauntPressure01(),
			0.f,
			1.f);

		Count++;
	}

	return (Count > 0) ? (Sum / float(Count)) : 0.f;
}

void UGvTDirectorSubsystem::RegisterUniqueTheft(const AGvTInteractableItem* Item)
{
	if (!Item)
	{
		return;
	}

	float Impulse = SmallTheftActivityImpulse;
	switch (Item->GetItemTier())
	{
	case EGvTItemTier::Medium:
		Impulse = MediumTheftActivityImpulse;
		break;
	case EGvTItemTier::Large:
		Impulse = LargeTheftActivityImpulse;
		break;
	case EGvTItemTier::MainObjective:
		Impulse = MainObjectiveTheftActivityImpulse;
		break;
	case EGvTItemTier::Small:
	default:
		break;
	}

	Impulse *= FMath::Max(0.f, Item->GetHouseTensionMultiplier());
	TheftActivity01 = FMath::Clamp(TheftActivity01 + Impulse, 0.f, 1.f);
	bHouseActivityStarted = true;

	UE_LOG(LogTemp, Log, TEXT("[HouseActivityTheft] Item=%s Tier=%d Impulse=%.2f TheftActivity=%.2f"), *GetNameSafe(Item), static_cast<int32>(Item->GetItemTier()), Impulse, TheftActivity01);
}

void UGvTDirectorSubsystem::UpdateHouseActivity(float DeltaSeconds)
{
	if (bHouseActivityStarted)
	{
		TimeActivity01 = FMath::Clamp(TimeActivity01 + FMath::Max(0.f, TimeActivityPerSecond) * DeltaSeconds, 0.f, 1.f);
	}

	const float AvgPanic01 = ComputeAveragePlayerPanic01();
	const float AvgPressure01 = ComputeAveragePlayerPressure01();
	const float TotalWeight = FMath::Max(0.01f, TheftActivityWeight + TimeActivityWeight + PanicActivityWeight + PressureActivityWeight);

	const float CalculatedActivity01 = FMath::Clamp(
		((TheftActivity01 * TheftActivityWeight) +
		 (TimeActivity01 * TimeActivityWeight) +
		 (AvgPanic01 * PanicActivityWeight) +
		 (AvgPressure01 * PressureActivityWeight)) / TotalWeight,
		0.f,
		1.f);

	// The players can recover; the house does not forgive theft. Panic and pressure
	// may raise the activity ceiling, but their decay can never lower it again.
	HouseActivity01 = FMath::Max(HouseActivity01, CalculatedActivity01);

	if (bLogHouseActivity)
	{
		const TCHAR* Band = HouseActivity01 < 0.25f ? TEXT("Dormant") : HouseActivity01 < 0.55f ? TEXT("Stirring") : HouseActivity01 < 0.80f ? TEXT("Awake") : TEXT("Hostile");
		UE_LOG(LogTemp, Log, TEXT("[HouseActivity] Theft=%.2f Time=%.2f Panic=%.2f Pressure=%.2f Total=%.2f Band=%s"), TheftActivity01, TimeActivity01, AvgPanic01, AvgPressure01, HouseActivity01, Band);
	}
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
	return FMath::Lerp(GlobalHauntCooldownMax, GlobalHauntCooldownMin, FMath::Max(HouseTension01, HouseActivity01));
}

float UGvTDirectorSubsystem::ScoreDoorForSlam(const APawn* TargetPawn, const AGvTDoorActor* Door) const
{
	if (!IsPawnEligibleForDirector(TargetPawn) || !IsValid(Door))
	{
		return -1.f;
	}

	if (!Door->CanTriggerScareSlam() && !Door->CanTriggerScareCreak())
	{
		return -1.f;
	}

	const float Distance = FVector::Dist2D(
		TargetPawn->GetActorLocation(),
		Door->GetActorLocation());

	if (Distance > DoorSlamSearchRadius)
	{
		return -1.f;
	}

	const float DistanceAlpha = FMath::Clamp(
		Distance / FMath::Max(1.f, DoorSlamSearchRadius),
		0.f,
		1.f);

	const float DistanceScore = 1.f - DistanceAlpha;

	const float ExitDoorMultiplier = Door->IsExitDoor()
		? FMath::Clamp(ExitDoorScareScoreMultiplier, 0.f, 1.f)
		: 1.f;

	return DistanceScore * DoorSlamDistanceWeight * ExitDoorMultiplier;
}

AGvTDoorActor* UGvTDirectorSubsystem::ChooseBestDoorSlamTarget(APawn* TargetPawn) const
{
	if (!GetWorld() || !IsPawnEligibleForDirector(TargetPawn))
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

void UGvTDirectorSubsystem::OnPlayerInteractionEvent(AActor* Interactor, AActor* TargetActor, EGvTInteractionVerb Verb)
{
	if (!Interactor)
	{
		return;
	}

	if (const AGvTPowerBoxActor* PowerBox = Cast<AGvTPowerBoxActor>(TargetActor))
	{
		// Breaker toggles are utility interactions, not paranormal triggers.
		// Random failure and scripted ghost power events remain independent.
		UE_LOG(LogTemp, VeryVerbose, TEXT("[DirectorInteraction] Ignored breaker interaction. Breaker=%s State=%d"), *GetNameSafe(PowerBox), static_cast<int32>(PowerBox->GetPowerState()));
		return;
	}

	APawn* Pawn = Cast<APawn>(Interactor);
	if (!IsPawnEligibleForDirector(Pawn))
	{
		return;
	}

	const AGvTInteractableItem* Item = Cast<AGvTInteractableItem>(TargetActor);

	// Item reactions represent stealing the item. Photo and scan interactions
	// must not raise house tension or roll a theft reaction.
	if (Item && Verb != EGvTInteractionVerb::Interact)
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

	bool bIsElectrical = TargetActor && TargetActor->ActorHasTag(TEXT("Electrical"));
	bool bIsValuable = TargetActor && TargetActor->ActorHasTag(TEXT("Valuable"));
	bool bIsNoisy = TargetActor && TargetActor->ActorHasTag(TEXT("Noisy"));

	float ItemValue01 = bIsValuable ? 1.0f : 0.0f;
	float ReactionChance = 0.10f;
	float TensionImpulse = 0.0f;

	if (Item)
	{
		if (!Item->ShouldUpsetGhostsOnInteract())
		{
			UE_LOG(LogTemp, Log, TEXT("[ItemReaction] Item=%s Tier=%d Result=SKIPPED Reason=UpsetsGhostsDisabled"), *GetNameSafe(Item), static_cast<int32>(Item->GetItemTier()));
			return;
		}

		bIsElectrical = bIsElectrical || Item->IsGhostElectrical();
		bIsValuable = bIsValuable || Item->IsGhostValuable();
		bIsNoisy = bIsNoisy || Item->IsGhostNoisy();
		ItemValue01 = FMath::Max(ItemValue01, Item->GetGhostItemValue01());
		ReactionChance = Item->GetGhostReactionChance();
		TensionImpulse = Item->GetGhostTensionImpulse();

		// Every unique theft permanently wakes the house and also affects short-term tension.
		RegisterUniqueTheft(Item);
		ApplyHouseTensionImpulse(TensionImpulse);

		float TheftPanic01 = SmallTheftPanic01;
		switch (Item->GetItemTier())
		{
		case EGvTItemTier::Medium: TheftPanic01 = MediumTheftPanic01; break;
		case EGvTItemTier::Large: TheftPanic01 = LargeTheftPanic01; break;
		case EGvTItemTier::MainObjective: TheftPanic01 = MainObjectiveTheftPanic01; break;
		case EGvTItemTier::Small:
		default: break;
		}

		TArray<AActor*> Thieves;
		UGameplayStatics::GetAllActorsOfClass(GetWorld(), AGvTThiefCharacter::StaticClass(), Thieves);
		for (AActor* ThiefActor : Thieves)
		{
			APawn* AffectedPawn = Cast<APawn>(ThiefActor);
			AGvTPlayerState* AffectedPS = AffectedPawn ? AffectedPawn->GetPlayerState<AGvTPlayerState>() : nullptr;
			if (!AffectedPS || AffectedPS->IsDeadForPanic())
			{
				continue;
			}

			const bool bPrimaryThief = AffectedPawn == Pawn;
			if (!bPrimaryThief && FVector::DistSquared(AffectedPawn->GetActorLocation(), Item->GetActorLocation()) > FMath::Square(NearbyTheftPanicRadius))
			{
				continue;
			}

			FGvTPanicEvent TheftPanicEvent;
			TheftPanicEvent.Source = Item->GetItemTier() >= EGvTItemTier::Large ? EGvTPanicSource::ItemPickupHighValue : EGvTPanicSource::ItemPickup;
			TheftPanicEvent.PanicDelta01 = TheftPanic01 * (bPrimaryThief ? 1.f : NearbyTheftPanicMultiplier);
			TheftPanicEvent.HauntPressureDelta01 = TheftPanicEvent.PanicDelta01 * 0.50f;
			TheftPanicEvent.InstigatorActor = Pawn;
			TheftPanicEvent.SourceActor = TargetActor;
			TheftPanicEvent.WorldLocation = Item->GetActorLocation();
			TheftPanicEvent.bExecutionSucceeded = true;
			AffectedPS->ApplyPanicEventAuthority(TheftPanicEvent);
		}

		if (Item->ShouldForceHauntReaction())
		{
			TriggerInteractionReaction(Pawn, TargetActor, Item, bIsElectrical, bIsValuable, bIsNoisy, ItemValue01);
		}
		else
		{
			ScheduleTheftReaction(Pawn, TargetActor, Item, bIsElectrical, bIsValuable, bIsNoisy, ItemValue01);
		}
		return;
	}
	else
	{
		// Non-loot interactions keep their lightweight generic reaction calculation.
		ReactionChance += Panic * 0.35f;
		ReactionChance += Pressure * 0.25f;

		if (bIsElectrical)
		{
			ReactionChance += 0.35f;
		}

		if (bIsValuable)
		{
			ReactionChance += FMath::Lerp(0.20f, 0.40f, FMath::Clamp(ItemValue01, 0.0f, 1.0f));
		}

		if (bIsNoisy)
		{
			ReactionChance += 0.25f;
		}
	}

	ReactionChance = FMath::Clamp(ReactionChance, 0.0f, 1.0f);

	const float ReactionRoll = FMath::FRand();
	const bool bReactionSucceeded = ReactionRoll <= ReactionChance;

	if (Item)
	{
		UE_LOG(LogTemp, Log, TEXT("[ItemReaction] Item=%s Tier=%d BaseChance=%.2f FinalChance=%.2f Roll=%.2f Result=%s TensionImpulse=%.2f"), *GetNameSafe(Item), static_cast<int32>(Item->GetItemTier()), Item->GetGhostReactionChance(), ReactionChance, ReactionRoll, bReactionSucceeded ? TEXT("SUCCESS") : TEXT("FAILED"), TensionImpulse);
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("[DirectorInteraction] Player=%s Target=%s Verb=%d Electrical=%d Valuable=%d Noisy=%d Panic=%.2f Pressure=%.2f Chance=%.2f Roll=%.2f Result=%s"), *GetNameSafe(Pawn), *GetNameSafe(TargetActor), static_cast<int32>(Verb), bIsElectrical ? 1 : 0, bIsValuable ? 1 : 0, bIsNoisy ? 1 : 0, Panic, Pressure, ReactionChance, ReactionRoll, bReactionSucceeded ? TEXT("SUCCESS") : TEXT("FAILED"));
	}

	if (!bReactionSucceeded)
	{
		return;
	}

	TriggerInteractionReaction(Pawn, TargetActor, Item, bIsElectrical, bIsValuable, bIsNoisy, ItemValue01);
}

void UGvTDirectorSubsystem::ScheduleTheftReaction(APawn* Pawn, AActor* TargetActor, const AGvTInteractableItem* Item, bool bIsElectrical, bool bIsValuable, bool bIsNoisy, float ItemValue01)
{
	UWorld* World = GetWorld();
	if (!World || !IsPawnEligibleForDirector(Pawn))
	{
		return;
	}

	PendingTheftPawn = Pawn;
	PendingTheftSource = TargetActor;
	bPendingTheftElectrical |= bIsElectrical;
	bPendingTheftValuable |= bIsValuable;
	bPendingTheftNoisy |= bIsNoisy;
	PendingTheftValue01 = FMath::Max(PendingTheftValue01, ItemValue01);
	PendingTheftCount = FMath::Max(1, PendingTheftCount + 1);

	if (!World->GetTimerManager().IsTimerActive(TimerHandle_TheftReaction))
	{
		const float MinDelay = FMath::Max(0.f, TheftReactionDelayMin);
		const float MaxDelay = FMath::Max(MinDelay, TheftReactionDelayMax);
		const float Delay = FMath::FRandRange(MinDelay, MaxDelay);
		World->GetTimerManager().SetTimer(TimerHandle_TheftReaction, this, &UGvTDirectorSubsystem::ExecutePendingTheftReaction, Delay, false);
		UE_LOG(LogTemp, Log, TEXT("[TheftReactionQueue] Scheduled response in %.2fs Player=%s Item=%s"), Delay, *GetNameSafe(Pawn), *GetNameSafe(TargetActor));
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("[TheftReactionQueue] Rapid theft merged. Count=%d Severity will increase."), PendingTheftCount);
	}
}

void UGvTDirectorSubsystem::ExecutePendingTheftReaction()
{
	APawn* Pawn = PendingTheftPawn.Get();
	AActor* Source = PendingTheftSource.Get();
	const AGvTInteractableItem* Item = Cast<AGvTInteractableItem>(Source);
	const int32 TheftCount = PendingTheftCount;
	const bool bElectrical = bPendingTheftElectrical;
	const bool bValuable = bPendingTheftValuable || TheftCount >= 2;
	const bool bNoisy = bPendingTheftNoisy || TheftCount >= 2;
	const float Value01 = FMath::Clamp(PendingTheftValue01 + (FMath::Max(0, TheftCount - 1) * 0.15f), 0.f, 1.f);

	PendingTheftPawn.Reset();
	PendingTheftSource.Reset();
	bPendingTheftElectrical = false;
	bPendingTheftValuable = false;
	bPendingTheftNoisy = false;
	PendingTheftValue01 = 0.f;
	PendingTheftCount = 0;

	if (!IsPawnEligibleForDirector(Pawn))
	{
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[TheftReactionQueue] Executing response. Count=%d Player=%s Source=%s"), TheftCount, *GetNameSafe(Pawn), *GetNameSafe(Source));
	TriggerInteractionReaction(Pawn, Source, Item, bElectrical, bValuable, bNoisy, Value01);
}

void UGvTDirectorSubsystem::TriggerInteractionReaction(
	APawn* Pawn,
	AActor* TargetActor,
	const AGvTInteractableItem* Item,
	bool bIsElectrical,
	bool bIsValuable,
	bool bIsNoisy,
	float ItemValue01)
{
	if (!IsPawnEligibleForDirector(Pawn))
	{
		return;
	}

	AGvTPlayerState* PS = Pawn->GetPlayerState<AGvTPlayerState>();
	if (!PS)
	{
		return;
	}

	const float Panic = PS->GetPanic01();

	if (Item && Item->ShouldForceHauntReaction())
	{
		UE_LOG(LogTemp, Warning, TEXT("[DirectorItemReaction] Main objective taken. Starting objective haunt. Item=%s Player=%s"), *GetNameSafe(TargetActor), *GetNameSafe(Pawn));

		bool bDispatched = false;
		if (AGvTHauntGhostBase* ExistingHaunt = FindActiveHauntGhost())
		{
			ExistingHaunt->ApplyObjectiveHauntTuning();
			bDispatched = true;
		}
		else
		{
			bDispatched = DispatchScareEventSimple(GvTScareTags::GhostHaunt_Chase(), Pawn, TargetActor, true);
			if (bDispatched)
			{
				if (AGvTHauntGhostBase* ObjectiveGhost = FindActiveHauntGhost())
				{
					ObjectiveGhost->ApplyObjectiveHauntTuning();
				}
			}
		}

		UE_LOG(LogTemp, Warning, TEXT("[ItemReactionResult] Item=%s Event=%s ObjectiveHaunt=1 Dispatched=%d"), *GetNameSafe(TargetActor), *GvTScareTags::GhostHaunt_Chase().ToString(), bDispatched ? 1 : 0);
		return;
	}

	TArray<FGameplayTag> WeightedEvents;
	WeightedEvents.Reserve(32);

	auto AddWeightedEvent = [&WeightedEvents](const FGameplayTag& EventTag, int32 Weight)
		{
			if (!EventTag.IsValid() || Weight <= 0)
			{
				return;
			}

			for (int32 Index = 0; Index < Weight; ++Index)
			{
				WeightedEvents.Add(EventTag);
			}
		};

	// ---------------------------------------------------------------------
	// Item-authored event preferences
	// ---------------------------------------------------------------------

	if (Item)
	{
		for (const FGameplayTag& PreferredEvent : Item->GetPreferredGhostEvents())
		{
			AddWeightedEvent(PreferredEvent, 6);
		}

		if (Item->HasGhostTrait(EGvTItemGhostTrait::Electrical))
		{
			AddWeightedEvent(GvTScareTags::LightChase(), 3);
			AddWeightedEvent(GvTScareTags::GhostScream(), 2);
		}

		if (Item->HasGhostTrait(EGvTItemGhostTrait::Valuable))
		{
			AddWeightedEvent(GvTScareTags::RearAudioSting(), 3);
			AddWeightedEvent(GvTScareTags::GhostScream(), 3);

			AddWeightedEvent(GvTScareTags::Mirror(), 3);
		}

		if (Item->HasGhostTrait(EGvTItemGhostTrait::Noisy))
		{
			AddWeightedEvent(GvTScareTags::RearAudioSting(), 5);
			AddWeightedEvent(GvTScareTags::GhostScream(), 3);

		}

		if (Item->HasGhostTrait(EGvTItemGhostTrait::Cursed))
		{
			AddWeightedEvent(GvTScareTags::GhostScream(), 4);
			AddWeightedEvent(GvTScareTags::CrawlerOverhead(), 3);

			AddWeightedEvent(GvTScareTags::Mirror(), 5);

		}

		if (Item->HasGhostTrait(EGvTItemGhostTrait::Religious))
		{
			AddWeightedEvent(GvTScareTags::GhostScream(), 4);
			AddWeightedEvent(GvTScareTags::LightChase(), 2);
			AddWeightedEvent(GvTScareTags::CrawlerOverhead(), 2);
		}

		if (Item->HasGhostTrait(EGvTItemGhostTrait::Historic))
		{
			AddWeightedEvent(GvTScareTags::RearAudioSting(), 4);
			AddWeightedEvent(GvTScareTags::GhostScream(), 2);
		}

		if (Item->HasGhostTrait(EGvTItemGhostTrait::Occult))
		{
			AddWeightedEvent(GvTScareTags::GhostScream(), 5);
			AddWeightedEvent(GvTScareTags::CrawlerOverhead(), 4);

			AddWeightedEvent(GvTScareTags::Mirror(), 4);

		}

		if (Item->HasGhostTrait(EGvTItemGhostTrait::MirrorBound))
		{
			AddWeightedEvent(GvTScareTags::Mirror(), 12);
		}

		if (Item->HasGhostTrait(EGvTItemGhostTrait::PersonalBelonging))
		{
			AddWeightedEvent(GvTScareTags::RearAudioSting(), 5);
			AddWeightedEvent(GvTScareTags::GhostScream(), 3);
		}

		if (Item->HasGhostTrait(EGvTItemGhostTrait::Possessed))
		{
			AddWeightedEvent(GvTScareTags::GhostScream(), 6);
			AddWeightedEvent(GvTScareTags::CrawlerOverhead(), 5);

		}
	}

	// ---------------------------------------------------------------------
	// Legacy trait compatibility
	// ---------------------------------------------------------------------

	if (bIsElectrical)
	{
		AddWeightedEvent(GvTScareTags::LightChase(), 3);
		AddWeightedEvent(GvTScareTags::GhostScream(), 2);
	}

	if (bIsValuable)
	{
		AddWeightedEvent(GvTScareTags::RearAudioSting(), 3);
		AddWeightedEvent(GvTScareTags::GhostScream(), 3);

		AddWeightedEvent(GvTScareTags::Mirror(), 2);
	}

	if (bIsNoisy)
	{
		AddWeightedEvent(GvTScareTags::RearAudioSting(), 4);
		AddWeightedEvent(GvTScareTags::GhostScream(), 2);
	}

	// ---------------------------------------------------------------------
	// General fallback events
	// ---------------------------------------------------------------------

	AddWeightedEvent(GvTScareTags::RearAudioSting(), 2);
	AddWeightedEvent(GvTScareTags::LightChase(), 1);
	AddWeightedEvent(GvTScareTags::GhostScream(), 2);

	AddWeightedEvent(GvTScareTags::Mirror(), 1);

	if (Panic >= GhostScareMinPanicThreshold01)
	{
		AddWeightedEvent(GvTScareTags::CrawlerOverhead(), 1);
	}


	// House Activity now changes the severity pool, not only the global timer.
	if (HouseActivity01 >= 0.25f)
	{
		AddWeightedEvent(GvTScareTags::GhostScream(), 2);
		AddWeightedEvent(GvTScareTags::Mirror(), 2);
	}
	if (HouseActivity01 >= 0.55f)
	{
		AddWeightedEvent(GvTScareTags::CrawlerOverhead(), 3);
		AddWeightedEvent(GvTScareTags::LightChase(), 1);
	}

	if (WeightedEvents.IsEmpty())
	{
		return;
	}

	// Try several distinct responses instead of allowing an unavailable choice to
	// make the theft feel ignored. Item-triggered events intentionally bypass the
	// organic panic gates; organic Director events retain the normal thresholds.
	TArray<FGameplayTag> AttemptOrder = WeightedEvents;
	for (int32 Index = AttemptOrder.Num() - 1; Index > 0; --Index)
	{
		AttemptOrder.Swap(Index, FMath::RandRange(0, Index));
	}

	TSet<FGameplayTag> AttemptedTags;
	for (const FGameplayTag& ChosenScare : AttemptOrder)
	{
		if (!ChosenScare.IsValid() || AttemptedTags.Contains(ChosenScare))
		{
			continue;
		}

		// Ordinary loot may provoke events and scares, but only the main objective
		// is allowed to directly start or strengthen a haunt.
		if (ChosenScare.MatchesTagExact(GvTScareTags::CrawlerChase()) || ChosenScare.MatchesTagExact(GvTScareTags::GhostHaunt_Chase()))
		{
			continue;
		}

		AttemptedTags.Add(ChosenScare);

		UE_LOG(LogTemp, Log, TEXT("[DirectorItemReaction] Item=%s Player=%s Event=%s Candidates=%d Panic=%.2f Activity=%.2f Attempt=%d"), *GetNameSafe(TargetActor), *GetNameSafe(Pawn), *ChosenScare.ToString(), WeightedEvents.Num(), Panic, HouseActivity01, AttemptedTags.Num());

		const bool bDispatched = DispatchScareEventSimple(ChosenScare, Pawn, TargetActor, Item != nullptr);
		UE_LOG(LogTemp, Log, TEXT("[ItemReactionResult] Item=%s Event=%s Forced=0 Attempt=%d Dispatched=%d"), *GetNameSafe(TargetActor), *ChosenScare.ToString(), AttemptedTags.Num(), bDispatched ? 1 : 0);

		if (bDispatched)
		{
			return;
		}
	}

	// Final guaranteed fallback uses the simplest client-local scare path.
	const bool bFallbackDispatched = DispatchScareEventSimple(GvTScareTags::RearAudioSting(), Pawn, TargetActor, true);
	UE_LOG(LogTemp, Warning, TEXT("[ItemReactionFallback] Item=%s Event=%s Dispatched=%d"), *GetNameSafe(TargetActor), *GvTScareTags::RearAudioSting().ToString(), bFallbackDispatched ? 1 : 0);

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

void UGvTDirectorSubsystem::SetDefaultHauntGhostClass(TSubclassOf<AGvTGhostCharacterBase> InGhostClass)
{
	DefaultHauntGhostClass = InGhostClass;

	UE_LOG(LogTemp, Warning,
		TEXT("[Director] DefaultHauntGhostClass set to %s"),
		*GetNameSafe(DefaultHauntGhostClass));
}

AGvTHauntGhostBase* UGvTDirectorSubsystem::FindActiveHauntGhost() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	for (TActorIterator<AGvTHauntGhostBase> It(World); It; ++It)
	{
		AGvTHauntGhostBase* Ghost = *It;

		if (!IsValid(Ghost))
		{
			continue;
		}

		if (Ghost->IsActorBeingDestroyed())
		{
			continue;
		}

		return Ghost;
	}

	return nullptr;
}

bool UGvTDirectorSubsystem::IsInPostHauntRecovery() const
{
	const UWorld* World = GetWorld();
	if (!World || PostHauntRecoveryDuration <= 0.0f)
	{
		return false;
	}

	const float Elapsed = World->GetTimeSeconds() - LastHauntEndTime;
	return Elapsed >= 0.f && Elapsed < PostHauntRecoveryDuration;
}

void UGvTDirectorSubsystem::NotifyMainObjectivePickedUp(APawn* CarrierPawn, AGvTInteractableItem* ObjectiveItem)
{
	UWorld* World = GetWorld();
	if (!World || !IsPawnEligibleForDirector(CarrierPawn) || !IsValid(ObjectiveItem) || !ObjectiveItem->IsMainObjective()) return;

	World->GetTimerManager().ClearTimer(TimerHandle_ObjectiveCursedHaunt);
	PendingObjectiveCarrier.Reset();
	PendingObjectiveItem.Reset();

	if (AGvTHauntGhostBase* ExistingHaunt = FindActiveHauntGhost())
	{
		if (ExistingHaunt->IsCursedHaunt()) ExistingHaunt->SetCursedFocusTarget(CarrierPawn);
		else ExistingHaunt->ActivateCursedHaunt(CarrierPawn, true);
		bObjectiveHasTriggeredCursedHaunt = true;
		return;
	}

	PendingObjectiveCarrier = CarrierPawn;
	PendingObjectiveItem = ObjectiveItem;
	const float Delay = bObjectiveHasTriggeredCursedHaunt
		? FMath::FRandRange(FMath::Max(0.f, RepeatObjectivePickupDelayMin), FMath::Max(RepeatObjectivePickupDelayMin, RepeatObjectivePickupDelayMax))
		: 0.f;
	if (Delay <= 0.f) ExecutePendingObjectiveCursedHaunt();
	else World->GetTimerManager().SetTimer(TimerHandle_ObjectiveCursedHaunt, this, &ThisClass::ExecutePendingObjectiveCursedHaunt, Delay, false);
}

void UGvTDirectorSubsystem::ExecutePendingObjectiveCursedHaunt()
{
	APawn* CarrierPawn = PendingObjectiveCarrier.Get();
	AGvTInteractableItem* ObjectiveItem = PendingObjectiveItem.Get();
	PendingObjectiveCarrier.Reset();
	PendingObjectiveItem.Reset();
	if (!IsPawnEligibleForDirector(CarrierPawn) || !IsValid(ObjectiveItem) || ObjectiveItem->GetCarrier() != CarrierPawn) return;

	if (AGvTHauntGhostBase* ExistingHaunt = FindActiveHauntGhost())
	{
		if (ExistingHaunt->IsCursedHaunt()) ExistingHaunt->SetCursedFocusTarget(CarrierPawn);
		else ExistingHaunt->ActivateCursedHaunt(CarrierPawn, true);
	}
	else if (DispatchScareEventSimple(GvTScareTags::GhostHaunt_Chase(), CarrierPawn, ObjectiveItem, true))
	{
		if (AGvTHauntGhostBase* SpawnedHaunt = FindActiveHauntGhost()) SpawnedHaunt->ActivateCursedHaunt(CarrierPawn, true);
	}
	bObjectiveHasTriggeredCursedHaunt = true;
}

void UGvTDirectorSubsystem::NotifyMainObjectiveDropped(APawn* PreviousCarrier, AGvTInteractableItem* ObjectiveItem)
{
	if (!IsValid(ObjectiveItem) || !ObjectiveItem->IsMainObjective()) return;
	if (UWorld* World = GetWorld()) World->GetTimerManager().ClearTimer(TimerHandle_ObjectiveCursedHaunt);
	PendingObjectiveCarrier.Reset();
	PendingObjectiveItem.Reset();
	if (AGvTHauntGhostBase* ExistingHaunt = FindActiveHauntGhost()) ExistingHaunt->ClearCursedFocusTarget(PreviousCarrier);
}

void UGvTDirectorSubsystem::NotifyMainObjectiveDeposited()
{
	if (UWorld* World = GetWorld()) World->GetTimerManager().ClearTimer(TimerHandle_ObjectiveCursedHaunt);
	PendingObjectiveCarrier.Reset();
	PendingObjectiveItem.Reset();
	if (AGvTHauntGhostBase* ExistingHaunt = FindActiveHauntGhost()) ExistingHaunt->ClearCursedFocusTarget();
}

float UGvTDirectorSubsystem::GetPostHauntRecoveryElapsed() const
{
	const UWorld* World = GetWorld();
	return World ? FMath::Max(0.f, World->GetTimeSeconds() - LastHauntEndTime) : BIG_NUMBER;
}

bool UGvTDirectorSubsystem::IsStrongRecoveryScare(const FGameplayTag& ScareTag) const
{
	return ScareTag.MatchesTagExact(GvTScareTags::Mirror())
		|| ScareTag.MatchesTagExact(GvTScareTags::GhostEvent_Mirror())
		|| ScareTag.MatchesTagExact(GvTScareTags::GhostScream())
		|| ScareTag.MatchesTagExact(GvTScareTags::GhostScare_Scream());
}

float UGvTDirectorSubsystem::GetRecoveryPanicMultiplier(const FGameplayTag& ScareTag) const
{
	return IsInPostHauntRecovery() ? FMath::Clamp(PostHauntPanicMultiplier, 0.f, 1.f) : 1.f;
}

float UGvTDirectorSubsystem::GetPerScareCooldown(const FGameplayTag& ScareTag) const
{
	if (ScareTag.MatchesTagExact(GvTScareTags::Mirror()) || ScareTag.MatchesTagExact(GvTScareTags::GhostEvent_Mirror()))
	{
		return MirrorRepeatCooldown;
	}

	if (ScareTag.MatchesTagExact(GvTScareTags::GhostScream()) || ScareTag.MatchesTagExact(GvTScareTags::GhostScare_Scream()))
	{
		return GhostScreamRepeatCooldown;
	}

	if (ScareTag.MatchesTagExact(GvTScareTags::DoorSlamBehind()))
	{
		return DoorScareRepeatCooldown;
	}

	return DefaultScareRepeatCooldown;
}

bool UGvTDirectorSubsystem::IsScareTagOnCooldown(const FGameplayTag& ScareTag) const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	const FGameplayTag CanonicalTag =
		(ScareTag.MatchesTagExact(GvTScareTags::GhostEvent_Mirror())) ? GvTScareTags::Mirror() :
		(ScareTag.MatchesTagExact(GvTScareTags::GhostScare_Scream())) ? GvTScareTags::GhostScream() : ScareTag;
	const float* LastTime = LastScareDispatchTimes.Find(CanonicalTag);
	if (!LastTime)
	{
		return false;
	}

	const float Elapsed = World->GetTimeSeconds() - *LastTime;
	return Elapsed >= 0.f && Elapsed < GetPerScareCooldown(CanonicalTag);
}

void UGvTDirectorSubsystem::RememberDispatchedScare(const FGameplayTag& ScareTag)
{
	if (!GetWorld())
	{
		return;
	}

	const FGameplayTag CanonicalTag =
		(ScareTag.MatchesTagExact(GvTScareTags::GhostEvent_Mirror())) ? GvTScareTags::Mirror() :
		(ScareTag.MatchesTagExact(GvTScareTags::GhostScare_Scream())) ? GvTScareTags::GhostScream() : ScareTag;
	LastScareDispatchTimes.Add(CanonicalTag, GetWorld()->GetTimeSeconds());
}

AGvTMirrorActor* UGvTDirectorSubsystem::FindEligibleMirrorForTarget(APawn* TargetPawn) const
{
	UWorld* World = GetWorld();
	if (!World || !IsValid(TargetPawn))
	{
		return nullptr;
	}

	FVector ViewLocation;
	FRotator ViewRotation;
	TargetPawn->GetActorEyesViewPoint(ViewLocation, ViewRotation);
	const FVector ViewForward = ViewRotation.Vector();

	AGvTMirrorActor* BestMirror = nullptr;
	float BestDot = 0.86f;

	for (TActorIterator<AGvTMirrorActor> It(World); It; ++It)
	{
		AGvTMirrorActor* Mirror = *It;
		if (!IsValid(Mirror))
		{
			continue;
		}

		const FVector ToMirror = Mirror->GetActorLocation() - ViewLocation;
		const float Distance = ToMirror.Size();
		if (Distance > 1500.f || Distance <= KINDA_SMALL_NUMBER)
		{
			continue;
		}

		const float Dot = FVector::DotProduct(ViewForward, ToMirror / Distance);
		if (Dot < BestDot)
		{
			continue;
		}

		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(GvT_DirectorMirrorVisibility), false, TargetPawn);
		Params.AddIgnoredActor(TargetPawn);
		const bool bHit = World->LineTraceSingleByChannel(Hit, ViewLocation, Mirror->GetActorLocation(), ECC_Visibility, Params);
		if (bHit && Hit.GetActor() != Mirror)
		{
			continue;
		}

		BestDot = Dot;
		BestMirror = Mirror;
	}

	return BestMirror;
}

bool UGvTDirectorSubsystem::IsAnyHauntActive() const
{
	return bHauntSpawnInProgress || IsValid(FindActiveHauntGhost());
}

FGvTScareEvent UGvTDirectorSubsystem::MakeCloseGhostScareEvent(AActor* Target) const
{
	FGvTScareEvent Event;
	Event.ScareTag = GvTScareTags::GhostScare_Close();
	Event.TargetActor = Target;
	Event.WorldHint = Target
		? Target->GetActorLocation()
		: FVector::ZeroVector;

	Event.Intensity01 = 1.0f;
	Event.Duration = 1.5f;
	Event.bTriggerLocalFlicker = false;
	Event.bTriggerGroupFlicker = false;
	Event.bAffectsPanic = true;
	Event.PanicAmount = CloseGhostScarePanicAmount;
	Event.LocalSeed = FMath::Rand();

	return Event;
}

bool UGvTDirectorSubsystem::IsPawnEligibleForDirector(
	const APawn* Pawn) const
{
	if (!IsValid(Pawn))
	{
		return false;
	}

	const AGvTThiefCharacter* Thief =
		Cast<AGvTThiefCharacter>(Pawn);

	if (!Thief || Thief->IsDead())
	{
		return false;
	}

	bool bFoundHouseBounds = false;
	if (!UGvTHouseBoundsLibrary::IsLocationInsideHouse(this, Pawn->GetActorLocation(), bFoundHouseBounds))
	{
		return false;
	}

	const AGvTPlayerState* PS =
		Pawn->GetPlayerState<AGvTPlayerState>();

	return PS && !PS->IsDeadForPanic();
}

void UGvTDirectorSubsystem::SetHauntExitDoorsLocked(bool bLocked)
{
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client)
	{
		return;
	}

	for (TActorIterator<AGvTDoorActor> It(World); It; ++It)
	{
		AGvTDoorActor* Door = *It;
		if (!IsValid(Door) || !Door->IsExitDoor())
		{
			continue;
		}

		if (bLocked)
		{
			Door->ApplyHauntExitLock();
		}
		else
		{
			Door->RemoveHauntExitLock();
		}
	}
}
