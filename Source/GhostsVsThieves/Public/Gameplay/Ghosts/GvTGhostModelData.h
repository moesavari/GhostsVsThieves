#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GvTGhostModelData.generated.h"

class AGvTGhostCharacterBase;

UCLASS(BlueprintType)
class GHOSTSVSTHIEVES_API UGvTGhostModelData : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GvT|Ghost|Model")
	FName ModelId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GvT|Ghost|Model")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GvT|Ghost|Model")
	TSubclassOf<AGvTGhostCharacterBase> GhostClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GvT|Ghost|Model")
	bool bCanHaunt = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GvT|Ghost|Model")
	bool bCanEvent = true;
};
