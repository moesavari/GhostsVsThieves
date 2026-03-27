#include "Gameplay/Scare/UGvTScareComponent.h"
#include "Gameplay/Ghosts/Mirror/GvTMirrorActor.h"
#include "Gameplay/Ghosts/Crawler/GvTCrawlerGhostCharacter.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameplayTagsManager.h"
#include "GvTPlayerState.h"
#include "Net/UnrealNetwork.h"
#include "Gameplay/Scare/GvTScareDefinition.h"
#include "Gameplay/Scare/UGvTGhostProfileAsset.h"
#include "Gameplay/Scare/UGvTScareSubsystem.h"
#include "Systems/Noise/GvTNoiseSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "Systems/Light/GvTLightFlickerSubsystem.h"
#include "Systems/Light/GvTLightFlickerTypes.h"
#include "Systems/Audio/GvTAmbientAudioDirector.h"
#include "Gameplay/Scare/GvTScareTags.h"
#include "Gameplay/Characters/Thieves/GvTThiefCharacter.h"
#include "DrawDebugHelpers.h"

namespace
{
	static AGvTAmbientAudioDirector* FindAmbientAudioDirector_Scare(const UObject* WorldContextObject) 
	{
		if (!WorldContextObject)
		{
			return nullptr;
		}

		return Cast<AGvTAmbientAudioDirector>(
			UGameplayStatics::GetActorOfClass(WorldContextObject, AGvTAmbientAudioDirector::StaticClass()));
	}
}

UGvTScareComponent::UGvTScareComponent()
{
	SetIsReplicatedByDefault(true);
	PrimaryComponentTick.bCanEverTick = false;
}

void UGvTScareComponent::BeginPlay()
{
	Super::BeginPlay();

	UE_LOG(LogTemp, Warning, TEXT("[Scare] BeginPlay. Owner=%s HasAuthority=%d NetMode=%d"),
		*GetNameSafe(GetOwner()),
		GetOwner() ? (int32)GetOwner()->HasAuthority() : -1,
		(int32)GetNetMode());

	if (IsServer())
	{
		if (ScareState.Seed == 0)
		{
			const int32 OwnerHash = GetOwner() ? int32(reinterpret_cast<uintptr_t>(GetOwner()) & 0x7fffffff) : 1337;
			const float Now = GetNowServerSeconds();
			ScareState.Seed = OwnerHash ^ int32(Now * 1000.f) ^ FMath::Rand();
		}

		const float Now = GetNowServerSeconds();
		ScareState.NextScareServerTime = Now + FMath::FRandRange(6.f, 10.f);
		ScareState.SafetyWindowEndTime = 0.f;
		ScareState.CooldownStacks = 0;
		ScareState.LastScareServerTime = 0.f;
		ScareState.LastPanicTier = 0;

		// Kill any old timer first, just in case PIE or stale BP defaults try funny business.
		if (GetWorld())
		{
			GetWorld()->GetTimerManager().ClearTimer(SchedulerTimer);
		}

		if (bEnableLegacyAutoScheduler)
		{
			GetWorld()->GetTimerManager().SetTimer(
				SchedulerTimer,
				this,
				&UGvTScareComponent::Server_SchedulerTick,
				SchedulerIntervalSeconds,
				true
			);

			UE_LOG(LogTemp, Warning, TEXT("[Scare] Legacy scheduler ENABLED for %s"), *GetNameSafe(GetOwner()));
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[Scare] Legacy scheduler DISABLED for %s"), *GetNameSafe(GetOwner()));
		}
	}

	// Client-only mirror reflect checks
	if (!IsServer() && bEnableReflectScare && GetNetMode() != NM_DedicatedServer)
	{
		GetWorld()->GetTimerManager().SetTimer(
			ReflectCheckTimer,
			this,
			&UGvTScareComponent::Client_ReflectTick,
			ReflectCheckInterval,
			true
		);
	}

	LifecycleState = EGvTScareLifecycleState::Idle;
	LocalScareActiveEndTime = 0.f;
	LocalScareRecoveryEndTime = 0.f;
}

void UGvTScareComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UGvTScareComponent, ScareState);
	DOREPLIFETIME(UGvTScareComponent, Panic);
}

void UGvTScareComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearLifecycleTimers();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TimerHandle_LightChaseStep);
		World->GetTimerManager().ClearTimer(TimerHandle_RearAudioFollowup);
	}

	Super::EndPlay(EndPlayReason);
}

void UGvTScareComponent::OnRep_Panic()
{
	UpdatePanicBand();
	UE_LOG(LogTemp, Warning, TEXT("[Panic] Owner=%s Panic=%.1f Band=%d"),
		*GetNameSafe(GetOwner()),
		Panic,
		(int32)CachedPanicBand);
}

void UGvTScareComponent::OnRep_ScareState()
{
	// Optional: client debug / UI
}

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

void UGvTScareComponent::ClearLifecycleTimers()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TimerHandle_ScareActiveFinish);
		World->GetTimerManager().ClearTimer(TimerHandle_ScareRecoveryFinish);
	}
}

void UGvTScareComponent::BeginLocalScareLifecycle(float ActiveDuration, float RecoveryDuration)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	UE_LOG(LogTemp, Warning,
		TEXT("[LifecycleTrace] Func=BeginLocalScareLifecycle Owner=%s State=%d ActiveEnd=%.2f RecoveryEnd=%.2f"),
		*GetNameSafe(GetOwner()),
		(int32)LifecycleState,
		LocalScareActiveEndTime,
		LocalScareRecoveryEndTime);

	if (LifecycleState != EGvTScareLifecycleState::Idle)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[ScareLifecycle] Begin ignored: Owner=%s is not idle (State=%d)"),
			*GetNameSafe(GetOwner()),
			(int32)LifecycleState);
		return;
	}

	ClearLifecycleTimers();

	const float SafeActiveDuration = FMath::Max(0.01f, ActiveDuration);
	const float SafeRecoveryDuration = FMath::Max(0.01f, RecoveryDuration);
	const float Now = World->GetTimeSeconds();

	LifecycleState = EGvTScareLifecycleState::Active;
	LocalScareActiveEndTime = Now + SafeActiveDuration;
	LocalScareRecoveryEndTime = LocalScareActiveEndTime + SafeRecoveryDuration;

	UE_LOG(LogTemp, Warning,
		TEXT("[ScareLifecycle] Begin Active Owner=%s ActiveDur=%.2f RecoveryDur=%.2f ActiveEnd=%.2f RecoveryEnd=%.2f"),
		*GetNameSafe(GetOwner()),
		SafeActiveDuration,
		SafeRecoveryDuration,
		LocalScareActiveEndTime,
		LocalScareRecoveryEndTime);

	World->GetTimerManager().SetTimer(
		TimerHandle_ScareActiveFinish,
		this,
		&UGvTScareComponent::HandleLifecycleActiveFinished,
		SafeActiveDuration,
		false);
}

void UGvTScareComponent::HandleLifecycleActiveFinished()
{
	UE_LOG(LogTemp, Warning,
		TEXT("[LifecycleTrace] Func=HandleLifecycleActiveFinished Owner=%s State=%d"),
		*GetNameSafe(GetOwner()),
		(int32)LifecycleState);

	if (LifecycleState != EGvTScareLifecycleState::Active)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[ScareLifecycle] Active finish ignored: Owner=%s State=%d"),
			*GetNameSafe(GetOwner()),
			(int32)LifecycleState);
		return;
	}

	EnterRecoveryState();
}

