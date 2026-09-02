#include "Gameplay/Characters/Thieves/GvTDeadSpectatorPawn.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GvTGameInstance.h"
#include "GvTPlayerController.h"

AGvTDeadSpectatorPawn::AGvTDeadSpectatorPawn()
{
    PrimaryActorTick.bCanEverTick = true;
    bReplicates = true;
    SetReplicateMovement(true);

    GetCapsuleComponent()->InitCapsuleSize(28.f, 72.f);
    GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    GetCapsuleComponent()->SetCollisionResponseToAllChannels(ECR_Ignore);
    GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
    GetCapsuleComponent()->SetGenerateOverlapEvents(false);

    GetMesh()->SetVisibility(false, true);
    GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
    Camera->SetupAttachment(GetCapsuleComponent());
    Camera->SetRelativeLocation(FVector(0.f, 0.f, StandingCameraHeight));
    Camera->bUsePawnControlRotation = true;

    bUseControllerRotationYaw = true;

    if (UCharacterMovementComponent* Movement = GetCharacterMovement())
    {
        Movement->bOrientRotationToMovement = false;
        Movement->MaxWalkSpeed = 500.f;
        Movement->MaxWalkSpeedCrouched = 350.f;
        Movement->GetNavAgentPropertiesRef().bCanCrouch = true;
    }
}

void AGvTDeadSpectatorPawn::BeginPlay()
{
    Super::BeginPlay();
    GetCapsuleComponent()->SetCollisionObjectType(SpectatorObjectChannel);
}

void AGvTDeadSpectatorPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent);
    if (!EnhancedInput)
    {
        return;
    }

    if (IA_Move)
    {
        EnhancedInput->BindAction(IA_Move, ETriggerEvent::Triggered, this, &ThisClass::OnMove);
    }

    if (IA_Look)
    {
        EnhancedInput->BindAction(IA_Look, ETriggerEvent::Triggered, this, &ThisClass::OnLook);
    }

    if (IA_Pause)
    {
        EnhancedInput->BindAction(IA_Pause, ETriggerEvent::Started, this, &ThisClass::OnPausePressed);
    }

    if (IA_Crouch)
    {
        EnhancedInput->BindAction(IA_Crouch, ETriggerEvent::Started, this, &ThisClass::OnCrouchStarted);
        EnhancedInput->BindAction(IA_Crouch, ETriggerEvent::Completed, this, &ThisClass::OnCrouchEnded);
        EnhancedInput->BindAction(IA_Crouch, ETriggerEvent::Canceled, this, &ThisClass::OnCrouchEnded);
    }
}

void AGvTDeadSpectatorPawn::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (!Camera)
    {
        return;
    }

    const float TargetHeight = bWantsCrouchedView ? CrouchedCameraHeight : StandingCameraHeight;
    FVector RelativeLocation = Camera->GetRelativeLocation();
    RelativeLocation.Z = FMath::FInterpTo(RelativeLocation.Z, TargetHeight, DeltaSeconds, CameraHeightInterpSpeed);
    Camera->SetRelativeLocation(RelativeLocation);
}

void AGvTDeadSpectatorPawn::OnMove(const FInputActionValue& Value)
{
    const FVector2D MovementValue = Value.Get<FVector2D>();
    if (MovementValue.IsNearlyZero())
    {
        return;
    }

    // Looking up or down must never turn movement into flight.
    const FRotator YawRotation(0.f, GetControlRotation().Yaw, 0.f);
    AddMovementInput(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X), MovementValue.Y);
    AddMovementInput(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y), MovementValue.X);
}

void AGvTDeadSpectatorPawn::OnLook(const FInputActionValue& Value)
{
    const FVector2D LookValue = Value.Get<FVector2D>();
    const UGvTGameInstance* GvTGameInstance = GetGameInstance<UGvTGameInstance>();
    const float Sensitivity = GvTGameInstance ? GvTGameInstance->GetMouseSensitivity() : 1.0f;
    AddControllerYawInput(LookValue.X * Sensitivity);
    AddControllerPitchInput(LookValue.Y * Sensitivity);
}

void AGvTDeadSpectatorPawn::OnPausePressed()
{
    if (AGvTPlayerController* GvTController = Cast<AGvTPlayerController>(GetController()))
    {
        GvTController->TogglePauseMenu();
    }
}

void AGvTDeadSpectatorPawn::OnCrouchStarted()
{
    bWantsCrouchedView = true;
    Crouch();
}

void AGvTDeadSpectatorPawn::OnCrouchEnded()
{
    bWantsCrouchedView = false;
    UnCrouch();
}
