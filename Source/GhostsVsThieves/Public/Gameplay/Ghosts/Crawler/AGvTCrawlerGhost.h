#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Systems/EGvTCrawlerGhostState.h"
#include "AGvTCrawlerGhost.generated.h"

class USkeletalMeshComponent;
class UCapsuleComponent;

UCLASS()
class GHOSTSVSTHIEVES_API AGvTCrawlerGhost : public AActor
{
    GENERATED_BODY()

public:
    AGvTCrawlerGhost();

    virtual void Tick(float DeltaSeconds) override;
    virtual void BeginPlay() override;

    UFUNCTION(BlueprintCallable, Server, Reliable)
    void Server_StartDropScare(AActor* Victim, const FVector& CeilingWorldPos, const FRotator& FaceRot, bool bVictimOnly);

    UFUNCTION(BlueprintCallable, Server, Reliable)
    void Server_StartHauntChase(AActor* Victim);

    UFUNCTION(BlueprintCallable, Server, Reliable)
    void Server_Vanish();

    UFUNCTION(BlueprintPure, Category = "Crawler")
    EGvTCrawlerGhostState GetState() const { return State; }

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    TObjectPtr<USkeletalMeshComponent> Mesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    TObjectPtr<UCapsuleComponent> Capsule;

    UPROPERTY(ReplicatedUsing = OnRep_State, BlueprintReadOnly)
    EGvTCrawlerGhostState State = EGvTCrawlerGhostState::IdleCeiling;

    UFUNCTION()
    void OnRep_State(EGvTCrawlerGhostState PrevState);

    void EnterState(EGvTCrawlerGhostState NewState);

    UPROPERTY(Replicated)
    TObjectPtr<AActor> TargetVictim;

    // Chase tuning
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

    // Drop scare
    UPROPERTY(EditDefaultsOnly, Category = "DropScare")
    float DropDuration = 0.35f;

    UPROPERTY(EditDefaultsOnly, Category = "DropScare")
    float DropOffsetDown = 220.f;

    float DropT = 0.f;
    FVector DropStart = FVector::ZeroVector;
    FVector DropEnd = FVector::ZeroVector;

    void TryKillVictim();

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};