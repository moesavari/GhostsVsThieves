#include "Gameplay/Interaction/GvTInteractionComponent.h"
#include "Systems/Director/GvTDirectorSubsystem.h"
#include "World/Items/GvTInteractableItem.h"
#include "World/Items/GvTMedicineItem.h"
#include "World/Items/GvTLockpickItem.h"
#include "World/Doors/GvTDoorActor.h"
#include "World/Extraction/GvTReconDepositActor.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"
#include "Gameplay/Interaction/GvTInteractable.h"
#include "Gameplay/Characters/Thieves/GvTThiefCharacter.h"
#include "Gameplay/Inventory/GvTInventoryComponent.h"
#include "GvTPlayerController.h"
#include "Systems/Noise/GvTNoiseSubsystem.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"

UGvTInteractionComponent::UGvTInteractionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	SetIsReplicatedByDefault(true);
}

void UGvTInteractionComponent::TryInteract()
{
	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!OwnerPawn || !OwnerPawn->IsLocallyControlled() || !bInteractionEnabled)
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

void UGvTInteractionComponent::TryUseSelectedEquipment()
{
	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!OwnerPawn || !OwnerPawn->IsLocallyControlled() || !bInteractionEnabled)
	{
		return;
	}

	if (bIsInteracting)
	{
		TryCancelInteraction(EGvTInteractionCancelReason::UserCanceled);
		return;
	}

	Server_TryUseSelectedEquipment();
}

void UGvTInteractionComponent::TryPhoto()
{
	TryScan();
}

void UGvTInteractionComponent::TryScan()
{
	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!OwnerPawn || !OwnerPawn->IsLocallyControlled() || !bInteractionEnabled)
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
	if (!bInteractionEnabled)
	{
		return;
	}

	PerformServerTraceAndTryStart(Verb, false);
}

void UGvTInteractionComponent::Server_TryUseSelectedEquipment_Implementation()
{
	if (!bInteractionEnabled)
	{
		return;
	}

	PerformServerTraceAndTryStart(EGvTInteractionVerb::Interact, true);
}

void UGvTInteractionComponent::SetInteractionEnabled(bool bEnabled)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	bInteractionEnabled = bEnabled;
	if (!bEnabled && bIsInteracting)
	{
		CancelInteractionInternal(EGvTInteractionCancelReason::Invalid);
	}
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

AActor* UGvTInteractionComponent::FindAssistedItemTarget(const FVector& Start, const FVector& End, const FCollisionQueryParams& Params) const
{
	if (!GetWorld() || ItemInteractionAssistRadius <= KINDA_SMALL_NUMBER)
	{
		return nullptr;
	}

	TArray<FHitResult> AssistHits;
	const FCollisionShape AssistShape = FCollisionShape::MakeSphere(ItemInteractionAssistRadius);
	GetWorld()->SweepMultiByChannel(AssistHits, Start, End, FQuat::Identity, TraceChannel, AssistShape, Params);

	AActor* BestItem = nullptr;
	float BestDistanceAlongTrace = TNumericLimits<float>::Max();
	const FVector TraceDirection = (End - Start).GetSafeNormal();

	for (const FHitResult& AssistHit : AssistHits)
	{
		AGvTInteractableItem* Item = Cast<AGvTInteractableItem>(AssistHit.GetActor());
		if (!Item)
		{
			continue;
		}

		// The sweep supplies aim forgiveness; this precise trace ensures a wall or
		// other obstruction still prevents the assisted pickup.
		const FVector ItemCenter = Item->GetComponentsBoundingBox(true).GetCenter();
		FHitResult VisibilityHit;
		if (!GetWorld()->LineTraceSingleByChannel(VisibilityHit, Start, ItemCenter, TraceChannel, Params)
			|| VisibilityHit.GetActor() != Item)
		{
			continue;
		}

		const float DistanceAlongTrace = FVector::DotProduct(ItemCenter - Start, TraceDirection);
		if (DistanceAlongTrace >= 0.f && DistanceAlongTrace < BestDistanceAlongTrace)
		{
			BestDistanceAlongTrace = DistanceAlongTrace;
			BestItem = Item;
		}
	}

#if GVT_ENABLE_DEBUG_TOOLS && !UE_BUILD_SHIPPING
	if (bDebugDraw)
	{
		DrawDebugCapsule(
			GetWorld(),
			(Start + End) * 0.5f,
			FVector::Distance(Start, End) * 0.5f,
			ItemInteractionAssistRadius,
			FQuat::FindBetweenNormals(FVector::UpVector, (End - Start).GetSafeNormal()),
			BestItem ? FColor::Cyan : FColor::Silver,
			false,
			1.0f,
			0,
			1.0f);
	}
#endif

	return BestItem;
}

