#include "Systems/Audio/GvTHouseVoiceDirector.h"

#include "Gameplay/Characters/Thieves/GvTThiefCharacter.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "Sound/SoundBase.h"

AGvTHouseVoiceDirector::AGvTHouseVoiceDirector()
{
	bReplicates = true;
	SetReplicateMovement(false);
}

void AGvTHouseVoiceDirector::TryPlayFirstEntryVoice(AActor* EnteringActor)
{
	const AGvTThiefCharacter* Thief = Cast<AGvTThiefCharacter>(EnteringActor);
	if (!HasAuthority() || bHasPlayedFirstEntryVoice || !Thief || Thief->IsDead())
	{
		return;
	}

	TArray<USoundBase*> ValidSounds;
	for (USoundBase* Sound : FirstEntrySounds)
	{
		if (Sound)
		{
			ValidSounds.Add(Sound);
		}
	}

	if (ValidSounds.IsEmpty())
	{
		return;
	}

	bHasPlayedFirstEntryVoice = true;
	Multicast_PlayHouseVoice(ValidSounds[FMath::RandRange(0, ValidSounds.Num() - 1)], GetActorLocation());
	ForceNetUpdate();
}

void AGvTHouseVoiceDirector::Multicast_PlayHouseVoice_Implementation(USoundBase* Sound, FVector WorldLocation)
{
	if (Sound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, Sound, WorldLocation);
	}
}

void AGvTHouseVoiceDirector::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AGvTHouseVoiceDirector, bHasPlayedFirstEntryVoice);
}
