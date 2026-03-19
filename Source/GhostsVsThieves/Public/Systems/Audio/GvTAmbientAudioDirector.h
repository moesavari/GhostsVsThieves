#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "GvTAmbientAudioDirector.generated.h"

class USoundBase;
class UAudioComponent;
class USceneComponent;
class AGvTAmbientAudioPoint;

USTRUCT(BlueprintType)
struct FGvTAmbientSoundEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Ambient")
	TObjectPtr<USoundBase> Sound = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Ambient", meta = (ClampMin = "0.0"))
	float Weight = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Ambient", meta = (ClampMin = "0.0"))
	float VolumeMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Ambient", meta = (ClampMin = "0.1"))
	float PitchMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Ambient")
	bool bPlay2D = false;
};

USTRUCT(BlueprintType)
struct FGvTTaggedAmbientAccentSet
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Ambient")
	FGameplayTag ScareTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Ambient")
	TArray<FGvTAmbientSoundEntry> StartAccents;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Ambient")
	TArray<FGvTAmbientSoundEntry> EndAccents;
};

UCLASS(BlueprintType)
class GHOSTSVSTHIEVES_API AGvTAmbientAudioDirector : public AActor
{
	GENERATED_BODY()

public:
	AGvTAmbientAudioDirector();

	virtual void BeginPlay() override;

	UFUNCTION(BlueprintCallable, Category = "GvT|Ambient")
	void HandleScareStarted(FGameplayTag ScareTag, FVector WorldLocation, float Intensity01 = 1.0f);

	UFUNCTION(BlueprintCallable, Category = "GvT|Ambient")
	void HandleScareEnded(FGameplayTag ScareTag, FVector WorldLocation, float Intensity01 = 1.0f);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GvT|Ambient")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GvT|Ambient|Base")
	TArray<TObjectPtr<USoundBase>> BaseAmbientLoops;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GvT|Ambient|Base", meta = (ClampMin = "0.0"))
	float BaseLoopVolume = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GvT|Ambient|Base", meta = (ClampMin = "0.1"))
	float BaseLoopPitch = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GvT|Ambient|Random")
	TArray<FGvTAmbientSoundEntry> RandomAmbientShots;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GvT|Ambient|Scare")
	TArray<FGvTAmbientSoundEntry> GenericScareStartAccents;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GvT|Ambient|Scare")
	TArray<FGvTAmbientSoundEntry> GenericScareEndAccents;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GvT|Ambient|Scare")
	TArray<FGvTTaggedAmbientAccentSet> TaggedScareAccentSets;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GvT|Ambient|Timing", meta = (ClampMin = "0.1"))
	float MinRandomShotDelay = 7.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GvT|Ambient|Timing", meta = (ClampMin = "0.1"))
	float MaxRandomShotDelay = 16.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GvT|Ambient|Timing", meta = (ClampMin = "0.0"))
	float PostScareSuppressionSeconds = 2.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GvT|Ambient|Random")
	bool bUseAmbientPoints = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GvT|Ambient|Random")
	bool bAllowRandomShots = true;

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_PlayScareAccent(bool bIsStart, FGameplayTag ScareTag, FVector WorldLocation, float Intensity01);

private:
	UPROPERTY(Transient)
	TArray<TObjectPtr<UAudioComponent>> ActiveBaseLoopComponents;

	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<AGvTAmbientAudioPoint>> AmbientPoints;

	FTimerHandle TimerHandle_RandomShot;
	float SuppressRandomUntilTime = 0.0f;

	void CacheAmbientPoints();
	void StartBaseLoops();
	void ScheduleNextRandomShot();
	void PlayRandomAmbientShot();
	void PlaySoundEntry(const FGvTAmbientSoundEntry& Entry, const FVector& WorldLocation, float Intensity01);
	const FGvTAmbientSoundEntry* PickWeightedEntry(const TArray<FGvTAmbientSoundEntry>& Entries) const;
	const FGvTTaggedAmbientAccentSet* FindTaggedSet(FGameplayTag ScareTag) const;
	FVector ResolveAmbientPlaybackLocation(const FVector& FallbackLocation) const;
	bool CanPlayLocalAudio() const;
};