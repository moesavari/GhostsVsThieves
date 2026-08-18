#include "GvTPlayerController.h"
#include "Blueprint/UserWidget.h"
#include "GvTPlayerState.h"
#include "GvTGameStateBase.h"
#include "Systems/GvTMissionResultsWidget.h"
#include "World/Doors/GvTDoorActor.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Gameplay/Interaction/GvTInteractable.h"
#include "Gameplay/Characters/Thieves/GvTThiefCharacter.h"
#include "Gameplay/Inventory/GvTInventoryComponent.h"
#include "World/Items/GvTInteractableItem.h"
#include "World/Items/GvTMedicineItem.h"
#include "World/Extraction/GvTReconDepositActor.h"
#include "World/Extraction/GvTVanInventoryActor.h"
#include "Systems/Session/GvTSessionSubsystem.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"

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

void AGvTPlayerController::Client_ShowOnboardingPrompt_Implementation(EGvTOnboardingPrompt Prompt)
{
	ShowOnboardingPromptLocal(Prompt);
}

void AGvTPlayerController::ShowOnboardingPromptLocal(EGvTOnboardingPrompt Prompt)
{
	if (!IsLocalController() || ShownOnboardingPrompts.Contains(Prompt))
	{
		return;
	}

	FText Message;
	bool bSuccess = false;
	switch (Prompt)
	{
		case EGvTOnboardingPrompt::FirstItemCollected:
			Message = NSLOCTEXT("GvTOnboarding", "FirstItem", "Mouse Wheel: Cycle Inventory  |  G: Drop Item");
			break;
		case EGvTOnboardingPrompt::ReturnToVan:
			Message = NSLOCTEXT("GvTOnboarding", "ReturnToVan", "Deposit carried valuables on the van's right side. Review stored items at the back.");
			break;
		case EGvTOnboardingPrompt::MainObjectiveWarning:
			Message = NSLOCTEXT("GvTOnboarding", "ObjectiveWarning", "WARNING: Taking the objective will trigger a haunt");
			break;
		case EGvTOnboardingPrompt::ObjectiveSecured:
			Message = NSLOCTEXT("GvTOnboarding", "ObjectiveSecured", "Return to the van and prepare for extraction");
			bSuccess = true;
			break;
		case EGvTOnboardingPrompt::MedicinePanic:
			Message = NSLOCTEXT("GvTOnboarding", "MedicinePanic", "Medicine lowers Panic and reduces its recovery floor");
			break;
		default:
			return;
	}

	ShownOnboardingPrompts.Add(Prompt);
	Client_ShowHUDMessage_Implementation(Message, bSuccess);
}

void AGvTPlayerController::Client_SetMissionInputLocked_Implementation(bool bLocked)
{
	bMissionInputLocked = bLocked;

	if (bLocked && bVanInventoryOpen)
	{
		CloseVanInventory();
	}

	if (bLocked && bPauseMenuOpen)
	{
		SetPauseMenuOpen(false);
	}

	SetIgnoreMoveInput(bLocked || bVanInventoryOpen || bPauseMenuOpen);
	SetIgnoreLookInput(bLocked || bVanInventoryOpen || bPauseMenuOpen);
}

void AGvTPlayerController::Client_ShowMissionResults_Implementation(const FGvTMissionResults& Results)
{
	if (!IsLocalController() || !GetWorld() || GetWorld()->bIsTearingDown) return;

	if (bVanInventoryOpen)
	{
		CloseVanInventory();
	}

	if (HUDWidget)
	{
		HUDWidget->SetVisibility(ESlateVisibility::Collapsed);
	}

	if (!MissionResultsWidgetClass)
	{
		UE_LOG(LogTemp, Error, TEXT("GvTPlayerController: MissionResultsWidgetClass is not set."));
		return;
	}

	if (!MissionResultsWidget)
	{
		MissionResultsWidget = CreateWidget<UGvTMissionResultsWidget>(this, MissionResultsWidgetClass);
	}

	if (!MissionResultsWidget) return;

	MissionResultsWidget->SetMissionResults(Results);
	if (!MissionResultsWidget->IsInViewport())
	{
		MissionResultsWidget->AddToViewport(5000);
	}

	bShowMouseCursor = false;
	SetInputMode(FInputModeUIOnly());
}

