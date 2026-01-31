#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "InputActionValue.h"
#include "GvTThiefCharacter.generated.h"

class UCameraComponent;
class USpringArmComponent;
class UInputMappingContext;
class UInputAction;

UCLASS()
class GHOSTSVSTHIEVES_API AGvTThiefCharacter : public ACharacter
{
    GENERATED_BODY()

public:
    AGvTThiefCharacter();

protected:
    virtual void BeginPlay() override;
    virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

    // Enhanced Input handlers
    void OnMove(const FInputActionValue& Value);
    void OnLook(const FInputActionValue& Value);

    void StartSprint();
    void StopSprint();

    void StartCrouch();
    void StopCrouch();

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

    UFUNCTION(Server, Reliable)
    void ServerSetSprinting(bool bNewSprinting);

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
    // Enhanced Input assets (assign in BP)
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
};
