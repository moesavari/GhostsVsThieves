#include "Scare/UGvTScareSubsystem.h"
#include "Scare/GvTScareDefinition.h"
#include "Scare/UGvTGhostProfileAsset.h"

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

	// Gentle tier boost (don’t turn weights into chaos)
	W *= (1.0f + 0.08f * float(PanicTier));

	// Ghost personality bias
	if (GhostProfile)
	{
		if (const float* Mult = GhostProfile->TagMultipliers.Find(Def->ScareTag))
		{
			W *= FMath::Max(0.f, *Mult);
		}
	}

	// Anti-repeat penalty
	if (const float* LastTime = LastTagTimeSeconds.Find(Def->ScareTag))
	{
		const float Delta = NowSeconds - *LastTime;
		if (Delta >= 0.f && Delta < Def->RepeatPenaltySeconds)
		{
			W *= Def->RepeatPenaltyMultiplier;
		}
	}

	return W;
}
