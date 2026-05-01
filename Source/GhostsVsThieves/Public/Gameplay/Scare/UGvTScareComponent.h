#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "Net/UnrealNetwork.h"
#include "Gameplay/Scare/GvTScareTypes.h"
#include "Systems/Light/GvTLightFlickerTypes.h"
#include "UGvTScareComponent.generated.h"

class USoundBase;
class AGvTMirrorActor;
class AActor;
class AGvTGhostCharacterBase;

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
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/* -------------------------------------------------------------------------
	 * Public scare request API
	 * These stay alive as compatibility wrappers, but the component is now
	 * strictly a player-local scare receiver. World ghost ownership stays in
	 * Director / active round ghost flow.
	 * ------------------------------------------------------------------------- */

	UFUNCTION(BlueprintCallable, Category = "GvT|Scare|Core")
	void RequestMirrorScare(float Intensity01 = 1.f, float LifeSeconds = 1.5f);

	UFUNCTION(BlueprintCallable, Category = "GvT|Scare|Core")
	void RequestLightChaseFromEvent(const FGvTScareEvent& Event);

	UFUNCTION(BlueprintCallable, Category = "GvT|Scare|Core")
	void RequestGhostChaseScare(AActor* Victim);

	UFUNCTION(BlueprintCallable, Category = "GvT|Scare|Core")
	void RequestGhostChaseFromEvent(AActor* Victim);

	UFUNCTION(BlueprintCallable, Category = "GvT|Scare|Core")
	void RequestGhostScareFromEvent(const FGvTScareEvent& Event);

	UFUNCTION(BlueprintCallable, Category = "GvT|Scare|Local")
	void PlayLocalGhostScare(const FGvTScareEvent& Event);

	UFUNCTION(BlueprintCallable, Category = "GvT|Scare|Core")
	void RequestRearAudioStingFromEvent(const FGvTScareEvent& Event);

	UFUNCTION(BlueprintCallable, Category = "GvT|Scare|Core")
	void RequestGhostScreamFromEvent(const FGvTScareEvent& Event);

	UFUNCTION(BlueprintCallable, Category = "GvT|Scare|Core")
	void RequestMirrorScareFromEvent(const FGvTScareEvent& Event);

	UFUNCTION(Client, Reliable)
	void Client_StartMirrorScare(const FGvTScareEvent& Event);

	void PlayLocalMirrorScare(const FGvTScareEvent& Event);

	/* -------------------------------------------------------------------------
	 * Debug
	 * ------------------------------------------------------------------------- */

	UFUNCTION(BlueprintCallable, Category = "GvT|Scare|Debug")
	void Test_MirrorScare(float Intensity01 = 1.f, float LifeSeconds = 1.5f);

	UFUNCTION(BlueprintCallable, Category = "GvT|Scare|Debug")
	void Debug_RequestGroupHouseLightFlicker(float Intensity01 = 1.f, float Duration = 2.f);

	UFUNCTION(BlueprintCallable, Category = "GvT|Scare|Debug")
	void Debug_RequestLocalHouseLightFlicker(float Intensity01 = 1.f, float Duration = 2.f);

	UFUNCTION(BlueprintCallable, Category = "GvT|Scare|Debug")
	void Debug_RequestLightChase(float Intensity01 = 1.0f);

	/* -------------------------------------------------------------------------
	 * Panic
	 * ------------------------------------------------------------------------- */

	UFUNCTION(BlueprintCallable, Category = "GvT|Scare|Panic")
	void AddPanic(float Amount);

	UFUNCTION(BlueprintCallable, Category = "GvT|Scare|Panic")
	void ReducePanic(float Amount);

	UFUNCTION(BlueprintPure, Category = "GvT|Scare|Panic")
	float GetPanicNormalized() const;

	UFUNCTION(BlueprintPure, Category = "GvT|Scare|Panic")
	EGvTPanicBand GetPanicBand() const;

	UFUNCTION(BlueprintPure, Category = "GvT|Scare|Panic")
	bool IsCriticalPanic() const { return Panic >= CriticalThreshold; }

	/* -------------------------------------------------------------------------
	 * Lifecycle
	 * ------------------------------------------------------------------------- */

	UFUNCTION(BlueprintPure, Category = "GvT|Scare|Lifecycle")
	bool IsScareActive() const;

	UFUNCTION(BlueprintPure, Category = "GvT|Scare|Lifecycle")
	bool IsScareRecovering() const;

	UFUNCTION(BlueprintPure, Category = "GvT|Scare|Lifecycle")
	bool IsScareBusy() const;

	/* -------------------------------------------------------------------------
	 * Local playback entry points
	 * ------------------------------------------------------------------------- */

	UFUNCTION(BlueprintCallable, Category = "GvT|Scare|Local")
	bool TryBeginLocalHauntGhostScare(const FGvTScareEvent& Event);

	void Server_ApplyDeathRipple(const FVector& DeathLocation, float Radius, float BaseIntensity01);

	FGvTLightFlickerEvent MakeLightFlickerEvent(float Intensity01, float Duration, bool bWholeHouse) const;

	UFUNCTION(Client, Reliable)
	void Client_PlayLightFlicker(const FGvTLightFlickerEvent& Event);

	void EndScareLifecycle();

