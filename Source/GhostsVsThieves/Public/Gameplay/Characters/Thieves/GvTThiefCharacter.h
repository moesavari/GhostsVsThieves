#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "InputActionValue.h"
#include "Gameplay/Scare/GvTScareTypes.h"
#include "GvTThiefCharacter.generated.h"

class UCameraComponent;
class USpringArmComponent;
class UInputMappingContext;
class UInputAction;
class UGvTNoiseEmitterComponent;
class UGvTInteractionComponent;
class UGvTDirectorSubsystem;
class AGvTGhostCharacterBase;

UCLASS()
class GHOSTSVSTHIEVES_API AGvTThiefCharacter : public ACharacter
{
    GENERATED_BODY()

public:
    AGvTThiefCharacter();

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
    void Client_PlayLocalGhostScare(const FGvTScareEvent& Event);

    UFUNCTION(BlueprintCallable, Category = "GvT|Interaction")
    bool IsInteractionMoveLocked() const { return bInteractionLockMove; }

    UFUNCTION(BlueprintCallable, Category = "GvT|Interaction")
    bool IsInteractionLookLocked() const { return bInteractionLockLook; }

    UFUNCTION()
    void OnRep_IsDead();

    UFUNCTION(Server, Reliable)
    void Server_SetDead(AActor* Killer);

    UFUNCTION(BlueprintCallable, Category = "Debug")
    void Debug_RequestMirrorScare();

    UFUNCTION(BlueprintCallable, Category = "Debug")
    void Debug_RequestRearAudioStingScare();

    UFUNCTION(BlueprintCallable, Category = "Debug")
    void Debug_RequestGhostScreamScare();

    UFUNCTION(BlueprintCallable, Category = "Debug")
    void Debug_RequestDoorSlamBehindScare();

    UFUNCTION(BlueprintCallable, Category = "Debug")
    void Debug_RequestLightChaseScare();

    UFUNCTION(BlueprintCallable, Category = "Debug")
    void Debug_RequestGhostChaseScare();

    UFUNCTION(BlueprintCallable, Category = "Debug")
    void Debug_RequestGhostScare();

    UFUNCTION(Server, Reliable)
    void Server_DebugRequestMirrorScare();

    UFUNCTION(Server, Reliable)
    void Server_DebugRequestRearAudioStingScare();

    UFUNCTION(Server, Reliable)
    void Server_DebugRequestGhostScreamScare();

    UFUNCTION(Server, Reliable)
    void Server_DebugRequestDoorSlamBehindScare();

    UFUNCTION(Server, Reliable)
    void Server_DebugRequestLightChaseScare();

    UFUNCTION(Server, Reliable)
    void Server_DebugTriggerMirrorActor(AGvTMirrorActor* Mirror);

    UFUNCTION(Server, Reliable)
    void Server_DebugRequestGhostChaseScare();

    UFUNCTION(Server, Reliable)
    void Server_DebugRequestGhostScare();

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GvT|Scare|Local")
    TSubclassOf<AGvTGhostCharacterBase> LocalHauntGhostClass;

protected:
    virtual void BeginPlay() override;
    virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    void OnMove(const FInputActionValue& Value);
    void OnLook(const FInputActionValue& Value);
    void StartSprint();
    void StopSprint();
    void StartCrouch();
    void StopCrouch();
    void TestNoise();
    void OnInteractPressed();
    void OnPhotoPressed();
    void OnTestMirrorPressed();

    UFUNCTION(Server, Reliable)
    void ServerSetSprinting(bool bNewSprinting);

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

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GvT|Scare")
    int32 ScareStunCount = 0;

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

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GvT|Input")
    UInputAction* IA_Photo;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GvT|Input")
    UInputAction* IA_TestMirror;

private:
    void ClearScareStun();
    void ClearScareStun(int32 ClearCountAtScheduleTime);
    void PlayLocalHauntGhostScare(const FGvTScareEvent& Event);

    FTimerHandle TimerHandle_ClearScareStun;
};
