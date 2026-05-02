#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "Net/UnrealNetwork.h"
#include "Gameplay/Scare/GvTScareTypes.h"
#include "Systems/Light/GvTLightFlickerTypes.h"
#include "UGvTScareComponent.generated.h"

class USoundBase;
class UGvTScareSubsystem;
class UGvTGhostProfileAsset;
class AGvTThiefCharacter;

UENUM(BlueprintType)
enum class EGvTPanicBand : uint8
{
	Calm,
	Unsettled,
	Shaken,
	Terrified,
	Critical
};

UENUM(BlueprintType)
enum class EGvTScareLifecycleState : uint8
{
	Idle,
	Active,
	Recovering
};

UCLASS(ClassGroup = (GvT), BlueprintType, Blueprintable, meta = (BlueprintSpawnableComponent))
class GHOSTSVSTHIEVES_API UGvTScareComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGvTScareComponent();

	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION(BlueprintCallable, Category = "GvT|TEMP_DEPRECATED|GhostPresentation")
	void RequestCrawlerChaseScare(AActor* Victim);

	UFUNCTION(BlueprintCallable, Category = "GvT|TEMP_DEPRECATED|GhostPresentation")
	void RequestCrawlerOverheadScare(AActor* Victim, bool bVictimOnly);

	UFUNCTION(BlueprintCallable, Category = "GvT|Scare|Director")
	void RequestCrawlerChaseFromEvent(AActor* Victim);

	UFUNCTION(BlueprintCallable, Category = "GvT|Scare|Director")
	void RequestCrawlerOverheadFromEvent(const FGvTScareEvent& Event);

	UFUNCTION(BlueprintCallable, Category = "GvT|Scare|Director")
	void RequestLightChaseFromEvent(const FGvTScareEvent& Event);

	UFUNCTION(BlueprintCallable, Category = "GvT|Scare|Test")
	void Debug_RequestCrawlerChase(APawn* Victim);

	UFUNCTION(BlueprintCallable, Category = "GvT|Scare|Test")
	void Debug_RequestGroupHouseLightFlicker(float Intensity01 = 1.f, float Duration = 2.f);

	UFUNCTION(BlueprintCallable, Category = "GvT|Scare|Test")
	void Debug_RequestLocalHouseLightFlicker(float Intensity01 = 1.f, float Duration = 2.f);

	UFUNCTION(BlueprintCallable, Category = "GvT|Scare|Test")
	void Debug_RequestLightChase(float Intensity01 = 1.0f);

	UFUNCTION(BlueprintCallable, Category = "Panic")
	void AddPanic(float Amount);

	UFUNCTION(BlueprintCallable, Category = "Panic")
	void ReducePanic(float Amount);

	UFUNCTION(BlueprintPure, Category = "Panic")
	float GetPanicNormalized() const;

	UFUNCTION(BlueprintPure, Category = "Panic")
	EGvTPanicBand GetPanicBand() const;

	UFUNCTION(BlueprintCallable, Category = "GvT|Scare|Crawler")
	void PlayLocalCrawlerOverheadScare(const FGvTScareEvent& Event);

	void Server_ApplyDeathRipple(const FVector& DeathLocation, float Radius, float BaseIntensity01);

	UFUNCTION(BlueprintPure, Category = "GvT|Scare|Lifecycle")
	bool IsScareActive() const;

	UFUNCTION(BlueprintPure, Category = "GvT|Scare|Lifecycle")
	bool IsScareRecovering() const;

	UFUNCTION(BlueprintPure, Category = "GvT|Scare|Lifecycle")
	bool IsScareBusy() const;

	UFUNCTION(BlueprintCallable, Category = "GvT|Scare|Director")
	void RequestRearAudioStingFromEvent(const FGvTScareEvent& Event);

	UFUNCTION(BlueprintCallable, Category = "GvT|Scare|Director")
	void RequestGhostScreamFromEvent(const FGvTScareEvent& Event);

protected:
	UFUNCTION(Server, Reliable)
	void Server_RequestCrawlerChaseScare(AActor* Victim);

	UFUNCTION(Server, Reliable)
	void Server_RequestCrawlerOverheadScare(AActor* Victim, bool bVictimOnly);

	UFUNCTION(Server, Reliable)
	void Server_ReportLightChaseResult(bool bSucceeded, float PanicAmount);

	UFUNCTION(BlueprintImplementableEvent, Category = "GvT|Scare")
	void BP_PlayScare(const FGvTScareEvent& Event);

	UFUNCTION(Client, Reliable)
	void Client_PlayScare(const FGvTScareEvent& Event);

	UFUNCTION(Client, Reliable)
	void Client_PlayLightFlicker(const FGvTLightFlickerEvent& Event);

	UFUNCTION(Client, Reliable)
	void Client_PlayMirrorScare(float Intensity01, float LifeSeconds);

	UFUNCTION(Client, Reliable)
	void Client_StartCrawlerOverheadScare(const FGvTScareEvent& Event);

	UFUNCTION()
	void OnRep_ScareState();

	UFUNCTION()
	void OnRep_Panic();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Scare|LightChase")
	float LightChaseRecoveryDuration = 0.9f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Scare|RearAudio")
	float RearAudioRecoveryDuration = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Scare|GhostScream")
	float GhostScreamRecoveryDuration = 0.55f;

	UFUNCTION(BlueprintPure, Category = "Panic")
	bool IsCriticalPanic() const { return Panic >= CriticalThreshold; }

	UPROPERTY(ReplicatedUsing = OnRep_Panic, EditAnywhere, BlueprintReadOnly, Category = "Panic")
	float Panic = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Panic")
	float PanicMax = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Panic")
	float CriticalThreshold = 90.f;

	UPROPERTY(BlueprintReadOnly, Category = "Panic")
	EGvTPanicBand CachedPanicBand = EGvTPanicBand::Calm;

	UPROPERTY(Transient)
	TWeakObjectPtr<class AGvTMirrorActor> LastTriggeredMirror;

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

	UPROPERTY(EditDefaultsOnly, Category = "GvT|Scare|Schedule")
	bool bEnableLegacyAutoScheduler = false;

