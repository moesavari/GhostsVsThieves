#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Gameplay/Interaction/GvTInteractionTypes.h"
#include "GvTInteractionComponent.generated.h"

class AGvTThiefCharacter;
struct FCollisionQueryParams;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FGvTInteractionStarted, EGvTInteractionVerb, Verb, AActor*, Target);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FGvTInteractionCanceled, EGvTInteractionVerb, Verb, AActor*, Target, EGvTInteractionCancelReason, Reason);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FGvTInteractionCompleted, EGvTInteractionVerb, Verb, AActor*, Target);

UCLASS(ClassGroup=(GvT), meta=(BlueprintSpawnableComponent))
class GHOSTSVSTHIEVES_API UGvTInteractionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGvTInteractionComponent();

	// Client entry points (bound to input)
	UFUNCTION(BlueprintCallable, Category="GvT|Interaction")
	void TryInteract();

	/** Uses the selected medicine or lockpick. Bind this to the equipment input, never world-interact. */
	UFUNCTION(BlueprintCallable, Category="GvT|Interaction")
	void TryUseSelectedEquipment();

	UFUNCTION(BlueprintCallable, Category="GvT|Interaction")
	void TryPhoto();

	UFUNCTION(BlueprintCallable, Category = "GvT|Interaction")
	void TryScan();

	// Client entry point to cancel (useful for "release to cancel" or pressing again)
	UFUNCTION(BlueprintCallable, Category="GvT|Interaction")
	void TryCancelInteraction(EGvTInteractionCancelReason Reason = EGvTInteractionCancelReason::UserCanceled);

	UFUNCTION(BlueprintCallable, Category="GvT|Interaction")
	bool IsInteracting() const { return bIsInteracting; }

	/** Authority-controlled mission gate. Disabling also cancels an active cast. */
	UFUNCTION(BlueprintCallable, Category="GvT|Interaction")
	void SetInteractionEnabled(bool bEnabled);

	UFUNCTION(BlueprintPure, Category="GvT|Interaction")
	bool IsInteractionEnabled() const { return bInteractionEnabled; }

	UFUNCTION(BlueprintCallable, Category = "GvT|Interaction")
	bool IsScanning() const;

	// Replicated timings for UI
	UFUNCTION(BlueprintCallable, Category="GvT|Interaction")
	float GetInteractionStartServerTime() const { return InteractionStartServerTime; }

	UFUNCTION(BlueprintCallable, Category="GvT|Interaction")
	float GetInteractionEndServerTime() const { return InteractionEndServerTime; }

	UFUNCTION(BlueprintCallable, Category="GvT|Interaction")
	EGvTInteractionVerb GetActiveVerb() const { return ActiveVerb; }

	UFUNCTION(BlueprintCallable, Category="GvT|Interaction")
	AActor* GetCurrentInteractable() const { return CurrentInteractable; }

	UFUNCTION(BlueprintCallable, Category="GvT|Interaction")
	FGvTInteractionSpec GetActiveSpec() const { return ActiveSpec; }

	UFUNCTION(BlueprintCallable, Category="GvT|Interaction")
	void SetDebugDraw(bool bEnabled) { bDebugDraw = bEnabled; }

	UPROPERTY(BlueprintAssignable, Category="GvT|Interaction")
	FGvTInteractionStarted OnInteractionStarted;

	UPROPERTY(BlueprintAssignable, Category="GvT|Interaction")
	FGvTInteractionCanceled OnInteractionCanceled;

	UPROPERTY(BlueprintAssignable, Category="GvT|Interaction")
	FGvTInteractionCompleted OnInteractionCompleted;

