#include "World/Doors/GvTDoorActor.h"

#include "Net/UnrealNetwork.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Systems/Noise/GvTNoiseEmitterComponent.h"
#include "GameFramework/GameStateBase.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"

AGvTDoorActor::AGvTDoorActor()
{
	bReplicates = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Hinge = CreateDefaultSubobject<USceneComponent>(TEXT("Hinge"));
	Hinge->SetupAttachment(Root);

	DoorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DoorMesh"));
	DoorMesh->SetupAttachment(Hinge);
	DoorMesh->SetMobility(EComponentMobility::Movable);

	DoorNoiseTag = FGameplayTag::RequestGameplayTag(TEXT("Noise.Interact"));

	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	DoorNoiseEmitter = CreateDefaultSubobject<UGvTNoiseEmitterComponent>(TEXT("DoorNoiseEmitter"));
}

void AGvTDoorActor::BeginPlay()
{
	Super::BeginPlay();
	ApplyDoorState(bIsOpen);
}

void AGvTDoorActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!bAnimating)
		return;

	const AGameStateBase* GS = GetWorld()->GetGameState();
	const float Now = GS ? GS->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();

	const float Elapsed = Now - DoorAnimStartServerTime;
	const float Alpha = FMath::Clamp(Elapsed / FMath::Max(OpenDuration, 0.01f), 0.f, 1.f);

	const float SmoothAlpha = FMath::SmoothStep(0.f, 1.f, Alpha);
	const float NewYaw = FMath::Lerp(AnimFromYaw, AnimToYaw, SmoothAlpha);

	Hinge->SetRelativeRotation(FRotator(0.f, NewYaw, 0.f));

	if (Alpha >= 1.f)
	{
		bAnimating = false;
		SetActorTickEnabled(false);
	}
}

void AGvTDoorActor::GetInteractionSpec_Implementation(APawn* InstigatorPawn, EGvTInteractionVerb Verb, FGvTInteractionSpec& OutSpec) const
{
	OutSpec = FGvTInteractionSpec{};
	OutSpec.CastTime = 0.f;
	OutSpec.bLockMovement = false;
	OutSpec.bLockLook = false;
	OutSpec.bCancelable = false;
	OutSpec.bEmitNoiseOnCancel = false;
	OutSpec.CancelNoiseRadius = 0.f;
	OutSpec.CancelNoiseLoudness = 0.f;
	OutSpec.InteractionTag = DoorNoiseTag;
}

bool AGvTDoorActor::CanInteract_Implementation(APawn* InstigatorPawn, EGvTInteractionVerb Verb) const
{
	return Verb == EGvTInteractionVerb::Interact;
}

void AGvTDoorActor::BeginInteract_Implementation(APawn* InstigatorPawn, EGvTInteractionVerb Verb)
{
	// No-op for instant door.
}

void AGvTDoorActor::CompleteInteract_Implementation(APawn* InstigatorPawn, EGvTInteractionVerb Verb)
{
	if (!HasAuthority())
		return;

	if (Verb != EGvTInteractionVerb::Interact)
		return;

	// If locked: rattle SFX + small noise, do not toggle.
	if (bIsLocked)
	{
		const bool bUnlocked = TryUnlock(InstigatorPawn, EDoorUnlockMethod::Key, /*bAutoOpenOnSuccess=*/true);
		if (!bUnlocked)
		{
			Multicast_PlaySFX(SFX_LockedRattle);
			if (DoorNoiseEmitter)
			{
				DoorNoiseEmitter->EmitNoise(DoorNoiseTag, 500.f, 1.0f);
			}
		}
		return;
	}

	// Clear any pending close-end thunk
	GetWorldTimerManager().ClearTimer(TimerHandle_CloseEnd);

	// Toggle open/close
	bIsOpen = !bIsOpen;
	DoorAnimStartServerTime = GetWorld()->GetTimeSeconds();
	StartDoorAnim(bIsOpen);

	// Start SFX (server multicasts ONCE)
	Multicast_PlaySFX(bIsOpen ? SFX_OpenStart : SFX_CloseStart);

	// Start noise (server emits ONCE)
	if (DoorNoiseEmitter)
	{
		DoorNoiseEmitter->EmitNoise(DoorNoiseTag, DoorNoiseRadius, 1.0f);
	}

	if (!bIsOpen)
	{
		GetWorldTimerManager().SetTimer(
			TimerHandle_CloseEnd,
			this,
			&AGvTDoorActor::Server_DoCloseEnd,
			FMath::Max(OpenDuration, 0.01f),
			false
		);
	}
}

