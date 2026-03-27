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
		return FGameplayTag::RequestGameplayTag(TEXT("Scare.RearAudioSting"));
	}

	FGameplayTag GvTScareTags::GhostScream()
	{
		return FGameplayTag::RequestGameplayTag(TEXT("Scare.GhostScream"));
	}

	FGameplayTag DoorSlamBehind()
	{
		return FGameplayTag::RequestGameplayTag(TEXT("Scare.DoorSlamBehind"));
	}
}