void UGvTScareComponent::EnterRecoveryState()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	UE_LOG(LogTemp, Warning,
		TEXT("[LifecycleTrace] Func=EnterRecoveryState Owner=%s State=%d Now=%.2f ActiveEnd=%.2f RecoveryEnd=%.2f"),
		*GetNameSafe(GetOwner()),
		(int32)LifecycleState,
		World->GetTimeSeconds(),
		LocalScareActiveEndTime,
		LocalScareRecoveryEndTime);

	if (LifecycleState != EGvTScareLifecycleState::Active)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[ScareLifecycle] EnterRecovery ignored: Owner=%s State=%d"),
			*GetNameSafe(GetOwner()),
			(int32)LifecycleState);
		return;
	}

	World->GetTimerManager().ClearTimer(TimerHandle_ScareActiveFinish);

	LifecycleState = EGvTScareLifecycleState::Recovering;

	const float RemainingRecovery = FMath::Max(0.01f, LocalScareRecoveryEndTime - World->GetTimeSeconds());

	UE_LOG(LogTemp, Warning,
		TEXT("[ScareLifecycle] Begin Recovery Owner=%s Remaining=%.2f RecoveryEnd=%.2f"),
		*GetNameSafe(GetOwner()),
		RemainingRecovery,
		LocalScareRecoveryEndTime);

	World->GetTimerManager().SetTimer(
		TimerHandle_ScareRecoveryFinish,
		this,
		&UGvTScareComponent::HandleLifecycleRecoveryFinished,
		RemainingRecovery,
		false);
}

void UGvTScareComponent::HandleLifecycleRecoveryFinished()
{
	UE_LOG(LogTemp, Warning,
		TEXT("[LifecycleTrace] Func=HandleLifecycleRecoveryFinished Owner=%s State=%d"),
		*GetNameSafe(GetOwner()),
		(int32)LifecycleState);

	if (LifecycleState != EGvTScareLifecycleState::Recovering)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[ScareLifecycle] Recovery finish ignored: Owner=%s State=%d"),
			*GetNameSafe(GetOwner()),
			(int32)LifecycleState);
		return;
	}

	EndScareLifecycle();
}

void UGvTScareComponent::EndScareLifecycle()
{
	UE_LOG(LogTemp, Warning,
		TEXT("[LifecycleTrace] Func=EndScareLifecycle Owner=%s State=%d"),
		*GetNameSafe(GetOwner()),
		(int32)LifecycleState);

	if (LifecycleState == EGvTScareLifecycleState::Idle)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[ScareLifecycle] End ignored: Owner=%s already idle"),
			*GetNameSafe(GetOwner()));
		return;
	}

	ClearLifecycleTimers();

	LifecycleState = EGvTScareLifecycleState::Idle;
	LocalScareActiveEndTime = 0.f;
	LocalScareRecoveryEndTime = 0.f;

	UE_LOG(LogTemp, Warning,
		TEXT("[ScareLifecycle] End Idle Owner=%s"),
		*GetNameSafe(GetOwner()));
}

void UGvTScareComponent::RequestMirrorScare(float Intensity01, float LifeSeconds)
{
	if (!CanStartNewScare())
	{
		UE_LOG(LogTemp, Warning, TEXT("[ScareLifecycle] Mirror request ignored: owner %s is busy"), *GetNameSafe(GetOwner()));
		return;
	}

	if (IsServer())
	{
		Client_PlayMirrorScare(Intensity01, LifeSeconds);
	}
	else
	{
		Test_MirrorScare(Intensity01, LifeSeconds);
	}
}

void UGvTScareComponent::RequestCrawlerChaseFromEvent(AActor* Victim)
{
	if (!Victim)
	{
		return;
	}

	if (Victim == GetOwner() && !CanStartNewScare())
	{
		UE_LOG(LogTemp, Warning, TEXT("[ScareLifecycle] Chase ignored: Owner=%s already busy"), *GetNameSafe(GetOwner()));
		return;
	}

	RequestCrawlerChaseScare(Victim);
}

void UGvTScareComponent::RequestCrawlerOverheadFromEvent(const FGvTScareEvent& Event)
{
	if (!IsServer())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Scare] CrawlerOverhead ignored: RequestCrawlerOverheadFromEvent must run on server."));
		return;
	}

	AGvTThiefCharacter* VictimThief = Cast<AGvTThiefCharacter>(Event.TargetActor);
	if (!VictimThief)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[Scare] CrawlerOverhead failed: Event.TargetActor %s is not a thief."),
			*GetNameSafe(Event.TargetActor));
		return;
	}

	UGvTScareComponent* VictimScare = VictimThief->FindComponentByClass<UGvTScareComponent>();
	if (!VictimScare)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[Scare] CrawlerOverhead failed: victim %s has no scare component."),
			*GetNameSafe(VictimThief));
		return;
	}

	UE_LOG(LogTemp, Warning,
		TEXT("[ScareRoute] RequestOwner=%s Victim=%s VictimScareOwner=%s VictimLocal=%d"),
		*GetNameSafe(GetOwner()),
		*GetNameSafe(VictimThief),
		*GetNameSafe(VictimScare->GetOwner()),
		VictimThief->IsLocallyControlled() ? 1 : 0);

	if (VictimScare->IsScareBusy())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[ScareLifecycle] CrawlerOverhead skipped: victim %s is busy"),
			*GetNameSafe(VictimThief));
		return;
	}

	VictimThief->ApplyScareStun(0.6f);
	VictimThief->Client_PlayLocalCrawlerOverheadScare(Event);
}

void UGvTScareComponent::RequestCrawlerChaseScare(AActor* Victim)
{
	if (!Victim)
	{
		return;
	}

	Server_RequestCrawlerChaseScare(Victim);
}

void UGvTScareComponent::RequestCrawlerOverheadScare(AActor* Victim, bool bVictimOnly)
{
	if (!Victim)
	{
		return;
	}

	Server_RequestCrawlerOverheadScare(Victim, bVictimOnly);
}

void UGvTScareComponent::RequestLightChaseFromEvent(const FGvTScareEvent& Event)
{
	if (!IsServer())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Scare] LightChase ignored: RequestLightChaseFromEvent must run on server."));
		return;
	}

	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	APawn* VictimPawn = Cast<APawn>(Event.TargetActor);

	if (!OwnerPawn || !VictimPawn)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Scare] LightChase failed: invalid owner/victim."));
		return;
	}

	if (OwnerPawn != VictimPawn)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[Scare] LightChase failed: component owner %s does not match victim %s."),
			*GetNameSafe(OwnerPawn),
			*GetNameSafe(VictimPawn));
		return;
	}

	Client_PlayScare(Event);
}

void UGvTScareComponent::RequestRearAudioStingFromEvent(const FGvTScareEvent& Event)
{
	if (!IsServer())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Scare] RearAudioSting ignored: must run on server."));
		return;
	}

	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	APawn* VictimPawn = Cast<APawn>(Event.TargetActor);

	if (!OwnerPawn || !VictimPawn || OwnerPawn != VictimPawn)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[Scare] RearAudioSting failed: invalid owner/victim. Owner=%s Victim=%s"),
			*GetNameSafe(OwnerPawn),
			*GetNameSafe(VictimPawn));
		return;
	}

	Client_PlayScare(Event);
}

void UGvTScareComponent::RequestGhostScreamFromEvent(const FGvTScareEvent& Event)
{
	if (!IsServer())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Scare] GhostScream ignored: must run on server."));
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	APawn* TargetPawn = Cast<APawn>(Event.TargetActor);
	if (!TargetPawn)
	{
		return;
	}

	TArray<AActor*> Thieves;
	UGameplayStatics::GetAllActorsOfClass(World, AGvTThiefCharacter::StaticClass(), Thieves);

	const float RadiusSq = FMath::Square(FMath::Max(0.f, Event.SharedAudioRadius));

	for (AActor* Actor : Thieves)
	{
		AGvTThiefCharacter* Thief = Cast<AGvTThiefCharacter>(Actor);
		if (!Thief)
		{
			continue;
		}

		if (RadiusSq > 0.f && FVector::DistSquared(Thief->GetActorLocation(), Event.WorldHint) > RadiusSq)
		{
			continue;
		}

		if (UGvTScareComponent* ListenerScareComp = Thief->FindComponentByClass<UGvTScareComponent>())
		{
			ListenerScareComp->Client_PlayScare(Event);
		}
	}
}

void UGvTScareComponent::AddPanic(float Amount)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		return;
	}

	Panic = FMath::Clamp(Panic + Amount, 0.f, PanicMax);
	UpdatePanicBand();
	OnRep_Panic();
}

