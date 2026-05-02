#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

namespace GvTScareTags
{
	GHOSTSVSTHIEVES_API FGameplayTag Mirror();
	GHOSTSVSTHIEVES_API FGameplayTag CrawlerChase();
	GHOSTSVSTHIEVES_API FGameplayTag CrawlerOverhead();
	GHOSTSVSTHIEVES_API FGameplayTag LightChase();
	GHOSTSVSTHIEVES_API FGameplayTag DoorSlamBehind();
	GHOSTSVSTHIEVES_API FGameplayTag RearAudioSting();
	GHOSTSVSTHIEVES_API FGameplayTag GhostScream();

	GHOSTSVSTHIEVES_API FGameplayTag GhostScare_Close();
	GHOSTSVSTHIEVES_API FGameplayTag GhostScare_Scream();
	GHOSTSVSTHIEVES_API FGameplayTag GhostScare_AudioRear();

	GHOSTSVSTHIEVES_API FGameplayTag GhostEvent_DoorSlam();
	GHOSTSVSTHIEVES_API FGameplayTag GhostEvent_Mirror();

	GHOSTSVSTHIEVES_API FGameplayTag GhostHaunt_Chase();
}