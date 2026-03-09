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

UCLASS()
class GHOSTSVSTHIEVES_API AGvTPowerBoxActor : public AActor, public IGvTInteractable
{
	GENERATED_BODY()

public:
	AGvTPowerBoxActor();

	virtual void GetInteractionSpec_Implementation(APawn* InstigatorPawn, EGvTInteractionVerb Verb, FGvTInteractionSpec& OutSpec) const override;
	virtual bool CanInteract_Implementation(APawn* InstigatorPawn, EGvTInteractionVerb Verb) const override;
	virtual void BeginInteract_Implementation(APawn* InstigatorPawn, EGvTInteractionVerb Verb) override;
	virtual void CompleteInteract_Implementation(APawn* InstigatorPawn, EGvTInteractionVerb Verb) override;
	virtual void CancelInteract_Implementation(APawn* InstigatorPawn, EGvTInteractionVerb Verb, EGvTInteractionCancelReason Reason) override;

protected:
	virtual void BeginPlay() override;

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

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

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "GvT|Power")
	void Server_SetPowerState(EGvTHousePowerState NewState);

	UFUNCTION(BlueprintCallable, Category = "GvT|Power")
	void TogglePower();

	UFUNCTION(BlueprintCallable, Category = "GvT|Power")
	void BlowPowerBox();

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

	UFUNCTION(BlueprintCallable, Category = "GvT|Power")
	void HandlePlayerInteract(APawn* InstigatorPawn);

protected:
	UFUNCTION()
	void OnRep_PowerState();

	void ApplyPowerState();

	void InitializeIndicatorLights(TObjectPtr<UPointLightComponent> IndicatorLight, FColor Color);
};