void UGvTScareComponent::ReducePanic(float Amount)
{
	if (GetOwnerRole() != ROLE_Authority)
	{
		return;
	}

	Panic = FMath::Clamp(Panic - Amount, 0.f, PanicMax);
	UpdatePanicBand();
	OnRep_Panic();
}

float UGvTScareComponent::GetPanicNormalized() const
{
	return (PanicMax > 0.f) ? (Panic / PanicMax) : 0.f;
}

EGvTPanicBand UGvTScareComponent::GetPanicBand() const
{
	return CachedPanicBand;
}

void UGvTScareComponent::PlayLocalCrawlerOverheadScare(const FGvTScareEvent& Event)
{
	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	APawn* EventVictimPawn = Cast<APawn>(Event.TargetActor);

	UE_LOG(LogTemp, Warning,
		TEXT("[ScareVictimCheck] Owner=%s EventVictim=%s OwnerLocal=%d"),
		*GetNameSafe(OwnerPawn),
		*GetNameSafe(EventVictimPawn),
		OwnerPawn ? (int32)OwnerPawn->IsLocallyControlled() : -1);

	if (!OwnerPawn || !EventVictimPawn)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Scare] PlayLocalCrawlerOverheadScare aborted: invalid owner/victim"));
		return;
	}

	if (OwnerPawn != EventVictimPawn)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[Scare] PlayLocalCrawlerOverheadScare aborted: Owner=%s does not match EventVictim=%s"),
			*GetNameSafe(OwnerPawn),
			*GetNameSafe(EventVictimPawn));
		return;
	}

	if (!OwnerPawn->IsLocallyControlled())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[Scare] PlayLocalCrawlerOverheadScare aborted: %s is not locally controlled"),
			*GetNameSafe(OwnerPawn));
		return;
	}

	if (!CanStartNewScare())
	{
		UE_LOG(LogTemp, Warning, TEXT("[ScareLifecycle] CrawlerOverhead ignored: Owner=%s already busy"), *GetNameSafe(GetOwner()));
		return;
	}

	const float ActiveDuration = Event.Duration > 0.f ? Event.Duration : 0.6f;
	BeginLocalScareLifecycle(ActiveDuration, OverheadRecoveryDuration);

	SpawnLocalCrawlerOverheadGhost(EventVictimPawn);

	UE_LOG(LogTemp, Warning, TEXT("[Scare] PlayLocalCrawlerOverheadScare local victim = %s"),
		*GetNameSafe(EventVictimPawn));
}

void UGvTScareComponent::PlayLocalLightChase(const FGvTScareEvent& Event)
{
	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	APawn* EventVictimPawn = Cast<APawn>(Event.TargetActor);

	if (!OwnerPawn || !EventVictimPawn)
	{
		return;
	}

	if (OwnerPawn != EventVictimPawn)
	{
		return;
	}

	if (!OwnerPawn->IsLocallyControlled())
	{
		return;
	}

	if (!CanStartNewScare())
	{
		UE_LOG(LogTemp, Warning, TEXT("[ScareLifecycle] LightChase ignored: Owner=%s already busy"), *GetNameSafe(GetOwner()));
		return;
	}

	BeginLocalLightChaseSequence(Event);
}

void UGvTScareComponent::PlayLocalRearAudioSting(const FGvTScareEvent& Event)
{
	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	APawn* EventVictimPawn = Cast<APawn>(Event.TargetActor);

	if (!OwnerPawn || !EventVictimPawn || OwnerPawn != EventVictimPawn)
	{
		return;
	}

	if (!OwnerPawn->IsLocallyControlled())
	{
		return;
	}

	if (!CanStartNewScare())
	{
		UE_LOG(LogTemp, Warning, TEXT("[ScareLifecycle] RearAudioSting ignored: Owner=%s already busy"), *GetNameSafe(GetOwner()));
		return;
	}

	const FVector SfxLoc = BuildRearAudioWorldLocation(Event);

	if (RearAudioStingSfx)
	{
		UGameplayStatics::PlaySoundAtLocation(this, RearAudioStingSfx, SfxLoc, 1.0f, 1.0f);
	}

	const float ActiveDuration = FMath::Max(0.08f, Event.Duration);
	BeginLocalScareLifecycle(ActiveDuration, RearAudioRecoveryDuration);

	if (Event.bTwoShotAudio && RearAudioStingFollowupSfx && GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(TimerHandle_RearAudioFollowup);

		FTimerDelegate FollowupDelegate;
		FollowupDelegate.BindWeakLambda(this, [this, Event]()
			{
				if (!GetWorld())
				{
					return;
				}

				const FVector FollowupLoc = BuildRearAudioWorldLocation(Event);
				UGameplayStatics::PlaySoundAtLocation(this, RearAudioStingFollowupSfx, FollowupLoc, 0.95f, 1.03f);
			});

		GetWorld()->GetTimerManager().SetTimer(
			TimerHandle_RearAudioFollowup,
			FollowupDelegate,
			FMath::Max(0.01f, Event.FollowupDelay),
			false);
	}
}

void UGvTScareComponent::PlayLocalGhostScreamShared(const FGvTScareEvent& Event)
{
	if (GhostScreamSfx)
	{
		UGameplayStatics::PlaySoundAtLocation(
			this,
			GhostScreamSfx,
			Event.WorldHint,
			1.0f,
			FMath::FRandRange(0.97f, 1.03f));
	}

	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	APawn* TargetPawn = Cast<APawn>(Event.TargetActor);

	if (!OwnerPawn || !TargetPawn || OwnerPawn != TargetPawn)
	{
		return; // listeners hear it, but do not enter scare lifecycle
	}

	if (!OwnerPawn->IsLocallyControlled())
	{
		return;
	}

	if (!CanStartNewScare())
	{
		return;
	}

	BeginLocalScareLifecycle(FMath::Max(0.10f, Event.Duration), GhostScreamRecoveryDuration);
}

void UGvTScareComponent::BeginLocalLightChaseSequence(const FGvTScareEvent& Event)
{
	bActiveLightChaseAffectedAnyLights = false;

	if (!GetWorld())
	{
		return;
	}

	ActiveLightChaseEvent = Event;

	const TArray<FVector> RawStepLocations = BuildLightChaseStepLocations(Event);

	ActiveLightChaseStepLocations.Reset();
	ActiveLightChaseStepLocations.Reserve(RawStepLocations.Num());

	for (const FVector& RawLoc : RawStepLocations)
	{
		const FVector SnappedLoc = ResolveLightChaseStepLocation(RawLoc);

#if !(UE_BUILD_SHIPPING)
		DrawDebugSphere(GetWorld(), RawLoc, 18.f, 8, FColor::Cyan, false, 2.0f, 0, 1.5f);
		DrawDebugSphere(GetWorld(), SnappedLoc, 24.f, 10, FColor::Yellow, false, 2.0f, 0, 2.0f);
		DrawDebugLine(GetWorld(), RawLoc, SnappedLoc, FColor::Green, false, 2.0f, 0, 1.0f);
#endif

		ActiveLightChaseStepLocations.Add(SnappedLoc);
	}

	CollapseLightChaseStepLocations(ActiveLightChaseStepLocations);

	ActiveLightChaseStepIndex = 0;
	bLightChaseSequenceActive = ActiveLightChaseStepLocations.Num() > 0;

	if (!bLightChaseSequenceActive)
	{
		return;
	}

	const float ActiveDuration = Event.Duration > 0.f
		? Event.Duration
		: ((Event.LightChaseStepCount * Event.LightChaseStepInterval) + 0.25f);

	BeginLocalScareLifecycle(ActiveDuration, LightChaseRecoveryDuration);

	GetWorld()->GetTimerManager().ClearTimer(TimerHandle_LightChaseStep);
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
		EndLocalLightChaseSequence();
		return;
	}

	const bool bIsFinalStep = (ActiveLightChaseStepIndex == ActiveLightChaseStepLocations.Num() - 1);
	const FVector StepLocation = ActiveLightChaseStepLocations[ActiveLightChaseStepIndex];

	DrawDebugSphere(
		GetWorld(),
		StepLocation,
		24.f,
		12,
		bIsFinalStep ? FColor::Red : FColor::Yellow,
		false,
		1.0f,
		0,
		2.0f);

	PlayLightChaseStepEffects(StepLocation, bIsFinalStep);

	++ActiveLightChaseStepIndex;

	if (ActiveLightChaseStepLocations.IsValidIndex(ActiveLightChaseStepIndex))
	{
		const float Interval = FMath::Max(0.01f, ActiveLightChaseEvent.LightChaseStepInterval);

		GetWorld()->GetTimerManager().SetTimer(
			TimerHandle_LightChaseStep,
			this,
			&UGvTScareComponent::AdvanceLocalLightChaseSequence,
			Interval,
			false);
	}
	else
	{
		EndLocalLightChaseSequence();
	}
}

