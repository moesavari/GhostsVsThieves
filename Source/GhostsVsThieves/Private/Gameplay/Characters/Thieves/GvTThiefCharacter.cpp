#include "Gameplay/Characters/Thieves/GvTThiefCharacter.h"

#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/PlayerController.h"
#include "Systems/Noise/GvTNoiseEmitterComponent.h"
#include "GameplayTagContainer.h"
#include "Gameplay/Interaction/GvTInteractionComponent.h"
#include "Gameplay/Ghosts/Mirror/GvTMirrorActor.h"
#include "Camera/PlayerCameraManager.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"

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

    InteractionComponent = CreateDefaultSubobject<UGvTInteractionComponent>(TEXT("InteractionComponent"));

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

    if (IA_Interact)
    {
        EIC->BindAction(IA_Interact, ETriggerEvent::Started, this, &AGvTThiefCharacter::OnInteractPressed);
    }

    if (IA_Photo)
    {
        EIC->BindAction(IA_Photo, ETriggerEvent::Started, this, &AGvTThiefCharacter::OnPhotoPressed);
    }

    if (IA_TestMirror)
    {
        EIC->BindAction(IA_TestMirror, ETriggerEvent::Started, this, &AGvTThiefCharacter::OnTestMirrorPressed);
    }

}

void AGvTThiefCharacter::OnMove(const FInputActionValue& Value)
{
    if (bInteractionLockMove) { return; }

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
    if (bInteractionLockLook) { return; }

    const FVector2D Look = Value.Get<FVector2D>();
    AddControllerYawInput(Look.X);
    AddControllerPitchInput(Look.Y);
}

void AGvTThiefCharacter::StartSprint()
{
    if (bInteractionLockMove) { return; }

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
    if (bInteractionLockMove) { return; }

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
    DOREPLIFETIME(AGvTThiefCharacter, bInteractionLockMove);
    DOREPLIFETIME(AGvTThiefCharacter, bInteractionLockLook);
    DOREPLIFETIME(AGvTThiefCharacter, bIsDead);
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

void AGvTThiefCharacter::OnInteractPressed()
{
    if (!IsLocallyControlled() || !InteractionComponent)
        return;

    InteractionComponent->TryInteract();
}

void AGvTThiefCharacter::OnPhotoPressed()
{
    if (!IsLocallyControlled() || !InteractionComponent)
        return;

    //InteractionComponent->TryPhoto();
    InteractionComponent->TryScan();
}


void AGvTThiefCharacter::SetInteractionLock(bool bLockMove, bool bLockLook)
{
    // Only the server should replicate authoritative lock flags,
    // but applying locally keeps input responsive.
    bInteractionLockMove = bLockMove;
    bInteractionLockLook = bLockLook;

    if (bInteractionLockMove)
    {
        // Hard stop movement immediately
        GetCharacterMovement()->StopMovementImmediately();
    }
}

void AGvTThiefCharacter::OnTestMirrorPressed()
{
    if (!IsLocallyControlled())
    {
        return;
    }

    APlayerController* PC = Cast<APlayerController>(GetController());
    if (!PC || !PC->PlayerCameraManager)
    {
        return;
    }

    const FVector Start = PC->PlayerCameraManager->GetCameraLocation();
    const FVector Dir = PC->PlayerCameraManager->GetCameraRotation().Vector();
    const FVector End = Start + Dir * 2000.f;

    FCollisionQueryParams Params(SCENE_QUERY_STAT(TestMirrorTrace), false, this);
    FHitResult Hit;

    const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params);
    if (!bHit)
    {
        UE_LOG(LogTemp, Warning, TEXT("[MirrorTest] No hit."));
        return;
    }

    AGvTMirrorActor* Mirror = Cast<AGvTMirrorActor>(Hit.GetActor());
    if (!Mirror)
    {
        UE_LOG(LogTemp, Warning, TEXT("[MirrorTest] Hit %s, not a mirror."), *GetNameSafe(Hit.GetActor()));
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("[MirrorTest] Triggering mirror %s"), *Mirror->GetName());
    Mirror->TriggerScare(1.0f, 1.5f); 
}

void AGvTThiefCharacter::Server_SetDead_Implementation(AActor* Killer)
{
    if (bIsDead)
    {
        return;
    }

    bIsDead = true;

    // Server-authoritative lockdown
    SetInteractionLock(true, true);
    GetCharacterMovement()->DisableMovement();
    GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    OnRep_IsDead();
}

void AGvTThiefCharacter::OnRep_IsDead()
{
    if (!bIsDead)
    {
        return;
    }

    // Client-side effects: UI, camera, input disable, etc.
    if (APlayerController* PC = Cast<APlayerController>(GetController()))
    {
        DisableInput(PC);
        // TODO: show "caught" widget / spectator / respawn flow
    }
}