protected:
	/* -------------------------------------------------------------------------
	 * RPCs
	 * ------------------------------------------------------------------------- */

	UFUNCTION(Server, Reliable)
	void Server_RequestMirrorActorScare(AGvTMirrorActor* Mirror, float Intensity01, float LifeSeconds);

	UFUNCTION(Server, Reliable)
	void Server_ReportLightChaseResult(bool bSucceeded, float PanicAmount);

	UFUNCTION(Client, Reliable)
	void Client_PlayScare(const FGvTScareEvent& Event);

	UFUNCTION(Server, Reliable)
	void Server_RequestGroupHouseLightFlicker(float Intensity01, float Duration);

	UFUNCTION(Client, Reliable)
	void Client_PlayMirrorScare(float Intensity01, float LifeSeconds);

	UFUNCTION(Client, Reliable)
	void Client_StartLightChase(const FGvTScareEvent& Event);

	UFUNCTION(Client, Reliable)
	void Client_StartRearAudioSting(const FGvTScareEvent& Event);

	UFUNCTION(Client, Reliable)
	void Client_StartGhostScream(const FGvTScareEvent& Event);

	UFUNCTION(Client, Reliable)
	void Client_StartGhostScare(const FGvTScareEvent& Event);

	UFUNCTION(BlueprintImplementableEvent, Category = "GvT|Scare|Local")
	void BP_PlayScare(const FGvTScareEvent& Event);

	UFUNCTION()
	void OnRep_ScareState();

	UFUNCTION()
	void OnRep_Panic();

	/* -------------------------------------------------------------------------
	 * Local scare tuning
	 * ------------------------------------------------------------------------- */

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Scare|Audio")
	TObjectPtr<USoundBase> LightChaseStepSfx = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Scare|Audio")
	TObjectPtr<USoundBase> LightChaseFinalSfx = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Scare|Audio")
	TObjectPtr<USoundBase> RearAudioStingSfx = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Scare|Audio")
	TObjectPtr<USoundBase> RearAudioStingFollowupSfx = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Scare|Audio")
	TObjectPtr<USoundBase> GhostScreamSfx = nullptr;

	/* -------------------------------------------------------------------------
	 * Panic
	 * ------------------------------------------------------------------------- */

	UPROPERTY(ReplicatedUsing = OnRep_Panic, EditAnywhere, BlueprintReadOnly, Category = "GvT|Scare|Panic")
	float Panic = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GvT|Scare|Panic")
	float PanicMax = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GvT|Scare|Panic")
	float CriticalThreshold = 90.f;

	UPROPERTY(BlueprintReadOnly, Category = "GvT|Scare|Panic")
	EGvTPanicBand CachedPanicBand = EGvTPanicBand::Calm;

	/* -------------------------------------------------------------------------
	 * Lifecycle tuning
	 * ------------------------------------------------------------------------- */

	UPROPERTY(ReplicatedUsing = OnRep_ScareState)
	FGvTScareState ScareState;

	UPROPERTY(VisibleInstanceOnly, Category = "GvT|Scare|Lifecycle")
	EGvTScareLifecycleState LifecycleState = EGvTScareLifecycleState::Idle;

	UPROPERTY(EditDefaultsOnly, Category = "GvT|Scare|Lifecycle")
	float DefaultRecoveryDuration = 1.0f;

	UPROPERTY(EditDefaultsOnly, Category = "GvT|Scare|Lifecycle")
	float MirrorRecoveryDuration = 0.75f;

	UPROPERTY(EditDefaultsOnly, Category = "GvT|Scare|Lifecycle")
	float ChaseRecoveryDuration = 2.0f;

	UPROPERTY(EditDefaultsOnly, Category = "GvT|Scare|Lifecycle")
	float LightChaseRecoveryDuration = 0.9f;

	UPROPERTY(EditDefaultsOnly, Category = "GvT|Scare|Lifecycle")
	float RearAudioRecoveryDuration = 0.35f;

	UPROPERTY(EditDefaultsOnly, Category = "GvT|Scare|Lifecycle")
	float GhostScreamRecoveryDuration = 0.55f;

	UPROPERTY(VisibleInstanceOnly, Category = "GvT|Scare|Lifecycle")
	float LocalScareActiveEndTime = 0.f;

	UPROPERTY(VisibleInstanceOnly, Category = "GvT|Scare|Lifecycle")
	float LocalScareRecoveryEndTime = 0.f;

