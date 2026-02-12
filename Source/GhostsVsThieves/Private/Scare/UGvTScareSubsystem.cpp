#include "Scare/UGvTScareSubsystem.h"
#include "Scare/GvTScareDefinition.h"
#include "Scare/UGvTGhostProfileAsset.h"
#include "HAL/IConsoleManager.h"

static TAutoConsoleVariable<int32> CVarGvTScareDebug(
	TEXT("gvt.ScareDebug"), 0,
	TEXT("0=off, 1=log candidate weights and selection"),
	ECVF_Default
);

static TAutoConsoleVariable<int32> CVarGvTScareDebugTopN(
	TEXT("gvt.ScareDebugTopN"), 5,
	TEXT("How many candidates to print"),
	ECVF_Default
);

static bool IsGvTScareDebugEnabled()
{
	return CVarGvTScareDebug.GetValueOnGameThread() != 0;
}

void UGvTScareSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UGvTScareSubsystem::RegisterScareDefinition(UGvTScareDefinition* Def)
{
	if (Def)
	{
		ScareDefinitions.AddUnique(Def);
	}

	UE_LOG(LogTemp, Log, TEXT("Registered Scare: %s"), *GetNameSafe(Def));
}

void UGvTScareSubsystem::RegisterGhostProfile(UGvTGhostProfileAsset* Profile)
{
	if (Profile && Profile->GhostTag.IsValid())
	{
		GhostProfiles.Add(Profile->GhostTag, Profile);
	}

	UE_LOG(LogTemp, Log, TEXT("Registered GhostProfile: %s"), *GetNameSafe(Profile));
}

const UGvTGhostProfileAsset* UGvTScareSubsystem::GetGhostProfile(FGameplayTag GhostTag) const
{
	if (const TObjectPtr<UGvTGhostProfileAsset>* Found = GhostProfiles.Find(GhostTag))
	{
		return Found->Get();
	}
	return nullptr;
}

const UGvTScareDefinition* UGvTScareSubsystem::PickScareWeighted(
	const FGameplayTagContainer& ContextTags,
	const UGvTGhostProfileAsset* GhostProfile,
	uint8 PanicTier,
	const TMap<FGameplayTag, float>& LastTagTimeSeconds,
	float NowSeconds,
	FRandomStream& Stream
) const
{
	TArray<FGvTWeightedScareCandidate> Candidates;
	Candidates.Reserve(ScareDefinitions.Num());

	float TotalWeight = 0.f;

	for (const UGvTScareDefinition* Def : ScareDefinitions)
	{
		if (!Def || !Def->ScareTag.IsValid())
		{
			continue;
		}

		if (PanicTier < Def->MinPanicTier || PanicTier > Def->MaxPanicTier)
		{
			continue;
		}

		if (!ContextTags.HasAll(Def->RequiredContexts))
		{
			continue;
		}

		if (ContextTags.HasAny(Def->BlockedContexts))
		{
			continue;
		}

		const float W = ComputeFinalWeight(Def, ContextTags, GhostProfile, PanicTier, LastTagTimeSeconds, NowSeconds);
		if (W <= KINDA_SMALL_NUMBER)
		{
			continue;
		}

		Candidates.Add({ Def, W });
		TotalWeight += W;
	}

	if (Candidates.Num() == 0 || TotalWeight <= KINDA_SMALL_NUMBER)
	{
		return nullptr;
	}

	const float Roll = Stream.FRandRange(0.f, TotalWeight);
	float Running = 0.f;

	for (const FGvTWeightedScareCandidate& C : Candidates)
	{
		Running += C.Weight;
		if (Roll <= Running)
		{
			return C.Def.Get();
		}
	}

	return Candidates.Last().Def.Get();
}