void AGvTPlayerController::Client_ReturnToMainMenuAfterMission_Implementation(FName ReturnMapName)
{
	if (!IsLocalController() || ReturnMapName.IsNone())
	{
		return;
	}

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UGvTSessionSubsystem* Sessions = GameInstance->GetSubsystem<UGvTSessionSubsystem>())
		{
			Sessions->LeaveSessionAndReturnToMenuImmediately(ReturnMapName);
			return;
		}
	}

	UGameplayStatics::OpenLevel(this, ReturnMapName, true);
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

void AGvTPlayerController::TogglePauseMenu()
{
	if (!IsLocalController() || bMissionInputLocked || !Cast<AGvTThiefCharacter>(GetPawn()))
	{
		return;
	}

	// Pause input closes the van inventory first instead of stacking two menus.
	if (bVanInventoryOpen)
	{
		CloseVanInventory();
		return;
	}

	SetPauseMenuOpen(!bPauseMenuOpen);
}

void AGvTPlayerController::ResumeFromPauseMenu()
{
	SetPauseMenuOpen(false);
}

void AGvTPlayerController::SetPauseMenuOpen(bool bOpen)
{
	if (!IsLocalController() || bPauseMenuOpen == bOpen)
	{
		return;
	}

	if (bOpen)
	{
		if (bMissionInputLocked || (MissionResultsWidget && MissionResultsWidget->IsInViewport()))
		{
			return;
		}

		if (bVanInventoryOpen)
		{
			CloseVanInventory();
		}

		if (!PauseMenuWidgetClass)
		{
			UE_LOG(LogTemp, Error, TEXT("GvTPlayerController: PauseMenuWidgetClass is not set."));
			return;
		}

		if (!PauseMenuWidget)
		{
			PauseMenuWidget = CreateWidget<UUserWidget>(this, PauseMenuWidgetClass);
		}

		if (!PauseMenuWidget)
		{
			UE_LOG(LogTemp, Error, TEXT("GvTPlayerController: Failed to create pause menu widget."));
			return;
		}

		bPauseMenuOpen = true;
		PauseMenuWidget->SetVisibility(ESlateVisibility::Visible);
		if (!PauseMenuWidget->IsInViewport())
		{
			PauseMenuWidget->AddToViewport(10000);
		}

		SetIgnoreMoveInput(true);
		SetIgnoreLookInput(true);
		bShowMouseCursor = true;

		FInputModeGameAndUI InputMode;
		InputMode.SetWidgetToFocus(PauseMenuWidget->TakeWidget());
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		InputMode.SetHideCursorDuringCapture(false);
		SetInputMode(InputMode);

		// A networked player must not freeze the shared server for everyone.
		if (GetNetMode() == NM_Standalone)
		{
			UGameplayStatics::SetGamePaused(this, true);
		}

		return;
	}

	if (GetNetMode() == NM_Standalone)
	{
		UGameplayStatics::SetGamePaused(this, false);
	}

	bPauseMenuOpen = false;
	if (PauseMenuWidget)
	{
		PauseMenuWidget->SetVisibility(ESlateVisibility::Collapsed);
	}

	SetIgnoreMoveInput(bMissionInputLocked || bVanInventoryOpen);
	SetIgnoreLookInput(bMissionInputLocked || bVanInventoryOpen);
	bShowMouseCursor = bVanInventoryOpen;

	if (!bMissionInputLocked && !bVanInventoryOpen)
	{
		SetInputMode(FInputModeGameOnly());
	}
}

