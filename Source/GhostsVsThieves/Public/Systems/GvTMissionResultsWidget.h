#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GvTGameStateBase.h"
#include "GvTMissionResultsWidget.generated.h"

UCLASS(Abstract, Blueprintable)
class GHOSTSVSTHIEVES_API UGvTMissionResultsWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintImplementableEvent, Category="GvT|Mission Results")
	void SetMissionResults(const FGvTMissionResults& Results);
};
