#pragma once

#include "CoreMinimal.h"
#include "Delegates/DelegateCombinations.h"
#include "GameFramework/PlayerState.h"
#include "GvTPlayerState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLootValueChanged, int32, NewLootValue);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPanicChanged, float, NewPanic01);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnHauntPressureChanged, float, NewPressure01);

UCLASS()
class GHOSTSVSTHIEVES_API AGvTPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	AGvTPlayerState();

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// Loot
	UFUNCTION(BlueprintCallable, Category = "GvT|Loot")
	void AddLoot(int32 Amount);

	UFUNCTION(BlueprintPure, Category = "GvT|Loot")
	int32 GetLoot() const { return LootValue; }

	UPROPERTY(BlueprintAssignable, Category = "GvT|Loot")
	FOnLootValueChanged OnLootValueChanged;

	// Panic
	UFUNCTION(BlueprintPure, Category = "GvT|Panic")
	float GetPanic01() const { return Panic01; }

	UPROPERTY(BlueprintAssignable, Category = "GvT|Panic")
	FOnPanicChanged OnPanicChanged;

	UFUNCTION(Server, Reliable)
	void Server_AddPanic(float Delta01);

	UFUNCTION(Server, Reliable)
	void Server_ReducePanic(float Delta01);

	void AddPanicAuthority(float Delta01);
	void ReducePanicAuthority(float Delta01);

	// Haunt Pressure
	UFUNCTION(BlueprintPure, Category = "GvT|Haunt")
	float GetRecentHauntPressure01() const { return RecentHauntPressure01; }

	UPROPERTY(BlueprintAssignable, Category = "GvT|Haunt")
	FOnHauntPressureChanged OnHauntPressureChanged;

	UFUNCTION(Server, Reliable)
	void Server_AddHauntPressure(float Delta01);

	UFUNCTION(Server, Reliable)
	void Server_ReduceHauntPressure(float Delta01);

	void AddHauntPressureAuthority(float Delta01);
	void ReduceHauntPressureAuthority(float Delta01);

protected:
	UPROPERTY(ReplicatedUsing = OnRep_LootValue, BlueprintReadOnly, Category = "GvT|Loot")
	int32 LootValue = 0;

	UFUNCTION()
	void OnRep_LootValue();

	UPROPERTY(ReplicatedUsing = OnRep_Panic, BlueprintReadOnly, Category = "GvT|Panic")
	float Panic01 = 0.f;

	UFUNCTION()
	void OnRep_Panic();

	UPROPERTY(ReplicatedUsing = OnRep_HauntPressure, BlueprintReadOnly, Category = "GvT|Haunt")
	float RecentHauntPressure01 = 0.f;

	UFUNCTION()
	void OnRep_HauntPressure();

	// Panic decay
	UPROPERTY(EditDefaultsOnly, Category = "GvT|Panic|Decay", meta = (ClampMin = "0.0"))
	float PanicDecayPerSecond = 0.03f;

	UPROPERTY(EditDefaultsOnly, Category = "GvT|Panic|Decay", meta = (ClampMin = "0.0"))
	float PanicRecoveryDelayAfterSpike = 5.0f;

	UPROPERTY(BlueprintReadOnly, Category = "GvT|Panic")
	float LastPanicSpikeTime = -1000.f;

	// Pressure decay
	UPROPERTY(EditDefaultsOnly, Category = "GvT|Haunt|Decay", meta = (ClampMin = "0.0"))
	float HauntPressureDecayPerSecond = 0.06f;

private:
	void ApplyPanicDecay(float DeltaSeconds);
	void ApplyHauntPressureDecay(float DeltaSeconds);
};