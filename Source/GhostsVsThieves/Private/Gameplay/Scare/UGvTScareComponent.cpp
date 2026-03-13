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
#include "Gameplay/Characters/Thieves/GvTThiefCharacter.h"

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

		GetWorld()->GetTimerManager().SetTimer(
			SchedulerTimer,
			this,
			&UGvTScareComponent::Server_SchedulerTick,
			SchedulerIntervalSeconds,
			true
		);
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
}

void UGvTScareComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UGvTScareComponent, ScareState);
	DOREPLIFETIME(UGvTScareComponent, Panic);
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



void UGvTScareComponent::RequestMirrorScare(float Intensity01, float LifeSeconds)
{
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
	RequestCrawlerChaseScare(Victim);
}

void UGvTScareComponent::RequestCrawlerOverheadFromEvent(const FGvTScareEvent& Event)
{
	AActor* Victim = Event.TargetActor;
	if (!Victim)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Scare] CrawlerOverhead failed: No victim."));
		return;
	}

	if (GetOwner() != Victim)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Scare] CrawlerOverhead aborted: component owner %s does not match victim %s"),
			*GetNameSafe(GetOwner()),
			*GetNameSafe(Victim));
		return;
	}

	AGvTThiefCharacter* Thief = Cast<AGvTThiefCharacter>(Victim);
	if (!Thief)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Scare] CrawlerOverhead failed: victim is not a thief."));
		return;
	}

	if (!IsServer())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Scare] CrawlerOverhead ignored: RequestCrawlerOverheadFromEvent must run on server."));
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("[Scare] CrawlerOverhead routing via victim actor RPC. ComponentOwner=%s Victim=%s"),
		*GetNameSafe(GetOwner()),
		*GetNameSafe(Victim));

	// Authoritative gameplay stun.
	Thief->ApplyScareStun(0.6f);

	// Victim-only local presentation on the owning client.
	Thief->Client_PlayLocalScareStun(0.6f);
	Thief->Client_PlayLocalCrawlerOverheadScare(Event);
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
	APawn* VictimPawn = Cast<APawn>(GetOwner());
	APawn* EventVictimPawn = Cast<APawn>(Event.TargetActor);

	if (!VictimPawn)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Scare] PlayLocalCrawlerOverheadScare failed: component owner is not a pawn."));
		return;
	}

	if (VictimPawn != EventVictimPawn)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[Scare] PlayLocalCrawlerOverheadScare aborted: owner %s != event victim %s"),
			*GetNameSafe(VictimPawn),
			*GetNameSafe(EventVictimPawn));
		return;
	}

	if (!VictimPawn->IsLocallyControlled())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[Scare] PlayLocalCrawlerOverheadScare ignored: owner is not locally controlled (%s)"),
			*GetNameSafe(VictimPawn));
		return;
	}

	UE_LOG(LogTemp, Warning,
		TEXT("[Scare] PlayLocalCrawlerOverheadScare begin Owner=%s NetMode=%d LocalRole=%d RemoteRole=%d LocallyControlled=%d VictimLoc=%s"),
		*GetNameSafe(VictimPawn),
		(int32)VictimPawn->GetNetMode(),
		(int32)VictimPawn->GetLocalRole(),
		(int32)VictimPawn->GetRemoteRole(),
		VictimPawn->IsLocallyControlled() ? 1 : 0,
		*VictimPawn->GetActorLocation().ToCompactString());

	SpawnLocalCrawlerOverheadGhost(VictimPawn);

	UE_LOG(LogTemp, Warning, TEXT("[Scare] PlayLocalCrawlerOverheadScare local victim = %s"),
		*GetNameSafe(VictimPawn));
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



void UGvTScareComponent::Client_PlayMirrorScare_Implementation(float Intensity01, float LifeSeconds)
{
	Test_MirrorScare(Intensity01, LifeSeconds);
}

void UGvTScareComponent::Client_PlayScare_Implementation(const FGvTScareEvent& Event)
{
	static const FGameplayTag LightFlickerTag = FGameplayTag::RequestGameplayTag(TEXT("Scare.LightFlicker"));

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
			if (AGvTPlayerState* PS = PC->GetPlayerState<AGvTPlayerState>())
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

	Mirror->TriggerScare(Intensity01, ReflectLifeSeconds);
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

	const AGvTPlayerState* PS = PC->GetPlayerState<AGvTPlayerState>();
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

