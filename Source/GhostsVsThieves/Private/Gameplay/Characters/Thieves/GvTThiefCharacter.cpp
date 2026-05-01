#include "Gameplay/Characters/Thieves/GvTThiefCharacter.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/PlayerController.h"
#include "Systems/Noise/GvTNoiseEmitterComponent.h"
#include "GameplayTagContainer.h"
#include "Gameplay/Interaction/GvTInteractionComponent.h"
#include "Gameplay/Ghosts/Mirror/GvTMirrorActor.h"
#include "Camera/PlayerCameraManager.h"
#include "Net/UnrealNetwork.h"
#include "Components/CapsuleComponent.h"
#include "Systems/Director/GvTDirectorSubsystem.h"
#include "Gameplay/Scare/GvTScareTypes.h"
#include "Gameplay/Scare/GvTScareTags.h"
#include "Engine/GameInstance.h"
#include "TimerManager.h"
#include "Gameplay/Scare/UGvTScareComponent.h"
#include "World/Doors/GvTDoorActor.h"
#include "Gameplay/Ghosts/GvTGhostCharacterBase.h"

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

void AGvTThiefCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AGvTThiefCharacter, bIsSprinting);
    DOREPLIFETIME(AGvTThiefCharacter, bInteractionLockMove);
    DOREPLIFETIME(AGvTThiefCharacter, bInteractionLockLook);
    DOREPLIFETIME(AGvTThiefCharacter, bIsDead);
}

void AGvTThiefCharacter::OnMove(const FInputActionValue& Value)
{
    if (bInteractionLockMove || IsScareStunned()) { return; }

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
    if (bInteractionLockLook || IsScareStunned()) { return; }

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

void AGvTThiefCharacter::ServerSetSprinting_Implementation(bool bNewSprinting)
{
    bIsSprinting = bNewSprinting;

    if (UCharacterMovementComponent* Move = GetCharacterMovement())
    {
        Move->MaxWalkSpeed = bIsSprinting ? SprintSpeed : WalkSpeed;
    }
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

void AGvTThiefCharacter::Client_PlayLocalScareStun_Implementation(float Duration)
{
    if (!IsLocallyControlled())
    {
        return;
    }

    if (GetNetMode() != NM_Client)
    {
        return;
    }

    if (GetLocalRole() != ROLE_AutonomousProxy)
    {
        return;
    }

    ApplyScareStun(Duration);
}

void AGvTThiefCharacter::Client_PlayLocalGhostScare_Implementation(const FGvTScareEvent& Event)
{
    UE_LOG(LogTemp, Warning, TEXT("[GhostScareTest] Client RPC reached %s local=%d tag=%s class=%s"),
        *GetNameSafe(this),
        IsLocallyControlled() ? 1 : 0,
        *Event.ScareTag.ToString(),
        *GetNameSafe(LocalHauntGhostClass));

    UE_LOG(LogTemp, Warning,
        TEXT("[ScareRPC] Client_PlayLocalGhostScare this=%s local=%d incomingTarget=%s"),
        *GetNameSafe(this),
        IsLocallyControlled() ? 1 : 0,
        *GetNameSafe(Event.TargetActor));

    if (!IsLocallyControlled())
    {
        UE_LOG(LogTemp, Warning, TEXT("[Scare] Client_PlayLocalGhostScare ignored: %s is not locally controlled"), *GetName());
        return;
    }

    UGvTScareComponent* ScareComp = FindComponentByClass<UGvTScareComponent>();
    if (!ScareComp)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Scare] Client_PlayLocalGhostScare failed: %s has no scare component"), *GetName());
        return;
    }

    FGvTScareEvent LocalEvent = Event;
    LocalEvent.TargetActor = this; // force victim identity from the RPC receiver

    PlayLocalHauntGhostScare(LocalEvent);
}