void UGvTInteractionComponent::PerformServerTraceAndTryStart(EGvTInteractionVerb Verb, bool bEquipmentUse)
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

#if GVT_ENABLE_DEBUG_TOOLS && !UE_BUILD_SHIPPING
	if (bDebugDraw)
	{
		DrawDebugLine(GetWorld(), Start, End, bHit ? FColor::Green : FColor::Red, false, 1.0f, 0, 1.0f);
		if (bHit)
		{
			DrawDebugSphere(GetWorld(), Hit.ImpactPoint, 10.f, 12, FColor::Yellow, false, 1.0f);
		}
	}
#endif

	AActor* HitActor = bHit ? Hit.GetActor() : nullptr;
	const bool bPreciseInteractableHit = HitActor && HitActor->GetClass()->ImplementsInterface(UGvTInteractable::StaticClass());
	if (Verb == EGvTInteractionVerb::Interact && !bEquipmentUse && !bPreciseInteractableHit)
	{
		HitActor = FindAssistedItemTarget(Start, End, Params);
	}
	const AGvTThiefCharacter* Thief = Cast<AGvTThiefCharacter>(OwnerPawn);
	const UGvTInventoryComponent* Inventory = Thief ? Thief->GetInventoryComponent() : nullptr;
	const bool bHoldingLockpick = Inventory && Inventory->GetSelectedItem() && Inventory->GetSelectedItem()->IsA<AGvTLockpickItem>();

	if (!HitActor || !HitActor->GetClass()->ImplementsInterface(UGvTInteractable::StaticClass()))
	{
		if (bEquipmentUse && Verb == EGvTInteractionVerb::Interact && TryStartSelectedMedicine())
		{
			return;
		}
		if (AGvTPlayerController* PC = Cast<AGvTPlayerController>(OwnerPawn->GetController()))
		{
			PC->Client_ShowHUDMessage(FText::FromString(TEXT("Nothing to interact with.")), false);
		}
		return;
	}

	if (const AGvTDoorActor* Door = Cast<AGvTDoorActor>(HitActor))
	{
		if (Door->IsLocked() && !Door->IsHauntLocked() && bHoldingLockpick && !bEquipmentUse)
		{
			if (AGvTPlayerController* PC = Cast<AGvTPlayerController>(OwnerPawn->GetController()))
			{
				PC->Client_ShowHUDMessage(FText::FromString(TEXT("Left Click to use the selected lockpick.")), false);
			}
			return;
		}
	}
	else if (bEquipmentUse)
	{
		if (AGvTPlayerController* PC = Cast<AGvTPlayerController>(OwnerPawn->GetController()))
		{
			PC->Client_ShowHUDMessage(FText::FromString(TEXT("That equipment cannot be used here.")), false);
		}
		return;
	}

	if (!IGvTInteractable::Execute_CanInteract(HitActor, OwnerPawn, Verb))
	{
		UE_LOG(LogTemp, Verbose, TEXT("[Interaction] Player=%s Target=%s Verb=%d Result=REJECTED"), *GetNameSafe(OwnerPawn), *GetNameSafe(HitActor), static_cast<int32>(Verb));

		FText FailureMessage = FText::FromString(TEXT("Cannot interact with that right now."));
		if (const AGvTDoorActor* Door = Cast<AGvTDoorActor>(HitActor))
		{
			if (Verb == EGvTInteractionVerb::Interact && Door->IsHauntLocked())
			{
				FailureMessage = FText::FromString(TEXT("The house will not let this door open."));
			}
			else if (Verb == EGvTInteractionVerb::Interact && Door->IsLocked())
			{
				FailureMessage = FText::FromString(TEXT("This door is locked. Select a lockpick to unlock it."));
			}
		}
		else if (const AGvTInteractableItem* Item = Cast<AGvTInteractableItem>(HitActor))
		{
			if (Verb == EGvTInteractionVerb::Scan && Item->HasBeenScanned())
			{
				FailureMessage = FText::FromString(TEXT("This item has already been scanned."));
			}
			else if (Verb == EGvTInteractionVerb::Interact)
			{
				const AGvTThiefCharacter* InventoryThief = Cast<AGvTThiefCharacter>(OwnerPawn);
				UGvTInventoryComponent* InventoryForCheck = InventoryThief ? InventoryThief->GetInventoryComponent() : nullptr;

				if (InventoryForCheck && Item->GetInventorySpaceCost() > InventoryForCheck->GetRemainingCapacity())
				{
					FailureMessage = FText::FromString(TEXT("Not enough inventory space."));
				}
			}
		}
		else if (HitActor->IsA<AGvTReconDepositActor>())
		{
			const AGvTThiefCharacter* DepositThief = Cast<AGvTThiefCharacter>(OwnerPawn);
			const UGvTInventoryComponent* DepositInventory = DepositThief ? DepositThief->GetInventoryComponent() : nullptr;
			const AGvTInteractableItem* SelectedItem = DepositInventory ? DepositInventory->GetSelectedItem() : nullptr;

			if (!SelectedItem)
			{
				FailureMessage = FText::FromString(TEXT("Select stolen loot, then press E to deposit it."));
			}
			else if (!SelectedItem->IsStolenLoot())
			{
				FailureMessage = FText::Format(
					FText::FromString(TEXT("{0} is equipment and cannot be deposited. Select stolen loot first.")),
					SelectedItem->GetInventoryDisplayName());
			}
		}

		if (AGvTPlayerController* PC = Cast<AGvTPlayerController>(OwnerPawn->GetController()))
		{
			PC->Client_ShowHUDMessage(FailureMessage, false);
		}
		return;
	}

	FGvTInteractionSpec Spec;
	IGvTInteractable::Execute_GetInteractionSpec(HitActor, OwnerPawn, Verb, Spec);

	// Instant interactions resolve immediately (no lock-in).
	// Still notify the Director; otherwise instant pickup items never anger ghosts.
	if (Spec.CastTime <= KINDA_SMALL_NUMBER)
	{
		IGvTInteractable::Execute_CompleteInteract(HitActor, OwnerPawn, Verb);

		// Inventory items notify the Director themselves only after pickup succeeds.
		if (GetOwner()->HasAuthority() && !HitActor->IsA<AGvTInteractableItem>())
		{
			if (UWorld* World = GetWorld())
			{
				if (UGvTDirectorSubsystem* Director = World->GetGameInstance()->GetSubsystem<UGvTDirectorSubsystem>())
				{
					Director->OnPlayerInteractionEvent(OwnerPawn, HitActor, Verb);
				}
			}
		}

		return;
	}

	BeginInteraction(HitActor, Verb, Spec);
}

