#include "Systems/Audio/GvTAmbientAudioDirector.h"
#include "Systems/Audio/GvTAmbientAudioPoint.h"
#include "Components/AudioComponent.h"
#include "Components/SceneComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

AGvTAmbientAudioDirector::AGvTAmbientAudioDirector()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
}

void AGvTAmbientAudioDirector::BeginPlay()
{
	Super::BeginPlay();

	CacheAmbientPoints();

	if (CanPlayLocalAudio())
	{
		StartBaseLoops();

		if (bAllowRandomShots)
		{
			ScheduleNextRandomShot();
		}
	}
}

bool AGvTAmbientAudioDirector::CanPlayLocalAudio() const
{
	return GetNetMode() != NM_DedicatedServer;
}

void AGvTAmbientAudioDirector::CacheAmbientPoints()
{
	AmbientPoints.Reset();

	TArray<AActor*> Found;
	UGameplayStatics::GetAllActorsOfClass(this, AGvTAmbientAudioPoint::StaticClass(), Found);

	for (AActor* Actor : Found)
	{
		if (AGvTAmbientAudioPoint* Point = Cast<AGvTAmbientAudioPoint>(Actor))
		{
			if (Point->IsPointEnabled())
			{
				AmbientPoints.Add(Point);
			}
		}
	}
}

void AGvTAmbientAudioDirector::StartBaseLoops()
{
	ActiveBaseLoopComponents.Reset();

	for (USoundBase* LoopSound : BaseAmbientLoops)
	{
		if (!LoopSound)
		{
			continue;
		}

		if (UAudioComponent* AC = UGameplayStatics::SpawnSoundAtLocation(
			this,
			LoopSound,
			GetActorLocation(),
			GetActorRotation(),
			BaseLoopVolume,
			BaseLoopPitch,
			0.0f,
			nullptr,
			nullptr,
			true))
		{
			//AC->bLooping = true;
			AC->Play();
			ActiveBaseLoopComponents.Add(AC);
		}
	}
}

void AGvTAmbientAudioDirector::ScheduleNextRandomShot()
{
	if (!GetWorld() || !bAllowRandomShots)
	{
		return;
	}

	const float Delay = FMath::FRandRange(MinRandomShotDelay, MaxRandomShotDelay);
	GetWorldTimerManager().SetTimer(
		TimerHandle_RandomShot,
		this,
		&AGvTAmbientAudioDirector::PlayRandomAmbientShot,
		Delay,
		false);
}

const FGvTAmbientSoundEntry* AGvTAmbientAudioDirector::PickWeightedEntry(const TArray<FGvTAmbientSoundEntry>& Entries) const
{
	float TotalWeight = 0.0f;
	for (const FGvTAmbientSoundEntry& Entry : Entries)
	{
		if (Entry.Sound && Entry.Weight > 0.0f)
		{
			TotalWeight += Entry.Weight;
		}
	}

	if (TotalWeight <= KINDA_SMALL_NUMBER)
	{
		return nullptr;
	}

	const float Roll = FMath::FRandRange(0.0f, TotalWeight);
	float Running = 0.0f;

	for (const FGvTAmbientSoundEntry& Entry : Entries)
	{
		if (!Entry.Sound || Entry.Weight <= 0.0f)
		{
			continue;
		}

		Running += Entry.Weight;
		if (Roll <= Running)
		{
			return &Entry;
		}
	}

	return nullptr;
}

FVector AGvTAmbientAudioDirector::ResolveAmbientPlaybackLocation(const FVector& FallbackLocation) const
{
	if (!bUseAmbientPoints || AmbientPoints.Num() == 0)
	{
		return FallbackLocation;
	}

	TArray<TWeakObjectPtr<AGvTAmbientAudioPoint>> ValidPoints;
	for (const TWeakObjectPtr<AGvTAmbientAudioPoint>& Point : AmbientPoints)
	{
		if (Point.IsValid() && Point->IsPointEnabled())
		{
			ValidPoints.Add(Point);
		}
	}

	if (ValidPoints.Num() == 0)
	{
		return FallbackLocation;
	}

	const int32 Index = FMath::RandRange(0, ValidPoints.Num() - 1);
	return ValidPoints[Index].Get()->GetActorLocation();
}

