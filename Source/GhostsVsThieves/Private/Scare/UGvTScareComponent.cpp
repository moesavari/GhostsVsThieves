#include "Scare/UGvTScareComponent.h"
#include "Net/UnrealNetwork.h"
#include "GameplayTagsManager.h"
#include "GameFramework/GameStateBase.h"
#include "Scare/UGvTScareSubsystem.h"
#include "Scare/GvTScareDefinition.h"
#include "Scare/UGvTGhostProfileAsset.h"

UGvTScareComponent::UGvTScareComponent()
{
	SetIsReplicatedByDefault(true);
	PrimaryComponentTick.bCanEverTick = false;
}

void UGvTScareComponent::BeginPlay()
{
	Super::BeginPlay();

	if (IsServer())
	{
		if (ScareState.Seed == 0)
		{
			const int32 OwnerHash = GetOwner() ? int32(reinterpret_cast<uintptr_t>(GetOwner()) & 0x7fffffff) : 1337;
			const float Now = GetNowServerSeconds();
			ScareState.Seed = OwnerHash ^ int32(Now * 1000.f) ^ FMath::Rand();
		}

		const float Now = GetNowServerSeconds();
		ScareState.NextScareServerTime = Now + 8.f;
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

void UGvTScareComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UGvTScareComponent, ScareState);
}

void UGvTScareComponent::OnRep_ScareState()
{
	// Optional: client debug / UI
}

uint8 UGvTScareComponent::GetPanicTier(float& OutPanic01) const
{
	// TODO: Replace with your AGvTPlayerState Panic01 once you add it.
	OutPanic01 = 0.f;

	if (OutPanic01 < 0.20f) return 0;
	if (OutPanic01 < 0.45f) return 1;
	if (OutPanic01 < 0.70f) return 2;
	if (OutPanic01 < 0.90f) return 3;
	return 4;
}

float UGvTScareComponent::ComputePressure01(float PlayerPanic01) const
{
	const float Elapsed = GetWorld() ? GetWorld()->TimeSeconds : 0.f;
	const float Elapsed01 = FMath::Clamp(Elapsed / 600.f, 0.f, 1.f); // 10 min ramp
	return FMath::Clamp(0.65f * PlayerPanic01 + 0.35f * Elapsed01, 0.f, 1.f);
}

void UGvTScareComponent::BuildContextTags(FGameplayTagContainer& OutContext) const
{
	// MVP: empty. Later: alone/group, noisy (noise subsystem), light/dark, phase tags, etc.
}

bool UGvTScareComponent::Server_CanTriggerNow(float Now) const
{
	if (Now < ScareState.NextScareServerTime) return false;
	if (ScareState.SafetyWindowEndTime > Now) return false;
	return true;
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

void UGvTScareComponent::Server_TriggerScare(float Now)
{
	UGvTScareSubsystem* Subsystem = GetWorld() ? GetWorld()->GetGameInstance()->GetSubsystem<UGvTScareSubsystem>() : nullptr;
	if (!Subsystem)
	{
		ScareState.NextScareServerTime = Now + 8.f;
		return;
	}

	float Panic01 = 0.f;
	const uint8 PanicTier = GetPanicTier(Panic01);
	ScareState.LastPanicTier = PanicTier;

	const float Pressure01 = ComputePressure01(Panic01);
	const float FearScore01 = FMath::Clamp(0.65f * Panic01 + 0.35f * Pressure01, 0.f, 1.f);

	FGameplayTagContainer ContextTags;
	BuildContextTags(ContextTags);

	const UGvTGhostProfileAsset* GhostProfile = ResolveGhostProfile(Subsystem);
	FRandomStream Stream = MakeStream(Now);

	const UGvTScareDefinition* Picked = Subsystem->PickScareWeighted(ContextTags, GhostProfile, PanicTier, LastTagTimeSeconds, Now, Stream);
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
	Event.bIsGroupScare = false;

	bPendingSafetySpike = false;

	Client_PlayScare(Event);

	LastTagTimeSeconds.Add(Event.ScareTag, Now);

	ScareState.CooldownStacks = FMath::Min(ScareState.CooldownStacks + 1, CooldownStackCap);
	ScareState.LastScareServerTime = Now;

	const float Cooldown = ComputeCooldownSeconds(FearScore01) * (GhostProfile ? GhostProfile->FrequencyMultiplier : 1.0f);
	ScareState.NextScareServerTime = Now + Cooldown;
}

void UGvTScareComponent::Client_PlayScare_Implementation(const FGvTScareEvent& Event)
{
	BP_PlayScare(Event);
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

	// TODO: Add panic to PlayerState here when your Panic01 exists.
}
