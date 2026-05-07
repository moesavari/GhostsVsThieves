#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Gameplay/Ghosts/GvTHauntGhostBase.h"
#include "GvTGhoulGhostAnimInstance.generated.h"

UCLASS(BlueprintType, Blueprintable)
class GHOSTSVSTHIEVES_API UGvTGhoulGhostAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, Category = "Ghoul|Anim")
	float Speed = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Ghoul|Anim")
	bool bIsChasing = false;

	UPROPERTY(BlueprintReadOnly, Category = "Ghoul|Anim")
	bool bIsPerformingScare = false;

	UPROPERTY(BlueprintReadOnly, Category = "Ghoul|Anim")
	float MovePlayRate = 1.f;

	UPROPERTY(BlueprintReadOnly, Category = "Ghoul|Anim")
	EGvTHauntGhostState GhostState = EGvTHauntGhostState::Idle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghoul|Anim|Tuning", meta = (ClampMin = "1.0"))
	float RefMoveSpeed = 350.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghoul|Anim|Tuning", meta = (ClampMin = "0.0"))
	float MinPlayRate = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ghoul|Anim|Tuning", meta = (ClampMin = "0.0"))
	float MaxPlayRate = 2.5f;

protected:
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;
};
