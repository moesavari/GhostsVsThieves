#include "GvTGameInstance.h"
#include "Blueprint/UserWidget.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/ConfigCacheIni.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundMix.h"
#include "UObject/UObjectGlobals.h"

namespace GvTAudioSettings
{
	static const TCHAR* Section = TEXT("HauntedHeists.Audio");
	static const TCHAR* MasterKey = TEXT("MasterVolume");
	static const TCHAR* MusicKey = TEXT("MusicVolume");
	static const TCHAR* SFXKey = TEXT("SFXVolume");
}

void UGvTGameInstance::Init()
{
	Super::Init();
	LoadAudioSettings();
	PreLoadMapHandle = FCoreUObjectDelegates::PreLoadMap.AddUObject(this, &ThisClass::HandlePreLoadMap);
	PostLoadMapHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &ThisClass::HandlePostLoadMap);
}

void UGvTGameInstance::Shutdown()
{
	StoreAudioSettings(true);
	FCoreUObjectDelegates::PreLoadMap.Remove(PreLoadMapHandle);
	FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapHandle);
	Super::Shutdown();
}

void UGvTGameInstance::HandlePreLoadMap(const FString& MapName)
{
	if (!LoadingScreenWidgetClass || LoadingScreenWidget)
	{
		return;
	}

	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		LoadingScreenWidget = CreateWidget<UUserWidget>(PC, LoadingScreenWidgetClass);
		if (LoadingScreenWidget)
		{
			LoadingScreenWidget->AddToViewport(10000);
		}
	}
}

void UGvTGameInstance::HandlePostLoadMap(UWorld* LoadedWorld)
{
	if (LoadingScreenWidget)
	{
		LoadingScreenWidget->RemoveFromParent();
		LoadingScreenWidget = nullptr;
	}

	ApplyAudioSettings();
}

void UGvTGameInstance::SetMasterVolume(float NewVolume)
{
	MasterVolume = FMath::Clamp(NewVolume, 0.0f, 1.0f);
	StoreAudioSettings(false);
	ApplyAudioSettings();
}

void UGvTGameInstance::SetMusicVolume(float NewVolume)
{
	MusicVolume = FMath::Clamp(NewVolume, 0.0f, 1.0f);
	StoreAudioSettings(false);
	ApplyAudioSettings();
}

void UGvTGameInstance::SetSFXVolume(float NewVolume)
{
	SFXVolume = FMath::Clamp(NewVolume, 0.0f, 1.0f);
	StoreAudioSettings(false);
	ApplyAudioSettings();
}

void UGvTGameInstance::SaveAudioSettings()
{
	StoreAudioSettings(true);
}

void UGvTGameInstance::LoadAudioSettings()
{
	if (!GConfig)
	{
		return;
	}

	GConfig->GetFloat(GvTAudioSettings::Section, GvTAudioSettings::MasterKey, MasterVolume, GGameUserSettingsIni);
	GConfig->GetFloat(GvTAudioSettings::Section, GvTAudioSettings::MusicKey, MusicVolume, GGameUserSettingsIni);
	GConfig->GetFloat(GvTAudioSettings::Section, GvTAudioSettings::SFXKey, SFXVolume, GGameUserSettingsIni);

	MasterVolume = FMath::Clamp(MasterVolume, 0.0f, 1.0f);
	MusicVolume = FMath::Clamp(MusicVolume, 0.0f, 1.0f);
	SFXVolume = FMath::Clamp(SFXVolume, 0.0f, 1.0f);
}

void UGvTGameInstance::ApplyAudioSettings()
{
	if (!VolumeSoundMix)
	{
		return;
	}

	UGameplayStatics::SetBaseSoundMix(this, VolumeSoundMix);

	if (MasterSoundClass)
	{
		UGameplayStatics::SetSoundMixClassOverride(this, VolumeSoundMix, MasterSoundClass, MasterVolume, 1.0f, 0.0f, false);
	}

	if (MusicSoundClass)
	{
		UGameplayStatics::SetSoundMixClassOverride(this, VolumeSoundMix, MusicSoundClass, MusicVolume, 1.0f, 0.0f, false);
	}

	if (SFXSoundClass)
	{
		UGameplayStatics::SetSoundMixClassOverride(this, VolumeSoundMix, SFXSoundClass, SFXVolume, 1.0f, 0.0f, false);
	}
}

void UGvTGameInstance::StoreAudioSettings(bool bFlush)
{
	if (!GConfig)
	{
		return;
	}

	GConfig->SetFloat(GvTAudioSettings::Section, GvTAudioSettings::MasterKey, MasterVolume, GGameUserSettingsIni);
	GConfig->SetFloat(GvTAudioSettings::Section, GvTAudioSettings::MusicKey, MusicVolume, GGameUserSettingsIni);
	GConfig->SetFloat(GvTAudioSettings::Section, GvTAudioSettings::SFXKey, SFXVolume, GGameUserSettingsIni);

	if (bFlush)
	{
		GConfig->Flush(false, GGameUserSettingsIni);
	}
}