void AGvTAmbientAudioDirector::PlaySoundEntry(const FGvTAmbientSoundEntry& Entry, const FVector& WorldLocation, float Intensity01)
{
	if (!Entry.Sound || !CanPlayLocalAudio())
	{
		return;
	}

	const float Volume = Entry.VolumeMultiplier * FMath::Lerp(0.75f, 1.25f, FMath::Clamp(Intensity01, 0.0f, 1.0f));

	if (Entry.bPlay2D)
	{
		UGameplayStatics::SpawnSound2D(
			this,
			Entry.Sound,
			Volume,
			Entry.PitchMultiplier);
	}
	else
	{
		UGameplayStatics::PlaySoundAtLocation(
			this,
			Entry.Sound,
			WorldLocation,
			Volume,
			Entry.PitchMultiplier);
	}
}

void AGvTAmbientAudioDirector::PlayRandomAmbientShot()
{
	if (!GetWorld())
	{
		return;
	}

	const float Now = GetWorld()->GetTimeSeconds();
	if (Now < SuppressRandomUntilTime)
	{
		ScheduleNextRandomShot();
		return;
	}

	if (const FGvTAmbientSoundEntry* Entry = PickWeightedEntry(RandomAmbientShots))
	{
		const FVector Loc = ResolveAmbientPlaybackLocation(GetActorLocation());
		PlaySoundEntry(*Entry, Loc, 1.0f);
	}

	ScheduleNextRandomShot();
}

const FGvTTaggedAmbientAccentSet* AGvTAmbientAudioDirector::FindTaggedSet(FGameplayTag ScareTag) const
{
	for (const FGvTTaggedAmbientAccentSet& Set : TaggedScareAccentSets)
	{
		if (Set.ScareTag.IsValid() && Set.ScareTag.MatchesTagExact(ScareTag))
		{
			return &Set;
		}
	}

	return nullptr;
}

void AGvTAmbientAudioDirector::HandleScareStarted(FGameplayTag ScareTag, FVector WorldLocation, float Intensity01)
{
	SuppressRandomUntilTime = GetWorld() ? (GetWorld()->GetTimeSeconds() + PostScareSuppressionSeconds) : 0.0f;

	if (HasAuthority())
	{
		Multicast_PlayScareAccent(true, ScareTag, WorldLocation, Intensity01);
	}
	else
	{
		Multicast_PlayScareAccent_Implementation(true, ScareTag, WorldLocation, Intensity01);
	}
}

void AGvTAmbientAudioDirector::HandleScareEnded(FGameplayTag ScareTag, FVector WorldLocation, float Intensity01)
{
	if (HasAuthority())
	{
		Multicast_PlayScareAccent(false, ScareTag, WorldLocation, Intensity01);
	}
	else
	{
		Multicast_PlayScareAccent_Implementation(false, ScareTag, WorldLocation, Intensity01);
	}
}

void AGvTAmbientAudioDirector::Multicast_PlayScareAccent_Implementation(bool bIsStart, FGameplayTag ScareTag, FVector WorldLocation, float Intensity01)
{
	const FGvTTaggedAmbientAccentSet* TaggedSet = FindTaggedSet(ScareTag);

	if (TaggedSet)
	{
		const TArray<FGvTAmbientSoundEntry>& Pool = bIsStart ? TaggedSet->StartAccents : TaggedSet->EndAccents;
		if (const FGvTAmbientSoundEntry* Entry = PickWeightedEntry(Pool))
		{
			PlaySoundEntry(*Entry, WorldLocation, Intensity01);
			return;
		}
	}

	const TArray<FGvTAmbientSoundEntry>& FallbackPool = bIsStart ? GenericScareStartAccents : GenericScareEndAccents;
	if (const FGvTAmbientSoundEntry* Entry = PickWeightedEntry(FallbackPool))
	{
		PlaySoundEntry(*Entry, WorldLocation, Intensity01);
	}
}