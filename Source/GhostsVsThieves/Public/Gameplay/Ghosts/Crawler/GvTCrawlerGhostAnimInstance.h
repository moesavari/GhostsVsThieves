#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Systems/EGvTCrawlerGhostState.h"
#include "GvTCrawlerGhostAnimInstance.generated.h"

UCLASS(BlueprintType, Blueprintable)
class GHOSTSVSTHIEVES_API UGvTCrawlerGhostAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, Category = "Crawler|Anim")
	EGvTCrawlerGhostState GhostState = EGvTCrawlerGhostState::IdleCeiling;

	UPROPERTY(BlueprintReadOnly, Category = "Crawler|Anim")
	float Speed = 0.f;

protected:
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;
};