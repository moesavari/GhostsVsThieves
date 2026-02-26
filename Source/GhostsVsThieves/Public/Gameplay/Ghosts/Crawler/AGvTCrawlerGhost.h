#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Systems/EGvTCrawlerGhostState.h"
#include "AGvTCrawlerGhost.generated.h"

class UCapsuleComponent;
class USkeletalMeshComponent;

UCLASS(BlueprintType)
class GHOSTSVSTHIEVES_API AGvTCrawlerGhost : public AActor
{
	GENERATED_BODY()

public:
	AGvTCrawlerGhost();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	// --- Network entrypoints (server authoritative) ---
	UFUNCTION(BlueprintCallable, Server, Reliable)
	void Server_StartDropScare(AActor* Victim, const FVector& CeilingWorldPos, const FRotator& FaceRot, bool bVictimOnly);

	UFUNCTION(BlueprintCallable, Server, Reliable)
	void Server_StartHauntChase(AActor* Victim);

	UFUNCTION(BlueprintCallable, Server, Reliable)
	void Server_Vanish();

	// --- Convenience wrappers (callable from BP or code) ---
	UFUNCTION(BlueprintCallable, Category = "Crawler|Haunt")
	void StartDropScare(AActor* Victim, const FVector& CeilingWorldPos, const FRotator& FaceRot, bool bVictimOnly);

	UFUNCTION(BlueprintCallable, Category = "Crawler|Haunt")
	void StartHauntChase(AActor* Victim);

	UFUNCTION(BlueprintCallable, Category = "Crawler|Haunt")
	void Vanish();

	UFUNCTION(BlueprintPure, Category = "Crawler")
	EGvTCrawlerGhostState GetState() const { return State; }

	/** Server-computed speed (UU/s) replicated to clients for animation. */
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Crawler|Anim")
	float ReplicatedSpeed = 0.f;

	UFUNCTION(BlueprintPure, Category = "Crawler|Anim")
	float GetReplicatedSpeed() const { return ReplicatedSpeed; }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UCapsuleComponent> Capsule = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USkeletalMeshComponent> Mesh = nullptr;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(ReplicatedUsing = OnRep_State, BlueprintReadOnly, Category = "Crawler")
	EGvTCrawlerGhostState State = EGvTCrawlerGhostState::IdleCeiling;

	UFUNCTION()
	void OnRep_State(EGvTCrawlerGhostState PrevState);

	void EnterState(EGvTCrawlerGhostState NewState);

	void TryKillVictim();

	UPROPERTY(Replicated)
	TObjectPtr<AActor> TargetVictim = nullptr;

	void StartDropScare_Internal(AActor* Victim, const FVector& CeilingWorldPos, const FRotator& FaceRot, bool bVictimOnly);
	void StartHauntChase_Internal(AActor* Victim);
	void Vanish_Internal();

	// =========================
	// Chase Tuning (Editable)
	// =========================

	/** If true, uses the old "lunge" burst movement. If false, uses smooth continuous crawl. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crawler|Chase|Tuning")
	bool bUseLungeMovement = false;

	/** Continuous crawl speed (UU/s) when bUseLungeMovement == false. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crawler|Chase|Tuning", meta=(ClampMin="0.0", UIMin="0.0", UIMax="2000.0"))
	float CrawlSpeed = 350.f;

	/** How fast the ghost turns to face the victim. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crawler|Chase|Tuning", meta=(ClampMin="0.0", UIMin="0.0", UIMax="2000.0"))
	float TurnSpeedDegPerSec = 540.f;

	/** Distance within which the victim is killed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crawler|Chase|Tuning", meta=(ClampMin="0.0", UIMin="0.0", UIMax="1000.0"))
	float KillDistance = 140.f;

	/** If victim gets farther than this, ghost gives up and vanishes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crawler|Chase|Tuning", meta=(ClampMin="0.0", UIMin="0.0", UIMax="20000.0"))
	float MaxChaseDistance = 5000.f;

	// ---- Legacy lunge parameters (only used if bUseLungeMovement == true) ----

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crawler|Chase|Lunge", meta=(ClampMin="0.01", UIMin="0.01", UIMax="5.0"))
	float LungeInterval = 0.65f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crawler|Chase|Lunge", meta=(ClampMin="0.0", UIMin="0.0", UIMax="5000.0"))
	float LungeSpeed = 1400.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crawler|Chase|Lunge", meta=(ClampMin="0.01", UIMin="0.01", UIMax="1.0"))
	float LungeDuration = 0.12f;

	float LungeTimer = 0.f;
	bool bIsLunging = false;

	// =========================
	// Drop Scare Tuning
	// =========================

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crawler|DropScare|Tuning", meta=(ClampMin="0.01", UIMin="0.01", UIMax="5.0"))
	float DropDuration = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crawler|DropScare|Tuning", meta=(ClampMin="0.0", UIMin="0.0", UIMax="2000.0"))
	float DropOffsetDown = 220.f;

	float DropT = 0.f;
	FVector DropStart = FVector::ZeroVector;
	FVector DropEnd = FVector::ZeroVector;

private:
	/** Used to compute ReplicatedSpeed after movement. */
	FVector PrevLoc = FVector::ZeroVector;
};
