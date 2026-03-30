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

protected:
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "GvT|Power")
	void Server_SetPowerState(EGvTHousePowerState NewState);

	UFUNCTION()
	void OnRep_PowerState();

	void ApplyPowerState();

	void InitializeIndicatorLights(TObjectPtr<UPointLightComponent> IndicatorLight, FColor Color);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GvT|Power")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GvT|Power")
	TObjectPtr<UStaticMeshComponent> PowerBoxMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GvT|Power")
	TObjectPtr<UBoxComponent> InteractionBounds;

	UPROPERTY(ReplicatedUsing = OnRep_PowerState, EditAnywhere, BlueprintReadOnly, Category = "GvT|Power")
	EGvTHousePowerState PowerState = EGvTHousePowerState::On;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "GvT|Power")
	TObjectPtr<AActor> HouseActor = nullptr;

	UFUNCTION(BlueprintPure, Category = "GvT|Power")
	bool IsPowerOn() const { return PowerState == EGvTHousePowerState::On; }

	UFUNCTION(BlueprintPure, Category = "GvT|Power")
	bool IsPowerBlown() const { return PowerState == EGvTHousePowerState::Blown; }

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GvT|Power")
	TObjectPtr<UPointLightComponent> OnIndicatorLight;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GvT|Power")
	TObjectPtr<UPointLightComponent> OffIndicatorLight;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GvT|Power")
	TObjectPtr<UPointLightComponent> BlownIndicatorLight;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GvT|Power")
	FGameplayTag PowerInteractNoiseTag;
};