void UGvTScareComponent::EndLocalLightChaseSequence()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(TimerHandle_LightChaseStep);
	}

	bLightChaseSequenceActive = false;
	ActiveLightChaseStepLocations.Reset();
	ActiveLightChaseStepIndex = 0;

	const bool bSucceeded = bActiveLightChaseAffectedAnyLights;
	const float PanicToApply = (bSucceeded && ActiveLightChaseEvent.bAffectsPanic)
		? ActiveLightChaseEvent.PanicAmount
		: 0.f;

	Server_ReportLightChaseResult(bSucceeded, PanicToApply);
}

TArray<FVector> UGvTScareComponent::BuildLightChaseStepLocations(const FGvTScareEvent& Event) const
{
	TArray<FVector> OutLocations;

	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!OwnerPawn)
	{
		return OutLocations;
	}

	FVector Forward = FVector::ZeroVector;

	if (/*const APawn* */OwnerPawn == Cast<APawn>(GetOwner()))
	{
		if (const AController* Controller = OwnerPawn->GetController())
		{
			const FRotator ControlRot = Controller->GetControlRotation();
			Forward = FRotationMatrix(ControlRot).GetUnitAxis(EAxis::X);
		}

		if (Forward.IsNearlyZero())
		{
			Forward = OwnerPawn->GetActorForwardVector();
		}
	}

	Forward.Z = 0.f;
	Forward = Forward.GetSafeNormal();

	if (Forward.IsNearlyZero())
	{
		Forward = FVector::ForwardVector;
	}

	const FVector Origin = OwnerPawn->GetActorLocation();
	const int32 StepCount = FMath::Max(2, Event.LightChaseStepCount);

	for (int32 StepIdx = 0; StepIdx < StepCount; ++StepIdx)
	{
		const float Alpha = (StepCount > 1) ? (float)StepIdx / (float)(StepCount - 1) : 1.0f;
		const float Dist = FMath::Lerp(Event.LightChaseStartDistance, Event.LightChaseEndDistance, Alpha);

		FVector StepLoc = Origin + (Forward * Dist);
		StepLoc.Z = Origin.Z + 50.f;

		OutLocations.Add(StepLoc);
	}

	return OutLocations;
}

FVector UGvTScareComponent::BuildRearAudioWorldLocation(const FGvTScareEvent& Event) const
{
	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!OwnerPawn)
	{
		return FVector::ZeroVector;
	}

	FVector ViewLoc = OwnerPawn->GetActorLocation() + FVector(0.f, 0.f, 60.f);
	FRotator ViewRot = OwnerPawn->GetActorRotation();

	if (const APlayerController* PC = Cast<APlayerController>(OwnerPawn->GetController()))
	{
		PC->GetPlayerViewPoint(ViewLoc, ViewRot);
	}
	else if (const AController* Controller = OwnerPawn->GetController())
	{
		ViewRot = Controller->GetControlRotation();
	}

	FVector Forward = FRotationMatrix(ViewRot).GetUnitAxis(EAxis::X);
	FVector Right = FRotationMatrix(ViewRot).GetUnitAxis(EAxis::Y);

	Forward.Z = 0.f;
	Right.Z = 0.f;
	Forward = Forward.GetSafeNormal();
	Right = Right.GetSafeNormal();

	if (Forward.IsNearlyZero())
	{
		Forward = OwnerPawn->GetActorForwardVector();
		Forward.Z = 0.f;
		Forward = Forward.GetSafeNormal();
	}

	if (Right.IsNearlyZero())
	{
		Right = OwnerPawn->GetActorRightVector();
		Right.Z = 0.f;
		Right = Right.GetSafeNormal();
	}

	FRandomStream Stream(Event.LocalSeed != 0 ? Event.LocalSeed : 1337);
	const float SideSign = (Stream.FRand() < 0.5f) ? 1.f : -1.f;

	return ViewLoc
		- (Forward * Event.RearAudioBackOffset)
		+ (Right * Event.RearAudioSideOffset * SideSign)
		+ FVector(0.f, 0.f, Event.RearAudioUpOffset);
}

void UGvTScareComponent::PlayLightChaseStepEffects(const FVector& StepLocation, bool bIsFinalStep)
{
	const int32 StepCount = FMath::Max(1, ActiveLightChaseStepLocations.Num());
	const float StepAlpha = (StepCount > 1)
		? (float)ActiveLightChaseStepIndex / (float)(StepCount - 1)
		: 1.0f;

	FGvTLightFlickerEvent FlickerEvent;
	FlickerEvent.WorldCenter = StepLocation;
	FlickerEvent.Radius = FMath::Max(100.f, ActiveLightChaseEvent.LightChaseFlickerRadius);
	FlickerEvent.Duration = bIsFinalStep ? 0.22f : 0.14f;
	FlickerEvent.Intensity01 = FMath::Clamp(ActiveLightChaseEvent.Intensity01 + (StepAlpha * 0.20f), 0.f, 1.f);
	FlickerEvent.bWholeHouse = false;
	FlickerEvent.bAllowFullOffBursts = true;
	FlickerEvent.MinDimAlpha = bIsFinalStep ? 0.00f : 0.08f;
	FlickerEvent.MaxDimAlpha = 1.0f;
	FlickerEvent.PulseIntervalMin = bIsFinalStep ? 0.02f : 0.03f;
	FlickerEvent.PulseIntervalMax = bIsFinalStep ? 0.06f : 0.08f;
	FlickerEvent.Seed = ActiveLightChaseEvent.LocalSeed + (ActiveLightChaseStepIndex * 31);

	PlayLocalLightFlicker(FlickerEvent);

	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	FVector AudioLoc = StepLocation;

	if (OwnerPawn)
	{
		const FVector TowardVictim = (OwnerPawn->GetActorLocation() - StepLocation).GetSafeNormal();
		AudioLoc += TowardVictim * ActiveLightChaseEvent.LightChaseAudioLeadDistance;
	}

	if (UWorld* World = GetWorld())
	{
		if (UGvTLightFlickerSubsystem* FlickerSubsystem = World->GetSubsystem<UGvTLightFlickerSubsystem>())
		{
			const int32 NumNearbyLights = FlickerSubsystem->CountLightsInRadius(
				StepLocation,
				FlickerEvent.Radius);

			if (NumNearbyLights > 0)
			{
				bActiveLightChaseAffectedAnyLights = true;
			}
		}
	}

	PlayLocalLightChaseSoundAtLocation(AudioLoc, bIsFinalStep, StepAlpha);
}

void UGvTScareComponent::PlayLocalLightChaseSoundAtLocation(const FVector& WorldLocation, bool bIsFinalStep, float StepAlpha) const
{
	USoundBase* ChosenSound = bIsFinalStep ? LightChaseFinalSfx : LightChaseStepSfx;
	if (!ChosenSound)
	{
		return;
	}

	const float Volume = FMath::Lerp(0.85f, 1.20f, FMath::Clamp(StepAlpha, 0.f, 1.f));
	const float Pitch = FMath::Lerp(0.96f, 1.06f, FMath::Clamp(StepAlpha, 0.f, 1.f));

	UGameplayStatics::PlaySoundAtLocation(
		this,
		ChosenSound,
		WorldLocation,
		Volume,
		Pitch);
}

