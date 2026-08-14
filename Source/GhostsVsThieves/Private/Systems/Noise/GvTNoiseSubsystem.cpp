#include "Systems/Noise/GvTNoiseSubsystem.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "Gameplay/Ghosts/GvTHauntGhostBase.h"
#include "Gameplay/Ghosts/GvTGhostPerceptionComponent.h"
#include "Systems/World/GvTHouseBoundsLibrary.h"

void UGvTNoiseSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    UE_LOG(LogTemp, Log, TEXT("GvT Noise Subsystem Initialized"));
}

void UGvTNoiseSubsystem::EmitNoise(const FGvTNoiseEvent& InEvent)
{
	bool bFoundHouseBounds = false;
	if (!UGvTHouseBoundsLibrary::IsLocationInsideHouse(this, InEvent.Location, bFoundHouseBounds))
	{
		UE_LOG(LogTemp, Verbose, TEXT("[Noise] Ignored outside HouseBounds Tag=%s Location=%s"), *InEvent.NoiseTag.ToString(), *InEvent.Location.ToCompactString());
		return;
	}

	FGvTNoiseEvent E = InEvent;
	E.TimeSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	E.EventId = NextEventId++;

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

#if GVT_ENABLE_DEBUG_TOOLS && !UE_BUILD_SHIPPING
	if (bDebugGhostHearingOnScreen && GetWorld())
	{
		for (TActorIterator<AGvTHauntGhostBase> It(GetWorld()); It; ++It)
		{
			AGvTHauntGhostBase* Ghost = *It;
			if (!IsValid(Ghost))
			{
				continue;
			}

			const float Dist = FVector::Dist(Ghost->GetActorLocation(), E.Location);
			const UGvTGhostPerceptionComponent* Perception = Ghost->FindComponentByClass<UGvTGhostPerceptionComponent>();
			const float GhostHearingRadius = Perception ? Perception->HearingRadius : 0.f;
			const float EffectiveRadius = (GhostHearingRadius + E.Radius) * GhostHearingLoudnessRadiusMultiplier;
			const bool bHeard = Dist <= EffectiveRadius;

			const FString Msg = FString::Printf(
				TEXT("[Ghost Hearing] %s %s %s | Dist %.0f / Radius %.0f | Loud %.2f"),
				*GetNameSafe(Ghost),
				bHeard ? TEXT("HEARD") : TEXT("ignored"),
				*E.NoiseTag.ToString(),
				Dist,
				EffectiveRadius,
				E.Loudness);

			UE_LOG(LogTemp, Warning, TEXT("%s"), *Msg);

			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(
					-1,
					DebugGhostHearingMessageTime,
					bHeard ? FColor::Red : FColor::Silver,
					Msg);
			}
		}
	}
#endif
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
	float& OutScore,
	int64& OutEventId) const
{
	OutNoiseLocation = FVector::ZeroVector;
	OutNoiseTag = FGameplayTag();
	OutScore = 0.f;
	OutEventId = 0;

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
			OutEventId = E.EventId;
		}
	}

	return bFound;
}
