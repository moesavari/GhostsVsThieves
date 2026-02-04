#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "GvTPlayerState.generated.h"

UCLASS()
class GHOSTSVSTHIEVES_API AGvTPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "GvT|Loot")
	void AddLoot(int32 Amount);

	UFUNCTION(BlueprintPure, Category = "GvT|Loot")
	int32 GetLoot() const { return LootValue; }

protected:
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "GvT|Loot")
	int32 LootValue = 0;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
