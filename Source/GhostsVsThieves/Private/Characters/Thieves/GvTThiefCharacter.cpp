#include "Characters/Thieves/GvTThiefCharacter.h"

#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/PlayerController.h"
#include "Systems/Noise/GvTNoiseEmitterComponent.h"
#include "GameplayTagContainer.h"

AGvTThiefCharacter::AGvTThiefCharacter()
{
    bReplicates = true;

    SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
    SpringArm->SetupAttachment(GetRootComponent());
    SpringArm->TargetArmLength = 0.f;               
    SpringArm->bUsePawnControlRotation = true;

    Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
    Camera->SetupAttachment(SpringArm);
    Camera->bUsePawnControlRotation = false;

    bUseControllerRotationYaw = true;

    if (UCharacterMovementComponent* Move = GetCharacterMovement())
    {
        Move->bOrientRotationToMovement = false;
        Move->MaxWalkSpeed = WalkSpeed;
    }

    NoiseEmitter = CreateDefaultSubobject<UGvTNoiseEmitterComponent>(TEXT("NoiseEmitter"));
    NoiseEmitter->EmitNoise(FGameplayTag::RequestGameplayTag(TEXT("Noise.Interact")), 600.f, 1.0f);

}

void AGvTThiefCharacter::BeginPlay()
{
    Super::BeginPlay();

    if (UCharacterMovementComponent* Move = GetCharacterMovement())
    {
        Move->MaxWalkSpeed = WalkSpeed;
    }

    if (APlayerController* PC = Cast<APlayerController>(Controller))
    {
        if (ULocalPlayer* LP = PC->GetLocalPlayer())
        {
            if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
                LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
            {
                if (DefaultMappingContext)
                {
                    Subsystem->AddMappingContext(DefaultMappingContext, 0);
                }
            }
        }
    }
}

void AGvTThiefCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent);
    if (!EIC) return;

    if (IA_Move)
    {
        EIC->BindAction(IA_Move, ETriggerEvent::Triggered, this, &AGvTThiefCharacter::OnMove);
    }

    if (IA_Look)
    {
        EIC->BindAction(IA_Look, ETriggerEvent::Triggered, this, &AGvTThiefCharacter::OnLook);
    }

    if (IA_Sprint)
    {
        EIC->BindAction(IA_Sprint, ETriggerEvent::Started, this, &AGvTThiefCharacter::StartSprint);
        EIC->BindAction(IA_Sprint, ETriggerEvent::Completed, this, &AGvTThiefCharacter::StopSprint);
        EIC->BindAction(IA_Sprint, ETriggerEvent::Canceled, this, &AGvTThiefCharacter::StopSprint);
    }

    if (IA_Crouch)
    {
        EIC->BindAction(IA_Crouch, ETriggerEvent::Started, this, &AGvTThiefCharacter::StartCrouch);
        EIC->BindAction(IA_Crouch, ETriggerEvent::Completed, this, &AGvTThiefCharacter::StopCrouch);
        EIC->BindAction(IA_Crouch, ETriggerEvent::Canceled, this, &AGvTThiefCharacter::StopCrouch);
    }

    if (IA_TestNoise)
    {
        EIC->BindAction(IA_TestNoise, ETriggerEvent::Started, this, &AGvTThiefCharacter::TestNoise);
    }

}

void AGvTThiefCharacter::OnMove(const FInputActionValue& Value)
{
    if (!Controller) return;

    const FVector2D Move = Value.Get<FVector2D>();
    if (Move.IsNearlyZero()) return;

    const FRotator YawRot(0.f, Controller->GetControlRotation().Yaw, 0.f);
    const FVector Forward = FRotationMatrix(YawRot).GetUnitAxis(EAxis::X);
    const FVector Right = FRotationMatrix(YawRot).GetUnitAxis(EAxis::Y);

    AddMovementInput(Forward, Move.Y);
    AddMovementInput(Right, Move.X);
}

void AGvTThiefCharacter::OnLook(const FInputActionValue& Value)
{
    const FVector2D Look = Value.Get<FVector2D>();
    AddControllerYawInput(Look.X);
    AddControllerPitchInput(Look.Y);
}

void AGvTThiefCharacter::StartSprint()
{
    if (bIsSprinting) return;
    ServerSetSprinting(true);
}

void AGvTThiefCharacter::StopSprint()
{
    if (!bIsSprinting) return;
    ServerSetSprinting(false);
}

void AGvTThiefCharacter::StartCrouch()
{
    Crouch();
}

void AGvTThiefCharacter::StopCrouch()
{
    UnCrouch();
}

void AGvTThiefCharacter::ServerSetSprinting_Implementation(bool bNewSprinting)
{
    bIsSprinting = bNewSprinting;

    if (UCharacterMovementComponent* Move = GetCharacterMovement())
    {
        Move->MaxWalkSpeed = bIsSprinting ? SprintSpeed : WalkSpeed;
    }
}

void AGvTThiefCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AGvTThiefCharacter, bIsSprinting);
}

void AGvTThiefCharacter::TestNoise()
{
    if (!NoiseEmitter) return;

    NoiseEmitter->EmitNoise(
        FGameplayTag::RequestGameplayTag(TEXT("Noise.Interact")),
        600.f,
        1.0f
    );
}
