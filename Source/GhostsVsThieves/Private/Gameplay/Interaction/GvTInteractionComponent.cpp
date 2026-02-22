#include "Gameplay/Interaction/GvTInteractionComponent.h"

#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"

#include "Gameplay/Interaction/GvTInteractable.h"
#include "Gameplay/Characters/Thieves/GvTThiefCharacter.h"
#include "Systems/Noise/GvTNoiseSubsystem.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"

UGvTInteractionComponent::UGvTInteractionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UGvTInteractionComponent::TryInteract()
{
	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!OwnerPawn || !OwnerPawn->IsLocallyControlled())
	{
		return;
	}

	// Pressing again while interacting cancels (MVP friendly)
	if (bIsInteracting)
	{
		TryCancelInteraction(EGvTInteractionCancelReason::UserCanceled);
		return;
	}

	Server_TryInteract(EGvTInteractionVerb::Interact);
}

void UGvTInteractionComponent::TryPhoto()
{
	//APawn* OwnerPawn = Cast<APawn>(GetOwner());
	//if (!OwnerPawn || !OwnerPawn->IsLocallyControlled())
	//{
	//	return;
	//}

	//if (bIsInteracting)
	//{
	//	TryCancelInteraction(EGvTInteractionCancelReason::UserCanceled);
	//	return;
	//}

	//Server_TryInteract(EGvTInteractionVerb::Photo);

	TryScan();
}

void UGvTInteractionComponent::TryScan()
{
	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!OwnerPawn || !OwnerPawn->IsLocallyControlled())
		return;

	if (bIsInteracting)
	{
		TryCancelInteraction(EGvTInteractionCancelReason::UserCanceled);
		return;
	}

	Server_TryInteract(EGvTInteractionVerb::Scan);
}

bool UGvTInteractionComponent::IsScanning() const
{
	return bIsInteracting && ActiveVerb == EGvTInteractionVerb::Scan;
}

void UGvTInteractionComponent::TryCancelInteraction(EGvTInteractionCancelReason Reason)
{
	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!OwnerPawn || !OwnerPawn->IsLocallyControlled())
	{
		return;
	}

	if (!bIsInteracting)
	{
		return;
	}

	Server_CancelInteraction(Reason);
}

void UGvTInteractionComponent::Server_TryInteract_Implementation(EGvTInteractionVerb Verb)
{
	PerformServerTraceAndTryStart(Verb);
}

void UGvTInteractionComponent::Server_CancelInteraction_Implementation(EGvTInteractionCancelReason Reason)
{
	if (!bIsInteracting)
	{
		return;
	}

	CancelInteractionInternal(Reason);
}

bool UGvTInteractionComponent::GetViewTrace(FVector& OutStart, FVector& OutEnd) const
{
	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!OwnerPawn)
	{
		return false;
	}

	AController* Controller = OwnerPawn->GetController();
	APlayerController* PC = Cast<APlayerController>(Controller);
	if (!PC)
	{
		return false;
	}

	FVector ViewLoc;
	FRotator ViewRot;
	PC->GetPlayerViewPoint(ViewLoc, ViewRot);

	OutStart = ViewLoc;
	OutEnd = ViewLoc + (ViewRot.Vector() * TraceDistance);
	return true;
}

