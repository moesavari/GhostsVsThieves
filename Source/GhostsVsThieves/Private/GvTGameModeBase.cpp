#include "GvTGameModeBase.h"
#include "GvTGameStateBase.h"
#include "GvTPlayerController.h"
#include "GvTPlayerState.h"
#include "Gameplay/Characters/Thieves/GvTThiefCharacter.h"
#include "Gameplay/Characters/Thieves/GvTDeadSpectatorPawn.h"
#include "Gameplay/Ghosts/GvTHauntGhostBase.h"
#include "Gameplay/Inventory/GvTInventoryComponent.h"
#include "Gameplay/Interaction/GvTInteractionComponent.h"
#include "Systems/Director/GvTDirectorSubsystem.h"
#include "World/Items/GvTInteractableItem.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

AGvTGameModeBase::AGvTGameModeBase()
{
	GameStateClass = AGvTGameStateBase::StaticClass();
	DeadSpectatorPawnClass = AGvTDeadSpectatorPawn::StaticClass();
}

void AGvTGameModeBase::StartPlay()
{
	Super::StartPlay();
	bMissionFinished = false;
	SecuredItemCount = 0;
	ReadyPlayers.Reset();
	if (AGvTGameStateBase* GS = GetGameState<AGvTGameStateBase>())
	{
		GS->SetMissionPhaseAuthority(EGvTMissionPhase::Active);
		GS->SetMissionOutcomeAuthority(EGvTMissionOutcome::None);
		GS->SetReadyPlayerCountAuthority(0);
	}
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UGvTDirectorSubsystem* Director = GI->GetSubsystem<UGvTDirectorSubsystem>())
		{
			Director->StartDirector();
		}
	}
	RefreshLivingPlayerCount();
}

void AGvTGameModeBase::PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage)
{
	Super::PreLogin(Options, Address, UniqueId, ErrorMessage);
	if (ErrorMessage.IsEmpty())
	{
		ErrorMessage = TEXT("Match already in progress.");
	}
}

void AGvTGameModeBase::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);
	RefreshLivingPlayerCount();
}

void AGvTGameModeBase::Logout(AController* Exiting)
{
	if (Exiting)
	{
		if (AGvTPlayerState* PlayerState = Exiting->GetPlayerState<AGvTPlayerState>())
		{
			ReadyPlayers.Remove(PlayerState);
		}
	}
	Super::Logout(Exiting);
	RefreshLivingPlayerCount();
	RefreshReadyPlayerCount();
	if (!bMissionFinished && !HasLivingThief())
	{
		FinishMission(false);
	}
}

void AGvTGameModeBase::NotifyLootDeposited(AGvTInteractableItem* DepositedItem, int32 SecuredValue)
{
	if (bMissionFinished || !IsValid(DepositedItem)) return;
	if (AGvTGameStateBase* GS = GetGameState<AGvTGameStateBase>())
	{
		GS->AddSecuredLootAuthority(SecuredValue, DepositedItem->IsMainObjective());
		++SecuredItemCount;
	}
	if (DepositedItem->IsMainObjective())
	{
		if (UGvTDirectorSubsystem* Director = GetGameInstance()->GetSubsystem<UGvTDirectorSubsystem>())
		{
			Director->NotifyMainObjectiveDeposited();
		}
	}
}

void AGvTGameModeBase::NotifyThiefDied(AGvTThiefCharacter* DeadThief)
{
	if (bMissionFinished) return;
	SpawnDeadSpectatorFor(DeadThief);
	RefreshReadyPlayerCount();
	RefreshLivingPlayerCount();
	if (!HasLivingThief())
	{
		FinishMission(false);
	}
}

