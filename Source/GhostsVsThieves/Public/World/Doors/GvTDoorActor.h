#pragma once

#include "TimerManager.h"
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "Interaction/GvTInteractable.h"
#include "GvTDoorActor.generated.h"

class USceneComponent;
class UStaticMeshComponent;
class UGvTNoiseEmitterComponent;
class USoundBase;

UENUM(BlueprintType)
enum class EDoorUnlockMethod : uint8
{
	Key,
	Lockpick,
	Hack,
	Force
};

UCLASS()
class GHOSTSVSTHIEVES_API AGvTDoorActor : public AActor, public IGvTInteractable
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Door|Lock")
	bool IsLocked() const { return bIsLocked; }

	UFUNCTION(BlueprintCallable, Category = "Door|Lock")
	void SetLocked(bool bNewLocked);

	UFUNCTION(BlueprintCallable, Category = "Door|Lock")
	bool TryUnlock(APawn* InstigatorPawn, EDoorUnlockMethod Method, bool bAutoOpenOnSuccess = true);

public:
	AGvTDoorActor();

	virtual void GetInteractionSpec_Implementation(APawn* InstigatorPawn, EGvTInteractionVerb Verb, FGvTInteractionSpec& OutSpec) const override;
	virtual bool CanInteract_Implementation(APawn* InstigatorPawn, EGvTInteractionVerb Verb) const override;
	virtual void BeginInteract_Implementation(APawn* InstigatorPawn, EGvTInteractionVerb Verb) override;
	virtual void CompleteInteract_Implementation(APawn* InstigatorPawn, EGvTInteractionVerb Verb) override;
	virtual void CancelInteract_Implementation(APawn* InstigatorPawn, EGvTInteractionVerb Verb, EGvTInteractionCancelReason Reason) override;

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

protected:
	UPROPERTY(VisibleAnywhere, Category = "Door")
	USceneComponent* Root = nullptr;

	UPROPERTY(VisibleAnywhere, Category = "Door")
	USceneComponent* Hinge = nullptr;

	UPROPERTY(VisibleAnywhere, Category = "Door")
	UStaticMeshComponent* DoorMesh = nullptr;

	// ---- Rotation ----
	UPROPERTY(EditAnywhere, Category = "Door|Rotation")
	float ClosedYaw = 0.f;

	UPROPERTY(EditAnywhere, Category = "Door|Rotation")
	float OpenYaw = 90.f;

	UPROPERTY(EditAnywhere, Category = "Door|Rotation")
	float OpenDuration = 0.6f;

	UPROPERTY(EditAnywhere, Category = "Door|Rotation")
	bool bInvertDirection = false;

	// ---- Noise ----
	UPROPERTY(EditAnywhere, Category = "Door|Noise")
	float DoorNoiseRadius = 1200.f;

	UPROPERTY(EditAnywhere, Category = "Door|Noise")
	float CloseEndNoiseRadius = 1200.f;

	UPROPERTY(EditAnywhere, Category = "Door|Noise")
	FGameplayTag DoorNoiseTag;

	UPROPERTY(VisibleAnywhere, Category = "Door|Noise")
	UGvTNoiseEmitterComponent* DoorNoiseEmitter = nullptr;

	// ---- Lock ----
	UPROPERTY(EditAnywhere, ReplicatedUsing = OnRep_IsLocked, Category = "Door|Lock")
	bool bIsLocked = false;

	UFUNCTION()
	void OnRep_IsLocked();

	UFUNCTION(Exec)
	void Debug_ToggleLock();

	// ---- Replicated anim state ----
	UPROPERTY(ReplicatedUsing = OnRep_IsOpen)
	bool bIsOpen = false;

	UPROPERTY(ReplicatedUsing = OnRep_DoorAnimStart)
	float DoorAnimStartServerTime = 0.f;

	UFUNCTION()
	void OnRep_DoorAnimStart();

	UFUNCTION()
	void OnRep_IsOpen();

	// ---- Local animation ----
	float AnimFromYaw = 0.f;
	float AnimToYaw = 0.f;
	bool bAnimating = false;

	void StartDoorAnim(bool bOpen);
	void ApplyDoorState(bool bOpen);

	// ---- Audio ----
	UPROPERTY(EditAnywhere, Category = "Door|Audio") USoundBase* SFX_OpenStart = nullptr;
	UPROPERTY(EditAnywhere, Category = "Door|Audio") USoundBase* SFX_CloseStart = nullptr;
	UPROPERTY(EditAnywhere, Category = "Door|Audio") USoundBase* SFX_CloseEnd = nullptr;
	UPROPERTY(EditAnywhere, Category = "Door|Audio") USoundBase* SFX_LockedRattle = nullptr;
	UPROPERTY(EditAnywhere, Category = "Door|Audio") USoundBase* SFX_Lock = nullptr;
	UPROPERTY(EditAnywhere, Category = "Door|Audio") USoundBase* SFX_Unlock = nullptr;

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_PlaySFX(USoundBase* Sound);

	// ---- Server-only close-end callback (one-shot) ----
	FTimerHandle TimerHandle_CloseEnd;

	void Server_DoCloseEnd();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