const UGvTScareDefinition* UGvTScareSubsystem::PickScareWeightedDebug(
	const FGameplayTagContainer& ContextTags,
	const UGvTGhostProfileAsset* GhostProfile,
	uint8 PanicTier,
	const TMap<FGameplayTag, float>& LastTagTimeSeconds,
	float NowSeconds,
	FRandomStream& Stream,
	TArray<FGvTScareWeightDebugRow>& OutDebugRows
) const
{
	OutDebugRows.Reset();

	TArray<FGvTWeightedScareCandidate> Candidates;
	Candidates.Reserve(ScareDefinitions.Num());

	float TotalWeight = 0.f;

	for (const UGvTScareDefinition* Def : ScareDefinitions)
	{
		FGvTScareWeightDebugRow Row;

		if (!Def || !Def->ScareTag.IsValid())
		{
			Row.FilterReason = TEXT("Invalid Def or ScareTag");
			OutDebugRows.Add(Row);
			continue;
		}

		Row.Tag = Def->ScareTag;
		Row.Base = Def->BaseWeight;

		if (PanicTier < Def->MinPanicTier || PanicTier > Def->MaxPanicTier)
		{
			Row.FilterReason = FString::Printf(TEXT("TierGate: PanicTier=%d not in [%d..%d]"),
				(int32)PanicTier, (int32)Def->MinPanicTier, (int32)Def->MaxPanicTier);
			OutDebugRows.Add(Row);
			continue;
		}

		if (!ContextTags.HasAll(Def->RequiredContexts))
		{
			FGameplayTagContainer Missing = Def->RequiredContexts;
			Missing.RemoveTags(ContextTags);
			Row.FilterReason = FString::Printf(TEXT("Missing RequiredContexts: %s"), *Missing.ToStringSimple());
			OutDebugRows.Add(Row);
			continue;
		}

		if (ContextTags.HasAny(Def->BlockedContexts))
		{
			FGameplayTagContainer Hit = ContextTags.Filter(Def->BlockedContexts);
			Row.FilterReason = FString::Printf(TEXT("Hit BlockedContexts: %s"), *Hit.ToStringSimple());
			OutDebugRows.Add(Row);
			continue;
		}

		float W = Def->BaseWeight;
		if (W <= 0.f)
		{
			Row.FilterReason = TEXT("BaseWeight <= 0");
			OutDebugRows.Add(Row);
			continue;
		}

		Row.TierMult = (1.0f + 0.08f * float(PanicTier));
		W *= Row.TierMult;

		Row.GhostMult = 1.f;
		if (GhostProfile)
		{
			if (const float* Mult = GhostProfile->TagMultipliers.Find(Def->ScareTag))
			{
				Row.GhostMult = FMath::Max(0.f, *Mult);
				W *= Row.GhostMult;
			}
		}

		Row.ContextMult = 1.f;
		if (Def->BoostMultiplier > 0.f && Def->BoostContexts.Num() > 0 && ContextTags.HasAny(Def->BoostContexts))
		{
			Row.ContextMult = Def->BoostMultiplier;
			W *= Row.ContextMult;
		}

		Row.RepeatMult = 1.f;
		if (const float* LastTime = LastTagTimeSeconds.Find(Def->ScareTag))
		{
			const float Delta = NowSeconds - *LastTime;
			if (Delta >= 0.f && Delta < Def->RepeatPenaltySeconds)
			{
				Row.RepeatMult = Def->RepeatPenaltyMultiplier;
				W *= Row.RepeatMult;
			}
		}

		Row.Final = W;

		if (W <= KINDA_SMALL_NUMBER)
		{
			Row.FilterReason = TEXT("FinalWeight <= small");
			OutDebugRows.Add(Row);
			continue;
		}

		Candidates.Add({ Def, W });
		TotalWeight += W;
		OutDebugRows.Add(Row);
	}

	if (Candidates.Num() == 0 || TotalWeight <= KINDA_SMALL_NUMBER)
	{
		if (IsGvTScareDebugEnabled())
		{
			UE_LOG(LogTemp, Log, TEXT("[ScareDebug] No candidates. Tier=%d TotalWeight=%.3f"),
				(int32)PanicTier, TotalWeight);
		}
		return nullptr;
	}

	if (IsGvTScareDebugEnabled())
	{
		TArray<FGvTScareWeightDebugRow> Viable;
		Viable.Reserve(OutDebugRows.Num());
		for (const FGvTScareWeightDebugRow& R : OutDebugRows)
		{
			if (R.Final > KINDA_SMALL_NUMBER && R.FilterReason.IsEmpty() && R.Tag.IsValid())
			{
				Viable.Add(R);
			}
		}

		Viable.Sort([](const FGvTScareWeightDebugRow& A, const FGvTScareWeightDebugRow& B)
			{
				return A.Final > B.Final;
			});

		const int32 TopN = FMath::Clamp(CVarGvTScareDebugTopN.GetValueOnGameThread(), 1, 20);

		UE_LOG(LogTemp, Log, TEXT("[ScareDebug] Candidates=%d TotalWeight=%.3f Tier=%d"),
			Viable.Num(), TotalWeight, (int32)PanicTier);

		for (int32 i = 0; i < FMath::Min(TopN, Viable.Num()); ++i)
		{
			const auto& R = Viable[i];
			UE_LOG(LogTemp, Log, TEXT("  #%d %s Final=%.3f (Base=%.2f Tier=%.2f Ghost=%.2f Ctx=%.2f Repeat=%.2f)"),
				i + 1, *R.Tag.ToString(), R.Final, R.Base, R.TierMult, R.GhostMult, R.ContextMult, R.RepeatMult);
		}
	}

	const float Roll = Stream.FRandRange(0.f, TotalWeight);
	float Running = 0.f;

	for (const FGvTWeightedScareCandidate& C : Candidates)
	{
		Running += C.Weight;
		if (Roll <= Running)
		{
			if (IsGvTScareDebugEnabled())
			{
				UE_LOG(LogTemp, Log, TEXT("[ScareDebug] Picked=%s Roll=%.3f / %.3f"),
					*C.Def->ScareTag.ToString(), Roll, TotalWeight);
			}
			return C.Def.Get();
		}
	}

	if (IsGvTScareDebugEnabled())
	{
		UE_LOG(LogTemp, Log, TEXT("[ScareDebug] Picked=LAST (fallback) Roll=%.3f / %.3f"),
			Roll, TotalWeight);
	}

	return Candidates.Last().Def.Get();
}

