#pragma once

#include "CoreMinimal.h"
#include "Delegates/DelegateCombinations.h"
#include "GameFramework/PlayerState.h"
#include "GvTPlayerState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLootValueChanged, int32, NewLootValue);

UCLASS()
class GHOSTSVSTHIEVES_API AGvTPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "GvT|Loot")
	void AddLoot(int32 Amount);

	UFUNCTION(BlueprintPure, Category = "GvT|Loot")
	int32 GetLoot() const { return LootValue; }

	UPROPERTY(BlueprintAssignable, Category = "GvT|Loot")
	FOnLootValueChanged OnLootValueChanged;

	UFUNCTION(BlueprintPure, Category = "GvT|Panic")
	float GetPanic01() const { return Panic01; }

	UFUNCTION(Server, Reliable)
	void Server_AddPanic(float Delta01);

protected:
	UPROPERTY(ReplicatedUsing = OnRep_LootValue, BlueprintReadOnly, Category = "GvT|Loot")
	int32 LootValue = 0;

	UFUNCTION()
	void OnRep_LootValue();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(ReplicatedUsing = OnRep_Panic, BlueprintReadOnly, Category = "GvT|Panic")
	float Panic01 = 0.f;

	UFUNCTION()
	void OnRep_Panic();
};