void UGvTScareComponent::Test_MirrorScare(float Intensity01, float LifeSeconds)
{
	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!OwnerPawn || !OwnerPawn->IsLocallyControlled())
	{
		return;
	}

	APlayerController* PC = Cast<APlayerController>(OwnerPawn->GetController());
	if (!PC)
	{
		return;
	}

	FVector CamLoc;
	FRotator CamRot;
	PC->GetPlayerViewPoint(CamLoc, CamRot);

	const FVector Start = CamLoc;
	const FVector End = Start + (CamRot.Vector() * ReflectTraceDistance);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(GvT_MirrorTestTrace), false, OwnerPawn);
	Params.AddIgnoredActor(OwnerPawn);

	FHitResult Hit;
	const bool bHit = GetWorld()->SweepSingleByChannel(
		Hit,
		Start,
		End,
		FQuat::Identity,
		ECC_Visibility,
		FCollisionShape::MakeSphere(ReflectSphereRadius),
		Params);

	if (!bHit)
	{
		return;
	}

	AGvTMirrorActor* Mirror = Cast<AGvTMirrorActor>(Hit.GetActor());
	if (!Mirror)
	{
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[MirrorTest] Triggering mirror %s"), *Mirror->GetName());

	if (IsServer())
	{
		Mirror->TriggerScare(Intensity01, LifeSeconds);
	}
	else
	{
		Server_RequestMirrorActorScare(Mirror, Intensity01, LifeSeconds);
	}
}

void UGvTScareComponent::Debug_RequestCrawlerChase(APawn* Victim)
{
	if (!Victim)
	{
		return;
	}

	RequestCrawlerChaseScare(Victim);
}

void UGvTScareComponent::Debug_RequestGroupHouseLightFlicker(float Intensity01, float Duration)
{
	if (!IsServer())
	{
		return;
	}

	FGvTLightFlickerEvent Event = MakeLightFlickerEvent(Intensity01, Duration, true);

	if (UWorld* World = GetWorld())
	{
		TArray<AActor*> Thieves;
		UGameplayStatics::GetAllActorsOfClass(World, AGvTThiefCharacter::StaticClass(), Thieves);

		for (AActor* A : Thieves)
		{
			if (AGvTThiefCharacter* T = Cast<AGvTThiefCharacter>(A))
			{
				if (UGvTScareComponent* C = T->FindComponentByClass<UGvTScareComponent>())
				{
					C->Client_PlayLightFlicker(Event);
				}
			}
		}
	}
}

void UGvTScareComponent::Debug_RequestLocalHouseLightFlicker(float Intensity01, float Duration)
{
	APawn* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn || !Pawn->IsLocallyControlled())
	{
		return;
	}

	const FGvTLightFlickerEvent Event = MakeLightFlickerEvent(Intensity01, Duration, true);
	PlayLocalLightFlicker(Event);
}

void UGvTScareComponent::Debug_RequestLightChase(float Intensity01)
{
	APawn* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn || !Pawn->IsLocallyControlled())
	{
		return;
	}

	FGvTScareEvent Event;
	Event.ScareTag = GvTScareTags::LightChase();
	Event.TargetActor = Pawn;
	Event.Intensity01 = FMath::Clamp(Intensity01, 0.f, 1.f);
	Event.LightChaseStepCount = 6;
	Event.LightChaseStepInterval = 0.10f;
	Event.LightChaseStartDistance = 900.f;
	Event.LightChaseEndDistance = 100.f;
	Event.LightChaseFlickerRadius = 650.f;
	Event.LightChaseAudioLeadDistance = 50.f;
	Event.Duration = (Event.LightChaseStepCount * Event.LightChaseStepInterval) + 0.25f;

	Event.bAffectsPanic = true;
	Event.PanicAmount = 7.0f; 

	Event.LocalSeed = MakeLocalSeed(GetNowServerSeconds());

	PlayLocalLightChase(Event);
}

void UGvTScareComponent::Client_PlayMirrorScare_Implementation(float Intensity01, float LifeSeconds)
{
	if (!CanStartNewScare())
	{
		UE_LOG(LogTemp, Warning, TEXT("[ScareLifecycle] Mirror ignored: Owner=%s already busy"), *GetNameSafe(GetOwner()));
		return;
	}

	BeginLocalScareLifecycle(LifeSeconds, MirrorRecoveryDuration);
	Test_MirrorScare(Intensity01, LifeSeconds);
}

void UGvTScareComponent::Client_PlayScare_Implementation(const FGvTScareEvent& Event)
{
	static const FGameplayTag LightFlickerTag = FGameplayTag::RequestGameplayTag(TEXT("Scare.LightFlicker"));

	if (Event.ScareTag.MatchesTagExact(GvTScareTags::LightChase()))
	{
		PlayLocalLightChase(Event);
		return;
	}

	if (Event.ScareTag.MatchesTagExact(GvTScareTags::RearAudioSting()))
	{
		PlayLocalRearAudioSting(Event);
		return;
	}

	if (Event.ScareTag.MatchesTagExact(GvTScareTags::GhostScream()))
	{
		PlayLocalGhostScreamShared(Event);
		return;
	}

	if (Event.ScareTag.MatchesTagExact(LightFlickerTag))
	{
		FGvTLightFlickerEvent Flicker;
		Flicker.WorldCenter = Event.WorldHint;
		Flicker.Radius = 8000.f;
		Flicker.Duration = FMath::Max(0.15f, Event.Duration);
		Flicker.Intensity01 = Event.Intensity01;
		Flicker.bWholeHouse = Event.bIsGroupScare;
		Flicker.bAllowFullOffBursts = true;
		Flicker.MinDimAlpha = 0.10f;
		Flicker.MaxDimAlpha = 1.0f;
		Flicker.PulseIntervalMin = 0.03f;
		Flicker.PulseIntervalMax = 0.09f;
		Flicker.Seed = Event.LocalSeed;

		PlayLocalLightFlicker(Flicker);
	}

	BP_PlayScare(Event);
}

void UGvTScareComponent::Client_StartCrawlerOverheadScare_Implementation(const FGvTScareEvent& Event)
{
	UE_LOG(LogTemp, Warning, TEXT("[Scare] Client_StartCrawlerOverheadScare is deprecated. Use victim actor RPC path instead."));
}

void UGvTScareComponent::Client_PlayLightFlicker_Implementation(const FGvTLightFlickerEvent& Event)
{
	PlayLocalLightFlicker(Event);
}

void UGvTScareComponent::Server_RequestCrawlerChaseScare_Implementation(AActor* Victim)
{
	if (!IsServer() || !Victim || !CrawlerGhostClass)
	{
		return;
	}

	if (!IsValid(ActiveCrawlerGhost))
	{
		const FVector VictimLoc = Victim->GetActorLocation();
		const FVector SpawnLoc = VictimLoc
			+ (Victim->GetActorForwardVector() * CrawlerSpawnForward)
			+ FVector(0.f, 0.f, FMath::Max(CrawlerSpawnUp, 50.f));

		const FRotator SpawnRot = (VictimLoc - SpawnLoc).Rotation();

		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Params.Owner = GetOwner();

		ActiveCrawlerGhost = GetWorld()->SpawnActor<AGvTCrawlerGhostCharacter>(CrawlerGhostClass, SpawnLoc, SpawnRot, Params);
	}

	if (IsValid(ActiveCrawlerGhost))
	{
		ActiveCrawlerGhost->SpawnDefaultController();

		if (APawn* VictimPawn = Cast<APawn>(Victim))
		{
			if (Victim == GetOwner())
			{
				BeginLocalScareLifecycle(CrawlerChaseActiveDuration, ChaseRecoveryDuration);
			}

			if (AGvTAmbientAudioDirector* AmbientDirector = FindAmbientAudioDirector_Scare(this))
			{
				AmbientDirector->HandleScareStarted(GvTScareTags::CrawlerChase(), VictimPawn->GetActorLocation(), 1.0f);
			}

			ActiveCrawlerGhost->Server_StartChase(VictimPawn);
		}
	}
}

