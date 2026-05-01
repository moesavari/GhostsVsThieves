#include "Gameplay/Scare/GvTScareTags.h"

namespace GvTScareTags
{
	FGameplayTag Mirror()
	{
		return FGameplayTag::RequestGameplayTag(TEXT("Scare.Mirror"));
	}

	FGameplayTag GhostChase()
	{
		return FGameplayTag::RequestGameplayTag(TEXT("Scare.GhostChase"));
	}

	FGameplayTag GhostScare()
	{
		return FGameplayTag::RequestGameplayTag(TEXT("Scare.GhostScare"));
	}

	FGameplayTag LightChase()
	{
		return FGameplayTag::RequestGameplayTag(TEXT("Scare.LightChase"));
	}

	FGameplayTag RearAudioSting()
	{
		return FGameplayTag::RequestGameplayTag(TEXT("Scare.RearAudioSting"));
	}

	FGameplayTag GhostScream()
	{
		return FGameplayTag::RequestGameplayTag(TEXT("Scare.GhostScream"));
	}

	FGameplayTag DoorSlamBehind()
	{
		return FGameplayTag::RequestGameplayTag(TEXT("Scare.DoorSlamBehind"));
	}
}