#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Systems/EGvTGhoulGhostState.h"
#include "GvTGhoulGhostAnimInstance.generated.h"

UCLASS()
class GHOSTSVSTHIEVES_API UGvTGhoulGhostAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

	UPROPERTY(BlueprintReadOnly, Category = "Ghost")
	EGvTGhoulGhostState GhostState = EGvTGhoulGhostState::Idle;

	UPROPERTY(BlueprintReadOnly, Category = "Ghost")
	float Speed = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Ghost")
	float RunPlayRate = 1.f;

	UPROPERTY(BlueprintReadOnly, Category = "Ghost")
	bool bIsMoving = false;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Ghost")
	float WalkThreshold = 3.f;

	UPROPERTY(EditDefaultsOnly, Category = "Ghost")
	float RunThreshold = 180.f;

	UPROPERTY(EditDefaultsOnly, Category = "Ghost")
	float RunSpeedForPlayRate = 250.f;

	UPROPERTY(EditDefaultsOnly, Category = "Ghost")
	float MinRunPlayRate = 0.75f;

	UPROPERTY(EditDefaultsOnly, Category = "Ghost")
	float MaxRunPlayRate = 1.5f;
};