void UGvTScareComponent::Server_RequestCrawlerOverheadScare_Implementation(AActor* Victim, bool bVictimOnly)
{
	if (!IsServer() || !Victim || !CrawlerGhostClass)
	{
		return;
	}

	if (!IsValid(ActiveCrawlerGhost))
	{
		const FVector VictimLoc = Victim->GetActorLocation();
		const FVector SpawnLoc = VictimLoc + FVector(0.f, 0.f, 50.f);
		const FRotator SpawnRot = (VictimLoc - SpawnLoc).Rotation();

		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Params.Owner = GetOwner();

		ActiveCrawlerGhost = GetWorld()->SpawnActor<AGvTCrawlerGhostCharacter>(CrawlerGhostClass, SpawnLoc, SpawnRot, Params);
	}

	if (IsValid(ActiveCrawlerGhost))
	{
		APawn* VictimPawn = Cast<APawn>(Victim);
		if (!VictimPawn)
		{
			return;
		}

		if (AGvTAmbientAudioDirector* AmbientDirector = FindAmbientAudioDirector_Scare(this))
		{
			AmbientDirector->HandleScareStarted(GvTScareTags::CrawlerOverhead(), VictimPawn->GetActorLocation(), 1.0f);
		}
		ActiveCrawlerGhost->Server_StartOverheadScare(VictimPawn, bVictimOnly);
	}
}

void UGvTScareComponent::Server_RequestMirrorActorScare_Implementation(AGvTMirrorActor* Mirror, float Intensity01, float LifeSeconds)
{
	if (!Mirror)
	{
		UE_LOG(LogTemp, Warning, TEXT("[MirrorTest] Server_RequestMirrorActorScare failed: Mirror is null."));
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[MirrorTest] Server triggering mirror %s"), *Mirror->GetName());

	if (AGvTAmbientAudioDirector* AmbientDirector = FindAmbientAudioDirector_Scare(this))
	{
		AmbientDirector->HandleScareStarted(GvTScareTags::Mirror(), Mirror->GetActorLocation(), Intensity01);
	}
	Mirror->TriggerScare(Intensity01, LifeSeconds);
}

void UGvTScareComponent::Server_ApplyDeathRipple(const FVector& DeathLocation, float Radius, float BaseIntensity01)
{
	if (!IsServer() || !GetOwner()) return;

	const float Dist = FVector::Dist(GetOwner()->GetActorLocation(), DeathLocation);
	if (Dist > Radius) return;

	FGvTScareEvent Event;
	Event.ScareTag = FGameplayTag::RequestGameplayTag(TEXT("Scare.DeathRipple"));
	Event.Intensity01 = FMath::Clamp(BaseIntensity01, 0.f, 1.f);
	Event.Duration = 1.0f;
	Event.WorldHint = DeathLocation;
	Event.LocalSeed = MakeLocalSeed(GetNowServerSeconds());
	Event.bIsGroupScare = true;

	Client_PlayScare(Event);

	if (APawn* Pawn = Cast<APawn>(GetOwner()))
	{
		if (APlayerController* PC = Cast<APlayerController>(Pawn->GetController()))
		{
			if (AGvTPlayerState* PS = Cast<AGvTPlayerState>(PC->PlayerState))
			{
				const float PanicDelta = 0.15f * Event.Intensity01;
				PS->Server_AddPanic(PanicDelta);
			}
		}
	}
}

void UGvTScareComponent::SpawnLocalCrawlerOverheadGhost(APawn* Victim)
{
	if (!Victim || !CrawlerGhostClass || !GetWorld())
	{
		return;
	}

	// Clean up any prior local-only overhead ghost on this client instance.
	if (IsValid(ActiveCrawlerGhost) && !ActiveCrawlerGhost->HasAuthority())
	{
		ActiveCrawlerGhost->Destroy();
		ActiveCrawlerGhost = nullptr;
	}

	const FVector VictimLoc = Victim->GetActorLocation();
	const FVector SpawnLoc = VictimLoc + FVector(0.f, 0.f, 50.f);
	const FRotator SpawnRot = FRotator::ZeroRotator;

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.Owner = GetOwner();

	AGvTCrawlerGhostCharacter* LocalGhost =
		GetWorld()->SpawnActor<AGvTCrawlerGhostCharacter>(CrawlerGhostClass, SpawnLoc, SpawnRot, Params);

	UE_LOG(LogTemp, Warning,
		TEXT("[Scare] OverheadSpawn Victim=%s VictimLoc=%s Ghost=%s GhostLoc=%s Hidden=%d Collision=%d"),
		*GetNameSafe(Victim),
		*Victim->GetActorLocation().ToCompactString(),
		*GetNameSafe(LocalGhost),
		*LocalGhost->GetActorLocation().ToCompactString(),
		LocalGhost->IsHidden() ? 1 : 0,
		LocalGhost->GetActorEnableCollision() ? 1 : 0);

	if (!LocalGhost)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Scare] Failed to spawn local crawler overhead ghost."));
		return;
	}

	LocalGhost->SetReplicates(false);
	LocalGhost->SetReplicateMovement(false);

	ActiveCrawlerGhost = LocalGhost;
	LocalGhost->StartLocalOverheadScare(Victim);

	UE_LOG(LogTemp, Warning, TEXT("[Scare] Spawned local-only crawler overhead ghost %s for victim %s"),
		*GetNameSafe(LocalGhost),
		*GetNameSafe(Victim));
}

void UGvTScareComponent::PlayLocalLightFlicker(const FGvTLightFlickerEvent& Event) const
{
	if (UWorld* World = GetWorld())
	{
		if (UGvTLightFlickerSubsystem* Subsystem = World->GetSubsystem<UGvTLightFlickerSubsystem>())
		{
			Subsystem->PlayFlickerEvent(Event);
		}
	}
}

FGvTLightFlickerEvent UGvTScareComponent::MakeLightFlickerEvent(float Intensity01, float Duration, bool bWholeHouse) const
{
	FGvTLightFlickerEvent Event;
	Event.WorldCenter = GetOwner() ? GetOwner()->GetActorLocation() : FVector::ZeroVector;
	Event.Radius = 8000.f;
	Event.Duration = Duration;
	Event.Intensity01 = FMath::Clamp(Intensity01, 0.f, 1.f);
	Event.bWholeHouse = bWholeHouse;
	Event.bAllowFullOffBursts = true;
	Event.MinDimAlpha = 0.10f;
	Event.MaxDimAlpha = 1.0f;
	Event.PulseIntervalMin = 0.03f;
	Event.PulseIntervalMax = 0.09f;
	Event.Seed = MakeLocalSeed(GetNowServerSeconds());
	return Event;
}

bool UGvTScareComponent::IsServer() const
{
	return GetOwner() && GetOwner()->HasAuthority();
}

float UGvTScareComponent::GetNowServerSeconds() const
{
	if (const UWorld* W = GetWorld())
	{
		if (const AGameStateBase* GS = W->GetGameState())
		{
			return GS->GetServerWorldTimeSeconds();
		}
		return W->TimeSeconds;
	}
	return 0.f;
}

void UGvTScareComponent::Server_SchedulerTick()
{
	if (!IsServer()) return;

	if (!bEnableLegacyAutoScheduler)
	{
		UE_LOG(LogTemp, Error, TEXT("[Scare] Server_SchedulerTick called while legacy scheduler is DISABLED. Owner=%s"), *GetNameSafe(GetOwner()));
		return;
	}

	const float Now = GetNowServerSeconds();

	// If safety just ended, arm spike once
	if (ScareState.SafetyWindowEndTime > 0.f && Now >= ScareState.SafetyWindowEndTime)
	{
		bPendingSafetySpike = true;
		ScareState.SafetyWindowEndTime = 0.f;
	}

	if (!Server_CanTriggerNow(Now)) return;
	Server_TriggerScare(Now);
}

bool UGvTScareComponent::Server_CanTriggerNow(float Now) const
{
	if (Now < ScareState.NextScareServerTime) return false;
	if (ScareState.SafetyWindowEndTime > Now) return false;
	return true;
}

