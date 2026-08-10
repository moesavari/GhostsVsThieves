#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "InputActionValue.h"
#include "Gameplay/Scare/GvTScareTypes.h"
#include "GameplayTagContainer.h"
#include "GvTThiefCharacter.generated.h"

class UCameraComponent;
class USpringArmComponent;
class UInputMappingContext;
class UInputAction;
class UGvTNoiseEmitterComponent;
class UGvTInteractionComponent;
class UGvTDirectorSubsystem;
class AGvTGhostCharacterBase;
class UGvTThiefPerceptionComponent;
class UGvTInventoryComponent;
class USceneComponent;
class USoundBase;

UCLASS()
class GHOSTSVSTHIEVES_API AGvTThiefCharacter : public ACharacter
{
    GENERATED_BODY()

public:
    AGvTThiefCharacter();

    UFUNCTION(BlueprintPure, Category = "GvT|Inventory")
    UGvTInventoryComponent* GetInventoryComponent() const { return InventoryComponent; }

    UFUNCTION(BlueprintPure, Category = "GvT|Inventory")
    USceneComponent* GetHeldItemAnchor() const { return HeldItemAnchor; }

    UFUNCTION(BlueprintCallable, Category = "GvT|Interaction")
    void SetInteractionLock(bool bLockMove, bool bLockLook);

    UFUNCTION(BlueprintCallable, Category = "GvT|Scare")
    void ApplyScareStun(float Duration);

    UFUNCTION(BlueprintPure, Category = "GvT|Death")
    bool IsDead() const { return bIsDead; }

    UFUNCTION(BlueprintPure, Category = "GvT|Scare")
    bool IsScareStunned() const { return ScareStunCount > 0; }

    UFUNCTION(Client, Reliable)
    void Client_PlayLocalScareStun(float Duration);

    UFUNCTION(Client, Reliable)
    void Client_PlayGhostEvent(FGameplayTag GhostEventTag);

    UFUNCTION(BlueprintCallable, Category = "GvT|Interaction")
    bool IsInteractionMoveLocked() const { return bInteractionLockMove; }

    UFUNCTION(BlueprintCallable, Category = "GvT|Interaction")
    bool IsInteractionLookLocked() const { return bInteractionLockLook; }

    UFUNCTION()
    void OnRep_IsDead();

    UFUNCTION(Server, Reliable)
    void Server_SetDead(AActor* Killer);

    UFUNCTION(BlueprintCallable, Category = "GvT|Ghost")
    void RequestGhostScare(FGameplayTag GhostScareTag);

    UFUNCTION(BlueprintCallable, Category = "GvT|Ghost")
    void RequestGhostEvent(FGameplayTag GhostEventTag);

    UFUNCTION(BlueprintCallable, Category = "GvT|Ghost")
    void RequestGhostHaunt(FGameplayTag GhostHauntTag);

    UFUNCTION(BlueprintCallable, Category = "Debug|Ghost")
    void Debug_RequestGhostScare(FGameplayTag GhostScareTag);

    UFUNCTION(BlueprintCallable, Category = "Debug|Ghost")
    void Debug_RequestGhostEvent(FGameplayTag GhostEventTag);

    UFUNCTION(BlueprintCallable, Category = "Debug|Ghost")
    void Debug_RequestGhostHaunt(FGameplayTag GhostHauntTag);

    UFUNCTION(Server, Reliable)
    void Server_RequestGhostScare(FGameplayTag GhostScareTag);

    UFUNCTION(Server, Reliable)
    void Server_RequestGhostHaunt(FGameplayTag GhostHauntTag);

    UFUNCTION(Server, Reliable)
    void Server_RequestGhostEvent(FGameplayTag GhostEventTag);

    UFUNCTION(Client, Reliable)
    void Client_PlayGhostScare(FGameplayTag GhostScareTag);

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GvT|Perception")
    UGvTThiefPerceptionComponent* ThiefPerceptionComponent;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug|Ghost")
    TSubclassOf<AGvTGhostCharacterBase> DebugGhostClass;

    UPROPERTY(Transient)
    TObjectPtr<AGvTGhostCharacterBase> DebugActiveGhost;

    UPROPERTY(Transient)
    TObjectPtr<AGvTGhostCharacterBase> LocalActiveScareGhost;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug|Ghost", meta = (ClampMin = "0.0"))
    float DebugGhostScareRequestCooldown = 1.25f;

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    bool TryFindSafeLocalGhostScareSpawn(FVector& OutLocation, FRotator& OutRotation) const;

