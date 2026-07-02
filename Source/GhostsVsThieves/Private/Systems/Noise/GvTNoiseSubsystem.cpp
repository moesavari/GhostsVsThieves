#include "Systems/Noise/GvTNoiseSubsystem.h"

void UGvTNoiseSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    UE_LOG(LogTemp, Log, TEXT("GvT Noise Subsystem Initialized"));
}

void UGvTNoiseSubsystem::EmitNoise(const FGvTNoiseEvent& InEvent)
{
	FGvTNoiseEvent E = InEvent;
	E.TimeSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;

	UE_LOG(LogTemp, Log, TEXT("Noise [%s] at %s | R=%.1f | L=%.2f"),
		*E.NoiseTag.ToString(),
		*E.Location.ToString(),
		E.Radius,
		E.Loudness);

	RecentEvents.Add(E);

	if (RecentEvents.Num() > MaxRecentEvents)
	{
		const int32 Extra = RecentEvents.Num() - MaxRecentEvents;
		RecentEvents.RemoveAt(0, Extra, false);
	}

	UE_LOG(LogTemp, Warning, TEXT("[Noise] Stored %s. RecentEvents=%d Time=%.2f"),
		*E.NoiseTag.ToString(),
		RecentEvents.Num(),
		E.TimeSeconds);
}

int32 UGvTNoiseSubsystem::GetRecentTagCount(FGameplayTag Tag, float WindowSeconds) const
{
	if (!Tag.IsValid()) return 0;
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;

	int32 Count = 0;
	for (int32 i = RecentEvents.Num() - 1; i >= 0; --i)
	{
		const FGvTNoiseEvent& E = RecentEvents[i];
		if ((Now - E.TimeSeconds) > WindowSeconds) break;
		if (E.NoiseTag == Tag) Count++;
	}
	return Count;
}

float UGvTNoiseSubsystem::GetRecentTagLoudnessSum(FGameplayTag Tag, float WindowSeconds) const
{
	if (!Tag.IsValid()) return 0.f;
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;

	float Sum = 0.f;
	for (int32 i = RecentEvents.Num() - 1; i >= 0; --i)
	{
		const FGvTNoiseEvent& E = RecentEvents[i];
		if ((Now - E.TimeSeconds) > WindowSeconds) break;
		if (E.NoiseTag == Tag) Sum += E.Loudness;
	}
	return Sum;
}

int32 UGvTNoiseSubsystem::GetRecentAnyCount(const FGameplayTagContainer& AnyTags, float WindowSeconds) const
{
	if (AnyTags.IsEmpty()) return 0;
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;

	int32 Count = 0;
	for (int32 i = RecentEvents.Num() - 1; i >= 0; --i)
	{
		const FGvTNoiseEvent& E = RecentEvents[i];
		if ((Now - E.TimeSeconds) > WindowSeconds) break;
		if (AnyTags.HasTag(E.NoiseTag)) Count++;
	}
	return Count;
}

bool UGvTNoiseSubsystem::TryGetBestRecentNoiseNearLocation(
	const FVector& ListenerLocation,
	float HearingRadius,
	float MemorySeconds,
	FVector& OutNoiseLocation,
	FGameplayTag& OutNoiseTag,
	float& OutScore) const
{
	OutNoiseLocation = FVector::ZeroVector;
	OutNoiseTag = FGameplayTag();
	OutScore = 0.f;

	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	const float HearingRadiusSq = FMath::Square(HearingRadius);

	bool bFound = false;

	for (int32 i = RecentEvents.Num() - 1; i >= 0; --i)
	{
		const FGvTNoiseEvent& E = RecentEvents[i];

		const float Age = Now - E.TimeSeconds;
		if (Age > MemorySeconds)
		{
			break;
		}

		const float DistSq = FVector::DistSquared(ListenerLocation, E.Location);
		const float EffectiveRadius = HearingRadius + E.Radius;

		if (DistSq > FMath::Square(EffectiveRadius))
		{
			continue;
		}

		const float AgeAlpha = 1.f - FMath::Clamp(Age / FMath::Max(MemorySeconds, 0.01f), 0.f, 1.f);
		const float DistAlpha = 1.f - FMath::Clamp(FMath::Sqrt(DistSq) / FMath::Max(EffectiveRadius, 1.f), 0.f, 1.f);
		const float Score = E.Loudness * AgeAlpha * FMath::Max(DistAlpha, 0.1f);

		if (!bFound || Score > OutScore)
		{
			bFound = true;
			OutNoiseLocation = E.Location;
			OutNoiseTag = E.NoiseTag;
			OutScore = Score;
		}
	}

	return bFound;
}