void AGvTThiefCharacter::ApplyScareStun(float Duration)
{
    UCharacterMovementComponent* MoveComp = GetCharacterMovement();
    if (!MoveComp)
    {
        return;
    }

    ScareStunCount++;

    bInteractionLockMove = true;
    bInteractionLockLook = true;

    UE_LOG(LogTemp, Warning,
        TEXT("[Scare] ApplyScareStun %.2fs on %s Count=%d ModeBefore=%d"),
        Duration,
        *GetName(),
        ScareStunCount,
        (int32)MoveComp->MovementMode);

    MoveComp->StopMovementImmediately();
    MoveComp->DisableMovement();

    const int32 ScheduledCount = ScareStunCount;

    FTimerDelegate ClearDelegate;
    ClearDelegate.BindLambda([this, ScheduledCount]()
        {
            ClearScareStun(ScheduledCount);
        });

    GetWorldTimerManager().SetTimer(
        TimerHandle_ClearScareStun,
        ClearDelegate,
        FMath::Max(0.01f, Duration),
        false);
}

void AGvTThiefCharacter::ClearScareStun()
{
    ClearScareStun(ScareStunCount);
}

void AGvTThiefCharacter::ClearScareStun(int32 ClearCountAtScheduleTime)
{
    if (ScareStunCount <= 0)
    {
        return;
    }

    if (ClearCountAtScheduleTime != ScareStunCount)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[Scare] ClearScareStun ignored on %s ScheduledCount=%d CurrentCount=%d"),
            *GetName(),
            ClearCountAtScheduleTime,
            ScareStunCount);
        return;
    }

    ScareStunCount = FMath::Max(0, ScareStunCount - 1);

    if (ScareStunCount == 0)
    {
        if (UCharacterMovementComponent* Move = GetCharacterMovement())
        {
            UE_LOG(LogTemp, Warning,
                TEXT("[Scare] ClearScareStun restoring movement on %s ModeBefore=%d"),
                *GetName(),
                (int32)Move->MovementMode);

            Move->SetMovementMode(MOVE_Walking);

            UE_LOG(LogTemp, Warning,
                TEXT("[Scare] ClearScareStun restored movement on %s ModeAfter=%d"),
                *GetName(),
                (int32)Move->MovementMode);
        }

        bInteractionLockMove = false;
        bInteractionLockLook = false;

        UE_LOG(LogTemp, Warning, TEXT("[Scare] ClearScareStun complete on %s"), *GetName());
    }
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

void AGvTThiefCharacter::Debug_RequestMirrorScare()
{
    OnTestMirrorPressed();
}

void AGvTThiefCharacter::Debug_RequestRearAudioStingScare()
{
    if (HasAuthority())
    {
        Server_DebugRequestRearAudioStingScare_Implementation();
        return;
    }

    Server_DebugRequestRearAudioStingScare();
}

void AGvTThiefCharacter::Debug_RequestGhostScreamScare()
{
    if (HasAuthority())
    {
        Server_DebugRequestGhostScreamScare_Implementation();
        return;
    }

    Server_DebugRequestGhostScreamScare();
}

void AGvTThiefCharacter::Debug_RequestDoorSlamBehindScare()
{
    if (HasAuthority())
    {
        Server_DebugRequestDoorSlamBehindScare_Implementation();
        return;
    }

    Server_DebugRequestDoorSlamBehindScare();
}

void AGvTThiefCharacter::Debug_RequestLightChaseScare()
{
    if (HasAuthority())
    {
        Server_DebugRequestLightChaseScare_Implementation();
        return;
    }

    Server_DebugRequestLightChaseScare();
}

void AGvTThiefCharacter::Server_DebugRequestMirrorScare_Implementation()
{
    if (UGameInstance* GI = GetGameInstance())
    {
        if (UGvTDirectorSubsystem* Director = GI->GetSubsystem<UGvTDirectorSubsystem>())
        {
            const FGvTScareEvent Event = Director->MakeMirrorEvent(this);

            UE_LOG(LogTemp, Warning, TEXT("[Debug] Server Mirror scare requested for %s"), *GetName());
            Director->DispatchScareEvent(Event);
        }
    }
}

