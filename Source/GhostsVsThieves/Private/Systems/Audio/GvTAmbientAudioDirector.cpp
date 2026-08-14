#include "Systems/Audio/GvTAmbientAudioDirector.h"
#include "Systems/Audio/GvTAmbientAudioPoint.h"
#include "Components/AudioComponent.h"
#include "Components/SceneComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Systems/World/GvTHouseBoundsLibrary.h"

AGvTAmbientAudioDirector::AGvTAmbientAudioDirector()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.20f;
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
		CurrentIndoorBlend = IsLocalPlayerInsideHouse() ? 1.0f : 0.0f;
		StartBaseLoops();

		if (bAllowRandomShots)
		{
			ScheduleNextRandomShot();
		}
	}
}

void AGvTAmbientAudioDirector::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (CanPlayLocalAudio())
	{
		UpdateZoneMix(DeltaSeconds);
	}
}

bool AGvTAmbientAudioDirector::CanPlayLocalAudio() const
{
	return GetNetMode() != NM_DedicatedServer;
}

void AGvTAmbientAudioDirector::CacheAmbientPoints()
{
	IndoorAmbientPoints.Reset();
	OutdoorAmbientPoints.Reset();

	TArray<AActor*> Found;
	UGameplayStatics::GetAllActorsOfClass(this, AGvTAmbientAudioPoint::StaticClass(), Found);

	for (AActor* Actor : Found)
	{
		if (AGvTAmbientAudioPoint* Point = Cast<AGvTAmbientAudioPoint>(Actor))
		{
			if (Point->IsPointEnabled())
			{
				if (Point->GetAmbientZone() == EGvTAmbientZone::Outdoor)
				{
					OutdoorAmbientPoints.Add(Point);
				}
				else
				{
					IndoorAmbientPoints.Add(Point);
				}
			}
		}
	}
}

void AGvTAmbientAudioDirector::StartBaseLoops()
{
	ActiveBaseLoopComponents.Reset();
	ActiveOutdoorLoopComponents.Reset();
	StartLoopSet(BaseAmbientLoops, BaseLoopPitch, ActiveBaseLoopComponents);
	StartLoopSet(OutdoorAmbientLoops, OutdoorLoopPitch, ActiveOutdoorLoopComponents);
	UpdateZoneMix(0.0f);
}

void AGvTAmbientAudioDirector::StartLoopSet(const TArray<TObjectPtr<USoundBase>>& Sounds, float Pitch, TArray<TObjectPtr<UAudioComponent>>& OutComponents)
{
	for (USoundBase* LoopSound : Sounds)
	{
		if (!LoopSound)
		{
			continue;
		}

		if (UAudioComponent* AC = UGameplayStatics::SpawnSound2D(
			this,
			LoopSound,
			1.0f,
			Pitch,
			0.0f,
			nullptr,
			false,
			true))
		{
			OutComponents.Add(AC);
		}
	}
}

bool AGvTAmbientAudioDirector::IsLocalPlayerInsideHouse() const
{
	if (!GetWorld())
	{
		return true;
	}

	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* PC = It->Get();
		const APawn* Pawn = PC && PC->IsLocalController() ? PC->GetPawn() : nullptr;
		if (!Pawn)
		{
			continue;
		}

		bool bFoundHouseBounds = false;
		const bool bInside = UGvTHouseBoundsLibrary::IsLocationInsideHouse(this, Pawn->GetActorLocation(), bFoundHouseBounds);
		return !bFoundHouseBounds || bInside;
	}

	return true;
}

void AGvTAmbientAudioDirector::UpdateZoneMix(float DeltaSeconds)
{
	const float TargetIndoorBlend = IsLocalPlayerInsideHouse() ? 1.0f : 0.0f;
	CurrentIndoorBlend = DeltaSeconds > 0.0f
		? FMath::FInterpTo(CurrentIndoorBlend, TargetIndoorBlend, DeltaSeconds, ZoneCrossfadeSpeed)
		: TargetIndoorBlend;

	const float IndoorVolume = BaseLoopVolume * FMath::Lerp(IndoorVolumeWhileOutside, 1.0f, CurrentIndoorBlend);
	const float OutdoorVolume = OutdoorLoopVolume * FMath::Lerp(1.0f, OutdoorVolumeWhileInside, CurrentIndoorBlend);

	for (UAudioComponent* Component : ActiveBaseLoopComponents)
	{
		if (Component)
		{
			Component->SetVolumeMultiplier(IndoorVolume);
		}
	}

	for (UAudioComponent* Component : ActiveOutdoorLoopComponents)
	{
		if (Component)
		{
			Component->SetVolumeMultiplier(OutdoorVolume);
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

FVector AGvTAmbientAudioDirector::ResolveAmbientPlaybackLocation(const FVector& FallbackLocation, bool bIndoor) const
{
	const TArray<TWeakObjectPtr<AGvTAmbientAudioPoint>>& ZonePoints = bIndoor ? IndoorAmbientPoints : OutdoorAmbientPoints;
	if (!bUseAmbientPoints || ZonePoints.Num() == 0)
	{
		return FallbackLocation;
	}

	TArray<TWeakObjectPtr<AGvTAmbientAudioPoint>> ValidPoints;
	for (const TWeakObjectPtr<AGvTAmbientAudioPoint>& Point : ZonePoints)
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

	const bool bIndoor = IsLocalPlayerInsideHouse();
	const TArray<FGvTAmbientSoundEntry>& Pool = !bIndoor && OutdoorRandomAmbientShots.Num() > 0
		? OutdoorRandomAmbientShots
		: RandomAmbientShots;

	if (const FGvTAmbientSoundEntry* Entry = PickWeightedEntry(Pool))
	{
		const FVector Loc = ResolveAmbientPlaybackLocation(GetActorLocation(), bIndoor);
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
