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

private:
	bool TriggerRequestedFlicker(const FGvTScareEvent& Event, class UGvTScareComponent* TargetScareComp);
	bool TryDispatchAutoScare();
	AActor* ChooseBestTarget() const;
};