void AGvTDoorActor::CancelInteract_Implementation(APawn* InstigatorPawn, EGvTInteractionVerb Verb, EGvTInteractionCancelReason Reason)
{
	// No-op
}

void AGvTDoorActor::Server_DoCloseEnd()
{
	// Server-only, one-shot: thunk SFX + end noise
	Multicast_PlaySFX(SFX_CloseEnd);

	if (DoorNoiseEmitter)
	{
		const float EndRadius = (CloseEndNoiseRadius > 0.f) ? CloseEndNoiseRadius : DoorNoiseRadius;
		DoorNoiseEmitter->EmitNoise(DoorNoiseTag, EndRadius, 1.0f);
	}
}

void AGvTDoorActor::Multicast_PlaySFX_Implementation(USoundBase* Sound)
{
	if (!Sound) return;
	UGameplayStatics::PlaySoundAtLocation(this, Sound, GetActorLocation());
}

void AGvTDoorActor::OnRep_DoorAnimStart()
{
	StartDoorAnim(bIsOpen);
}

void AGvTDoorActor::OnRep_IsOpen()
{

}

void AGvTDoorActor::ApplyDoorState(bool bOpen)
{
	const float SignedOpen = bInvertDirection ? -OpenYaw : OpenYaw;
	const float TargetYaw = bOpen ? SignedOpen : ClosedYaw;
	Hinge->SetRelativeRotation(FRotator(0.f, TargetYaw, 0.f));
}

void AGvTDoorActor::StartDoorAnim(bool bOpen)
{
	const float SignedOpen = bInvertDirection ? -OpenYaw : OpenYaw;
	const float TargetYaw = bOpen ? SignedOpen : ClosedYaw;

	AnimFromYaw = Hinge->GetRelativeRotation().Yaw;
	AnimToYaw = TargetYaw;

	bAnimating = true;
	SetActorTickEnabled(true);
}

bool AGvTDoorActor::TryUnlock(APawn* InstigatorPawn, EDoorUnlockMethod Method, bool bAutoOpenOnSuccess)
{
	if (!HasAuthority())
		return false;

	if (!bIsLocked)
		return true;

	// MVP gating:
	// - Force always succeeds (debug / future brute force)
	// - Others fail for now until inventory/skill exists
	const bool bSuccess = (Method == EDoorUnlockMethod::Force);

	if (!bSuccess)
	{
		// failure feedback (optional)
		Multicast_PlaySFX(SFX_LockedRattle);
		if (DoorNoiseEmitter)
		{
			DoorNoiseEmitter->EmitNoise(DoorNoiseTag, 700.f, 1.0f);
		}
		return false;
	}

	// Success: unlock
	bIsLocked = false;

	Multicast_PlaySFX(SFX_Unlock);
	if (DoorNoiseEmitter)
	{
		DoorNoiseEmitter->EmitNoise(DoorNoiseTag, 600.f, 1.0f);
	}

	if (bAutoOpenOnSuccess && !bIsOpen)
	{
		// Open it immediately
		GetWorldTimerManager().ClearTimer(TimerHandle_CloseEnd);

		bIsOpen = true;
		DoorAnimStartServerTime = GetWorld()->GetTimeSeconds();
		StartDoorAnim(true);

		Multicast_PlaySFX(SFX_OpenStart);
		if (DoorNoiseEmitter)
		{
			DoorNoiseEmitter->EmitNoise(DoorNoiseTag, DoorNoiseRadius, 1.0f);
		}
	}

	return true;
}

void AGvTDoorActor::SetLocked(bool bNewLocked)
{
	if (!HasAuthority())
		return;

	if (bIsLocked == bNewLocked)
		return;

	bIsLocked = bNewLocked;

	// Lock/unlock SFX + small noise
	Multicast_PlaySFX(bIsLocked ? SFX_Lock : SFX_Unlock);

	if (DoorNoiseEmitter)
	{
		DoorNoiseEmitter->EmitNoise(DoorNoiseTag, 600.f, 1.0f);
	}
}

void AGvTDoorActor::OnRep_IsLocked()
{
	// Optional visuals.
	// Audio is multicast from the server when the state changes.
}

void AGvTDoorActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AGvTDoorActor, bIsOpen);
	DOREPLIFETIME(AGvTDoorActor, DoorAnimStartServerTime);
	DOREPLIFETIME(AGvTDoorActor, bIsLocked);
}

void AGvTDoorActor::Debug_ToggleLock()
{
	if (!HasAuthority())
		return;

	SetLocked(!bIsLocked);
}
