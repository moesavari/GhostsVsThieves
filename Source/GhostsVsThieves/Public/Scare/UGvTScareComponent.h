#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "Scare/GvTScareTypes.h"
#include "Characters/Thieves/GvTThiefCharacter.h"
#include "UGvTScareComponent.generated.h"

class UGvTScareSubsystem;
class UGvTGhostProfileAsset;

UCLASS(ClassGroup = (GvT), BlueprintType, Blueprintable, meta = (BlueprintSpawnableComponent))
class GHOSTSVSTHIEVES_API UGvTScareComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGvTScareComponent();

	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	void Server_ApplyDeathRipple(const FVector& DeathLocation, float Radius, float BaseIntensity01);

	UFUNCTION(BlueprintCallable, Category = "GvT|Scare")
	FGvTScareState GetScareState() const { return ScareState; }

protected:
	UFUNCTION(BlueprintImplementableEvent, Category = "GvT|Scare")
	void BP_PlayScare(const FGvTScareEvent& Event);

	UFUNCTION(Client, Reliable)
	void Client_PlayScare(const FGvTScareEvent& Event);

	UFUNCTION()
	void OnRep_ScareState();

private:
	UPROPERTY(ReplicatedUsing = OnRep_ScareState)
	FGvTScareState ScareState;

	UPROPERTY(EditDefaultsOnly, Category = "GvT|Scare|Schedule")
	float SchedulerIntervalSeconds = 0.50f;

	UPROPERTY(EditDefaultsOnly, Category = "GvT|Scare|Schedule")
	int32 CooldownStackCap = 5;

	UPROPERTY(EditDefaultsOnly, Category = "GvT|Scare|Schedule")
	float BaseCooldownMin = 6.f;

	UPROPERTY(EditDefaultsOnly, Category = "GvT|Scare|Schedule")
	float BaseCooldownMax = 18.f;

	UPROPERTY(EditDefaultsOnly, Category = "GvT|Scare|Schedule")
	float CooldownStackPenaltySeconds = 2.5f;

	UPROPERTY(EditDefaultsOnly, Category = "GvT|Scare|Schedule")
	float SafetySpikeBonus = 0.15f;

	UPROPERTY(EditDefaultsOnly, Category = "GvT|Scare|Schedule")
	FGameplayTag DefaultGhostTag;

	FTimerHandle SchedulerTimer;

	TMap<FGameplayTag, float> LastTagTimeSeconds;
	bool bPendingSafetySpike = false;

	bool IsServer() const;
	float GetNowServerSeconds() const;

	uint8 GetPanicTier(float& OutPanic01) const;

	float ComputePressure01(float PlayerPanic01, float AvgPanic01, float TimeSinceLastScare01) const;

	void BuildContextTags(FGameplayTagContainer& OutContext) const;

	void Server_SchedulerTick();
	bool Server_CanTriggerNow(float Now) const;
	void Server_TriggerScare(float Now);

	float ComputeCooldownSeconds(float FearScore01) const;
	int32 MakeLocalSeed(float Now) const;
	FRandomStream MakeStream(float Now) const;

	const UGvTGhostProfileAsset* ResolveGhostProfile(const UGvTScareSubsystem* Subsystem) const;
	
	float ComputeAveragePanic01() const;
	float ComputeTimeSinceLastScare01(float Now) const;

	AGvTThiefCharacter* ChooseTargetThief(FRandomStream& Stream) const;
};
