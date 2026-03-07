#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Systems/EGvTCrawlerGhostState.h"
#include "AGvTCrawlerGhost.generated.h"

class UCapsuleComponent;
class USkeletalMeshComponent;


USTRUCT(BlueprintType)
struct FGvTCrawlerAnimTuning
{
    GENERATED_BODY()

    /** Speed (UU/s) that looks correct when PlayRate == 1.0. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Crawler|Anim")
    float RefCrawlSpeed = 350.f;

    /** Clamp range for derived/override PlayRate. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Crawler|Anim")
    float MinPlayRate = 0.2f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Crawler|Anim")
    float MaxPlayRate = 3.0f;

    /** Extra multiplier applied after derivation. (1.0 = no change) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Crawler|Anim")
    float PlayRateScalar = 1.0f;

    /** Optional smoothing for Speed->PlayRate to avoid jitter. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Crawler|Anim")
    bool bSmoothSpeed = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Crawler|Anim", meta=(ClampMin="0.0"))
    float SpeedInterp = 8.f;
};

UCLASS(BlueprintType)
class GHOSTSVSTHIEVES_API AGvTCrawlerGhost : public AActor
{
    GENERATED_BODY()

public:
    AGvTCrawlerGhost();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

    UFUNCTION(BlueprintCallable, Server, Reliable)
    void Server_StartDropScare(AActor* Victim, const FVector& CeilingWorldPos, const FRotator& FaceRot, bool bVictimOnly);

	UFUNCTION(BlueprintCallable, Server, Reliable)
	void Server_StartOverheadScare(AActor* Victim, bool bVictimOnly);

    UFUNCTION(BlueprintCallable, Server, Reliable)
    void Server_StartHauntChase(AActor* Victim);

    UFUNCTION(BlueprintCallable, Server, Reliable)
    void Server_Vanish();

    UFUNCTION(BlueprintCallable, Category = "Crawler|Haunt")
    void StartDropScare(AActor* Victim, const FVector& CeilingWorldPos, const FRotator& FaceRot, bool bVictimOnly);

	UFUNCTION(BlueprintCallable, Category = "Crawler|Haunt")
	void StartOverheadScare(AActor* Victim, bool bVictimOnly);

    /** Called by an AnimNotify at the "drag" moment to move the ghost forward in chase. (Server-authoritative) */
    UFUNCTION(BlueprintCallable, Category="Crawler|Chase")
    void OnCrawlerDragStep(); 
    UFUNCTION(BlueprintCallable, Category = "Crawler|Haunt")
    void StartHauntChase(AActor* Victim);

    UFUNCTION(BlueprintCallable, Category = "Crawler|Haunt")
    void Vanish();

    UFUNCTION(BlueprintPure, Category = "Crawler")
    EGvTCrawlerGhostState GetState() const { return State; }

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Crawler|Anim")
    float ReplicatedSpeed = 0.f;