void UGvTScareComponent::Server_TriggerScare(float Now)
{
	UGvTScareSubsystem* Subsystem = GetWorld() ? GetWorld()->GetGameInstance()->GetSubsystem<UGvTScareSubsystem>() : nullptr;

	UE_LOG(LogTemp, Warning, TEXT("[Scare] Gate: Now=%.2f Next=%.2f"),
		Now, ScareState.NextScareServerTime);

	if (!Subsystem)
	{
		ScareState.NextScareServerTime = Now + 8.f;
		return;
	}

	float Panic01 = 0.f;

	const float AvgPanic01 = ComputeAveragePanic01();
	const float TimeSince01 = ComputeTimeSinceLastScare01(Now);

	const uint8 PanicTier = GetPanicTier(Panic01);
	ScareState.LastPanicTier = PanicTier;

	const float Pressure01 = ComputePressure01(Panic01, AvgPanic01, TimeSince01);
	const float FearScore01 = FMath::Clamp(0.65f * Panic01 + 0.35f * Pressure01, 0.f, 1.f);

	FGameplayTagContainer ContextTags;
	BuildContextTags(ContextTags);

	UE_LOG(LogTemp, Warning, TEXT("[Scare] ContextTags: %s"), *ContextTags.ToStringSimple());

	const UGvTGhostProfileAsset* GhostProfile = ResolveGhostProfile(Subsystem);
	FRandomStream Stream = MakeStream(Now);

	TArray<FGvTScareWeightDebugRow> DebugRows;
	const bool bDebug = UGvTScareSubsystem::IsScareDebugEnabled();

	const UGvTScareDefinition* Picked = bDebug
		? Subsystem->PickScareWeightedDebug(ContextTags, GhostProfile, PanicTier, LastTagTimeSeconds, Now, Stream, DebugRows)
		: Subsystem->PickScareWeighted(ContextTags, GhostProfile, PanicTier, LastTagTimeSeconds, Now, Stream);

	if (!Picked)
	{
		ScareState.NextScareServerTime = Now + 2.0f;
		return;
	}

	// False safety window roll
	if (GhostProfile && !Picked->bAllowDuringSafetyWindow)
	{
		const float SafetyChance = GhostProfile->SafetyWindowChance * (1.0f - FearScore01 * 0.6f);
		if (Stream.FRand() < SafetyChance)
		{
			const float Dur = Stream.FRandRange(GhostProfile->SafetyWindowRangeSeconds.X, GhostProfile->SafetyWindowRangeSeconds.Y);
			ScareState.SafetyWindowEndTime = Now + Dur;
			ScareState.NextScareServerTime = ScareState.SafetyWindowEndTime;
			return;
		}
	}

	FGvTScareEvent Event;
	Event.ScareTag = Picked->ScareTag;

	const float Intensity =
		Stream.FRandRange(Picked->IntensityMin01, Picked->IntensityMax01)
		+ (bPendingSafetySpike ? SafetySpikeBonus : 0.f);

	Event.Intensity01 = FMath::Clamp(Intensity, 0.f, 1.f);
	Event.Duration = 1.0f;
	Event.WorldHint = GetOwner() ? GetOwner()->GetActorLocation() : FVector::ZeroVector;
	Event.LocalSeed = MakeLocalSeed(Now);

	const float GroupChance = (Picked->bIsGroupEligible ? 0.25f : 0.0f); // TEST VALUE
	Event.bIsGroupScare = (GroupChance > 0.f) && (Stream.FRand() < GroupChance);

	static const FGameplayTag LightFlickerTag = FGameplayTag::RequestGameplayTag(TEXT("Scare.LightFlicker"));

	if (Event.ScareTag.MatchesTagExact(LightFlickerTag))
	{
		Event.Duration = FMath::Lerp(0.75f, 2.5f, Event.Intensity01);
	}


	if (Event.bIsGroupScare)
	{
		UWorld* World = GetWorld();
		if (World)
		{
			TArray<AActor*> Thieves;
			UGameplayStatics::GetAllActorsOfClass(World, AGvTThiefCharacter::StaticClass(), Thieves);

			for (AActor* A : Thieves)
			{
				AGvTThiefCharacter* T = Cast<AGvTThiefCharacter>(A);
				if (!T) continue;

				if (UGvTScareComponent* C = T->FindComponentByClass<UGvTScareComponent>())
				{
					C->Client_PlayScare(Event);
				}
			}
		}
	}
	else
	{
		AGvTThiefCharacter* Target = ChooseTargetThief(Stream);
		if (Target)
		{
			if (UGvTScareComponent* C = Target->FindComponentByClass<UGvTScareComponent>())
			{
				C->Client_PlayScare(Event);
			}
			else
			{
				Client_PlayScare(Event);
			}
		}
		else
		{
			Client_PlayScare(Event);
		}
	}

	LastTagTimeSeconds.Add(Event.ScareTag, Now);

	ScareState.CooldownStacks = FMath::Min(ScareState.CooldownStacks + 1, CooldownStackCap);
	ScareState.LastScareServerTime = Now;

	const float Cooldown = ComputeCooldownSeconds(FearScore01) * (GhostProfile ? GhostProfile->FrequencyMultiplier : 1.0f);
	ScareState.NextScareServerTime = Now + Cooldown;
}

AGvTThiefCharacter* UGvTScareComponent::ChooseTargetThief(FRandomStream& Stream) const
{
	UWorld* World = GetWorld();
	if (!World) return nullptr;

	TArray<AActor*> Thieves;
	UGameplayStatics::GetAllActorsOfClass(World, AGvTThiefCharacter::StaticClass(), Thieves);

	TArray<AGvTThiefCharacter*> Candidates;
	for (AActor* A : Thieves)
	{
		AGvTThiefCharacter* T = Cast<AGvTThiefCharacter>(A);
		if (!T) continue;

		// MVP rules: alive, has controller, etc. (keep simple)
		if (T->GetController() != nullptr)
			Candidates.Add(T);
	}

	if (Candidates.Num() == 0) return nullptr;
	return Candidates[Stream.RandRange(0, Candidates.Num() - 1)];
}


void UGvTScareComponent::Client_ReflectTick()
{
	if (!bEnableReflectScare)
	{
		return;
	}

	APawn* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn || !Pawn->IsLocallyControlled())
	{
		return;
	}

	APlayerController* PC = Cast<APlayerController>(Pawn->GetController());
	if (!PC)
	{
		return;
	}

	FVector CamLoc;
	FRotator CamRot;
	PC->GetPlayerViewPoint(CamLoc, CamRot);
	const FVector CamFwd = CamRot.Vector();

	const FVector Start = CamLoc;
	const FVector End = Start + CamFwd * ReflectTraceDistance;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(GvTReflectTrace), false, Pawn);
	Params.AddIgnoredActor(Pawn);

	FHitResult Hit;
	const bool bHit = GetWorld()->SweepSingleByChannel(
		Hit,
		Start,
		End,
		FQuat::Identity,
		ECC_Visibility,
		FCollisionShape::MakeSphere(ReflectSphereRadius),
		Params);

	if (!bHit)
	{
		return;
	}

	AGvTMirrorActor* Mirror = Cast<AGvTMirrorActor>(Hit.GetActor());
	if (!Mirror)
	{
		return;
	}

	const FVector ToCam = (CamLoc - Hit.ImpactPoint).GetSafeNormal();
	const float Dot = FVector::DotProduct(CamFwd, -ToCam);

	if (Dot < ReflectDotMin)
	{
		return;
	}

	const float Intensity01 = FMath::Clamp(
		(Dot - ReflectDotMin) / FMath::Max(KINDA_SMALL_NUMBER, (1.f - ReflectDotMin)),
		0.f,
		1.f);

	const float Now = GetWorld() ? GetWorld()->TimeSeconds : 0.f;

	if (Now < NextAllowedReflectTime)
	{
		return;
	}

	if (LastTriggeredMirror.IsValid() && LastTriggeredMirror.Get() == Mirror)
	{
		return;
	}

	Server_RequestMirrorActorScare(Mirror, Intensity01, ReflectLifeSeconds);
	LastTriggeredMirror = Mirror;
	NextAllowedReflectTime = Now + ReflectLifeSeconds + 0.25f;
}

