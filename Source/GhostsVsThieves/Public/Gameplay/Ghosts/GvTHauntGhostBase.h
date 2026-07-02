#pragma once

#include "CoreMinimal.h"
#include "TimerManager.h"
#include "Gameplay/Ghosts/GvTGhostCharacterBase.h"
#include "GvTHauntGhostBase.generated.h"

class ACharacter;
class APawn;
class UCharacterMovementComponent;
class UGvTGhostPerceptionComponent;

UENUM(BlueprintType)
enum class EGvTHauntGhostState : uint8
{
	Idle,
	SpawnIntro,
	Roaming,
	Investigating,
	Chasing,
	Searching,
	PerformingScare,
	Recovering,
	Despawning
};

UCLASS(Abstract, Blueprintable)
class GHOSTSVSTHIEVES_API AGvTHauntGhostBase : public AGvTGhostCharacterBase
{
	GENERATED_BODY()

public:
	AGvTHauntGhostBase();

	UFUNCTION(BlueprintCallable, Category = "GvT|Ghost|Haunt")
	virtual void StartGhostChase(AActor* Target);

	UFUNCTION(BlueprintCallable, Category = "GvT|Ghost|Haunt")
	virtual void StopGhostChase();

	virtual void BeginGhostHaunt(AActor* Target, FGameplayTag HauntTag) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintCallable, Category = "GvT|Ghost|Haunt")
	virtual void StartHauntDespawnSequence();

	UFUNCTION(BlueprintPure, Category = "GvT|Ghost|Haunt")
	EGvTHauntGhostState GetHauntState() const { return HauntState; }

	UFUNCTION(BlueprintPure, Category = "GvT|Ghost|Targeting")
	AActor* GetAssignedChaseTarget() const { return AssignedChaseTarget; }

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(ReplicatedUsing=OnRep_HauntState, VisibleAnywhere, BlueprintReadOnly, Category = "GvT|Ghost|Haunt")
	EGvTHauntGhostState HauntState = EGvTHauntGhostState::Idle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Ghost|Movement")
	float RoamSpeed = 220.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Ghost|Movement")
	float ChaseSpeed = 450.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Ghost|Movement")
	float MovementAcceptanceRadius = 80.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Ghost|Movement")
	bool bUseDirectChaseFallback = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Ghost|Movement", meta = (ClampMin = "0.0"))
	float DirectChaseFallbackSpeed = 420.f;

	void ApplyDirectChaseFallback(AActor* Target, float DeltaSeconds);
	virtual void HandleCaughtTarget(AActor* Target);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Ghost|Lifecycle", meta = (ClampMin = "0.0"))
	float PreHauntDelaySeconds = 0.65f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Ghost|Lifecycle", meta = (ClampMin = "0.0"))
	float PreDespawnDelaySeconds = 0.85f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Ghost|Targeting")
	float CatchDistance = 120.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Ghost|Targeting")
	float LoseSightGraceSeconds = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Ghost|Targeting")
	bool bRequireLineOfSightToCatch = true;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "GvT|Ghost|Targeting")
	TObjectPtr<AActor> CurrentTarget = nullptr;

	UPROPERTY(Replicated, Transient, BlueprintReadOnly, Category = "GvT|Ghost|Targeting")
	TObjectPtr<AActor> AssignedChaseTarget = nullptr;

	// Shared hunt tuning. Model-specific ghosts can read these instead of duplicating behavior.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Ghost|Search", meta = (ClampMin = "0.0"))
	float SearchRetargetDelaySeconds = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Ghost|Search", meta = (ClampMin = "0.0"))
	float SearchAfterLostSightSeconds = 6.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Ghost|Search", meta = (ClampMin = "0.0"))
	float SearchRepathInterval = 1.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Ghost|Search", meta = (ClampMin = "0.0"))
	float SearchPointRadius = 450.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Ghost|Haunt", meta = (ClampMin = "0.0"))
	float MaxHauntDurationSeconds = 30.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Ghost|Roaming", meta = (ClampMin = "0.0"))
	float RoamRepathInterval = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Ghost|Roaming", meta = (ClampMin = "0.0"))
	float RoamPointRadius = 900.f;

	float HauntElapsedSeconds = 0.f;
	float RoamRepathTimer = 0.f;

	virtual void UpdateRoaming(float DeltaSeconds);
	virtual void MoveToRandomRoamLocation();
	TObjectPtr<UGvTGhostPerceptionComponent> GhostPerceptionComponent;

	// MVP/Normal: one kill ends the hunt. Later hard mode can enable this to keep hunting.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Ghost|Difficulty")
	bool bContinueHuntAfterKill = false;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "GvT|Ghost|Targeting")
	FVector LastKnownTargetLocation = FVector::ZeroVector;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "GvT|Ghost|Targeting")
	float LastTimeTargetSeen = -999.f;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "GvT|Ghost|Lifecycle")
	TObjectPtr<AActor> PendingHauntTarget = nullptr;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "GvT|Ghost|Lifecycle")
	FGameplayTag PendingHauntTag;

	FTimerHandle TimerHandle_BeginHauntAfterSpawn;
	FTimerHandle TimerHandle_FinishDespawn;

	float SearchElapsedSeconds = 0.f;
	float SearchRepathTimer = 0.f;

	UFUNCTION(BlueprintImplementableEvent, Category = "GvT|Ghost|Lifecycle")
	void BP_OnGhostSpawnIntroStarted(FGameplayTag HauntTag);

	UFUNCTION(BlueprintImplementableEvent, Category = "GvT|Ghost|Lifecycle")
	void BP_OnGhostHauntStarted(FGameplayTag HauntTag);

	UFUNCTION(BlueprintImplementableEvent, Category = "GvT|Ghost|Lifecycle")
	void BP_OnGhostDespawnStarted();

	virtual void BeginHauntAction(AActor* Target, FGameplayTag HauntTag);
	void HandleBeginHauntAfterSpawnDelay();
	void FinishDespawnSequence();

	UPROPERTY(Transient)
	FVector DefaultMeshRelativeLocation = FVector::ZeroVector;

	UPROPERTY(Transient)
	FRotator DefaultMeshRelativeRotation = FRotator::ZeroRotator;

	UPROPERTY(Transient)
	bool bCachedDefaultMeshTransform = false;

	void CacheDefaultMeshTransform();
	void ResetMeshVisualTransform();
	void ConfigureClientGhostProxyMovement();

	virtual bool TryInvestigateRecentNoise();
	virtual void StartSearchFromLastKnownLocation();
	virtual void UpdateSearch(float DeltaSeconds);
	virtual void MoveToSearchLocation(const FVector& SearchLocation);
	virtual bool TryResumeChaseFromSearch();
	virtual void HandleSearchExpired();

	// Presentation hooks. Base owns behavior/state; derived ghosts only dress it up.
	virtual void OnHauntChaseStarted(AActor* Target);
	virtual void OnHauntChaseEnded();
	virtual void OnHauntSearchStarted();
	virtual void OnHauntTargetCaught(AActor* Target);

	UFUNCTION()
	void OnRep_HauntState(EGvTHauntGhostState OldState);

	virtual bool IsValidHauntVictim(const APawn* CandidatePawn) const;
	virtual bool HasLineOfSightToPawn(const APawn* CandidatePawn) const;
	virtual APawn* FindBestVisibleVictim() const;
	virtual void SetAssignedChaseTarget(AActor* Target);
	virtual void SetCurrentChaseTarget(AActor* Target);
	virtual FVector GetNavigationSafeGhostLocation(const FVector& DesiredLocation) const;
	virtual void SnapGhostToNavigationSafeHeight();
	virtual bool HasLineOfSightToTarget(AActor* Target) const;
	virtual void UpdateChase(float DeltaSeconds);
	virtual void TryCatchTarget();
	virtual bool CanTransitionTo(EGvTHauntGhostState NewState) const;
	virtual void ExitHauntState(EGvTHauntGhostState OldState, EGvTHauntGhostState NewState);
	virtual void EnterHauntState(EGvTHauntGhostState NewState, EGvTHauntGhostState OldState);
	virtual bool SetHauntState(EGvTHauntGhostState NewState);
	static const TCHAR* GetHauntStateName(EGvTHauntGhostState State);

	void EnterSpawnIntroHold();
	void ExitSpawnIntroHold();

	UPROPERTY(Transient)
	bool bSpawnIntroMovementHeld = false;

	UPROPERTY(Transient)
	TEnumAsByte<EMovementMode> SavedSpawnIntroMovementMode = MOVE_Walking;

	UPROPERTY(Transient)
	float SavedSpawnIntroGravityScale = 1.f;

	UPROPERTY(Transient)
	FVector SpawnIntroHoldLocation = FVector::ZeroVector;
};