EGvTExtractionRequestResult AGvTGameModeBase::RequestExtraction(APawn* RequestingPawn)
{
	AGvTThiefCharacter* Thief = Cast<AGvTThiefCharacter>(RequestingPawn);
	if (bMissionFinished || !IsValid(Thief) || Thief->IsDead())
	{
		return EGvTExtractionRequestResult::Invalid;
	}

	const AGvTGameStateBase* GS = GetGameState<AGvTGameStateBase>();
	const bool bHasObjective = GS && GS->IsMainObjectiveSecured();
	if (HasAnyLivingThiefCarryingStolenLoot())
	{
		return EGvTExtractionRequestResult::CarryingStolenLoot;
	}
	if (bRequireMainObjectiveForSuccess && !bHasObjective)
	{
		return EGvTExtractionRequestResult::MainObjectiveMissing;
	}

	AGvTPlayerState* PlayerState = Thief->GetPlayerState<AGvTPlayerState>();
	if (!PlayerState)
	{
		return EGvTExtractionRequestResult::Invalid;
	}

	ReadyPlayers.Add(PlayerState);
	RefreshReadyPlayerCount();
	const int32 LivingCount = GetLivingThiefCount();
	UE_LOG(LogTemp, Log, TEXT("[ExtractionReady] Player=%s Ready=%d Living=%d"), *GetNameSafe(Thief), ReadyPlayers.Num(), LivingCount);

	if (LivingCount > 0 && ReadyPlayers.Num() >= LivingCount)
	{
		FinishMission(true);
		return EGvTExtractionRequestResult::DepartureStarted;
	}
	return EGvTExtractionRequestResult::WaitingForPlayers;
}

FText AGvTGameModeBase::BuildExtractionStatusMessage(EGvTExtractionRequestResult Result) const
{
	auto GetThiefName = [](const AGvTThiefCharacter* Thief)
	{
		if (const AGvTPlayerState* PS = Thief ? Thief->GetPlayerState<AGvTPlayerState>() : nullptr)
		{
			if (!PS->GetPlayerName().IsEmpty())
			{
				return PS->GetPlayerName();
			}
		}
		return GetNameSafe(Thief);
	};

	auto JoinNames = [](const TArray<FString>& Names)
	{
		FString Joined;
		for (int32 Index = 0; Index < Names.Num(); ++Index)
		{
			if (Index > 0)
			{
				Joined += Index == Names.Num() - 1 ? TEXT(" and ") : TEXT(", ");
			}
			Joined += Names[Index];
		}
		return Joined;
	};

	if (Result == EGvTExtractionRequestResult::CarryingStolenLoot)
	{
		TArray<FString> Carriers;
		for (TActorIterator<AGvTThiefCharacter> It(GetWorld()); It; ++It)
		{
			const UGvTInventoryComponent* Inventory = It->GetInventoryComponent();
			if (!It->IsDead() && Inventory && Inventory->ContainsStolenLoot())
			{
				Carriers.Add(GetThiefName(*It));
			}
		}
		return FText::Format(
			NSLOCTEXT("GvTExtraction", "LootCarriers", "Departure blocked: {0} must deposit carried valuables."),
			FText::FromString(JoinNames(Carriers)));
	}

	if (Result == EGvTExtractionRequestResult::MainObjectiveMissing)
	{
		return NSLOCTEXT("GvTExtraction", "ObjectiveMissing", "Departure blocked: secure and deposit the main objective first.");
	}

	if (Result == EGvTExtractionRequestResult::WaitingForPlayers)
	{
		TArray<FString> UnreadyPlayers;
		for (TActorIterator<AGvTThiefCharacter> It(GetWorld()); It; ++It)
		{
			if (It->IsDead())
			{
				continue;
			}
			const AGvTPlayerState* PS = It->GetPlayerState<AGvTPlayerState>();
			bool bPlayerReady = false;
			for (const TWeakObjectPtr<AGvTPlayerState>& ReadyPlayer : ReadyPlayers)
			{
				if (ReadyPlayer.Get() == PS)
				{
					bPlayerReady = true;
					break;
				}
			}
			if (!PS || !bPlayerReady)
			{
				UnreadyPlayers.Add(GetThiefName(*It));
			}
		}
		return FText::Format(
			NSLOCTEXT("GvTExtraction", "WaitingForPlayers", "Ready ({0}/{1}). Waiting for: {2}."),
			FText::AsNumber(ReadyPlayers.Num()),
			FText::AsNumber(GetLivingThiefCount()),
			FText::FromString(JoinNames(UnreadyPlayers)));
	}

	if (Result == EGvTExtractionRequestResult::DepartureStarted)
	{
		return NSLOCTEXT("GvTExtraction", "DepartureStarted", "Everyone is ready. Leaving now.");
	}

	return NSLOCTEXT("GvTExtraction", "Unavailable", "Departure is unavailable right now.");
}

