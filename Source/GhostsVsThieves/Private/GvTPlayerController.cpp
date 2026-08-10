#include "GvTPlayerController.h"
#include "Blueprint/UserWidget.h"
#include "GvTPlayerState.h"
#include "GvTGameStateBase.h"
#include "World/Doors/GvTDoorActor.h"
#include "Engine/World.h"
#include "Gameplay/Interaction/GvTInteractable.h"
#include "Gameplay/Characters/Thieves/GvTThiefCharacter.h"
#include "World/Extraction/GvTVanInventoryActor.h"
#include "InputCoreTypes.h"

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
		BindHUDToGameState();
	}
}

void AGvTPlayerController::BindHUDToGameState()
{
	if (!IsLocalController() || !HUDWidget)
	{
		return;
	}

	AGvTGameStateBase* GS = GetWorld() ? GetWorld()->GetGameState<AGvTGameStateBase>() : nullptr;
	if (!GS)
	{
		if (!GetWorldTimerManager().IsTimerActive(TimerHandle_BindGameStateRetry))
		{
			GetWorldTimerManager().SetTimer(TimerHandle_BindGameStateRetry, this, &ThisClass::BindHUDToGameState, 0.1f, true);
		}
		return;
	}

	GetWorldTimerManager().ClearTimer(TimerHandle_BindGameStateRetry);
	HUDWidget->SetTeamSecuredLoot(GS->GetTeamSecuredLoot());
	GS->OnTeamSecuredLootChanged.RemoveDynamic(this, &ThisClass::HandleTeamSecuredLootChanged);
	GS->OnTeamSecuredLootChanged.AddDynamic(this, &ThisClass::HandleTeamSecuredLootChanged);
}

void AGvTPlayerController::HandleTeamSecuredLootChanged(int32 NewTeamSecuredLoot)
{
	if (HUDWidget)
	{
		HUDWidget->SetTeamSecuredLoot(NewTeamSecuredLoot);
	}
}

void AGvTPlayerController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!IsLocalController())
		return;

	UpdateHighlight();
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

	PS->OnPanicChanged.RemoveDynamic(this, &AGvTPlayerController::HandlePanicChanged);
	PS->OnPanicChanged.AddDynamic(this, &AGvTPlayerController::HandlePanicChanged);

	PS->OnHauntPressureChanged.RemoveDynamic(this, &AGvTPlayerController::HandleHauntPressureChanged);
	PS->OnHauntPressureChanged.AddDynamic(this, &AGvTPlayerController::HandleHauntPressureChanged);

	// Push current value immediately so HUD is correct right away.
	HandlePanicChanged(PS->GetPanic01());
}

void AGvTPlayerController::Client_ShowScanResult_Implementation(AActor* Item, const FText& ItemDisplayName, int32 ScannedValue)
{
	if (HUDWidget)
	{
		HUDWidget->ShowScanValueNamed(Item, ItemDisplayName, ScannedValue);
	}
}

void AGvTPlayerController::Client_ShowExtractionMessage_Implementation(const FText& Message, bool bSuccess)
{
	OnExtractionMessage(Message, bSuccess);
	Client_ShowHUDMessage_Implementation(Message, bSuccess);
}

void AGvTPlayerController::Client_ShowHUDMessage_Implementation(const FText& Message, bool bSuccess)
{
	if (Message.IsEmpty())
	{
		return;
	}

	if (HUDWidget)
	{
		HUDWidget->ShowHUDMessage(Message, bSuccess);
	}
	else
	{
		// Preserve feedback if the HUD Blueprint has not been configured yet.
		ClientMessage(Message.ToString());
	}
}

void AGvTPlayerController::Client_SetMissionInputLocked_Implementation(bool bLocked)
{
	SetIgnoreMoveInput(bLocked);
	SetIgnoreLookInput(bLocked);
}

void AGvTPlayerController::Client_OpenVanInventory_Implementation(AGvTVanInventoryActor* VanInventory)
{
	// Client RPCs can arrive repeatedly if interact is spammed before input lock
	// reaches the player. Never create more than one menu instance.
	if (!IsLocalController() || bVanInventoryOpen || !IsValid(VanInventory))
	{
		return;
	}

	SetVanInventoryOpen(true);
	OnOpenVanInventory(VanInventory);
}

bool AGvTPlayerController::InputKey(FKey Key, EInputEvent EventType, float AmountDepressed, bool bGamepad)
{
	if (bVanInventoryOpen && Key == EKeys::Tab && EventType == IE_Pressed)
	{
		CloseVanInventory();
		return true;
	}

	return Super::InputKey(Key, EventType, AmountDepressed, bGamepad);
}

void AGvTPlayerController::CloseVanInventory()
{
	if (!IsLocalController() || !bVanInventoryOpen)
	{
		return;
	}

	// Restore controller state even if the Blueprint close event removes the
	// widget synchronously. The open guard prevents orphan duplicate widgets.
	OnCloseVanInventory();
	SetVanInventoryOpen(false);
}

void AGvTPlayerController::RequestTakeVanItem(AGvTVanInventoryActor* VanInventory, int32 StackIndex)
{
	if (IsLocalController() && IsValid(VanInventory) && StackIndex >= 0)
	{
		Server_RequestTakeVanItem(VanInventory, StackIndex);
	}
}

void AGvTPlayerController::Server_RequestTakeVanItem_Implementation(AGvTVanInventoryActor* VanInventory, int32 StackIndex)
{
	FText FailureMessage;
	AGvTThiefCharacter* Thief = Cast<AGvTThiefCharacter>(GetPawn());
	if (!IsValid(VanInventory) || !VanInventory->TryTakeItem(Thief, StackIndex, FailureMessage))
	{
		Client_ShowHUDMessage(FailureMessage.IsEmpty() ? NSLOCTEXT("GvTVanInventory", "RequestFailed", "Unable to take that item.") : FailureMessage, false);
		return;
	}

	Client_ShowHUDMessage(NSLOCTEXT("GvTVanInventory", "ItemTaken", "Item added to inventory."), true);
}

void AGvTPlayerController::SetVanInventoryOpen(bool bOpen)
{
	if (!IsLocalController())
	{
		return;
	}

	bVanInventoryOpen = bOpen;
	SetIgnoreMoveInput(bOpen);
	SetIgnoreLookInput(bOpen);
	bShowMouseCursor = bOpen;

	if (bOpen)
	{
		FInputModeGameAndUI InputMode;
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		InputMode.SetHideCursorDuringCapture(false);
		SetInputMode(InputMode);
	}
	else
	{
		SetInputMode(FInputModeGameOnly());
	}
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

void AGvTPlayerController::HandlePanicChanged(float NewPanic01)
{
	if (!HUDWidget)
	{
		return;
	}

	if (UGvTHUDWidget* GvTHUD = Cast<UGvTHUDWidget>(HUDWidget))
	{
		GvTHUD->UpdatePanicDisplay(NewPanic01);
	}

	UE_LOG(LogTemp, Warning, TEXT("[HUD] Panic display updated: %.2f"), NewPanic01);
}

void AGvTPlayerController::HandleHauntPressureChanged(float NewPressure01)
{
	// Optional later for pressure on the HUD.
}
