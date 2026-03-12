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

protected:
	UPROPERTY(EditAnywhere, Category = "GvT|Director|Runtime")
	bool bEnableAutoHaunts = true;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|Runtime", meta = (ClampMin = "0.10"))
	float DirectorTickInterval = 4.0f;

	UPROPERTY(EditAnywhere, Category = "GvT|Director|Runtime", meta = (ClampMin = "0.10"))
	float GlobalHauntCooldown = 10.0f;

	UPROPERTY(BlueprintReadOnly, Category = "GvT|Director|Runtime")
	float LastGlobalHauntTime = -1000.0f;

	FTimerHandle TimerHandle_DirectorTick;

private:
	bool TriggerRequestedFlicker(const FGvTScareEvent& Event, class UGvTScareComponent* TargetScareComp);
	bool TryDispatchAutoScare();
	AActor* ChooseBestTarget() const;
};