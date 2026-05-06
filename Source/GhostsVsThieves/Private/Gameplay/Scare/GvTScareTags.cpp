#include "Gameplay/Scare/GvTScareTags.h"

namespace GvTScareTags
{
	FGameplayTag Mirror()
	{
		return FGameplayTag::RequestGameplayTag(TEXT("Scare.Mirror"));
	}

	FGameplayTag CrawlerChase()
	{
		return FGameplayTag::RequestGameplayTag(TEXT("Scare.Crawler.Chase"));
	}

	FGameplayTag CrawlerOverhead()
	{
		return FGameplayTag::RequestGameplayTag(TEXT("Scare.Crawler.Overhead"));
	}

	FGameplayTag LightChase()
	{
		return FGameplayTag::RequestGameplayTag(TEXT("Scare.LightChase"));
	}

	FGameplayTag GvTScareTags::RearAudioSting()
	{
		// Legacy accessor kept so older code paths still compile, but this is now a real GhostScare.
		return GhostScare_AudioRear();
	}

	FGameplayTag GvTScareTags::GhostScream()
	{
		// Legacy accessor kept so older code paths still compile, but this is now a real GhostScare.
		return GhostScare_Scream();
	}

	FGameplayTag DoorSlamBehind()
	{
		return FGameplayTag::RequestGameplayTag(TEXT("Scare.DoorSlamBehind"));
	}

	FGameplayTag GhostScare_Close()
	{
		return FGameplayTag::RequestGameplayTag(TEXT("GhostScare.Close"));
	}

	FGameplayTag GhostScare_Scream()
	{
		return FGameplayTag::RequestGameplayTag(TEXT("GhostScare.Scream"));
	}

	FGameplayTag GhostScare_AudioRear()
	{
		return FGameplayTag::RequestGameplayTag(TEXT("GhostScare.AudioRear"));
	}

	FGameplayTag GhostEvent_DoorSlam()
	{
		return FGameplayTag::RequestGameplayTag(TEXT("GhostEvent.DoorSlam"));
	}

	FGameplayTag GhostEvent_Mirror()
	{
		return FGameplayTag::RequestGameplayTag(TEXT("GhostEvent.Mirror"));
	}

	FGameplayTag GhostHaunt_Chase()
	{
		return FGameplayTag::RequestGameplayTag(TEXT("GhostHaunt.Chase"));
	}
}