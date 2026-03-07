#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Net/UnrealNetwork.h"
#include "Systems/EGvTCrawlerGhostState.h"
#include "GvTCrawlerGhostCharacter.generated.h"

UCLASS(BlueprintType)
class GHOSTSVSTHIEVES_API AGvTCrawlerGhostCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AGvTCrawlerGhostCharacter();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintCallable, Server, Reliable, Category="GvT|Crawler|Chase")
	void Server_StartChase(APawn* Victim);

	/** Server-only: spawn over victim's head (camera-relative), upside down, then rotate/pitch toward victim while playing overhead anim. */
	UFUNCTION(BlueprintCallable, Server, Reliable, Category="GvT|Crawler|Overhead")
	void Server_StartOverheadScare(APawn* Victim, bool bVictimOnly = true);

	UFUNCTION(BlueprintCallable, Category="GvT|Crawler|Chase")
	void StopAndDie();

	UFUNCTION(BlueprintCallable, Category="GvT|Crawler|Chase")
	APawn* GetTargetVictim() const { return TargetVictim; }

	UFUNCTION(BlueprintCallable, Server, Reliable)
	void Server_DragStep();

	UFUNCTION(BlueprintPure, Category = "Crawler")
	EGvTCrawlerGhostState GetState() const { return State; }

	UFUNCTION(BlueprintPure, Category = "Crawler|Anim")
	float GetReplicatedSpeed() const { return ReplicatedSpeed; }

	void OnCrawlerDragStep();

protected:
	UPROPERTY(Replicated, BlueprintReadOnly, Category="GvT|Crawler|Chase")
	TObjectPtr<APawn> TargetVictim;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GvT|Crawler|Chase", meta=(ClampMin="0.05", ClampMax="2.0"))
	float RepathInterval = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GvT|Crawler|Chase", meta=(ClampMin="0.0", ClampMax="5000.0"))
	float AcceptRadius = 60.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GvT|Crawler|Chase", meta=(ClampMin="0.0", ClampMax="5000.0"))
	float KillDistance = 140.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GvT|Crawler|Chase", meta=(ClampMin="0.0", ClampMax="100000.0"))
	float MaxChaseDistance = 6000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GvT|Crawler|Movement", meta=(ClampMin="0.0", ClampMax="2000.0"))
	float MaxSpeed = 350.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GvT|Crawler|Movement", meta=(ClampMin="0.0", ClampMax="5000.0"))
	float Acceleration = 2048.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GvT|Crawler|Movement", meta=(ClampMin="0.0", ClampMax="5000.0"))
	float BrakingDecel = 2048.f;

	UPROPERTY(EditDefaultsOnly, Category = "Chase|AnimDrive")
	bool bUseDragStepMovement = true;

	// ----------------------------
	// Overhead scare tuning
	// ----------------------------
	/** Upward trace distance from victim camera to find ceiling. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GvT|Crawler|Overhead", meta=(ClampMin="0.0"))
	float OverheadTraceUpDistance = 1000.f;

	/** Clearance below the hit ceiling point. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GvT|Crawler|Overhead", meta=(ClampMin="0.0"))
	float OverheadCeilingClearance = 100.f;

	/** Extra forward offset from the camera's forward (feels "in front"). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GvT|Crawler|Overhead")
	float OverheadForwardOffset = 80.f;

	/** Additional framing offsets (camera-relative). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GvT|Crawler|Overhead")
	float OverheadFaceForward = 50.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GvT|Crawler|Overhead")
	float OverheadFaceDown = 25.f;

	/** Start/end pitch (deg) used during overhead scare. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GvT|Crawler|Overhead")
	float OverheadPitchStartDeg = -45.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GvT|Crawler|Overhead")
	float OverheadPitchEndDeg = 8.f;

	/** How long the overhead scare lasts (seconds). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GvT|Crawler|Overhead", meta=(ClampMin="0.01"))
	float OverheadDuration = 0.8f;

	/** If true, automatically transition into chase after overhead ends. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GvT|Crawler|Overhead")
	bool bOverheadAutoStartChase = false;

	/** If true, destroy the ghost after overhead ends (if not auto-chasing). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GvT|Crawler|Overhead")
	bool bOverheadAutoVanish = true;

	/** Degrees/sec yaw turn speed while facing the victim during overhead. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GvT|Crawler|Overhead", meta=(ClampMin="0.0"))
	float OverheadTurnSpeedDegPerSec = 6000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crawler|DragStep")
	float DragStepDistance = 55.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crawler|DragStep")
	float DragStepDuration = 0.18f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crawler|DragStep")
	float DragStepForwardBias = 1.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Chase|Grounding")
	bool bSnapToGroundInChase = true;

	UPROPERTY(EditDefaultsOnly, Category = "Chase|Grounding", meta = (ClampMin = "0.0"))
	float GroundTraceUp = 50.f;

	UPROPERTY(EditDefaultsOnly, Category = "Chase|Grounding", meta = (ClampMin = "0.0"))
	float GroundTraceDown = 300.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crawler|DragStep")
	bool bDragStepUseVelocityDir = true;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Crawler")
	EGvTCrawlerGhostState State = EGvTCrawlerGhostState::IdleCeiling;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Crawler|Anim")
	float ReplicatedSpeed = 0.f;

	UFUNCTION()
	void OnRep_State();

	void SetState(EGvTCrawlerGhostState NewState);

	void SnapToGround();

	void OverheadTick(float DeltaSeconds);
	bool GetVictimCamera(FVector& OutCamLoc, FRotator& OutCamRot) const;
	void StartOverhead_Internal(APawn* Victim);
	void EndOverhead();

private:
	float RepathTimer = 0.f;
	bool bOverheadActive = false;
	float OverheadStartTime = 0.f;

	void ChaseTick(float DeltaSeconds);
	void TryKillVictim();
	void ApplyDragStep();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