protected:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	UPROPERTY(EditDefaultsOnly, Category="GvT|Interaction")
	float TraceDistance = 350.f;

	UPROPERTY(EditDefaultsOnly, Category="GvT|Interaction")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

	/**
	 * Radius of the fallback sweep used when the precise interaction trace does
	 * not hit an interactable item. This makes small loot less pixel-perfect to
	 * target without increasing interaction distance or permitting wall grabs.
	 */
	UPROPERTY(EditDefaultsOnly, Category="GvT|Interaction|Item Assist", meta=(ClampMin="0.0", UIMin="0.0", UIMax="50.0"))
	float ItemInteractionAssistRadius = 22.f;

	UPROPERTY(EditDefaultsOnly, Category="GvT|Interaction")
	bool bDebugDraw = false;

	UFUNCTION(BlueprintImplementableEvent, Category = "GvT|Interaction")
	void BP_OnInteractionStateChanged(bool bNowInteracting, EGvTInteractionVerb Verb, AActor* Target, FGvTInteractionSpec Spec);

	// Replicated state
	UPROPERTY(ReplicatedUsing=OnRep_InteractionState)
	bool bIsInteracting = false;

	UPROPERTY(Replicated)
	bool bInteractionEnabled = true;

	UPROPERTY(ReplicatedUsing=OnRep_InteractionState)
	AActor* CurrentInteractable = nullptr;

	UPROPERTY(ReplicatedUsing=OnRep_InteractionState)
	float InteractionStartServerTime = 0.f;

	UPROPERTY(ReplicatedUsing=OnRep_InteractionState)
	float InteractionEndServerTime = 0.f;

	UPROPERTY(ReplicatedUsing=OnRep_InteractionState)
	EGvTInteractionVerb ActiveVerb = EGvTInteractionVerb::Interact;

	UPROPERTY(ReplicatedUsing=OnRep_InteractionState)
	FGvTInteractionSpec ActiveSpec;

	UPROPERTY()
	float LastScanServerTime = -9999.f;

	UPROPERTY(EditDefaultsOnly, Category = "GvT|Interaction")
	float ScanAttemptCooldownSeconds = 0.25f;

	UFUNCTION()
	void OnRep_InteractionState();

	// RPCs (server authoritative)
	UFUNCTION(Server, Reliable)
	void Server_TryInteract(EGvTInteractionVerb Verb);

	UFUNCTION(Server, Reliable)
	void Server_TryUseSelectedEquipment();

	UFUNCTION(Server, Reliable)
	void Server_CancelInteraction(EGvTInteractionCancelReason Reason);

	UFUNCTION(Client, Reliable)
	void Client_PlayInteractionFinishSfx(bool bCompleted, EGvTInteractionVerb Verb, const FGvTInteractionSpec& Spec);

private:
	// Server helpers
	void PerformServerTraceAndTryStart(EGvTInteractionVerb Verb, bool bEquipmentUse = false);
	AActor* FindAssistedItemTarget(const FVector& Start, const FVector& End, const FCollisionQueryParams& Params) const;
	bool TryStartSelectedMedicine();
	bool GetViewTrace(FVector& OutStart, FVector& OutEnd) const;

	void BeginInteraction(AActor* Target, EGvTInteractionVerb Verb, const FGvTInteractionSpec& Spec);
	void CompleteInteraction();
	void CancelInteractionInternal(EGvTInteractionCancelReason Reason);

	void ApplyLockIn(const FGvTInteractionSpec& Spec);
	void ClearLockIn();

	AGvTThiefCharacter* GetOwnerThief() const;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	FTimerHandle InteractionTimerHandle;
	FTimerHandle PeriodicNoiseTimerHandle;

	UPROPERTY(Transient)
	TObjectPtr<AActor> RequiredSelectedItem = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "GvT|Interaction", meta = (ClampMin = "0.0"))
	float ActiveInteractionRange = 425.f;

	void EmitPeriodicInteractionNoise();
	bool IsActiveInteractionStillValid() const;

	bool bPrevInteracting_Local = false;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> ActiveLoopAudio = nullptr;

	EGvTInteractionVerb PrevVerb_Local = EGvTInteractionVerb::Interact;
	FGvTInteractionSpec PrevSpec_Local;
};
