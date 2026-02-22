#include "GvTPlayerController.h"
#include "Blueprint/UserWidget.h"
#include "GvTPlayerState.h"
#include "World/Doors/GvTDoorActor.h"
#include "Engine/World.h"
#include "Gameplay/Interaction/GvTInteractable.h"

void AGvTPlayerController::BeginPlay()
{
    Super::BeginPlay();

    if (!IsLocalController())
    {
        return;
    }

    if (!HUDWidgetClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("GvTPlayerController: HUDWidgetClass is not set."));
        return;
    }

	HUDWidget = CreateWidget<UGvTHUDWidget>(this, HUDWidgetClass);
	UE_LOG(LogTemp, Warning, TEXT("HUDWidgetClass=%s"), *GetNameSafe(HUDWidgetClass));
	UE_LOG(LogTemp, Warning, TEXT("HUDWidget=%s"), *GetNameSafe(HUDWidget));

	if (HUDWidget)
	{
		HUDWidget->AddToViewport();
		BindHUDToPlayerState();
	}
}

void AGvTPlayerController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!IsLocalController())
		return;

	UpdateHighlight();
}


static AGvTDoorActor* FindDoorLookAt(APlayerController* PC, float MaxDistance)
{
	if (!PC || !PC->GetWorld()) return nullptr;

	FVector Loc; FRotator Rot;
	PC->GetPlayerViewPoint(Loc, Rot);

	FHitResult Hit;
	const FVector End = Loc + Rot.Vector() * MaxDistance;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(DoorDebug), false);
	if (APawn* P = PC->GetPawn()) Params.AddIgnoredActor(P);

	if (!PC->GetWorld()->LineTraceSingleByChannel(Hit, Loc, End, ECC_Visibility, Params))
		return nullptr;

	return Cast<AGvTDoorActor>(Hit.GetActor());
}

void AGvTPlayerController::DoorLock(float MaxDistance)
{
	if (!HasAuthority()) return;
	if (AGvTDoorActor* Door = FindDoorLookAt(this, MaxDistance))
	{
		Door->SetLocked(true);
	}
}

void AGvTPlayerController::DoorUnlock(float MaxDistance)
{
	if (!HasAuthority()) return;
	if (AGvTDoorActor* Door = FindDoorLookAt(this, MaxDistance))
	{
		Door->SetLocked(false);
	}
}

void AGvTPlayerController::DoorToggleLock(float MaxDistance)
{
	if (!HasAuthority()) return;
	if (AGvTDoorActor* Door = FindDoorLookAt(this, MaxDistance))
	{
		Door->SetLocked(!Door->IsLocked());
	}
}

void AGvTPlayerController::DoorForceUnlock(float MaxDistance)
{
	if (!HasAuthority()) return;
	if (AGvTDoorActor* Door = FindDoorLookAt(this, MaxDistance))
	{
		Door->TryUnlock(GetPawn(), EDoorUnlockMethod::Force, true);
	}
}

void AGvTPlayerController::UpdateHighlight()
{
	FVector ViewLoc;
	FRotator ViewRot;
	GetPlayerViewPoint(ViewLoc, ViewRot);

	const FVector Start = ViewLoc;
	const FVector End = Start + ViewRot.Vector() * HighlightTraceDistance;

	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GvTHighlightTrace), false);
	if (APawn* P = GetPawn())
	{
		Params.AddIgnoredActor(P);
	}

	AActor* HitActor = nullptr;
	if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
	{
		HitActor = Hit.GetActor();
	}

	// Only highlight interactables you can Interact with (spooky, not spammy)
	if (HitActor && HitActor->GetClass()->ImplementsInterface(UGvTInteractable::StaticClass()))
	{
		const bool bCan = IGvTInteractable::Execute_CanInteract(HitActor, GetPawn(), EGvTInteractionVerb::Interact);
		if (!bCan)
		{
			HitActor = nullptr;
		}
	}
	else
	{
		HitActor = nullptr;
	}

	AActor* Prev = CurrentHighlightedActor.Get();
	if (Prev != HitActor)
	{
		if (Prev) SetActorHighlighted(Prev, false);
		if (HitActor) SetActorHighlighted(HitActor, true);
		CurrentHighlightedActor = HitActor;
	}
}

void AGvTPlayerController::SetActorHighlighted(AActor* Actor, bool bHighlighted)
{
	if (!Actor) return;

	TArray<UPrimitiveComponent*> PrimComps;
	Actor->GetComponents<UPrimitiveComponent>(PrimComps);

	for (UPrimitiveComponent* Comp : PrimComps)
	{
		if (!Comp || !Comp->IsRegistered())
			continue;

		Comp->SetRenderCustomDepth(bHighlighted);
		if (bHighlighted)
		{
			Comp->SetCustomDepthStencilValue(HighlightStencilValue);
		}
	}
}

void AGvTPlayerController::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	BindHUDToPlayerState();
}

void AGvTPlayerController::BindHUDToPlayerState()
{
	if (!IsLocalController() || !HUDWidget)
		return;

	AGvTPlayerState* PS = GetPlayerState<AGvTPlayerState>();

	if (!PS)
	{
		UE_LOG(LogTemp, Warning, TEXT("BindHUDToPlayerState: PS None, retrying..."));

		if (!GetWorldTimerManager().IsTimerActive(TimerHandle_BindHUDRetry))
		{
			GetWorldTimerManager().SetTimer(
				TimerHandle_BindHUDRetry,
				this,
				&AGvTPlayerController::BindHUDToPlayerState,
				0.1f,
				true
			);
		}
		return;
	}

	GetWorldTimerManager().ClearTimer(TimerHandle_BindHUDRetry);

	UE_LOG(LogTemp, Warning, TEXT("BindHUDToPlayerState: SUCCESS PS=%s Loot=%d"),
		*GetNameSafe(PS), PS->GetLoot());

	HUDWidget->SetLootValue(PS->GetLoot());

	PS->OnLootValueChanged.RemoveDynamic(HUDWidget, &UGvTHUDWidget::HandleLootChanged);
	PS->OnLootValueChanged.AddDynamic(HUDWidget, &UGvTHUDWidget::HandleLootChanged);
}

void AGvTPlayerController::Client_ShowScanResult_Implementation(AActor* Item, const FText& ItemDisplayName, int32 ScannedValue)
{
	if (HUDWidget)
	{
		HUDWidget->ShowScanValueNamed(Item, ItemDisplayName, ScannedValue);
	}
}