void AGvTThiefCharacter::Server_DebugRequestRearAudioStingScare_Implementation()
{
    if (UGameInstance* GI = GetGameInstance())
    {
        if (UGvTDirectorSubsystem* Director = GI->GetSubsystem<UGvTDirectorSubsystem>())
        {
            const FGvTScareEvent Event = Director->MakeRearAudioStingEvent(this);

            UE_LOG(LogTemp, Warning, TEXT("[Debug] Server RearAudioSting scare requested for %s"), *GetName());
            Director->DispatchScareEvent(Event);
        }
    }
}

void AGvTThiefCharacter::Server_DebugRequestGhostScreamScare_Implementation()
{
    if (UGameInstance* GI = GetGameInstance())
    {
        if (UGvTDirectorSubsystem* Director = GI->GetSubsystem<UGvTDirectorSubsystem>())
        {
            const FGvTScareEvent Event = Director->MakeGhostScreamEvent(this);

            UE_LOG(LogTemp, Warning, TEXT("[Debug] Server GhostScream scare requested for %s"), *GetName());
            Director->DispatchScareEvent(Event);
        }
    }
}

void AGvTThiefCharacter::Server_DebugRequestDoorSlamBehindScare_Implementation()
{
    if (UGameInstance* GI = GetGameInstance())
    {
        if (UGvTDirectorSubsystem* Director = GI->GetSubsystem<UGvTDirectorSubsystem>())
        {
            AGvTDoorActor* Door = Cast<AGvTDoorActor>(Director->FindBestDoorSlamDoor(this));
            if (!Door)
            {
                UE_LOG(LogTemp, Warning, TEXT("[Debug] DoorSlamBehind failed: no valid open door for %s"), *GetName());
                return;
            }

            const FGvTScareEvent Event = Director->MakeDoorSlamBehindEvent(this, Door);

            UE_LOG(LogTemp, Warning,
                TEXT("[Debug] Server DoorSlamBehind scare requested for %s using %s"),
                *GetName(),
                *GetNameSafe(Door));

            Director->DispatchScareEvent(Event);
        }
    }
}

void AGvTThiefCharacter::Server_DebugRequestLightChaseScare_Implementation()
{
    if (UGameInstance* GI = GetGameInstance())
    {
        if (UGvTDirectorSubsystem* Director = GI->GetSubsystem<UGvTDirectorSubsystem>())
        {
            const FGvTScareEvent Event = Director->MakeLightChaseEvent(this);

            UE_LOG(LogTemp, Warning, TEXT("[Debug] Server LightChase scare requested for %s"), *GetName());
            Director->DispatchScareEvent(Event);
        }
    }
}

void AGvTThiefCharacter::OnTestMirrorPressed()
{
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

    UE_LOG(LogTemp, Warning, TEXT("[MirrorTest] Requesting mirror payload on %s"), *Mirror->GetName());

    if (HasAuthority())
    {
        Server_DebugTriggerMirrorActor_Implementation(Mirror);
    }
    else
    {
        Server_DebugTriggerMirrorActor(Mirror);
    }
}

void AGvTThiefCharacter::Server_DebugTriggerMirrorActor_Implementation(AGvTMirrorActor* Mirror)
{
    if (!Mirror)
    {
        return;
    }

    if (UGameInstance* GI = GetGameInstance())
    {
        if (UGvTDirectorSubsystem* Director = GI->GetSubsystem<UGvTDirectorSubsystem>())
        {
            FGvTScareEvent Event = Director->MakeMirrorEvent(this);
            Event.SourceActor = Mirror;
            Event.WorldHint = Mirror->GetActorLocation();

            UE_LOG(LogTemp, Warning,
                TEXT("[Debug] Server mirror payload requested for %s using %s"),
                *GetName(),
                *GetNameSafe(Mirror));

            Director->DispatchScareEvent(Event);
        }
    }
}