void AGvTGameModeBase::FinishMission(bool bSuccess)
{
	if (bMissionFinished) return;
	bMissionFinished = true;

	FGvTMissionResults Results;
	Results.Outcome = bSuccess ? EGvTMissionOutcome::Success : EGvTMissionOutcome::Failure;
	Results.bMissionComplete = bSuccess;
	Results.SurvivingPlayers = GetLivingThiefCount();
	Results.ItemsStolen = SecuredItemCount;

	if (AGvTGameStateBase* GS = GetGameState<AGvTGameStateBase>())
	{
		Results.TotalPlayers = GS->PlayerArray.Num();
		Results.MoneyAccumulated = GS->GetTeamSecuredLoot();
		GS->SetMissionResultsAuthority(Results);
		GS->SetMissionOutcomeAuthority(Results.Outcome);
		GS->SetMissionPhaseAuthority(EGvTMissionPhase::Results);
	}

	if (UGameInstance* GI = GetGameInstance())
	{
		if (UGvTDirectorSubsystem* Director = GI->GetSubsystem<UGvTDirectorSubsystem>())
		{
			Director->StopDirector();
			Director->SetHauntExitDoorsLocked(false);
		}
	}

	for (TActorIterator<AGvTHauntGhostBase> It(GetWorld()); It; ++It)
	{
		It->StartHauntDespawnSequence();
	}

	for (TActorIterator<AGvTThiefCharacter> It(GetWorld()); It; ++It)
	{
		if (It->IsDead()) continue;
		if (UGvTInteractionComponent* Interaction = It->FindComponentByClass<UGvTInteractionComponent>())
		{
			Interaction->SetInteractionEnabled(false);
		}
		It->SetInteractionLock(true, true);
		if (UCharacterMovementComponent* Movement = It->GetCharacterMovement())
		{
			Movement->DisableMovement();
		}
	}

	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (AGvTPlayerController* PC = Cast<AGvTPlayerController>(It->Get()))
		{
			PC->Client_SetMissionInputLocked(true);
			PC->Client_ShowMissionResults(Results);
		}
	}

	if (!MainMenuMapPath.IsEmpty())
	{
		GetWorldTimerManager().SetTimer(RestartTimerHandle, this, &AGvTGameModeBase::ReturnAllPlayersToMainMenu, ResultsScreenDuration, false);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[MissionResults] MainMenuMapPath is empty. Results screen will remain open instead of restarting the mission."));
	}
}

void AGvTGameModeBase::RestartMission()
{
	if (!HasAuthority()) return;
	UGameplayStatics::OpenLevel(this, FName(*GetWorld()->GetMapName()), true);
}

void AGvTGameModeBase::ReturnAllPlayersToMainMenu()
{
	if (!HasAuthority() || MainMenuMapPath.IsEmpty() || !GetWorld()) return;

	const FName ReturnMapName(*MainMenuMapPath);
	AGvTPlayerController* LocalHostController = nullptr;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (AGvTPlayerController* PC = Cast<AGvTPlayerController>(It->Get()))
		{
			if (PC->IsLocalController())
			{
				LocalHostController = PC;
			}
			else
			{
				PC->Client_ReturnToMainMenuAfterMission(ReturnMapName);
			}
		}
	}

	if (LocalHostController)
	{
		TWeakObjectPtr<AGvTPlayerController> WeakHostController = LocalHostController;
		FTimerHandle HostReturnTimer;
		GetWorldTimerManager().SetTimer(
			HostReturnTimer,
			FTimerDelegate::CreateWeakLambda(this, [WeakHostController, ReturnMapName]()
			{
				if (AGvTPlayerController* HostController = WeakHostController.Get())
				{
					HostController->Client_ReturnToMainMenuAfterMission(ReturnMapName);
				}
			}),
			0.25f,
			false);
	}
}

