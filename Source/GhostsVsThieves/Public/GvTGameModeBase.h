#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GvTGameModeBase.generated.h"

class AGvTInteractableItem;
class AGvTPlayerState;
class AGvTThiefCharacter;
class AGvTDeadSpectatorPawn;

UENUM(BlueprintType)
enum class EGvTExtractionRequestResult : uint8
{
	Invalid,
	CarryingStolenLoot,
	MainObjectiveMissing,
	WaitingForPlayers,
	DepartureStarted
};

UCLASS()
class GHOSTSVSTHIEVES_API AGvTGameModeBase : public AGameModeBase
{
	GENERATED_BODY()

public:
	AGvTGameModeBase();
	virtual void StartPlay() override;
	virtual void PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage) override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;

	void NotifyLootDeposited(AGvTInteractableItem* DepositedItem, int32 SecuredValue);
	void NotifyThiefDied(AGvTThiefCharacter* DeadThief);

	UFUNCTION(BlueprintCallable, Category="GvT|Mission")
	EGvTExtractionRequestResult RequestExtraction(APawn* RequestingPawn);

	/** Describes exactly what is blocking departure or which living players are not ready. */
	FText BuildExtractionStatusMessage(EGvTExtractionRequestResult Result) const;

	UFUNCTION(BlueprintCallable, Category="GvT|Mission")
	void RestartMission();

	UFUNCTION(BlueprintCallable, Category="GvT|Mission")
	void ReturnAllPlayersToMainMenu();

protected:
	/** Invisible roaming pawn possessed by a player after their thief dies. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GvT|Death")
	TSubclassOf<AGvTDeadSpectatorPawn> DeadSpectatorPawnClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GvT|Mission")
	bool bRequireMainObjectiveForSuccess = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GvT|Mission", meta=(ClampMin="0.0"))
	float ResultsScreenDuration = 8.f;

	/** Package path such as /Game/Maps/L_MainMenu. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GvT|Mission")
	FString MainMenuMapPath;

private:
	void SpawnDeadSpectatorFor(AGvTThiefCharacter* DeadThief);
	void RefreshLivingPlayerCount();
	void RefreshReadyPlayerCount();
	void RemoveInvalidReadyPlayers();
	void FinishMission(bool bSuccess);
	bool HasLivingThief() const;
	bool HasAnyLivingThiefCarryingStolenLoot() const;
	int32 GetLivingThiefCount() const;

	TSet<TWeakObjectPtr<AGvTPlayerState>> ReadyPlayers;
	int32 SecuredItemCount = 0;
	bool bMissionFinished = false;
	FTimerHandle RestartTimerHandle;
};
