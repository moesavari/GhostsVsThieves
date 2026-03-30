#include "GvTPlayerState.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"

AGvTPlayerState::AGvTPlayerState()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
}

void AGvTPlayerState::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!HasAuthority())
	{
		return;
	}

	ApplyPanicDecay(DeltaSeconds);
	ApplyHauntPressureDecay(DeltaSeconds);
}

void AGvTPlayerState::AddLoot(int32 Amount)
{
	if (!HasAuthority() || Amount == 0)
	{
		return;
	}

	LootValue = FMath::Max(0, LootValue + Amount);
	OnLootValueChanged.Broadcast(LootValue);
	ForceNetUpdate();
}

void AGvTPlayerState::OnRep_LootValue()
{
	OnLootValueChanged.Broadcast(LootValue);
}

void AGvTPlayerState::AddPanicAuthority(float Delta01)
{
	if (!HasAuthority() || Delta01 <= 0.f)
	{
		return;
	}

	Panic01 = FMath::Clamp(Panic01 + Delta01, 0.f, 1.f);
	LastPanicSpikeTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	OnRep_Panic();
	ForceNetUpdate();
}

void AGvTPlayerState::ReducePanicAuthority(float Delta01)
{
	if (!HasAuthority() || Delta01 <= 0.f)
	{
		return;
	}

	Panic01 = FMath::Clamp(Panic01 - Delta01, 0.f, 1.f);
	OnRep_Panic();
	ForceNetUpdate();
}

void AGvTPlayerState::Server_AddPanic_Implementation(float Delta01)
{
	AddPanicAuthority(Delta01);
}

void AGvTPlayerState::Server_ReducePanic_Implementation(float Delta01)
{
	ReducePanicAuthority(Delta01);
}

void AGvTPlayerState::OnRep_Panic()
{
	UE_LOG(LogTemp, Verbose, TEXT("[Panic] %s Panic01=%.2f"), *GetNameSafe(this), Panic01);
	OnPanicChanged.Broadcast(Panic01);
}

void AGvTPlayerState::AddHauntPressureAuthority(float Delta01)
{
	if (!HasAuthority() || Delta01 <= 0.f)
	{
		return;
	}

	RecentHauntPressure01 = FMath::Clamp(RecentHauntPressure01 + Delta01, 0.f, 1.f);
	OnRep_HauntPressure();
	ForceNetUpdate();
}

void AGvTPlayerState::ReduceHauntPressureAuthority(float Delta01)
{
	if (!HasAuthority() || Delta01 <= 0.f)
	{
		return;
	}

	RecentHauntPressure01 = FMath::Clamp(RecentHauntPressure01 - Delta01, 0.f, 1.f);
	OnRep_HauntPressure();
	ForceNetUpdate();
}

void AGvTPlayerState::Server_AddHauntPressure_Implementation(float Delta01)
{
	AddHauntPressureAuthority(Delta01);
}

void AGvTPlayerState::Server_ReduceHauntPressure_Implementation(float Delta01)
{
	ReduceHauntPressureAuthority(Delta01);
}

void AGvTPlayerState::OnRep_HauntPressure()
{
	UE_LOG(LogTemp, Verbose, TEXT("[Pressure] %s Pressure01=%.2f"), *GetNameSafe(this), RecentHauntPressure01);
	OnHauntPressureChanged.Broadcast(RecentHauntPressure01);
}

void AGvTPlayerState::Server_ApplyPanicEvent_Implementation(const FGvTPanicEvent& Event)
{
	ApplyPanicEventAuthority(Event);
}

