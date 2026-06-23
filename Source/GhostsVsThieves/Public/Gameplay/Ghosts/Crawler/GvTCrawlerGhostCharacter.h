#pragma once

#include "CoreMinimal.h"
#include "Gameplay/Ghosts/GvTHauntGhostBase.h"
#include "Net/UnrealNetwork.h"
#include "Systems/EGvTCrawlerGhostState.h"
#include "Systems/Noise/GvTScareAudioTypes.h"
#include "GvTCrawlerGhostCharacter.generated.h"

class UAudioComponent;

UCLASS(BlueprintType)
class GHOSTSVSTHIEVES_API AGvTCrawlerGhostCharacter : public AGvTHauntGhostBase
{
	GENERATED_BODY()

public:
	AGvTCrawlerGhostCharacter();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintCallable, Server, Reliable, Category = "GvT|Crawler|Chase")
	void Server_StartChase(APawn* Victim);

	UFUNCTION(BlueprintCallable, Server, Reliable, Category = "GvT|Crawler|Overhead")
	void Server_StartOverheadScare(APawn* Victim, bool bVictimOnly = true);

	UFUNCTION(BlueprintPure, Category = "Crawler")
	EGvTCrawlerGhostState GetState() const { return State; }

	UFUNCTION(BlueprintPure, Category = "Crawler|Anim")
	float GetReplicatedSpeed() const { return ReplicatedSpeed; }

	void StartLocalOverheadScare(APawn* Victim);
	void StopAndDie();

	virtual void BeginGhostScare(AActor* Target, FGameplayTag ScareTag) override;
	virtual void BeginGhostHaunt(AActor* Target, FGameplayTag HauntTag) override;

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual void OnHauntChaseStarted(AActor* Target) override;
	virtual void OnHauntChaseEnded() override;
	virtual void OnHauntSearchStarted() override;
	virtual void OnHauntTargetCaught(AActor* Target) override;

	void OnRep_State();
	void SetState(EGvTCrawlerGhostState NewState);
	void StartOverhead_Internal(APawn* Victim);
	void EndOverhead();
	void OverheadTick(float DeltaSeconds);
	bool GetVictimCamera(FVector& OutCamLoc, FRotator& OutCamRot) const;

	UFUNCTION(BlueprintCallable, Category="GvT|Crawler|Chase")
	APawn* GetTargetVictim() const { return TargetVictim; }

	UPROPERTY(Replicated, BlueprintReadOnly, Category="GvT|Crawler|Chase")
	TObjectPtr<APawn> TargetVictim;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GvT|Crawler|Chase", meta=(ClampMin="0.0", ClampMax="5000.0"))
	float KillDistance = 140.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GvT|Crawler|Movement", meta=(ClampMin="0.0", ClampMax="2000.0"))
	float MaxSpeed = 350.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GvT|Crawler|Movement", meta=(ClampMin="0.0", ClampMax="5000.0"))
	float Acceleration = 2048.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GvT|Crawler|Movement", meta=(ClampMin="0.0", ClampMax="5000.0"))
	float BrakingDecel = 2048.f;

	// ----------------------------
	// Overhead scare tuning
	// ----------------------------
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GvT|Crawler|Overhead", meta=(ClampMin="0.0"))
	float OverheadTraceUpDistance = 1000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GvT|Crawler|Overhead", meta=(ClampMin="0.0"))
	float OverheadCeilingClearance = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GvT|Crawler|Overhead")
	float OverheadForwardOffset = 80.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GvT|Crawler|Overhead")
	float OverheadFaceForward = 50.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GvT|Crawler|Overhead")
	float OverheadFaceDown = 25.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GvT|Crawler|Overhead")
	float OverheadPitchStartDeg = -45.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GvT|Crawler|Overhead")
	float OverheadPitchEndDeg = 8.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GvT|Crawler|Overhead", meta=(ClampMin="0.01"))
	float OverheadDuration = 0.8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GvT|Crawler|Overhead")
	bool bOverheadAutoStartChase = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GvT|Crawler|Overhead")
	bool bOverheadAutoVanish = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GvT|Crawler|Overhead", meta=(ClampMin="0.0"))
	float OverheadTurnSpeedDegPerSec = 6000.f;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Crawler")
	EGvTCrawlerGhostState State = EGvTCrawlerGhostState::IdleCeiling;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Crawler|Anim")
	float ReplicatedSpeed = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GvT|Crawler|Audio")
	FGvTScareAudioSet OverheadAudio;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GvT|Crawler|Audio")
	FGvTScareAudioSet ChaseAudio;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> ActiveSustainAudio = nullptr;

	void PlayScareAudioStart(const FGvTScareAudioSet& AudioSet, bool bAttachToActor);
	void StartScareAudioSustain(const FGvTScareAudioSet& AudioSet, bool bAttachToActor);
	void StopScareAudioSustain(const FGvTScareAudioSet& AudioSet);
	void PlayScareAudioEnd(const FGvTScareAudioSet& AudioSet, bool bAttachToActor);
	void StopCurrentScareAudio(bool bPlayEnd, const FGvTScareAudioSet* AudioSet, bool bAttachToActor);

private:
	bool bOverheadActive = false;
	float OverheadStartTime = 0.f;
	bool bLocalOverheadOnly = false;
	bool bDisableOverheadVictimTracking = false;
};