void UGvTInteractionComponent::PerformServerTraceAndTryStart(EGvTInteractionVerb Verb)
{
	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!OwnerPawn || bIsInteracting)
	{
		return;
	}

	if (Verb == EGvTInteractionVerb::Scan)
	{
		const float Now = GetWorld()->GetTimeSeconds();
		if (Now - LastScanServerTime < ScanAttemptCooldownSeconds)
			return;

		LastScanServerTime = Now;
	}

	FVector Start, End;
	if (!GetViewTrace(Start, End))
	{
		return;
	}

	FCollisionQueryParams Params(SCENE_QUERY_STAT(GvTInteractionTrace), false);
	Params.AddIgnoredActor(OwnerPawn);

	FHitResult Hit;
	const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, Start, End, TraceChannel, Params);

	if (bDebugDraw)
	{
		DrawDebugLine(GetWorld(), Start, End, bHit ? FColor::Green : FColor::Red, false, 1.0f, 0, 1.0f);
		if (bHit)
		{
			DrawDebugSphere(GetWorld(), Hit.ImpactPoint, 10.f, 12, FColor::Yellow, false, 1.0f);
		}
	}

	AActor* HitActor = bHit ? Hit.GetActor() : nullptr;
	if (!HitActor || !HitActor->GetClass()->ImplementsInterface(UGvTInteractable::StaticClass()))
	{
		return;
	}

	FGvTInteractionSpec Spec;
	IGvTInteractable::Execute_GetInteractionSpec(HitActor, OwnerPawn, Verb, Spec);

	// Instant interactions resolve immediately (no lock-in)
	if (Spec.CastTime <= KINDA_SMALL_NUMBER)
	{
		IGvTInteractable::Execute_CompleteInteract(HitActor, OwnerPawn, Verb);
		return;
	}

	BeginInteraction(HitActor, Verb, Spec);
}

void UGvTInteractionComponent::BeginInteraction(AActor* Target, EGvTInteractionVerb Verb, const FGvTInteractionSpec& Spec)
{
	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!OwnerPawn || !Target)
	{
		return;
	}

	bIsInteracting = true;
	CurrentInteractable = Target;
	ActiveVerb = Verb;
	ActiveSpec = Spec;

	InteractionStartServerTime = GetWorld()->GetTimeSeconds();
	InteractionEndServerTime = InteractionStartServerTime + Spec.CastTime;

	ApplyLockIn(Spec);

	IGvTInteractable::Execute_BeginInteract(Target, OwnerPawn, Verb);

	// Server timer to complete
	GetWorld()->GetTimerManager().ClearTimer(InteractionTimerHandle);
	GetWorld()->GetTimerManager().SetTimer(
		InteractionTimerHandle,
		this,
		&UGvTInteractionComponent::CompleteInteraction,
		Spec.CastTime,
		false
	);

	// Notify local + remote UI
	OnRep_InteractionState();
	OnInteractionStarted.Broadcast(Verb, Target);
}

void UGvTInteractionComponent::CompleteInteraction()
{
	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!OwnerPawn || !bIsInteracting || !CurrentInteractable)
	{
		CancelInteractionInternal(EGvTInteractionCancelReason::Invalid);
		return;
	}

	AActor* Target = CurrentInteractable;

	// Clear state first (prevents re-entrancy)
	GetWorld()->GetTimerManager().ClearTimer(InteractionTimerHandle);
	bIsInteracting = false;
	CurrentInteractable = nullptr;

	const EGvTInteractionVerb Verb = ActiveVerb;

	ClearLockIn();

	// Complete on target
	if (Target->GetClass()->ImplementsInterface(UGvTInteractable::StaticClass()))
	{
		IGvTInteractable::Execute_CompleteInteract(Target, OwnerPawn, Verb);
	}

	// Reset timings/spec
	InteractionStartServerTime = 0.f;
	InteractionEndServerTime = 0.f;

	Client_PlayInteractionFinishSfx(true, Verb, ActiveSpec);
	ActiveSpec = FGvTInteractionSpec{};

	OnRep_InteractionState();
	OnInteractionCompleted.Broadcast(Verb, Target);
}

