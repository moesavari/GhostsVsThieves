#pragma once

#include "CoreMinimal.h"
#include "Gameplay/Ghosts/GvTHauntGhostBase.h"
#include "Systems/Noise/GvTScareAudioTypes.h"
#include "GvTGhoulGhostCharacter.generated.h"

class UAudioComponent;

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
	virtual void BeginHauntAction(AActor* Target, FGameplayTag HauntTag) override;

	UFUNCTION(BlueprintCallable, Server, Reliable, Category = "GvT|Ghoul|Chase")
	void Server_StartChase(APawn* Victim);

	UFUNCTION(BlueprintPure, Category = "GvT|Ghoul|Anim")
	float GetReplicatedSpeed() const { return ReplicatedSpeed; }

	UFUNCTION(BlueprintPure, Category = "GvT|Ghoul|Anim")
	bool IsChasing() const { return bIsChasing; }

	UFUNCTION(BlueprintPure, Category = "GvT|Ghoul|Anim")
	bool IsPerformingScare() const { return bIsPerformingScare; }

	UFUNCTION(BlueprintPure, Category = "GvT|Ghoul|Anim")
	bool IsSearching() const { return bIsSearching; }

	UFUNCTION(BlueprintPure, Category = "GvT|Ghoul|Anim")
	EGvTHauntGhostState GetGhoulHauntState() const { return HauntState; }

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual void OnHauntChaseStarted(AActor* Target) override;
	virtual void OnHauntChaseEnded() override;
	virtual void OnHauntSearchStarted() override;
	virtual void OnHauntTargetCaught(AActor* Target) override;

	void TryOpenNearbyDoors();
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

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "GvT|Ghoul|Anim")
	bool bIsSearching = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Ghoul|Chase", meta = (ClampMin = "0.0"))
	float MaxSpeed = 420.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Ghoul|Chase", meta = (ClampMin = "0.0"))
	float Acceleration = 2048.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Ghoul|Chase", meta = (ClampMin = "0.0"))
	float BrakingDecel = 2048.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Ghoul|Chase", meta = (ClampMin = "0.0"))
	float GhoulCatchDistance = 135.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Ghoul|Doors", meta = (ClampMin = "0.0"))
	float DoorProbeRadius = 165.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Ghoul|Doors", meta = (ClampMin = "0.0"))
	float DoorProbeForwardOffset = 95.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Ghoul|Doors", meta = (ClampMin = "0.0"))
	float DoorProbeInterval = 0.20f;

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
	float DoorProbeTimer = 0.f;
	FTimerHandle TimerHandle_CloseScareEnd;
};