void AGvTThiefCharacter::Debug_RequestGhostChaseScare()
{
    if (HasAuthority())
    {
        Server_DebugRequestGhostChaseScare_Implementation();
        return;
    }

    Server_DebugRequestGhostChaseScare();
}

void AGvTThiefCharacter::Debug_RequestGhostScare()
{
    if (HasAuthority())
    {
        Server_DebugRequestGhostScare_Implementation();
        return;
    }

    Server_DebugRequestGhostScare();
}

void AGvTThiefCharacter::Server_DebugRequestGhostChaseScare_Implementation()
{
    if (UGameInstance* GI = GetGameInstance())
    {
        if (UGvTDirectorSubsystem* Director = GI->GetSubsystem<UGvTDirectorSubsystem>())
        {
            const FGvTScareEvent Event = Director->MakeGhostChaseEvent(this);
            Director->DispatchScareEvent(Event);
        }
    }
}

void AGvTThiefCharacter::Server_DebugRequestGhostScare_Implementation()
{
    if (UGameInstance* GI = GetGameInstance())
    {
        if (UGvTDirectorSubsystem* Director = GI->GetSubsystem<UGvTDirectorSubsystem>())
        {
            const FGvTScareEvent Event = Director->MakeGhostScareEvent(this);
            Director->DispatchScareEvent(Event);
        }
    }
}

void AGvTThiefCharacter::PlayLocalHauntGhostScare(const FGvTScareEvent& Event)
{
    UGvTScareComponent* ScareComp = FindComponentByClass<UGvTScareComponent>();
    if (!ScareComp)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Scare] Local haunt scare failed: %s has no scare component"), *GetName());
        return;
    }

    if (!ScareComp->TryBeginLocalHauntGhostScare(Event))
    {
        UE_LOG(LogTemp, Warning, TEXT("[GhostScareFix] Force clearing stuck scare state"));
        ScareComp->EndScareLifecycle();

        return;
    }

    if (!LocalHauntGhostClass)
    {
        UE_LOG(LogTemp, Error, TEXT("[Scare] Local haunt scare failed: LocalHauntGhostClass is not assigned on %s"), *GetName());
        return;
    }

    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = this;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    TSubclassOf<AGvTGhostCharacterBase> SpawnClass = LocalHauntGhostClass;

    if (UGameInstance* GI = GetGameInstance())
    {
        if (UGvTDirectorSubsystem* Director = GI->GetSubsystem<UGvTDirectorSubsystem>())
        {
            if (AGvTGhostCharacterBase* PrimaryGhost = Director->GetPrimaryGhost())
            {
                SpawnClass = PrimaryGhost->GetClass();
            }
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("[GhostScareTest] Spawning local ghost class=%s (override=%s)"),
        *GetNameSafe(SpawnClass),
        *GetNameSafe(LocalHauntGhostClass));

    AGvTGhostCharacterBase* LocalGhost = World->SpawnActor<AGvTGhostCharacterBase>(
        SpawnClass,
        GetActorLocation(),
        FRotator::ZeroRotator,
        SpawnParams);;

    if (!LocalGhost)
    {
        UE_LOG(LogTemp, Error, TEXT("[Scare] Local haunt scare failed: could not spawn local ghost."));
        return;
    }

    //LocalGhost->SetReplicates(false);
    //LocalGhost->SetReplicateMovement(false);

    if (UGameInstance* GI = GetGameInstance())
    {
        if (UGvTDirectorSubsystem* Director = GI->GetSubsystem<UGvTDirectorSubsystem>())
        {
            if (AGvTGhostCharacterBase* PrimaryGhost = Director->GetPrimaryGhost())
            {
                LocalGhost->InitializeGhost(
                    TEXT("LocalGhostScare"),
                    PrimaryGhost->GetGhostModelData(),
                    PrimaryGhost->GetGhostTypeData(),
                    PrimaryGhost->GetRuntimeConfig());
            }
        }
    }

    LocalGhost->BeginHauntGhostScare(this, Event.ScareTag);
}