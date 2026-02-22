#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "InputActionValue.h"
#include "GvTThiefCharacter.generated.h"

class UCameraComponent;
class USpringArmComponent;
class UInputMappingContext;
class UInputAction;
class UGvTNoiseEmitterComponent;
class UGvTInteractionComponent;

UCLASS()
class GHOSTSVSTHIEVES_API AGvTThiefCharacter : public ACharacter
{
    GENERATED_BODY()

public:
    AGvTThiefCharacter();

public:
    // Interaction lock-in (cast-time interactions)
    UFUNCTION(BlueprintCallable, Category="GvT|Interaction")
    void SetInteractionLock(bool bLockMove, bool bLockLook);

    UFUNCTION(BlueprintCallable, Category="GvT|Interaction")
    bool IsInteractionMoveLocked() const { return bInteractionLockMove; }

    UFUNCTION(BlueprintCallable, Category="GvT|Interaction")
    bool IsInteractionLookLocked() const { return bInteractionLockLook; }

    // --- Death / caught state ---
    UPROPERTY(ReplicatedUsing = OnRep_IsDead, BlueprintReadOnly, Category = "GvT|Death")
    bool bIsDead = false;

    UFUNCTION()
    void OnRep_IsDead();

    UFUNCTION(Server, Reliable)
    void Server_SetDead(AActor* Killer);

    UFUNCTION(BlueprintPure, Category = "GvT|Death")
    bool IsDead() const { return bIsDead; }

protected:
    virtual void BeginPlay() override;
    virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

    void OnMove(const FInputActionValue& Value);
    void OnLook(const FInputActionValue& Value);

    void StartSprint();
    void StopSprint();

    void StartCrouch();
    void StopCrouch();

    void TestNoise();

    void OnInteractPressed();

    void OnPhotoPressed();

protected:
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

    UFUNCTION(Server, Reliable)
    void ServerSetSprinting(bool bNewSprinting);

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    
protected:
    UPROPERTY(Replicated, BlueprintReadOnly, Category="GvT|Interaction")
    bool bInteractionLockMove = false;

    UPROPERTY(Replicated, BlueprintReadOnly, Category="GvT|Interaction")
    bool bInteractionLockLook = false;

UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GvT|Interaction")
    UGvTInteractionComponent* InteractionComponent;

protected:
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
    UInputAction* IA_TestNoise;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GvT|Input")
    UInputAction* IA_Interact;

    UFUNCTION(BlueprintImplementableEvent, Category = "GvT|Input")
    void BP_OnInteractPressed();

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GvT|Input")
    UInputAction* IA_Photo;

    UFUNCTION(BlueprintImplementableEvent, Category = "GvT|Input")
    void BP_OnPhotoPressed();

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GvT|Input")
    UInputAction* IA_TestMirror;

    UFUNCTION()
    void OnTestMirrorPressed();
};