bool AGvTPlayerState::WouldApplyPanicEvent(const FGvTPanicEvent& Event) const
{
	if (!HasAuthority())
	{
		return false;
	}

	if (Event.Source == EGvTPanicSource::None)
	{
		return false;
	}

	if (Event.bRequiresSuccessfulExecution && !Event.bExecutionSucceeded)
	{
		return false;
	}

	if (Event.bRequiresProximity)
	{
		APawn* OwnerPawn = GetPawn();
		if (!OwnerPawn)
		{
			return false;
		}

		if (Event.SourceRadius <= 0.f)
		{
			return false;
		}

		const float DistSq = FVector::DistSquared(OwnerPawn->GetActorLocation(), Event.WorldLocation);
		if (DistSq > FMath::Square(Event.SourceRadius))
		{
			return false;
		}
	}

	const float CooldownSeconds = ResolveCooldownForSource(Event.Source, Event.CooldownSeconds);
	if (!CanApplyPanicSource(Event.Source, CooldownSeconds))
	{
		return false;
	}

	return true;
}

void AGvTPlayerState::ApplyPanicEventAuthority(const FGvTPanicEvent& Event)
{
	if (!WouldApplyPanicEvent(Event))
	{
		UE_LOG(LogTemp, Verbose,
			TEXT("[PanicEvent] Rejected PlayerState=%s Source=%s Success=%d PanicDelta=%.3f PressureDelta=%.3f"),
			*GetNameSafe(this),
			PanicSourceToString(Event.Source),
			Event.bExecutionSucceeded ? 1 : 0,
			Event.PanicDelta01,
			Event.HauntPressureDelta01);
		return;
	}

	const float OldPanic = Panic01;
	const float OldPressure = RecentHauntPressure01;

	const float NewPanic = FMath::Clamp(Panic01 + Event.PanicDelta01, 0.f, 1.f);
	const float NewPressure = FMath::Clamp(RecentHauntPressure01 + Event.HauntPressureDelta01, 0.f, 1.f);

	const bool bPanicChanged = !FMath::IsNearlyEqual(NewPanic, OldPanic);
	const bool bPressureChanged = !FMath::IsNearlyEqual(NewPressure, OldPressure);

	Panic01 = NewPanic;
	RecentHauntPressure01 = NewPressure;

	if (Event.PanicDelta01 > 0.f)
	{
		LastPanicSpikeTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	}

	MarkPanicSourceApplied(Event.Source);

	if (bPanicChanged)
	{
		OnRep_Panic();
	}

	if (bPressureChanged)
	{
		OnRep_HauntPressure();
	}

	if (bPanicChanged || bPressureChanged)
	{
		ForceNetUpdate();
	}

	UE_LOG(LogTemp, Log,
		TEXT("[PanicEvent] PlayerState=%s Source=%s Panic %.3f->%.3f Pressure %.3f->%.3f SrcActor=%s Instigator=%s Radius=%.1f Success=%d"),
		*GetNameSafe(this),
		PanicSourceToString(Event.Source),
		OldPanic,
		Panic01,
		OldPressure,
		RecentHauntPressure01,
		*GetNameSafe(Event.SourceActor),
		*GetNameSafe(Event.InstigatorActor),
		Event.SourceRadius,
		Event.bExecutionSucceeded ? 1 : 0);
}

void AGvTPlayerState::ApplyPanicDecay(float DeltaSeconds)
{
	if (Panic01 <= 0.f)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const float Now = World->GetTimeSeconds();
	if ((Now - LastPanicSpikeTime) < PanicRecoveryDelayAfterSpike)
	{
		return;
	}

	const float NewPanic = FMath::Clamp(Panic01 - PanicDecayPerSecond * DeltaSeconds, 0.f, 1.f);
	if (!FMath::IsNearlyEqual(NewPanic, Panic01))
	{
		Panic01 = NewPanic;
		OnRep_Panic();
	}
}

void AGvTPlayerState::ApplyHauntPressureDecay(float DeltaSeconds)
{
	if (RecentHauntPressure01 <= 0.f)
	{
		return;
	}

	const float NewPressure = FMath::Clamp(
		RecentHauntPressure01 - HauntPressureDecayPerSecond * DeltaSeconds,
		0.f,
		1.f
	);

	if (!FMath::IsNearlyEqual(NewPressure, RecentHauntPressure01))
	{
		RecentHauntPressure01 = NewPressure;
		OnRep_HauntPressure();
	}
}

