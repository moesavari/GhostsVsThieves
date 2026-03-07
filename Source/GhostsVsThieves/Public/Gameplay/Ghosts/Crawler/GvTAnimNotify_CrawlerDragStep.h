#pragma once
#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "GvTAnimNotify_CrawlerDragStep.generated.h"

UCLASS()
class GHOSTSVSTHIEVES_API UGvTAnimNotify_CrawlerDragStep : public UAnimNotify
{
	GENERATED_BODY()
public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation) override;
};