bool UGvTInteractionComponent::TryStartSelectedMedicine()
{
	AGvTThiefCharacter* Thief = GetOwnerThief();
	UGvTInventoryComponent* Inventory = Thief ? Thief->GetInventoryComponent() : nullptr;
	AGvTMedicineItem* Medicine = Inventory ? Cast<AGvTMedicineItem>(Inventory->GetSelectedItem()) : nullptr;
	if (!Medicine || !Medicine->CanUseMedicine(Thief))
	{
		return false;
	}

	FGvTInteractionSpec Spec;
	Medicine->BuildMedicineUseSpec(Spec);
	BeginInteraction(Medicine, EGvTInteractionVerb::Interact, Spec);
	return true;
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
	if (AGvTThiefCharacter* Thief = GetOwnerThief())
	{
		RequiredSelectedItem = Thief->GetInventoryComponent() ? Thief->GetInventoryComponent()->GetSelectedItem() : nullptr;
	}
	ActiveVerb = Verb;
	ActiveSpec = Spec;

	InteractionStartServerTime = GetWorld()->GetTimeSeconds();
	InteractionEndServerTime = InteractionStartServerTime + Spec.CastTime;

	ApplyLockIn(Spec);

	IGvTInteractable::Execute_BeginInteract(Target, OwnerPawn, Verb);
	SetComponentTickEnabled(true);
	if (Spec.bEmitPeriodicNoise)
	{
		EmitPeriodicInteractionNoise();
		GetWorld()->GetTimerManager().SetTimer(PeriodicNoiseTimerHandle, this, &UGvTInteractionComponent::EmitPeriodicInteractionNoise, FMath::Max(0.05f, Spec.PeriodicNoiseInterval), true);
	}

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
	if (!OwnerPawn || !bIsInteracting || !CurrentInteractable || !IsActiveInteractionStillValid())
	{
		CancelInteractionInternal(EGvTInteractionCancelReason::Invalid);
		return;
	}

	AActor* Target = CurrentInteractable;

	// Clear state first (prevents re-entrancy)
	GetWorld()->GetTimerManager().ClearTimer(InteractionTimerHandle);
	GetWorld()->GetTimerManager().ClearTimer(PeriodicNoiseTimerHandle);
	SetComponentTickEnabled(false);
	bIsInteracting = false;
	CurrentInteractable = nullptr;

	const EGvTInteractionVerb Verb = ActiveVerb;

	ClearLockIn();

	// Complete on target
	if (Target->GetClass()->ImplementsInterface(UGvTInteractable::StaticClass()))
	{
		IGvTInteractable::Execute_CompleteInteract(Target, OwnerPawn, Verb);
	}

	// Inventory items notify the Director themselves only after pickup succeeds.
	if (GetOwner()->HasAuthority() && !Target->IsA<AGvTInteractableItem>())
	{
		if (UWorld* World = GetWorld())
		{
			if (UGvTDirectorSubsystem* Director = World->GetGameInstance()->GetSubsystem<UGvTDirectorSubsystem>())
			{
				Director->OnPlayerInteractionEvent(OwnerPawn, Target, Verb);
			}
		}
	}

	// Reset timings/spec
	InteractionStartServerTime = 0.f;
	InteractionEndServerTime = 0.f;

	Client_PlayInteractionFinishSfx(true, Verb, ActiveSpec);
	ActiveSpec = FGvTInteractionSpec{};
	RequiredSelectedItem = nullptr;

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
	GetWorld()->GetTimerManager().ClearTimer(PeriodicNoiseTimerHandle);
	SetComponentTickEnabled(false);

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
	RequiredSelectedItem = nullptr;

	OnRep_InteractionState();
	OnInteractionCanceled.Broadcast(Verb, Target, Reason);
}

void UGvTInteractionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (GetOwner() && GetOwner()->HasAuthority() && bIsInteracting && !IsActiveInteractionStillValid())
	{
		CancelInteractionInternal(EGvTInteractionCancelReason::Invalid);
	}
}

bool UGvTInteractionComponent::IsActiveInteractionStillValid() const
{
	const AGvTThiefCharacter* Thief = GetOwnerThief();
	if (!Thief || Thief->IsDead() || !IsValid(CurrentInteractable))
	{
		return false;
	}

	const UGvTInventoryComponent* Inventory = Thief->GetInventoryComponent();
	if (RequiredSelectedItem && (!Inventory || Inventory->GetSelectedItem() != RequiredSelectedItem))
	{
		return false;
	}

	if (CurrentInteractable != RequiredSelectedItem && FVector::DistSquared(Thief->GetActorLocation(), CurrentInteractable->GetActorLocation()) > FMath::Square(ActiveInteractionRange))
	{
		return false;
	}

	if (const AGvTDoorActor* Door = Cast<AGvTDoorActor>(CurrentInteractable))
	{
		if (ActiveSpec.bEmitPeriodicNoise && !Door->IsLocked())
		{
			return false;
		}
	}

	return IGvTInteractable::Execute_CanInteract(CurrentInteractable, const_cast<AGvTThiefCharacter*>(Thief), ActiveVerb);
}

void UGvTInteractionComponent::EmitPeriodicInteractionNoise()
{
	if (!bIsInteracting || !ActiveSpec.bEmitPeriodicNoise || !IsActiveInteractionStillValid())
	{
		return;
	}

	if (UGameInstance* GI = GetWorld()->GetGameInstance())
	{
		if (UGvTNoiseSubsystem* Noise = GI->GetSubsystem<UGvTNoiseSubsystem>())
		{
			FGvTNoiseEvent Event;
			Event.Location = CurrentInteractable ? CurrentInteractable->GetActorLocation() : GetOwner()->GetActorLocation();
			Event.Radius = ActiveSpec.PeriodicNoiseRadius;
			Event.Loudness = ActiveSpec.PeriodicNoiseLoudness;
			Event.NoiseTag = ActiveSpec.InteractionTag;
			Noise->EmitNoise(Event);
		}
	}
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
	DOREPLIFETIME(UGvTInteractionComponent, bInteractionEnabled);
	DOREPLIFETIME(UGvTInteractionComponent, CurrentInteractable);
	DOREPLIFETIME(UGvTInteractionComponent, InteractionStartServerTime);
	DOREPLIFETIME(UGvTInteractionComponent, InteractionEndServerTime);
	DOREPLIFETIME(UGvTInteractionComponent, ActiveVerb);
	DOREPLIFETIME(UGvTInteractionComponent, ActiveSpec);
}
