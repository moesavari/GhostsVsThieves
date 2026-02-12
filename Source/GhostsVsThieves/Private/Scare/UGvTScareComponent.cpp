#include "Scare/UGvTScareComponent.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameplayTagsManager.h"
#include "GvTPlayerState.h"
#include "Net/UnrealNetwork.h"
#include "Scare/GvTScareDefinition.h"
#include "Scare/UGvTGhostProfileAsset.h"
#include "Scare/UGvTScareSubsystem.h"
#include "Systems/Noise/GvTNoiseSubsystem.h"
#include "Kismet/GameplayStatics.h"

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
