#pragma once

#include "CoreMinimal.h"
#include "Gameplay/Ghosts/GvTHauntGhostBase.h"
#include "Systems/Noise/GvTScareAudioTypes.h"
#include "GvTGhoulGhostCharacter.generated.h"

class UAudioComponent;
class USoundBase;

UCLASS(BlueprintType)
class GHOSTSVSTHIEVES_API AGvTGhoulGhostCharacter : public AGvTHauntGhostBase
{
	GENERATED_BODY()

public:
	AGvTGhoulGhostCharacter();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void BeginGhostScare(AActor* Target, FGameplayTag ScareTag) override;
	virtual void BeginGhostHaunt(AActor* Target, FGameplayTag HauntTag) override;

	UFUNCTION(BlueprintCallable, Server, Reliable, Category = "GvT|Ghoul|Chase")
	void Server_StartChase(APawn* Victim);

	UFUNCTION(BlueprintPure, Category = "GvT|Ghoul|Anim")
	float GetReplicatedSpeed() const { return ReplicatedSpeed; }

	UFUNCTION(BlueprintPure, Category = "GvT|Ghoul|Anim")
	bool IsChasing() const { return bIsChasing; }

	UFUNCTION(BlueprintPure, Category = "GvT|Ghoul|Anim")
	bool IsPerformingScare() const { return bIsPerformingScare; }

	UFUNCTION(BlueprintPure, Category = "GvT|Ghoul|Anim")
	EGvTHauntGhostState GetGhoulHauntState() const { return HauntState; }

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual void UpdateChase(float DeltaSeconds) override;
	void ChaseTick(float DeltaSeconds);
	void TryCatchVictim();
	void StopAndVanish();
	void BeginCloseScarePresentation(AActor* Target);
	void EndCloseScarePresentation();
	void FaceTargetFlat(const AActor* Target, float DeltaSeconds, float TurnSpeedDegPerSecond);
	void PlayScareAudioStart(const FGvTScareAudioSet& AudioSet, bool bAttachToActor);
	void StartScareAudioSustain(const FGvTScareAudioSet& AudioSet, bool bAttachToActor);
	void StopCurrentScareAudio(bool bPlayEnd, const FGvTScareAudioSet* AudioSet, bool bAttachToActor);

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "GvT|Ghoul|Target")
	TObjectPtr<APawn> TargetVictim;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "GvT|Ghoul|Anim")
	float ReplicatedSpeed = 0.f;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "GvT|Ghoul|Anim")
	bool bIsChasing = false;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "GvT|Ghoul|Anim")
	bool bIsPerformingScare = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Ghoul|Chase", meta = (ClampMin = "0.0"))
	float MaxSpeed = 420.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Ghoul|Chase", meta = (ClampMin = "0.0"))
	float Acceleration = 2048.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Ghoul|Chase", meta = (ClampMin = "0.0"))
	float BrakingDecel = 2048.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Ghoul|Chase", meta = (ClampMin = "0.0"))
	float RepathInterval = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Ghoul|Chase", meta = (ClampMin = "0.0"))
	float AcceptRadius = 80.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Ghoul|Chase", meta = (ClampMin = "0.0"))
	float GhoulCatchDistance = 135.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Ghoul|Chase", meta = (ClampMin = "0.0"))
	float MaxChaseDistance = 6000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Ghoul|Chase")
	bool bUseDirectChaseFallback = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Ghoul|Chase", meta = (ClampMin = "0.0"))
	float DirectChaseFallbackDelay = 0.85f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Ghoul|Chase", meta = (ClampMin = "0.0"))
	float DirectChaseMinSpeed = 12.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Ghoul|Chase", meta = (ClampMin = "0.0"))
	float DirectChaseFallbackForceDelay = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Ghoul|Catch", meta = (ClampMin = "0.0"))
	float MinimumChaseTimeBeforeCatch = 0.75f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Ghoul|Catch", meta = (ClampMin = "0.0"))
	float MinimumSpeedToCatch = 35.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Ghoul|Catch", meta = (ClampMin = "0.0"))
	float ChaseProgressDistanceThreshold = 8.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Ghoul|Catch", meta = (ClampMin = "0.0"))
	float MaxTimeSinceProgressForCatch = 1.50f;


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Ghoul|CloseScare", meta = (ClampMin = "0.0"))
	float CloseScareForwardOffset = 230.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Ghoul|CloseScare")
	float CloseScareCameraZOffset = -45.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Ghoul|CloseScare", meta = (ClampMin = "0.0"))
	float CloseScareDuration = 0.85f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Ghoul|CloseScare", meta = (ClampMin = "0.0"))
	float CloseScareStunDuration = 0.75f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GvT|Ghoul|Audio")
	FGvTScareAudioSet ChaseAudio;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GvT|Ghoul|Audio")
	FGvTScareAudioSet CloseScareAudio;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> ActiveSustainAudio = nullptr;

private:
	float RepathTimer = 0.f;
	float DirectChaseStuckTime = 0.f;
	float ChaseStartTimeSeconds = 0.f;
	float LastDistanceToVictim = 0.f;
	float LastChaseProgressTimeSeconds = 0.f;
	FTimerHandle TimerHandle_CloseScareEnd;
};