void AGvTPlayerController::ReturnToMainMenuFromPauseMenu()
{
	if (!IsLocalController() || MainMenuMapName.IsNone())
	{
		return;
    }

    SetPauseMenuOpen(false);
    if (UGameInstance* GameInstance = GetGameInstance())
    {
        if (UGvTSessionSubsystem* Sessions = GameInstance->GetSubsystem<UGvTSessionSubsystem>())
        {
            Sessions->LeaveSessionAndReturnToMenu(MainMenuMapName);
            return;
        }
    }

    UGameplayStatics::OpenLevel(this, MainMenuMapName, true);
}

void AGvTPlayerController::QuitGameFromPauseMenu()
{
	if (!IsLocalController())
	{
		return;
	}

	SetPauseMenuOpen(false);
	UKismetSystemLibrary::QuitGame(this, this, EQuitPreference::Quit, false);
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
	const FGvTVanItemHoverInfo RequestedItem = IsValid(VanInventory)
		? VanInventory->GetItemHoverInfo(StackIndex)
		: FGvTVanItemHoverInfo{};

	FText FailureMessage;
	AGvTThiefCharacter* Thief = Cast<AGvTThiefCharacter>(GetPawn());
	if (!IsValid(VanInventory) || !VanInventory->TryTakeItem(Thief, StackIndex, FailureMessage))
	{
		Client_ShowHUDMessage(FailureMessage.IsEmpty() ? NSLOCTEXT("GvTVanInventory", "RequestFailed", "Unable to take that item.") : FailureMessage, false);
		return;
	}

	const FGvTVanItemHoverInfo UpdatedItem = VanInventory->GetItemHoverInfo(StackIndex);
	const FText ItemName = RequestedItem.bIsValid && !RequestedItem.DisplayName.IsEmpty()
		? RequestedItem.DisplayName
		: NSLOCTEXT("GvTVanInventory", "FallbackItemName", "Item");

	const FText SuccessMessage = UpdatedItem.bIsValid && UpdatedItem.Quantity > 0
		? FText::Format(
			NSLOCTEXT("GvTVanInventory", "ItemTakenRemaining", "{0} added. {1} remaining in the van. Mouse Wheel: equip | G: drop"),
			ItemName,
			FText::AsNumber(UpdatedItem.Quantity))
		: FText::Format(
			NSLOCTEXT("GvTVanInventory", "ItemTakenEmpty", "{0} added. That stack is now empty. Mouse Wheel: equip | G: drop"),
			ItemName);

	Client_ShowHUDMessage(SuccessMessage, true);
}

void AGvTPlayerController::SetVanInventoryOpen(bool bOpen)
{
	if (!IsLocalController())
	{
		return;
	}

	bVanInventoryOpen = bOpen;
	SetIgnoreMoveInput(bOpen || bPauseMenuOpen || bMissionInputLocked);
	SetIgnoreLookInput(bOpen || bPauseMenuOpen || bMissionInputLocked);
	bShowMouseCursor = bOpen || bPauseMenuOpen;

	if (bOpen)
	{
		FInputModeGameAndUI InputMode;
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		InputMode.SetHideCursorDuringCapture(false);
		SetInputMode(InputMode);
	}
	else if (!bPauseMenuOpen && !bMissionInputLocked)
	{
		SetInputMode(FInputModeGameOnly());
	}
}

#if GVT_ENABLE_DEBUG_TOOLS && !UE_BUILD_SHIPPING
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
#endif

void AGvTPlayerController::DoorLock(float MaxDistance)
{
#if GVT_ENABLE_DEBUG_TOOLS && !UE_BUILD_SHIPPING
	if (!HasAuthority()) return;
	if (AGvTDoorActor* Door = FindDoorLookAt(this, MaxDistance))
	{
		Door->SetLocked(true);
	}
#endif
}

