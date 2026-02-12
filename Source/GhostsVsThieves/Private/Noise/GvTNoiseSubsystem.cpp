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