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

    UFUNCTION(BlueprintCallable, Server, Reliable)
    void Server_StartDropScare(AActor* Victim, const FVector& CeilingWorldPos, const FRotator& FaceRot, bool bVictimOnly);

    UFUNCTION(BlueprintCallable, Server, Reliable)
    void Server_StartHauntChase(AActor* Victim);

    UFUNCTION(BlueprintCallable, Server, Reliable)
    void Server_Vanish();

    UFUNCTION(BlueprintCallable, Category = "Crawler|Haunt")
    void StartDropScare(AActor* Victim, const FVector& CeilingWorldPos, const FRotator& FaceRot, bool bVictimOnly);

    UFUNCTION(BlueprintCallable, Category = "Crawler|Haunt")
    void StartHauntChase(AActor* Victim);

    UFUNCTION(BlueprintCallable, Category = "Crawler|Haunt")
    void Vanish();

    UFUNCTION(BlueprintPure, Category = "Crawler")
    EGvTCrawlerGhostState GetState() const { return State; }

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

    float DropT = 0.f;
    FVector DropStart = FVector::ZeroVector;
    FVector DropEnd = FVector::ZeroVector;

private:
    FVector PrevLoc = FVector::ZeroVector;
};