void AGvTGameModeBase::SpawnDeadSpectatorFor(AGvTThiefCharacter* DeadThief)
{
	if (!HasAuthority() || !IsValid(DeadThief) || !DeadSpectatorPawnClass)
	{
		return;
	}

	APlayerController* PlayerController = Cast<APlayerController>(DeadThief->GetController());
	if (!PlayerController)
	{
		return;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = PlayerController;
	SpawnParameters.Instigator = DeadThief;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	const FVector SpawnLocation = DeadThief->GetActorLocation();
	const FRotator SpawnRotation = PlayerController->GetControlRotation();
	AGvTDeadSpectatorPawn* SpectatorPawn = GetWorld()->SpawnActor<AGvTDeadSpectatorPawn>(
		DeadSpectatorPawnClass,
		SpawnLocation,
		FRotator(0.f, SpawnRotation.Yaw, 0.f),
		SpawnParameters);

	if (!SpectatorPawn)
	{
		UE_LOG(LogTemp, Error, TEXT("[DeathSpectator] Failed to spawn for %s"), *GetNameSafe(DeadThief));
		return;
	}

	PlayerController->Possess(SpectatorPawn);
	PlayerController->SetControlRotation(SpawnRotation);

	UE_LOG(LogTemp, Display, TEXT("[DeathSpectator] Player=%s Corpse=%s Spectator=%s"),
		*GetNameSafe(PlayerController), *GetNameSafe(DeadThief), *GetNameSafe(SpectatorPawn));
}

bool AGvTGameModeBase::HasLivingThief() const
{
	for (TActorIterator<AGvTThiefCharacter> It(GetWorld()); It; ++It)
	{
		if (!It->IsDead()) return true;
	}
	return false;
}

void AGvTGameModeBase::RefreshLivingPlayerCount()
{
	if (AGvTGameStateBase* GS = GetGameState<AGvTGameStateBase>())
	{
		GS->SetLivingPlayerCountAuthority(GetLivingThiefCount());
	}
}

int32 AGvTGameModeBase::GetLivingThiefCount() const
{
	int32 Count = 0;
	for (TActorIterator<AGvTThiefCharacter> It(GetWorld()); It; ++It)
	{
		if (!It->IsDead()) ++Count;
	}
	return Count;
}

bool AGvTGameModeBase::HasAnyLivingThiefCarryingStolenLoot() const
{
	for (TActorIterator<AGvTThiefCharacter> It(GetWorld()); It; ++It)
	{
		if (It->IsDead()) continue;
		const UGvTInventoryComponent* Inventory = It->GetInventoryComponent();
		if (Inventory && Inventory->ContainsStolenLoot()) return true;
	}
	return false;
}

void AGvTGameModeBase::RemoveInvalidReadyPlayers()
{
	for (auto It = ReadyPlayers.CreateIterator(); It; ++It)
	{
		const AGvTPlayerState* ReadyState = It->Get();
		bool bStillLiving = false;
		for (TActorIterator<AGvTThiefCharacter> ThiefIt(GetWorld()); ThiefIt; ++ThiefIt)
		{
			if (!ThiefIt->IsDead() && ThiefIt->GetPlayerState<AGvTPlayerState>() == ReadyState)
			{
				bStillLiving = true;
				break;
			}
		}
		if (!bStillLiving) It.RemoveCurrent();
	}
}

void AGvTGameModeBase::RefreshReadyPlayerCount()
{
	RemoveInvalidReadyPlayers();
	if (AGvTGameStateBase* GS = GetGameState<AGvTGameStateBase>())
	{
		GS->SetReadyPlayerCountAuthority(ReadyPlayers.Num());
	}
}
