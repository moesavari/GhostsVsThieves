#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Gameplay/Scare/GvTScareTypes.h"
#include "GvTDirectorSubsystem.generated.h"

class AGvTDoorActor;

UCLASS()
class GHOSTSVSTHIEVES_API UGvTDirectorSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UPROPERTY(BlueprintReadOnly, Category = "GvT|Director")
	float Heat = 0.f;

	UFUNCTION(BlueprintCallable, Category = "GvT|Director")
	bool DispatchScareEvent(const FGvTScareEvent& Event);

	UFUNCTION(BlueprintCallable, Category = "GvT|Director")
	void StartDirector();

	UFUNCTION(BlueprintCallable, Category = "GvT|Director")
	void StopDirector();

	UFUNCTION()
	void TickDirector();

	UFUNCTION(BlueprintCallable, Category = "GvT|Director")
	FGvTScareEvent MakeMirrorEvent(AActor* Target) const;

	UFUNCTION(BlueprintCallable, Category = "GvT|Director")
	FGvTScareEvent MakeCrawlerOverheadEvent(AActor* Target) const;

	UFUNCTION(BlueprintCallable, Category = "GvT|Director")
	FGvTScareEvent MakeCrawlerChaseEvent(AActor* Target) const;

	UFUNCTION(BlueprintCallable, Category = "GvT|Director")
	FGvTScareEvent MakeLightChaseEvent(AActor* Target) const;

	UFUNCTION(BlueprintCallable, Category = "GvT|Director")
	FGvTScareEvent MakeRearAudioStingEvent(AActor* Target) const;

	UFUNCTION(BlueprintCallable, Category = "GvT|Director")
	FGvTScareEvent MakeGhostScreamEvent(AActor* Target) const;

	UFUNCTION(BlueprintCallable, Category = "GvT|Director")
	FGvTScareEvent MakeDoorSlamBehindEvent(AActor* Target, AActor* DoorActor) const;

	UFUNCTION(BlueprintPure, Category = "GvT|Director|Tension")
	float GetHouseTension01() const { return HouseTension01; }

	UFUNCTION(BlueprintPure, Category = "GvT|Director|Tension")
	float GetCurrentGlobalHauntCooldown() const;

	UFUNCTION(BlueprintCallable, Category = "GvT|Director")
	AActor* FindBestDoorSlamDoor(AActor* Target) const;