private:
	/* -------------------------------------------------------------------------
	 * Internal helpers
	 * ------------------------------------------------------------------------- */

	void UpdatePanicBand();
	bool IsServer() const;
	float GetNowServerSeconds() const;

	UFUNCTION(BlueprintCallable, Category = "GvT|Scare|Core")
	FGvTScareState GetScareState() const { return ScareState; }

	void BeginLocalScareLifecycle(float ActiveDuration, float RecoveryDuration);
	void HandleLifecycleActiveFinished();
	void EnterRecoveryState();
	void HandleLifecycleRecoveryFinished();
	void ClearLifecycleTimers();
	bool CanStartNewScare() const;

	void PlayLocalLightFlicker(const FGvTLightFlickerEvent& Event) const;

	void PlayLocalLightChase(const FGvTScareEvent& Event);
	void BeginLocalLightChaseSequence(const FGvTScareEvent& Event);
	void AdvanceLocalLightChaseSequence();
	void EndLocalLightChaseSequence();

	TArray<FVector> BuildLightChaseStepLocations(const FGvTScareEvent& Event) const;
	FVector ResolveLightChaseStepLocation(const FVector& RawStepLocation) const;
	void CollapseLightChaseStepLocations(TArray<FVector>& StepLocations) const;
	void PlayLightChaseStepEffects(const FVector& StepLocation, bool bIsFinalStep);
	void PlayLocalLightChaseSoundAtLocation(const FVector& WorldLocation, bool bIsFinalStep, float StepAlpha) const;
	void PlayLocalRearAudioSting(const FGvTScareEvent& Event);
	void PlayLocalGhostScreamShared(const FGvTScareEvent& Event);
	FVector BuildRearAudioWorldLocation(const FGvTScareEvent& Event) const;

	/* -------------------------------------------------------------------------
	 * Local transient state
	 * ------------------------------------------------------------------------- */

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

	float ActiveRecoveryDuration = 0.5f;

	FTimerHandle TimerHandle_ScareActiveFinish;
	FTimerHandle TimerHandle_ScareRecoveryFinish;
	FTimerHandle TimerHandle_LightChaseStep;
	FTimerHandle TimerHandle_RearAudioFollowup;
};