float UGvTScareSubsystem::ComputeFinalWeight(
	const UGvTScareDefinition* Def,
	const FGameplayTagContainer& ContextTags,
	const UGvTGhostProfileAsset* GhostProfile,
	uint8 PanicTier,
	const TMap<FGameplayTag, float>& LastTagTimeSeconds,
	float NowSeconds
) const
{
	if (!Def)
	{
		return 0.f;
	}

	float W = Def->BaseWeight;
	if (W <= 0.f)
	{
		return 0.f;
	}

	W *= (1.0f + 0.08f * float(PanicTier));

	if (GhostProfile)
	{
		if (const float* Mult = GhostProfile->TagMultipliers.Find(Def->ScareTag))
		{
			W *= FMath::Max(0.f, *Mult);
		}
	}

	if (const float* LastTime = LastTagTimeSeconds.Find(Def->ScareTag))
	{
		const float Delta = NowSeconds - *LastTime;
		if (Delta >= 0.f && Delta < Def->RepeatPenaltySeconds)
		{
			W *= Def->RepeatPenaltyMultiplier;
		}
	}

	if (Def->BoostMultiplier > 0.f && Def->BoostContexts.Num() > 0)
	{
		if (ContextTags.HasAny(Def->BoostContexts))
		{
			W *= Def->BoostMultiplier;
		}
	}

	return W;
}

static bool GvTScareDebugEnabled()
{
	return CVarGvTScareDebug.GetValueOnGameThread() != 0;
}

bool UGvTScareSubsystem::IsScareDebugEnabled()
{
	return GvTScareDebugEnabled();
}