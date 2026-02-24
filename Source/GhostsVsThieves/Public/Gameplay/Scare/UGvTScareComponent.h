#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "Gameplay/Scare/GvTScareTypes.h"
#include "Gameplay/Characters/Thieves/GvTThiefCharacter.h"
#include "UGvTScareComponent.generated.h"

class UGvTScareSubsystem;
class UGvTGhostProfileAsset;
class AGvTCrawlerGhost;

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

	UFUNCTION(BlueprintCallable, Category = "GvT|Scare|Crawler")
	void StartCrawlerHaunt(AActor* Victim);

	UPROPERTY(EditAnywhere, Category = "GvT|Scare|Crawler")
	float CrawlerSpawnForward = 800.f;

	UPROPERTY(EditAnywhere, Category = "GvT|Scare|Crawler")
	float CrawlerSpawnUp = 180.f;

	UPROPERTY(EditAnywhere, Category = "GvT|Scare|Crawler")
	TSubclassOf<AGvTCrawlerGhost> CrawlerGhostClass;

protected:
	UFUNCTION(Server, Reliable)
	void Server_StartCrawlerHaunt(AActor* Victim);

	UPROPERTY(Transient)
	TObjectPtr<AGvTCrawlerGhost> ActiveCrawlerGhost;

	UFUNCTION(BlueprintImplementableEvent, Category = "GvT|Scare")
	void BP_PlayScare(const FGvTScareEvent& Event);

	UPROPERTY(EditAnywhere, Category = "GvT|Reflect")
	bool bEnableReflectScare = true;

	UPROPERTY(EditAnywhere, Category = "GvT|Reflect")
	float ReflectTraceDistance = 1500.f;

	UPROPERTY(EditAnywhere, Category = "GvT|Reflect")
	float ReflectSphereRadius = 35.f;

	UPROPERTY(EditAnywhere, Category = "GvT|Reflect")
	float ReflectDotMin = 0.86f;

	UPROPERTY(EditAnywhere, Category = "GvT|Reflect")
	float ReflectCheckInterval = 0.05f;

	UPROPERTY(EditAnywhere, Category = "GvT|Reflect")
	float ReflectLifeSeconds = 0.6f;

	UFUNCTION(Client, Reliable)
	void Client_PlayScare(const FGvTScareEvent& Event);

	UFUNCTION()
	void OnRep_ScareState();

	UPROPERTY(Transient)
	TWeakObjectPtr<class AGvTMirrorActor> LastTriggeredMirror;

	UPROPERTY(Transient)
	float NextAllowedReflectTime = 0.f;

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
	FTimerHandle ReflectCheckTimer;

	TMap<FGameplayTag, float> LastTagTimeSeconds;
	bool bPendingSafetySpike = false;

	bool IsServer() const;
	float GetNowServerSeconds() const;

	uint8 GetPanicTier(float& OutPanic01) const;

	float ComputePressure01(float PlayerPanic01, float AvgPanic01, float TimeSinceLastScare01) const;

	void BuildContextTags(FGameplayTagContainer& OutContext) const;

	void Server_SchedulerTick();
	void Client_ReflectTick();
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
