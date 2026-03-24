#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Gameplay/Scare/GvTScareTypes.h"
#include "GvTDirectorSubsystem.generated.h"

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

	UFUNCTION(BlueprintPure, Category = "GvT|Director|Tension")
	float GetHouseTension01() const { return HouseTension01; }

	UFUNCTION(BlueprintPure, Category = "GvT|Director|Tension")
	float GetCurrentGlobalHauntCooldown() const;

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

	UPROPERTY(EditAnywhere, Category = "GvT|Director|Tension")
	bool bLogHouseTension = true;

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
};