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
	UE_LOG(LogTemp, Warning, TEXT("[Panic] %s Panic01=%.2f"), *GetNameSafe(this), Panic01);
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
	UE_LOG(LogTemp, Warning, TEXT("[Pressure] %s Pressure01=%.2f"), *GetNameSafe(this), RecentHauntPressure01);
	OnHauntPressureChanged.Broadcast(RecentHauntPressure01);
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

	Panic01 = FMath::Clamp(Panic01 - PanicDecayPerSecond * DeltaSeconds, 0.f, 1.f);
	OnRep_Panic();
}

void AGvTPlayerState::ApplyHauntPressureDecay(float DeltaSeconds)
{
	if (RecentHauntPressure01 <= 0.f)
	{
		return;
	}

	RecentHauntPressure01 = FMath::Clamp(
		RecentHauntPressure01 - HauntPressureDecayPerSecond * DeltaSeconds,
		0.f,
		1.f
	);

	OnRep_HauntPressure();
}

void AGvTPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AGvTPlayerState, LootValue);
	DOREPLIFETIME(AGvTPlayerState, Panic01);
	DOREPLIFETIME(AGvTPlayerState, RecentHauntPressure01);
}