void AGvTPlayerController::DoorUnlock(float MaxDistance)
{
#if GVT_ENABLE_DEBUG_TOOLS && !UE_BUILD_SHIPPING
	if (!HasAuthority()) return;
	if (AGvTDoorActor* Door = FindDoorLookAt(this, MaxDistance))
	{
		Door->SetLocked(false);
	}
#endif
}

void AGvTPlayerController::DoorToggleLock(float MaxDistance)
{
#if GVT_ENABLE_DEBUG_TOOLS && !UE_BUILD_SHIPPING
	if (!HasAuthority()) return;
	if (AGvTDoorActor* Door = FindDoorLookAt(this, MaxDistance))
	{
		Door->SetLocked(!Door->IsLocked());
	}
#endif
}

void AGvTPlayerController::DoorForceUnlock(float MaxDistance)
{
#if GVT_ENABLE_DEBUG_TOOLS && !UE_BUILD_SHIPPING
	if (!HasAuthority()) return;
	if (AGvTDoorActor* Door = FindDoorLookAt(this, MaxDistance))
	{
		Door->TryUnlock(GetPawn(), EDoorUnlockMethod::Force, true);
	}
#endif
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
	if (!HitActor || !HitActor->GetClass()->ImplementsInterface(UGvTInteractable::StaticClass()))
	{
		HitActor = nullptr;
	}
	//else
	//{
	//	HitActor = nullptr;
	//}

	AActor* Prev = CurrentHighlightedActor.Get();
	if (Prev != HitActor)
	{
		if (Prev) SetActorHighlighted(Prev, false);
		if (HitActor)
		{
			SetActorHighlighted(HitActor, true);

			if (const AGvTInteractableItem* Item = Cast<AGvTInteractableItem>(HitActor))
			{
				if (Item->IsMainObjective())
				{
					ShowOnboardingPromptLocal(EGvTOnboardingPrompt::MainObjectiveWarning);
				}
				else if (Item->IsA<AGvTMedicineItem>())
				{
					ShowOnboardingPromptLocal(EGvTOnboardingPrompt::MedicinePanic);
				}
			}

			if (HitActor->IsA<AGvTReconDepositActor>() || HitActor->IsA<AGvTVanInventoryActor>())
			{
				const AGvTThiefCharacter* Thief = Cast<AGvTThiefCharacter>(GetPawn());
				const UGvTInventoryComponent* Inventory = Thief ? Thief->GetInventoryComponent() : nullptr;
				if (Inventory && Inventory->ContainsStolenLoot())
				{
					ShowOnboardingPromptLocal(EGvTOnboardingPrompt::ReturnToVan);
				}
			}
		}
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
			const AGvTInteractableItem* Item = Cast<AGvTInteractableItem>(Actor);
			Comp->SetCustomDepthStencilValue(Item && Item->IsMainObjective() ? MainObjectiveHighlightStencilValue : HighlightStencilValue);
		}
	}
}

void AGvTPlayerController::HandlePanicChanged(float NewPanic01)
{
	if (NewPanic01 >= MedicinePanicHintThreshold01)
	{
		ShowOnboardingPromptLocal(EGvTOnboardingPrompt::MedicinePanic);
	}

#if GVT_ENABLE_DEBUG_TOOLS && !UE_BUILD_SHIPPING
	const int32 DisplayedPercent = FMath::RoundToInt(FMath::Clamp(NewPanic01, 0.f, 1.f) * 100.f);
	if (DisplayedPercent == LastDisplayedPanicPercent)
	{
		return;
	}
	LastDisplayedPanicPercent = DisplayedPercent;

	if (UGvTHUDWidget* GvTHUD = Cast<UGvTHUDWidget>(HUDWidget))
	{
		GvTHUD->UpdatePanicDisplay(NewPanic01);
	}

	UE_LOG(LogTemp, Verbose, TEXT("[HUD] Panic display updated: %d%%"), DisplayedPercent);
#endif
}

void AGvTPlayerController::HandleHauntPressureChanged(float NewPressure01)
{
	// Optional later for pressure on the HUD.
}
