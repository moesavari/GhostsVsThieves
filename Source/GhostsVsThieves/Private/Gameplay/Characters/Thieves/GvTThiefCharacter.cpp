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
#include "Gameplay/Characters/Thieves/GvTThiefPerceptionComponent.h"
#include "DrawDebugHelpers.h"
#include "GvTPlayerState.h"
#include "Kismet/GameplayStatics.h"
#include "Gameplay/Inventory/GvTInventoryComponent.h"
#include "World/Items/GvTFlashlightItem.h"
#include "Components/SceneComponent.h"
#include "Gameplay/Ghosts/GvTHauntGhostBase.h"
#include "Gameplay/Ghosts/GvTGhostPerceptionComponent.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "GvTGameModeBase.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundAttenuation.h"

AGvTThiefCharacter::AGvTThiefCharacter()
{
    PrimaryActorTick.bCanEverTick = true;
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
        Move->GetNavAgentPropertiesRef().bCanCrouch = true;
        Move->MaxWalkSpeed = WalkSpeed;
    }

    NoiseEmitter = CreateDefaultSubobject<UGvTNoiseEmitterComponent>(TEXT("NoiseEmitter"));

    InteractionComponent = CreateDefaultSubobject<UGvTInteractionComponent>(TEXT("InteractionComponent"));

    InventoryComponent = CreateDefaultSubobject<UGvTInventoryComponent>(TEXT("InventoryComponent"));

    HeldItemAnchor = CreateDefaultSubobject<USceneComponent>(TEXT("HeldItemAnchor"));
    HeldItemAnchor->SetupAttachment(Camera);

    ThiefPerceptionComponent = CreateDefaultSubobject<UGvTThiefPerceptionComponent>(TEXT("ThiefPerception"));

    FootstepNoiseTag = FGameplayTag::RequestGameplayTag(TEXT("Noise.Footstep"));
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

void AGvTThiefCharacter::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (HasAuthority())
    {
        UpdateFootsteps(DeltaSeconds);
    }

#if !UE_BUILD_SHIPPING
    if (IsLocallyControlled() && bDebugHUDEnabled)
    {
        UpdateDebugHUD();
    }
#endif
}

void AGvTThiefCharacter::UpdateFootsteps(float DeltaSeconds)
{
    if (bIsDead || !GetCharacterMovement() || !GetCharacterMovement()->IsMovingOnGround())
    {
        FootstepTimeAccumulator = 0.f;
        return;
    }

    const float Speed2D = GetVelocity().Size2D();
    if (Speed2D < 15.f)
    {
        FootstepTimeAccumulator = 0.f;
        return;
    }

    const bool bCrouchedNow = bIsCrouched;
    const float Interval = bCrouchedNow ? CrouchFootstepInterval : (bIsSprinting ? SprintFootstepInterval : WalkFootstepInterval);
    FootstepTimeAccumulator += DeltaSeconds;
    if (FootstepTimeAccumulator < Interval)
    {
        return;
    }

    FootstepTimeAccumulator = FMath::Fmod(FootstepTimeAccumulator, FMath::Max(Interval, 0.05f));

    if (FootstepSounds.Num() > 0)
    {
        const int32 SoundIndex = FMath::RandRange(0, FootstepSounds.Num() - 1);
        USoundBase* SelectedSound = FootstepSounds[SoundIndex];
        const float Volume = bCrouchedNow ? 0.55f : (bIsSprinting ? 1.0f : 0.8f);
        const float Pitch = FMath::FRandRange(0.96f, 1.04f);
        Multicast_PlayFootstep(SelectedSound, Volume, Pitch);
    }

    if (NoiseEmitter && FootstepNoiseTag.IsValid())
    {
        const float Radius = bCrouchedNow ? CrouchFootstepNoiseRadius : (bIsSprinting ? SprintFootstepNoiseRadius : WalkFootstepNoiseRadius);
        NoiseEmitter->EmitNoise(FootstepNoiseTag, Radius, FootstepNoiseLoudness);
    }
}

