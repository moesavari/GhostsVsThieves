#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Gameplay/Interaction/GvTInteractable.h"
#include "GameplayTagContainer.h"
#include "GvTPowerBoxActor.generated.h"

class UStaticMeshComponent;
class UGvTLightFlickerComponent;
class UPointLightComponent;
class UBoxComponent;
class USoundBase;

UENUM(BlueprintType)
enum class EGvTHousePowerState : uint8
{
	On		UMETA(DisplayName = "On"),
	Off		UMETA(DisplayName = "Off"),
	Blown	UMETA(DisplayName = "Blown")
};

UENUM(BlueprintType)
enum class EGvTPowerChangeCause : uint8
{
	PlayerInteraction,
	GhostEvent,
	SystemScript,
	Debug
};

UCLASS()
class GHOSTSVSTHIEVES_API AGvTPowerBoxActor : public AActor, public IGvTInteractable
{
	GENERATED_BODY()

public:
	AGvTPowerBoxActor();

	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual void GetInteractionSpec_Implementation(APawn* InstigatorPawn, EGvTInteractionVerb Verb, FGvTInteractionSpec& OutSpec) const override;
	virtual bool CanInteract_Implementation(APawn* InstigatorPawn, EGvTInteractionVerb Verb) const override;
	virtual void BeginInteract_Implementation(APawn* InstigatorPawn, EGvTInteractionVerb Verb) override;
	virtual void CompleteInteract_Implementation(APawn* InstigatorPawn, EGvTInteractionVerb Verb) override;
	virtual void CancelInteract_Implementation(APawn* InstigatorPawn, EGvTInteractionVerb Verb, EGvTInteractionCancelReason Reason) override;

	UFUNCTION(BlueprintCallable, Category = "GvT|Power")
	void TogglePower();

	UFUNCTION(Server, Reliable)
	void Server_TogglePower();

	UFUNCTION(BlueprintCallable, Category = "GvT|Power")
	void BlowPowerBox();

	UFUNCTION(BlueprintCallable, Category = "GvT|Power")
	void ForcePowerStateFromGhost(EGvTHousePowerState NewState);

	UFUNCTION(Server, Reliable)
	void Server_ForcePowerStateFromGhost(EGvTHousePowerState NewState);

	UFUNCTION(BlueprintCallable, Category = "GvT|Power")
	void HandlePlayerInteract(APawn* InstigatorPawn);

	UFUNCTION(BlueprintPure, Category = "GvT|Power")
	EGvTHousePowerState GetPowerState() const { return PowerState; }

	UFUNCTION(BlueprintPure, Category = "GvT|Power")
	bool IsPowerAvailableForScares() const { return PowerState == EGvTHousePowerState::On; }

	UFUNCTION(BlueprintPure, Category = "GvT|Power")
	bool IsPowerOn() const { return PowerState == EGvTHousePowerState::On; }

	UFUNCTION(BlueprintPure, Category = "GvT|Power")
	bool IsPowerBlown() const { return PowerState == EGvTHousePowerState::Blown; }

	UFUNCTION(BlueprintCallable, Category = "GvT|Power")
	void SetHouseActor(AActor* InHouseActor);

	UFUNCTION(BlueprintPure, Category = "GvT|Power")
	AActor* GetHouseActor() const { return HouseActor; }

	/** Called by the HouseManager on the server. Inactive decoys are hidden and non-interactable. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "GvT|Power")
	void SetActiveBreaker(bool bNewActive);

	UFUNCTION(BlueprintPure, Category = "GvT|Power")
	bool IsActiveBreaker() const { return bIsActiveBreaker; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Power|Ghost Reaction", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float BreakerGhostReactionChance = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GvT|Power|Ghost Reaction")
	bool bBreakerScareOnlyWhenTurningPowerOn = true;

protected:
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "GvT|Power")
	void Server_SetPowerState(EGvTHousePowerState NewState);

	UFUNCTION()
	void OnRep_PowerState();

	UFUNCTION()
	void OnRep_IsActiveBreaker();

	void ApplyPowerState();
	void ApplyActiveBreakerState();
	void ScheduleRandomFailureCheck();
	void RunRandomFailureCheck();
	float GetRandomFailureChance() const;

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlayPowerStateAudio(EGvTHousePowerState NewState);

	void InitializeIndicatorLights(TObjectPtr<UPointLightComponent> IndicatorLight, FColor Color);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GvT|Power")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GvT|Power")
	TObjectPtr<UStaticMeshComponent> PowerBoxMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GvT|Power")
	TObjectPtr<UBoxComponent> InteractionBounds;

	UPROPERTY(ReplicatedUsing = OnRep_PowerState, EditAnywhere, BlueprintReadOnly, Category = "GvT|Power")
	EGvTHousePowerState PowerState = EGvTHousePowerState::Off;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "GvT|Power")
	TObjectPtr<AActor> HouseActor = nullptr;

	UPROPERTY(ReplicatedUsing = OnRep_IsActiveBreaker, VisibleInstanceOnly, BlueprintReadOnly, Category = "GvT|Power")
	bool bIsActiveBreaker = true;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GvT|Power")
	TObjectPtr<UPointLightComponent> OnIndicatorLight;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GvT|Power")
	TObjectPtr<UPointLightComponent> OffIndicatorLight;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GvT|Power")
	TObjectPtr<UPointLightComponent> BlownIndicatorLight;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GvT|Power")
	FGameplayTag PowerInteractNoiseTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GvT|Power|Failure", meta = (ClampMin = "0.0"))
	float FailureProtectionSeconds = 25.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GvT|Power|Failure", meta = (ClampMin = "0.1"))
	float FailureCheckIntervalMin = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GvT|Power|Failure", meta = (ClampMin = "0.1"))
	float FailureCheckIntervalMax = 12.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GvT|Power|Failure", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float FailureChanceDormant = 0.01f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GvT|Power|Failure", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float FailureChanceStirring = 0.03f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GvT|Power|Failure", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float FailureChanceAwake = 0.08f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GvT|Power|Failure", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float FailureChanceHostile = 0.16f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GvT|Power|Failure", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float OffOnlyActivityMax = 0.30f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GvT|Power|Failure", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float BlowChanceAtMaxActivity = 0.65f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GvT|Power|Audio")
	TObjectPtr<USoundBase> PowerOnSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GvT|Power|Audio")
	TObjectPtr<USoundBase> PowerOffSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GvT|Power|Audio")
	TObjectPtr<USoundBase> PowerBlownSound;

	FTimerHandle TimerHandle_RandomFailure;
	float PowerTurnedOnTime = -1000.0f;
};
