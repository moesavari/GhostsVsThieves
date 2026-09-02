#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "InputActionValue.h"
#include "GvTDeadSpectatorPawn.generated.h"

class UCameraComponent;
class UInputAction;

/** Invisible, non-interactive roaming camera possessed after a thief dies. */
UCLASS()
class GHOSTSVSTHIEVES_API AGvTDeadSpectatorPawn : public ACharacter
{
    GENERATED_BODY()

public:
    AGvTDeadSpectatorPawn();
    virtual void BeginPlay() override;
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
    virtual void Tick(float DeltaSeconds) override;

protected:
    void OnMove(const FInputActionValue& Value);
    void OnLook(const FInputActionValue& Value);
    void OnPausePressed();
    void OnCrouchStarted();
    void OnCrouchEnded();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GvT|Spectator")
    TObjectPtr<UCameraComponent> Camera;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GvT|Spectator|Input")
    TObjectPtr<UInputAction> IA_Move;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GvT|Spectator|Input")
    TObjectPtr<UInputAction> IA_Look;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GvT|Spectator|Input")
    TObjectPtr<UInputAction> IA_Pause;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GvT|Spectator|Input")
    TObjectPtr<UInputAction> IA_Crouch;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GvT|Spectator", meta=(ClampMin="0.0"))
    float StandingCameraHeight = 64.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GvT|Spectator", meta=(ClampMin="0.0"))
    float CrouchedCameraHeight = 34.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GvT|Spectator", meta=(ClampMin="0.0"))
    float CameraHeightInterpSpeed = 10.f;

    /** Set this to the custom DeadSpectator object channel in the Blueprint defaults. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GvT|Spectator|Collision")
    TEnumAsByte<ECollisionChannel> SpectatorObjectChannel = ECC_Pawn;

private:
    bool bWantsCrouchedView = false;
};
