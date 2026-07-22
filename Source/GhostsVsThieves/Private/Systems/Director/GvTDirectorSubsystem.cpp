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
#include "NavigationSystem.h"
#include "Systems/GvTPowerBoxActor.h"
#include "Gameplay/Ghosts/GvTGhostCharacterBase.h"
#include "Gameplay/Ghosts/GvTHauntGhostBase.h"
#include "Gameplay/Ghosts/GvTGhostSpawnPoint.h"
#include "Gameplay/Ghosts/GvTGhostModelData.h"
#include "Gameplay/Ghosts/GvTGhostTypeData.h"
#include "Gameplay/Ghosts/GvTEventGhostBase.h"
#include "World/Items/GvTInteractableItem.h"
#include "Gameplay/Characters/Thieves/GvTThiefPerceptionComponent.h"

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
	const bool bHauntActive = IsAnyHauntActive();

	APawn* TargetPawn = Cast<APawn>(ChooseBestTarget());
	if (!IsPawnEligibleForDirector(TargetPawn))
	{
		return false;
	}

	const float Panic01 = GetPanicForPawn(TargetPawn);

	if (!bHauntActive && Panic01 >= HauntChasePanicThreshold01) 
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

	if (!bHauntActive && Panic01 >= GhostScareMinPanicThreshold01)
	{
		EligibleEvents.Add(MakeRearAudioStingEvent(TargetPawn));
		EligibleEvents.Add(MakeGhostScreamEvent(TargetPawn));

		if (!bHauntActive && FMath::FRand() <= CloseGhostScareSelectionChance)
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
	if (Panic01 >= HauntChasePanicThreshold01)
	{
		EligibleEvents.Add(MakeCrawlerChaseEvent(TargetPawn));
	}

	if (EligibleEvents.Num() == 0)
	{
		UE_LOG(LogTemp, Log, TEXT("[DirectorThreshold] No eligible auto scares for Target=%s Panic=%.2f"), *GetNameSafe(TargetPawn), Panic01);
		return false;
	}

	const int32 Index = FMath::RandRange(0, EligibleEvents.Num() - 1);
	const FGvTScareEvent Event = EligibleEvents[Index];

	const bool bDispatched = DispatchScareEvent(Event);
	if (bDispatched && GetWorld())
	{
		if (APawn* RememberPawn = Cast<APawn>(Event.TargetActor))
		{
			RememberTarget(RememberPawn);
		}

		LastGlobalHauntTime = GetWorld()->GetTimeSeconds();
	}
	if(bDispatched)
		ApplyHouseTensionImpulse(GetDispatchTensionImpulse(Event));

	UE_LOG(LogTemp, Log,
		TEXT("[DirectorThreshold] AutoScare Target=%s Tag=%s Panic=%.2f MirrorMin=%.2f ChaseMin=%.2f Dispatched=%d"),
		*GetNameSafe(Event.TargetActor),
		*Event.ScareTag.ToString(),
		Panic01,
		MirrorEventPanicThreshold01,
		HauntChasePanicThreshold01,
		bDispatched ? 1 : 0);

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
	PanicEvent.PanicDelta01 = FMath::Max(0.f, DoorSlamBehindPanicAmount) / 100.f;

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
	PanicEvent.PanicDelta01 = Event.PanicAmount / 100.f;
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
	PanicEvent.PanicDelta01 = HauntStartPanicAmount / 100.f;
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

	if (!Event.bIgnorePanicThreshold)
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

	const bool bIsHauntEvent =
		Event.ScareTag.MatchesTagExact(GvTScareTags::CrawlerChase()) ||
		Event.ScareTag.MatchesTagExact(GvTScareTags::GhostHaunt_Chase());

	const bool bIsPhysicalCloseScare =
		Event.ScareTag.MatchesTagExact(
			GvTScareTags::GhostScare_Close());

	if (IsAnyHauntActive() && (bIsHauntEvent || bIsPhysicalCloseScare))
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

		return true;
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

		ApplyDoorSlamPanicToNearbyPlayers(Door, Target, bSlammed);

		const float PanicRadius = FMath::Max(0.f, DoorSlamPanicRadius);

		const float TargetDistance = FVector::Dist(TargetPawn->GetActorLocation(), Door->GetActorLocation());

		const bool bTargetWithinPanicVicinity = TargetDistance <= PanicRadius;

		UE_LOG(LogTemp, Warning,
			TEXT("[Director] Dispatch DoorSlamBehind target=%s door=%s withinPanicVicinity=%d dist=%.1f radius=%.1f"),
			*GetNameSafe(Target),
			*GetNameSafe(Door),
			bTargetWithinPanicVicinity ? 1 : 0,
			TargetDistance,
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

	ApplyGlobalScarePanicToOtherPlayers(Event, Target);

	if (Event.ScareTag.MatchesTagExact(GvTScareTags::Mirror()) ||
		Event.ScareTag.MatchesTagExact(GvTScareTags::GhostEvent_Mirror()))
	{
		if (AGvTThiefCharacter* Thief = Cast<AGvTThiefCharacter>(Target))
		{
			Thief->Client_PlayGhostEvent(GvTScareTags::GhostEvent_Mirror());
			return true;
		}

		return false;
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

FTransform UGvTDirectorSubsystem::ChooseHauntSpawnTransform(APawn* TargetPawn, FGameplayTag HauntTag) const
{
	if (!TargetPawn)
	{
		return FTransform::Identity;
	}

	const FVector TargetLoc = TargetPawn->GetActorLocation();
	FVector SpawnLoc = TargetLoc - TargetPawn->GetActorForwardVector() * HauntSpawnIdealDistance + FVector(0.f, 0.f, 80.f);
	bool bUsedSpawnPoint = false;

	if (bUseGhostSpawnPoints)
	{
		AGvTGhostSpawnPoint* BestPoint = nullptr;
		float BestScore = -FLT_MAX;

		if (UWorld* World = GetWorld())
		{
			for (TActorIterator<AGvTGhostSpawnPoint> It(World); It; ++It)
			{
				AGvTGhostSpawnPoint* Point = *It;
				if (!Point || !Point->SupportsHauntTag(HauntTag))
				{
					continue;
				}

				const float Score = Point->ScoreForTarget(TargetPawn, HauntSpawnIdealDistance, HauntSpawnMinDistance, HauntSpawnMaxDistance);
				if (Score > BestScore)
				{
					BestScore = Score;
					BestPoint = Point;
				}
			}
		}

		if (BestPoint)
		{
			// Designers place spawn points on the floor. Character actor locations are capsule centers,
			// so add a small Z offset and do NOT let nav projection jump us to another floor.
			SpawnLoc = BestPoint->GetActorLocation() + FVector(0.f, 0.f, HauntSpawnPointZOffset);
			bUsedSpawnPoint = true;

			UE_LOG(LogTemp, Warning,
				TEXT("[GhostSpawn] Using spawn point=%s raw=%s final=%s tag=%s"),
				*GetNameSafe(BestPoint),
				*BestPoint->GetActorLocation().ToString(),
				*SpawnLoc.ToString(),
				*HauntTag.ToString());
		}
		else
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[GhostSpawn] No valid indoor spawn point found for target=%s tag=%s. Falling back behind target."),
				*GetNameSafe(TargetPawn),
				*HauntTag.ToString());
		}
	}

	if (!bUsedSpawnPoint)
	{
		if (UWorld* World = GetWorld())
		{
			if (UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World))
			{
				FNavLocation ProjectedLoc;
				if (NavSys->ProjectPointToNavigation(SpawnLoc, ProjectedLoc, FVector(300.f, 300.f, 600.f)))
				{
					SpawnLoc = ProjectedLoc.Location;
				}
			}

			FHitResult GroundHit;
			FCollisionQueryParams Params(SCENE_QUERY_STAT(GvT_HauntSpawnGroundSnap), false, TargetPawn);
			Params.AddIgnoredActor(TargetPawn);
			if (World->LineTraceSingleByChannel(
				GroundHit,
				SpawnLoc + FVector(0.f, 0.f, 600.f),
				SpawnLoc - FVector(0.f, 0.f, 1200.f),
				ECC_Visibility,
				Params))
			{
				SpawnLoc.Z = GroundHit.ImpactPoint.Z + HauntSpawnPointZOffset;
			}
		}
	}

	const FRotator SpawnRot = (TargetLoc - SpawnLoc).Rotation();
	return FTransform(SpawnRot, SpawnLoc);
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

	const FTransform SpawnTransform = ChooseHauntSpawnTransform(TargetPawn, HauntTag);

	FActorSpawnParameters Params;
	Params.Owner = TargetPawn;
	Params.Instigator = TargetPawn;
	Params.SpawnCollisionHandlingOverride =	ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	bHauntSpawnInProgress = true;

	AGvTGhostCharacterBase* Ghost =
		World->SpawnActor<AGvTGhostCharacterBase>(
			SpawnClass,
			SpawnTransform,
			Params);

	bHauntSpawnInProgress = false;

	if (!IsValid(Ghost))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[GhostHaunt] Failed to spawn haunt ghost class=%s."),
			*GetNameSafe(SpawnClass));

		return nullptr;
	}

	UE_LOG(LogTemp, Warning,
		TEXT("[GhostHaunt] Spawned single world haunt tag=%s Ghost=%s Target=%s Spawn=%s"),
		*HauntTag.ToString(),
		*GetNameSafe(Ghost),
		*GetNameSafe(TargetPawn),
		*Ghost->GetActorLocation().ToString());

	ActiveHauntGhostByTarget.Add(TargetKey, Ghost);

	SetHauntExitDoorsLocked(true);
	Ghost->BeginGhostHaunt(TargetPawn, HauntTag);

	ApplyHauntStartPanicToAllPlayers(Ghost, TargetPawn);

	return Ghost;
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
		return Panic01 >= GhostScareMinPanicThreshold01 && Panic01 <= 1.0f;
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
	if (!IsPawnEligibleForDirector(TargetPawn) || !IsValid(Door))
	{
		return -1.f;
	}

	if (!Door->CanTriggerScareSlam())
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

	return DistanceScore * DoorSlamDistanceWeight;
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

		// Every theft affects house tension even when the immediate reaction roll fails.
		ApplyHouseTensionImpulse(TensionImpulse);
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

	if (Item && Item->HasForcedGhostEvent())
	{
		const float ForcedEventRoll = FMath::FRand();
		const float ForcedEventChance = FMath::Clamp(Item->GetForcedGhostEventChance(), 0.0f, 1.0f);

		UE_LOG(LogTemp, Log, TEXT("[ItemForcedEvent] Item=%s Event=%s Chance=%.2f Roll=%.2f Result=%s"), *GetNameSafe(TargetActor), *Item->GetForcedGhostEvent().ToString(), ForcedEventChance, ForcedEventRoll, ForcedEventRoll <= ForcedEventChance ? TEXT("SUCCESS") : TEXT("FAILED"));

		if (ForcedEventRoll <= ForcedEventChance)
		{
			const bool bDispatched = DispatchScareEventSimple(Item->GetForcedGhostEvent(), Pawn, TargetActor, Item->ShouldForcedEventIgnorePanicThreshold());
			UE_LOG(LogTemp, Log, TEXT("[ItemReactionResult] Item=%s Event=%s Forced=1 Dispatched=%d"), *GetNameSafe(TargetActor), *Item->GetForcedGhostEvent().ToString(), bDispatched ? 1 : 0);

			if (bDispatched)
			{
				return;
			}
		}
	}

	if (Item && Item->ShouldForceHauntReaction() && !IsAnyHauntActive())
	{
		UE_LOG(
			LogTemp,
			Log,
			TEXT("[DirectorItemReaction] Main objective forced haunt. Item=%s Player=%s"),
			*GetNameSafe(TargetActor),
			*GetNameSafe(Pawn));

		const bool bDispatched = DispatchScareEventSimple(GvTScareTags::GhostHaunt_Chase(), Pawn, TargetActor, true);
		UE_LOG(LogTemp, Log, TEXT("[ItemReactionResult] Item=%s Event=%s Forced=1 Dispatched=%d"), *GetNameSafe(TargetActor), *GvTScareTags::GhostHaunt_Chase().ToString(), bDispatched ? 1 : 0);

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
			AddWeightedEvent(GvTScareTags::LightChase(), 6);
			AddWeightedEvent(GvTScareTags::GhostScream(), 2);
		}

		if (Item->HasGhostTrait(EGvTItemGhostTrait::Valuable))
		{
			AddWeightedEvent(GvTScareTags::RearAudioSting(), 3);
			AddWeightedEvent(GvTScareTags::GhostScream(), 3);

			if (Panic >= MirrorEventPanicThreshold01)
			{
				AddWeightedEvent(GvTScareTags::Mirror(), 3);
			}
		}

		if (Item->HasGhostTrait(EGvTItemGhostTrait::Noisy))
		{
			AddWeightedEvent(GvTScareTags::RearAudioSting(), 5);
			AddWeightedEvent(GvTScareTags::GhostScream(), 3);

			if (Panic >= HauntChasePanicThreshold01)
			{
				AddWeightedEvent(GvTScareTags::CrawlerChase(), 2);
			}
		}

		if (Item->HasGhostTrait(EGvTItemGhostTrait::Cursed))
		{
			AddWeightedEvent(GvTScareTags::GhostScream(), 4);
			AddWeightedEvent(GvTScareTags::CrawlerOverhead(), 3);

			if (Panic >= MirrorEventPanicThreshold01)
			{
				AddWeightedEvent(GvTScareTags::Mirror(), 5);
			}

			if (Panic >= HauntChasePanicThreshold01)
			{
				AddWeightedEvent(GvTScareTags::CrawlerChase(), 3);
			}
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

			if (Panic >= MirrorEventPanicThreshold01)
			{
				AddWeightedEvent(GvTScareTags::Mirror(), 4);
			}

			if (Panic >= HauntChasePanicThreshold01)
			{
				AddWeightedEvent(GvTScareTags::CrawlerChase(), 4);
			}
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

			if (Panic >= HauntChasePanicThreshold01)
			{
				AddWeightedEvent(GvTScareTags::CrawlerChase(), 5);
			}
		}
	}

	// ---------------------------------------------------------------------
	// Legacy trait compatibility
	// ---------------------------------------------------------------------

	if (bIsElectrical)
	{
		AddWeightedEvent(GvTScareTags::LightChase(), 5);
		AddWeightedEvent(GvTScareTags::GhostScream(), 2);
	}

	if (bIsValuable)
	{
		AddWeightedEvent(GvTScareTags::RearAudioSting(), 3);
		AddWeightedEvent(GvTScareTags::GhostScream(), 3);

		if (Panic >= MirrorEventPanicThreshold01)
		{
			AddWeightedEvent(GvTScareTags::Mirror(), 2);
		}
	}

	if (bIsNoisy)
	{
		AddWeightedEvent(GvTScareTags::RearAudioSting(), 4);
		AddWeightedEvent(GvTScareTags::GhostScream(), 2);

		if (Panic >= HauntChasePanicThreshold01)
		{
			AddWeightedEvent(GvTScareTags::CrawlerChase(), 2);
		}
	}

	// ---------------------------------------------------------------------
	// General fallback events
	// ---------------------------------------------------------------------

	AddWeightedEvent(GvTScareTags::RearAudioSting(), 2);
	AddWeightedEvent(GvTScareTags::LightChase(), 2);
	AddWeightedEvent(GvTScareTags::GhostScream(), 1);

	if (Panic >= MirrorEventPanicThreshold01)
	{
		AddWeightedEvent(GvTScareTags::Mirror(), 1);
	}

	if (Panic >= GhostScareMinPanicThreshold01)
	{
		AddWeightedEvent(GvTScareTags::CrawlerOverhead(), 1);
	}

	if (Panic >= HauntChasePanicThreshold01 && !IsAnyHauntActive())
	{
		AddWeightedEvent(GvTScareTags::CrawlerChase(), 1);
	}

	if (WeightedEvents.IsEmpty())
	{
		return;
	}

	const int32 SelectedIndex = FMath::RandRange(0, WeightedEvents.Num() - 1);

	const FGameplayTag ChosenScare = WeightedEvents[SelectedIndex];

	const bool bMirrorBoundOverride =
		Item
		&& Item->HasGhostTrait(EGvTItemGhostTrait::MirrorBound)
		&& ChosenScare.MatchesTagExact(GvTScareTags::Mirror());

	if (ChosenScare.MatchesTagExact(GvTScareTags::LightChase()) && bIsElectrical)
	{
		if (AGvTPowerBoxActor* Power = FindPowerBoxInWorld())
		{
			Power->ForcePowerStateFromGhost(
				EGvTHousePowerState::Off);
		}
	}

	UE_LOG(
		LogTemp,
		Log,
		TEXT("[DirectorItemReaction] Item=%s Player=%s Event=%s Candidates=%d Panic=%.2f Value01=%.2f MirrorOverride=%d"),
		*GetNameSafe(TargetActor),
		*GetNameSafe(Pawn),
		*ChosenScare.ToString(),
		WeightedEvents.Num(),
		Panic,
		ItemValue01,
		bMirrorBoundOverride ? 1 : 0);

	const bool bDispatched = DispatchScareEventSimple(ChosenScare, Pawn, TargetActor, bMirrorBoundOverride);
	UE_LOG(LogTemp, Log, TEXT("[ItemReactionResult] Item=%s Event=%s Forced=0 Candidates=%d Dispatched=%d"), *GetNameSafe(TargetActor), *ChosenScare.ToString(), WeightedEvents.Num(), bDispatched ? 1 : 0);
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