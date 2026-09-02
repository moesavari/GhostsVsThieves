// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "GvTGameInstance.generated.h"

class UUserWidget;
class UWorld;
class USoundClass;
class USoundMix;

UCLASS()
class GHOSTSVSTHIEVES_API UGvTGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	virtual void Init() override;
	virtual void Shutdown() override;

	UFUNCTION(BlueprintCallable, Category="GvT|Audio Settings")
	void SetMasterVolume(float NewVolume);

	UFUNCTION(BlueprintCallable, Category="GvT|Audio Settings")
	void SetMusicVolume(float NewVolume);

	UFUNCTION(BlueprintCallable, Category="GvT|Audio Settings")
	void SetSFXVolume(float NewVolume);

	UFUNCTION(BlueprintCallable, Category="GvT|Audio Settings")
	void SaveAudioSettings();

	UFUNCTION(BlueprintCallable, Category="GvT|Input Settings")
	void SetMouseSensitivity(float NewSensitivity);

	UFUNCTION(BlueprintCallable, Category="GvT|Accessibility")
	void SetPanicVisualEffectsEnabled(bool bEnabled);

	UFUNCTION(BlueprintCallable, Category="GvT|Accessibility")
	void SetPanicAudioEffectsEnabled(bool bEnabled);

	UFUNCTION(BlueprintPure, Category="GvT|Audio Settings")
	float GetMasterVolume() const { return MasterVolume; }

	UFUNCTION(BlueprintPure, Category="GvT|Audio Settings")
	float GetMusicVolume() const { return MusicVolume; }

	UFUNCTION(BlueprintPure, Category="GvT|Audio Settings")
	float GetSFXVolume() const { return SFXVolume; }

	UFUNCTION(BlueprintPure, Category="GvT|Input Settings")
	float GetMouseSensitivity() const { return MouseSensitivity; }

	UFUNCTION(BlueprintPure, Category="GvT|Accessibility")
	bool GetPanicVisualEffectsEnabled() const { return bPanicVisualEffectsEnabled; }

	UFUNCTION(BlueprintPure, Category="GvT|Accessibility")
	bool GetPanicAudioEffectsEnabled() const { return bPanicAudioEffectsEnabled; }

	/** Full-screen widget shown during regular map travel. Use an indeterminate Progress Bar or Throbber; Open Level does not expose real progress. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GvT|Loading Screen")
	TSubclassOf<UUserWidget> LoadingScreenWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GvT|Audio Settings")
	TObjectPtr<USoundMix> VolumeSoundMix;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GvT|Audio Settings")
	TObjectPtr<USoundClass> MasterSoundClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GvT|Audio Settings")
	TObjectPtr<USoundClass> MusicSoundClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GvT|Audio Settings")
	TObjectPtr<USoundClass> SFXSoundClass;

private:
	void HandlePreLoadMap(const FString& MapName);
	void HandlePostLoadMap(UWorld* LoadedWorld);
	void LoadAudioSettings();
	void ApplyAudioSettings();
	void StoreAudioSettings(bool bFlush);

	UPROPERTY(Transient)
	float MasterVolume = 0.70f;

	UPROPERTY(Transient)
	float MusicVolume = 0.45f;

	UPROPERTY(Transient)
	float SFXVolume = 0.65f;

	UPROPERTY(Transient)
	float MouseSensitivity = 1.0f;

	UPROPERTY(Transient)
	bool bPanicVisualEffectsEnabled = true;

	UPROPERTY(Transient)
	bool bPanicAudioEffectsEnabled = true;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> LoadingScreenWidget;

	FDelegateHandle PreLoadMapHandle;
	FDelegateHandle PostLoadMapHandle;
};