private:
	void PlayLocalLightFlicker(const FGvTLightFlickerEvent& Event) const;
	FGvTLightFlickerEvent MakeLightFlickerEvent(float Intensity01, float Duration, bool bWholeHouse) const;

	void UpdatePanicBand();
	bool IsServer() const;
	float GetNowServerSeconds() const;

	UFUNCTION(BlueprintCallable, Category = "GvT|Scare")
	FGvTScareState GetScareState() const { return ScareState; }

	uint8 GetPanicTier(float& OutPanic01) const;
	float ComputePressure01(float PlayerPanic01, float AvgPanic01, float TimeSinceLastScare01) const;
	void BuildContextTags(FGameplayTagContainer& OutContext) const;
	float ComputeCooldownSeconds(float FearScore01) const;
	int32 MakeLocalSeed(float Now) const;
	FRandomStream MakeStream(float Now) const;
	const UGvTGhostProfileAsset* ResolveGhostProfile(const UGvTScareSubsystem* Subsystem) const;
	float ComputeAveragePanic01() const;
	float ComputeTimeSinceLastScare01(float Now) const;
	AGvTThiefCharacter* ChooseTargetThief(FRandomStream& Stream) const;

	void Server_SchedulerTick();
	bool Server_CanTriggerNow(float Now) const;
	void Server_TriggerScare(float Now);

	FTimerHandle SchedulerTimer;

	TMap<FGameplayTag, float> LastTagTimeSeconds;
	bool bPendingSafetySpike = false;

	void BeginLocalScareLifecycle(float ActiveDuration, float RecoveryDuration);
	void EnterRecoveryState();
	void EndScareLifecycle();

	void HandleLifecycleActiveFinished();
	void HandleLifecycleRecoveryFinished();

	bool CanStartNewScare() const;
	void ClearLifecycleTimers();

	void PlayLocalLightChase(const FGvTScareEvent& Event);
	void BeginLocalLightChaseSequence(const FGvTScareEvent& Event);
	void AdvanceLocalLightChaseSequence();
	void EndLocalLightChaseSequence();

	TArray<FVector> BuildLightChaseStepLocations(const FGvTScareEvent& Event) const;
	void PlayLightChaseStepEffects(const FVector& StepLocation, bool bIsFinalStep);

	FVector ResolveLightChaseStepLocation(const FVector& RawStepLocation) const;
	void CollapseLightChaseStepLocations(TArray<FVector>& StepLocations) const;

	UPROPERTY(VisibleInstanceOnly, Category = "GvT|Scare|Lifecycle")
	EGvTScareLifecycleState LifecycleState = EGvTScareLifecycleState::Idle;

	UPROPERTY(EditDefaultsOnly, Category = "GvT|Scare|Lifecycle")
	float DefaultRecoveryDuration = 1.0f;

	UPROPERTY(EditDefaultsOnly, Category = "GvT|Scare|Lifecycle")
	float MirrorRecoveryDuration = 0.75f;

	UPROPERTY(EditDefaultsOnly, Category = "GvT|Scare|Lifecycle")
	float ChaseRecoveryDuration = 2.0f;

	UPROPERTY(VisibleInstanceOnly, Category = "GvT|Scare|Lifecycle")
	float LocalScareActiveEndTime = 0.f;

	UPROPERTY(VisibleInstanceOnly, Category = "GvT|Scare|Lifecycle")
	float LocalScareRecoveryEndTime = 0.f;

	UPROPERTY(Transient)
	FGvTScareEvent ActiveLightChaseEvent;

	UPROPERTY(Transient)
	TArray<FVector> ActiveLightChaseStepLocations;

	UPROPERTY(Transient)
	int32 ActiveLightChaseStepIndex = 0;

	UPROPERTY(Transient)
	bool bLightChaseSequenceActive = false;

	UPROPERTY(Transient)
	bool bActiveLightChaseAffectedAnyLights = false;

	FTimerHandle TimerHandle_ScareActiveFinish;
	FTimerHandle TimerHandle_ScareRecoveryFinish;
	FTimerHandle TimerHandle_LightChaseStep;
};