float UGvTScareComponent::ComputePressure01(float PlayerPanic01, float AvgPanic01, float TimeSinceLastScare01) const
{
	const float Elapsed = GetWorld() ? GetWorld()->TimeSeconds : 0.f;
	const float Elapsed01 = FMath::Clamp(Elapsed / 600.f, 0.f, 1.f);

	const float Pressure =
		0.45f * PlayerPanic01 +
		0.25f * AvgPanic01 +
		0.20f * TimeSinceLastScare01 +
		0.10f * Elapsed01;

	return FMath::Clamp(Pressure, 0.f, 1.f);
}

void UGvTScareComponent::BuildContextTags(FGameplayTagContainer& OutContext) const
{
	UE_LOG(LogTemp, Warning, TEXT("[Scare] BuildContextTags running. Owner=%s HasAuthority=%d"),
		*GetOwner()->GetName(),
		GetOwner()->HasAuthority() ? 1 : 0);

	UWorld* W = GetWorld();
	UGameInstance* GI = W ? W->GetGameInstance() : nullptr;
	if (!GI) return;

	UGvTNoiseSubsystem* Noise = GI->GetSubsystem<UGvTNoiseSubsystem>();
	if (!Noise) return;

	const FGameplayTag ElectricNoise = FGameplayTag::RequestGameplayTag(TEXT("Noise.Electric"));
	const FGameplayTag ContextElectricalHigh = FGameplayTag::RequestGameplayTag(TEXT("Context.ElectricalHigh"));

	const int32 ElectricCount = Noise->GetRecentTagCount(ElectricNoise, 25.f);

	UE_LOG(LogTemp, Warning, TEXT("[Scare] ElectricCount=%d (window=25s)"), ElectricCount);

	if (ElectricCount >= 1)
	{
		OutContext.AddTag(ContextElectricalHigh);
	}
}

float UGvTScareComponent::ComputeCooldownSeconds(float FearScore01) const
{
	const float Base = FMath::Lerp(BaseCooldownMax, BaseCooldownMin, FearScore01);
	const float StackPenalty = float(ScareState.CooldownStacks) * CooldownStackPenaltySeconds;
	return FMath::Clamp(Base + StackPenalty, 4.f, 30.f);
}

int32 UGvTScareComponent::MakeLocalSeed(float Now) const
{
	const int32 Bucket = int32(FMath::FloorToInt(Now / 5.f));
	return ScareState.Seed ^ (Bucket * 196613) ^ (ScareState.CooldownStacks * 834927);
}

FRandomStream UGvTScareComponent::MakeStream(float Now) const
{
	return FRandomStream(MakeLocalSeed(Now));
}

const UGvTGhostProfileAsset* UGvTScareComponent::ResolveGhostProfile(const UGvTScareSubsystem* Subsystem) const
{
	if (!Subsystem) return nullptr;
	const FGameplayTag GhostTag = DefaultGhostTag.IsValid() ? DefaultGhostTag : FGameplayTag();
	return GhostTag.IsValid() ? Subsystem->GetGhostProfile(GhostTag) : nullptr;
}

uint8 UGvTScareComponent::GetPanicTier(float& OutPanic01) const
{
	OutPanic01 = 0.f;

	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn) return 0;

	const APlayerController* PC = Cast<APlayerController>(Pawn->GetController());
	if (!PC) return 0;

	const AGvTPlayerState* PS = Cast<AGvTPlayerState>(PC->PlayerState);
	if (!PS) return 0;

	OutPanic01 = FMath::Clamp(PS->GetPanic01(), 0.f, 1.f);

	if (OutPanic01 < 0.20f) return 0;
	if (OutPanic01 < 0.45f) return 1;
	if (OutPanic01 < 0.70f) return 2;
	if (OutPanic01 < 0.90f) return 3;
	return 4;
}

float UGvTScareComponent::ComputeAveragePanic01() const
{
	const UWorld* W = GetWorld();
	const AGameStateBase* GS = W ? W->GetGameState() : nullptr;
	if (!GS || GS->PlayerArray.Num() == 0) return 0.f;

	float Sum = 0.f;
	int32 Count = 0;

	for (APlayerState* PS : GS->PlayerArray)
	{
		if (const AGvTPlayerState* GvTPS = Cast<AGvTPlayerState>(PS))
		{
			Sum += FMath::Clamp(GvTPS->GetPanic01(), 0.f, 1.f);
			Count++;
		}
	}

	return (Count > 0) ? (Sum / float(Count)) : 0.f;
}

float UGvTScareComponent::ComputeTimeSinceLastScare01(float Now) const
{
	if (ScareState.LastScareServerTime <= 0.f) return 1.f;
	const float Delta = FMath::Max(0.f, Now - ScareState.LastScareServerTime);

	return FMath::Clamp(Delta / 30.f, 0.f, 1.f);
}

void UGvTScareComponent::UpdatePanicBand()
{
	const float N = GetPanicNormalized();

	if (N >= 0.90f) CachedPanicBand = EGvTPanicBand::Critical;
	else if (N >= 0.70f) CachedPanicBand = EGvTPanicBand::Terrified;
	else if (N >= 0.45f) CachedPanicBand = EGvTPanicBand::Shaken;
	else if (N >= 0.20f) CachedPanicBand = EGvTPanicBand::Unsettled;
	else CachedPanicBand = EGvTPanicBand::Calm;
}

FVector UGvTScareComponent::ResolveLightChaseStepLocation(const FVector& RawStepLocation) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return RawStepLocation;
	}

	const UGvTLightFlickerSubsystem* FlickerSubsystem = World->GetSubsystem<UGvTLightFlickerSubsystem>();
	if (!FlickerSubsystem)
	{
		return RawStepLocation;
	}

	FGvTLightClusterQueryResult QueryResult;
	const float SnapSearchRadius = FMath::Max(ActiveLightChaseEvent.LightChaseFlickerRadius * 1.25f, 500.f);

	if (!FlickerSubsystem->FindNearestLightClusterCenter(RawStepLocation, SnapSearchRadius, QueryResult))
	{
		return RawStepLocation;
	}

	if (!QueryResult.bFoundAny || QueryResult.NumLights <= 0)
	{
		return RawStepLocation;
	}

	return QueryResult.ClusterCenter;
}

void UGvTScareComponent::CollapseLightChaseStepLocations(TArray<FVector>& StepLocations) const
{
	if (StepLocations.Num() <= 1)
	{
		return;
	}

	TArray<FVector> Filtered;
	Filtered.Reserve(StepLocations.Num());

	const float MinSeparationSq = FMath::Square(180.f);

	for (const FVector& Loc : StepLocations)
	{
		if (Filtered.Num() == 0)
		{
			Filtered.Add(Loc);
			continue;
		}

		if (FVector::DistSquared(Filtered.Last(), Loc) >= MinSeparationSq)
		{
			Filtered.Add(Loc);
		}
	}

	StepLocations = MoveTemp(Filtered);
}

void UGvTScareComponent::Server_ReportLightChaseResult_Implementation(bool bSucceeded, float PanicAmount)
{
	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!OwnerPawn)
	{
		return;
	}

	AGvTPlayerState* PS = OwnerPawn->GetPlayerState<AGvTPlayerState>();
	if (!PS)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[LightChase] No PlayerState on %s, cannot apply panic."),
			*GetNameSafe(OwnerPawn));
		return;
	}

	if (!bSucceeded || PanicAmount <= 0.f)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[LightChase] No panic awarded to %s. Success=%d Panic=%.2f"),
			*GetNameSafe(OwnerPawn),
			bSucceeded ? 1 : 0,
			PanicAmount);
		return;
	}

	const float PanicDelta01 = PanicAmount / 100.f;
	PS->AddPanicAuthority(PanicDelta01);

	UE_LOG(LogTemp, Warning,
		TEXT("[LightChase] Awarded panic to %s. PanicRaw=%.2f Panic01=%.3f"),
		*GetNameSafe(OwnerPawn),
		PanicAmount,
		PanicDelta01);
}