    /** If > 0, forces crawl animation play rate (useful for AnimNotify-driven movement). 0 = auto from Speed/RefCrawlSpeed. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Crawler|Anim")
    float ChasePlayRateOverride = 0.f;

    /** Optional per-mode animation tuning (editable in BP_GvTCrawlerGhost). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Crawler|Anim|Modes")
    FGvTCrawlerAnimTuning PatrolAnimTuning;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Crawler|Anim|Modes")
    FGvTCrawlerAnimTuning ChaseAnimTuning;

    /** If true, AnimInstance uses the per-mode tunings above instead of its own defaults. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Crawler|Anim|Modes")
    bool bUseModeAnimTuning = true;

    /** A simple "mode" the AnimInstance can use (future: path patrol, searching, etc). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Crawler|Anim|Modes")
    bool bIsSearchingOrPatrolling = false;

    FORCEINLINE const FGvTCrawlerAnimTuning& GetPatrolAnimTuning() const { return PatrolAnimTuning; }
    FORCEINLINE const FGvTCrawlerAnimTuning& GetChaseAnimTuning() const { return ChaseAnimTuning; }


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

    UPROPERTY(EditDefaultsOnly, Category = "Chase")
    float LungeInterval = 0.65f;

    UPROPERTY(EditDefaultsOnly, Category = "Chase")
    float LungeSpeed = 1400.f;

    UPROPERTY(EditDefaultsOnly, Category = "Chase")
    float TurnSpeedDegPerSec = 540.f;

    UPROPERTY(EditDefaultsOnly, Category = "Chase")
    float KillDistance = 140.f;

    UPROPERTY(EditDefaultsOnly, Category = "Chase")
    float MaxChaseDistance = 5000.f;

    float LungeTimer = 0.f;
    bool bIsLunging = false;

    UPROPERTY(EditDefaultsOnly, Category = "DropScare")
	float DropDuration = 0.35f;

	UPROPERTY(EditDefaultsOnly, Category = "DropScare")
	float DropOffsetDown = 220.f;

	// If true, DropScare uses the AnimBP's non-looping sequence in the DropScare pose (no montage).
	UPROPERTY(EditDefaultsOnly, Category = "DropScare")
	bool bDropUseAnimSequence = true;

	// How long to stay in DropScare after the drop interpolation finishes (lets the one-shot anim play).
	UPROPERTY(EditDefaultsOnly, Category = "DropScare", meta=(ClampMin="0.0"))
	float DropScareAnimDuration = 0.85f;

	// Rotate toward the victim camera while scaring.
	UPROPERTY(EditDefaultsOnly, Category = "DropScare")
	bool bDropFaceVictimCamera = true;

	// If true, sets Roll=180 while in overhead scare.
	UPROPERTY(EditDefaultsOnly, Category = "DropScare")
	bool bDropUpsideDown = true;

	// Turn speed during DropScare (deg/sec).
	UPROPERTY(EditDefaultsOnly, Category = "DropScare")
	float DropTurnSpeedDegPerSec = 2400.f;

	// Optional: after scare finishes, immediately chase the victim.
	UPROPERTY(EditDefaultsOnly, Category = "DropScare")
	bool bChaseAfterDropScare = false;

	// --- Overhead scare placement ---
	UPROPERTY(EditDefaultsOnly, Category = "Overhead")
	float OverheadTraceUpDistance = 900.f;

	UPROPERTY(EditDefaultsOnly, Category = "Overhead")
	float OverheadCeilingClearance = 50.f;

	UPROPERTY(EditDefaultsOnly, Category = "Overhead")
	float OverheadForwardOffset = 80.f;

	// If true, overhead scare does not drop down (spawns at ceiling pose position).
	UPROPERTY(EditDefaultsOnly, Category = "Overhead")
	bool bOverheadNoDrop = true;


    // --- Overhead scare look/pitch snap ---
    UPROPERTY(EditDefaultsOnly, Category="Overhead|Pitch")
    float OverheadPitchStartDeg = -45.f;

    UPROPERTY(EditDefaultsOnly, Category="Overhead|Pitch")
    float OverheadPitchEndDeg = 8.f;

    UPROPERTY(EditDefaultsOnly, Category="Overhead|Pitch", meta=(ClampMin="0.01"))
    float OverheadPitchSnapTime = 0.12f;

    UPROPERTY(EditDefaultsOnly, Category="Overhead|Placement")
    float OverheadFaceForward = 40.f;

    UPROPERTY(EditDefaultsOnly, Category="Overhead|Placement")
    float OverheadFaceDown = 35.f;

    // --- Chase movement via animation notifies ---
    UPROPERTY(EditDefaultsOnly, Category="Chase|AnimDrive")
    bool bUseDragStepMovement = false;

    UPROPERTY(EditDefaultsOnly, Category="Chase|AnimDrive", meta=(ClampMin="0.0"))
    float DragStepDistance = 45.f;

    UPROPERTY(EditDefaultsOnly, Category="Chase|Grounding")
    bool bSnapToGroundInChase = true;

    UPROPERTY(EditDefaultsOnly, Category="Chase|Grounding", meta=(ClampMin="0.0"))
    float GroundTraceUp = 50.f;

    UPROPERTY(EditDefaultsOnly, Category="Chase|Grounding", meta=(ClampMin="0.0"))
    float GroundTraceDown = 300.f;

    UFUNCTION()
    void SnapToGround();

    FVector GetVictimCameraLocation() const;

    float DropT = 0.f;
	float DropHoldT = 0.f;
	float DropDurationRuntime = 0.f;

	FVector DropStart = FVector::ZeroVector;
	FVector DropEnd = FVector::ZeroVector;
private:
    bool bLastDropWasOverhead = false;
    bool bOverheadPitchSnapping = false;
    float OverheadPitchStartTime = 0.f;

    FVector PrevLoc = FVector::ZeroVector;
};