bool AGvTPlayerState::CanApplyPanicSource(EGvTPanicSource Source, float CooldownSeconds) const
{
	if (CooldownSeconds <= 0.f)
	{
		return true;
	}

	const UWorld* World = GetWorld();
	if (!World)
	{
		return true;
	}

	const float* LastTimePtr = LastAppliedPanicSourceTime.Find(static_cast<uint8>(Source));
	if (!LastTimePtr)
	{
		return true;
	}

	const float Now = World->GetTimeSeconds();
	return (Now - *LastTimePtr) >= CooldownSeconds;
}

void AGvTPlayerState::MarkPanicSourceApplied(EGvTPanicSource Source)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	LastAppliedPanicSourceTime.FindOrAdd(static_cast<uint8>(Source)) = World->GetTimeSeconds();
}

float AGvTPlayerState::ResolveCooldownForSource(EGvTPanicSource Source, float OverrideCooldownSeconds) const
{
	if (OverrideCooldownSeconds > 0.f)
	{
		return OverrideCooldownSeconds;
	}

	switch (Source)
	{
	case EGvTPanicSource::DoorSlam:
		return DefaultDoorSlamCooldownSeconds;

	case EGvTPanicSource::LightFlicker:
	case EGvTPanicSource::LightShutdown:
		return DefaultLightCooldownSeconds;

	case EGvTPanicSource::PowerOutage:
	case EGvTPanicSource::PowerRestore:
		return DefaultPowerCooldownSeconds;

	case EGvTPanicSource::MirrorScare:
	case EGvTPanicSource::CrawlerOverhead:
	case EGvTPanicSource::CrawlerChaseStart:
	case EGvTPanicSource::CrawlerChaseTick:
	case EGvTPanicSource::RearAudioSting:
	case EGvTPanicSource::GhostScream:
	case EGvTPanicSource::DeathRipple:
		return DefaultScareCooldownSeconds;

	default:
		return 0.f;
	}
}

const TCHAR* AGvTPlayerState::PanicSourceToString(EGvTPanicSource Source)
{
	switch (Source)
	{
	case EGvTPanicSource::None:					return TEXT("None");
	case EGvTPanicSource::ItemPickup:			return TEXT("ItemPickup");
	case EGvTPanicSource::ItemPickupHighValue:	return TEXT("ItemPickupHighValue");
	case EGvTPanicSource::ItemPickupHaunted:	return TEXT("ItemPickupHaunted");
	case EGvTPanicSource::ScannerRevealHaunted:	return TEXT("ScannerRevealHaunted");
	case EGvTPanicSource::DoorSlam:				return TEXT("DoorSlam");
	case EGvTPanicSource::LightFlicker:			return TEXT("LightFlicker");
	case EGvTPanicSource::LightShutdown:		return TEXT("LightShutdown");
	case EGvTPanicSource::PowerOutage:			return TEXT("PowerOutage");
	case EGvTPanicSource::PowerRestore:			return TEXT("PowerRestore");
	case EGvTPanicSource::MirrorScare:			return TEXT("MirrorScare");
	case EGvTPanicSource::CrawlerOverhead:		return TEXT("CrawlerOverhead");
	case EGvTPanicSource::CrawlerChaseStart:	return TEXT("CrawlerChaseStart");
	case EGvTPanicSource::CrawlerChaseTick:		return TEXT("CrawlerChaseTick");
	case EGvTPanicSource::RearAudioSting:		return TEXT("RearAudioSting");
	case EGvTPanicSource::GhostScream:			return TEXT("GhostScream");
	case EGvTPanicSource::DeathRipple:			return TEXT("DeathRipple");
	default:									return TEXT("Unknown");
	}
}

void AGvTPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AGvTPlayerState, LootValue);
	DOREPLIFETIME(AGvTPlayerState, Panic01);
	DOREPLIFETIME(AGvTPlayerState, RecentHauntPressure01);
}