protected:
	UPROPERTY(EditAnywhere, Category = "GvT|Director|Runtime")
	bool bEnableAutoHaunts = true;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|Runtime", meta = (ClampMin = "0.10"))
	float DirectorTickInterval = 4.0f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|Runtime", meta = (ClampMin = "0.10"))
	float GlobalHauntCooldown = 10.0f;

	UPROPERTY(BlueprintReadOnly, Category = "GvT|Director|Runtime")
	float LastGlobalHauntTime = -1000.0f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|ScareTuning|Mirror", meta = (ClampMin = "0.10"))
	float MirrorDuration = 1.5f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|ScareTuning|Mirror", meta = (ClampMin = "0.0"))
	float MirrorPanicAmount = 12.0f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|ScareTuning|Mirror")
	bool bMirrorTriggersLocalFlicker = true;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|ScareTuning|CrawlerOverhead", meta = (ClampMin = "0.10"))
	float CrawlerOverheadDuration = 1.0f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|ScareTuning|CrawlerOverhead", meta = (ClampMin = "0.0"))
	float CrawlerOverheadPanicAmount = 10.0f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|ScareTuning|CrawlerOverhead")
	bool bCrawlerOverheadTriggersLocalFlicker = false;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|ScareTuning|CrawlerChase", meta = (ClampMin = "0.10"))
	float CrawlerChaseDuration = 12.0f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|ScareTuning|CrawlerChase", meta = (ClampMin = "0.0"))
	float CrawlerChasePanicAmount = 18.0f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|ScareTuning|CrawlerChase")
	bool bCrawlerChaseTriggersGroupFlicker = true;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|ScareTuning|LightChase", meta = (ClampMin = "0.10"))
	float LightChaseDuration = 1.1f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|ScareTuning|LightChase", meta = (ClampMin = "0.0"))
	float LightChasePanicAmount = 7.0f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|ScareTuning|LightChase", meta = (ClampMin = "2"))
	int32 LightChaseStepCount = 5;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|ScareTuning|LightChase", meta = (ClampMin = "0.01"))
	float LightChaseStepInterval = 0.16f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|ScareTuning|LightChase", meta = (ClampMin = "0.0"))
	float LightChaseStartDistance = 1800.f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|ScareTuning|LightChase", meta = (ClampMin = "0.0"))
	float LightChaseEndDistance = 120.f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|ScareTuning|LightChase", meta = (ClampMin = "0.0"))
	float LightChaseFlickerRadius = 350.f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|ScareTuning|LightChase", meta = (ClampMin = "0.0"))
	float LightChaseAudioLeadDistance = 80.f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|ScareTuning|RearAudioSting", meta = (ClampMin = "0.01"))
	float RearAudioStingDuration = 0.20f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|ScareTuning|RearAudioSting", meta = (ClampMin = "0.0"))
	float RearAudioStingPanicAmount = 4.0f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|ScareTuning|RearAudioSting")
	bool bRearAudioAllowTwoShot = true;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|ScareTuning|RearAudioSting", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float RearAudioTwoShotChance = 0.35f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|ScareTuning|RearAudioSting", meta = (ClampMin = "0.0"))
	float RearAudioBackOffset = 180.f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|ScareTuning|RearAudioSting", meta = (ClampMin = "0.0"))
	float RearAudioSideOffset = 110.f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|ScareTuning|RearAudioSting")
	float RearAudioUpOffset = -10.f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|ScareTuning|GhostScream", meta = (ClampMin = "0.01"))
	float GhostScreamDuration = 0.25f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|ScareTuning|GhostScream", meta = (ClampMin = "0.0"))
	float GhostScreamPanicAmount = 9.0f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|ScareTuning|GhostScream", meta = (ClampMin = "0.0"))
	float GhostScreamAudibleRadius = 1200.f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|ScareTuning|GhostScream", meta = (ClampMin = "0.0"))
	float GhostScreamSpawnDistanceMin = 180.f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|ScareTuning|GhostScream", meta = (ClampMin = "0.0"))
	float GhostScreamSpawnDistanceMax = 320.f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|ScareTuning|GhostScream", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float GhostScreamHighestPanicBiasChance = 0.75f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|ScareTuning|DoorSlamBehind", meta = (ClampMin = "0.01"))
	float DoorSlamBehindDuration = 0.25f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|ScareTuning|DoorSlamBehind", meta = (ClampMin = "0.0"))
	float DoorSlamBehindPanicAmount = 8.0f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|ScareTuning|DoorSlamBehind", meta = (ClampMin = "0.0"))
	float DoorSlamSearchRadius = 900.f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|ScareTuning|DoorSlamBehind", meta = (ClampMin = "0.0"))
	float DoorSlamMaxFrontDot = 0.25f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|ScareTuning|DoorSlamBehind", meta = (ClampMin = "0.0"))
	float DoorSlamDistanceWeight = 0.35f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|ScareTuning|DoorSlamBehind", meta = (ClampMin = "0.0"))
	float DoorSlamBehindWeight = 1.0f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|ScareTuning|DoorSlamBehind", meta = (ClampMin = "0.0"))
	float DoorSlamPanicRadius = 450.f;

	FTimerHandle TimerHandle_DirectorTick;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|Targeting")
	float PanicTargetWeight = 0.45f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|Targeting")
	float IsolationTargetWeight = 0.30f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|Targeting")
	float NearbyNoiseTargetWeight = 0.15f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|Targeting")
	float HauntPressurePenaltyWeight = 0.35f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|Tension", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float HouseTension01 = 0.0f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|Tension", meta = (ClampMin = "0.0"))
	float HouseTensionDecayPerSecond = 0.04f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|Tension", meta = (ClampMin = "0.0"))
	float AvgPanicToTensionWeight = 0.65f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|Tension", meta = (ClampMin = "0.0"))
	float AvgPressureToTensionWeight = 0.35f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|Tension", meta = (ClampMin = "0.0"))
	float GenericDispatchTensionImpulse = 0.08f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|Tension", meta = (ClampMin = "0.0"))
	float MirrorDispatchTensionImpulse = 0.10f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|Tension", meta = (ClampMin = "0.0"))
	float CrawlerOverheadDispatchTensionImpulse = 0.14f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|Tension", meta = (ClampMin = "0.0"))
	float CrawlerChaseDispatchTensionImpulse = 0.20f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|Tension", meta = (ClampMin = "0.1"))
	float GlobalHauntCooldownMin = 4.0f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|Tension", meta = (ClampMin = "0.1"))
	float GlobalHauntCooldownMax = 12.0f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|Tension", meta = (ClampMin = "0.0"))
	float LightChaseDispatchTensionImpulse = 0.10f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|Tension")
	bool bLogHouseTension = true;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|Tension", meta = (ClampMin = "0.0"))
	float RearAudioDispatchTensionImpulse = 0.06f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|Tension", meta = (ClampMin = "0.0"))
	float GhostScreamDispatchTensionImpulse = 0.12f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|Tension", meta = (ClampMin = "0.0"))
	float DoorSlamDispatchTensionImpulse = 0.10f;


private:
	UPROPERTY(EditAnywhere, Category = "GvT|Director|Targeting")
	float BaseTargetScore = 0.25f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|Targeting")
	float RecentTargetPenaltyWeight = 0.55f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|Targeting")
	float RecentTargetMemorySeconds = 20.0f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|Targeting")
	float TopScoreWindow = 0.20f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|Targeting")
	float BusyTargetPenalty = 1.0f;

	mutable TMap<TWeakObjectPtr<APawn>, float> LastTargetedTimeSeconds;

	float ComputeIsolationScore(const APawn* Pawn) const;
	float ComputeRecentTargetPenalty01(const APawn* Pawn) const;
	void RememberTarget(APawn* Pawn);

	bool TriggerRequestedFlicker(const FGvTScareEvent& Event, class UGvTScareComponent* TargetScareComp);
	bool TryDispatchAutoScare();
	AActor* ChooseBestTarget() const;
	float GetHauntPressureForPawn(const APawn* Pawn) const;
	float ScoreTarget(APawn* Pawn) const;

	void UpdateHouseTension(float DeltaSeconds);
	float ComputeAveragePlayerPanic01() const;
	float ComputeAveragePlayerPressure01() const;
	float GetDispatchTensionImpulse(const FGvTScareEvent& Event) const;
	void ApplyHouseTensionImpulse(float Delta01);
	AActor* ChooseHighestPanicTarget() const;
	AGvTDoorActor* ChooseBestDoorSlamTarget(APawn* TargetPawn) const;
	float ScoreDoorForSlam(const APawn* TargetPawn, const AGvTDoorActor* Door) const;
};