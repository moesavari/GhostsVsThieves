#pragma once

#include "CoreMinimal.h"
#include "Gameplay/Ghosts/GvTGhostCharacterBase.h"
#include "GvTHauntGhostBase.generated.h"

class ACharacter;

UENUM(BlueprintType)
enum class EGvTHauntGhostState : uint8
{
	Idle,
	Roaming,
	Investigating,
	Chasing,
	Searching,
	PerformingScare,
	Recovering
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

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GvT|Ghost|Haunt")
	EGvTHauntGhostState HauntState = EGvTHauntGhostState::Idle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Ghost|Movement")
	float RoamSpeed = 220.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Ghost|Movement")
	float ChaseSpeed = 450.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Ghost|Targeting")
	float CatchDistance = 120.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Ghost|Targeting")
	float LoseSightGraceSeconds = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Ghost|Targeting")
	bool bRequireLineOfSightToCatch = true;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "GvT|Ghost|Targeting")
	TObjectPtr<AActor> CurrentTarget = nullptr;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "GvT|Ghost|Targeting")
	FVector LastKnownTargetLocation = FVector::ZeroVector;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "GvT|Ghost|Targeting")
	float LastTimeTargetSeen = -999.f;

	virtual bool HasLineOfSightToTarget(AActor* Target) const;
	virtual void UpdateChase(float DeltaSeconds);
	virtual void TryCatchTarget();
	virtual void SetHauntState(EGvTHauntGhostState NewState);
};