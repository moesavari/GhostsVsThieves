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
}