void AGvTThiefCharacter::Multicast_PlayFootstep_Implementation(USoundBase* Sound, float Volume, float Pitch)
{
    if (Sound)
    {
        UGameplayStatics::PlaySoundAtLocation(this, Sound, GetActorLocation(), Volume, Pitch);
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

    if (IA_Jump)
    {
        EIC->BindAction(IA_Jump, ETriggerEvent::Started, this, &AGvTThiefCharacter::OnJumpStarted);
        EIC->BindAction(IA_Jump, ETriggerEvent::Completed, this, &AGvTThiefCharacter::OnJumpStopped);
        EIC->BindAction(IA_Jump, ETriggerEvent::Canceled, this, &AGvTThiefCharacter::OnJumpStopped);
    }

#if !UE_BUILD_SHIPPING
    if (IA_ToggleDebugHUD)
    {
        EIC->BindAction(IA_ToggleDebugHUD, ETriggerEvent::Started, this, &AGvTThiefCharacter::ToggleDebugHUD);
    }
#endif

    if (IA_TestNoise)
    {
        EIC->BindAction(IA_TestNoise, ETriggerEvent::Started, this, &AGvTThiefCharacter::TestNoise);
    }

    if (IA_Interact)
    {
        EIC->BindAction(IA_Interact, ETriggerEvent::Started, this, &AGvTThiefCharacter::OnInteractPressed);
    }

    if (IA_UseHeldItem)
    {
        EIC->BindAction(IA_UseHeldItem, ETriggerEvent::Started, this, &AGvTThiefCharacter::OnUseHeldItemPressed);
    }

    if (IA_Photo)
    {
        EIC->BindAction(IA_Photo, ETriggerEvent::Started, this, &AGvTThiefCharacter::OnPhotoPressed);
    }

    if (IA_TestMirror)
    {
        EIC->BindAction(IA_TestMirror, ETriggerEvent::Started, this, &AGvTThiefCharacter::OnTestMirrorPressed);
    }

    if (IA_InventoryNext)
    {
        EIC->BindAction(IA_InventoryNext, ETriggerEvent::Started, this, &AGvTThiefCharacter::OnInventoryNext);
    }

    if (IA_InventoryPrevious)
    {
        EIC->BindAction(IA_InventoryPrevious, ETriggerEvent::Started, this, &AGvTThiefCharacter::OnInventoryPrevious);
    }

    if (IA_DropItem)
    {
        EIC->BindAction(IA_DropItem, ETriggerEvent::Started, this, &AGvTThiefCharacter::OnDropItem);
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

void AGvTThiefCharacter::OnJumpStarted()
{
    if (bIsDead || bInteractionLockMove || IsScareStunned())
    {
        return;
    }

    if (bIsCrouched)
    {
        UnCrouch();
    }

    Jump();
}

void AGvTThiefCharacter::OnJumpStopped()
{
    StopJumping();
}

void AGvTThiefCharacter::ToggleDebugHUD()
{
#if !UE_BUILD_SHIPPING
    if (!IsLocallyControlled())
    {
        return;
    }

    bDebugHUDEnabled = !bDebugHUDEnabled;
    Server_SetDebugDrawEnabled(bDebugHUDEnabled);

    if (!bDebugHUDEnabled && GEngine)
    {
        GEngine->RemoveOnScreenDebugMessage(DebugHUDMessageKey);
    }
#endif
}

void AGvTThiefCharacter::Server_SetDebugDrawEnabled_Implementation(bool bEnabled)
{
#if !UE_BUILD_SHIPPING
    Multicast_SetDebugDrawEnabled(bEnabled);
#endif
}

void AGvTThiefCharacter::Multicast_SetDebugDrawEnabled_Implementation(bool bEnabled)
{
#if !UE_BUILD_SHIPPING
    ApplyDebugDrawState(bEnabled);
#endif
}

void AGvTThiefCharacter::ApplyDebugDrawState(bool bEnabled)
{
#if !UE_BUILD_SHIPPING
    if (!GetWorld())
    {
        return;
    }

    for (TActorIterator<AActor> It(GetWorld()); It; ++It)
    {
        TArray<UGvTGhostPerceptionComponent*> PerceptionComponents;
        It->GetComponents<UGvTGhostPerceptionComponent>(PerceptionComponents);
        for (UGvTGhostPerceptionComponent* Perception : PerceptionComponents)
        {
            if (Perception)
            {
                Perception->bDrawPerceptionDebug = bEnabled;
                Perception->bDrawDetectionRangesConstantly = bEnabled;
            }
        }

        TArray<UGvTNoiseEmitterComponent*> NoiseEmitters;
        It->GetComponents<UGvTNoiseEmitterComponent>(NoiseEmitters);
        for (UGvTNoiseEmitterComponent* Emitter : NoiseEmitters)
        {
            if (Emitter)
            {
                Emitter->bDrawDebug = bEnabled;
            }
        }
    }

    if (InteractionComponent)
    {
        InteractionComponent->SetDebugDraw(bEnabled);
    }
#endif
}

void AGvTThiefCharacter::UpdateDebugHUD()
{
#if !UE_BUILD_SHIPPING
    if (!GEngine || !GetWorld())
    {
        return;
    }

    const AGvTPlayerState* PS = GetPlayerState<AGvTPlayerState>();
    const UGvTDirectorSubsystem* Director = GetGameInstance() ? GetGameInstance()->GetSubsystem<UGvTDirectorSubsystem>() : nullptr;
    const UCharacterMovementComponent* Move = GetCharacterMovement();

    FString MovementState = TEXT("Walking");
    if (Move && !Move->IsMovingOnGround()) MovementState = TEXT("Airborne");
    else if (bIsCrouched) MovementState = TEXT("Crouched");
    else if (bIsSprinting) MovementState = TEXT("Sprinting");
    else if (GetVelocity().Size2D() < 15.f) MovementState = TEXT("Idle");

    const FString Text = FString::Printf(
        TEXT("HAUNTED HEISTS DEBUG\n")
        TEXT("Director: Activity %.0f%% | Tension %.0f%% | Haunt %s\n")
        TEXT("Activity Parts: Theft %.0f%% | Time %.0f%%\n")
        TEXT("Player: Panic %.0f%% | Pressure %.0f%% | %s\n")
        TEXT("Footsteps: Tag %s | Radius %.0f\n")
        TEXT("Debug Draw: ON"),
        Director ? Director->GetHouseActivity01() * 100.f : 0.f,
        Director ? Director->GetHouseTension01() * 100.f : 0.f,
        Director && Director->IsHauntActiveForDebug() ? TEXT("ACTIVE") : TEXT("None"),
        Director ? Director->GetTheftActivity01() * 100.f : 0.f,
        Director ? Director->GetTimeActivity01() * 100.f : 0.f,
        PS ? PS->GetPanic01() * 100.f : 0.f,
        PS ? PS->GetRecentHauntPressure01() * 100.f : 0.f,
        *MovementState,
        *FootstepNoiseTag.ToString(),
        bIsCrouched ? CrouchFootstepNoiseRadius : (bIsSprinting ? SprintFootstepNoiseRadius : WalkFootstepNoiseRadius));

    GEngine->AddOnScreenDebugMessage(DebugHUDMessageKey, 0.f, FColor::Cyan, Text, true, FVector2D(1.0f, 1.0f));
#endif
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

const TArray<TObjectPtr<USoundBase>>& AGvTThiefCharacter::GetFearReactionPool(EGvTFearReactionType ReactionType) const
{
    switch (ReactionType)
    {
    case EGvTFearReactionType::LightStartle:
        return LightStartleSounds;
    case EGvTFearReactionType::SevereFear:
        return SevereFearSounds;
    case EGvTFearReactionType::HauntStart:
        return HauntStartSounds;
    case EGvTFearReactionType::ModerateGasp:
    default:
        return ModerateGaspSounds;
    }
}

void AGvTThiefCharacter::PlayFearReactionAuthority(EGvTFearReactionType ReactionType)
{
    if (!HasAuthority() || bIsDead)
    {
        return;
    }

    const TArray<TObjectPtr<USoundBase>>& Pool = GetFearReactionPool(ReactionType);
    TArray<USoundBase*> ValidSounds;
    for (USoundBase* Sound : Pool)
    {
        if (IsValid(Sound))
        {
            ValidSounds.Add(Sound);
        }
    }

    if (ValidSounds.IsEmpty())
    {
        return;
    }

    USoundBase* SelectedSound = ValidSounds[FMath::RandRange(0, ValidSounds.Num() - 1)];
    const bool bIsHauntStart = ReactionType == EGvTFearReactionType::HauntStart;
    Client_PlayFearReactionLocal(SelectedSound, bIsHauntStart);

    // Every player receives their own haunt-start line locally. Suppressing its
    // teammate version prevents six spatial voices from stacking at once.
    Multicast_PlayFearReactionSpatial(SelectedSound, bIsHauntStart, bIsHauntStart);
}

void AGvTThiefCharacter::PlayFearReactionComponent(USoundBase* Sound, bool bSpatialized, bool bInterruptExisting)
{
    if (!Sound || bIsDead || GetNetMode() == NM_DedicatedServer)
    {
        return;
    }

    if (ActiveFearReactionAudio && ActiveFearReactionAudio->IsPlaying())
    {
        if (!bInterruptExisting)
        {
            return;
        }

        ActiveFearReactionAudio->Stop();
        ActiveFearReactionAudio = nullptr;
    }

    if (bSpatialized)
    {
        ActiveFearReactionAudio = UGameplayStatics::SpawnSoundAtLocation(this, Sound, GetActorLocation(), FRotator::ZeroRotator, FearReactionVolume, 1.0f, 0.0f, FearReactionAttenuation);
    }
    else
    {
        ActiveFearReactionAudio = UGameplayStatics::SpawnSound2D(this, Sound, FearReactionVolume);
    }
}

void AGvTThiefCharacter::Client_PlayFearReactionLocal_Implementation(USoundBase* Sound, bool bInterruptExisting)
{
    if (IsLocallyControlled())
    {
        PlayFearReactionComponent(Sound, false, bInterruptExisting);
    }
}

void AGvTThiefCharacter::Multicast_PlayFearReactionSpatial_Implementation(USoundBase* Sound, bool bSuppressSpatialPlayback, bool bInterruptExisting)
{
    if (bSuppressSpatialPlayback || IsLocallyControlled())
    {
        return;
    }

    PlayFearReactionComponent(Sound, true, bInterruptExisting);
}

void AGvTThiefCharacter::OnUseHeldItemPressed()
{
    if (!IsLocallyControlled() || bIsDead || IsScareStunned() || bInteractionLockMove || bInteractionLockLook || !InventoryComponent || !InteractionComponent)
    {
        return;
    }

    if (Cast<AGvTFlashlightItem>(InventoryComponent->GetSelectedItem()))
    {
        OnToggleFlashlight();
        return;
    }

    InteractionComponent->TryUseSelectedEquipment();
}

void AGvTThiefCharacter::OnPhotoPressed()
{
    if (!IsLocallyControlled() || !InteractionComponent)
        return;

    //InteractionComponent->TryPhoto();
    InteractionComponent->TryScan();
}

void AGvTThiefCharacter::OnInventoryNext()
{
    if (IsLocallyControlled() && InventoryComponent)
    {
        InventoryComponent->SelectNextItem();
    }
}

void AGvTThiefCharacter::OnInventoryPrevious()
{
    if (IsLocallyControlled() && InventoryComponent)
    {
        InventoryComponent->SelectPreviousItem();
    }
}

void AGvTThiefCharacter::OnDropItem()
{
    if (IsLocallyControlled() && InventoryComponent)
    {
        InventoryComponent->DropSelectedItem();
    }
}

void AGvTThiefCharacter::OnToggleFlashlight()
{
    if (!IsLocallyControlled() || bIsDead || bInteractionLockMove || bInteractionLockLook || !InventoryComponent)
    {
        return;
    }

    AGvTFlashlightItem* Flashlight = Cast<AGvTFlashlightItem>(InventoryComponent->GetSelectedItem());
    if (!Flashlight)
    {
        return;
    }

    Flashlight->ToggleFlashlight();
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

void AGvTThiefCharacter::ApplyScareStun(float Duration)
{
    UCharacterMovementComponent* MoveComp = GetCharacterMovement();
    if (!MoveComp)
    {
        return;
    }

    // Stuns are an on/off lock, not a stack.
    // Spamming a scare used to increment ScareStunCount while reusing one timer handle,
    // which could leave the player stuck/"dead" after the final timer only decremented once.
    const bool bWasAlreadyStunned = ScareStunCount > 0;
    ScareStunCount = 1;

    bInteractionLockMove = true;
    bInteractionLockLook = true;

    UE_LOG(LogTemp, Warning,
        TEXT("[Scare] ApplyScareStun %.2fs on %s Refresh=%d Count=%d ModeBefore=%d"),
        Duration,
        *GetName(),
        bWasAlreadyStunned ? 1 : 0,
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

    if (AGvTPlayerState* PS =
        GetPlayerState<AGvTPlayerState>())
    {
        PS->SetDeadForPanicAuthority(true);
    }

    if (InventoryComponent)
    {
        InventoryComponent->DropAllItemsOnDeath();
    }

    // Server-authoritative lockdown
    SetInteractionLock(true, true);
    GetCharacterMovement()->DisableMovement();
    GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    OnRep_IsDead();

    if (AGvTGameModeBase* GM = GetWorld()->GetAuthGameMode<AGvTGameModeBase>())
    {
        GM->NotifyThiefDied(this);
    }
}

void AGvTThiefCharacter::OnRep_IsDead()
{
    if (!bIsDead)
    {
        return;
    }

    if (ActiveFearReactionAudio)
    {
        ActiveFearReactionAudio->Stop();
        ActiveFearReactionAudio = nullptr;
    }

    // Client-side effects: UI, camera, input disable, etc.
    if (APlayerController* PC = Cast<APlayerController>(GetController()))
    {
        DisableInput(PC);
        // TODO: show "caught" widget / spectator / respawn flow
    }
}

void AGvTThiefCharacter::Debug_RequestGhostScare(FGameplayTag GhostScareTag)
{
    RequestGhostScare(GhostScareTag);
}

void AGvTThiefCharacter::Debug_RequestGhostEvent(FGameplayTag GhostEventTag)
{
    RequestGhostEvent(GhostEventTag);
}

void AGvTThiefCharacter::Debug_RequestGhostHaunt(FGameplayTag GhostHauntTag)
{
    RequestGhostHaunt(GhostHauntTag);
}

void AGvTThiefCharacter::RequestGhostScare(FGameplayTag GhostScareTag)
{
    if (!GhostScareTag.IsValid())
    {
        return;
    }

    if (!HasAuthority())
    {
        Server_RequestGhostScare(GhostScareTag);
        return;
    }

    Server_RequestGhostScare_Implementation(GhostScareTag);
}

void AGvTThiefCharacter::RequestGhostEvent(FGameplayTag GhostEventTag)
{
    if (GhostEventTag.MatchesTagExact(GvTScareTags::GhostEvent_Mirror()))
    {
        if (ThiefPerceptionComponent)
        {
            ThiefPerceptionComponent->Test_MirrorScare(1.f, 1.5f);
        }
        return;
    }

    if (!HasAuthority())
    {
        Server_RequestGhostEvent(GhostEventTag);
        return;
    }

    Server_RequestGhostEvent_Implementation(GhostEventTag);
}

void AGvTThiefCharacter::RequestGhostHaunt(FGameplayTag GhostHauntTag)
{
    if (!HasAuthority())
    {
        Server_RequestGhostHaunt(GhostHauntTag);
        return;
    }

    Server_RequestGhostHaunt_Implementation(GhostHauntTag);
}

void AGvTThiefCharacter::Client_PlayGhostScare_Implementation(FGameplayTag GhostScareTag)
{
    if (!IsLocallyControlled())
    {
        return;
    }

    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    if (!GhostScareTag.MatchesTagExact(GvTScareTags::GhostScare_Close()))
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[GhostScare] Ignored local model spawn for non-model scare tag=%s. Audio/scream scares route through ScareComponent."),
            *GhostScareTag.ToString());
        return;
    }

    if (IsScareStunned())
    {
        UE_LOG(LogTemp, Warning, TEXT("[GhostScare] Close scare ignored: target is already scare-stunned."));
        return;
    }

    if (UGvTScareComponent* ScareComp = FindComponentByClass<UGvTScareComponent>())
    {
        if (ScareComp->IsScareBusy())
        {
            UE_LOG(LogTemp, Warning, TEXT("[GhostScare] Close scare ignored: ScareComponent is busy."));
            return;
        }
    }

    if (IsValid(LocalActiveScareGhost))
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[GhostScare] Close scare ignored: local scare ghost is still active (%s)."),
            *GetNameSafe(LocalActiveScareGhost));
        return;
    }

    UE_LOG(LogTemp, Warning,
        TEXT("[GhostScare] Attempting local scare. Tag=%s DebugGhostClass=%s Local=%d Stunned=%d"),
        *GhostScareTag.ToString(),
        *GetNameSafe(DebugGhostClass),
        IsLocallyControlled() ? 1 : 0,
        IsScareStunned() ? 1 : 0);

    if (!DebugGhostClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("[GhostScare] Ghost presentation class is not set."));
        return;
    }

    for (TActorIterator<AGvTHauntGhostBase> It(World); It; ++It)
    {
        AGvTHauntGhostBase* ActiveHauntGhost = *It;
        if (!IsValid(ActiveHauntGhost) || ActiveHauntGhost->IsActorBeingDestroyed())
        {
            continue;
        }

        if (ActiveHauntGhost->GetClass() == DebugGhostClass.Get())
        {
            UE_LOG(LogTemp, Warning,
                TEXT("[GhostScare] Close scare blocked: presentation class %s matches active haunt ghost %s."),
                *GetNameSafe(DebugGhostClass),
                *GetNameSafe(ActiveHauntGhost));
            return;
        }
    }

    FVector SpawnLoc;
    FRotator SpawnRot;

    if (!TryFindSafeLocalGhostScareSpawn(SpawnLoc, SpawnRot))
    {
        UE_LOG(LogTemp, Warning, TEXT("[GhostScare] Could not find safe local scare spawn."));
        return;
    }

    FActorSpawnParameters Params;
    Params.Owner = this;
    Params.Instigator = this;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    AGvTGhostCharacterBase* Ghost = World->SpawnActor<AGvTGhostCharacterBase>(
        DebugGhostClass,
        SpawnLoc,
        SpawnRot,
        Params);

    if (!Ghost)
    {
        UE_LOG(LogTemp, Warning, TEXT("[GhostScare] Failed to spawn local ghost presentation."));
        return;
    }

    Ghost->SetReplicates(false);
    Ghost->SetReplicateMovement(false);
    LocalActiveScareGhost = Ghost;

    UE_LOG(LogTemp, Warning,
        TEXT("[GhostScare] Local target-only scare tag=%s Ghost=%s Target=%s"),
        *GhostScareTag.ToString(),
        *GetNameSafe(Ghost),
        *GetName());

    Ghost->BeginGhostScare(this, GhostScareTag);

    constexpr float LocalCloseScareCleanupDelay = 1.25f;
    TWeakObjectPtr<AGvTGhostCharacterBase> SpawnedGhost = Ghost;
    GetWorldTimerManager().SetTimer(
        TimerHandle_LocalCloseScareCleanup,
        FTimerDelegate::CreateWeakLambda(this, [this, SpawnedGhost]()
            {
                if (SpawnedGhost.IsValid())
                {
                    SpawnedGhost->Destroy();
                }

                if (LocalActiveScareGhost == SpawnedGhost.Get())
                {
                    LocalActiveScareGhost = nullptr;
                }
            }),
        LocalCloseScareCleanupDelay,
        false);
}

void AGvTThiefCharacter::Server_RequestGhostScare_Implementation(FGameplayTag GhostScareTag)
{
    if (!HasAuthority() || !GhostScareTag.IsValid())
    {
        return;
    }

    if (UWorld* World = GetWorld())
    {
        const float Now = World->GetTimeSeconds();
        if (DebugGhostScareRequestCooldown > 0.f &&
            Now - LastGhostScareRequestWorldTime < DebugGhostScareRequestCooldown)
        {
            UE_LOG(LogTemp, Warning,
                TEXT("[GhostScare] Request ignored by cooldown. Tag=%s Remaining=%.2f"),
                *GhostScareTag.ToString(),
                DebugGhostScareRequestCooldown - (Now - LastGhostScareRequestWorldTime));
            return;
        }

        LastGhostScareRequestWorldTime = Now;
    }

    if (GhostScareTag.MatchesTagExact(GvTScareTags::GhostScare_Close()) && IsScareStunned())
    {
        UE_LOG(LogTemp, Warning, TEXT("[GhostScare] Request ignored: target is already scare-stunned."));
        return;
    }

    if (UGameInstance* GI = GetGameInstance())
    {
        if (UGvTDirectorSubsystem* Director =
            GI->GetSubsystem<UGvTDirectorSubsystem>())
        {
            Director->DispatchScareEventSimple(
                GhostScareTag,
                this,
                this,
                true);

            return;
        }
    }

    UE_LOG(LogTemp, Warning,
        TEXT("[GhostScare] Failed to route tag=%s: DirectorSubsystem unavailable."),
        *GhostScareTag.ToString());
}

void AGvTThiefCharacter::Server_RequestGhostHaunt_Implementation(FGameplayTag GhostHauntTag)
{
    if (!HasAuthority())
    {
        return;
    }

    if (IsValid(DebugActiveGhost))
    {
        DebugActiveGhost->Destroy();
        DebugActiveGhost = nullptr;
    }

    if (UGameInstance* GI = GetGameInstance())
    {
        if (UGvTDirectorSubsystem* Director = GI->GetSubsystem<UGvTDirectorSubsystem>())
        {
            DebugActiveGhost = Director->SpawnHauntGhostForTarget(this, GhostHauntTag, DebugGhostClass);
            return;
        }
    }

    UE_LOG(LogTemp, Warning,
        TEXT("[GhostHaunt] Failed to route tag=%s: DirectorSubsystem unavailable."),
        *GhostHauntTag.ToString());
}

void AGvTThiefCharacter::Server_RequestGhostEvent_Implementation(FGameplayTag GhostEventTag)
{
    if (GhostEventTag.MatchesTagExact(GvTScareTags::GhostEvent_DoorSlam()))
    {
        if (UGameInstance* GI = GetGameInstance())
        {
            if (UGvTDirectorSubsystem* Director = GI->GetSubsystem<UGvTDirectorSubsystem>())
            {
                AGvTDoorActor* Door = Cast<AGvTDoorActor>(Director->FindBestDoorSlamDoor(this));
                if (!Door)
                {
                    UE_LOG(LogTemp, Warning,
                        TEXT("[GhostEvent] DoorSlam failed: no valid door for %s"),
                        *GetName());
                    return;
                }

                const FGvTScareEvent Event = Director->MakeDoorSlamBehindEvent(this, Door);

                UE_LOG(LogTemp, Warning,
                    TEXT("[GhostEvent] DoorSlam requested for %s using %s"),
                    *GetName(),
                    *GetNameSafe(Door));

                Director->DispatchScareEvent(Event);
            }
        }

        return;
    }

    UE_LOG(LogTemp, Warning,
        TEXT("[GhostEvent] Unsupported GhostEvent tag=%s"),
        *GhostEventTag.ToString());
}

void AGvTThiefCharacter::Client_PlayGhostEvent_Implementation(FGameplayTag GhostEventTag)
{
    if (!IsLocallyControlled())
    {
        return;
    }

    if (GhostEventTag.MatchesTagExact(GvTScareTags::GhostEvent_Mirror()))
    {
        if (ThiefPerceptionComponent)
        {
            ThiefPerceptionComponent->Test_MirrorScare(1.f, 1.5f);
        }
    }
}

bool AGvTThiefCharacter::TryFindSafeLocalGhostScareSpawn(FVector& OutLocation, FRotator& OutRotation) const
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return false;
    }

    const FVector PlayerLoc = GetActorLocation();
    const FVector Forward = GetActorForwardVector();
    const FVector Right = GetActorRightVector();

    const TArray<FVector> CandidateOffsets =
    {
        -Forward * 300.f,
        Right * 280.f,
        -Right * 280.f,
        (-Forward + Right).GetSafeNormal() * 330.f,
        (-Forward - Right).GetSafeNormal() * 330.f,
        Forward * 320.f
    };

    FCollisionQueryParams Params(SCENE_QUERY_STAT(GhostScareSpawn), false, this);

    for (const FVector& Offset : CandidateOffsets)
    {
        const FVector Candidate = PlayerLoc + Offset;

        FHitResult FloorHit;
        const FVector TraceStart = Candidate + FVector(0.f, 0.f, 250.f);
        const FVector TraceEnd = Candidate - FVector(0.f, 0.f, 700.f);

        if (!World->LineTraceSingleByChannel(FloorHit, TraceStart, TraceEnd, ECC_WorldStatic, Params))
        {
            continue;
        }

        const FVector SpawnLoc = FloorHit.ImpactPoint + FVector(0.f, 0.f, 96.f);

        const FCollisionShape GhostShape = FCollisionShape::MakeCapsule(34.f, 88.f);

        const bool bBlockedByWorld = World->OverlapBlockingTestByChannel(
            SpawnLoc,
            FQuat::Identity,
            ECC_WorldStatic,
            GhostShape,
            Params);

        if (bBlockedByWorld)
        {
            continue;
        }

        OutLocation = SpawnLoc;

        FVector LookDir = PlayerLoc - SpawnLoc;
        LookDir.Z = 0.f;

        OutRotation = LookDir.IsNearlyZero()
            ? GetActorRotation()
            : LookDir.Rotation();

        return true;
    }

    // Emergency fallback: do NOT kill the scare entirely.
    OutLocation = PlayerLoc - Forward * 280.f + FVector(0.f, 0.f, 90.f);

    FVector LookDir = PlayerLoc - OutLocation;
    LookDir.Z = 0.f;

    OutRotation = LookDir.IsNearlyZero()
        ? GetActorRotation()
        : LookDir.Rotation();

    UE_LOG(LogTemp, Warning, TEXT("[GhostScare] Safe spawn failed; using emergency fallback behind player."));

    return true;
}
