#pragma once

#include "CoreMinimal.h"
#include "TimerManager.h"
#include "Gameplay/Ghosts/GvTGhostCharacterBase.h"
#include "GvTHauntGhostBase.generated.h"

class ACharacter;
class APawn;
class UCharacterMovementComponent;

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

	UFUNCTION(BlueprintImplementableEvent, Category = "GvT|Ghost|Lifecycle")
	void BP_OnGhostSpawnIntroStarted(FGameplayTag HauntTag);

	UFUNCTION(BlueprintImplementableEvent, Category = "GvT|Ghost|Lifecycle")
	void BP_OnGhostHauntStarted(FGameplayTag HauntTag);

	UFUNCTION(BlueprintImplementableEvent, Category = "GvT|Ghost|Lifecycle")
	void BP_OnGhostDespawnStarted();

	virtual void BeginHauntAction(AActor* Target, FGameplayTag HauntTag);
	void HandleBeginHauntAfterSpawnDelay();
	void FinishDespawnSequence();


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