void UGvTInteractionComponent::CancelInteractionInternal(EGvTInteractionCancelReason Reason)
{
	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!OwnerPawn || !bIsInteracting)
	{
		return;
	}

	AActor* Target = CurrentInteractable;
	const EGvTInteractionVerb Verb = ActiveVerb;
	const FGvTInteractionSpec SpecSnapshot = ActiveSpec;

	GetWorld()->GetTimerManager().ClearTimer(InteractionTimerHandle);

	bIsInteracting = false;
	CurrentInteractable = nullptr;

	ClearLockIn();

	// Notify target
	if (Target && Target->GetClass()->ImplementsInterface(UGvTInteractable::StaticClass()))
	{
		IGvTInteractable::Execute_CancelInteract(Target, OwnerPawn, Verb, Reason);
	}

	// Emit cancel noise (server)
	if (SpecSnapshot.bEmitNoiseOnCancel && SpecSnapshot.CancelNoiseRadius > KINDA_SMALL_NUMBER)
	{
		if (UGameInstance* GI = GetWorld()->GetGameInstance())
		{
			if (UGvTNoiseSubsystem* Noise = GI->GetSubsystem<UGvTNoiseSubsystem>())
			{
				FGvTNoiseEvent E; E.Location = OwnerPawn->GetActorLocation(); E.Radius = SpecSnapshot.CancelNoiseRadius; E.Loudness = SpecSnapshot.CancelNoiseLoudness; E.NoiseTag = SpecSnapshot.InteractionTag; Noise->EmitNoise(E);
			}
		}
	}

	InteractionStartServerTime = 0.f;
	InteractionEndServerTime = 0.f;
	Client_PlayInteractionFinishSfx(false, Verb, SpecSnapshot);
	ActiveSpec = FGvTInteractionSpec{};

	OnRep_InteractionState();
	OnInteractionCanceled.Broadcast(Verb, Target, Reason);
}

void UGvTInteractionComponent::ApplyLockIn(const FGvTInteractionSpec& Spec)
{
	if (AGvTThiefCharacter* Thief = GetOwnerThief())
	{
		Thief->SetInteractionLock(Spec.bLockMovement, Spec.bLockLook);
	}
}

void UGvTInteractionComponent::ClearLockIn()
{
	if (AGvTThiefCharacter* Thief = GetOwnerThief())
	{
		Thief->SetInteractionLock(false, false);
	}
}

AGvTThiefCharacter* UGvTInteractionComponent::GetOwnerThief() const
{
	return Cast<AGvTThiefCharacter>(GetOwner());
}

void UGvTInteractionComponent::OnRep_InteractionState()
{
	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!OwnerPawn || !OwnerPawn->IsLocallyControlled())
		return;

	if (!bPrevInteracting_Local && bIsInteracting)
	{
		// entering interaction locally
		PrevVerb_Local = ActiveVerb;
		PrevSpec_Local = ActiveSpec;

		if (ActiveSpec.LoopSfx)
		{
			ActiveLoopAudio = UGameplayStatics::SpawnSoundAttached(
				ActiveSpec.LoopSfx,
				GetOwner()->GetRootComponent()
			);
		}
	}
	else if (bPrevInteracting_Local && !bIsInteracting)
	{
		// leaving interaction locally
		if (ActiveLoopAudio)
		{
			ActiveLoopAudio->Stop();
			ActiveLoopAudio = nullptr;
		}
	}

	if (bPrevInteracting_Local != bIsInteracting)
	{
		bPrevInteracting_Local = bIsInteracting;
		BP_OnInteractionStateChanged(bIsInteracting, ActiveVerb, CurrentInteractable, ActiveSpec);
	}
}

void UGvTInteractionComponent::Client_PlayInteractionFinishSfx_Implementation(
	bool bCompleted, EGvTInteractionVerb Verb, const FGvTInteractionSpec& Spec)
{
	USoundBase* Sfx = bCompleted ? Spec.EndSfx : Spec.CancelSfx;
	if (Sfx)
	{
		UGameplayStatics::PlaySound2D(this, Sfx);
	}
}


void UGvTInteractionComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UGvTInteractionComponent, bIsInteracting);
	DOREPLIFETIME(UGvTInteractionComponent, CurrentInteractable);
	DOREPLIFETIME(UGvTInteractionComponent, InteractionStartServerTime);
	DOREPLIFETIME(UGvTInteractionComponent, InteractionEndServerTime);
	DOREPLIFETIME(UGvTInteractionComponent, ActiveVerb);
	DOREPLIFETIME(UGvTInteractionComponent, ActiveSpec);
}