    void OnMove(const FInputActionValue& Value);
    void OnLook(const FInputActionValue& Value);
    void StartSprint();
    void StopSprint();
    void StartCrouch();
    void StopCrouch();
    void OnJumpStarted();
    void OnJumpStopped();
    void ToggleDebugHUD();
    void UpdateDebugHUD();
    void ApplyDebugDrawState(bool bEnabled);
    void TestNoise();
    void OnInteractPressed();
    void OnPhotoPressed();
    void OnTestMirrorPressed();
    void OnInventoryNext();
    void OnInventoryPrevious();
    void OnDropItem();
    void OnToggleFlashlight();

    UFUNCTION(NetMulticast, Unreliable)
    void Multicast_PlayFootstep(USoundBase* Sound, float Volume, float Pitch);

    void UpdateFootsteps(float DeltaSeconds);

    UFUNCTION(Server, Reliable)
    void ServerSetSprinting(bool bNewSprinting);

    UFUNCTION(Server, Reliable)
    void Server_SetDebugDrawEnabled(bool bEnabled);

    UFUNCTION(NetMulticast, Reliable)
    void Multicast_SetDebugDrawEnabled(bool bEnabled);

    UFUNCTION(BlueprintImplementableEvent, Category = "GvT|Input")
    void BP_OnInteractPressed();

    UFUNCTION(BlueprintImplementableEvent, Category = "GvT|Input")
    void BP_OnPhotoPressed();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GvT|Camera")
    USpringArmComponent* SpringArm;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GvT|Camera")
    UCameraComponent* Camera;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GvT|Movement")
    float WalkSpeed = 450.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GvT|Movement")
    float SprintSpeed = 650.f;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "GvT|Movement")
    bool bIsSprinting = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GvT|Noise")
    UGvTNoiseEmitterComponent* NoiseEmitter;

    UPROPERTY(ReplicatedUsing = OnRep_IsDead, BlueprintReadOnly, Category = "GvT|Death")
    bool bIsDead = false;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "GvT|Interaction")
    bool bInteractionLockMove = false;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "GvT|Interaction")
    bool bInteractionLockLook = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GvT|Interaction")
    UGvTInteractionComponent* InteractionComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GvT|Inventory")
    UGvTInventoryComponent* InventoryComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GvT|Inventory")
    USceneComponent* HeldItemAnchor;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GvT|Scare")
    int32 ScareStunCount = 0;


    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GvT|Footsteps")
    TArray<TObjectPtr<USoundBase>> FootstepSounds;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GvT|Footsteps")
    FGameplayTag FootstepNoiseTag;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GvT|Footsteps", meta = (ClampMin = "0.05"))
    float WalkFootstepInterval = 0.48f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GvT|Footsteps", meta = (ClampMin = "0.05"))
    float SprintFootstepInterval = 0.32f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GvT|Footsteps", meta = (ClampMin = "0.05"))
    float CrouchFootstepInterval = 0.68f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GvT|Footsteps", meta = (ClampMin = "0.0"))
    float WalkFootstepNoiseRadius = 500.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GvT|Footsteps", meta = (ClampMin = "0.0"))
    float SprintFootstepNoiseRadius = 850.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GvT|Footsteps", meta = (ClampMin = "0.0"))
    float CrouchFootstepNoiseRadius = 225.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GvT|Footsteps", meta = (ClampMin = "0.0"))
    float FootstepNoiseLoudness = 1.0f;

    float FootstepTimeAccumulator = 0.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GvT|Input")
    UInputMappingContext* DefaultMappingContext;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GvT|Input")
    UInputAction* IA_Move;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GvT|Input")
    UInputAction* IA_Look;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GvT|Input")
    UInputAction* IA_Sprint;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GvT|Input")
    UInputAction* IA_Crouch;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GvT|Input")
    UInputAction* IA_Jump;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GvT|Input")
    UInputAction* IA_ToggleDebugHUD;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GvT|Input")
    UInputAction* IA_TestNoise;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GvT|Input")
    UInputAction* IA_Interact;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GvT|Input")
    UInputAction* IA_Photo;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GvT|Input")
    UInputAction* IA_TestMirror;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GvT|Input")
    UInputAction* IA_InventoryNext;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GvT|Input")
    UInputAction* IA_InventoryPrevious;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GvT|Input")
    UInputAction* IA_DropItem;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GvT|Input")
    UInputAction* IA_ToggleFlashlight;

private:
    void ClearScareStun();
    void ClearScareStun(int32 ClearCountAtScheduleTime);

    float LastGhostScareRequestWorldTime = -1000.f;
    bool bDebugHUDEnabled = false;
    static constexpr uint64 DebugHUDMessageKey = 0x4756544445425547ULL;

    FTimerHandle TimerHandle_ClearScareStun;
    FTimerHandle TimerHandle_LocalCloseScareCleanup;
};
